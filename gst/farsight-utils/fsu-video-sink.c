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

  fsu_filter_manager_insert_filter (manager, FSU_FILTER (filter), 0);
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
  fsu_sink_class->add_filters = add_filters;
}

static void
fsu_video_sink_init (FsuVideoSink *self,
    FsuVideoSinkClass *klass)
{
}
