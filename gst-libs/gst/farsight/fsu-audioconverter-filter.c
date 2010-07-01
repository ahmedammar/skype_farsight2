/*
 * fsu-audioconverter-filter.c - Source for FsuAudioconverterFilter
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
#  include "config.h"
#endif

#include <gst/farsight/fsu-audioconverter-filter.h>
#include <gst/farsight/fsu-filter-helper.h>

FSU_DEFINE_FILTER (FsuAudioconverterFilter, audioconverter);

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

  if (!singleton)
    singleton = fsu_audioconverter_filter_new ();

  g_debug ("Getting audioconverter singleton %p", singleton);
  return singleton;
}


static GstPad *
fsu_audioconverter_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  GstElement *converters = NULL;
  GstPad *out_pad = NULL;


  converters = fsu_filter_add_element_by_description (bin, pad,
      "audioconvert ! audioresample ! audioconvert", &out_pad);

  g_debug ("Applying audioconverter filter : %p", converters);

  return out_pad;
}

static GstPad *
fsu_audioconverter_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  return fsu_filter_revert_bin (bin, pad);
}

