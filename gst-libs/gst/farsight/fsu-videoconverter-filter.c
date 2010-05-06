/*
 * fsu-videoconverter-filter.c - Source for FsuVideoconverterFilter
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

#include "fsu-videoconverter-filter.h"
#include <gst/farsight/fsu-filter-helper.h>

G_DEFINE_TYPE (FsuVideoconverterFilter, fsu_videoconverter_filter,
    FSU_TYPE_FILTER);

static GstPad *fsu_videoconverter_filter_apply (FsuFilter *filter,
    GstBin *bin, GstPad *pad);
static GstPad *fsu_videoconverter_filter_revert (FsuFilter *filter,
    GstBin *bin, GstPad *pad);


static void
fsu_videoconverter_filter_class_init (FsuVideoconverterFilterClass *klass)
{
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  fsufilter_class->apply = fsu_videoconverter_filter_apply;
  fsufilter_class->revert = fsu_videoconverter_filter_revert;

}

static void
fsu_videoconverter_filter_init (FsuVideoconverterFilter *self)
{
}

FsuVideoconverterFilter *
fsu_videoconverter_filter_new (void)
{
  return g_object_new (FSU_TYPE_VIDEOCONVERTER_FILTER, NULL);
}

FsuVideoconverterFilter *
fsu_videoconverter_filter_get_singleton (void)
{
  static FsuVideoconverterFilter *singleton = NULL;

  if (singleton == NULL)
    singleton = fsu_videoconverter_filter_new ();
  return singleton;
}


static GstPad *
fsu_videoconverter_filter_apply (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  GstElement *colorspace = NULL;
  GstPad *out_pad = NULL;


  colorspace = fsu_filter_add_element_by_name (bin, pad,
      "ffmpegcolorspace", GST_PAD_IS_SRC (pad) ? "sink" : "src",
      &out_pad, GST_PAD_IS_SRC (pad) ? "src" : "sink");

  if (colorspace == NULL) {
    return NULL;
  } else {
    gst_object_unref (pad);
    pad = out_pad;
  }

  return pad;
}

static GstPad *
fsu_videoconverter_filter_revert (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  GstElement *converters = GST_ELEMENT (gst_pad_get_parent (pad));
  GstPad *other_pad = NULL;
  GstPad *out_pad = NULL;


  other_pad = gst_element_get_static_pad (converters,
      GST_PAD_IS_SRC (pad) ? "sink" : "src");
  out_pad = gst_pad_get_peer (other_pad);
  gst_object_unref (other_pad);

  gst_bin_remove (bin, converters);
  gst_object_unref (converters);

  return out_pad;
}

