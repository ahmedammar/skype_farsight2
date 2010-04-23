/*
 * fsu-audio-source.c - Source for FsuAudioSource
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

#include "fsu-audio-source.h"


GST_DEBUG_CATEGORY_STATIC (fsu_audio_source_debug);
#define GST_CAT_DEFAULT fsu_audio_source_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)


static const GstElementDetails fsu_audio_source_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Audio Source",
  "Audio Source",
  "FsuAudioSource class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_audio_source_debug, "fsuaudiosource", 0, "fsuaudiosource element");
}

GST_BOILERPLATE_FULL (FsuAudioSource, fsu_audio_source,
    FsuSource, FSU_TYPE_SOURCE, _do_init)

static const gchar *audio_priority_sources[] = {"dshowaudiosrc",
                                                "directsoundsrc",
                                                "osxaudiosrc",
                                                "gconfaudiosrc",
                                                "pulsesrc",
                                                "alsasrc",
                                                "oss4src",
                                                "osssrc",
                                                NULL};

static const gchar *audio_blacklisted_sources[] = {"audiotestsrc",
                                                   "autoaudiosrc",
                                                   "dtmfsrc",
                                                   NULL};

static GstPad *
add_converters (FsuSource *self, GstPad *pad)
{
  GstElement *convert = gst_element_factory_make ("audioconvert", NULL);
  GstElement *resample = gst_element_factory_make ("audioresample", NULL);
  GstElement *convert2 = gst_element_factory_make ("audioconvert", NULL);
  GstPad *sink_pad = NULL;
  GstPad *src_pad = NULL;

  DEBUG ("Adding converters");

  if (convert == NULL)
    WARNING ("Could not create audioconverter");
  if (resample == NULL)
    WARNING ("Could not create audioresampler");
  if (convert2 == NULL)
    WARNING ("Could not create second audioconverter");

  if (convert != NULL &&
      gst_bin_add (GST_BIN (self), convert) == FALSE) {
    WARNING ("Could not add converter to bin");
    gst_object_unref (convert);
    convert = NULL;
  }
  if (resample != NULL &&
      gst_bin_add (GST_BIN (self), resample) == FALSE) {
    WARNING ("Could not add resampler to bin");
    gst_object_unref (resample);
    resample = NULL;
  }
  if (convert2 != NULL &&
      gst_bin_add (GST_BIN (self), convert2) == FALSE) {
    WARNING ("Could not add second converter to bin");
    gst_object_unref (convert2);
    convert2 = NULL;
  }

  if (convert != NULL) {
    sink_pad = gst_element_get_static_pad (convert, "sink");
    src_pad = gst_element_get_static_pad (convert, "src");

    if (src_pad == NULL || sink_pad == NULL ||
        gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK)  {
      WARNING ("Could not get converter pads (%p - %p) or link the source to it",
          src_pad, sink_pad);
      gst_bin_remove (GST_BIN (self), convert);
      if (src_pad != NULL)
        gst_object_unref (src_pad);
      if (sink_pad != NULL)
        gst_object_unref (sink_pad);
    } else {
      DEBUG ("audioconvert added");
      gst_object_unref (sink_pad);
      gst_object_unref (pad);
      pad = src_pad;
    }
  }

  if (resample != NULL) {
    sink_pad = gst_element_get_static_pad (resample, "sink");
    src_pad = gst_element_get_static_pad (resample, "src");

    if (src_pad == NULL || sink_pad == NULL ||
        gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK)  {
      WARNING ("Could not get resampler pads (%p - %p) or link the source to it",
          src_pad, sink_pad);
      gst_bin_remove (GST_BIN (self), resample);
      if (src_pad != NULL)
        gst_object_unref (src_pad);
      if (sink_pad != NULL)
        gst_object_unref (sink_pad);
    } else {
      DEBUG ("audioresample added");
      gst_object_unref (sink_pad);
      gst_object_unref (pad);
      pad = src_pad;
    }
  }

  if (convert2 != NULL) {
    sink_pad = gst_element_get_static_pad (convert2, "sink");
    src_pad = gst_element_get_static_pad (convert2, "src");

    if (src_pad == NULL || sink_pad == NULL ||
        gst_pad_link(pad, sink_pad) != GST_PAD_LINK_OK)  {
      WARNING ("Could not get converter2 pads (%p - %p) or link the source to it",
          src_pad, sink_pad);
      gst_bin_remove (GST_BIN (self), convert2);
      if (src_pad != NULL)
        gst_object_unref (src_pad);
      if (sink_pad != NULL)
        gst_object_unref (sink_pad);
    } else {
      DEBUG ("audioconvert2 added");
      gst_object_unref (sink_pad);
      gst_object_unref (pad);
      pad = src_pad;
    }
  }

  return pad;
}


static void
fsu_audio_source_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_audio_source_details);
}

static void
fsu_audio_source_class_init (FsuAudioSourceClass *klass)
{
  FsuSourceClass *fsu_source_class = FSU_SOURCE_CLASS (klass);

  fsu_source_class->priority_sources = audio_priority_sources;
  fsu_source_class->blacklisted_sources = audio_blacklisted_sources;
  fsu_source_class->klass_check = is_audio_source;
  fsu_source_class->add_converters = add_converters;
}

static void
fsu_audio_source_init (FsuAudioSource *self, FsuAudioSourceClass *klass)
{
}
