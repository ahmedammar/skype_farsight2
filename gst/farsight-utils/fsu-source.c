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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <string.h>
#include <gst/interfaces/propertyprobe.h>

#include "fsu-source.h"
#include "fsu-common.h"
#include "fsu-audio-source.h"


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


static void fsu_source_constructed (GObject *object);
static void fsu_source_dispose (GObject *object);
static void fsu_source_finalize (GObject *object);
static void fsu_source_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_source_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);

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

  /* The src tee coming out of the source element */
  GstElement *source;
  GstElement *tee;
};

/*
static const gchar *video_priority_sources[] = {"dshowvideosrc",
                                                "ksvideosrc",
                                                "gconfvideosrc",
                                                "gconfv4l2src",
                                                "v4l2src",
                                                "v4lsrc",
                                                NULL};
*/
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

  g_type_class_add_private (klass, sizeof (FsuSourcePrivate));

  gobject_class->get_property = fsu_source_get_property;
  gobject_class->set_property = fsu_source_set_property;
  gobject_class->constructed = fsu_source_constructed;
  gobject_class->dispose = fsu_source_dispose;
  gobject_class->finalize = fsu_source_finalize;

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (fsu_source_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (fsu_source_release_pad);

  g_object_class_install_property (gobject_class, PROP_DISABLED,
      g_param_spec_boolean ("disabled", "Disable the source",
          "Set to TRUE to disable the source",
          FALSE,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_NAME,
      g_param_spec_string ("source-name", "Source element name",
          "The name of the source element to create.",
          NULL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_DEVICE,
      g_param_spec_string ("source-device", "Source device",
          "The device to use with the source",
          NULL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SOURCE_PIPELINE,
      g_param_spec_string ("source-pipeline", "Source pipeline",
          "The pipeline to use for creating the source",
          NULL,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_source_init (FsuSource *self, FsuSourceClass *klass)
{
  FsuSourcePrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SOURCE,
          FsuSourcePrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
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
fsu_source_constructed (GObject *object)
{
  void (*chain_up) (GObject *) = G_OBJECT_CLASS (parent_class)->constructed;
  FsuSource *self = FSU_SOURCE (object);

  (void)self;

  if (chain_up != NULL)
    chain_up (object);

}

static void
fsu_source_dispose (GObject *object)
{
  FsuSource *self = (FsuSource *)object;
  FsuSourcePrivate *priv = self->priv;
  GList *item;

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

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
fsu_source_finalize (GObject *object)
{
  FsuSource *self = (FsuSource *)object;

  (void)self;
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstElement *
create_tee (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;
  GstElement *src = NULL;
  GstElement *tee = NULL;
  GstPad *src_pad = NULL;
  GstPad *tee_pad = NULL;

  tee = gst_element_factory_make ("tee", "srctee");

  if (tee == NULL) {
    WARNING ("Could not create src tee");
    return NULL;
  }

  if (gst_bin_add (GST_BIN (self), tee) == FALSE) {
    WARNING ("Could not add src tee to Source");
    gst_object_unref (tee);
    return NULL;
  }

  src = create_source (self);
  if (src != NULL) {
    if (gst_bin_add (GST_BIN (self), src) == FALSE) {
      WARNING ("Could not add source element to Source");
      return NULL;
    }

    src_pad = gst_element_get_static_pad (src, "src");
  }

  if (src_pad != NULL) {
    tee_pad = gst_element_get_static_pad (tee, "sink");

    if (tee_pad != NULL) {
      if (gst_pad_link (src_pad, tee_pad) != GST_PAD_LINK_OK) {
        WARNING ("Couldn't link source pad with src tee");
        gst_bin_remove (GST_BIN (self), tee);
        gst_bin_remove (GST_BIN (self), src);
        return NULL;
      }
      gst_object_unref (tee_pad);
    }
  }

  if (src_pad != NULL) {
    gst_object_unref (src_pad);
    priv->source = src;
  }

  return tee;
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

  priv->tee = create_tee (self);

  if (priv->tee == NULL) {
    WARNING ("Couldn't create a tee to request pad from");
    return NULL;
  }
  tee_pad = gst_element_get_request_pad (priv->tee, "src%d");
  if (tee_pad == NULL) {
    WARNING ("Couldn't get new request pad from src tee");
    gst_bin_remove (GST_BIN (self), priv->tee);
    priv->tee = NULL;
    return NULL;
  }
  pad = gst_ghost_pad_new (name, tee_pad);
  gst_object_unref (tee_pad);
  if (pad == NULL) {
    WARNING ("Couldn't create ghost pad for tee");
    gst_bin_remove (GST_BIN (self), priv->tee);
    priv->tee = NULL;
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
    gst_element_release_request_pad (priv->tee, tee_pad);
  }

  gst_element_remove_pad (element, pad);

  for (item = GST_ELEMENT_PADS (self); item; item = g_list_next (item)) {
    GstPad *pad = GST_PAD (item->data);

    if (GST_PAD_IS_SRC (pad)) {
      return;
    }
  }

  /*TODO : state to NULL */
  DEBUG ("No more request source pads");
  gst_bin_remove (GST_BIN (self), priv->tee);
  priv->tee = NULL;
  gst_bin_remove (GST_BIN (self), priv->source);
  priv->source = NULL;
}

static gchar *
get_device_property_name (const gchar *plugin_name)
{
  if (!strcmp (plugin_name, "dshowaudiosrc") ||
	  !strcmp (plugin_name, "dshowvideosrc"))
    return "device-name";
  else
    return "device";
}


/*
static GstElement *find_source (GstElement *src)
{
  GstElement *source = NULL;


  if (GST_IS_BIN (src)) {
    GstIterator *it = gst_bin_iterate_sources (GST_BIN(src));
    gboolean done = FALSE;
    GstElement *item = NULL;

    while (!done) {
      switch (gst_iterator_next (it, &item)) {
        case GST_ITERATOR_OK:
          source = item;
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
*/
static GstElement *
test_source (FsuSource *self, const gchar *name)
{
  GstPropertyProbe *probe = NULL;
  GstElement *element = NULL;
  GstElement *tee = NULL;
  GstElement *queue = NULL;
  GstElement *valve = NULL;
  GstElement *fakesink = NULL;
  GstElement *bin = NULL;
  GstPad *bin_pad = NULL;
  GstStateChangeReturn state_ret;
  GValueArray *arr;
  const gchar **blacklist = FSU_SOURCE_GET_CLASS (self)->blacklisted_sources;

  DEBUG ("Testing source %s", name);


  for (;blacklist && *blacklist; blacklist++) {
    if (strcmp (name, *blacklist) == 0)
      return NULL;
  }


  element = gst_element_factory_make (name, NULL);

  if (element == NULL)
    return NULL;

  if (g_object_has_property (G_OBJECT (element), "buffer-time"))
    g_object_set(element, "buffer-time", G_GINT64_CONSTANT(20000), NULL);

  if (g_object_has_property (G_OBJECT (element), "is-live"))
    g_object_set(element, "is-live", TRUE, NULL);

  /* Build a bin and put the element in it with a tee ! fakesink */
  bin = gst_bin_new ("source_bin");
  if (bin == NULL) {
    DEBUG ("Could not create source bin");
    gst_object_unref (element);
    return NULL;
  }

  if (gst_bin_add (GST_BIN (bin), element) == FALSE) {
    DEBUG ("Could not add source to source bin");
    gst_object_unref (element);
    gst_object_unref (bin);
    return NULL;
  }
  tee = gst_element_factory_make ("tee", NULL);
  if (tee == NULL || gst_bin_add (GST_BIN (bin), tee) == FALSE) {
    DEBUG ("Could not add tee to source bin");
    if (tee) gst_object_unref (tee);
    gst_object_unref (bin);
    return NULL;
  }

  if (gst_element_link(element, tee) == FALSE)  {
    DEBUG ("Could not link source to tee");
    gst_object_unref (bin);
    return NULL;
  }

  queue = gst_element_factory_make ("queue", NULL);
  if (queue == NULL ||
      gst_bin_add (GST_BIN (bin), queue) == FALSE) {
    DEBUG ("Could not add queue to source bin");
    gst_object_unref (queue);
    gst_object_unref (bin);
    return NULL;
  }

  if (gst_element_link(tee, queue) == FALSE)  {
    DEBUG ("Could not link tee to queue");
    gst_object_unref (bin);
    return NULL;
  }

  fakesink = gst_element_factory_make ("fakesink", NULL);
  if (fakesink == NULL) {
    DEBUG ("Could not create fakesink in source bin");
    gst_object_unref (bin);
    return NULL;
  }
  g_object_set (fakesink, "async", FALSE, NULL);
  g_object_set (fakesink, "sync", FALSE, NULL);

  if (gst_bin_add (GST_BIN (bin), fakesink) == FALSE) {
    DEBUG ("Could not add fakesink to source bin");
    gst_object_unref (fakesink);
    gst_object_unref (bin);
    return NULL;
  }

  if (gst_element_link(queue, fakesink) == FALSE)  {
    DEBUG ("Could not link fakesink to source");
    gst_object_unref (bin);
    return NULL;
  }

  queue = gst_element_factory_make ("queue", NULL);
  if (queue == NULL ||
      gst_bin_add (GST_BIN (bin), queue) == FALSE) {
    DEBUG ("Could not add second queue to source bin");
    gst_object_unref (queue);
    gst_object_unref (bin);
    return NULL;
  }

  if (gst_element_link(tee, queue) == FALSE)  {
    DEBUG ("Could not link tee to second queue");
    gst_object_unref (bin);
    return NULL;
  }

  valve = gst_element_factory_make ("valve", "hack_valve");
  if (valve == NULL ||
      gst_bin_add (GST_BIN (bin), valve) == FALSE) {
    DEBUG ("Could not add valve to source bin");
    gst_object_unref (valve);
    gst_object_unref (bin);
    return NULL;
  }

  if (gst_element_link(queue, valve) == FALSE)  {
    DEBUG ("Could not link queue to valve");
    gst_object_unref (bin);
    return NULL;
  }

  /* TODO: set to FALSE before returning ? */
  g_object_set (valve, "drop", TRUE, NULL);

  bin_pad = gst_element_get_static_pad (valve, "src");
  gst_element_add_pad (GST_ELEMENT (bin),  gst_ghost_pad_new ("src", bin_pad));
  gst_object_unref (bin_pad);
  GST_OBJECT_FLAG_UNSET (bin, GST_ELEMENT_IS_SINK);

  /* Test the source */
  state_ret = gst_element_set_state (bin, GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_ASYNC) {
    DEBUG ("Waiting for %s to go to state PLAYING", name);
    state_ret = gst_element_get_state (bin, NULL, NULL,
        GST_CLOCK_TIME_NONE);
  }

  if (state_ret != GST_STATE_CHANGE_FAILURE) {
    state_ret = gst_element_set_state (bin, GST_STATE_READY);
    if (state_ret == GST_STATE_CHANGE_ASYNC) {
      DEBUG ("Waiting for %s to go to state READY", name);
      state_ret = gst_element_get_state (bin, NULL, NULL,
          GST_CLOCK_TIME_NONE);
    }
    g_object_set (valve, "drop", FALSE, NULL);
    //gst_element_set_locked_state (bin, TRUE);
    gst_object_ref (bin);
    return bin;
  }

  if (GST_IS_PROPERTY_PROBE (element)) {
    probe = GST_PROPERTY_PROBE (element);
    if (probe) {
      arr = gst_property_probe_probe_and_get_values_name (probe,
          get_device_property_name(name));
      if (arr && arr->n_values > 0) {
        guint i;
        for (i = 0; i < arr->n_values; ++i) {
          const gchar *device;
          GValue *val;

          val = g_value_array_get_nth (arr, i);
          if (val == NULL || !G_VALUE_HOLDS_STRING (val))
            continue;

          device = g_value_get_string (val);
          if (device == NULL)
            continue;

          g_object_set(element, get_device_property_name(name), device, NULL);

          state_ret = gst_element_set_state (bin, GST_STATE_PLAYING);
          if (state_ret == GST_STATE_CHANGE_ASYNC) {
            DEBUG ("Waiting for %s to go to state PLAYING", name);
            state_ret = gst_element_get_state (bin, NULL, NULL,
                GST_CLOCK_TIME_NONE);
          }

          if (state_ret != GST_STATE_CHANGE_FAILURE) {
            g_value_array_free (arr);
            state_ret = gst_element_set_state (bin, GST_STATE_READY);
            if (state_ret == GST_STATE_CHANGE_ASYNC) {
              DEBUG ("Waiting for %s to go to state READY", name);
              state_ret = gst_element_get_state (bin, NULL, NULL,
                  GST_CLOCK_TIME_NONE);
            }
            g_object_set (valve, "drop", FALSE, NULL);
            //gst_element_set_locked_state (bin, TRUE);
            gst_object_ref (bin);
            return bin;
          }
        }
        g_value_array_free (arr);
      }
    }
  }

  gst_element_set_state (bin, GST_STATE_NULL);
  gst_object_unref (bin);
  return NULL;
}


static GstElement *
create_source (FsuSource *self)
{
  FsuSourcePrivate *priv = self->priv;

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
    desc = g_strdup_printf ("bin.( %s ! queue )", priv->source_pipeline);
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
    GST_OBJECT_FLAG_UNSET (bin, GST_ELEMENT_IS_SINK);

    return GST_ELEMENT (bin);

  } else if (priv->source_name != NULL) {
    GstStateChangeReturn state_ret;
    GstElement *src = NULL;

    src = gst_element_factory_make (priv->source_name, NULL);

    if (src && priv->source_device)
        g_object_set(src, get_device_property_name(priv->source_name),
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
      GST_OBJECT_FLAG_UNSET (src, GST_ELEMENT_IS_SINK);

      return src;

  } else {
    GstElement *src = NULL;
    GList *sources, *walk;
    const gchar **priority_source = FSU_SOURCE_GET_CLASS (self)->priority_sources;

    for (;priority_source && *priority_source;
         priority_source++) {
      GstElement *element = test_source (self, *priority_source);

      if (element == NULL)
        continue;

      DEBUG ("Using source %s", *priority_source);
      src = element;
      break;
    }

    if (src != NULL) {
      GST_OBJECT_FLAG_UNSET (src, GST_ELEMENT_IS_SINK);
      return src;
    }

    sources = get_plugins_filtered (TRUE, FSU_SOURCE_GET_CLASS (self)->audio);

    for (walk = sources; walk; walk = g_list_next (walk)) {
      GstElement *element;
      GstElementFactory *factory = GST_ELEMENT_FACTORY(walk->data);

      element = test_source (self, GST_PLUGIN_FEATURE_NAME(factory));

      if (element == NULL)
        continue;

      DEBUG ("Using source %s", GST_PLUGIN_FEATURE_NAME(factory));
      src = element;
      break;
    }
    for (walk = sources; walk; walk = g_list_next (walk)) {
      if (walk->data)
        gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
    }
    g_list_free (sources);

    if (src) {
      GST_OBJECT_FLAG_UNSET (src, GST_ELEMENT_IS_SINK);
      return src;
    } else {
      return NULL;
    }
  }

  return NULL;
}


static gboolean plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "fsuaudiosrc",
                               GST_RANK_NONE, FSU_TYPE_AUDIO_SOURCE);
}

GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "fsutils",
  "Farsight utils plugin",
  plugin_init,
  VERSION,
  "LGPL",
  "Farsight",
  "http://farsight.freedesktop.org/"
)
