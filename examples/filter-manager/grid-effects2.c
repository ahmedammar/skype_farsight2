
#include <string.h>
#include <gst/gst.h>
#include <gst/farsight/fsu-single-filter-manager.h>
#include <gst/farsight/fsu-resolution-filter.h>
#include <gst/farsight/fsu-gnome-effect-filter.h>
#include <gst/farsight/fsu-preview-filter.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static FsuFilter *preview = NULL;

static GList *effects = NULL;

static void button_clicked (GtkButton *button, gpointer user_data);


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


static guint *
create_grid (gint rows, gint columns)
{
  GtkWidget *table;
  GtkWidget *window = NULL;
  static guint *wids;
  gint i,j;

  wids = g_malloc0 (rows * columns * sizeof(guint));
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  table = gtk_table_new(rows * 2, columns, TRUE);
  gtk_container_add(GTK_CONTAINER(window), table);
  gtk_table_set_homogeneous (GTK_TABLE (table), FALSE);

  for (i = 0; i < columns; i++) {
    for (j = 0; j < rows; j++) {
      GtkWidget *da = gtk_drawing_area_new();
      GtkWidget *but = gtk_button_new();
      GdkWindow *gdk_win = NULL;
      gchar *name;
      gchar *filename = g_list_nth_data (effects, i * rows + j);
      FsuGnomeEffectFilter *f = NULL;
      GtkWidget *label = gtk_label_new(NULL);
      int k = 2*j;

      if (filename == NULL) continue;

      f = fsu_gnome_effect_filter_new (filename, NULL);
      g_object_get (f, "name", &name, NULL);
      gtk_label_set_text (GTK_LABEL (label), name);

      gtk_widget_set_size_request(da, 128, 96);
      gtk_widget_set_app_paintable(da, TRUE);
      gtk_widget_set_double_buffered(da, FALSE);

      gtk_container_add(GTK_CONTAINER(but), da);
      gtk_table_attach_defaults (GTK_TABLE(table), but, i, i+1, k, k+1);
      gtk_table_attach_defaults (GTK_TABLE(table), label, i, i+1, k+1, k+2);
      gtk_widget_realize(da);
      gtk_widget_realize(but);
      gtk_widget_realize(label);

      gdk_win = gtk_widget_get_window (da);
      wids[i * rows + j] = GDK_WINDOW_XID (gdk_win);
      gdk_window_unref (gdk_win);

      g_signal_connect (but, "clicked", (GCallback) button_clicked, filename);
    }
  }

  gtk_widget_show_all(window);

  return wids;
}

static void
button_clicked (GtkButton *button, gpointer user_data)
{
  static FsuFilterId *effect_id = NULL;
  static gchar *previous_filename = NULL;
  gchar *filename = user_data;
  FsuFilterManager *preview_manager = NULL;
  FsuFilter *filter = FSU_FILTER (fsu_gnome_effect_filter_new (filename, NULL));

  g_object_get (preview, "filter-manager", &preview_manager, NULL);

  if (effect_id == NULL) {
    /* Add the filter if there are none */
    effect_id = fsu_filter_manager_append_filter (preview_manager, filter);
    previous_filename = filename;
  } else if (!strcmp (filename, previous_filename)) {
    /* Remove the existing effect if it was clicked twice */
    fsu_filter_manager_remove_filter (preview_manager, effect_id);
    effect_id = NULL;
    filename = NULL;
  } else {
    /* Replace the existing effect with the new one*/
    effect_id = fsu_filter_manager_replace_filter (preview_manager, filter,
        effect_id);
  }

  previous_filename = filename;
  g_object_unref (filter);
  g_object_unref (preview_manager);
}

int main (int argc, char *argv[])
{
  GMainLoop *mainloop = NULL;
  GstElement *pipeline = NULL;
  GstElement *src = NULL;
  GstElement *sink = NULL;
  GstPad *src_pad = NULL;
  GstPad *sink_pad = NULL;
  guint *wids = NULL;
  FsuFilterManager *filters = NULL;
  FsuResolutionFilter *high_res = NULL;
  FsuResolutionFilter *low_res = NULL;
  FsuFilterId *res_id = NULL;
  FsuFilterManager *preview_manager = NULL;
  GList *l;
  int i;

  /* Init */
  gst_and_gtk_init (&argc, &argv);
  mainloop = g_main_loop_new (NULL, FALSE);
  pipeline = gst_pipeline_new (NULL);
  src = gst_element_factory_make ("fsuvideosrc", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);
  src_pad = gst_element_get_request_pad (src, "src%d");
  sink_pad = gst_element_get_static_pad (sink, "sink");
  filters = fsu_single_filter_manager_new ();

  /* build pipeline */
  gst_bin_add (GST_BIN (pipeline), src);
  gst_bin_add (GST_BIN (pipeline), sink);
  sink_pad = fsu_filter_manager_apply (filters, GST_BIN (pipeline), sink_pad);
  gst_pad_link (src_pad, sink_pad);
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Build UI and add filters */
  high_res = fsu_resolution_filter_new (640, 480);
  low_res = fsu_resolution_filter_new (128, 96);
  preview = FSU_FILTER (fsu_preview_filter_new (0));

  fsu_filter_manager_append_filter (filters, FSU_FILTER (low_res));
  res_id = fsu_filter_manager_prepend_filter (filters, FSU_FILTER (high_res));
  fsu_filter_manager_insert_filter_after (filters, preview, res_id);

  /* Add grid of effects */
  effects = fsu_gnome_effect_list_effects ("/usr/local/share/gnome-video-effects");
  wids = create_grid (4, 4);
  for (i = 0, l = effects; i < 16 && l; l = l->next, i++) {
    FsuPreviewFilter *prev = fsu_preview_filter_new (wids[i]);
    FsuGnomeEffectFilter *effect = fsu_gnome_effect_filter_new (l->data, NULL);

    fsu_filter_manager_append_filter (filters, FSU_FILTER (prev));
    g_object_get (prev, "filter-manager", &preview_manager, NULL);

    fsu_filter_manager_append_filter (preview_manager, FSU_FILTER (effect));
  }

  g_main_loop_run (mainloop);
  return 0;
}
