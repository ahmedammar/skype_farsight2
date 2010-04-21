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

static const GstElementDetails fsu_video_source_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Video Source",
  "Video Source",
  "FsuVideoSource class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

GST_BOILERPLATE (FsuVideoSource, fsu_video_source,
    FsuSource, FSU_TYPE_SOURCE)

static const gchar *video_priority_sources[] = {"dshowvideosrc",
                                                "ksvideosrc",
                                                "gconfvideosrc",
                                                "gconfv4l2src",
                                                "v4l2src",
                                                "v4lsrc",
                                                NULL};

static const gchar *video_blacklisted_sources[] = {"videotestsrc",
                                                   "autovideosrc",
                                                   "ximagesrc",
                                                   "dx9screencapsrc",
                                                   "gdiscreencapsrc",
                                                   NULL};

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
}

static void
fsu_video_source_init (FsuVideoSource *self, FsuVideoSourceClass *klass)
{
}
