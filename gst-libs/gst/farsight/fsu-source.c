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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <string.h>
#include <gst/interfaces/propertyprobe.h>

#include <gst/farsight/fsu-source-class.h>
#include <gst/farsight/fsu-common.h>


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

/* properties */
enum
{
  PROP_DISABLED = 1,
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
  /* The src tee coming out of the source element */
  GstElement *tee;
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
  /* The list of FsuFilterManager filters to apply on the source */
  GList *filters;
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

  switch (property_id)
  {
    case PROP_DISABLED:
      priv->disabled = g_value_get_boolean (value);
      break;
    case PROP_SOURCE_NAME:
      priv->source_name = g_value_dup_string (value);
      break;
    case PROP_SOURCE_DEVICE:
      priv->source_device = g_value_dup_string (value);
      break;
    case PROP_SOURCE_PIPELINE:
      priv->source_pipeline = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
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

  priv->current_filtered_source = NULL;

  for (walk = priv->filtered_sources; walk; walk = g_list_next (walk))
  {
    if (walk->data)
      gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
  }
  g_list_free (priv->filtered_sources);
  priv->filtered_sources = NULL;

  if (priv->source_name)
    g_free (priv->source_name);
  priv->source_name = NULL;
  if (priv->source_device)
    g_free (priv->source_device);
  priv->source_device = NULL;
  if (priv->source_pipeline)
    g_free (priv->source_pipeline);
  priv->source_pipeline = NULL;

  if (priv->source)
    gst_object_unref (priv->source);
  priv->source = NULL;
  if (priv->tee)
    gst_object_unref (priv->tee);
  priv->tee = NULL;

  priv->priority_source_ptr = NULL;

  g_assert (priv->filters == NULL);
  g_assert (priv->thread == NULL);

  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static void
add_filters (FsuSource *self,
    FsuFilterManager *manager)
{
  if (FSU_SOURCE_GET_CLASS (self)->add_filters)
    FSU_SOURCE_GET_CLASS (self)->add_filters (self, manager);
}

static void
check_and_remove_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GList *item;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  for (item = GST_ELEMENT_PADS (self); item; item = g_list_next (item))
  {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SRC (pad))
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      return;
    }
  }

  DEBUG ("No more request source pads");
  if (priv->tee)
  {
    GstElement *tee = priv->tee;

    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    gst_bin_remove (GST_BIN (self), tee);
    gst_element_set_state (tee, GST_STATE_NULL);
    gst_object_unref (tee);

    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->tee = NULL;
  }
  if (priv->source)
  {
    GstElement *source = priv->source;

    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    gst_bin_remove (GST_BIN (self), source);
    gst_element_set_state (source, GST_STATE_NULL);
    gst_object_unref (source);

    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->source = NULL;
  }
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
}

static void
create_source_and_link_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *src = NULL;
  GstPad *src_pad = NULL;
  GstPad *tee_pad = NULL;

  /* TODO: mutex on these ? */
  g_return_if_fail (priv->tee);
  g_return_if_fail (!priv->source);

  src = create_source (self);
  if (src)
  {
    if (!gst_bin_add (GST_BIN (self), src))
    {
      WARNING ("Could not add source element to Source");
      gst_object_unref (src);
      return;
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
      if (gst_pad_link (src_pad, tee_pad) != GST_PAD_LINK_OK)
      {
        WARNING ("Couldn't link source pad with src tee");
        gst_object_unref (tee_pad);
        gst_object_unref (src_pad);
        gst_bin_remove (GST_BIN (self), src);
        check_and_remove_tee (self);
        return;
      }
      if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
        gst_element_sync_state_with_parent  (src);
      gst_object_unref (tee_pad);
    }
    else
    {
      WARNING ("Could not get tee pad to link with source");
      gst_object_unref (src_pad);
      gst_bin_remove (GST_BIN (self), src);
      check_and_remove_tee (self);
      return;
    }

    gst_object_unref (src_pad);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->source = gst_object_ref (src);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
  }

}

static void
create_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *tee = NULL;

  tee = gst_element_factory_make ("tee", "srctee");

  if (!tee)
  {
    WARNING ("Could not create src tee");
    return;
  }
  if (!gst_bin_add (GST_BIN (self), tee))
  {
    WARNING ("Could not add src tee to Source");
    gst_object_unref (tee);
    return;
  }
  GST_OBJECT_LOCK (GST_OBJECT (self));
  priv->tee = gst_object_ref (tee);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
  {
    gst_element_sync_state_with_parent  (tee);
    create_source_and_link_tee (self);
  }
  else
  {
    DEBUG ("State NULL, not creating source now...");
  }

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
  FsuFilterManager *filter = NULL;
  GstElement *tee = NULL;

  DEBUG ("requesting pad");

  GST_OBJECT_LOCK (GST_OBJECT (self));
  if (!priv->tee)
  {
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    create_tee (self);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    if (!priv->tee)
    {
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      WARNING ("Couldn't create a tee to request pad from");
      return NULL;
    }
  }

  tee = gst_object_ref (priv->tee);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  tee_pad = gst_element_get_request_pad (tee, "src%d");
  gst_object_unref (tee);

  if (!tee_pad)
  {
    WARNING ("Couldn't get new request pad from src tee");
    check_and_remove_tee (self);
    return NULL;
  }

  filter = fsu_single_filter_manager_new ();
  add_filters (self, filter);

  GST_OBJECT_LOCK (GST_OBJECT (self));
  priv->filters = g_list_prepend (priv->filters, filter);
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  filter_pad = fsu_filter_manager_apply (filter, GST_BIN (self), tee_pad);

  if (!filter_pad)
  {
    WARNING ("Could not add filters to source tee pad");
    filter_pad = gst_object_ref (tee_pad);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->filters = g_list_remove (priv->filters, filter);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    g_object_unref (filter);
    filter = NULL;
  }
  gst_object_unref (tee_pad);

  pad = gst_ghost_pad_new (name, filter_pad);
  gst_object_unref (filter_pad);
  if (!pad)
  {
    WARNING ("Couldn't create ghost pad for tee");
    check_and_remove_tee (self);
    return NULL;
  }

  gst_pad_set_active (pad, TRUE);

  gst_element_add_pad (element, pad);

  return pad;
}

static void
fsu_source_release_pad (GstElement * element,
    GstPad * pad)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;

  DEBUG ("releasing pad");

  gst_pad_set_active (pad, FALSE);

  if (GST_IS_GHOST_PAD (pad))
  {
    GstPad *filter_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
    GstPad *tee_pad = NULL;
    GList *i = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    i = priv->filters;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    for (; i; i = i->next)
    {
      FsuFilterManager *manager = i->data;
      GstPad *out_pad = NULL;
      g_object_get (manager,
          "out-pad", &out_pad,
          NULL);
      if (out_pad == filter_pad)
      {
        tee_pad = fsu_filter_manager_revert (manager, GST_BIN (self),
            filter_pad);

        GST_OBJECT_LOCK (GST_OBJECT (self));
        priv->filters = g_list_remove (priv->filters, manager);
        GST_OBJECT_UNLOCK (GST_OBJECT (self));

        g_object_unref (manager);
        gst_object_unref (out_pad);
        break;
      }
      gst_object_unref (out_pad);
    }
    gst_object_unref (filter_pad);
    if (tee_pad)
    {
      GstElement *tee = NULL;

      GST_OBJECT_LOCK (GST_OBJECT (self));
      tee = priv->tee;
      GST_OBJECT_UNLOCK (GST_OBJECT (self));

      gst_element_release_request_pad (tee, tee_pad);
      gst_object_unref (tee_pad);
    }
  }

  gst_element_remove_pad (element, pad);

  check_and_remove_tee (self);
}

static const gchar *
get_device_property_name (GstElement *element)
{
  if (g_object_has_property (G_OBJECT (element), "device") &&
      !g_object_has_property (G_OBJECT (element), "device-name"))
  {
    return "device";
  }
  else if (!g_object_has_property (G_OBJECT (element), "device") &&
      g_object_has_property (G_OBJECT (element), "device-name"))
  {
    return "device-name";
  }
  else if (g_object_has_property (G_OBJECT (element), "device") &&
      g_object_has_property (G_OBJECT (element), "device-name"))
  {
    if (!strcmp (GST_ELEMENT_NAME (element), "dshowaudiosrc") ||
        !strcmp (GST_ELEMENT_NAME (element), "dshowvideosrc"))
      return "device-name";
    else
      return "device";
  }
  else
  {
    return NULL;
  }
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


static GstElement *
test_source (FsuSource *self,
    const gchar *name)
{
  FsuSourcePrivate *priv = self->priv;
  GstPropertyProbe *probe = NULL;
  GstElement *element = NULL;
  GstState target_state = GST_STATE_READY;
  const gchar * target_state_name = "READY";
  GstStateChangeReturn state_ret;
  GValueArray *arr;
  const gchar **blacklist = FSU_SOURCE_GET_CLASS (self)->blacklisted_sources;
  gint probe_idx;

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

  GST_OBJECT_LOCK (GST_OBJECT (self));
  probe_idx = priv->probe_idx;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  DEBUG ("Testing source %s. probe idx = %d", name, probe_idx);

  for (;blacklist && *blacklist; blacklist++)
  {
    if (!strcmp (name, *blacklist))
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

  if (GST_IS_PROPERTY_PROBE (element))
  {
    probe = GST_PROPERTY_PROBE (element);
    if (probe && get_device_property_name(element))
    {
      arr = gst_property_probe_probe_and_get_values_name (probe,
          get_device_property_name(element));
      if (arr)
      {
        guint i;
        for (i = probe_idx; i < arr->n_values; i++)
        {
          const gchar *device;
          GValue *val;

          val = g_value_array_get_nth (arr, i);
          if (!val || !G_VALUE_HOLDS_STRING (val))
            continue;

          device = g_value_get_string (val);
          if (!device)
            continue;

          DEBUG ("Testing device %s", device);
          g_object_set(element,
              get_device_property_name(element), device,
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
            g_value_array_free (arr);

            GST_OBJECT_LOCK (GST_OBJECT (self));
            priv->probe_idx = i + 1;
            GST_OBJECT_UNLOCK (GST_OBJECT (self));

            return element;
          }
        }
        g_value_array_free (arr);
      }
    }
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
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  DEBUG ("Creating source : %s -- %s (%s)",
      source_pipeline ? source_pipeline : "(null)",
      source_name ? source_name : "(null)",
      source_device ? source_device : "(null)");

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
    if (state_ret == GST_STATE_CHANGE_ASYNC)
    {
      DEBUG ("Waiting for source-pipeline to go to state READY");
      state_ret = gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }

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

    src = gst_element_factory_make (source_name, NULL);

    if (src && source_device && get_device_property_name(src))
      g_object_set(src,
          get_device_property_name(src), source_device,
          NULL);

    if (src)
    {
      state_ret = gst_element_set_state (src, GST_STATE_READY);
      if (state_ret == GST_STATE_CHANGE_ASYNC)
      {
        DEBUG ("Waiting for %s to go to state READY", source_name);
        state_ret = gst_element_get_state (src, NULL, NULL,
            GST_CLOCK_TIME_NONE);
      }

      if (state_ret == GST_STATE_CHANGE_FAILURE)
      {
        DEBUG ("Unable to set source to READY");
        gst_object_unref (src);
        goto error;
      }
    }

  }
  else
  {
    GList *walk;
    const gchar **priority_source = NULL;

    g_free (source_pipeline);
    g_free (source_name);
    g_free (source_device);

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
        walk = get_plugins_filtered (FSU_SOURCE_GET_CLASS (self)->klass_check);

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

  if (src)
  {
    GST_OBJECT_FLAG_UNSET (src, GST_ELEMENT_IS_SINK);
    real_src = find_source (src);
    if (g_object_has_property (G_OBJECT (real_src), "buffer-time"))
      g_object_set(real_src, "buffer-time", G_GINT64_CONSTANT(20000), NULL);

    if (g_object_has_property (G_OBJECT (real_src), "is-live"))
      g_object_set(real_src, "is-live", TRUE, NULL);

    gst_object_unref (real_src);
  }

  return src;

 error:
  g_free (source_pipeline);
  g_free (source_name);
  g_free (source_device);

  return NULL;
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

        GST_OBJECT_UNLOCK (GST_OBJECT (self));
        DEBUG ("Setting source to state NULL");
        ret = gst_element_set_state (source, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_ASYNC)
        {
          DEBUG ("Waiting for source to go to state NULL");
          ret = gst_element_get_state (source, NULL, NULL,
              GST_CLOCK_TIME_NONE);
        }
        gst_bin_remove (GST_BIN (self), source);
        gst_object_unref (source);
        GST_OBJECT_LOCK (GST_OBJECT (self));
        priv->source = NULL;
      }
      GST_OBJECT_UNLOCK (GST_OBJECT (self));
      break;
    default:
      break;
  }

  return ret;
}

static gpointer
replace_source_thread (gpointer data)
{
  FsuSource *self = FSU_SOURCE (data);
  FsuSourcePrivate *priv = self->priv;
  GstStateChangeReturn state_ret;
  GstElement *source = NULL;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  source = priv->source;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));

  if (source)
  {
    state_ret = gst_element_set_state (source, GST_STATE_NULL);
    if (state_ret == GST_STATE_CHANGE_ASYNC)
    {
      DEBUG ("Waiting for source to go to state NULL");
      gst_element_get_state (source, NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }
    gst_object_unref (source);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->source = NULL;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    create_source_and_link_tee (self);

    GST_OBJECT_LOCK (GST_OBJECT (self));
    source = priv->source;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    if (source && GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
      gst_element_sync_state_with_parent  (source);
  }

  GST_OBJECT_LOCK (GST_OBJECT (self));
  priv->thread = NULL;
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
  gst_object_unref (self);

  return NULL;
}

static void
destroy_source (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GError *error = NULL;

  GST_OBJECT_LOCK (GST_OBJECT (self));
  if (priv->source && !priv->thread)
  {
    GstElement *source = priv->source;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    gst_bin_remove (GST_BIN (self), source);

    gst_object_ref (self);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->thread = g_thread_create (replace_source_thread, self, FALSE, &error);

    if (!priv->thread)
    {
      WARNING ("Could not start stopping thread for FsuSource:"
          " %s", error->message);
    }
    g_clear_error (&error);
  }
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
}

static void
fsu_source_handle_message (GstBin *bin,
    GstMessage *message)
{
  FsuSource *self = FSU_SOURCE (bin);
  FsuSourcePrivate *priv = self->priv;

  DEBUG ("Got message of type %s from %s", GST_MESSAGE_TYPE_NAME(message),
      GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)));

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
  {
    GError *error;
    GstElement *source = NULL;
    GstElement *real_source = NULL;

    GST_OBJECT_LOCK (GST_OBJECT (self));
    source = gst_object_ref (priv->source);
    GST_OBJECT_UNLOCK (GST_OBJECT (self));

    real_source = find_source (source);
    gst_object_unref (source);

    gst_message_parse_error (message, &error, NULL);

    if (error->domain == GST_RESOURCE_ERROR &&
        GST_MESSAGE_SRC (message) == GST_OBJECT (real_source))
    {
      DEBUG ("Source got an error. setting source to state NULL");
      destroy_source (self);
      gst_message_unref (message);
      gst_object_unref (real_source);
      return;
    }
    gst_object_unref (real_source);
  }
  GST_BIN_CLASS (parent_class)->handle_message (bin, message);
}
