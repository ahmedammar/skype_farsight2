/*
 * fsu-preview-filter.c - Source for FsuPreviewFilter
 *
 * Copyright (C) 2010 Collabora Ltd.
 *  @author: Youness Alaoui <youness.alaoui@collabora.co.uk>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <gst/farsight/fsu-preview-filter.h>
#include <gst/farsight/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuPreviewFilter, fsu_preview_filter, FSU_TYPE_FILTER);

static void fsu_preview_filter_dispose (GObject *object);
static void fsu_preview_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_preview_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static GstPad *fsu_preview_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_preview_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);


/* properties */
enum {
  PROP_ID = 1,
  LAST_PROPERTY
};



struct _FsuPreviewFilterPrivate
{
  gpointer id;
  GstElement *sink;
  GstPad *sink_pad;
};

static void
fsu_preview_filter_class_init (FsuPreviewFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuPreviewFilterPrivate));

  gobject_class->get_property = fsu_preview_filter_get_property;
  gobject_class->set_property = fsu_preview_filter_set_property;

  fsufilter_class->apply = fsu_preview_filter_apply;
  fsufilter_class->revert = fsu_preview_filter_revert;
  fsufilter_class->name = "preview";

  g_object_class_install_property (gobject_class, PROP_ID,
      g_param_spec_pointer ("id", "The id for the preview",
          "The id to use for embedding the preview window",
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_preview_filter_init (FsuPreviewFilter *self)
{
  FsuPreviewFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_PREVIEW_FILTER,
          FsuPreviewFilterPrivate);

  self->priv = priv;
}

static void
fsu_preview_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuPreviewFilter *self = FSU_PREVIEW_FILTER (object);
  FsuPreviewFilterPrivate *priv = self->priv;


  switch (property_id) {
    case PROP_ID:
      g_value_set_pointer (value, priv->id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_preview_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuPreviewFilter *self = FSU_PREVIEW_FILTER (object);
  FsuPreviewFilterPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_ID:
      priv->id = g_value_get_pointer (value);
      if (priv->sink)
        g_object_set (priv->sink, "id", priv->id, NULL);

      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

FsuPreviewFilter *
fsu_preview_filter_new (gpointer id)
{
  return g_object_new (FSU_TYPE_PREVIEW_FILTER,
      "id", id,
      NULL);
}


static GstPad *
fsu_preview_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuPreviewFilter *self = FSU_PREVIEW_FILTER (filter);
  FsuPreviewFilterPrivate *priv = self->priv;
  GstElement *tee = NULL;
  GstElement *sink = NULL;
  GstElement *filter_bin = NULL;
  GstPad *out_pad = NULL;
  GstPad *tee_pad = NULL;
  GstPad *preview_pad = NULL;
  GstPad *sink_pad = NULL;

  if (priv->sink)
    return NULL;

  tee = gst_element_factory_make ("tee", NULL);
  sink = gst_element_factory_make ("fsuvideosink", NULL);

  if (!tee || !sink) {
    if (tee)
      gst_object_unref (tee);
    if (sink)
      gst_object_unref (sink);
    g_debug ("Could not create tee or fsuvideosink elements");
    return NULL;
  }

  if (GST_PAD_IS_SRC (pad)) {
    tee_pad = gst_element_get_static_pad (tee, "sink");
    out_pad = gst_element_get_request_pad (tee, "src%d");
    if (out_pad)
      g_object_set (tee, "alloc-pad", out_pad, NULL);
  } else {
    out_pad = gst_element_get_static_pad (tee, "sink");
    tee_pad = gst_element_get_request_pad (tee, "src%d");
    if (tee_pad)
      g_object_set (tee, "alloc-pad", tee_pad, NULL);
  }
  preview_pad = gst_element_get_request_pad (tee, "src%d");
  sink_pad = gst_element_get_request_pad (sink, "sink%d");


  if (!tee_pad || !out_pad ||
      !preview_pad || !sink_pad) {
    gst_object_unref (tee);
    gst_object_unref (sink);
    if (GST_PAD_IS_SRC (pad) && out_pad)
      gst_element_release_request_pad (tee, out_pad);
    if (GST_PAD_IS_SINK (pad) && tee_pad)
      gst_element_release_request_pad (tee, tee_pad);
    if (tee_pad)
      gst_object_unref (tee_pad);
    if (out_pad)
      gst_object_unref (out_pad);
    if (preview_pad) {
      gst_element_release_request_pad (tee, preview_pad);
      gst_object_unref (preview_pad);
    }
    if (sink_pad) {
      gst_element_release_request_pad (sink, sink_pad);
      gst_object_unref (sink_pad);
    }
    g_debug ("Failed trying to add tee");
    return NULL;
  }

  if (!fsu_filter_add_element (bin, pad, tee, tee_pad)) {
    gst_object_unref (tee);
    gst_object_unref (sink);
    if (GST_PAD_IS_SRC (pad) && out_pad)
      gst_element_release_request_pad (tee, out_pad);
    if (GST_PAD_IS_SINK (pad) && tee_pad)
      gst_element_release_request_pad (tee, tee_pad);
    gst_element_release_request_pad (tee, preview_pad);
    gst_object_unref (preview_pad);
    gst_element_release_request_pad (sink, sink_pad);
    gst_object_unref (sink_pad);
    gst_object_unref (tee_pad);
    gst_object_unref (out_pad);
    g_debug ("Failed trying to add tee");
    return NULL;
  }



  if (!fsu_filter_add_element (bin, preview_pad, sink, sink_pad)) {
    gst_bin_remove (bin, tee);
    gst_object_unref (sink);
    if (GST_PAD_IS_SRC (pad) && out_pad)
      gst_element_release_request_pad (tee, out_pad);
    if (GST_PAD_IS_SINK (pad) && tee_pad)
      gst_element_release_request_pad (tee, tee_pad);
    gst_element_release_request_pad (tee, preview_pad);
    gst_object_unref (preview_pad);
    gst_element_release_request_pad (sink, sink_pad);
    gst_object_unref (sink_pad);
    gst_object_unref (tee_pad);
    gst_object_unref (out_pad);
    g_debug ("Failed trying to add sink");
    return NULL;
  }

  gst_object_unref (tee_pad);
  gst_object_unref (preview_pad);
  priv->sink = sink;
  priv->sink_pad = sink_pad;
  g_object_set (priv->sink, "id", priv->id, NULL);

  return out_pad;
}

static GstPad *
fsu_preview_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuPreviewFilter *self = FSU_PREVIEW_FILTER (filter);
  FsuPreviewFilterPrivate *priv = self->priv;
  GstElement *tee = GST_ELEMENT (gst_pad_get_parent (pad));
  GstPad *tee_pad = NULL;
  GstPad *out_pad = NULL;

  if (GST_PAD_IS_SRC (pad)) {
    GstPad *other_pad = NULL;
    other_pad = gst_element_get_static_pad (tee, "sink");
    out_pad = gst_pad_get_peer (other_pad);
    gst_object_unref (other_pad);
    tee_pad = gst_object_ref (pad);
  } else {
    g_object_get (tee, "alloc-pad", &tee_pad, NULL);
    out_pad = gst_pad_get_peer (tee_pad);
  }

  gst_bin_remove (bin, tee);
  gst_element_release_request_pad (tee, tee_pad);
  gst_object_unref (tee_pad);
  tee_pad = gst_pad_get_peer (priv->sink_pad);
  gst_element_release_request_pad (tee, tee_pad);
  gst_object_unref (tee_pad);
  gst_object_unref (tee);
  gst_bin_remove (bin, priv->sink);

  priv->sink = NULL;
  priv->sink_pad = NULL;

  return out_pad;
}

