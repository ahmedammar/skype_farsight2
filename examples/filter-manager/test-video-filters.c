#include <gst/gst.h>
#include <gst/farsight/fsu-multi-filter-manager.h>
#include <gst/farsight/fsu-videoconverter-filter.h>
#include <gst/farsight/fsu-resolution-filter.h>
#include <gst/farsight/fsu-effectv-filter.h>
#include <gst/farsight/fsu-framerate-filter.h>
#include <gst/farsight/fsu-preview-filter.h>


#define TIMEOUT 1000
#define TWICE 1

static FsuFilterId *effect_id = NULL;
static FsuFilterId *framerate_id = NULL;
static FsuFilterId *resolution_id = NULL;
static FsuFilterId *last_converter_id = NULL;
static GMainLoop *mainloop = NULL;

static gboolean dump  (gpointer data)
{

  g_debug ("Dumping pipeline");
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS   (data, GST_DEBUG_GRAPH_SHOW_ALL, "test-filters");

  return TRUE;
}

static gboolean add_effects  (gpointer data)
{
  FsuFilterManager *filters = data;
  static gint idx = 0;
  static gchar *effects[] = {"agingtv",
                             "dicetv",
                             "warptv",
                             "quarktv",
                             "rippletv",
  };
  FsuEffectvFilter *filter = fsu_effectv_filter_new (effects[idx]);
  FsuVideoconverterFilter *f1 = fsu_videoconverter_filter_get_singleton ();

  g_debug ("Insert filter %s", effects[idx]);
  fsu_filter_manager_insert_filter_before (filters, FSU_FILTER (filter), last_converter_id);

  idx++;
  if (idx > 4) {
    GList *f = fsu_filter_manager_list_filters (filters);
    GList *i;
    gboolean remove = FALSE;

    for (i = f; i; i = i->next) {
      if (i->data == effect_id)
        remove = TRUE;
      if (i->data == last_converter_id)
        remove = FALSE;
      if (remove)
        fsu_filter_manager_remove_filter (filters, i->data);
      g_timeout_add (TIMEOUT, (GSourceFunc) g_main_loop_quit, mainloop);
    }

    return FALSE;
  }

  return TRUE;
}

static gboolean change_effect  (gpointer data)
{
  FsuFilterManager *filters = data;
  static gint idx = 0;
  static gchar *effects[] = {"bullshit",
                             "edgetv",
                             "agingtv",
                             "dicetv",
                             "warptv",
                             "shagadelictv",
                             "vertigotv",
                             "revtv",
                             "quarktv",
                             "optv",
                             "streaktv",
                             "rippletv",
  };
  FsuEffectvFilter *filter = fsu_effectv_filter_new (effects[idx]);

  if (effect_id == NULL)
  {
    idx++;
    return TRUE;
  }

  g_debug ("Changing effect to %s", effects[idx]);
  effect_id = fsu_filter_manager_replace_filter (filters, FSU_FILTER (filter), effect_id);

  idx++;
  if (idx > 10) {
    g_timeout_add (TIMEOUT, add_effects, filters);
    return FALSE;
  }

  return TRUE;
}

static gboolean change_reso  (gpointer data)
{
  FsuFilterManager *filters = data;
  static gint width = 320;
  static gint height = 240;
  FsuResolutionFilter *filter = fsu_resolution_filter_new (width, height);

  resolution_id = fsu_filter_manager_replace_filter (filters, FSU_FILTER (filter), resolution_id);

  if (width == 1280) {
    return FALSE;
  }

  width /= 2;
  height /= 2;

  if (width < 80) {
    width = 1280;
    height = 960;
    g_timeout_add (TIMEOUT, change_effect, filters);
  }

  return TRUE;
}

static gboolean change_fps  (gpointer data)
{
  FsuFilterManager *filters = data;
  static gint fps = 10;
  FsuFramerateFilter *filter = FSU_FRAMERATE_FILTER (
      fsu_filter_manager_get_filter_by_id (filters, framerate_id));

  if (fps == 1) {
    g_object_set (filter, "fps", 30, NULL);
    printf ("Setting FPS back to 30, changing resolutions now\n\n");
    g_timeout_add (TIMEOUT, change_reso, filters);
    return FALSE;
  }

  fps -= 1;

  if (fps <= 0)
    fps = 1;

  printf ("Setting FPS to %d\n\n", fps);
  g_object_set (filter, "fps", fps, NULL);

  return TRUE;
}


static gboolean add_preview  (gpointer data)
{
  FsuFilterManager *filters = data;

  g_debug ("Adding preview filter");
  fsu_filter_manager_append_filter (filters,
      FSU_FILTER (fsu_preview_filter_new (GINT_TO_POINTER (0))));
  //g_timeout_add (5000, change_fps, filters);
  //g_timeout_add (5000, change_reso, filters);
  g_timeout_add (TIMEOUT, change_effect, filters);

  return FALSE;
}
static gboolean add_filters  (gpointer data)
{
  FsuFilterManager *filters = data;
  FsuFilter *f1 = FSU_FILTER (fsu_videoconverter_filter_new ());
  FsuFilter *f2 = FSU_FILTER (fsu_resolution_filter_new (640, 480));
  FsuFilter *f3 = FSU_FILTER (fsu_framerate_filter_new (30));
  FsuFilter *f4 = FSU_FILTER (fsu_effectv_filter_new ("vertigotv"));

  g_debug ("timeout triggered");

  resolution_id = fsu_filter_manager_append_filter (filters, f2);
  fsu_filter_manager_prepend_filter (filters, f1);
  framerate_id = fsu_filter_manager_insert_filter (filters, f3, -1);
  fsu_filter_manager_append_filter (filters, f1);
  last_converter_id = fsu_filter_manager_insert_filter (filters, f1, 10);
  effect_id = fsu_filter_manager_insert_filter_before (filters, f4, last_converter_id);

  /*
  FsuFilter *f3 = fsu_effectv_filter_new ("vertigotv");
  fsu_filter_manager_insert_filter (filters, f3, 3);
  g_timeout_add (1000, add_effects, filters);
  //g_timeout_add (1000, change_effect, filters);
  */
  //g_timeout_add (5000, change_fps, filters);
  g_timeout_add (TIMEOUT/2, add_preview, filters);


  return FALSE;
}


int main (int argc, char *argv[]) {
  gst_init (&argc, &argv);

  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *src = gst_element_factory_make ("fsuvideosrc", NULL);
  GstElement *sink = gst_element_factory_make ("fakesink", NULL);
  g_assert (src != NULL);
  g_assert (sink != NULL);
  g_object_set (src,
      "source-pipeline", "videotestsrc is-live=TRUE pattern=1",
      NULL);
#if TWICE
  GstElement *src2 = gst_element_factory_make ("videotestsrc", NULL);
  GstElement *sink2 = gst_element_factory_make ("fakesink", NULL);
  g_assert (src2 != NULL);
  g_assert (sink2 != NULL);
#endif
  GstPad *out_pad = NULL;
  GstPad *src_pad = gst_element_get_request_pad (src, "src%d");
  GstPad *sink_pad = gst_element_get_static_pad (sink, "sink");
  g_assert (src_pad != NULL);
  g_assert (sink_pad != NULL);
#if TWICE
  GstPad *out_pad2 = NULL;
  GstPad *src_pad2 = gst_element_get_static_pad (src2, "src");
  GstPad *sink_pad2 = gst_element_get_static_pad (sink2, "sink");
  g_assert (src_pad2 != NULL);
  g_assert (sink_pad2 != NULL);
#endif
  FsuFilterManager *filters = fsu_multi_filter_manager_new ();
  mainloop = g_main_loop_new (NULL, FALSE);

  add_filters (filters);
  //g_timeout_add (TIMEOUT, (GSourceFunc) g_main_loop_quit, mainloop);

  g_assert (gst_bin_add (GST_BIN (pipeline), src) == TRUE);
  g_assert (gst_bin_add (GST_BIN (pipeline), sink) == TRUE);

  out_pad = fsu_filter_manager_apply (filters, GST_BIN (pipeline), sink_pad);
  g_assert (out_pad != NULL);
  gst_object_unref (sink_pad);
  g_assert (gst_pad_link (src_pad, out_pad) == GST_PAD_LINK_OK);

#if TWICE
  g_debug ("Creating second pipeline");

  g_assert (gst_bin_add (GST_BIN (pipeline), src2) == TRUE);
  g_assert (gst_bin_add (GST_BIN (pipeline), sink2) == TRUE);
  out_pad2 = fsu_filter_manager_apply (filters, GST_BIN (pipeline), src_pad2);
  g_assert (out_pad2 != NULL);
  gst_object_unref (src_pad2);
  g_assert (gst_pad_link (out_pad2, sink_pad2) == GST_PAD_LINK_OK);
#endif

  g_timeout_add (TIMEOUT, dump, pipeline);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_debug ("Running mainloop");
  g_main_loop_run (mainloop);
  g_debug ("Mainloop terminated");

  gst_element_set_state (pipeline, GST_STATE_NULL);

  g_assert (fsu_filter_manager_revert (filters, GST_BIN (pipeline),
          src_pad) == NULL);

  g_assert (gst_pad_unlink (src_pad, out_pad) == TRUE);
  gst_element_release_request_pad (src, src_pad);
  gst_object_unref (src_pad);
  gst_bin_remove (GST_BIN (pipeline), src);
  g_assert (fsu_filter_manager_revert (filters, GST_BIN (pipeline),
          out_pad) == sink_pad);
  gst_object_unref (sink_pad);
  gst_object_unref (out_pad);
  gst_bin_remove (GST_BIN (pipeline), sink);

#if TWICE
  g_assert (gst_pad_unlink (out_pad2, sink_pad2) == TRUE);
  gst_object_unref (sink_pad2);
  gst_bin_remove (GST_BIN (pipeline), sink2);
  g_assert (fsu_filter_manager_revert (filters, GST_BIN (pipeline),
          out_pad2) == src_pad2);
  gst_object_unref (out_pad2);
  gst_object_unref (src_pad2);
  gst_bin_remove (GST_BIN (pipeline), src2);
#endif

  g_object_unref (filters);
  gst_object_unref (pipeline);

  return 0;
}
