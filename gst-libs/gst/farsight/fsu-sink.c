/*
 * fsu-sink.c - Sink for FsuSink
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
 * SECTION:fsu-sink
 * @short_description: A magical sink specific for Farsight-utils
 *
 * The #FsuSink elements are just standard GStreamer element that act a bit
 * like autoaudiosink and autovideosink but have a slightly different behavior.
 * The purpose of the #FsuSink is to find the best suitable sink for your
 * needs and allow you to use the same exclusive-access sink multiple times.
 * The #FsuSink elements only have request pads so you can request access to the
 * sink as many times as you want. It also serves as some kind of refcounting
 * for the sink access, you never need to set the element to NULL when you
 * stop using it (because someone else might still be using it), you only need
 * to release its request pad, and if no request pads are used by the sink,
 * then the internal sink will be stopped and destroyed automatically.
 * <para>
     Current available sinks as 'fsuaudiosink' and 'fsuvideosink'.
 * </para>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <string.h>
#include <gst/interfaces/propertyprobe.h>

#include "fsu-sink-class.h"
#include <gst/farsight/fsu-common.h>


GST_DEBUG_CATEGORY_STATIC (fsu_sink_debug);
#define GST_CAT_DEFAULT fsu_sink_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)

static const GstElementDetails fsu_sink_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Sink",
  "Sink",
  "Base FsuSink class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static GstStaticPadTemplate fsu_sink_sink_template =
  GST_STATIC_PAD_TEMPLATE ("sink%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_sink_debug, "fsusink", 0, "fsusink element");
}

GST_BOILERPLATE_FULL (FsuSink, fsu_sink,
    GstBin, GST_TYPE_BIN, _do_init)


static void fsu_sink_dispose (GObject *object);
static void fsu_sink_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_sink_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

static void fsu_sink_handle_message (GstBin *bin,
    GstMessage *message);
static GstPad *fsu_sink_request_new_pad (GstElement * element,
    GstPadTemplate * templ,
    const gchar * name);
static void fsu_sink_release_pad (GstElement * element,
    GstPad * pad);
static GstElement *create_sink (FsuSink *self);

/* properties */
enum
{
  PROP_SINK_NAME = 1,
  PROP_SINK_DEVICE,
  PROP_SINK_PIPELINE,
  PROP_SYNC,
  PROP_ASYNC,
  LAST_PROPERTY
};

struct _FsuSinkPrivate
{
  /* Properties */
  gchar *sink_name;
  gchar *sink_device;
  gchar *sink_pipeline;
  gboolean sync;
  gboolean async;

  /* The mixer element to mix */
  GstElement *mixer;
  GList *sinks;
  /* The FsuMultiFilterManager filters to apply on the sink */
  FsuFilterManager *filters;
};


static void
fsu_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_sink_details);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&fsu_sink_sink_template));
}

static void
fsu_sink_class_init (FsuSinkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBinClass *gstbin_class = GST_BIN_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuSinkPrivate));

  gobject_class->get_property = fsu_sink_get_property;
  gobject_class->set_property = fsu_sink_set_property;
  gobject_class->dispose = fsu_sink_dispose;

  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR (fsu_sink_handle_message);
  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (fsu_sink_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (fsu_sink_release_pad);

  g_object_class_install_property (gobject_class, PROP_SINK_NAME,
      g_param_spec_string ("sink-name", "Sink element name",
          "The name of the sink element to create.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SINK_DEVICE,
      g_param_spec_string ("sink-device", "Sink device",
          "The device to use with the sink",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SINK_PIPELINE,
      g_param_spec_string ("sink-pipeline", "Sink pipeline",
          "The pipeline to use for creating the sink",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SYNC,
      g_param_spec_boolean ("sync", "Sync on the clock",
          "Sync on the clock",
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ASYNC,
      g_param_spec_boolean ("async", "Go asynchronously to PAUSED",
          "Go asynchronously to PAUSED",
          TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_sink_init (FsuSink *self, FsuSinkClass *klass)
{
  FsuSinkPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SINK,
          FsuSinkPrivate);

  self->priv = priv;
  priv->sync = priv->async = TRUE;
  priv->filters = fsu_multi_filter_manager_new ();
  if (klass->add_filters)
    klass->add_filters (self, priv->filters);
}

static void
fsu_sink_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuSink *self = FSU_SINK (object);
  FsuSinkPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_SINK_NAME:
      g_value_set_string (value, priv->sink_name);
      break;
    case PROP_SINK_DEVICE:
      g_value_set_string (value, priv->sink_device);
      break;
    case PROP_SINK_PIPELINE:
      g_value_set_string (value, priv->sink_pipeline);
      break;
    case PROP_SYNC:
      g_value_set_boolean (value, priv->sync);
      break;
    case PROP_ASYNC:
      g_value_set_boolean (value, priv->async);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_sink_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuSink *self = FSU_SINK (object);
  FsuSinkPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_SINK_NAME:
      g_free (priv->sink_name);
      priv->sink_name = g_value_dup_string (value);
      break;
    case PROP_SINK_DEVICE:
      g_free (priv->sink_device);
      priv->sink_device = g_value_dup_string (value);
      break;
    case PROP_SINK_PIPELINE:
      g_free (priv->sink_pipeline);
      priv->sink_pipeline = g_value_dup_string (value);
      break;
    case PROP_SYNC:
      priv->sync = g_value_get_boolean (value);
      break;
    case PROP_ASYNC:
      priv->async = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_sink_dispose (GObject *object)
{
  FsuSink *self = FSU_SINK (object);
  FsuSinkPrivate *priv = self->priv;
  GList *item;

 restart:
  GST_OBJECT_LOCK (GST_OBJECT (self));
  for (item = GST_ELEMENT_PADS (object); item; item = g_list_next (item))
  {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SINK (pad))
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      gst_element_release_request_pad (GST_ELEMENT (object), pad);
      goto restart;
    }
  }

  g_free (priv->sink_name);
  priv->sink_name = NULL;
  g_free (priv->sink_device);
  priv->sink_device = NULL;
  g_free (priv->sink_pipeline);
  priv->sink_pipeline = NULL;

  g_object_unref (priv->filters);

  g_assert (!priv->mixer);
  g_assert (!priv->sinks);

  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


void
fsu_sink_element_added (FsuSink *self,
    GstBin *bin,
    GstElement *sink)
{
  if (_fsu_g_object_has_property (G_OBJECT (sink), "sync") &&
      _fsu_g_object_has_property (G_OBJECT (sink), "async"))
  {
    FsuSinkPrivate *priv = self->priv;
    gboolean sync, async;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    sync = priv->sync;
    async = priv->async;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    g_object_set (sink,
        "sync", sync,
        "async", async,
        NULL);
  }
}

static GstElement *
create_auto_sink (FsuSink *self)
{

  if (FSU_SINK_GET_CLASS (self)->create_auto_sink)
    return FSU_SINK_GET_CLASS (self)->create_auto_sink (self);

  return NULL;
}

static gchar *
need_mixer (FsuSink *self,
    GstElement *sink)
{

  if (FSU_SINK_GET_CLASS (self)->need_mixer)
    return FSU_SINK_GET_CLASS (self)->need_mixer (self, sink);

  return NULL;
}

static GstElement *
find_sink (GstElement *snk)
{
  if (GST_IS_BIN (snk))
  {
    GstElement *sink = NULL;
    GstIterator *it = gst_bin_iterate_sinks (GST_BIN(snk));
    gboolean done = FALSE;
    gpointer item = NULL;

    while (!done)
    {
      switch (gst_iterator_next (it, &item))
      {
        case GST_ITERATOR_OK:
          sink = GST_ELEMENT (item);
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

    if (!sink)
      return gst_object_ref (snk);

    snk = find_sink (sink);
    gst_object_unref (sink);
    return snk;
  }
  else
  {
    return gst_object_ref (snk);
  }
}


static GstElement *
create_mixer (FsuSink *self,
    GstElement *sink)
{
  GstElement *mixer = NULL;
  GstPad *sink_pad = NULL;
  GstPad *mixer_pad = NULL;
  GstElement *real_sink = find_sink (sink);
  gchar *mixer_name = need_mixer (self, real_sink);

  gst_object_unref (real_sink);

  if (!mixer_name)
    return NULL;

  mixer = gst_element_factory_make (mixer_name, "sinkmixer");

  if (!mixer)
  {
    WARNING ("Could not create sink mixer");
    return NULL;
  }

  if (!gst_bin_add (GST_BIN (self), mixer))
  {
    WARNING ("Could not add sink mixer to Sink");
    gst_object_unref (mixer);
    return NULL;
  }

  sink_pad = gst_element_get_static_pad (sink, "sink");
  mixer_pad = gst_element_get_static_pad (mixer, "src");
  if (!sink_pad || !mixer_pad)
  {
    WARNING ("Could not get sink pad");
    goto error_added;
  }
  else if (GST_PAD_LINK_FAILED (gst_pad_link (mixer_pad, sink_pad)))
  {
    WARNING ("Couldn't link mixer pad with sink pad");
    goto error_added;
  }

  if (!gst_element_sync_state_with_parent (mixer))
    goto error_state;

  gst_object_unref (sink_pad);
  gst_object_unref (mixer_pad);

  return gst_object_ref (mixer);

 error_state:
  gst_pad_unlink (mixer_pad, sink_pad);
 error_added:
  gst_bin_remove (GST_BIN (self), mixer);
  if (sink_pad)
    gst_object_unref (sink_pad);
  if (mixer_pad)
    gst_object_unref (mixer_pad);
  return NULL;

}

static void
check_and_remove_mixer (FsuSink *self)
{
  FsuSinkPrivate *priv = self->priv;
  GstElement *mixer = NULL;
  GstIterator *iter = NULL;
  gboolean still_has_sink = FALSE;
  gboolean done = FALSE;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  mixer = gst_object_ref (priv->mixer);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  iter = gst_element_iterate_sink_pads (mixer);
  while (!done)
  {
    gpointer item = NULL;
    switch (gst_iterator_next (iter, &item))
    {
      case GST_ITERATOR_OK:
        gst_object_unref (GST_OBJECT (item));
        still_has_sink = TRUE;
        done = TRUE;
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);

  if (!still_has_sink)
  {
    gst_element_set_state (mixer, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (self), mixer);
    gst_object_unref (mixer);

    GST_OBJECT_LOCK (GST_OBJECT (self));
    if (priv->mixer)
    {
      gst_object_unref (priv->mixer);
      priv->mixer = NULL;
    }

    if (priv->sinks)
    {
      GstElement *sink = priv->sinks->data;

      g_assert (g_list_length (priv->sinks) == 1);
      priv->sinks = g_list_remove (priv->sinks, sink);
      GST_OBJECT_UNLOCK (GST_OBJECT (self));

      gst_element_set_state (sink, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (self), sink);
      gst_object_unref (sink);
    }
    else
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
    }
  }
}

static GstPad *
fsu_sink_request_new_pad (GstElement * element,
    GstPadTemplate * templ,
    const gchar * name)
{
  FsuSink *self = FSU_SINK (element);
  FsuSinkPrivate *priv = self->priv;
  GstElement *sink = NULL;
  GstElement *mixer = NULL;
  GstPad *pad = NULL;
  GstPad *sink_pad = NULL;
  GstPad *filter_pad = NULL;

  DEBUG ("requesting pad");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  mixer = priv->mixer;

  /* If this is our first sink or second sink with no mixer*/
  if (!priv->sinks || !mixer)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    sink = create_sink (self);
    if (!sink)
    {
      sink = gst_element_factory_make ("fakesink", NULL);
      if (!sink)
      {
        WARNING ("Could not create sink or fakesink elements");
        goto error_not_added;
      }
      if (!gst_bin_add (GST_BIN (self), sink))
      {
        WARNING ("Could not add sink element to Sink");
        goto error_not_added;
      }

      gst_object_ref (sink);

      /* A fakesink never needs a mixer */
      sink_pad = gst_element_get_static_pad (sink, "sink");
      if (!sink_pad)
      {
        WARNING ("Could not get sink pad from sink");
        goto error_not_filtered;
      }
    }
    else
    {
      if (!gst_bin_add (GST_BIN (self), sink))
      {
        WARNING ("Could not add sink element to Sink");
        goto error_not_added;
      }

      gst_object_ref (sink);

      GST_OBJECT_LOCK (GST_OBJECT (self));
      /* If this isn't our first sink then we don't need a mixer, get the pad */
      if (priv->sinks)
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
        sink_pad = gst_element_get_static_pad (sink, "sink");
        if (!sink_pad)
        {
          WARNING ("Could not get sink pad from sink");
          goto error_not_filtered;
        }
      }
      else
      {
        GST_OBJECT_UNLOCK (GST_OBJECT (self));
      }
    }
  }
  else
  {
    gst_object_ref (mixer);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    /* This isn't our first sink and we already have a mixer */
    sink_pad = gst_element_get_request_pad (mixer, "sink%d");
    if (!sink_pad)
    {
      WARNING ("Couldn't get new request pad from mixer");
      goto error_not_filtered;
    }
  }

  /* Set to READY so the auto*sink can create the underlying sink and the
     subclass can decide whether or not a mixer is needed for that sink */
  if (sink && gst_element_set_state (sink, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE)
  {
    WARNING ("Unable to set sink to READY");
    if (sink_pad)
    {
      gst_object_unref (sink_pad);
      sink_pad = NULL;
    }
    gst_bin_remove (GST_BIN (self), sink);
    gst_object_unref (sink);

    DEBUG ("Replacing sink with fakesink");
    sink = gst_element_factory_make ("fakesink", NULL);
    if (!sink)
    {
      WARNING ("Could not create sink or fakesink elements");
      goto error_not_added;
    }
    if (!gst_bin_add (GST_BIN (self), sink))
    {
      WARNING ("Could not add fakesink element to Sink");
      goto error_not_added;
    }

    gst_object_ref (sink);

    /* A fakesink never needs a mixer */
    sink_pad = gst_element_get_static_pad (sink, "sink");
    if (!sink_pad)
    {
      WARNING ("Could not get sink pad from fakesink");
      goto error_not_filtered;
    }
  }

  /* First sink, we don't know yet if we need a mixer */
  if (!sink_pad)
  {
    mixer = create_mixer (self, sink);
    /* Check if we need a mixer */
    if (!mixer)
      sink_pad = gst_element_get_static_pad (sink, "sink");
    else
      sink_pad = gst_element_get_request_pad (mixer, "sink%d");

    if (!sink_pad)
    {
      WARNING ("Couldn't get new request pad from mixer");
      if (mixer)
      {
        gst_bin_remove (GST_BIN (self), mixer);
        gst_object_unref (mixer);
      }
      mixer = NULL;
      goto error_not_filtered;
    }
    GST_OBJECT_LOCK (GST_OBJECT (self));
    if (priv->mixer)
    {
      /* TODO: we need a mutex for this whole thing to prevent two from being
       * run at the same time */
    }
    if (mixer)
      priv->mixer = gst_object_ref (mixer);
    else
      priv->mixer = NULL;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
  }

  filter_pad = fsu_filter_manager_apply (priv->filters,
      GST_BIN (self), sink_pad);

  if (!filter_pad)
    filter_pad = gst_object_ref (sink_pad);
  gst_object_unref (sink_pad);

  pad = gst_ghost_pad_new (name, filter_pad);

  if (!pad)
  {
    WARNING ("Couldn't create ghost pad for mixer/sink");
    goto error_filtered;
  }
  gst_object_unref (filter_pad);

  gst_pad_set_active (pad, TRUE);

  if (!gst_element_add_pad (element, pad))
  {
    WARNING ("Couldn't add pad");
    goto error_filtered;
  }

  if (sink && !gst_element_sync_state_with_parent (sink))
  {
    WARNING ("Couldn't sync sink state with parent");
    goto error_filtered;
  }

  if (sink)
  {
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->sinks = g_list_prepend (priv->sinks, sink);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
  }

  if (mixer)
    gst_object_unref (mixer);

  return pad;

 error_filtered:
  sink_pad = fsu_filter_manager_revert (priv->filters,
      GST_BIN (self), filter_pad);
 error_not_filtered:
  if (mixer)
  {
    if (sink_pad)
      gst_element_release_request_pad (mixer, sink_pad);
    gst_object_unref (mixer);
    check_and_remove_mixer (self);
  }
  if (sink)
    gst_bin_remove (GST_BIN (self), sink);
 error_not_added:
  if (sink)
    gst_object_unref (sink);
  if (sink_pad)
    gst_object_unref (sink_pad);
  if (filter_pad)
    gst_object_unref (filter_pad);

  return NULL;

}

static void
fsu_sink_release_pad (GstElement * element,
    GstPad * pad)
{
  FsuSink *self = FSU_SINK (element);
  FsuSinkPrivate *priv = self->priv;

  DEBUG ("releasing pad");

  gst_pad_set_active (pad, FALSE);

  if (GST_IS_GHOST_PAD (pad))
  {
    GstPad *filter_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
    GstPad *sink_pad = NULL;
    GstElement *mixer = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    mixer = priv->mixer;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    sink_pad = fsu_filter_manager_revert (priv->filters,
        GST_BIN (self), filter_pad);
    if (!sink_pad)
      sink_pad = gst_object_ref (filter_pad);
    gst_object_unref (filter_pad);

    if (mixer)
    {
      gst_element_release_request_pad (mixer, sink_pad);
      gst_object_unref (sink_pad);
      check_and_remove_mixer (self);
    }
    else
    {
      GstElement *sink = GST_ELEMENT (
          gst_object_get_parent (GST_OBJECT (sink_pad)));
      gst_object_unref (sink_pad);
      gst_element_set_state (sink, GST_STATE_NULL);
      gst_bin_remove (GST_BIN (self), sink);

      GST_OBJECT_LOCK (GST_OBJECT (self));
      priv->sinks = g_list_remove (priv->sinks, sink);
      GST_OBJECT_UNLOCK (GST_OBJECT (self));

      gst_object_unref (sink);
      /* From the get_parent */
      gst_object_unref (sink);
    }
  }

  gst_element_remove_pad (element, pad);

}

static GstElement *
create_sink (FsuSink *self)
{
  FsuSinkPrivate *priv = self->priv;
  GstElement *sink = NULL;
  gchar *auto_sink_name = FSU_SINK_GET_CLASS (self)->auto_sink_name;
  gchar *sink_pipeline = NULL;
  gchar *sink_name = NULL;
  gchar *sink_device = NULL;
  gboolean sync, async;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  sync = priv->sync;
  async = priv->async;
  sink_pipeline = g_strdup (priv->sink_pipeline);
  sink_name = g_strdup (priv->sink_name);
  sink_device = g_strdup (priv->sink_device);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  DEBUG ("Creating sink : %s -- %s (%s)",
      sink_pipeline ? sink_pipeline : "(null)",
      sink_name ? sink_name : "(null)",
      sink_device ? sink_device : "(null)");

  if (sink_pipeline)
  {
    GstBin *bin = NULL;
    gchar *desc = NULL;
    GError *error  = NULL;
    GstElement *real_sink = NULL;

    /* parse the pipeline to a bin */
    desc = g_strdup_printf ("bin.( queue ! %s )", sink_pipeline);
    bin = (GstBin *) gst_parse_launch (desc, &error);
    g_free (desc);

    if (bin)
    {
      GstPad *pad = NULL;
      /* find pads and ghost them if necessary */
      pad = gst_bin_find_unlinked_pad (bin, GST_PAD_SINK);
      if (pad)
      {
        gst_element_add_pad (GST_ELEMENT (bin),
            gst_ghost_pad_new ("sink", pad));
        gst_object_unref (pad);
      }
    }
    if (error)
    {
      WARNING ("Error while creating sink-pipeline (%d): %s",
          error->code, (error->message? error->message : "(null)"));
      goto done;
    }

    sink = GST_ELEMENT (bin);
    real_sink = find_sink (sink);
    if (_fsu_g_object_has_property (G_OBJECT (real_sink), "sync") &&
        _fsu_g_object_has_property (G_OBJECT (real_sink), "async"))
      g_object_set(real_sink,
          "sync", sync,
          "async", async,
          NULL);
    gst_object_unref (real_sink);

    goto done;
  }
  else if (sink_name &&
      (!auto_sink_name || strcmp (sink_name, auto_sink_name)))
  {
    sink = gst_element_factory_make (sink_name, NULL);

    if (sink && sink_device)
      g_object_set(sink,
          "device", sink_device,
          "sync", sync,
          "async", async,
          NULL);
    else if (sink)
      g_object_set(sink,
          "sync", sync,
          "async", async,
          NULL);

    goto done;
  }

  sink = create_auto_sink (self);

 done:
  g_free (sink_pipeline);
  g_free (sink_name);
  g_free (sink_device);

  return sink;
}

static void
fsu_sink_handle_message (GstBin *bin,
    GstMessage *message)
{
  FsuSink *self = FSU_SINK (bin);
  //FsuSinkPrivate *priv = self->priv;

  DEBUG ("Got message of type %s from %s", GST_MESSAGE_TYPE_NAME(message),
      GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)));

  /* TODO: handle_message on the filter manager */
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
  {
    gst_message_unref (message);
    return;
  }
  GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}
