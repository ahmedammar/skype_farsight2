/*
 * fsu-resolution-filter.c - Source for FsuResolutionFilter
 *
 * Copyright (C) 2010 Collabora Ltd.

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

#include "fsu-resolution-filter.h"
#include <gst/farsight/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuResolutionFilter, fsu_resolution_filter, FSU_TYPE_FILTER);

static void fsu_resolution_filter_dispose (GObject *object);
static GstPad *fsu_resolution_filter_apply (FsuFilter *filter,
    GstBin *bin, GstPad *pad);
static GstPad *fsu_resolution_filter_revert (FsuFilter *filter,
    GstBin *bin, GstPad *pad);


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

  gobject_class->dispose = fsu_resolution_filter_dispose;

  fsufilter_class->apply = fsu_resolution_filter_apply;
  fsufilter_class->revert = fsu_resolution_filter_revert;


}

static void
fsu_resolution_filter_init (FsuResolutionFilter *self)
{
  FsuResolutionFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_RESOLUTION_FILTER,
          FsuResolutionFilterPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->width = 320;
  priv->height = 240;
}


static void
fsu_resolution_filter_dispose (GObject *object)
{
  FsuResolutionFilter *self = (FsuResolutionFilter *)object;
  FsuResolutionFilterPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  G_OBJECT_CLASS (fsu_resolution_filter_parent_class)->dispose (object);
}

void
fsu_resolution_filter_set_resolution (FsuResolutionFilter *self,
    guint width, guint height)
{
  FsuResolutionFilterPrivate *priv = self->priv;
  GList *i;

  if (priv->caps != NULL)
    gst_object_unref (priv->caps);


  priv->caps = gst_caps_new_simple ("video/x-raw-yuv",
      "width", G_TYPE_UINT, width,
      "height", G_TYPE_UINT, height,
      NULL);

  for (i = priv->elements; i; i = i->next) {
    GstElement *element = i->data;
    g_object_set (element, "caps", priv->caps, NULL);
  }
}

FsuResolutionFilter *
fsu_resolution_filter_new (guint width, guint height)
{
  FsuResolutionFilter * obj = g_object_new (FSU_TYPE_RESOLUTION_FILTER, NULL);
  fsu_resolution_filter_set_resolution (obj, width, height);
  return obj;
}


static GstPad *
fsu_resolution_filter_apply (FsuFilter *filter, GstBin *bin, GstPad *pad)
{

  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (filter);
  FsuResolutionFilterPrivate *priv = self->priv;
  GstElement *capsfilter = NULL;
  GstPad *ret = NULL;

  ret = fsu_filter_add_standard_element (bin, pad, "capsfilter",
      &capsfilter, &priv->elements);

  if (capsfilter != NULL) {
    g_object_set (capsfilter,
        "caps", priv->caps,
        NULL);
  }

  return ret;
}

static GstPad *
fsu_resolution_filter_revert (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  FsuResolutionFilter *self = FSU_RESOLUTION_FILTER (filter);
  return fsu_filter_revert_standard_element (bin, pad, &self->priv->elements);
}

