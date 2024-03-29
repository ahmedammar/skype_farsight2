/*
 * fsu-framerate-filter.c - Source for FsuFramerateFilter
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


#include <gst/filters/fsu-framerate-filter.h>
#include <gst/filters/fsu-filter-helper.h>

G_DEFINE_TYPE (FsuFramerateFilter, fsu_framerate_filter, FSU_TYPE_FILTER);

static void fsu_framerate_filter_constructed (GObject *object);
static void fsu_framerate_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_framerate_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void fsu_framerate_filter_dispose (GObject *object);
static GstPad *fsu_framerate_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_framerate_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);

#define DEFAULT_FRAMERATE 30

/* properties */
enum
{
  PROP_FPS = 1,
  LAST_PROPERTY
};


struct _FsuFramerateFilterPrivate
{
  GList *elements;
  GstCaps *caps;
  guint fps;
};

static void
fsu_framerate_filter_class_init (FsuFramerateFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuFramerateFilterPrivate));

  gobject_class->constructed = fsu_framerate_filter_constructed;
  gobject_class->get_property = fsu_framerate_filter_get_property;
  gobject_class->set_property = fsu_framerate_filter_set_property;
  gobject_class->dispose = fsu_framerate_filter_dispose;

  fsufilter_class->apply = fsu_framerate_filter_apply;
  fsufilter_class->revert = fsu_framerate_filter_revert;
  fsufilter_class->name = "framerate";

  /**
   * FsuFramerateFilter:fps:
   *
   * The maximum framerate allowed
   */
  g_object_class_install_property (gobject_class, PROP_FPS,
      g_param_spec_uint ("fps", "Frames per second",
          "The framerate per second to set",
          0, G_MAXINT,
          DEFAULT_FRAMERATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_framerate_filter_init (FsuFramerateFilter *self)
{
  FsuFramerateFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_FRAMERATE_FILTER,
          FsuFramerateFilterPrivate);

  self->priv = priv;
  priv->fps = DEFAULT_FRAMERATE;
}


static void
fsu_framerate_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuFramerateFilter *self = FSU_FRAMERATE_FILTER (object);
  FsuFramerateFilterPrivate *priv = self->priv;


  switch (property_id)
  {
    case PROP_FPS:
      g_value_set_uint (value, priv->fps);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_framerate_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuFramerateFilter *self = FSU_FRAMERATE_FILTER (object);
  FsuFramerateFilterPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_FPS:
      {
        GList *i;

        priv->fps = g_value_get_uint (value);

        if (priv->caps)
          gst_caps_unref (priv->caps);

        priv->caps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
                "framerate", GST_TYPE_FRACTION, priv->fps, 1,
                NULL),
            gst_structure_new ("video/x-raw-rgb",
                "framerate", GST_TYPE_FRACTION, priv->fps, 1,
                NULL),
            gst_structure_new ("video/x-raw-gray",
                "framerate", GST_TYPE_FRACTION, priv->fps, 1,
                NULL),
            NULL);

        fsu_filter_lock (FSU_FILTER (self));
        for (i = priv->elements; i; i = i->next)
        {
          GstElement *element = i->data;
          g_object_set (element, "caps", priv->caps, NULL);
        }
        fsu_filter_unlock (FSU_FILTER (self));
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_framerate_filter_dispose (GObject *object)
{
  FsuFramerateFilter *self = FSU_FRAMERATE_FILTER (object);
  FsuFramerateFilterPrivate *priv = self->priv;
  GList *i;

  if(priv->caps)
    gst_caps_unref (priv->caps);

  for (i = priv->elements; i; i = i->next)
    gst_object_unref (i->data);
  g_list_free (priv->elements);
  priv->elements = NULL;

  G_OBJECT_CLASS (fsu_framerate_filter_parent_class)->dispose (object);
}


static void
fsu_framerate_filter_constructed (GObject *object)
{
  FsuFramerateFilter *self = FSU_FRAMERATE_FILTER (object);
  FsuFramerateFilterPrivate *priv = self->priv;

  if (G_OBJECT_CLASS (fsu_framerate_filter_parent_class)->constructed)
    G_OBJECT_CLASS (fsu_framerate_filter_parent_class)->constructed (object);

  priv->caps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
          "framerate", GST_TYPE_FRACTION, priv->fps, 1,
          NULL),
      gst_structure_new ("video/x-raw-rgb",
          "framerate", GST_TYPE_FRACTION, priv->fps, 1,
          NULL),
      gst_structure_new ("video/x-raw-gray",
          "framerate", GST_TYPE_FRACTION, priv->fps, 1,
          NULL),
      NULL);
}

/**
 * fsu_framerate_filter_new:
 * @fps: The maximum FPS allowed
 *
 * Creates a new framerate filter.
 * This filter will make sure that the stream does not output more frames than
 * the specified FPS. It will not generate duplicate frames, so this filter is
 * mainly to be used in a streaming pipeline.
 * It will basically add 'videomaxrate ! capsfilter' to the pipeline.
 *
 * Returns: A new #FsuFramerateFilter
 */
FsuFramerateFilter *
fsu_framerate_filter_new (guint fps)
{
  return g_object_new (FSU_TYPE_FRAMERATE_FILTER,
      "fps", fps,
      NULL);
}


static GstPad *
fsu_framerate_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{

  /*FsuFramerateFilter *self = FSU_FRAMERATE_FILTER (filter);
  FsuFramerateFilterPrivate *priv = self->priv;
  GstElement *capsfilter = NULL;
  GstElement *filter_bin = NULL;
  GstPad *out_pad = NULL;

  filter_bin = fsu_filter_add_element_by_description (bin, pad,
      "videorate ! capsfilter name=capsfilter", &out_pad);

  if (filter_bin)
  {
    capsfilter = gst_bin_get_by_name (GST_BIN (filter_bin), "capsfilter");

    if (capsfilter)
    {
      priv->elements = g_list_prepend (priv->elements, capsfilter);

      g_object_set (capsfilter,
          "caps", priv->caps,
          NULL);
    }

    gst_object_unref (filter_bin);
  }

  return out_pad;*/
  return pad;
}

static GstPad *
fsu_framerate_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuFramerateFilter *self = FSU_FRAMERATE_FILTER (filter);
  FsuFramerateFilterPrivate *priv = self->priv;
  GstElement *filter_bin = GST_ELEMENT (gst_pad_get_parent (pad));
  GstElement *capsfilter = NULL;
  GstPad *ret = NULL;

  capsfilter = gst_bin_get_by_name (GST_BIN (filter_bin), "capsfilter");
  if (g_list_find (priv->elements, capsfilter))
  {
    priv->elements = g_list_remove (priv->elements, capsfilter);
    gst_object_unref (capsfilter);
  }
  if (capsfilter)
    gst_object_unref (capsfilter);

  ret = fsu_filter_revert_bin (bin, pad);
  gst_object_unref (filter_bin);

  return ret;
}

