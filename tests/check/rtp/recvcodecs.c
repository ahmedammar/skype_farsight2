/* Farsight 2 unit tests for FsRtpConference
 *
 * Copyright (C) 2007 Collabora, Nokia
 * @author: Olivier Crete <olivier.crete@collabora.co.uk>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/rtp/gstrtpbuffer.h>

#include <gst/farsight/fs-conference-iface.h>

#include "check-threadsafe.h"

static void
src_pad_added_cb (FsStream *self,
    GstPad   *pad,
    FsCodec  *codec,
    GstElement *pipeline)
{
  GstElement *sink;
  GstPad *sinkpad;

  sink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (sink, "sync", TRUE, NULL);
  fail_unless (gst_bin_add (GST_BIN (pipeline), sink));
  gst_element_set_state (sink, GST_STATE_PLAYING);
  sinkpad = gst_element_get_static_pad (sink, "sink");
  fail_unless (GST_PAD_LINK_SUCCESSFUL (gst_pad_link (pad, sinkpad)));
  gst_object_unref (sinkpad);

  GST_DEBUG ("Pad added");
}

GST_START_TEST (test_rtprecv_no_config_data)
{
  FsParticipant *participant = NULL;
  FsStream *stream = NULL;
  GstElement *pipeline;
  GstElement *sink;
  GstBus *bus;
  GstMessage *msg;
  guint port = 0;
  GError *error = NULL;
  GList *codecs = NULL;
  GstElement *fspipeline;
  GstElement *conference;
  FsSession *session;
  GList *item;

  fspipeline = gst_pipeline_new (NULL);

  conference = gst_element_factory_make ("fsrtpconference", NULL);

  fail_unless (gst_bin_add (GST_BIN (fspipeline), conference));

  session = fs_conference_new_session (FS_CONFERENCE (conference),
      FS_MEDIA_TYPE_VIDEO, &error);
  if (error)
    fail ("Error while creating new session (%d): %s",
        error->code, error->message);
  fail_if (session == NULL, "Could not make session, but no GError!");


  for (item = codecs; item; item = item->next)
  {
    FsCodec *codec = item->data;

    if (!g_ascii_strcasecmp ("THEORA", codec->encoding_name))
      break;
  }
  fs_codec_list_destroy (codecs);

  if (!item)
  {
    GST_INFO ("Skipping %s because THEORA is not detected", G_STRFUNC);
    g_object_unref (session);
    gst_object_unref (fspipeline);
    return;
  }

  participant = fs_conference_new_participant (
      FS_CONFERENCE (conference), "blob@blob.com", &error);
  if (error)
    ts_fail ("Error while creating new participant (%d): %s",
        error->code, error->message);
  ts_fail_if (participant == NULL,
      "Could not make participant, but no GError!");

  stream = fs_session_new_stream (session, participant,
      FS_DIRECTION_RECV, "rawudp", 0, NULL, &error);
  if (error)
    ts_fail ("Error while creating new stream (%d): %s",
        error->code, error->message);
  ts_fail_if (stream == NULL, "Could not make stream, but no GError!");

  g_signal_connect (stream, "src-pad-added",
      G_CALLBACK (src_pad_added_cb), fspipeline);

  g_object_get (session, "codecs-without-config", &codecs, NULL);


  codecs = g_list_prepend (NULL, fs_codec_new (96, "THEORA",
          FS_MEDIA_TYPE_VIDEO, 90000));
  fail_unless (fs_session_set_codec_preferences (session, codecs,
          &error),
      "Unable to set codec preferences: %s",
      error ? error->message : "UNKNOWN");

  fs_codec_list_destroy (codecs);


  pipeline = gst_parse_launch ("videotestsrc is-live=1 num-buffers=50 !"
      " video/x-raw-yuv, framerate=(fraction)30/1 ! theoraenc !"
      " rtptheorapay name=pay ! application/x-rtp, payload=96 !"
      " udpsink host=127.0.0.1 name=sink", NULL);

  gst_element_set_state (fspipeline, GST_STATE_PLAYING);


  bus = gst_element_get_bus (fspipeline);
  while (port == 0)
  {
    const GstStructure *s;

    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
        GST_MESSAGE_ELEMENT);
    fail_unless (msg != NULL);
    s = gst_message_get_structure (msg);

    fail_if (gst_structure_has_name (s, "farsight-local-candidates-prepared"));

    if (gst_structure_has_name (s, "farsight-new-local-candidate"))
    {
      const GValue *value;
      FsCandidate *candidate;

      ts_fail_unless (
          gst_structure_has_field_typed (s, "candidate", FS_TYPE_CANDIDATE),
          "farsight-new-local-candidate structure has no candidate field");

      value = gst_structure_get_value (s, "candidate");
      candidate = g_value_get_boxed (value);

      if (candidate->type == FS_CANDIDATE_TYPE_HOST)
        port = candidate->port;

      GST_DEBUG ("Got port %u", port);
    }

    gst_message_unref (msg);
  }

  gst_object_unref (bus);

  sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
  g_object_set (sink, "port", port, NULL);
  gst_object_unref (sink);


  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_EOS);
  gst_message_unref (msg);
  gst_object_unref (bus);

  bus = gst_element_get_bus (fspipeline);
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_ERROR);
  if (msg)
  {
    GError *error;
    gchar *debug;

    gst_message_parse_error (msg, &error, &debug);

    ts_fail ("Got an error on the BUS (%d): %s (%s)", error->code,
        error->message, debug);
    g_error_free (error);
    g_free (debug);
    gst_message_unref (msg);
  }
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  gst_object_unref (participant);
  gst_object_unref (stream);
  gst_object_unref (session);

  gst_element_set_state (fspipeline, GST_STATE_NULL);



  gst_object_unref (fspipeline);
}
GST_END_TEST;


static Suite *
fsrtprecvcodecs_suite (void)
{
  Suite *s = suite_create ("fsrtprecvcodecs");
  TCase *tc_chain;
  GLogLevelFlags fatal_mask;

  fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
  fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
  g_log_set_always_fatal (fatal_mask);

  tc_chain = tcase_create ("fsrtprecv_no_config_data");
  tcase_add_test (tc_chain, test_rtprecv_no_config_data);
  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (fsrtprecvcodecs);
