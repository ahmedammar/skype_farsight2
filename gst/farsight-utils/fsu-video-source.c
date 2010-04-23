/*
 * fsu-video-source.c - Source for FsuVideoSource
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

#include "fsu-video-source.h"

GST_DEBUG_CATEGORY_STATIC (fsu_video_source_debug);
#define GST_CAT_DEFAULT fsu_video_source_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)


static const GstElementDetails fsu_video_source_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Video Source",
  "Video Source",
  "FsuVideoSource class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_video_source_debug, "fsuvideosource", 0, "fsuvideosource element");
}

GST_BOILERPLATE_FULL (FsuVideoSource, fsu_video_source,
    FsuSource, FSU_TYPE_SOURCE, _do_init)

static const gchar *video_priority_sources[] = {"dshowvideosrc",
                                                "ksvideosrc",
                                                "gconfvideosrc",
                                                "v4l2src",
                                                "v4lsrc",
                                                NULL};

static const gchar *video_blacklisted_sources[] = {"videotestsrc",
                                                   "autovideosrc",
                                                   "ximagesrc",
                                                   "dx9screencapsrc",
                                                   "gdiscreencapsrc",
                                                   NULL};

static GstPad *
add_converters (FsuSource *self, GstPad *pad)
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
      gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK)  {
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
  gst_object_unref (sink_pad);
  gst_object_unref (pad);
  return src_pad;
}

static void
fsu_video_source_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_video_source_details);
}

static void
fsu_video_source_class_init (FsuVideoSourceClass *klass)
{
  FsuSourceClass *fsu_source_class = FSU_SOURCE_CLASS (klass);

  fsu_source_class->priority_sources = video_priority_sources;
  fsu_source_class->blacklisted_sources = video_blacklisted_sources;
  fsu_source_class->klass_check = is_video_source;
  fsu_source_class->add_converters = add_converters;
}

static void
fsu_video_source_init (FsuVideoSource *self, FsuVideoSourceClass *klass)
{
}
