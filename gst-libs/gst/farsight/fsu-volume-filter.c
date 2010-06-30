/*
 * fsu-volume-filter.c - Source for FsuVolumeFilter
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

#include <gst/farsight/fsu-volume-filter.h>
#include <gst/farsight/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuVolumeFilter, fsu_volume_filter, FSU_TYPE_FILTER);

static void fsu_volume_filter_dispose (GObject *object);
static void fsu_volume_filter_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_volume_filter_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);
static GstPad *fsu_volume_filter_apply (FsuFilter *filter,
    GstBin *bin, GstPad *pad);
static GstPad *fsu_volume_filter_revert (FsuFilter *filter,
    GstBin *bin, GstPad *pad);


/* properties */
enum {
  PROP_VOLUME = 1,
  PROP_MUTE,
  LAST_PROPERTY
};

struct _FsuVolumeFilterPrivate
{
  gboolean dispose_has_run;

  gdouble volume;
  gboolean mute;
  /* a list of GstElement * */
  GList *elements;
};

static void
fsu_volume_filter_class_init (FsuVolumeFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuVolumeFilterPrivate));

  gobject_class->get_property = fsu_volume_filter_get_property;
  gobject_class->set_property = fsu_volume_filter_set_property;
  gobject_class->dispose = fsu_volume_filter_dispose;

  fsufilter_class->apply = fsu_volume_filter_apply;
  fsufilter_class->revert = fsu_volume_filter_revert;
  fsufilter_class->name = "volume";

  g_object_class_install_property (gobject_class, PROP_VOLUME,
      g_param_spec_double ("volume", "Volume",
          "The volume up to 10x amplificaton. 1.0 = 100%",
          0.0, 10.0,
          1.0,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MUTE,
      g_param_spec_boolean ("mute", "Mute volume",
          "Set to mute the sound",
          FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_volume_filter_init (FsuVolumeFilter *self)
{
  FsuVolumeFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_VOLUME_FILTER,
          FsuVolumeFilterPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->volume = 1.0;
}

static void
fsu_volume_filter_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuVolumeFilter *self = FSU_VOLUME_FILTER (object);
  FsuVolumeFilterPrivate *priv = self->priv;


  switch (property_id) {
    case PROP_VOLUME:
      g_value_set_double (value, priv->volume);
      break;
    case PROP_MUTE:
      g_value_set_boolean (value, priv->mute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_volume_filter_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuVolumeFilter *self = FSU_VOLUME_FILTER (object);
  FsuVolumeFilterPrivate *priv = self->priv;
  GList *i = NULL;

  switch (property_id) {
    case PROP_VOLUME:
      priv->volume = g_value_get_double (value);
      for (i = priv->elements; i; i = i->next) {
        GstElement *element = i->data;
        g_object_set (element, "volume", priv->volume, NULL);
      }
      break;
    case PROP_MUTE:
      priv->mute = g_value_get_boolean (value);
      for (i = priv->elements; i; i = i->next) {
        GstElement *element = i->data;
        g_object_set (element, "mute", priv->mute, NULL);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_volume_filter_dispose (GObject *object)
{
  FsuVolumeFilter *self = (FsuVolumeFilter *)object;
  FsuVolumeFilterPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  G_OBJECT_CLASS (fsu_volume_filter_parent_class)->dispose (object);
}


FsuVolumeFilter *
fsu_volume_filter_new (void)
{
  return g_object_new (FSU_TYPE_VOLUME_FILTER, NULL);
}

static GstPad *
fsu_volume_filter_apply (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  FsuVolumeFilter *self = FSU_VOLUME_FILTER (filter);
  GstElement *volume = NULL;
  GstPad *ret = NULL;

  ret = fsu_filter_add_standard_element (bin, pad, "volume",
      &volume, &self->priv->elements);

  if (volume != NULL) {
    g_object_set (volume,
        "volume", self->priv->volume,
        "mute", self->priv->mute, NULL);
  }

  return ret;
}

static GstPad *
fsu_volume_filter_revert (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  FsuVolumeFilter *self = FSU_VOLUME_FILTER (filter);
  return fsu_filter_revert_standard_element (bin, pad, &self->priv->elements);
}
