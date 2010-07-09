/*
 * fsu-resolution-filter.c - Source for FsuResolutionFilter
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


#include <gst/farsight/fsu-resolution-filter.h>
#include <gst/farsight/fsu-filter-helper.h>

G_DEFINE_TYPE (FsuResolutionFilter, fsu_resolution_filter, FSU_TYPE_FILTER);

static void fsu_resolution_filter_constructed (GObject *object);
static void fsu_resolution_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_resolution_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void fsu_resolution_filter_dispose (GObject *object);
static GstPad *fsu_resolution_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_resolution_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);

#define DEFAULT_WIDTH 320
#define DEFAULT_HEIGHT 240

/* properties */
enum
{
  PROP_WIDTH = 1,
  PROP_HEIGHT,
  LAST_PROPERTY
};


struct _FsuResolutionFilterPrivate
{
  gboolean dispose_has_run;
  GList *elements;
  GstCaps *caps;
  gint width;
  gint height;
};

static void
fsu_resolution_filter_class_init (FsuResolutionFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuResolutionFilterPrivate));

  gobject_class->constructed = fsu_resolution_filter_constructed;
  gobject_class->get_property = fsu_resolution_filter_get_property;
  gobject_class->set_property = fsu_resolution_filter_set_property;
  gobject_class->dispose = fsu_resolution_filter_dispose;

  fsufilter_class->apply = fsu_resolution_filter_apply;
  fsufilter_class->revert = fsu_resolution_filter_revert;
  fsufilter_class->name = "resolution";

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_uint ("width", "The width",
          "Set requested width of the image",
          0, G_MAXINT,
          DEFAULT_WIDTH,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_uint ("height", "The height",
          "Set requested height of the image",
          0, G_MAXINT,
          DEFAULT_HEIGHT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_resolution_filter_init (FsuResolutionFilter *self)
{
  FsuResolutionFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_RESOLUTION_FILTER,
          FsuResolutionFilterPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->width = DEFAULT_WIDTH;
  priv->height = DEFAULT_HEIGHT;
}


static void
fsu_resolution_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (object);
  FsuResolutionFilterPrivate *priv = self->priv;


  switch (property_id)
  {
    case PROP_WIDTH:
      g_value_set_uint (value, priv->width);
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, priv->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_resolution_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (object);
  FsuResolutionFilterPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_WIDTH:
      priv->width = g_value_get_uint (value);
      break;
    case PROP_HEIGHT:
      priv->height = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_resolution_filter_dispose (GObject *object)
{
  FsuResolutionFilter *self = (FsuResolutionFilter *)object;
  FsuResolutionFilterPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  gst_caps_unref (priv->caps);

  G_OBJECT_CLASS (fsu_resolution_filter_parent_class)->dispose (object);
}

static void
fsu_resolution_filter_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
      G_OBJECT_CLASS (fsu_resolution_filter_parent_class)->constructed;
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (object);
  FsuResolutionFilterPrivate *priv = self->priv;

  if (chain_up)
    chain_up (object);

  priv->caps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
          "width", G_TYPE_INT, priv->width,
          "height", G_TYPE_INT, priv->height,
          NULL),
      gst_structure_new ("video/x-raw-rgb",
          "width", G_TYPE_INT, priv->width,
          "height", G_TYPE_INT, priv->height,
          NULL),
      gst_structure_new ("video/x-raw-gray",
          "width", G_TYPE_INT, priv->width,
          "height", G_TYPE_INT, priv->height,
          NULL),
      NULL);
}

FsuResolutionFilter *
fsu_resolution_filter_new (guint width,
    guint height)
{
  return g_object_new (FSU_TYPE_RESOLUTION_FILTER,
      "width", width,
      "height", height,
      NULL);
}


static GstPad *
fsu_resolution_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (filter);
  FsuResolutionFilterPrivate *priv = self->priv;
  GstElement *capsfilter = NULL;
  GstElement *filter_bin = NULL;
  GstPad *out_pad = NULL;

  filter_bin = fsu_filter_add_element_by_description (bin, pad,
      "videoscale ! capsfilter name=capsfilter", &out_pad);

  if (filter_bin)
  {
    capsfilter = gst_bin_get_by_name (GST_BIN (filter_bin), "capsfilter");

    if (capsfilter)
    {
      priv->elements = g_list_prepend (priv->elements, capsfilter);
      gst_object_ref (capsfilter);

      g_object_set (capsfilter,
          "caps", priv->caps,
          NULL);
    }
    gst_object_unref (filter_bin);
  }

  return out_pad;
}

static GstPad *
fsu_resolution_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (filter);
  FsuResolutionFilterPrivate *priv = self->priv;
  GstElement *filter_bin = GST_ELEMENT (gst_pad_get_parent (pad));
  GstElement *capsfilter = NULL;

  capsfilter = gst_bin_get_by_name (GST_BIN (filter_bin), "capsfilter");
  if (g_list_find (priv->elements, capsfilter))
  {
    priv->elements = g_list_remove (priv->elements, capsfilter);
    gst_object_unref (capsfilter);
  }
  return fsu_filter_revert_bin (bin, pad);
}

