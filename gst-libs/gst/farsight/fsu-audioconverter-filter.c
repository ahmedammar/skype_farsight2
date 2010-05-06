/*
 * fsu-audioconverter-filter.c - Source for FsuAudioconverterFilter
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

#include "fsu-audioconverter-filter.h"
#include <gst/farsight/fsu-filter-helper.h>

G_DEFINE_TYPE (FsuAudioconverterFilter, fsu_audioconverter_filter,
    FSU_TYPE_FILTER);

static GstPad *fsu_audioconverter_filter_apply (FsuFilter *filter,
    GstBin *bin, GstPad *pad);
static GstPad *fsu_audioconverter_filter_revert (FsuFilter *filter,
    GstBin *bin, GstPad *pad);


static void
fsu_audioconverter_filter_class_init (FsuAudioconverterFilterClass *klass)
{
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  fsufilter_class->apply = fsu_audioconverter_filter_apply;
  fsufilter_class->revert = fsu_audioconverter_filter_revert;

}

static void
fsu_audioconverter_filter_init (FsuAudioconverterFilter *self)
{
}

FsuAudioconverterFilter *
fsu_audioconverter_filter_new (void)
{
  return g_object_new (FSU_TYPE_AUDIOCONVERTER_FILTER, NULL);
}

FsuAudioconverterFilter *
fsu_audioconverter_filter_get_singleton (void)
{
  static FsuAudioconverterFilter *singleton = NULL;

  if (singleton == NULL)
    singleton = fsu_audioconverter_filter_new ();

  g_debug ("Getting audioconverter singleton %p", singleton);
  return singleton;
}


static GstPad *
fsu_audioconverter_filter_apply (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  GstElement *converters = NULL;
  GstPad *out_pad = NULL;


  converters = fsu_filter_add_element_by_description (bin, pad,
      "audioconvert ! audioresample ! audioconvert", &out_pad);

  g_debug ("Applying audioconverter filter : %p", converters);

  if (converters != NULL)
    gst_object_unref (pad);

  return out_pad;
}

static GstPad *
fsu_audioconverter_filter_revert (FsuFilter *filter, GstBin *bin, GstPad *pad)
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

