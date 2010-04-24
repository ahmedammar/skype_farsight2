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

GST_DEBUG_CATEGORY_STATIC (fsu_audio_sink_debug);
#define GST_CAT_DEFAULT fsu_audio_sink_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)

static const GstElementDetails fsu_audio_sink_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Audio Sink",
  "Audio Sink",
  "FsuAudioSink class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_audio_sink_debug, "fsuaudiosink", 0, "fsuaudiosink element");
}

GST_BOILERPLATE_FULL (FsuAudioSink, fsu_audio_sink,
    FsuSink, FSU_TYPE_SINK, _do_init)

static GstPad *
add_converters (FsuSink *self, GstPad *pad)
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
        gst_pad_link(src_pad, pad) != GST_PAD_LINK_OK)  {
      WARNING ("Could not get converter pads (%p - %p) or link the source to it",
          src_pad, sink_pad);
      gst_bin_remove (GST_BIN (self), convert);
      if (src_pad != NULL)
        gst_object_unref (src_pad);
      if (sink_pad != NULL)
        gst_object_unref (sink_pad);
    } else {
      DEBUG ("audioconvert added");
      gst_object_unref (src_pad);
      gst_object_unref (pad);
      pad = sink_pad;
    }
  }

  if (resample != NULL) {
    sink_pad = gst_element_get_static_pad (resample, "sink");
    src_pad = gst_element_get_static_pad (resample, "src");

    if (src_pad == NULL || sink_pad == NULL ||
        gst_pad_link(src_pad, pad) != GST_PAD_LINK_OK)  {
      WARNING ("Could not get resampler pads (%p - %p) or link the source to it",
          src_pad, sink_pad);
      gst_bin_remove (GST_BIN (self), resample);
      if (src_pad != NULL)
        gst_object_unref (src_pad);
      if (sink_pad != NULL)
        gst_object_unref (sink_pad);
    } else {
      DEBUG ("audioresample added");
      gst_object_unref (src_pad);
      gst_object_unref (pad);
      pad = sink_pad;
    }
  }

  if (convert2 != NULL) {
    sink_pad = gst_element_get_static_pad (convert2, "sink");
    src_pad = gst_element_get_static_pad (convert2, "src");

    if (src_pad == NULL || sink_pad == NULL ||
        gst_pad_link(src_pad, pad) != GST_PAD_LINK_OK)  {
      WARNING ("Could not get converter2 pads (%p - %p) or link the source to it",
          src_pad, sink_pad);
      gst_bin_remove (GST_BIN (self), convert2);
      if (src_pad != NULL)
        gst_object_unref (src_pad);
      if (sink_pad != NULL)
        gst_object_unref (sink_pad);
    } else {
      DEBUG ("audioconvert2 added");
      gst_object_unref (src_pad);
      gst_object_unref (pad);
      pad = sink_pad;
    }
  }

  return pad;
}


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
  fsu_sink_class->add_converters = add_converters;
}

static void
fsu_audio_sink_init (FsuAudioSink *self, FsuAudioSinkClass *klass)
{
}
