/*
 * fsu-source.c - Source for FsuSource
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

#include <string.h>
#include <gst/interfaces/propertyprobe.h>

#include "fsu-source.h"
#include "fsu-common.h"


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
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_source_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);

static GstStateChangeReturn fsu_source_change_state (GstElement *element,
    GstStateChange transition);
static void fsu_source_handle_message (GstBin *bin, GstMessage *message);
static GstPad *fsu_source_request_new_pad (GstElement * element,
  GstPadTemplate * templ, const gchar * name);
static void fsu_source_release_pad (GstElement * element, GstPad * pad);
static GstElement *create_source (FsuSource *self);

/* properties */
enum {
  PROP_DISABLED = 1,
  PROP_SOURCE_NAME,
  PROP_SOURCE_DEVICE,
  PROP_SOURCE_PIPELINE,
  LAST_PROPERTY
};

struct _FsuSourcePrivate
{
  gboolean dispose_has_run;

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
  priv->dispose_has_run = FALSE;
  priv->probe_idx = -1;
}

static void
fsu_source_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuSource *self = FSU_SOURCE (object);
  FsuSourcePrivate *priv = self->priv;

  switch (property_id) {
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
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuSource *self = FSU_SOURCE (object);
  FsuSourcePrivate *priv = self->priv;

  switch (property_id) {
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
  FsuSource *self = (FsuSource *)object;
  FsuSourcePrivate *priv = self->priv;
  GList *item, *sources, *walk;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

 restart:
  for (item = GST_ELEMENT_PADS (object); item; item = g_list_next (item)) {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SRC (pad)) {
      gst_element_release_request_pad (GST_ELEMENT (object), pad);
      goto restart;
    }
  }

  priv->current_filtered_source = NULL;

  for (walk = priv->filtered_sources; walk; walk = g_list_next (walk)) {
    if (walk->data)
      gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
  }
  g_list_free (priv->filtered_sources);
  priv->filtered_sources = NULL;


  G_OBJECT_CLASS (parent_class)->dispose (object);
}


static GstPad *
add_converters (FsuSource *self, GstPad *pad)
{
  GstPad *(*func) (FsuSource *self, GstPad *pad) = \
      FSU_SOURCE_GET_CLASS (self)->add_converters;

  if (func != NULL)
    return func (self, pad);

  return pad;
}

static void
create_source_and_link_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *src = NULL;
  GstPad *src_pad = NULL;
  GstPad *tee_pad = NULL;

  g_return_if_fail (priv->tee != NULL);
  g_return_if_fail (priv->source == NULL);

  src = create_source (self);
  if (src != NULL) {
    if (gst_bin_add (GST_BIN (self), src) == FALSE) {
      WARNING ("Could not add source element to Source");
      return;
    }

    src_pad = gst_element_get_static_pad (src, "src");
  } else {
    WARNING ("Could not find a usable source");
  }

  if (src_pad != NULL) {
    tee_pad = gst_element_get_static_pad (priv->tee, "sink");

    if (tee_pad != NULL) {
      if (gst_pad_link (src_pad, tee_pad) != GST_PAD_LINK_OK) {
        WARNING ("Couldn't link source pad with src tee");
        gst_bin_remove (GST_BIN (self), src);
        return;
      }
      gst_object_unref (tee_pad);
    }

    gst_object_unref (src_pad);
    priv->source = src;
  }

}

static GstElement *
create_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;

  priv->tee = gst_element_factory_make ("tee", "srctee");

  if (priv->tee == NULL) {
    WARNING ("Could not create src tee");
    return NULL;
  }

  if (gst_bin_add (GST_BIN (self), priv->tee) == FALSE) {
    WARNING ("Could not add src tee to Source");
    gst_object_unref (priv->tee);
    priv->tee = NULL;
    return NULL;
  }

  if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL) {
    create_source_and_link_tee (self);
  } else {
    DEBUG ("State NULL, not creating source now...");
  }

  return priv->tee;
}

static GstPad *
fsu_source_request_new_pad (GstElement * element, GstPadTemplate * templ,
  const gchar * name)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;
  GstPad *pad = NULL;
  GstPad *tee_pad = NULL;

  DEBUG ("requesting pad");

  if (priv->tee == NULL)
    create_tee (self);

  if (priv->tee == NULL) {
    WARNING ("Couldn't create a tee to request pad from");
    return NULL;
  }
  tee_pad = gst_element_get_request_pad (priv->tee, "src%d");
  if (tee_pad == NULL) {
    WARNING ("Couldn't get new request pad from src tee");
    return NULL;
  }

  tee_pad = add_converters (self, tee_pad);

  pad = gst_ghost_pad_new (name, tee_pad);
  gst_object_unref (tee_pad);
  if (pad == NULL) {
    WARNING ("Couldn't create ghost pad for tee");
    return NULL;
  }

  gst_pad_set_active (pad, TRUE);

  gst_element_add_pad (element, pad);

  return pad;
}

static void
fsu_source_release_pad (GstElement * element, GstPad * pad)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;
  GList *item = NULL;

  DEBUG ("releasing pad");

  gst_pad_set_active (pad, FALSE);

  if (GST_IS_GHOST_PAD (pad)) {
    GstPad *tee_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (pad));
    /* converter before tee... :( */
    /*gst_element_release_request_pad (priv->tee, tee_pad);*/
  }

  gst_element_remove_pad (element, pad);

  for (item = GST_ELEMENT_PADS (self); item; item = g_list_next (item)) {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SRC (pad)) {
      return;
    }
  }

  DEBUG ("No more request source pads");
  if (priv->tee != NULL) {
    gst_object_ref (priv->tee);
    gst_bin_remove (GST_BIN (self), priv->tee);
    gst_element_set_state (priv->tee, GST_STATE_NULL);
    gst_object_unref (priv->tee);
  }
  priv->tee = NULL;
  if (priv->source != NULL) {
    gst_object_ref (priv->source);
    gst_bin_remove (GST_BIN (self), priv->source);
    gst_element_set_state (priv->source, GST_STATE_NULL);
    gst_object_unref (priv->source);
  }
  priv->source = NULL;
}

static gchar *
get_device_property_name (GstElement *element)
{
  if (g_object_has_property (G_OBJECT (element), "device") == TRUE &&
      g_object_has_property (G_OBJECT (element), "device-name") == FALSE)
    return "device";
  else if (g_object_has_property (G_OBJECT (element), "device") == FALSE &&
      g_object_has_property (G_OBJECT (element), "device-name") == TRUE)
    return "device-name";
  else if (g_object_has_property (G_OBJECT (element), "device") == TRUE &&
      g_object_has_property (G_OBJECT (element), "device-name") == TRUE) {
    if (strcmp (GST_ELEMENT_NAME (element), "dshowaudiosrc") == 0 ||
        strcmp (GST_ELEMENT_NAME (element), "dshowvideosrc") == 0)
      return "device-name";
    else
      return "device";
  } else {
    return NULL;
  }
}

static GstElement *
find_source (GstElement *src)
{
  GstElement *source = NULL;


  if (GST_IS_BIN (src)) {
    GstIterator *it = gst_bin_iterate_sources (GST_BIN(src));
    gboolean done = FALSE;
    gpointer item = NULL;

    while (!done) {
      switch (gst_iterator_next (it, &item)) {
        case GST_ITERATOR_OK:
          source = GST_ELEMENT (item);
          gst_object_unref (item);
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

    if (source == NULL)
      return src;

  } else {
    return src;
  }
  return find_source (source);
}


static GstElement *
test_source (FsuSource *self, const gchar *name)
{
  FsuSourcePrivate *priv = self->priv;
  GstPropertyProbe *probe = NULL;
  GstElement *element = NULL;
  GstState target_state = GST_STATE_READY;
  gchar * target_state_name = "READY";
  GstStateChangeReturn state_ret;
  GValueArray *arr;
  const gchar **blacklist = FSU_SOURCE_GET_CLASS (self)->blacklisted_sources;

  if (GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL) {
    target_state = GST_STATE (GST_ELEMENT (self));
    if (GST_STATE (GST_ELEMENT (self)) == GST_STATE_READY)
      target_state_name = "READY";
    else if (GST_STATE (GST_ELEMENT (self)) == GST_STATE_PAUSED)
      target_state_name = "PAUSED";
    else if (GST_STATE (GST_ELEMENT (self)) == GST_STATE_PLAYING)
      target_state_name = "PLAYING";
    DEBUG ("we are not NULL, target state set to %s", target_state_name);
  }

  DEBUG ("Testing source %s. probe idx = %d", name, priv->probe_idx);

  for (;blacklist && *blacklist; blacklist++) {
    if (strcmp (name, *blacklist) == 0) {
      priv->probe_idx = -1;
      return NULL;
    }
  }


  element = gst_element_factory_make (name, NULL);

  if (element == NULL) {
    priv->probe_idx = -1;
    return NULL;
  }

  /* Test the source */
  if (priv->probe_idx == -1) {
    priv->probe_idx = 0;

    state_ret = gst_element_set_state (element, target_state);
    if (state_ret == GST_STATE_CHANGE_ASYNC) {
      DEBUG ("Waiting for %s to go to state %s", name, target_state_name);
      state_ret = gst_element_get_state (element, NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }

    if (state_ret != GST_STATE_CHANGE_FAILURE) {
      return element;
    }
  }

  if (GST_IS_PROPERTY_PROBE (element)) {
    probe = GST_PROPERTY_PROBE (element);
    if (probe != NULL && get_device_property_name(element) != NULL) {
      arr = gst_property_probe_probe_and_get_values_name (probe,
          get_device_property_name(element));
      if (arr && arr->n_values > 0) {
        guint i;
        for (i = priv->probe_idx; i < arr->n_values; i++) {
          const gchar *device;
          GValue *val;

          val = g_value_array_get_nth (arr, i);
          if (val == NULL || !G_VALUE_HOLDS_STRING (val))
            continue;

          device = g_value_get_string (val);
          if (device == NULL)
            continue;

          DEBUG ("Testing device %s", device);
          g_object_set(element, get_device_property_name(element), device, NULL);

          state_ret = gst_element_set_state (element, target_state);
          if (state_ret == GST_STATE_CHANGE_ASYNC) {
            DEBUG ("Waiting for %s to go to state %s", name, target_state_name);
            state_ret = gst_element_get_state (element, NULL, NULL,
                GST_CLOCK_TIME_NONE);
          }

          if (state_ret != GST_STATE_CHANGE_FAILURE) {
            DEBUG ("Took device %s with idx %d", device, i);
            g_value_array_free (arr);
            priv->probe_idx = i + 1;
            return element;
          }
        }
        g_value_array_free (arr);
      }
    }
  }

  gst_element_set_state (element, GST_STATE_NULL);
  gst_object_unref (element);
  priv->probe_idx = -1;
  return NULL;
}


static GstElement *
create_source (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *src = NULL;
  GstElement *real_src = NULL;

  if (priv->disabled) {
    DEBUG ("Source is disabled");
    return NULL;
  }

  DEBUG ("Creating source : %p - %s -- %s (%s)",
      priv->source,
      priv->source_pipeline != NULL ? priv->source_pipeline : "(null)",
      priv->source_name != NULL ? priv->source_name : "(null)",
      priv->source_device != NULL ? priv->source_device : "(null)");

  if (priv->source != NULL) {
    return priv->source;
  } else if (priv->source_pipeline != NULL) {
    GstPad *pad = NULL;
    GstBin *bin = NULL;
    gchar *desc = NULL;
    GError *error  = NULL;
    GstStateChangeReturn state_ret;

    /* parse the pipeline to a bin */
    desc = g_strdup_printf ("bin.( %s ! identity )", priv->source_pipeline);
    bin = (GstBin *) gst_parse_launch (desc, &error);
    g_free (desc);

    if (bin != NULL) {
      /* find pads and ghost them if necessary */
      if ((pad = gst_bin_find_unlinked_pad (bin, GST_PAD_SRC))) {
        gst_element_add_pad (GST_ELEMENT (bin), gst_ghost_pad_new ("src", pad));
        gst_object_unref (pad);
      }
    }
    if (error) {
      WARNING ("Error while creating source-pipeline (%d): %s",
          error->code, (error->message != NULL? error->message : "(null)"));
      return NULL;
    }

    state_ret = gst_element_set_state (GST_ELEMENT (bin), GST_STATE_READY);
    if (state_ret == GST_STATE_CHANGE_ASYNC) {
      DEBUG ("Waiting for source-pipeline to go to state READY");
      state_ret = gst_element_get_state (GST_ELEMENT (bin), NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      WARNING ("Could not change state of source-pipeline to READY");
      gst_object_unref (bin);
      return NULL;
    }

    src = GST_ELEMENT (bin);
  } else if (priv->source_name != NULL) {
    GstStateChangeReturn state_ret;

    src = gst_element_factory_make (priv->source_name, NULL);

    if (src && priv->source_device && get_device_property_name(src) != NULL)
      g_object_set(src, get_device_property_name(src),
          priv->source_device, NULL);

    state_ret = gst_element_set_state (src, GST_STATE_READY);
    if (state_ret == GST_STATE_CHANGE_ASYNC) {
      DEBUG ("Waiting for %s to go to state READY", priv->source_name);
      state_ret = gst_element_get_state (src, NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      DEBUG ("Unable to set source to READY");
      gst_object_unref (src);
      return NULL;
    }

  } else {
    GList *sources, *walk;
    const gchar **priority_source = priv->priority_source_ptr;

    if (priority_source == NULL)
      priority_source = FSU_SOURCE_GET_CLASS (self)->priority_sources;
    for (;priority_source && *priority_source;
         priority_source++) {
      GstElement *element = test_source (self, *priority_source);

      if (element == NULL)
        continue;

      DEBUG ("Using source %s", *priority_source);
      src = element;
      break;
    }
    priv->priority_source_ptr = priority_source;

    if (src == NULL) {
      if (!priv->filtered_sources_done && priv->filtered_sources == NULL)
        priv->filtered_sources = get_plugins_filtered (
            FSU_SOURCE_GET_CLASS (self)->klass_check);
      sources = priv->filtered_sources;

      walk = priv->current_filtered_source;
      if (walk == NULL && !priv->filtered_sources_done)
        walk = sources;

      for (;walk; walk = g_list_next (walk)) {
        GstElement *element;
        GstElementFactory *factory = GST_ELEMENT_FACTORY(walk->data);

        element = test_source (self, GST_PLUGIN_FEATURE_NAME(factory));

        if (element == NULL)
          continue;

        DEBUG ("Using source %s", GST_PLUGIN_FEATURE_NAME(factory));
        src = element;
        break;
      }
      priv->current_filtered_source = walk;

      if (walk == NULL) {
        priv->filtered_sources_done = TRUE;
        for (walk = sources; walk; walk = g_list_next (walk)) {
          if (walk->data)
            gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
        }
        g_list_free (sources);
        priv->filtered_sources = NULL;
      }

    }
  }

  if (src != NULL) {
    GST_OBJECT_FLAG_UNSET (src, GST_ELEMENT_IS_SINK);
    real_src = find_source (src);
    if (g_object_has_property (G_OBJECT (src), "buffer-time"))
      g_object_set(src, "buffer-time", G_GINT64_CONSTANT(20000), NULL);

    if (g_object_has_property (G_OBJECT (src), "is-live"))
      g_object_set(src, "is-live", TRUE, NULL);
  }

  return src;
}

static GstStateChangeReturn
fsu_source_change_state (GstElement *element, GstStateChange transition)
{
  FsuSource *self = FSU_SOURCE (element);
  FsuSourcePrivate *priv = self->priv;
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (priv->source == NULL && priv->tee != NULL) {
        DEBUG ("Going to READY state. Creating source and linking to tee");
        create_source_and_link_tee (self);
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL:
      if (priv->source != NULL) {
        DEBUG ("Setting source to state NULL");
        ret = gst_element_set_state (priv->source, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_ASYNC) {
          DEBUG ("Waiting for source to go to state NULL");
          ret = gst_element_get_state (priv->source, NULL, NULL,
              GST_CLOCK_TIME_NONE);
        }
        gst_bin_remove (GST_BIN (self), priv->source);
        priv->source = NULL;
      }
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

  if (source != NULL) {
    state_ret = gst_element_set_state (source, GST_STATE_NULL);
    if (state_ret == GST_STATE_CHANGE_ASYNC) {
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
    if (source != NULL &&
        GST_STATE (GST_ELEMENT (self)) > GST_STATE_NULL)
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
  if (priv->source != NULL && priv->thread == NULL) {
    GstElement *source = priv->source;
    GST_OBJECT_UNLOCK (GST_OBJECT (self));
    gst_object_ref (source);
    gst_bin_remove (GST_BIN (self), source);

    gst_object_ref (self);
    GST_OBJECT_LOCK (GST_OBJECT (self));
    priv->thread = g_thread_create (replace_source_thread, self, FALSE, &error);

    if (!priv->thread)
    {
      WARNING ("Could not start stopping thread for FsRtpSpecialSource:"
          " %s", error->message);
    }
    g_clear_error (&error);
  }
  GST_OBJECT_UNLOCK (GST_OBJECT (self));
}

static void
fsu_source_handle_message (GstBin *bin, GstMessage *message)
{
  FsuSource *self = FSU_SOURCE (bin);
  FsuSourcePrivate *priv = self->priv;

  DEBUG ("Got message of type %s from %s", GST_MESSAGE_TYPE_NAME(message),
      GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)));

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    if (GST_MESSAGE_SRC (message) != GST_OBJECT (find_source (priv->source))) {
      WARNING ("source different : %p - %p - %p", GST_MESSAGE_SRC (message),
          priv->source, find_source (priv->source));
    }
    DEBUG ("Source got an error. setting source to state NULL");
    destroy_source (self);
  } else {
    GST_BIN_CLASS (parent_class)->handle_message (bin, message);
  }
}
