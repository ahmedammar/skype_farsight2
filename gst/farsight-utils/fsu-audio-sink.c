/*
 * fsu-audio-sink.c - Sink for FsuAudioSink
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

#include "fsu-audio-sink.h"

static const GstElementDetails fsu_audio_sink_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Audio Sink",
  "Audio Sink",
  "FsuAudioSink class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

GST_BOILERPLATE (FsuAudioSink, fsu_audio_sink,
    FsuSink, FSU_TYPE_SINK)

static void
sink_element_added (GstBin *bin, GstElement *sink, gpointer user_data)
{

  /*g_object_set (sink, "async", FALSE, NULL);*/
}

static GstElement *
create_auto_sink (FsuSink *self)
{
  GstElement *sink = gst_element_factory_make ("autoaudiosink", NULL);

  g_signal_connect (sink, "element-added",
        G_CALLBACK (sink_element_added), NULL);

  return sink;
}

static gchar *
need_mixer (FsuSink *self, GstElement *sink)
{
  GstElementFactory *factory = gst_element_get_factory (sink);
  gchar *name = GST_PLUGIN_FEATURE_NAME(factory);
  gchar *no_mixer[] = {"pulsesink",
                       "oss4sink",
                       "osxaudiosink",
                       "fakesink",
                       NULL};
  gchar **ptr;

  /* TODO: better list of sinks */
  for (ptr = no_mixer; *ptr; ptr++) {
    if (strcmp (name, *ptr) == 0)
      return NULL;
  }

  return "liveadder";
}

static void
fsu_audio_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_audio_sink_details);
}

static void
fsu_audio_sink_class_init (FsuAudioSinkClass *klass)
{
  FsuSinkClass *fsu_sink_class = FSU_SINK_CLASS (klass);

  fsu_sink_class->auto_sink_name = "autoaudiosink";
  fsu_sink_class->create_auto_sink = create_auto_sink;
  fsu_sink_class->need_mixer = need_mixer;
}

static void
fsu_audio_sink_init (FsuAudioSink *self, FsuAudioSinkClass *klass)
{
}
