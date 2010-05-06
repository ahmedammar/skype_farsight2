/*
 * fsu-sink.c - Sink for FsuSink
 *
 * Copyright (C) 2010 Collabora Ltd.

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

#include "fsu-sink.h"
#include "fsu-common.h"


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
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_sink_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);

static GstPad *fsu_sink_request_new_pad (GstElement * element,
  GstPadTemplate * templ, const gchar * name);
static void fsu_sink_release_pad (GstElement * element, GstPad * pad);
static GstElement *create_sink (FsuSink *self);

/* properties */
enum {
  PROP_SINK_NAME = 1,
  PROP_SINK_DEVICE,
  PROP_SINK_PIPELINE,
  LAST_PROPERTY
};

struct _FsuSinkPrivate
{
  gboolean dispose_has_run;

  /* Properties */
  gchar *sink_name;
  gchar *sink_device;
  gchar *sink_pipeline;

  /* The mixer element to mix */
  GstElement *mixer;
  GstElement *sink;
  GList *sinks;
  /* The list of FsuFilterManager filters to apply on the sink */
  GList *filters;
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

  g_type_class_add_private (klass, sizeof (FsuSinkPrivate));

  gobject_class->get_property = fsu_sink_get_property;
  gobject_class->set_property = fsu_sink_set_property;
  gobject_class->dispose = fsu_sink_dispose;

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
}

static void
fsu_sink_init (FsuSink *self, FsuSinkClass *klass)
{
  FsuSinkPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SINK,
          FsuSinkPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
}

static void
fsu_sink_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuSink *self = FSU_SINK (object);
  FsuSinkPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_SINK_NAME:
      g_value_set_string (value, priv->sink_name);
      break;
    case PROP_SINK_DEVICE:
      g_value_set_string (value, priv->sink_device);
      break;
    case PROP_SINK_PIPELINE:
      g_value_set_string (value, priv->sink_pipeline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_sink_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuSink *self = FSU_SINK (object);
  FsuSinkPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_SINK_NAME:
      priv->sink_name = g_value_dup_string (value);
      break;
    case PROP_SINK_DEVICE:
      priv->sink_device = g_value_dup_string (value);
      break;
    case PROP_SINK_PIPELINE:
      priv->sink_pipeline = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_sink_dispose (GObject *object)
{
  FsuSink *self = (FsuSink *)object;
  FsuSinkPrivate *priv = self->priv;
  GList *item;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

 restart:
  for (item = GST_ELEMENT_PADS (object); item; item = g_list_next (item)) {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SINK (pad)) {
      gst_element_release_request_pad (GST_ELEMENT (object), pad);
      goto restart;
    }
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static GstElement *
create_auto_sink (FsuSink *self)
{
  GstElement *(*func)(FsuSink *self) = \
      FSU_SINK_GET_CLASS (self)->create_auto_sink;

  if (func != NULL)
    return func (self);

  return NULL;
}

static gchar *
need_mixer (FsuSink *self, GstElement *sink)
{
  gchar *(*func) (FsuSink *, GstElement *) = \
      FSU_SINK_GET_CLASS (self)->need_mixer;

  if (func != NULL)
    return func (self, sink);

  return NULL;
}

static GstElement *
find_sink (GstElement *snk)
{
  GstElement *sink = NULL;

  if (GST_IS_BIN (snk)) {
    GstIterator *it = gst_bin_iterate_sinks (GST_BIN(snk));
    gboolean done = FALSE;
    gpointer item = NULL;

    while (!done) {
      switch (gst_iterator_next (it, &item)) {
        case GST_ITERATOR_OK:
          sink = GST_ELEMENT (item);
          gst_object_unref (sink);
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

    if (sink == NULL)
      return snk;
  } else {
    return snk;
  }
  return find_sink (sink);
}


static GstElement *
create_mixer (FsuSink *self, GstElement *sink)
{
  GstElement *mixer = NULL;
  GstPad *sink_pad = NULL;
  GstPad *mixer_pad = NULL;
  gchar *mixer_name = need_mixer (self, find_sink (sink));

  if (mixer_name == NULL)
    return NULL;

  mixer = gst_element_factory_make (mixer_name, "sinkmixer");

  if (mixer == NULL) {
    WARNING ("Could not create sink mixer");
    return NULL;
  }

  if (gst_bin_add (GST_BIN (self), mixer) == FALSE) {
    WARNING ("Could not add sink mixer to Sink");
    gst_object_unref (mixer);
    return NULL;
  }

  sink_pad = gst_element_get_static_pad (sink, "sink");
  mixer_pad = gst_element_get_static_pad (mixer, "src");
  if (sink_pad == NULL || mixer_pad == NULL) {
    WARNING ("Could not get sink pad");
    gst_bin_remove (GST_BIN (self), mixer);
    if (sink_pad != NULL)
      gst_object_unref (sink_pad);
    if (mixer_pad != NULL)
      gst_object_unref (mixer_pad);
    return NULL;
  } else {
    if (gst_pad_link (mixer_pad, sink_pad) != GST_PAD_LINK_OK) {
      WARNING ("Couldn't link mixer pad with sink pad");
      gst_bin_remove (GST_BIN (self), mixer);
      return NULL;
    }
    gst_object_unref (sink_pad);
    gst_object_unref (mixer_pad);
  }

  gst_element_sync_state_with_parent (mixer);
  return mixer;
}

static void
add_filters (FsuSink *self, FsuFilterManager *manager)
{
  void (*func) (FsuSink *self, FsuFilterManager *manager) = \
      FSU_SINK_GET_CLASS (self)->add_filters;

  if (func != NULL)
    func (self, manager);
}


static GstPad *
fsu_sink_request_new_pad (GstElement * element, GstPadTemplate * templ,
  const gchar * name)
{
  FsuSink *self = FSU_SINK (element);
  FsuSinkPrivate *priv = self->priv;
  GstElement *sink = NULL;
  GstPad *pad = NULL;
  GstPad *sink_pad = NULL;
  GstPad *filter_pad = NULL;
  FsuFilterManager *filter = NULL;

  DEBUG ("requesting pad");

  /* If this is our first sink or second sink with no mixer*/
  if (priv->sinks == NULL ||
      priv->mixer == NULL) {
    sink = create_sink (self);
    if (sink == NULL) {
      sink = gst_element_factory_make ("fakesink", NULL);
      if (sink == NULL) {
        WARNING ("Could not create sink or fakesink elements");
        return NULL;
      }
      if (gst_bin_add (GST_BIN (self), sink) == FALSE) {
        WARNING ("Could not add sink element to Sink");
        gst_object_unref (sink);
        return NULL;
      }

      /* A fakesink never needs a mixer */
      sink_pad = gst_element_get_static_pad (sink, "sink");
      if (sink_pad == NULL) {
        WARNING ("Could not get sink pad from sink");
        gst_object_unref (sink);
        return NULL;
      }
    } else {
      if (gst_bin_add (GST_BIN (self), sink) == FALSE) {
        WARNING ("Could not add sink element to Sink");
        gst_object_unref (sink);
        return NULL;
      }
      /* If this isn't our first sink, then we don't need a mixer, get the pad */
      if (priv->sinks != NULL) {
        sink_pad = gst_element_get_static_pad (sink, "sink");
        if (sink_pad == NULL) {
          WARNING ("Could not get sink pad from sink");
          gst_object_unref (sink);
          return NULL;
        }
      }
    }
  } else {
    /* This isn't our first sink and we already have a mixer */
    sink_pad = gst_element_get_request_pad (priv->mixer, "sink%d");
    if (sink_pad == NULL) {
      WARNING ("Couldn't get new request pad from mixer");
      return NULL;
    }
  }

  /* Set to READY so the auto*sink can create the underlying sink and the
     subclass can decide whether or not a mixer is needed for that sink */
  if (gst_element_set_state (sink, GST_STATE_READY) ==
      GST_STATE_CHANGE_FAILURE) {
    WARNING ("Unable to set sink to READY");
    gst_bin_remove (GST_BIN (self), sink);
    if (sink_pad != NULL)
      gst_object_unref (sink_pad);
    return NULL;
  }

  /* First sink, we don't know yet if we need a mixer */
  if (sink_pad == NULL) {
    priv->mixer = create_mixer (self, sink);
    /* Check if we need a mixer */
    if (priv->mixer == NULL)
      sink_pad = gst_element_get_static_pad (sink, "sink");
    else
      sink_pad = gst_element_get_request_pad (priv->mixer, "sink%d");

    if (sink_pad == NULL) {
      WARNING ("Couldn't get new request pad from mixer");
      if (sink != NULL)
        gst_bin_remove (GST_BIN (self), sink);
      if (priv->mixer != NULL)
        gst_bin_remove (GST_BIN (self), priv->mixer);
      priv->mixer = NULL;
      return NULL;
    }
  }

  filter = fsu_filter_manager_new ();
  add_filters (self, filter);
  priv->filters = g_list_prepend (priv->filters, filter);

  filter_pad = fsu_filter_manager_apply (filter, GST_BIN (self), sink_pad, NULL);

  if (filter_pad == NULL) {
    WARNING ("Could not add filters to sink pad");
    filter_pad = sink_pad;
  }

  pad = gst_ghost_pad_new (name, filter_pad);
  gst_object_unref (filter_pad);

  if (pad == NULL) {
    WARNING ("Couldn't create ghost pad for tee");
    if (sink != NULL)
      gst_bin_remove (GST_BIN (self), sink);
    if (priv->mixer != NULL)
      gst_bin_remove (GST_BIN (self), priv->mixer);
    priv->mixer = NULL;
    return NULL;
  }

  gst_pad_set_active (pad, TRUE);

  gst_element_add_pad (element, pad);

  gst_element_sync_state_with_parent (sink);
  if (priv->mixer != NULL)
    gst_element_sync_state_with_parent (priv->mixer);

  priv->sinks = g_list_prepend (priv->sinks, sink);

  return pad;
}

static void
fsu_sink_release_pad (GstElement * element, GstPad * pad)
{
  FsuSink *self = FSU_SINK (element);
  FsuSinkPrivate *priv = self->priv;

  DEBUG ("releasing pad");

  gst_pad_set_active (pad, FALSE);

  if (GST_IS_GHOST_PAD (pad)) {
    GstPad *filter_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
    GstPad *mixer_pad = NULL;
    GList *i = NULL;

    for (i = priv->filters; i && mixer_pad == NULL; i = i->next) {
      FsuFilterManager *manager = i->data;
      mixer_pad = fsu_filter_manager_revert (manager,
          GST_BIN (self), filter_pad);
    }
    gst_object_unref (filter_pad);
    if (priv->mixer != NULL && mixer_pad != NULL) {
      gst_element_release_request_pad (priv->mixer, mixer_pad);
      gst_object_unref (mixer_pad);
    }
  }

  gst_element_remove_pad (element, pad);

}

static GstElement *
create_sink (FsuSink *self)
{
  FsuSinkPrivate *priv = self->priv;
  gchar *auto_sink_name = FSU_SINK_GET_CLASS (self)->auto_sink_name;

  DEBUG ("Creating sink : %p - %s -- %s (%s)",
      priv->sink,
      priv->sink_pipeline != NULL ? priv->sink_pipeline : "(null)",
      priv->sink_name != NULL ? priv->sink_name : "(null)",
      priv->sink_device != NULL ? priv->sink_device : "(null)");

  if (priv->sink != NULL) {
    return priv->sink;
  } else if (priv->sink_pipeline != NULL) {
    GstPad *pad = NULL;
    GstBin *bin = NULL;
    gchar *desc = NULL;
    GError *error  = NULL;

    /* parse the pipeline to a bin */
    desc = g_strdup_printf ("bin.( queue ! %s )", priv->sink_pipeline);
    bin = (GstBin *) gst_parse_launch (desc, &error);
    g_free (desc);

    if (bin != NULL) {
      /* find pads and ghost them if necessary */
      if ((pad = gst_bin_find_unlinked_pad (bin, GST_PAD_SINK))) {
        gst_element_add_pad (GST_ELEMENT (bin), gst_ghost_pad_new ("sink", pad));
        gst_object_unref (pad);
      }
    }
    if (error) {
      WARNING ("Error while creating sink-pipeline (%d): %s",
          error->code, (error->message != NULL? error->message : "(null)"));
      return NULL;
    }

    return GST_ELEMENT (bin);

  } else if (priv->sink_name != NULL &&
      (auto_sink_name == NULL ||
          strcmp (priv->sink_name, auto_sink_name) != 0)) {
    GstElement *sink = NULL;

    sink = gst_element_factory_make (priv->sink_name, NULL);

    if (sink && priv->sink_device)
        g_object_set(sink,
            "device", priv->sink_device,
            NULL);

      return sink;
  }

  return create_auto_sink (self);
}
