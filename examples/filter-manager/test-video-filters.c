#include <gst/gst.h>
#include <gst/farsight/fsu-multi-filter-manager.h>
#include <gst/farsight/fsu-videoconverter-filter.h>
#include <gst/farsight/fsu-resolution-filter.h>
#include <gst/farsight/fsu-effectv-filter.h>
#include <gst/farsight/fsu-framerate-filter.h>
#include <gst/farsight/fsu-preview-filter.h>


static FsuFilterId *effect_id = NULL;
static FsuFilterId *framerate_id = NULL;
static FsuFilterId *resolution_id = NULL;
static FsuFilterId *last_converter_id = NULL;


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

  g_debug ("Changing effect to %s", effects[idx]);
  effect_id = fsu_filter_manager_replace_filter (filters, FSU_FILTER (filter), effect_id);

  idx++;
  if (idx > 10) {
    g_timeout_add (10000, add_effects, filters);
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
    FsuEffectvFilter *f2 = fsu_effectv_filter_new ("vertigotv");
    effect_id = fsu_filter_manager_insert_filter_before (filters, FSU_FILTER (f2), last_converter_id);
    g_timeout_add (10000, change_effect, filters);
    return FALSE;
  }

  width /= 2;
  height /= 2;

  if (width < 80) {
    width = 1280;
    height = 960;
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
    g_timeout_add (10000, change_reso, filters);
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
  FsuEffectvFilter *f2 = fsu_effectv_filter_new ("vertigotv");
  effect_id = fsu_filter_manager_insert_filter_before (filters, FSU_FILTER (f2), last_converter_id);
  g_timeout_add (10000, change_effect, filters);

  return FALSE;
}
static gboolean add_filters  (gpointer data)
{
  FsuFilterManager *filters = data;
  FsuFilter *f1 = FSU_FILTER (fsu_videoconverter_filter_new ());
  FsuFilter *f2 = FSU_FILTER (fsu_resolution_filter_new (1280, 960));
  FsuFilter *f3 = FSU_FILTER (fsu_framerate_filter_new (30));

  g_debug ("timeout triggered");

  resolution_id = fsu_filter_manager_append_filter (filters, f2);
  fsu_filter_manager_prepend_filter (filters, f1);
  framerate_id = fsu_filter_manager_insert_filter (filters, f3, -1);
  fsu_filter_manager_append_filter (filters, f1);
  last_converter_id = fsu_filter_manager_insert_filter (filters, f1, 10);

  /*
  FsuFilter *f3 = fsu_effectv_filter_new ("vertigotv");
  fsu_filter_manager_insert_filter (filters, f3, 3);
  g_timeout_add (1000, add_effects, filters);
  //g_timeout_add (1000, change_effect, filters);
  */
  //g_timeout_add (5000, change_fps, filters);
  g_timeout_add (5000, add_preview, filters);


  return FALSE;
}


int main (int argc, char *argv[]) {
  gst_init (&argc, &argv);

  GstElement *pipeline = gst_pipeline_new (NULL);
  GstElement *src = gst_element_factory_make ("fsuvideosrc", NULL);
  GstElement *sink = gst_element_factory_make ("fakesink", NULL);
  g_assert (src != NULL);
  g_assert (sink != NULL);
  //g_object_set (sink, "sink-name", "fpsdisplaysink", NULL);
  GstPad *src_pad = gst_element_get_request_pad (src, "src%d");
  //  GstPad *sink_pad = gst_element_get_request_pad (sink, "sink%d");
  GstPad *sink_pad = gst_element_get_static_pad (sink, "sink");
  GstPad *out_pad = NULL;
  FsuFilterManager *filters = fsu_multi_filter_manager_new ();
  add_filters (filters);

  g_assert (src_pad != NULL);
  g_assert (sink_pad != NULL);

  g_assert (gst_bin_add (GST_BIN (pipeline), src) == TRUE);
  g_assert (gst_bin_add (GST_BIN (pipeline), sink) == TRUE);

  out_pad = fsu_filter_manager_apply (filters, GST_BIN (pipeline), sink_pad);

  g_assert (out_pad != NULL);

  g_assert (gst_pad_link (src_pad, out_pad) == GST_PAD_LINK_OK);

  g_debug ("Creating second pipeline");

  src = gst_element_factory_make ("videotestsrc", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);
  src_pad = gst_element_get_static_pad (src, "src");
  sink_pad = gst_element_get_static_pad (sink, "sink");
  g_assert (src_pad != NULL);
  g_assert (sink_pad != NULL);

  g_assert (gst_bin_add (GST_BIN (pipeline), src) == TRUE);
  g_assert (gst_bin_add (GST_BIN (pipeline), sink) == TRUE);

  out_pad = fsu_filter_manager_apply (filters, GST_BIN (pipeline), sink_pad);

  g_assert (out_pad != NULL);

  g_assert (gst_pad_link (src_pad, out_pad) == GST_PAD_LINK_OK);

  //g_timeout_add (5000, add_filters, filters);
  g_timeout_add (10000, dump, pipeline);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_debug ("Running mainloop");
  g_main_loop_run (g_main_loop_new (NULL, FALSE));
  g_debug ("Mainloop terminated");

  return 0;
}
