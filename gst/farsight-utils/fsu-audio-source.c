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

static const GstElementDetails fsu_audio_source_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Audio Source",
  "Audio Source",
  "FsuAudioSource class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

GST_BOILERPLATE (FsuAudioSource, fsu_audio_source,
    FsuSource, FSU_TYPE_SOURCE)

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
}

static void
fsu_audio_source_init (FsuAudioSource *self, FsuAudioSourceClass *klass)
{
}
