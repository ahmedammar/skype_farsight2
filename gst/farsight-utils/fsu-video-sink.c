/*
 * fsu-video-sink.c - Sink for FsuVideoSink
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
  #include "config.h"
#endif

#include "fsu-video-sink.h"
#include "fsu.h"
#include <gst/farsight/fsu-videoconverter-filter.h>
#include <gst/interfaces/xoverlay.h>


GST_DEBUG_CATEGORY_STATIC (fsu_video_sink_debug);
#define GST_CAT_DEFAULT fsu_video_sink_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)


static const GstElementDetails fsu_video_sink_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Video Sink",
  "Video Sink",
  "FsuVideoSink class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_video_sink_debug, "fsuvideosink", 0, "fsuvideosink element");
}

GST_BOILERPLATE_FULL (FsuVideoSink, fsu_video_sink,
    FsuSink, FSU_TYPE_SINK, _do_init)

/* properties */
enum
{
  PROP_ID = 1,
};

struct _FsuVideoSinkPrivate
{
  /* Properties */
  gpointer id;
};


static void fsu_video_sink_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_video_sink_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void fsu_video_sink_handle_message (GstBin *bin,
    GstMessage *message);

static void
sink_element_added (GstBin *bin,
    GstElement *sink,
    gpointer user_data)
{
  FsuSink *self = user_data;

  fsu_sink_element_added (self, bin, sink);
}

static GstElement *
create_auto_sink (FsuSink *self)
{
#ifdef __APPLE__
  return gst_element_factory_make ("osxvideosink", NULL);
#else
  GstElement *sink = gst_element_factory_make ("autovideosink", NULL);
  g_signal_connect (sink, "element-added",
      G_CALLBACK (sink_element_added), self);
  return sink;
#endif
}

static gchar *
need_mixer (FsuSink *self,
    GstElement *sink)
{
  return "fsfunnel";
}

static void
add_filters (FsuSink *self,
    FsuFilterManager *manager)
{
  FsuVideoconverterFilter *filter = fsu_videoconverter_filter_get_singleton ();

  fsu_filter_manager_append_filter (manager, FSU_FILTER (filter));
  g_object_unref (filter);
}

static void
fsu_video_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_video_sink_details);
}

static void
fsu_video_sink_class_init (FsuVideoSinkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuSinkClass *fsu_sink_class = FSU_SINK_CLASS (klass);
  GstBinClass *gstbin_class = GST_BIN_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuVideoSinkPrivate));

  gobject_class->get_property = fsu_video_sink_get_property;
  gobject_class->set_property = fsu_video_sink_set_property;

  fsu_sink_class->auto_sink_name = "autovideosink";
  fsu_sink_class->create_auto_sink = create_auto_sink;
  fsu_sink_class->need_mixer = need_mixer;
  fsu_sink_class->add_filters = add_filters;

  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR (fsu_video_sink_handle_message);

  g_object_class_install_property (gobject_class, PROP_ID,
      g_param_spec_pointer ("id", "Sink id",
          "The xid of the window in which to embed the video sink.",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_video_sink_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuVideoSink *self = FSU_VIDEO_SINK (object);
  FsuVideoSinkPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_ID:
      g_value_set_pointer (value, priv->id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_video_sink_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuVideoSink *self = FSU_VIDEO_SINK (object);
  FsuVideoSinkPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_ID:
      priv->id = g_value_get_pointer (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}


static void
fsu_video_sink_init (FsuVideoSink *self,
    FsuVideoSinkClass *klass)
{
  FsuVideoSinkPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SINK,
          FsuVideoSinkPrivate);

  self->priv = priv;
  priv->id = GINT_TO_POINTER (0);
}

static void
fsu_video_sink_handle_message (GstBin *bin,
    GstMessage *message)
{
  FsuVideoSink *self = FSU_VIDEO_SINK (bin);
  const GstStructure *s = gst_message_get_structure (message);

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT &&
      gst_structure_has_name (s, "prepare-xwindow-id"))
    gst_x_overlay_set_xwindow_id (GST_X_OVERLAY (GST_MESSAGE_SRC (message)),
        GPOINTER_TO_INT (self->priv->id));
#ifdef __APPLE__
  else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT &&
      gst_structure_has_name (s, "have-ns-view")) {
    (void);
  }
#endif
}
