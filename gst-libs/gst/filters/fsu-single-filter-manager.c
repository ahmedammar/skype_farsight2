/*
 * fsu-single-filter-manager.c - Source for FsuSingleFilterManager
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


#include <gst/filters/fsu-single-filter-manager.h>


static void fsu_single_filter_manager_interface_init (
    FsuFilterManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (FsuSingleFilterManager,
    fsu_single_filter_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (FSU_TYPE_FILTER_MANAGER,
        fsu_single_filter_manager_interface_init));

static void fsu_single_filter_manager_dispose (GObject *object);
static void fsu_single_filter_manager_finalize (GObject *object);
static void fsu_single_filter_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void free_filter_id (FsuFilterId *id);
static void pad_block_do_nothing (GstPad *pad,
    gboolean blocked,
    gpointer user_data);

static GList *fsu_single_filter_manager_list_filters (
    FsuFilterManager *iface);
static FsuFilterId *fsu_single_filter_manager_insert_filter_before (
    FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *before);
static FsuFilterId *fsu_single_filter_manager_insert_filter_after (
    FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *after);
static FsuFilterId *fsu_single_filter_manager_replace_filter (
    FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *replace);
static FsuFilterId *fsu_single_filter_manager_insert_filter (
    FsuFilterManager *iface,
    FsuFilter *filter,
    gint position);
static FsuFilterId *fsu_single_filter_manager_insert_filter_unlock (
    FsuSingleFilterManager *self,
    FsuFilter *filter,
    gint position);
static gboolean fsu_single_filter_manager_remove_filter (
    FsuFilterManager *iface,
    FsuFilterId *id);
static FsuFilter *fsu_single_filter_manager_get_filter_by_id (
    FsuFilterManager *iface,
    FsuFilterId *id);
static GstPad *fsu_single_filter_manager_apply (
    FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_single_filter_manager_revert (
    FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad);
static gboolean fsu_single_filter_manager_handle_message (
    FsuFilterManager *iface,
    GstMessage *message);

typedef enum {INSERT, REMOVE, REPLACE} ModificationAction;

enum
{
  PROP_APPLIED = 1,
  PROP_APPLIED_BIN,
  PROP_APPLIED_PAD,
  PROP_OUT_PAD,
  LAST_PROPERTY
};

typedef struct
{
  ModificationAction action;
  FsuFilterId *id;
  gint insert_position;
  FsuFilterId *replace_id;
} FilterModification;

struct _FsuSingleFilterManagerPrivate
{
  GList *applied_filters;
  int list_id;
  GList *filters;
  GstBin *applied_bin;
  GstPad *applied_pad;
  GstPad *out_pad;
  GQueue *modifications;
  GMutex *mutex;
  gboolean applying;
};

struct _FsuFilterId
{
  FsuFilter *filter;
  GstPad *in_pad;
  GstPad *out_pad;
};


static void
fsu_single_filter_manager_class_init (FsuSingleFilterManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuSingleFilterManagerPrivate));

  gobject_class->get_property = fsu_single_filter_manager_get_property;
  gobject_class->dispose = fsu_single_filter_manager_dispose;
  gobject_class->finalize = fsu_single_filter_manager_finalize;

  /**
   * FsuSingleFilterManager:applied:
   *
   * Whether or not this filter manager has been applied on a pad or not
   */
  g_object_class_install_property (gobject_class, PROP_APPLIED,
      g_param_spec_boolean ("applied", "Applied status",
          "Whether the filter manager has been applied",
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuSingleFilterManager:applied-bin:
   *
   * The #GstBin this filter manager was applied on or #NULL if not applied yet
   */
  g_object_class_install_property (gobject_class, PROP_APPLIED_BIN,
      g_param_spec_object ("applied-bin", "Applied bin",
          "If applied, the bin it was applied on",
          GST_TYPE_BIN,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuSingleFilterManager:applied-pad:
   *
   * The #GstPad this filter manager was applied on or #NULL if not applied yet
   */
  g_object_class_install_property (gobject_class, PROP_APPLIED_PAD,
      g_param_spec_object ("applied-pad", "Applied pad",
          "If applied, the pad it was applied on",
          GST_TYPE_PAD,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuSingleFilterManager:out-pad:
   *
   * The output #GstPad at the end of this filter manager or #NULL if not
   * applied yet.
   *
   * See also: fsu_filter_manager_revert()
   */
  g_object_class_install_property (gobject_class, PROP_OUT_PAD,
      g_param_spec_object ("out-pad", "Output pad",
          "If applied, the output pad it applied",
          GST_TYPE_PAD,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_single_filter_manager_interface_init (FsuFilterManagerInterface *iface)
{
  iface->list_filters = fsu_single_filter_manager_list_filters;
  iface->insert_filter_before = fsu_single_filter_manager_insert_filter_before;
  iface->insert_filter_after = fsu_single_filter_manager_insert_filter_after;
  iface->replace_filter = fsu_single_filter_manager_replace_filter;
  iface->insert_filter = fsu_single_filter_manager_insert_filter;
  iface->remove_filter = fsu_single_filter_manager_remove_filter;
  iface->get_filter_by_id = fsu_single_filter_manager_get_filter_by_id;
  iface->apply = fsu_single_filter_manager_apply;
  iface->revert = fsu_single_filter_manager_revert;
  iface->handle_message = fsu_single_filter_manager_handle_message;

}

static void
fsu_single_filter_manager_init (FsuSingleFilterManager *self)
{
  FsuSingleFilterManagerPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SINGLE_FILTER_MANAGER,
          FsuSingleFilterManagerPrivate);

  self->priv = priv;
  priv->modifications = g_queue_new ();
  priv->mutex = g_mutex_new ();
}


static void
fsu_single_filter_manager_dispose (GObject *object)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (object);
  FsuSingleFilterManagerPrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);

  if (priv->applied_bin)
  {
    g_critical ("Disposing of FsuSingleFilterManager while applied.\n"
        "Make sure to properly revert any filter manager before\n"
        "disposing it. Unexpected behavior to be expected!");
    /* Force the revert to make sure the priv->modifications gets emptied and
     * the pad block is unblocked if it was.. */
    g_mutex_unlock (priv->mutex);
    fsu_filter_manager_revert (FSU_FILTER_MANAGER (self),
        priv->applied_bin, priv->out_pad);
    g_mutex_lock (priv->mutex);
  }

  g_assert (g_queue_is_empty (priv->modifications));

  if (priv->applied_filters)
    g_list_free (priv->applied_filters);
  priv->applied_filters = NULL;
  priv->list_id = -1;

  if (priv->filters)
  {
    g_list_foreach (priv->filters, (GFunc) free_filter_id, NULL);
    g_list_free (priv->filters);
  }
  priv->filters = NULL;

  if (priv->applied_bin)
    gst_object_unref (priv->applied_bin);
  if (priv->applied_pad)
    gst_object_unref (priv->applied_pad);
  if (priv->out_pad)
    gst_object_unref (priv->out_pad);

  g_mutex_unlock (priv->mutex);

  G_OBJECT_CLASS (fsu_single_filter_manager_parent_class)->dispose (object);
}

static void
fsu_single_filter_manager_finalize (GObject *object)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (object);

  g_queue_free (self->priv->modifications);
  g_mutex_free (self->priv->mutex);

  G_OBJECT_CLASS (fsu_single_filter_manager_parent_class)->finalize (object);
}

static void
fsu_single_filter_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (object);
  FsuSingleFilterManagerPrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);
  switch (property_id)
  {
    case PROP_APPLIED:
      g_value_set_boolean (value, priv->applied_bin ? TRUE : FALSE);
      break;
    case PROP_APPLIED_BIN:
      g_value_set_object (value, priv->applied_bin);
      break;
    case PROP_APPLIED_PAD:
      g_value_set_object (value, priv->applied_pad);
      break;
    case PROP_OUT_PAD:
      g_value_set_object (value, priv->out_pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
  g_mutex_unlock (priv->mutex);
}

/**
 * fsu_single_filter_manager_new:
 *
 * Creates a Single filter manager.
 * The Single filter manager is a filter manager that can only be applied once.
 *
 * Returns: A new #FsuFilterManager
 */
FsuFilterManager *fsu_single_filter_manager_new (void)
{
  return g_object_new (FSU_TYPE_SINGLE_FILTER_MANAGER, NULL);
}

static void
pad_block_do_nothing (GstPad *pad,
    gboolean blocked,
    gpointer user_data)
{
}

static void
destroy_pad_block_data (gpointer user_data)
{
  FsuSingleFilterManager *self = user_data;
  /* Unref the reference that was held by the pad_block*/
  g_object_unref (self);
}

static void
free_filter_id (FsuFilterId *id)
{
  g_object_unref (id->filter);
  if (id->in_pad)
    gst_object_unref (id->in_pad);
  if (id->out_pad)
    gst_object_unref (id->out_pad);

  g_slice_free (FsuFilterId, id);
}


static void
apply_modifs (GstPad *pad,
    gboolean blocked,
    gpointer user_data)
{
  FsuSingleFilterManager *self = user_data;
  FsuSingleFilterManagerPrivate *priv = self->priv;


  g_mutex_lock (priv->mutex);

  /* There could be a race where this thread and a _revert get called at the
   * same time. In which case the _revert will unblock the pad and unref our
   * reference on self. Once it releases the lock, we might get here, in which
   * case we need to check if the pad is blocked, and if it's not, then just
   * return and ignore we were ever called.
   */
  if (!gst_pad_is_blocked (pad))
  {
    g_mutex_unlock (priv->mutex);
    return;
  }

  while (!g_queue_is_empty (priv->modifications))
  {
    FilterModification *modif = g_queue_pop_head (priv->modifications);
    GstPad *current_pad = NULL;
    GstPad *out_pad = NULL;
    GstPad *srcpad = NULL;
    GstPad *sinkpad = NULL;
    GList *current_pos = NULL;
    FsuFilterId *current_id = NULL;
    gboolean remove = FALSE;
    FsuFilterId *to_remove = NULL;
    gboolean insert = FALSE;
    gint insert_position = -1;

    /* Set initial values representing the current action */
    if (modif->action == INSERT)
    {
      current_pos = g_list_nth (priv->applied_filters, modif->insert_position);
      insert = TRUE;
    }
    else if (modif->action == REMOVE)
    {
      to_remove = modif->id;
      /* Do not set remove to TRUE because we don't know yet if the filter
         to remove had failed or not */
    }
    else if (modif->action == REPLACE)
    {
      current_pos = g_list_find (priv->applied_filters, modif->replace_id);
      to_remove = modif->replace_id;
      insert = TRUE;
      /* Do not set remove to TRUE because we don't know yet if the filter
         to remove had failed or not */
    }

    /* Find the last successfully applied filter after the current one if
       the one at our position had failed to apply */
    for (; current_pos; current_pos = current_pos->next)
    {
      current_id = current_pos->data;
      if (current_id->in_pad)
        break;
    }

    /* If we want to insert or to replace a failed filter, then find the correct
       pads to unlink/link */
    if (modif->action == INSERT ||
        (modif->action == REPLACE && !to_remove->in_pad))
    {
      if (!current_pos)
      {
        if (GST_PAD_IS_SRC (priv->applied_pad))
        {
          current_pad = gst_object_ref (priv->out_pad);
          srcpad = gst_object_ref (current_pad);
          sinkpad = gst_pad_get_peer (srcpad);
        }
        else
        {
          current_pad = gst_object_ref (priv->applied_pad);
          sinkpad = gst_object_ref (current_pad);
          srcpad = gst_pad_get_peer (sinkpad);
        }
      }
      else
      {
        if (GST_PAD_IS_SRC (priv->applied_pad))
        {
          sinkpad = gst_object_ref (current_id->in_pad);
          current_pad = gst_pad_get_peer (sinkpad);
          srcpad = gst_object_ref (current_pad);
        }
        else
        {
          current_pad = gst_object_ref (current_id->out_pad);
          sinkpad = gst_object_ref (current_pad);
          srcpad = gst_pad_get_peer (sinkpad);
        }
      }
    }
    else
    {
      /* If the action is REMOVE or REPLACE and the filter to remove had been
         applied successfully */
      if (to_remove->in_pad)
      {
        remove = TRUE;
        current_pad = gst_object_ref (to_remove->out_pad);
        if (GST_PAD_IS_SRC (priv->applied_pad))
        {
          srcpad = gst_object_ref (current_pad);
          sinkpad = gst_pad_get_peer (srcpad);
        }
        else
        {
          sinkpad = gst_object_ref (current_pad);
          srcpad = gst_pad_get_peer (sinkpad);
        }
      }
    }

    /* We may have nothing to do and no need to unlink if we want to REMOVE a
       filter which had failed to apply previously */
    if (insert || remove)
    {
      out_pad = NULL;

      // unlink
      if (gst_pad_unlink (srcpad, sinkpad))
      {
        if (remove)
          out_pad = fsu_filter_revert (to_remove->filter, priv->applied_bin,
              current_pad);
        else if (insert)
          out_pad = fsu_filter_apply (modif->id->filter, priv->applied_bin,
              current_pad);
      }

      if (out_pad)
      {
        /* If we want to replace, we need to apply after the revert */
        if (remove && insert)
        {
          GstPad *out_pad2 = NULL;
          out_pad2 = fsu_filter_apply (modif->id->filter,
              priv->applied_bin, out_pad);
          if (out_pad2)
          {
            modif->id->in_pad = gst_pad_get_peer (out_pad);
            modif->id->out_pad = gst_object_ref (out_pad2);
            gst_object_unref (out_pad);
            out_pad = out_pad2;
          }
          else
          {
            if (modif->id->in_pad)
              gst_object_unref (modif->id->in_pad);
            if (modif->id->out_pad)
              gst_object_unref (modif->id->out_pad);
            modif->id->in_pad = modif->id->out_pad = NULL;
          }
        }
        else if (insert)
        {
          modif->id->in_pad = gst_pad_get_peer (current_pad);
          modif->id->out_pad = gst_object_ref (out_pad);
        }

        if (GST_PAD_IS_SRC (priv->applied_pad))
        {
          gst_object_unref (srcpad);
          srcpad = gst_object_ref (out_pad);
        }
        else
        {
          gst_object_unref (sinkpad);
          sinkpad = gst_object_ref (out_pad);
        }

        /* Update the out pad*/
        if (current_pad == priv->out_pad)
        {
          gst_object_unref (priv->out_pad);
          priv->out_pad = gst_object_ref (out_pad);
        }
        gst_object_unref (out_pad);
      }
      else if (insert)
      {
        if (modif->id->in_pad)
          gst_object_unref (modif->id->in_pad);
        if (modif->id->out_pad)
          gst_object_unref (modif->id->out_pad);
        modif->id->in_pad = modif->id->out_pad = NULL;
      }

      gst_object_unref (current_pad);

      // Link
      if (GST_PAD_LINK_SUCCESSFUL(gst_pad_link (srcpad, sinkpad)))
      {
        gst_object_unref (srcpad);
        gst_object_unref (sinkpad);
      } else {
        /* TODO: what should we do here???? */
      }
    }

    /* Synchronize our applied_filters list with the filters list */
    if (modif->action == REPLACE)
      insert_position = g_list_index (priv->applied_filters, to_remove);
    else if (modif->action == INSERT)
      insert_position = modif->insert_position;

    if (insert_position != -1)
    {
      priv->applied_filters = g_list_insert (priv->applied_filters,
          modif->id, insert_position);
      priv->list_id++;
    }
    if (to_remove)
    {
      priv->applied_filters = g_list_remove (priv->applied_filters, to_remove);
      free_filter_id (to_remove);
      priv->list_id++;
    }

    g_slice_free (FilterModification, modif);
  }
  g_mutex_unlock (priv->mutex);

  gst_pad_set_blocked_async (pad, FALSE, pad_block_do_nothing, NULL);
}

static void
new_modification (FsuSingleFilterManager *self,
    ModificationAction action,
    FsuFilterId *id,
    gint insert_position,
    FsuFilterId *replace_id)
{
  FsuSingleFilterManagerPrivate *priv = self->priv;
  FilterModification *modif = g_slice_new0 (FilterModification);

  modif->action = action;
  modif->id = id;
  modif->insert_position = insert_position;
  modif->replace_id = replace_id;

  g_queue_push_tail (priv->modifications, modif);

  /* Keep a reference to self for the pad block thread */
  g_object_ref (self);
  if (GST_PAD_IS_SRC (priv->applied_pad))
  {
    gst_pad_set_blocked_async_full (priv->applied_pad, TRUE, apply_modifs,
        self, destroy_pad_block_data);
  }
  else
  {
    GstPad *src_pad = gst_pad_get_peer (priv->out_pad);
    gst_pad_set_blocked_async_full (src_pad, TRUE, apply_modifs, self,
        destroy_pad_block_data);
    gst_object_unref (src_pad);
  }
}

static FsuFilterId *
fsu_single_filter_manager_insert_filter_before (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *before)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  gint index = -1;

  g_mutex_lock (priv->mutex);

  index = g_list_index (priv->filters, before);

  if (priv->applying || index < 0)
  {
    g_mutex_unlock (priv->mutex);
    return NULL;
  }

  return fsu_single_filter_manager_insert_filter_unlock (self, filter, index);
}

static FsuFilterId *
fsu_single_filter_manager_insert_filter_after (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *after)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  gint index = -1;

  g_mutex_lock (priv->mutex);
  index = g_list_index (priv->filters, after);

  if (priv->applying || index < 0)
  {
    g_mutex_unlock (priv->mutex);
    return NULL;
  }

  return fsu_single_filter_manager_insert_filter_unlock (self, filter,
      index + 1);
}

static FsuFilterId *
fsu_single_filter_manager_replace_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *replace)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  gint index = -1;
  FsuFilterId *id = NULL;

  g_mutex_lock (priv->mutex);

  index = g_list_index (priv->filters, replace);

  if (priv->applying || index < 0)
  {
    g_mutex_unlock (priv->mutex);
    return NULL;
  }

  id = g_slice_new0 (FsuFilterId);
  id->filter = g_object_ref (filter);

  priv->filters = g_list_remove (priv->filters, replace);
  priv->filters = g_list_insert (priv->filters, id, index);

  if (priv->applied_bin)
    new_modification (self, REPLACE, id, 0, replace);
  else
    free_filter_id (replace);

  g_mutex_unlock (priv->mutex);

  return id;
}

static FsuFilterId *
fsu_single_filter_manager_insert_filter_unlock (FsuSingleFilterManager *self,
    FsuFilter *filter,
    gint position)
{
  FsuSingleFilterManagerPrivate *priv = self->priv;
  FsuFilterId *id = g_slice_new0 (FsuFilterId);

  if (position < 0 || position > g_list_length (priv->filters))
    position = g_list_length (priv->filters);

  id->filter = g_object_ref (filter);

  priv->filters = g_list_insert (priv->filters, id, position);

  if (priv->applied_bin)
    new_modification (self, INSERT, id, position, NULL);

  g_mutex_unlock (priv->mutex);

  return id;
}

static FsuFilterId *
fsu_single_filter_manager_insert_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    gint position)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);

  if (priv->applying)
  {
    g_mutex_unlock (priv->mutex);
    return NULL;
  }

  return fsu_single_filter_manager_insert_filter_unlock (self, filter,
      position);
}

static GList *
fsu_single_filter_manager_list_filters (FsuFilterManager *iface)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  GList *ret = NULL;

  g_mutex_lock (priv->mutex);
  ret = g_list_copy (priv->filters);
  g_mutex_unlock (priv->mutex);

  return ret;
}

static gboolean
fsu_single_filter_manager_remove_filter (FsuFilterManager *iface,
    FsuFilterId *id)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  GList *find = NULL;

  g_mutex_lock (priv->mutex);
  find = g_list_find (priv->filters, id);

  if (priv->applying || !find)
  {
    g_mutex_unlock (priv->mutex);
    return FALSE;
  }

  priv->filters = g_list_remove (priv->filters, id);

  if (priv->applied_bin)
    new_modification (self, REMOVE, id, 0, NULL);
  else
    free_filter_id (id);

  g_mutex_unlock (priv->mutex);

  return TRUE;
}

static FsuFilter *
fsu_single_filter_manager_get_filter_by_id (FsuFilterManager *iface,
    FsuFilterId *id)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  FsuFilter *result = NULL;

  g_mutex_lock (priv->mutex);
  if (g_list_find (priv->filters, id))
    result = g_object_ref (id->filter);
  g_mutex_unlock (priv->mutex);

  return result;
}


static GstPad *
fsu_single_filter_manager_apply (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;

  if (!bin || !pad)
  {
    //g_debug ("Bin and/or pad NULL. Cannot apply");
    return NULL;
  }

  g_mutex_lock (priv->mutex);

  /* Check applied filters to make sure we're not in a revert */
  if (priv->applied_bin || priv->applied_filters)
  {
    g_mutex_unlock (priv->mutex);
    //g_debug ("Can only apply the filters once");
    return NULL;
  }

  /* Set the applied_bin/applied_pad here so a parallel append will work */
  priv->applied_pad = gst_object_ref (pad);
  priv->applied_bin = gst_object_ref (bin);
  priv->applying = TRUE;

  if (GST_PAD_IS_SRC (pad))
    i = priv->filters;
  else
    i = g_list_last (priv->filters);

  pad = gst_object_ref (pad);
  while (i)
  {
    FsuFilterId *id = i->data;
    GstPad *out_pad = NULL;

    g_mutex_unlock (priv->mutex);
    out_pad = fsu_filter_apply (id->filter, bin, pad);
    g_mutex_lock (priv->mutex);


    if (out_pad)
    {
      id->in_pad = gst_pad_get_peer (pad);
      id->out_pad = gst_object_ref (out_pad);
      gst_object_unref (pad);
      pad = out_pad;
    }
    else
    {
      if (id->in_pad)
        gst_object_unref (id->in_pad);
      if (id->out_pad)
        gst_object_unref (id->out_pad);
      id->in_pad = id->out_pad = NULL;
    }

    if (GST_PAD_IS_SRC (pad))
      i = i->next;
    else
      i = i->prev;
  }

  priv->out_pad = gst_object_ref (pad);
  priv->applied_filters = g_list_copy (self->priv->filters);
  priv->list_id++;
  priv->applying = FALSE;

  g_mutex_unlock (priv->mutex);

  return pad;
}

static GstPad *
fsu_single_filter_manager_revert (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;


  if (!bin || !pad)
  {
    //g_debug ("Bin and/or pad NULL. Cannot revert");
    return NULL;
  }

  g_mutex_lock (priv->mutex);
  if (!priv->applied_bin)
  {
    g_mutex_unlock (priv->mutex);
    //g_debug ("Can not revert unapplied filters");
    return NULL;
  }

  if (priv->applied_bin != bin || priv->out_pad != pad)
  {
    g_mutex_unlock (priv->mutex);
    //g_debug ("Cannot revert filters from a different bin/pad");
    return NULL;
  }

  /* Disable the pad block but don't free removed filter ids */
  if (!g_queue_is_empty (priv->modifications))
  {
    if (GST_PAD_IS_SRC (priv->applied_pad))
    {
      gst_pad_set_blocked_async (priv->applied_pad, FALSE,
          pad_block_do_nothing, NULL);
    }
    else
    {
      GstPad *src_pad = gst_pad_get_peer (priv->out_pad);
      /* The source might have been removed already */
      if (src_pad)
      {
        gst_pad_set_blocked_async (src_pad, FALSE, pad_block_do_nothing, NULL);
        gst_object_unref (src_pad);
      }
    }
  }

  if (GST_PAD_IS_SRC (pad))
    i = g_list_last (priv->applied_filters);
  else
    i = priv->applied_filters;

  /* Remove applied_bin as it's unnecessary and will prevent concurrent
     additions/removals from triggering the pad_block */
  gst_object_unref (priv->applied_bin);
  priv->applied_bin = NULL;

  pad = gst_object_ref (pad);

  while (i)
  {
    FsuFilterId *id = i->data;
    GstPad *out_pad = NULL;

    if (id->in_pad)
    {
      g_mutex_unlock (priv->mutex);
      out_pad = fsu_filter_revert (id->filter, bin, pad);
      g_mutex_lock (priv->mutex);

      gst_object_unref (id->in_pad);
      if (id->out_pad)
        gst_object_unref (id->out_pad);
      id->in_pad = id->out_pad = NULL;

      if (out_pad)
      {
        gst_object_unref (pad);
        pad = out_pad;
      }
    }

    if (GST_PAD_IS_SRC (pad))
      i = i->prev;
    else
      i = i->next;
  }

  if (priv->applied_pad != pad)
  {
    g_warning ("Reverting failed, result pad different from applied pad");
    //pad = priv->applied_pad;
  }

  gst_object_unref (priv->applied_pad);
  priv->applied_pad = NULL;
  gst_object_unref (priv->out_pad);
  priv->out_pad = NULL;

  /* Now free the removed filter ids */
  while (!g_queue_is_empty (priv->modifications))
  {
    FilterModification *modif = g_queue_pop_head (priv->modifications);

    if (modif->action == REMOVE)
      free_filter_id (modif->id);
    else if (modif->action == REPLACE)
      free_filter_id (modif->replace_id);

    g_slice_free (FilterModification, modif);
  }

  if (priv->applied_filters)
    g_list_free (priv->applied_filters);
  priv->applied_filters = NULL;
  priv->list_id++;

  g_mutex_unlock (priv->mutex);

  return pad;
}

static gboolean
fsu_single_filter_manager_handle_message (FsuFilterManager *iface,
    GstMessage *message)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;
  gboolean drop = FALSE;
  gint list_id = 0;

  g_mutex_lock (priv->mutex);
 retry:
  for (i = priv->applied_filters, list_id = priv->list_id;
       i && !drop && list_id == priv->list_id;
       i = i->next)
  {
    FsuFilterId *id = i->data;

    /* Only handle messages to successfully applied filters */
    if (id->in_pad) {
      g_mutex_unlock (priv->mutex);
      drop = fsu_filter_handle_message (id->filter, message);
      g_mutex_lock (priv->mutex);
    }
  }
  if (list_id != priv->sessions_id)
    goto retry;
  g_mutex_unlock (priv->mutex);

  return drop;
}
