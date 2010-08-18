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


/**
 * SECTION:fsu-filter
 * @short_description: Filters to apply on a pipeline
 *
 * This base class represents filters that can be applied on a pipeline or that
 * can be inserted in a #FsuFilterManager. Each filter does a specific task
 * and provides an easy to use API or properties/signals to control your
 * pipeline.
 * It also provides some helper functions to more easily add/revert elements
 * or bins to the pipeline.
 *
 * See also #FsuFilterManager
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/filters/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuFilter, fsu_filter, G_TYPE_OBJECT);

static void fsu_filter_dispose (GObject *object);
static void fsu_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
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
  GMutex *mutex;
};

static void
fsu_filter_class_init (FsuFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuFilterPrivate));

  gobject_class->get_property = fsu_filter_get_property;
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
  priv->mutex = g_mutex_new ();
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
fsu_filter_dispose (GObject *object)
{
  FsuFilter *self = FSU_FILTER (object);
  FsuFilterPrivate *priv = self->priv;

  if (priv->pads)
    g_hash_table_destroy (priv->pads);
  priv->pads = NULL;

  if (priv->mutex)
    g_mutex_free (priv->mutex);
  priv->mutex = NULL;

  G_OBJECT_CLASS (fsu_filter_parent_class)->dispose (object);
}


/**
 * fsu_filter_apply:
 * @self: The #FsuFilter
 * @bin: The #GstBin to apply the filter to
 * @pad: The #GstPad to apply the filter to
 *
 * This will apply the filter to a bin on a specific pad.
 * This will make the filter add elements to the @bin and link them with @pad.
 * It will return an output pad at the end of the inserted elements.
 * The filter can be applied on either a source pad or a sink pad, it
 * will still work the same.
 *
 * Returns: The new applied #GstPad to link with the rest of the pipeline or
 * #NULL in case of an error.
 * See also: fsu_filter_revert()
 */
GstPad *
fsu_filter_apply (FsuFilter *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);
  FsuFilterPrivate *priv = self->priv;
  GstPad *out_pad = NULL;

  g_assert (klass->apply);

  fsu_filter_lock (self);
  out_pad = klass->apply (self, bin, pad);
  fsu_filter_unlock (self);

  if (out_pad)
  {
    gst_object_ref (out_pad);
    fsu_filter_lock (self);
    g_hash_table_insert (priv->pads, out_pad, gst_pad_get_peer (pad));
    fsu_filter_unlock (self);
  }

  return out_pad;
}

/**
 * fsu_filter_revert:
 * @self: The #FsuFilter
 * @bin: The #GstBin to revert the filter from
 * @pad: The #GstPad to revert the filter from
 *
 * This will revert the filter from a bin on the specified pad.
 * This should remove any elements the filter previously added to the bin and
 * return the original pad it received in the fsu_filter_apply()
 * The pad to revert should be the output pad returned from fsu_filter_apply().
 * Note that the returned pad may not be the same as the input pad of
 * fsu_filter_apply() because if the pad before the filter's application
 * point was unlinked and relinked somewhere else, that might change (as in the
 * case of adding/removing a filter from a #FsuFilterManager in which this
 * filter resides)
 *
 * Returns: The original #GstPad from which the filter manager was applied
 * See also: fsu_filter_manager_apply()
 */
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

  fsu_filter_lock (self);
  in_pad = g_hash_table_lookup (priv->pads, pad);

  if (!in_pad)
  {
    fsu_filter_unlock (self);
    g_debug ("Can't revert filter %s (%p), never got applied on this pad",
        klass->name, self);
    return NULL;
  }
  expected = gst_pad_get_peer (in_pad);
  g_hash_table_remove (priv->pads, pad);

  out_pad = klass->revert (self, bin, pad);
  fsu_filter_unlock (self);

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

  return out_pad;
}


/**
 * fsu_filter_follow:
 * @self: The #FsuFilter
 * @pad: The #GstPad to follow the filter from
 *
 * This will not do anything but it will give you the expected output result
 * from fsu_filter_revert().
 * It basically will follow the filter from it's output pad all the way to the
 * other side of the elements that it added and will give you the input pad that
 * it should return if fsu_filter_revert() was called but without modifying
 * the pipeline.
 *
 * Returns: The original #GstPad from which the filter manager was applied
 * See also: fsu_filter_manager_revert()
 */
GstPad *
fsu_filter_follow (FsuFilter *self,
    GstPad *pad)
{
  FsuFilterPrivate *priv = self->priv;
  GstPad *in_pad = NULL;
  GstPad *out_pad = NULL;

  fsu_filter_lock (self);
  in_pad = g_hash_table_lookup (priv->pads, pad);
  if (in_pad)
    out_pad = gst_pad_get_peer (in_pad);
  fsu_filter_unlock (self);

  return out_pad;
}

/**
 * fsu_filter_handle_message:
 * @self: The #FsuFilter
 * @message: The message to handle
 *
 * Try to handle a message originally received on the #GstBus to the filter.
 *
 * Returns: #TRUE if the message has been handled and should be dropped,
 * #FALSE otherwise.
 */
gboolean
fsu_filter_handle_message (FsuFilter *self,
    GstMessage *message)
{
  FsuFilterClass *klass = FSU_FILTER_GET_CLASS (self);
  gboolean ret = FALSE;

  if (klass->handle_message)
  {
    fsu_filter_lock (self);
    ret = klass->handle_message (self, message);
    fsu_filter_unlock (self);
  }

  return ret;
}

/**
 * fsu_filter_lock:
 * @self: The #FsuFilter
 *
 * Locks the filter's mutex.
 * All virtual methods get called with the mutex locked so filters do not need
 * to bother with threading. However, if the filter needs to lock the mutex to
 * do something outside of the virtual methods (like in a g_object_set), then
 * it should lock the filter's mutex by calling this.
 *
 * See also: fsu_filter_unlock()
 */
void
fsu_filter_lock (FsuFilter *self)
{
    g_mutex_lock (self->priv->mutex);
}

/**
 * fsu_filter_unlock:
 * @self: The #FsuFilter
 *
 * Unlocks the filter's mutex.
 * All virtual methods get called with the mutex locked so filters do not need
 * to bother with threading. However, if the filter needs to lock the mutex to
 * do something outside of the virtual methods (like in a g_object_set), then
 * it should call fsu_filter_lock() then unlock it's mutex by calling this
 * method.
 *
 * See also: fsu_filter_lock()
 */
void
fsu_filter_unlock (FsuFilter *self)
{
    g_mutex_unlock (self->priv->mutex);
}
