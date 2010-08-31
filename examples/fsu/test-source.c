
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

gboolean
dump  (gpointer data)
{

  g_debug ("Dumping pipeline");
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (data, GST_DEBUG_GRAPH_SHOW_ALL,
      "test-source");

  return TRUE;
}

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

#define TEST_SINK 0

static void
button_clicked (GtkButton *button, gpointer user_data)
{
  if (src_pad)
  {
    g_debug ("Stopping source");
    g_debug ("unlinking");
    gst_pad_unlink (src_pad, sink_pad);
    g_debug ("releasing source pad");
    gst_element_release_request_pad (src, src_pad);
    gst_object_unref (src_pad);
    src_pad = NULL;
#if TEST_SINK
    g_debug ("releasing sink pad");
    gst_element_release_request_pad (sink, sink_pad);
    gst_object_unref (sink_pad);
    sink_pad = NULL;
#endif
  }
  else
  {
    g_debug ("Starting source");
    g_debug ("Requesting source pad");
    src_pad = gst_element_get_request_pad (src, "src%d");
#if TEST_SINK
    g_debug ("Requesting sink pad");
    sink_pad = gst_element_get_request_pad (sink, "sink%d");
#endif
    g_debug ("linking");
    gst_pad_link (src_pad, sink_pad);
    dump (pipeline);
  }
}


static GstBusSyncReply
_bus_callback (GstBus *bus, GstMessage *message, gpointer user_data)
{

  g_debug ("Got message of type %s from %s", GST_MESSAGE_TYPE_NAME(message),
      GST_ELEMENT_NAME (GST_MESSAGE_SRC (message)));
  gst_message_unref (message);
  return GST_BUS_DROP;
}

int main (int argc, char *argv[])
{
  GMainLoop *mainloop = NULL;
  GstBus *bus = NULL;
  GtkWidget *window = NULL;
  GtkWidget *but = NULL;

  /* Init */
  gst_and_gtk_init (&argc, &argv);
  mainloop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);
  bus = gst_element_get_bus (pipeline);
  gst_bus_set_sync_handler (bus, _bus_callback, pipeline);
  gst_object_unref (bus);

  /* build pipeline */
  sink = gst_element_factory_make ("fsuvideosink", NULL);
  sink_pad = gst_element_get_request_pad (sink, "sink%d");
  gst_bin_add (GST_BIN (pipeline), sink);
  src = gst_element_factory_make ("fsuvideosrc", NULL);
  src_pad = gst_element_get_request_pad (src, "src%d");
  gst_bin_add (GST_BIN (pipeline), src);
  gst_pad_link (src_pad, sink_pad);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Build UI and run */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  but = gtk_button_new_with_label("Start/Stop source");
  gtk_container_add(GTK_CONTAINER(window), but);
  g_signal_connect (but, "clicked", (GCallback) button_clicked, NULL);
  gtk_widget_show_all(window);

  g_main_loop_run (mainloop);

  return 0;
}
