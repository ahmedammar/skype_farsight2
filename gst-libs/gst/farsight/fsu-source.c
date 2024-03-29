/*
 * fsu-source.c - Source for FsuSource
 *
 * Copyright (C) 2010 Collabora Ltd.
 *  @author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * SECTION:fsu-source
 * @short_description: A magical source specific for Farsight-utils
 *
 * The #FsuSource elements are just standard GStreamer element that act a bit
 * like autoaudiosrc and autovideosrc but have a slightly different behavior.
 * The purpose of the #FsuSource is to find the best suitable source for your
 * needs and allow you to use the same exclusive-access source multiple times.
 * The #FsuSource elements only have request pads so you can request the source
 * data as many times as you want. It also serves as some kind of refcounting
 * for the source access, you never need to set the element to NULL when you
 * stop using it (because someone else might still be using it), you only need
 * to release its request pad, and if no request pads are used by the source,
 * then the internal source will be stopped and destroyed automatically.
 * <para>
     Current available sources as 'fsuaudiosrc' and 'fsuvideosrc'.
 * </para>
 *
 * The sources will communicate asynchronous events to the user through
 * #GstMessage of type #GST_MESSAGE_ELEMENT sent over the #GstBus.
 * </para>
 * <refsect2><title>The "<literal>fsusource-no-sources-available</literal>"
 *   message</title>
 * <para>
 * This message is sent on the bus when the source cannot find any suitable
 * source device for capture.
 * </para>
 * </refsect2>
 *
 * </para>
 * <refsect2><title>The "<literal>fsusource-source-chosen</literal>"
 *   message</title>
 * |[
 * "source"             #GstElement The source element chosen
 * "source-name"        gchar *     The name of the chosen element
 * "source-device"      gchar *     The chosen device
 * "source-device-name" gchar *     The user-friendly name of the chosen device
 * ]|
 * <para>
 * This message is sent when the source has been chosen and correctly opened
 * and ready for capture.
 * Note that source-name, source-device and source-device-name could be #NULL if
 * the source was created with the #FsuSource:source-pipeline property set.
 * </para>
 * </refsect2>
 *
 * </para>
 * <refsect2><title>The "<literal>fsusource-source-destroyed</literal>"
 *   message</title>
 * <para>
 * This message is sent when the source has been destroyed.
 * </para>
 * </refsect2>
 *
 * </para>
 * <refsect2><title>The "<literal>fsusource-source-error</literal>"
 *   message</title>
 * |[
 * "error"              #GError     The error received from the source
 * ]|
 * <para>
 * This message is sent when the source received a Resource error. This will
 * usually only be sent when the device is no longer available. It will
 * shortly be followed by a 'fsusource-source-destroyed' message then either
 * a 'fsusource-source-chosen' or 'fsusource-no-sources-available' message.
 * </para>
 * </refsect2>
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <string.h>
#include <gst/interfaces/propertyprobe.h>

#include <gst/farsight/fsu-source-class.h>
#include <gst/farsight/fsu-common.h>
#include <gst/farsight/fsu-probe.h>
#include "fs-marshal.h"

GST_DEBUG_CATEGORY_STATIC (fsu_source_debug);
#define GST_CAT_DEFAULT fsu_source_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)

static const GstElementDetails fsu_source_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Source",
  "Source",
  "Base FsuSource class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static GstStaticPadTemplate fsu_source_src_template =
  GST_STATIC_PAD_TEMPLATE ("src%d",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_source_debug, "fsusource", 0, "fsusource element");
}

GST_BOILERPLATE_FULL (FsuSource, fsu_source,
    GstBin, GST_TYPE_BIN, _do_init)


static void fsu_source_dispose (GObject *object);
static void fsu_source_finalize (GObject *object);

static void fsu_source_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_source_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

static GstStateChangeReturn fsu_source_change_state (GstElement *element,
    GstStateChange transition);
static void fsu_source_handle_message (GstBin *bin,
    GstMessage *message);
static GstPad *fsu_source_request_new_pad (GstElement * element,
  GstPadTemplate * templ,
    const gchar * name);
static void fsu_source_release_pad (GstElement * element,
    GstPad * pad);
static GstElement *create_source (FsuSource *self);
static void destroy_source (FsuSource *self);
static void destroy_source_locked (FsuSource *self);
static void create_source_and_link_tee (FsuSource *self);

/* properties */
enum
{
  PROP_DISABLED = 1,
  PROP_SOURCE_ELEMENT,
  PROP_SOURCE_NAME,
  PROP_SOURCE_DEVICE,
  PROP_SOURCE_PIPELINE,
  LAST_PROPERTY
};

struct _FsuSourcePrivate
{

  /* Properties */
  gboolean disabled;
  gchar *source_name;
  gchar *source_device;
  gchar *source_pipeline;

  /* The source element inside the bin */
  GstElement *source;
  /* Source when being removed, to ignore its messages */
  GstElement *ignore_source;
  /* Last known working configuration */
  gchar *last_working_source;
  gchar *last_working_device;
  /* The src tee coming out of the source element */
  GstElement *tee;
  /* The fakesink linked to the tee to prevent not-linked issues */
  GstElement *fakesink;
  /* Which priority source we are trying */
  const gchar **priority_source_ptr;
  /* plugins filtered */
  GList *filtered_sources;
  /* current plugins filtered being tested */
  GList *current_filtered_source;
  /* whether we're done with the plugins filtered or not */
  gboolean filtered_sources_done;
  /* Which device we were probing */
  gint probe_idx;
  /* Thread for replacing the source */
  GThread *thread;
  /* A FsuMultiFilterManager filters to apply on the source */
  FsuFilterManager *filters;

  /* A mutex to block concurrent request/release pad calls.
     one pipeline modification at a time is allowed */
  GMutex *mutex;
  /* Queue of GstMessage to send */
  GQueue *messages;
};


static void
fsu_source_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_source_details);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&fsu_source_src_template));
}

static void
fsu_source_class_init (FsuSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBinClass *gstbin_class = GST_BIN_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuSourcePrivate));

  gobject_class->get_property = fsu_source_get_property;
  gobject_class->set_property = fsu_source_set_property;
  gobject_class->dispose = fsu_source_dispose;
  gobject_class->finalize = fsu_source_finalize;

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (fsu_source_change_state);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (fsu_source_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (fsu_source_release_pad);
  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR (fsu_source_handle_message);

  g_object_class_install_property (gobject_class, PROP_DISABLED,
      g_param_spec_boolean ("disabled", "Disable the source",
          "Set to TRUE to disable the source",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_ELEMENT,
      g_param_spec_object ("source-element", "The source element created",
          "The internal source element created.",
          GST_TYPE_ELEMENT,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_NAME,
      g_param_spec_string ("source-name", "Source element name",
          "The name of the source element to create.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_DEVICE,
      g_param_spec_string ("source-device", "Source device",
          "The device to use with the source",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_PIPELINE,
      g_param_spec_string ("source-pipeline", "Source pipeline",
          "The pipeline to use for creating the source",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_source_init (FsuSource *self, FsuSourceClass *klass)
{
  FsuSourcePrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SOURCE,
          FsuSourcePrivate);

  self->priv = priv;
  priv->probe_idx = -1;
  priv->filters = fsu_multi_filter_manager_new ();
  if (klass->add_filters)
    klass->add_filters (self, priv->filters);
  priv->mutex = g_mutex_new ();
  priv->messages = g_queue_new ();

}

static void
reset_source_search_locked (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GList *item;

  priv->current_filtered_source = NULL;

  for (item = priv->filtered_sources; item; item = g_list_next (item))
  {
    if (item->data)
      gst_object_unref (GST_ELEMENT_FACTORY (item->data));
  }
  g_list_free (priv->filtered_sources);
  priv->filtered_sources = NULL;
  priv->priority_source_ptr = NULL;
  priv->filtered_sources_done = FALSE;
  priv->probe_idx = -1;
}

static void
reset_and_restart_source_unlock (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;

  reset_source_search_locked (self);
  if (priv->source)
  {
    destroy_source_locked (self);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
  }
  else if (priv->tee && GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    g_mutex_lock (priv->mutex);
    create_source_and_link_tee (self);
    g_mutex_unlock (priv->mutex);
    g_object_notify (G_OBJECT (self), "source-element");
    /* Send pending GstMessages once unlocked */
    while (!g_queue_is_empty (priv->messages))
    {
      GstMessage *msg = g_queue_pop_head (priv->messages);

      gst_element_post_message (GST_ELEMENT (self), msg);
    }
  }
  else
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
  }
}

static void
fsu_source_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuSource *self = FSU_SOURCE (object);
  FsuSourcePrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_DISABLED:
      g_value_set_boolean (value, priv->disabled);
      break;
    case PROP_SOURCE_ELEMENT:
      GST_OBJECT_LOCK (GST_OBJECT (self));
      g_value_set_object (value, priv->source);
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      break;
    case PROP_SOURCE_NAME:
      g_value_set_string (value, priv->source_name);
      break;
    case PROP_SOURCE_DEVICE:
      g_value_set_string (value, priv->source_device);
      break;
    case PROP_SOURCE_PIPELINE:
      g_value_set_string (value, priv->source_pipeline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_source_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuSource *self = FSU_SOURCE (object);
  FsuSourcePrivate *priv = self->priv;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  switch (property_id)
  {
    case PROP_DISABLED:
      priv->disabled = g_value_get_boolean (value);
      break;
    case PROP_SOURCE_NAME:
      g_free (priv->source_name);
      priv->source_name = g_value_dup_string (value);
      break;
    case PROP_SOURCE_DEVICE:
      g_free (priv->source_device);
      priv->source_device = g_value_dup_string (value);
      break;
    case PROP_SOURCE_PIPELINE:
      g_free (priv->source_pipeline);
      priv->source_pipeline = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  reset_and_restart_source_unlock (self);
}

static void
fsu_source_dispose (GObject *object)
{
  FsuSource *self = FSU_SOURCE (object);
  FsuSourcePrivate *priv = self->priv;
  GList *item, *walk;

 restart:
  GST_OBJECT_LOCK (GST_OBJECT (self));
  for (item = GST_ELEMENT_PADS (object); item; item = g_list_next (item))
  {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SRC (pad))
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      gst_element_release_request_pad (GST_ELEMENT (object), pad);
      goto restart;
    }
  }

  while (!g_queue_is_empty (priv->messages))
  {
    GstMessage *msg = g_queue_pop_head (priv->messages);

    gst_message_unref (msg);
  }

  priv->current_filtered_source = NULL;

  for (walk = priv->filtered_sources; walk; walk = g_list_next (walk))
  {
    if (walk->data)
      gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
  }
  g_list_free (priv->filtered_sources);
  priv->filtered_sources = NULL;

  g_free (priv->source_name);
  priv->source_name = NULL;
  g_free (priv->source_device);
  priv->source_device = NULL;
  g_free (priv->source_pipeline);
  priv->source_pipeline = NULL;

  g_free (priv->last_working_source);
  priv->last_working_source = NULL;
  g_free (priv->last_working_device);
  priv->last_working_device = NULL;


  if (priv->source)
    gst_object_unref (priv->source);
  priv->source = NULL;
  if (priv->tee)
    gst_object_unref (priv->tee);
  priv->tee = NULL;
  if (priv->fakesink)
    gst_object_unref (priv->fakesink);
  priv->fakesink = NULL;

  priv->priority_source_ptr = NULL;

  g_object_unref (priv->filters);

  g_assert (!priv->thread);

  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
fsu_source_finalize (GObject *object)
{
  FsuSource *self = FSU_SOURCE (object);

  g_mutex_free (self->priv->mutex);
  g_queue_free (self->priv->messages);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstElement *
find_source (GstElement *src)
{


  if (GST_IS_BIN (src))
  {
    GstElement *source = NULL;
    GstIterator *it = gst_bin_iterate_sources (GST_BIN(src));
    gboolean done = FALSE;
    gpointer item = NULL;

    while (!done)
    {
      switch (gst_iterator_next (it, &item))
      {
        case GST_ITERATOR_OK:
          source = GST_ELEMENT (item);
          done = TRUE;
          break;
        case GST_ITERATOR_RESYNC:
          gst_iterator_resync (it);
          break;
        case GST_ITERATOR_ERROR:
          done = TRUE;
          break;
        case GST_ITERATOR_DONE:
          done = TRUE;
          break;
      }
    }
    gst_iterator_free (it);

    if (!source)
      return gst_object_ref (src);

    src = find_source (source);
    gst_object_unref (source);
    return src;
  }
  else
  {
    return gst_object_ref (src);
  }
}

static void
check_and_remove_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  if (GST_ELEMENT (self)->numsrcpads != 0)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    return;
  }

  DEBUG ("No more request source pads");
  if (priv->tee)
  {
    GstElement *tee = priv->tee;
    GstElement *fakesink = priv->fakesink;

    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    if(fakesink)
      gst_element_unlink (tee, fakesink);
    gst_bin_remove (GST_BIN (self), tee);
    gst_element_set_state (tee, GST_STATE_NULL);
    gst_object_unref (tee);

    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->tee = NULL;
  }

  if (priv->fakesink)
  {
    GstElement *fakesink = priv->fakesink;

    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    gst_bin_remove (GST_BIN (self), fakesink);
    gst_element_set_state (fakesink, GST_STATE_NULL);
    gst_object_unref (fakesink);

    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->fakesink = NULL;
  }

  if (priv->source)
  {
    GstElement *source = priv->source;

    priv->source = NULL;
    priv->ignore_source = find_source (source);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    gst_bin_remove (GST_BIN (self), source);
    gst_element_set_state (source, GST_STATE_NULL);
    gst_object_unref (source);

    g_queue_push_tail (priv->messages,
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new ("fsusource-source-destroyed",
                NULL)));

    GST_OBJECT_LOCK (GST_OBJECT (self));
    if (priv->ignore_source)
      gst_object_unref (priv->ignore_source);
    priv->ignore_source = NULL;
  }
  reset_source_search_locked (self);

  GST_OBJECT_UNLOCK (GST_OBJECT (self));
}

static void
create_source_and_link_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *src = NULL;
  GstPad *src_pad = NULL;
  GstPad *tee_pad = NULL;

  g_return_if_fail (priv->tee);
  g_return_if_fail (!priv->source);

  src = create_source (self);
  if (src)
  {
    if (!gst_bin_add (GST_BIN (self), src))
    {
      WARNING ("Could not add source element to Source");
      gst_object_unref (src);
      goto error_destroy_source;
    }

    src_pad = gst_element_get_static_pad (src, "src");
  }
  else
  {
    WARNING ("Could not find a usable source");
  }

  if (src_pad)
  {
    GstElement *tee = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    tee = gst_object_ref (priv->tee);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    tee_pad = gst_element_get_static_pad (tee, "sink");
    gst_object_unref (tee);

    if (tee_pad)
    {
      if (GST_PAD_LINK_FAILED (gst_pad_link (src_pad, tee_pad)))
      {
        WARNING ("Couldn't link source pad with src tee");
        goto error_link;
      }
      GST_OBJECT_LOCK (GST_OBJECT (self));
      if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
        if (!gst_element_sync_state_with_parent  (src))
        {
          WARNING ("Couldn't sync src state with parent");
          goto error_state;
        }
      }
      else
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
      }
      gst_object_unref (tee_pad);
    }
    else
    {
      WARNING ("Could not get tee pad to link with source");
      goto error_pad;
    }

    gst_object_unref (src_pad);
  }

  return;
 error_state:
  gst_pad_unlink (src_pad, tee_pad);
 error_link:
  gst_object_unref (tee_pad);
 error_pad:
  gst_object_unref (src_pad);
  GST_OBJECT_LOCK (GST_OBJECT (self));
  if (priv->ignore_source || !priv->source)
  {
    /* The source was already destroyed or is being removed, which means it
     * got an async error. Let it get destroyed by the destroy thread.
     */
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    return;
  }
  priv->ignore_source = find_source (src);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  gst_bin_remove (GST_BIN (self), src);
 error_destroy_source:
  GST_OBJECT_LOCK (GST_OBJECT (self));
  src = priv->source;
  priv->source = NULL;
  g_free (priv->last_working_source);
  priv->last_working_source = NULL;
  g_free (priv->last_working_device);
  priv->last_working_device = NULL;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  gst_element_set_state (src, GST_STATE_NULL);
  gst_object_unref (src);

  g_queue_push_tail (priv->messages,
      gst_message_new_element (GST_OBJECT (self),
          gst_structure_new ("fsusource-source-destroyed",
              NULL)));

  GST_OBJECT_LOCK (GST_OBJECT (self));
  if (priv->ignore_source)
  {
    gst_object_unref (priv->ignore_source);
    priv->ignore_source = NULL;
  }
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  check_and_remove_tee (self);
}

static void
create_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *tee = NULL;
  GstElement *fakesink = NULL;

  tee = gst_element_factory_make ("tee", "srctee");
  fakesink = gst_element_factory_make ("fakesink", NULL);

  if (!tee)
  {
    WARNING ("Could not create src tee");
    if (fakesink)
      gst_object_unref (fakesink);
    return;
  }
  if (!gst_bin_add (GST_BIN (self), tee))
  {
    WARNING ("Could not add src tee to Source");
    gst_object_unref (tee);
    if (fakesink)
      gst_object_unref (fakesink);
    return;
  }
  if (fakesink && !gst_bin_add (GST_BIN (self), fakesink))
  {
    gst_object_unref (fakesink);
    fakesink = NULL;
  }
  if (fakesink && !gst_element_link (tee, fakesink))
  {
    gst_bin_remove (GST_BIN (self), fakesink);
    fakesink = NULL;
  }
  if (fakesink)
  {
    g_object_set (fakesink,
        "sync", FALSE,
        "async", FALSE,
        NULL);
  }

  GST_OBJECT_LOCK (GST_OBJECT (self));
  priv->tee = gst_object_ref (tee);
  priv->fakesink = fakesink? gst_object_ref (fakesink) : NULL;

  if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    if (!gst_element_sync_state_with_parent  (tee) ||
        (fakesink && !gst_element_sync_state_with_parent  (fakesink)))
    {
      WARNING ("Could not sync tee state with parent");
      if (fakesink)
      {
        gst_element_unlink (tee, fakesink);
        gst_bin_remove (GST_BIN (self), fakesink);
      }
      gst_bin_remove (GST_BIN (self), tee);

      GST_OBJECT_LOCK (GST_OBJECT (self));
      if (priv->tee)
        gst_object_unref (priv->tee);
      priv->tee = NULL;
      if (priv->fakesink)
        gst_object_unref (priv->fakesink);
      priv->fakesink = NULL;
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      return;
    }
    create_source_and_link_tee (self);
  }
  else
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    DEBUG ("State NULL, not creating source now...");
  }

}

static gboolean
pad_event_probe (GstPad  *pad, GstEvent *event, gpointer user_data)
{
  if (GST_EVENT_TYPE (event) == GST_EVENT_NEWSEGMENT)
    gst_pad_push_event (pad, gst_event_new_flush_stop ());

  return TRUE;
}

static GstPad *
fsu_source_request_new_pad (GstElement * element,
    GstPadTemplate * templ,
    const gchar * name)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;
  GstPad *pad = NULL;
  GstPad *tee_pad = NULL;
  GstPad *filter_pad = NULL;
  GstElement *tee = NULL;

  DEBUG ("requesting pad");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  g_mutex_lock (priv->mutex);

  if (!priv->tee)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    create_tee (self);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    if (!priv->tee)
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      WARNING ("Couldn't create a tee to request pad from");
      goto out;
    }
  }

  tee = gst_object_ref (priv->tee);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  tee_pad = gst_element_get_request_pad (tee, "src%d");

  if (!tee_pad)
  {
    WARNING ("Couldn't get new request pad from src tee");
    check_and_remove_tee (self);
    goto out;
  }

  filter_pad = fsu_filter_manager_apply (priv->filters,
      GST_BIN (self), tee_pad);

  if (!filter_pad)
  {
    WARNING ("Couldn't apply filter manager");
    filter_pad = gst_object_ref (tee_pad);
  }
  gst_object_unref (tee_pad);

  pad = gst_ghost_pad_new (name, filter_pad);
  gst_object_unref (filter_pad);
  if (!pad)
  {
    WARNING ("Couldn't create ghost pad for tee");
    tee_pad = fsu_filter_manager_revert (priv->filters,
        GST_BIN (self), filter_pad);
    gst_element_release_request_pad (tee, tee_pad);
    gst_object_unref (tee_pad);
    gst_object_unref (tee);
    check_and_remove_tee (self);
    goto out;
  }
  gst_object_unref (tee);

  gst_pad_set_active (pad, TRUE);

  gst_element_add_pad (element, pad);

  gst_pad_add_event_probe (pad, (GCallback)pad_event_probe, self);

 out:
  g_mutex_unlock (priv->mutex);

  g_object_notify (G_OBJECT (self), "source-element");

  /* Send pending GstMessages once unlocked */
  while (!g_queue_is_empty (priv->messages))
  {
      GstMessage *msg = g_queue_pop_head (priv->messages);

      gst_element_post_message (GST_ELEMENT (self), msg);
  }
  return pad;
}

static void
fsu_source_release_pad (GstElement * element,
    GstPad * pad)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;

  DEBUG ("releasing pad");

  g_mutex_lock (priv->mutex);

  gst_pad_set_active (pad, FALSE);

  if (GST_IS_GHOST_PAD (pad))
  {
    GstPad *filter_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
    GstPad *tee_pad = NULL;
    GstElement *tee = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    tee = gst_object_ref (priv->tee);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    tee_pad = fsu_filter_manager_revert (priv->filters,
        GST_BIN (self), filter_pad);
    if (!tee_pad)
      tee_pad = gst_object_ref (filter_pad);
    gst_object_unref (filter_pad);

    gst_element_release_request_pad (tee, tee_pad);
    gst_object_unref (tee_pad);
    gst_object_unref (tee);
  }

  gst_element_remove_pad (element, pad);

  check_and_remove_tee (self);

  g_mutex_unlock (priv->mutex);

  g_object_notify (G_OBJECT (self), "source-element");

  /* Send pending GstMessages once unlocked */
  while (!g_queue_is_empty (priv->messages))
  {
    GstMessage *msg = g_queue_pop_head (priv->messages);

    gst_element_post_message (GST_ELEMENT (self), msg);
  }
}


static GstElement *
test_source (FsuSource *self,
    const gchar *name)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *element = NULL;
  GstState target_state = GST_STATE_READY;
  const gchar * target_state_name = "READY";
  GstStateChangeReturn state_ret;
  const gchar **blacklist = FSU_SOURCE_GET_CLASS (self)->blacklisted_sources;
  FsuProbeDeviceElement *probe = NULL;
  gint probe_idx;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
  {
    target_state = GST_STATE (GST_ELEMENT (self));
    if (GST_STATE (GST_ELEMENT (self)) == GST_STATE_READY)
      target_state_name = "READY";
    else if (GST_STATE (GST_ELEMENT (self)) == GST_STATE_PAUSED)
      target_state_name = "PAUSED";
    else if (GST_STATE (GST_ELEMENT (self)) == GST_STATE_PLAYING)
      target_state_name = "PLAYING";
    DEBUG ("we are not NULL, target state set to %s", target_state_name);
  }

  probe_idx = priv->probe_idx;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  DEBUG ("Testing source %s. probe idx = %d", name, probe_idx);

  for (;blacklist && *blacklist; blacklist++)
  {
    if (!g_strcmp0 (name, *blacklist))
    {
      GST_OBJECT_LOCK (GST_OBJECT (self));
      priv->probe_idx = -1;
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      return NULL;
    }
  }


  element = gst_element_factory_make (name, NULL);

  if (!element)
  {
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->probe_idx = -1;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    return NULL;
  }

  /* Test the source */
  if (probe_idx == -1)
  {
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->probe_idx = 0;
    probe_idx = 0;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    state_ret = gst_element_set_state (element, target_state);
    if (state_ret == GST_STATE_CHANGE_ASYNC)
    {
      DEBUG ("Waiting for %s to go to state %s", name, target_state_name);
      state_ret = gst_element_get_state (element, NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }

    if (state_ret != GST_STATE_CHANGE_FAILURE)
    {
      return element;
    }
  }

  probe = fsu_probe_element (name);
  if (probe)
  {
    guint i;
    for (i = probe_idx; i < g_list_length (probe->devices); i++)
    {
      const gchar *device = g_list_nth_data (probe->devices, i);

      DEBUG ("Testing device %s", device);
      g_object_set(element,
          _fsu_get_device_property_name(element), device,
          NULL);

      state_ret = gst_element_set_state (element, target_state);
      if (state_ret == GST_STATE_CHANGE_ASYNC)
      {
        DEBUG ("Waiting for %s to go to state %s", name, target_state_name);
        state_ret = gst_element_get_state (element, NULL, NULL,
            GST_CLOCK_TIME_NONE);
      }

      if (state_ret != GST_STATE_CHANGE_FAILURE)
      {
        DEBUG ("Took device %s with idx %d", device, i);
        fsu_probe_device_element_free (probe);

        GST_OBJECT_LOCK (GST_OBJECT (self));
        priv->probe_idx = i + 1;
        GST_OBJECT_UNLOCK (GST_OBJECT (self));

        return element;
      }
    }
    fsu_probe_device_element_free (probe);
  }

  gst_element_set_state (element, GST_STATE_NULL);
  gst_object_unref (element);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  priv->probe_idx = -1;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  return NULL;
}


static GstElement *
create_source (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *src = NULL;
  GstElement *real_src = NULL;
  gchar *source_pipeline = NULL;
  gchar *source_name = NULL;
  gchar *source_device = NULL;
  gboolean using_last_working = FALSE;

  GST_OBJECT_LOCK (GST_OBJECT (self));

  g_assert (!priv->source);

  if (priv->disabled)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    DEBUG ("Source is disabled");
    return NULL;
  }

  source_pipeline = g_strdup (priv->source_pipeline);
  source_name = g_strdup (priv->source_name);
  source_device = g_strdup (priv->source_device);

  if (!source_pipeline && !source_name && priv->last_working_source)
  {
    source_name = g_strdup (priv->last_working_source);
    g_free (source_device);
    source_device = g_strdup (priv->last_working_device);
    using_last_working = TRUE;
  }

  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  DEBUG ("Creating source : %s -- %s (%s)",
      source_pipeline ? source_pipeline : "(null)",
      source_name ? source_name : "(null)",
      source_device ? source_device : "(null)");

 retry:
  if (source_pipeline)
  {
    GstBin *bin = NULL;
    gchar *desc = NULL;
    GError *error  = NULL;
    GstStateChangeReturn state_ret;

    /* parse the pipeline to a bin */
    desc = g_strdup_printf ("bin.( %s ! identity )", source_pipeline);
    bin = (GstBin *) gst_parse_launch (desc, &error);
    g_free (desc);

    if (bin)
    {
      GstPad *pad = NULL;
      /* find pads and ghost them if necessary */
      pad = gst_bin_find_unlinked_pad (bin, GST_PAD_SRC);
      if (pad)
      {
        gst_element_add_pad (GST_ELEMENT (bin), gst_ghost_pad_new ("src", pad));
        gst_object_unref (pad);
      }
    }
    if (error)
    {
      WARNING ("Error while creating source-pipeline (%d): %s",
          error->code, (error->message? error->message : "(null)"));
      goto error;
    }

    state_ret = gst_element_set_state (GST_ELEMENT (bin), GST_STATE_READY);
    if (state_ret == GST_STATE_CHANGE_FAILURE)
    {
      WARNING ("Could not change state of source-pipeline to READY");
      gst_object_unref (bin);
      goto error;
    }

    src = GST_ELEMENT (bin);
  }
  else if (source_name)
  {
    GstStateChangeReturn state_ret;

    /* If no device specified, try to autodetect device */
    if (source_device)
    {
      src = gst_element_factory_make (source_name, NULL);

      if (src && _fsu_get_device_property_name(src))
        g_object_set(src,
            _fsu_get_device_property_name(src), source_device,
            NULL);
    }
    else
    {
      src = test_source (self, source_name);

      /* If we can't test the source (maybe blacklisted or other reasons..
       * then create the element and let the rest of the code handle the error
       */
      if (!src)
        src = gst_element_factory_make (source_name, NULL);
    }

    if (src)
    {
      state_ret = gst_element_set_state (src, GST_STATE_READY);
      if (state_ret == GST_STATE_CHANGE_FAILURE)
      {
        DEBUG ("Unable to set source to READY");
        gst_object_unref (src);
        src = NULL;
        goto error;
      }
    }

  }
  else
  {
    GList *walk;
    const gchar **priority_source = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    priority_source = priv->priority_source_ptr;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    if (!priority_source)
      priority_source = FSU_SOURCE_GET_CLASS (self)->priority_sources;
    for (;priority_source && *priority_source; priority_source++)
    {
      GstElement *element = test_source (self, *priority_source);

      if (!element)
        continue;

      DEBUG ("Using source %s", *priority_source);
      src = element;
      break;
    }
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->priority_source_ptr = priority_source;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    if (!src)
    {
      GST_OBJECT_LOCK (GST_OBJECT (self));
      walk = priv->current_filtered_source;
      if (!walk && !priv->filtered_sources_done)
        walk = priv->filtered_sources;

      if (!walk && !priv->filtered_sources_done)
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
        walk = _fsu_get_plugins_filtered (
            FSU_SOURCE_GET_CLASS (self)->klass_check);

        GST_OBJECT_LOCK (GST_OBJECT (self));
        priv->filtered_sources = walk;
      }
      GST_OBJECT_UNLOCK (GST_OBJECT (self));

      for (;walk; walk = g_list_next (walk))
      {
        GstElement *element;
        GstElementFactory *factory = GST_ELEMENT_FACTORY(walk->data);
        GstPluginFeature *plugin_feature = GST_PLUGIN_FEATURE (factory);

        if (gst_plugin_feature_get_rank (plugin_feature) == GST_RANK_NONE)
          continue;

        element = test_source (self, GST_PLUGIN_FEATURE_NAME(factory));

        if (!element)
          continue;

        DEBUG ("Using source %s", GST_PLUGIN_FEATURE_NAME(factory));
        src = element;
        break;
      }

      GST_OBJECT_LOCK (GST_OBJECT (self));
      priv->current_filtered_source = walk;
      GST_OBJECT_UNLOCK (GST_OBJECT (self));

      if (!walk)
      {
        GST_OBJECT_LOCK (GST_OBJECT (self));
        priv->filtered_sources_done = TRUE;
        walk = priv->filtered_sources;
        GST_OBJECT_UNLOCK (GST_OBJECT (self));

        for (; walk; walk = g_list_next (walk))
        {
          if (walk->data)
            gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
        }
        GST_OBJECT_LOCK (GST_OBJECT (self));
        g_list_free (priv->filtered_sources);
        priv->filtered_sources = NULL;
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
      }

    }
  }

 error:
  if (!src && using_last_working)
  {
    g_free (source_name);
    source_name = NULL;
    g_free (source_device);
    source_device = NULL;
    using_last_working = FALSE;
    goto retry;
  }

  if (src)
  {
    gchar *device = NULL;
    gchar *device_name = NULL;
    const gchar *element_name = NULL;
    GstElement *chosen_src = NULL;
    GstElementFactory *factory = NULL;

    GST_OBJECT_FLAG_UNSET (src, GST_ELEMENT_IS_SINK);
    real_src = find_source (src);

    if (_fsu_g_object_has_property (G_OBJECT (real_src), "is-live"))
      g_object_set(real_src, "is-live", TRUE, NULL);

    if (source_pipeline)
    {
      chosen_src = src;
    }
    else
    {
      chosen_src = real_src;
      factory = gst_element_get_factory (chosen_src);
      element_name = GST_PLUGIN_FEATURE_NAME(factory);

      if (_fsu_get_device_property_name(chosen_src))
      {
        g_object_get (chosen_src,
            _fsu_get_device_property_name(chosen_src), &device,
            NULL);
        if (_fsu_g_object_has_property (G_OBJECT (chosen_src), "device-name"))
        {
          g_object_get (chosen_src,
              "device-name", &device_name,
              NULL);
        }
        else
        {
          device_name = g_strdup (device);
        }
      }
    }

    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->source = gst_object_ref (src);
    g_free (priv->last_working_source);
    priv->last_working_source = g_strdup (element_name);
    g_free (priv->last_working_device);
    priv->last_working_device = g_strdup (device);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));


    g_queue_push_tail (priv->messages,
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new ("fsusource-source-chosen",
                "source", GST_TYPE_ELEMENT, chosen_src,
                "source-name", G_TYPE_STRING, element_name,
                "source-device", G_TYPE_STRING, device,
                "source-device-name", G_TYPE_STRING, device_name,
                NULL)));

    g_free (device);
    g_free (device_name);
    gst_object_unref (real_src);
  }
  else
  {
    GST_OBJECT_LOCK (GST_OBJECT (self));
    g_free (priv->last_working_source);
    priv->last_working_source = NULL;
    g_free (priv->last_working_device);
    priv->last_working_device = NULL;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    g_queue_push_tail (priv->messages,
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new ("fsusource-no-sources-available",
                NULL)));
  }

  g_free (source_pipeline);
  g_free (source_name);
  g_free (source_device);

  return src;
}

static GstStateChangeReturn
fsu_source_change_state (GstElement *element,
    GstStateChange transition)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;
  GstStateChangeReturn ret;

  switch (transition)
  {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_OBJECT_LOCK (GST_OBJECT (self));
      if (!priv->source && priv->tee)
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
        DEBUG ("Going to READY state. Creating source and linking to tee");
        create_source_and_link_tee (self);
      }
      else
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition)
  {
    case GST_STATE_CHANGE_READY_TO_NULL:
      GST_OBJECT_LOCK (GST_OBJECT (self));
      if (priv->source)
      {
        GstElement *source = priv->source;

        priv->source = NULL;
        g_free (priv->last_working_source);
        priv->last_working_source = NULL;
        g_free (priv->last_working_device);
        priv->last_working_device = NULL;
        priv->ignore_source = find_source (source);
        GST_OBJECT_UNLOCK (GST_OBJECT (self));

        DEBUG ("Setting source to state NULL");
        ret = gst_element_set_state (source, GST_STATE_NULL);
        gst_bin_remove (GST_BIN (self), source);
        gst_object_unref (source);

        g_queue_push_tail (priv->messages,
            gst_message_new_element (GST_OBJECT (self),
                gst_structure_new ("fsusource-source-destroyed",
                    NULL)));
        g_object_notify (G_OBJECT (self), "source-element");

        GST_OBJECT_LOCK (GST_OBJECT (self));
        gst_object_unref (priv->ignore_source);
        priv->ignore_source = NULL;
      }
      reset_source_search_locked (self);
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      /* We are a live source, force the result to be NO_PREROLL..
         It may not be if we were unable to find an available source */
      if (ret == GST_STATE_CHANGE_SUCCESS)
        ret = GST_STATE_CHANGE_NO_PREROLL;
    default:
      break;
  }

  return ret;
}

static gpointer
replace_source_thread_unlock (gpointer data)
{
  FsuSource *self = FSU_SOURCE (data);
  FsuSourcePrivate *priv = self->priv;
  GstStateChangeReturn state_ret;
  GstElement *source = NULL;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  source = priv->source;
  priv->source = NULL;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (source)
  {
    state_ret = gst_element_set_state (source, GST_STATE_NULL);
    gst_object_unref (source);

    if (state_ret == GST_STATE_CHANGE_ASYNC)
    {
      DEBUG ("Waiting for source to go to state NULL");
      state_ret = gst_element_get_state (source, NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }

    g_queue_push_tail (priv->messages,
        gst_message_new_element (GST_OBJECT (self),
            gst_structure_new ("fsusource-source-destroyed",
                NULL)));

  }

  GST_OBJECT_LOCK (GST_OBJECT (self));
  gst_object_unref (priv->ignore_source);
  priv->ignore_source = NULL;
  priv->thread = NULL;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (source)
    create_source_and_link_tee (self);
  gst_object_unref (self);

  /* This was locked before creating the thread, so unlock it now.*/
  g_mutex_unlock (priv->mutex);

  g_object_notify (G_OBJECT (self), "source-element");

  /* Send pending GstMessages once unlocked */
  while (!g_queue_is_empty (priv->messages))
  {
    GstMessage *msg = g_queue_pop_head (priv->messages);

    gst_element_post_message (GST_ELEMENT (self), msg);
  }

  return NULL;
}

static void
destroy_source_locked (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GError *error = NULL;

  /* Lock until the thread finishes its job */
  g_mutex_lock (priv->mutex);

  if (priv->source && !priv->thread)
  {
    GstElement *source = priv->source;

    priv->ignore_source = find_source (source);
    g_free (priv->last_working_source);
    priv->last_working_source = NULL;
    g_free (priv->last_working_device);
    priv->last_working_device = NULL;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    gst_bin_remove (GST_BIN (self), source);

    gst_object_ref (self);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->thread = g_thread_create (replace_source_thread_unlock, self,
        FALSE, &error);

    if (!priv->thread)
    {
      WARNING ("Could not start stopping thread for FsuSource:"
          " %s", error->message);
      gst_object_unref (self);
      g_mutex_unlock (priv->mutex);
    }
    g_clear_error (&error);
  }
  else
  {
    g_mutex_unlock (priv->mutex);
  }
}

static void
destroy_source (FsuSource *self)
{
  GST_OBJECT_LOCK (GST_OBJECT (self));
  destroy_source_locked (self);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
}


static void
fsu_source_handle_message (GstBin *bin,
    GstMessage *message)
{
  FsuSource *self = FSU_SOURCE (bin);
  FsuSourcePrivate *priv = self->priv;
  GstElement *ignore_source = NULL;

  DEBUG ("Got message of type %s from %s", GST_MESSAGE_TYPE_NAME(message),
      GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)));


  GST_OBJECT_LOCK (GST_OBJECT (self));
  ignore_source = priv->ignore_source;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (GST_MESSAGE_SRC (message) == GST_OBJECT (ignore_source))
  {
    DEBUG ("Message received from currently being destroyed source. Ignoring");
    /* Ignoring messages */
    gst_message_unref (message);
    return;
  }

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
  {
    GError *error;
    GstElement *source = NULL;
    GstElement *real_source = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    if (!priv->source)
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      goto done;
    }
    source = gst_object_ref (priv->source);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    real_source = find_source (source);
    gst_object_unref (source);

    gst_message_parse_error (message, &error, NULL);

    if (error->domain == GST_RESOURCE_ERROR &&
        GST_MESSAGE_SRC (message) == GST_OBJECT (real_source))
    {
      DEBUG ("Source got an error. Destroying source");
      g_queue_push_tail (priv->messages,
          gst_message_new_element (GST_OBJECT (self),
              gst_structure_new ("fsusource-source-error",
                  "error", GST_TYPE_G_ERROR, error,
                  NULL)));
      destroy_source (self);
      gst_message_unref (message);
      gst_object_unref (real_source);
      g_error_free (error);
      return;
    }
    g_error_free (error);
    gst_object_unref (real_source);
  }
 done:
  if (fsu_filter_manager_handle_message (priv->filters, message))
    gst_message_unref (message);
  else
    GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}
