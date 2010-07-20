/*
 * fsu-filter.c - Source for FsuFilter
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


#include <gst/farsight/fsu-filter.h>


G_DEFINE_TYPE (FsuFilter, fsu_filter, G_TYPE_OBJECT);

static void fsu_filter_dispose (GObject *object);
static void fsu_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);


/* properties */
enum
{
  PROP_NAME = 1,
  LAST_PROPERTY
};

struct _FsuFilterPrivate
{
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

  g_object_class_install_property (gobject_class, PROP_NAME,
      g_param_spec_string ("name", "The name of the filter",
          "The name of the filter",
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_filter_init (FsuFilter *self)
{
  FsuFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_FILTER,
          FsuFilterPrivate);

  self->priv = priv;
  priv->pads = g_hash_table_new_full (NULL, NULL,
      gst_object_unref, gst_object_unref);
}


static void
fsu_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuFilter *self = FSU_FILTER (object);
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);

  switch (property_id)
  {
    case PROP_NAME:
      g_value_set_string (value, klass->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_filter_dispose (GObject *object)
{
  FsuFilter *self = FSU_FILTER (object);
  FsuFilterPrivate *priv = self->priv;

  if (priv->pads)
    g_hash_table_destroy (priv->pads);
  priv->pads = NULL;

  G_OBJECT_CLASS (fsu_filter_parent_class)->dispose (object);
}


GstPad *
fsu_filter_apply (FsuFilter *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);
  FsuFilterPrivate *priv = self->priv;
  GstPad *out_pad = NULL;

  g_assert (klass->apply);

  out_pad = klass->apply (self, bin, pad);

  if (out_pad)
  {
    gst_object_ref (out_pad);
    g_hash_table_insert (priv->pads, out_pad, gst_pad_get_peer (pad));
  }

  return out_pad;
}

GstPad *
fsu_filter_revert (FsuFilter *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);
  FsuFilterPrivate *priv = self->priv;
  GstPad *expected = NULL;
  GstPad *in_pad = NULL;
  GstPad *out_pad = NULL;

  g_assert (klass->revert);

  in_pad = GST_PAD (g_hash_table_lookup (priv->pads, pad));

  if (!in_pad)
  {
    g_debug ("Can't revert filter %s (%p), never got applied on this pad",
        klass->name, self);
    return NULL;
  }
  expected = gst_pad_get_peer (in_pad);

  out_pad = klass->revert (self, bin, pad);

  if (out_pad != expected)
  {
    g_warning ("Reverted pad on filter %s (%p) not as expected",
        klass->name, self);
    if (out_pad)
      gst_object_unref (out_pad);
    out_pad = expected;
  }
  else
  {
    gst_object_unref (expected);
  }

  g_hash_table_remove (priv->pads, pad);

  return out_pad;
}

GstPad *
fsu_filter_follow (FsuFilter *self,
    GstPad *pad)
{
  GstPad *in_pad = g_hash_table_lookup (self->priv->pads, pad);

  if (in_pad)
    return gst_pad_get_peer (in_pad);
  else
    return NULL;
}

gboolean
fsu_filter_handle_message (FsuFilter *self,
    GstMessage *message)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);

  if (klass->handle_message)
    return klass->handle_message (self, message);

  return FALSE;
}
