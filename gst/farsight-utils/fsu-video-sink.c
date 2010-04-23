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

static const GstElementDetails fsu_video_sink_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Video Sink",
  "Video Sink",
  "FsuVideoSink class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

GST_BOILERPLATE (FsuVideoSink, fsu_video_sink,
    FsuSink, FSU_TYPE_SINK)

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
}

static void
fsu_video_sink_init (FsuVideoSink *self, FsuVideoSinkClass *klass)
{
}
