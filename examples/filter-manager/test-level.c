
#include <string.h>
#include <gst/gst.h>
#include <gst/filters/fsu-single-filter-manager.h>
#include <gst/filters/fsu-level-filter.h>

static GMainLoop *mainloop = NULL;


gboolean
done (gpointer data)
{
  g_debug ("Dumping pipeline");
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (data, GST_DEBUG_GRAPH_SHOW_ALL,
      "test-level");
  g_main_loop_quit (mainloop);
  return FALSE;
}

static void
got_level (FsuLevelFilter *filter, gdouble level)
{
  g_debug ("Got level : %f", level);
}


static GstBusSyncReply
_bus_callback (GstBus *bus, GstMessage *message, gpointer user_data)
{
  FsuFilterManager *filters = user_data;

  if (!fsu_filter_manager_handle_message (filters, message))
    return GST_BUS_PASS;

  gst_message_unref (message);
  return GST_BUS_DROP;
}


int main (int argc, char *argv[]) {
  GstElement *pipeline = NULL;
  GstBus *bus = NULL;
  GstElement *src = NULL;
  GstElement *sink = NULL;
  GstPad *out_pad = NULL;
  GstPad *src_pad = NULL;
  GstPad *sink_pad = NULL;
  FsuFilterManager *filters = NULL;
  FsuFilter *filter = NULL;

  gst_init (&argc, &argv);

  filters = fsu_single_filter_manager_new ();
  mainloop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);

  bus = gst_element_get_bus (pipeline);
  gst_bus_set_sync_handler (bus, _bus_callback, filters);
  gst_object_unref (bus);

  src = gst_element_factory_make ("fsuaudiosrc", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);
  g_assert (src != NULL);
  g_assert (sink != NULL);
  g_object_set (sink,
      "sync", FALSE,
      "async", FALSE,
      NULL);
  src_pad = gst_element_get_request_pad (src, "src%d");
  sink_pad = gst_element_get_static_pad (sink, "sink");
  g_assert (src_pad != NULL);
  g_assert (sink_pad != NULL);


  filter = FSU_FILTER (fsu_level_filter_new ());
  fsu_filter_manager_append_filter (filters, filter);

  g_signal_connect (filter, "level", (GCallback) got_level, NULL);
  g_object_unref (filter);

  g_assert (gst_bin_add (GST_BIN (pipeline), src) == TRUE);
  g_assert (gst_bin_add (GST_BIN (pipeline), sink) == TRUE);

  out_pad = fsu_filter_manager_apply (filters, GST_BIN (pipeline), sink_pad);
  g_assert (out_pad != NULL);
  gst_object_unref (sink_pad);
  g_assert (gst_pad_link (src_pad, out_pad) == GST_PAD_LINK_OK);

  g_timeout_add (5000, done, pipeline);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_debug ("Running mainloop");
  g_main_loop_run (mainloop);
  g_debug ("Mainloop terminated");

  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_assert (fsu_filter_manager_revert (filters, GST_BIN (pipeline),
          src_pad) == NULL);

  gst_object_unref (out_pad);
  out_pad = gst_pad_get_peer (src_pad);

  g_assert (gst_pad_unlink (src_pad, out_pad) == TRUE);
  gst_element_release_request_pad (src, src_pad);
  gst_object_unref (src_pad);
  gst_bin_remove (GST_BIN (pipeline), src);
  g_assert (fsu_filter_manager_revert (filters, GST_BIN (pipeline),
          out_pad) == sink_pad);
  gst_object_unref (sink_pad);
  gst_object_unref (out_pad);
  gst_bin_remove (GST_BIN (pipeline), sink);

  g_object_unref (filters);
  gst_object_unref (pipeline);
  g_main_loop_unref (mainloop);

  return 0;
}
