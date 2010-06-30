/*
 * fsu-effectv-filter.c - Source for FsuEffectvFilter
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

#include <gst/farsight/fsu-effectv-filter.h>
#include <gst/farsight/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuEffectvFilter, fsu_effectv_filter, FSU_TYPE_FILTER);

static void fsu_effectv_filter_dispose (GObject *object);
static void fsu_effectv_filter_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_effectv_filter_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);
static GstPad *fsu_effectv_filter_apply (FsuFilter *filter,
    GstBin *bin, GstPad *pad);
static GstPad *fsu_effectv_filter_revert (FsuFilter *filter,
    GstBin *bin, GstPad *pad);


/* properties */
enum {
  PROP_EFFECT = 1,
  LAST_PROPERTY
};



struct _FsuEffectvFilterPrivate
{
  gchar *effect;
};

static void
fsu_effectv_filter_class_init (FsuEffectvFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuEffectvFilterPrivate));

  gobject_class->get_property = fsu_effectv_filter_get_property;
  gobject_class->set_property = fsu_effectv_filter_set_property;
  gobject_class->dispose = fsu_effectv_filter_dispose;

  fsufilter_class->apply = fsu_effectv_filter_apply;
  fsufilter_class->revert = fsu_effectv_filter_revert;
  fsufilter_class->name = "effectv";

  g_object_class_install_property (gobject_class, PROP_EFFECT,
      g_param_spec_string ("effect", "The effect to use",
          "The name of the effectv element to use",
          "vertigotv",
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_effectv_filter_init (FsuEffectvFilter *self)
{
  FsuEffectvFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_EFFECTV_FILTER,
          FsuEffectvFilterPrivate);

  self->priv = priv;
}

static void
fsu_effectv_filter_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuEffectvFilter *self = FSU_EFFECTV_FILTER (object);
  FsuEffectvFilterPrivate *priv = self->priv;


  switch (property_id) {
    case PROP_EFFECT:
      g_value_set_string (value, priv->effect);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_effectv_filter_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuEffectvFilter *self = FSU_EFFECTV_FILTER (object);
  FsuEffectvFilterPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_EFFECT:
      priv->effect = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_effectv_filter_dispose (GObject *object)
{
  FsuEffectvFilter *self = (FsuEffectvFilter *)object;
  FsuEffectvFilterPrivate *priv = self->priv;

  g_free (priv->effect);
  priv->effect = NULL;

  G_OBJECT_CLASS (fsu_effectv_filter_parent_class)->dispose (object);
}


FsuEffectvFilter *
fsu_effectv_filter_new (const gchar *effect)
{
  return g_object_new (FSU_TYPE_EFFECTV_FILTER,
      "effect", effect,
      NULL);
}


static GstPad *
fsu_effectv_filter_apply (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  FsuEffectvFilter *self = FSU_EFFECTV_FILTER (filter);
  return fsu_filter_add_standard_element (bin, pad, self->priv->effect,
      NULL, NULL);
}

static GstPad *
fsu_effectv_filter_revert (FsuFilter *filter, GstBin *bin, GstPad *pad)
{
  return fsu_filter_revert_standard_element (bin, pad, NULL);
}

