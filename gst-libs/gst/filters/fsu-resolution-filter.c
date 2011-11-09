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


#include <gst/filters/fsu-resolution-filter.h>
#include <gst/filters/fsu-filter-helper.h>

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

#define DEFAULT_WIDTH 240 
#define DEFAULT_HEIGHT 128

/* properties */
enum
{
  PROP_MIN_WIDTH = 1,
  PROP_MAX_WIDTH,
  PROP_MIN_HEIGHT,
  PROP_MAX_HEIGHT,
  LAST_PROPERTY
};


struct _FsuResolutionFilterPrivate
{
  GstCaps *caps;
  gint min_width;
  gint max_width;
  gint min_height;
  gint max_height;
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

  /**
   * FsuResolutionFilter:min-width:
   *
   * The minimum width of the video
   */
  g_object_class_install_property (gobject_class, PROP_MIN_WIDTH,
      g_param_spec_uint ("min-width", "The minimum width",
          "The minimum width of the image",
          0, G_MAXINT,
          DEFAULT_WIDTH,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuResolutionFilter:max-width:
   *
   * The maximum width of the video
   */
  g_object_class_install_property (gobject_class, PROP_MAX_WIDTH,
      g_param_spec_uint ("max-width", "The maximum width",
          "The maximum width of the image",
          0, G_MAXINT,
          DEFAULT_WIDTH,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuResolutionFilter:min-height:
   *
   * The minimum height of the video
   */
  g_object_class_install_property (gobject_class, PROP_MIN_HEIGHT,
      g_param_spec_uint ("min-height", "The minimum height",
          "The minimum height of the image",
          0, G_MAXINT,
          DEFAULT_HEIGHT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuResolutionFilter:max-height:
   *
   * The maximum height of the video
   */
  g_object_class_install_property (gobject_class, PROP_MAX_HEIGHT,
      g_param_spec_uint ("max-height", "The maximum height",
          "The maximum height of the image",
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
  priv->min_width = DEFAULT_WIDTH;
  priv->max_width = DEFAULT_WIDTH;
  priv->min_height = DEFAULT_HEIGHT;
  priv->max_height = DEFAULT_HEIGHT;
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
    case PROP_MIN_WIDTH:
      g_value_set_uint (value, priv->min_width);
      break;
    case PROP_MAX_WIDTH:
      g_value_set_uint (value, priv->max_width);
      break;
    case PROP_MIN_HEIGHT:
      g_value_set_uint (value, priv->min_height);
      break;
    case PROP_MAX_HEIGHT:
      g_value_set_uint (value, priv->max_height);
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
    case PROP_MIN_WIDTH:
      priv->min_width = g_value_get_uint (value);
      break;
    case PROP_MAX_WIDTH:
      priv->max_width = g_value_get_uint (value);
      break;
    case PROP_MIN_HEIGHT:
      priv->min_height = g_value_get_uint (value);
      break;
    case PROP_MAX_HEIGHT:
      priv->max_height = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_resolution_filter_dispose (GObject *object)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (object);
  FsuResolutionFilterPrivate *priv = self->priv;

  if (priv->caps)
    gst_caps_unref (priv->caps);

  G_OBJECT_CLASS (fsu_resolution_filter_parent_class)->dispose (object);
}

static void
fsu_resolution_filter_constructed (GObject *object)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (object);
  FsuResolutionFilterPrivate *priv = self->priv;

  if (G_OBJECT_CLASS (fsu_resolution_filter_parent_class)->constructed)
    G_OBJECT_CLASS (fsu_resolution_filter_parent_class)->constructed (object);

  if (priv->min_width < priv->max_width && priv->min_height < priv->max_height)
  {
    priv->caps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
            "width", GST_TYPE_INT_RANGE, priv->min_width, priv->max_width,
            "height", GST_TYPE_INT_RANGE, priv->min_height, priv->max_height,
            NULL),
        gst_structure_new ("video/x-raw-rgb",
            "width", GST_TYPE_INT_RANGE, priv->min_width, priv->max_width,
            "height", GST_TYPE_INT_RANGE, priv->min_height, priv->max_height,
            NULL),
        gst_structure_new ("video/x-raw-gray",
            "width", GST_TYPE_INT_RANGE, priv->min_width, priv->max_width,
            "height", GST_TYPE_INT_RANGE, priv->min_height, priv->max_height,
            NULL),
        NULL);
  }
  else
  {
    priv->caps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
            "width", G_TYPE_INT, priv->min_width,
            "height", G_TYPE_INT, priv->min_height,
            NULL),
        gst_structure_new ("video/x-raw-rgb",
            "width", G_TYPE_INT, priv->min_width,
            "height", G_TYPE_INT, priv->min_height,
            NULL),
        gst_structure_new ("video/x-raw-gray",
            "width", G_TYPE_INT, priv->min_width,
            "height", G_TYPE_INT, priv->min_height,
            NULL),
        NULL);
  }
}

/**
 * fsu_resolution_filter_new:
 * @width: The requested width of the video
 * @height: The requested height of the video
 *
 * Creates a new resolution filter.
 * This filter will force the video output to have a specific resolution. It
 * will add a videoscale element to the pipeline as well as a capsfilter.
 * If the source can use the new resolution, it will do it, otherwise, the
 * videoscale will scale the video to make sure it has the right resolution.
 *
 * Returns: A new #FsuResolutionFilter
 * See also: fsu_resolution_filter_new_range()
 */
FsuResolutionFilter *
fsu_resolution_filter_new (guint width,
    guint height)
{
  return fsu_resolution_filter_new_range (width, height, width, height);
}

/**
 * fsu_resolution_filter_new_range:
 * @min_width: The minimum width of the video
 * @min_height: The minimum height of the video
 * @max_width: The maximum width of the video
 * @max_height: The maximum height of the video
 *
 * Creates a new resolution filter.
 * This filter will force the video output to have a resolution in a specific
 * range. It will allow you to set a 'reasonable resolution' rather than a
 * 'specific resolution'.
 * It will add a videoscale element to the pipeline as well as a capsfilter.
 * If the source can use a resolution in that range, it will do it, otherwise,
 * the videoscale will scale the video to make sure it has the right resolution.
 *
 * Returns: A new #FsuResolutionFilter
 * See also: fsu_resolution_filter_new()
 */
FsuResolutionFilter *
fsu_resolution_filter_new_range (guint min_width,
    guint min_height,
    guint max_width,
    guint max_height)
{
  return g_object_new (FSU_TYPE_RESOLUTION_FILTER,
      "min-width", min_width,
      "min-height", min_height,
      "max-width", max_width,
      "max-height", max_height,
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
      g_object_set (capsfilter,
          "caps", priv->caps,
          NULL);
      gst_object_unref (capsfilter);
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
  return fsu_filter_revert_bin (bin, pad);
}

