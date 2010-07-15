#include <gst/gst.h>
#include <gst/farsight/fsu-single-filter-manager.h>
#include <gst/farsight/fsu-videoconverter-filter.h>
#include <gst/farsight/fsu-resolution-filter.h>
#include <gst/farsight/fsu-effectv-filter.h>
#include <gst/farsight/fsu-preview-filter.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>


#define ROWS 3
#define COLUMNS 3
#define WIDTH 128
#define HEIGHT 96

static GMainLoop *mainloop = NULL;

gboolean
dump  (gpointer data)
{

  g_debug ("Dumping pipeline");
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (data, GST_DEBUG_GRAPH_SHOW_ALL,
      "grid-effects");

  return TRUE;
}

gboolean
done (gpointer data)
{
  g_main_loop_quit (mainloop);
  return FALSE;
}

static guint *
create_grid (int rows, int columns)
{
  GtkWidget *window;
  GtkWidget *table;
  guint *wids = NULL;
  gint i,j;

  wids = g_malloc (rows * columns * sizeof(guint));

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  table = gtk_table_new(rows, columns, TRUE);
  gtk_container_add(GTK_CONTAINER(window), table);

  for (i = 0; i < rows; i++)
    for (j = 0; j < columns; j++) {
      GtkWidget *da = gtk_drawing_area_new();

      gtk_widget_set_size_request(da, WIDTH, HEIGHT);
      gtk_widget_set_app_paintable(da, TRUE);
      gtk_widget_set_double_buffered(da, FALSE);

      gtk_table_attach_defaults (GTK_TABLE(table), da, i, i+1, j, j+1);
      gtk_widget_realize(da);
      wids[i * rows + j] = GDK_WINDOW_XID (gtk_widget_get_window (da));
    }

  gtk_widget_show_all(window);

  return wids;
}

gboolean
add_effect  (FsuFilterManager *filters, const gchar *effect, gint id)
{
  FsuFilter *preview = FSU_FILTER (fsu_preview_filter_new (
          GINT_TO_POINTER (id)));
  FsuFilterManager *preview_manager = NULL;
  FsuFilter *queue = FSU_FILTER (fsu_effectv_filter_new ("queue"));
  FsuFilter *filter = FSU_FILTER (fsu_effectv_filter_new (effect));

  fsu_filter_manager_append_filter (filters, preview);
  g_object_get (preview, "filter-manager", &preview_manager, NULL);
  g_object_unref (preview);

  fsu_filter_manager_append_filter (preview_manager, queue);
  fsu_filter_manager_append_filter (preview_manager, filter);
  g_object_unref (queue);
  g_object_unref (filter);
  g_object_unref (preview_manager);

  return TRUE;
}


gboolean
add_filters  (gpointer data)
{
  FsuFilterManager *filters = data;
  static gchar *effects[] = {"edgetv",
                             "agingtv",
                             "dicetv",
                             "warptv",
                             "shagadelictv",
                             "vertigotv",
                             "revtv",
                             "quarktv",
                             "rippletv",
                             NULL};

  guint *wids = create_grid (ROWS, COLUMNS);
  int i = 0;
  FsuFilter *f1 = FSU_FILTER (fsu_videoconverter_filter_new ());
  FsuFilter *f2 = FSU_FILTER (fsu_resolution_filter_new (1280, 960));
  FsuFilter *f3 = FSU_FILTER (fsu_preview_filter_new (GINT_TO_POINTER (0)));
  FsuFilter *f4 = FSU_FILTER (fsu_resolution_filter_new (WIDTH, HEIGHT));

  fsu_filter_manager_append_filter (filters, f1);
  fsu_filter_manager_append_filter (filters, f2);
  fsu_filter_manager_append_filter (filters, f3);
  fsu_filter_manager_append_filter (filters, f4);

  g_object_unref (f1);
  g_object_unref (f2);
  g_object_unref (f3);
  g_object_unref (f4);

  for (i = 0; effects[i]; i++)
    add_effect (filters, effects[i], wids[i]);

  g_free (wids);

  return FALSE;
}


int main (int argc, char *argv[]) {
  GstElement *pipeline = NULL;
  GstElement *src = NULL;
  GstElement *sink = NULL;
  GstPad *out_pad = NULL;
  GstPad *src_pad = NULL;
  GstPad *sink_pad = NULL;
  FsuFilterManager *filters = NULL;
  GError *error = NULL;
  GOptionContext *optcontext;

  optcontext = g_option_context_new ("Grid Effects example");
  g_option_context_add_group (optcontext, gst_init_get_option_group ());
  g_option_context_add_group (optcontext, gtk_get_option_group (TRUE));

  if (g_option_context_parse (optcontext, &argc, &argv, &error) == FALSE) {
    g_print ("%s\nRun '%s --help' to see a full list of available command line options.\n",
        error->message, argv[0]);
    return 1;
  }

  g_option_context_free (optcontext);

  mainloop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);

  src = gst_element_factory_make ("fsuvideosrc", NULL);
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

  filters = fsu_single_filter_manager_new ();


  add_filters (filters);

  g_assert (gst_bin_add (GST_BIN (pipeline), src) == TRUE);
  g_assert (gst_bin_add (GST_BIN (pipeline), sink) == TRUE);

  out_pad = fsu_filter_manager_apply (filters, GST_BIN (pipeline), sink_pad);
  g_assert (out_pad != NULL);
  gst_object_unref (sink_pad);
  g_assert (gst_pad_link (src_pad, out_pad) == GST_PAD_LINK_OK);

  g_timeout_add (5000, dump, pipeline);
  g_timeout_add (60000, done, pipeline);

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
