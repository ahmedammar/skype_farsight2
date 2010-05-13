/*
 * fsu-filter.c - Source for FsuFilter
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

#include "fsu-filter.h"


G_DEFINE_TYPE (FsuFilter, fsu_filter, G_TYPE_OBJECT);

static void fsu_filter_dispose (GObject *object);
static void fsu_filter_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_filter_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);


/* properties */
enum {
  PROP_CAN_FAIL = 1,
  LAST_PROPERTY
};

struct _FsuFilterPrivate
{
  gboolean dispose_has_run;
  gboolean can_fail;
  GHashTable *pads;
};

static void
fsu_filter_class_init (FsuFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuFilterPrivate));

  gobject_class->get_property = fsu_filter_get_property;
  gobject_class->set_property = fsu_filter_set_property;
  gobject_class->dispose = fsu_filter_dispose;

  g_object_class_install_property (gobject_class, PROP_CAN_FAIL,
      g_param_spec_boolean ("can-fail", "whether the filter can fail",
          "Set to TRUE if the filter can fail applying",
          FALSE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_filter_init (FsuFilter *self)
{
  FsuFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_FILTER,
          FsuFilterPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->pads = g_hash_table_new_full (NULL, NULL,
      gst_object_unref, gst_object_unref);
}


static void
fsu_filter_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuFilter *self = FSU_FILTER (object);
  FsuFilterPrivate *priv = self->priv;


  switch (property_id) {
    case PROP_CAN_FAIL:
      g_value_set_boolean (value, priv->can_fail);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_filter_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuFilter *self = FSU_FILTER (object);
  FsuFilterPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CAN_FAIL:
      priv->can_fail = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_filter_dispose (GObject *object)
{
  FsuFilter *self = (FsuFilter *)object;
  FsuFilterPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->pads != NULL)
    g_hash_table_destroy (priv->pads);
  priv->pads = NULL;

  G_OBJECT_CLASS (fsu_filter_parent_class)->dispose (object);
}


GstPad *
fsu_filter_apply (FsuFilter *self, GstBin *bin, GstPad *pad)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);
  FsuFilterPrivate *priv = self->priv;
  GstPad *(*func) (FsuFilter *self, GstBin *bin, GstPad *pad) = klass->apply;
  GstPad *out_pad = NULL;

  g_assert (func != NULL);

  g_debug ("Applying on filter %p : %p", self, pad);
  out_pad = func (self, bin, pad);
  g_debug ("Applied filter %p : %p", self, out_pad);

  if (out_pad != NULL) {
    gst_object_ref (pad);
    gst_object_ref (out_pad);
    g_hash_table_insert (priv->pads, out_pad, pad);
  }

  return out_pad;
}

GstPad *
fsu_filter_revert (FsuFilter *self, GstBin *bin, GstPad *pad)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);
  FsuFilterPrivate *priv = self->priv;
  GstPad *(*func) (FsuFilter *self, GstBin *bin, GstPad *pad) = klass->revert;
  GstPad *expected = NULL;
  GstPad *out_pad = NULL;

  g_assert (func != NULL);

  expected = GST_PAD (g_hash_table_lookup (priv->pads, pad));

  if (expected == NULL) {
    g_debug ("Can't revert, never got applied on this pad");
    return NULL;
  }

  g_debug ("Reverting on filter %p : %p", self, pad);
  out_pad = func (self, bin, pad);
  g_debug ("Reverted filter %p : %p", self, out_pad);

  if (out_pad != expected) {
    g_warning ("Reverted pad not as expected");
    if (out_pad != NULL)
      gst_object_unref (out_pad);
    gst_object_ref (expected);
  }

  g_hash_table_remove (priv->pads, pad);

  return expected;
}

gboolean
fsu_filter_update_link (FsuFilter *self, GstPad *pad,
    GstPad *old_pad, GstPad *new_pad)
{
  FsuFilterPrivate *priv = self->priv;
  GstPad *expected = GST_PAD (g_hash_table_lookup (priv->pads, pad));

  if (expected == old_pad) {
    gst_object_ref (new_pad);
    gst_object_ref (pad);
    g_hash_table_replace (priv->pads, pad, new_pad);
    return TRUE;
  }
  return FALSE;
}

GstPad *
fsu_filter_follow (FsuFilter *self, GstPad *pad)
{
  return GST_PAD (g_hash_table_lookup (self->priv->pads, pad));
}
