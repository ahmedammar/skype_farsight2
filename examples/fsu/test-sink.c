
#include <string.h>
#include <gst/gst.h>
#include <gst/filters/fsu-single-filter-manager.h>
#include <gst/filters/fsu-resolution-filter.h>
#include <gst/filters/fsu-gnome-effect-filter.h>
#include <gst/filters/fsu-preview-filter.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static GstElement *pipeline = NULL;
static GstElement *src = NULL;
static GstPad *src_pad = NULL;
static GstElement *sink = NULL;
static GstPad *sink_pad = NULL;

void
gst_and_gtk_init (int *argc, char ***argv)
{
  GOptionContext *optcontext;

  if (!g_thread_get_initialized ())
    g_thread_init(NULL);

  optcontext = g_option_context_new ("Grid Effects example");
  g_option_context_add_group (optcontext, gst_init_get_option_group ());
  g_option_context_add_group (optcontext, gtk_get_option_group (TRUE));
  g_option_context_parse (optcontext, argc, argv, NULL);
  g_option_context_free (optcontext);
}


int main (int argc, char *argv[])
{
  GMainLoop *mainloop = NULL;

  /* Init */
  gst_and_gtk_init (&argc, &argv);
  mainloop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);

  /* build pipeline */
  sink = gst_element_factory_make ("fsuaudiosink", NULL);
  sink_pad = gst_element_get_request_pad (sink, "sink%d");
  gst_bin_add (GST_BIN (pipeline), sink);
  src = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (src, "wave", 8, NULL);
  src_pad = gst_element_get_static_pad (src, "src");
  gst_bin_add (GST_BIN (pipeline), src);
  gst_pad_link (src_pad, sink_pad);
  sink_pad = gst_element_get_request_pad (sink, "sink%d");
  src = gst_element_factory_make ("audiotestsrc", NULL);
  src_pad = gst_element_get_static_pad (src, "src");
  gst_bin_add (GST_BIN (pipeline), src);
  gst_pad_link (src_pad, sink_pad);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "test-sink");

  g_main_loop_run (mainloop);

  return 0;
}
