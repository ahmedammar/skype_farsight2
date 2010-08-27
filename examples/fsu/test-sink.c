
#include <string.h>
#include <gst/gst.h>

int main (int argc, char *argv[])
{
  GMainLoop *mainloop = NULL;
  GstElement *pipeline = NULL;
  GstElement *src = NULL;
  GstElement *src2 = NULL;
  GstPad *src_pad = NULL;
  GstPad *src_pad2 = NULL;
  GstElement *sink = NULL;
  GstPad *sink_pad = NULL;
  GstPad *sink_pad2 = NULL;

  /* Init */
  gst_init (&argc, &argv);
  mainloop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);

  /* build pipeline */
  sink = gst_element_factory_make ("fsuaudiosink", NULL);
  src = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (src, "wave", 6, NULL);
  src2 = gst_element_factory_make ("audiotestsrc", NULL);
  sink_pad = gst_element_get_request_pad (sink, "sink%d");
  sink_pad2 = gst_element_get_request_pad (sink, "sink%d");
  src_pad = gst_element_get_static_pad (src, "src");
  src_pad2 = gst_element_get_static_pad (src2, "src");

  gst_bin_add (GST_BIN (pipeline), sink);
  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), src2);
  gst_pad_link (src_pad, sink_pad);
  gst_pad_link (src_pad2, sink_pad2);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "test-sink");

  g_timeout_add (2000, (GSourceFunc) g_main_loop_quit, mainloop);

  g_main_loop_run (mainloop);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_pad_unlink (src_pad, sink_pad);
  gst_pad_unlink (src_pad2, sink_pad2);
  gst_element_release_request_pad (sink, sink_pad);
  gst_element_release_request_pad (sink, sink_pad2);

  gst_bin_remove (GST_BIN (pipeline), sink);
  gst_bin_remove (GST_BIN (pipeline), src);
  gst_bin_remove (GST_BIN (pipeline), src2);
  gst_object_unref (pipeline);

  return 0;
}
