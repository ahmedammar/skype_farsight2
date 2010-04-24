/*
 * fsu-video-sink.c - Sink for FsuVideoSink
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

#include "fsu-video-sink.h"


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

static GstElement *
create_auto_sink (FsuSink *self)
{
#ifdef __APPLE__
  return gst_element_factory_make ("osxvideosink", NULL);
#else
  return gst_element_factory_make ("autovideosink", NULL);
#endif
}

static gchar *
need_mixer (FsuSink *self, GstElement *sink)
{
  return "fsfunnel";
}

static GstPad *
add_converters (FsuSink *self, GstPad *pad)
{
  GstElement *colorspace = gst_element_factory_make ("ffmpegcolorspace", NULL);
  GstPad *sink_pad = NULL;
  GstPad *src_pad = NULL;

  DEBUG ("Adding converters");

  if (colorspace == NULL ||
      gst_bin_add (GST_BIN (self), colorspace) == FALSE) {
    WARNING ("Could not create colorspace (%p) or add it to bin", colorspace);
    if (colorspace != NULL)
      gst_object_unref (colorspace);
    return pad;
  }
  sink_pad = gst_element_get_static_pad (colorspace, "sink");
  src_pad = gst_element_get_static_pad (colorspace, "src");

  if (src_pad == NULL || sink_pad == NULL ||
      gst_pad_link(src_pad, pad) != GST_PAD_LINK_OK)  {
    WARNING ("Could not get colorspace pads (%p - %p) or link the source to it",
        src_pad, sink_pad);
    gst_bin_remove (GST_BIN (self), colorspace);
    if (src_pad != NULL)
      gst_object_unref (src_pad);
    if (sink_pad != NULL)
      gst_object_unref (sink_pad);
    return pad;
  }

  DEBUG ("Converter ffmpegcolorspace added");
  gst_object_unref (src_pad);
  gst_object_unref (pad);
  return sink_pad;
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
  FsuSinkClass *fsu_sink_class = FSU_SINK_CLASS (klass);

  fsu_sink_class->auto_sink_name = "autovideosink";
  fsu_sink_class->create_auto_sink = create_auto_sink;
  fsu_sink_class->need_mixer = need_mixer;
  fsu_sink_class->add_converters = add_converters;
}

static void
fsu_video_sink_init (FsuVideoSink *self, FsuVideoSinkClass *klass)
{
}
