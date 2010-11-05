/*
 * fsu-bin-filter.c - Source for FsuBinFilter
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


#include <gst/filters/fsu-bin-filter.h>
#include <gst/filters/fsu-filter-helper.h>

G_DEFINE_TYPE (FsuBinFilter, fsu_bin_filter, FSU_TYPE_FILTER);

static void fsu_bin_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_bin_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void fsu_bin_filter_dispose (GObject *object);
static GstPad *fsu_bin_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_bin_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);


/* properties */
enum
{
  PROP_DESCRIPTION = 1,
  LAST_PROPERTY
};


struct _FsuBinFilterPrivate
{
  gchar *description;
};

static void
fsu_bin_filter_class_init (FsuBinFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuBinFilterPrivate));

  gobject_class->get_property = fsu_bin_filter_get_property;
  gobject_class->set_property = fsu_bin_filter_set_property;
  gobject_class->dispose = fsu_bin_filter_dispose;

  fsufilter_class->apply = fsu_bin_filter_apply;
  fsufilter_class->revert = fsu_bin_filter_revert;
  fsufilter_class->name = "bin";

  /**
   * FsuBinFilter:description:
   *
   * The bin description to be added to the pipeline when the filter is applied
   */
  g_object_class_install_property (gobject_class, PROP_DESCRIPTION,
      g_param_spec_string ("description", "bin description",
          "The bin description to add to the pipeline",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_bin_filter_init (FsuBinFilter *self)
{
  FsuBinFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_BIN_FILTER,
          FsuBinFilterPrivate);

  self->priv = priv;
  priv->description = NULL;
}


static void
fsu_bin_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuBinFilter *self = FSU_BIN_FILTER (object);
  FsuBinFilterPrivate *priv = self->priv;

  fsu_filter_lock (FSU_FILTER (self));
  switch (property_id)
  {
    case PROP_DESCRIPTION:
      g_value_set_string (value, priv->description);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  fsu_filter_unlock (FSU_FILTER (self));
}

static void
fsu_bin_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuBinFilter *self = FSU_BIN_FILTER (object);
  FsuBinFilterPrivate *priv = self->priv;

  fsu_filter_lock (FSU_FILTER (self));
  switch (property_id)
  {
    case PROP_DESCRIPTION:
      g_free (priv->description);
      priv->description = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  fsu_filter_unlock (FSU_FILTER (self));
}

static void
fsu_bin_filter_dispose (GObject *object)
{
  FsuBinFilter *self = FSU_BIN_FILTER (object);
  FsuBinFilterPrivate *priv = self->priv;

  if (priv->description)
    g_free (priv->description);
  priv->description = NULL;

  G_OBJECT_CLASS (fsu_bin_filter_parent_class)->dispose (object);
}

/**
 * fsu_bin_filter_new:
 * @description: The bin description to apply
 *
 * Creates a new bin filter.
 * The bin filter will insert a custom pipeline as defined by the bin
 * @description in the pipeline whenever the filter gets applied
 *
 * Returns: A new #FsuBinFilter
 */
FsuBinFilter *
fsu_bin_filter_new (const gchar *description)
{

  g_return_val_if_fail (description, NULL);

  return g_object_new (FSU_TYPE_BIN_FILTER,
      "description", description,
      NULL);
}


static GstPad *
fsu_bin_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{

  FsuBinFilter *self = FSU_BIN_FILTER (filter);
  FsuBinFilterPrivate *priv = self->priv;
  GstElement *filter_bin = NULL;
  GstPad *out_pad = NULL;

  filter_bin = fsu_filter_add_element_by_description (bin, pad,
      priv->description, &out_pad);

  if (filter_bin)
    gst_object_unref (filter_bin);

  return out_pad;
}

static GstPad *
fsu_bin_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  return fsu_filter_revert_bin (bin, pad);
}

