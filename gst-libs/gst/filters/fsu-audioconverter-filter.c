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

/**
 * SECTION:fsu-filters
 * @short_description: A set of filters
 *
 * Many filters come with Farsigh-utils, and you can use them directly or with
 * the #FsuFilterManager in order to easily create your pipelines.
 * These are the filters currently available and shipped with Farsight.
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/filters/fsu-audioconverter-filter.h>
#include <gst/filters/fsu-filter-helper.h>


FSU_DEFINE_FILTER (FsuAudioconverterFilter, audioconverter);

static void
fsu_audioconverter_filter_init (FsuAudioconverterFilter *self)
{
}

/**
 * fsu_audioconverter_filter_new:
 *
 * Creates a new audioconverter filter or reuse an existing one.
 * This filter will take care of converting audio from one format to another.
 * It will basically add 'audioconvert ! audioresample ! audioconvert'
 * to the pipeline.
 *
 * Returns: A new #FsuAudioconverterFilter
 */
FsuAudioconverterFilter *
fsu_audioconverter_filter_new (void)
{
  static FsuAudioconverterFilter *singleton = NULL;
  GStaticMutex mutex = G_STATIC_MUTEX_INIT;

  g_static_mutex_lock (&mutex);
  if (!singleton)
  {
    singleton = g_object_new (FSU_TYPE_AUDIOCONVERTER_FILTER, NULL);
    g_object_add_weak_pointer (G_OBJECT (singleton), (gpointer *)&singleton);
  }
  else
  {
    g_object_ref (singleton);
  }
  g_static_mutex_unlock (&mutex);

  return singleton;
}


static GstPad *
fsu_audioconverter_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  //GstElement *converters = NULL;
  //GstPad *out_pad = NULL;


  //converters = fsu_filter_add_element_by_description (bin, pad,
  //    "audioconvert", &out_pad);

  //gst_object_unref (converters);

  return pad;//out_pad;
}

static GstPad *
fsu_audioconverter_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  return fsu_filter_revert_bin (bin, pad);
}

