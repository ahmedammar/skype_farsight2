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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/filters/fsu-preview-filter.h>
#include <gst/filters/fsu-filter-helper.h>
#include <gst/filters/fsu-single-filter-manager.h>


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
enum
{
  PROP_XID = 1,
  PROP_FILTER_MANAGER,
  LAST_PROPERTY
};



struct _FsuPreviewFilterPrivate
{
  gint xid;
  GstElement *sink;
  GstPad *sink_pad;
  FsuFilterManager *manager;
};

static void
fsu_preview_filter_class_init (FsuPreviewFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuPreviewFilterPrivate));

  gobject_class->get_property = fsu_preview_filter_get_property;
  gobject_class->set_property = fsu_preview_filter_set_property;
  gobject_class->dispose = fsu_preview_filter_dispose;

  fsufilter_class->apply = fsu_preview_filter_apply;
  fsufilter_class->revert = fsu_preview_filter_revert;
  fsufilter_class->name = "preview";

  /**
   * FsuPreviewFilter:xid:
   *
   * The X window id to use for embedding the preview window
   */
  g_object_class_install_property (gobject_class, PROP_XID,
      g_param_spec_int ("xid", "The X window id for the preview",
          "The X Window id to use for embedding the preview window",
          G_MININT, G_MAXINT, 0,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuPreviewFilter:filter-manager:
   *
   * The filter manager to apply on the preview window.
   * This preview filter has a filter manager of its own which allows you to
   * add custom filters specific to a preview window. You can access it through
   * this property and modify the filter manager as you see fit.
   */
  g_object_class_install_property (gobject_class, PROP_FILTER_MANAGER,
      g_param_spec_object ("filter-manager", "Filter manager",
          "The filter manager to apply on the preview window",
          FSU_TYPE_FILTER_MANAGER,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_preview_filter_init (FsuPreviewFilter *self)
{
  FsuPreviewFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_PREVIEW_FILTER,
          FsuPreviewFilterPrivate);

  self->priv = priv;
  priv->manager = fsu_single_filter_manager_new ();
}

static void
fsu_preview_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuPreviewFilter *self = FSU_PREVIEW_FILTER (object);
  FsuPreviewFilterPrivate *priv = self->priv;


  switch (property_id)
  {
    case PROP_XID:
      g_value_set_int (value, priv->xid);
      break;
    case PROP_FILTER_MANAGER:
      g_value_set_object (value, priv->manager);
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

  switch (property_id)
  {
    case PROP_XID:
      priv->xid = g_value_get_int (value);
      if (priv->sink)
        g_object_set (priv->sink, "xid", priv->xid, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_preview_filter_dispose (GObject *object)
{
  FsuPreviewFilter *self = FSU_PREVIEW_FILTER (object);
  FsuPreviewFilterPrivate *priv = self->priv;

  if (priv->sink_pad)
    gst_object_unref (priv->sink_pad);
  priv->sink_pad = NULL;
  if (priv->sink)
    gst_object_unref (priv->sink);
  priv->sink = NULL;

  if (priv->manager)
    g_object_unref (priv->manager);
  priv->manager = NULL;

  G_OBJECT_CLASS (fsu_preview_filter_parent_class)->dispose (object);
}


/**
 * fsu_preview_filter_new:
 * @xid: The window x-id in which to embed the video output
 *
 * Creates a new video preview filter.
 * This filter allows you to preview your video stream into a preview window by
 * creating a tee and linking it with an fsuvideosink to which it sets the
 * specified @xid value
 *
 * Returns: A new #FsuPreviewFilter
 */
FsuPreviewFilter *
fsu_preview_filter_new (gint xid)
{
  return g_object_new (FSU_TYPE_PREVIEW_FILTER,
      "xid", xid,
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
  GstPad *out_pad = NULL;
  GstPad *tee_pad = NULL;
  GstPad *preview_pad = NULL;
  GstPad *filter_pad = NULL;
  GstPad *sink_pad = NULL;

  if (priv->sink)
    return NULL;

  tee = gst_element_factory_make ("tee", NULL);
  sink = gst_element_factory_make ("fsuvideosink", NULL);

  if (!tee || !sink)
  {
    if (tee)
      gst_object_unref (tee);
    if (sink)
      gst_object_unref (sink);
    g_debug ("Could not create tee or fsuvideosink elements");
    return NULL;
  }

  if (GST_PAD_IS_SRC (pad))
  {
    tee_pad = gst_element_get_static_pad (tee, "sink");
    out_pad = gst_element_get_request_pad (tee, "src%d");
    if (out_pad)
      g_object_set (tee, "alloc-pad", out_pad, NULL);
  }
  else
  {
    out_pad = gst_element_get_static_pad (tee, "sink");
    tee_pad = gst_element_get_request_pad (tee, "src%d");
    if (tee_pad)
      g_object_set (tee, "alloc-pad", tee_pad, NULL);
  }
  preview_pad = gst_element_get_request_pad (tee, "src%d");
  sink_pad = gst_element_get_request_pad (sink, "sink%d");


  if (!tee_pad || !out_pad ||
      !preview_pad || !sink_pad)
  {
    if (GST_PAD_IS_SRC (pad) && out_pad)
      gst_element_release_request_pad (tee, out_pad);
    if (GST_PAD_IS_SINK (pad) && tee_pad)
      gst_element_release_request_pad (tee, tee_pad);
    if (tee_pad)
      gst_object_unref (tee_pad);
    if (out_pad)
      gst_object_unref (out_pad);
    if (preview_pad)
    {
      gst_element_release_request_pad (tee, preview_pad);
      gst_object_unref (preview_pad);
    }
    if (sink_pad)
    {
      gst_element_release_request_pad (sink, sink_pad);
      gst_object_unref (sink_pad);
    }

    gst_object_unref (tee);
    gst_object_unref (sink);

    g_debug ("Failed trying to request pads");
    return NULL;
  }

  if (!fsu_filter_add_element (bin, pad, tee, tee_pad))
  {
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
    gst_object_unref (tee);
    gst_object_unref (sink);
    return NULL;
  }

  g_object_set (sink,
      "xid", priv->xid,
      "sync", FALSE,
      "async", FALSE,
      NULL);

  filter_pad = fsu_filter_manager_apply (priv->manager, bin, preview_pad);

  if (!filter_pad)
    filter_pad = gst_object_ref (preview_pad);
  gst_object_unref (preview_pad);

  if (!fsu_filter_add_element (bin, filter_pad, sink, sink_pad))
  {
    if (GST_PAD_IS_SRC (pad) && out_pad)
      gst_element_release_request_pad (tee, out_pad);
    if (GST_PAD_IS_SINK (pad) && tee_pad)
      gst_element_release_request_pad (tee, tee_pad);
    gst_element_release_request_pad (tee, preview_pad);
    gst_object_unref (filter_pad);
    gst_element_release_request_pad (sink, sink_pad);
    gst_object_unref (sink_pad);
    gst_object_unref (tee_pad);
    gst_object_unref (out_pad);
    gst_bin_remove (bin, tee);
    gst_object_unref (sink);
    g_debug ("Failed trying to add sink");
    return NULL;
  }

  gst_object_unref (tee_pad);
  gst_object_unref (filter_pad);
  priv->sink = gst_object_ref (sink);
  priv->sink_pad = sink_pad;

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
  GstPad *filter_pad = NULL;
  GstPad *out_pad = NULL;

  if (GST_PAD_IS_SRC (pad))
  {
    GstPad *other_pad = NULL;
    other_pad = gst_element_get_static_pad (tee, "sink");
    out_pad = gst_pad_get_peer (other_pad);
    gst_object_unref (other_pad);
    tee_pad = gst_object_ref (pad);
  }
  else
  {
    g_object_get (tee, "alloc-pad", &tee_pad, NULL);
    out_pad = gst_pad_get_peer (tee_pad);
  }

  gst_element_release_request_pad (tee, tee_pad);
  gst_object_unref (tee_pad);
  filter_pad = gst_pad_get_peer (priv->sink_pad);
  tee_pad = fsu_filter_manager_revert (priv->manager, bin, filter_pad);
  if (!tee_pad)
    tee_pad = gst_object_ref (filter_pad);
  gst_object_unref (filter_pad);
  gst_element_release_request_pad (tee, tee_pad);
  gst_object_unref (tee_pad);
  gst_bin_remove (bin, tee);
  gst_element_set_state (tee, GST_STATE_NULL);
  gst_object_unref (tee);

  gst_element_release_request_pad (priv->sink, priv->sink_pad);
  gst_object_unref (priv->sink_pad);
  priv->sink_pad = NULL;
  gst_bin_remove (bin, priv->sink);
  gst_element_set_state (priv->sink, GST_STATE_NULL);
  gst_object_unref (priv->sink);
  priv->sink = NULL;

  return out_pad;
}

