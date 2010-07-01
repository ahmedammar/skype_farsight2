/*
 * fsu-videoconverter-filter.c - Source for FsuVideoconverterFilter
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

#include <gst/farsight/fsu-videoconverter-filter.h>
#include <gst/farsight/fsu-filter-helper.h>

FSU_DEFINE_FILTER (FsuVideoconverterFilter, videoconverter);

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

  if (!singleton)
    singleton = fsu_videoconverter_filter_new ();
  return singleton;
}


static GstPad *
fsu_videoconverter_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  GstPad *out = fsu_filter_add_standard_element (bin, pad, "ffmpegcolorspace",
      NULL, NULL);
  g_debug ("output video converter filter : %p", out);
  return out;
}

static GstPad *
fsu_videoconverter_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  return fsu_filter_revert_standard_element (bin, pad, NULL);
}

