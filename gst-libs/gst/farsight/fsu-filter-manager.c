/*
 * fsu-filter-manager.c - Source for FsuFilterManager
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


#include <gst/farsight/fsu-filter-manager.h>


G_DEFINE_TYPE (FsuFilterManager, fsu_filter_manager, G_TYPE_OBJECT);

static void fsu_filter_manager_constructed (GObject *object);
static void fsu_filter_manager_dispose (GObject *object);
static void fsu_filter_manager_finalize (GObject *object);

typedef enum {INSERT, REMOVE, REPLACE} ModificationAction;

typedef struct
{
  ModificationAction action;
  FsuFilterId *id;
  gint insert_position;
  FsuFilterId *replace_id;
} FilterModification;

struct _FsuFilterManagerPrivate
{
  gboolean dispose_has_run;
  GList *applied_filters;
  GList *filters;
  GstBin *applied_bin;
  GstPad *applied_pad;
  GstPad *out_pad;
  GQueue *modifications;
};

struct _FsuFilterId
{
  FsuFilter *filter;
  GstPad *in_pad;
  GstPad *out_pad;
};


static void
fsu_filter_manager_class_init (FsuFilterManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuFilterManagerPrivate));

  gobject_class->constructed = fsu_filter_manager_constructed;
  gobject_class->dispose = fsu_filter_manager_dispose;
  gobject_class->finalize = fsu_filter_manager_finalize;

}

static void
fsu_filter_manager_init (FsuFilterManager *self)
{
  FsuFilterManagerPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_FILTER_MANAGER,
          FsuFilterManagerPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->modifications = g_queue_new ();
}


static void
fsu_filter_manager_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
      G_OBJECT_CLASS (fsu_filter_manager_parent_class)->constructed;
  FsuFilterManager *self = FSU_FILTER_MANAGER (object);
  FsuFilterManagerPrivate *priv = self->priv;

  /* Make compiler happy */
  (void)priv;

  if (chain_up)
    chain_up (object);

}

static void
fsu_filter_manager_dispose (GObject *object)
{
  FsuFilterManager *self = (FsuFilterManager *)object;
  FsuFilterManagerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  G_OBJECT_CLASS (fsu_filter_manager_parent_class)->dispose (object);
}

static void
fsu_filter_manager_finalize (GObject *object)
{
  FsuFilterManager *self = (FsuFilterManager *)object;

  /* Make compiler happy */
  (void)self;

  G_OBJECT_CLASS (fsu_filter_manager_parent_class)->finalize (object);
}


FsuFilterManager *fsu_filter_manager_new (void)
{
  return g_object_new (FSU_TYPE_FILTER_MANAGER, NULL);
}

static void
pad_block_do_nothing (GstPad *pad,
    gboolean blocked,
    gpointer user_data)
{
  g_debug ("Pad unblocked");
}

static void
free_filter_id (FsuFilterId *id)
{
  g_object_unref (id->filter);
  g_slice_free (FsuFilterId, id);
}


static void
apply_modifs (GstPad *pad,
    gboolean blocked,
    gpointer user_data)
{
  FsuFilterManager *self = user_data;
  FsuFilterManagerPrivate *priv = self->priv;

  g_debug ("Pad blocked, now modifying pipeline with %d modifications",
      g_queue_get_length (priv->modifications));

  g_debug ("Currently : applied pad %p and output pad %p", priv->applied_pad,
      priv->out_pad);

  /* TODO: mutex */
  while (!g_queue_is_empty (priv->modifications))
  {
    FilterModification *modif = g_queue_pop_head (priv->modifications);
    GstPad *current_pad = NULL;
    GstPad *out_pad = NULL;
    GstPad *srcpad, *sinkpad;
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
          srcpad = current_pad = priv->out_pad;
          sinkpad = gst_pad_get_peer (srcpad);
        }
        else
        {
          sinkpad = current_pad = priv->applied_pad;
          srcpad = gst_pad_get_peer (sinkpad);
        }
      }
      else
      {
        if (GST_PAD_IS_SRC (priv->applied_pad))
        {
          sinkpad = current_id->in_pad;
          srcpad = current_pad = gst_pad_get_peer (sinkpad);
        }
        else
        {
          sinkpad = current_pad = current_id->out_pad;
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
        current_pad = to_remove->out_pad;
        if (GST_PAD_IS_SRC (priv->applied_pad))
        {
          srcpad = current_pad;
          sinkpad = gst_pad_get_peer (srcpad);
        }
        else
        {
          sinkpad = current_pad;
          srcpad = gst_pad_get_peer (sinkpad);
        }
      }
    }

    /* We may have nothing to do and no need to unlink if we want to REMOVE a
       filter which had failed to apply previously */
    if (insert || remove)
    {
      // unlink
      gst_pad_unlink (srcpad, sinkpad);

      if (remove)
        out_pad = fsu_filter_revert (to_remove->filter, priv->applied_bin,
            current_pad);
      else if (insert)
        out_pad = fsu_filter_apply (modif->id->filter, priv->applied_bin,
            current_pad);

      if (to_remove)
        to_remove->in_pad = to_remove->out_pad = NULL;

      if (out_pad)
      {
        /* If we want to replace, we need to apply after the revert */
        if (remove && insert)
        {
          GstPad *out_pad2 = NULL;
          out_pad2 = fsu_filter_apply (modif->id->filter,
              priv->applied_bin, out_pad);
          modif->id->in_pad = gst_pad_get_peer (out_pad);
          modif->id->out_pad = out_pad2;
          if (out_pad2)
            out_pad = out_pad2;
        }
        else if (insert)
        {
          modif->id->in_pad = gst_pad_get_peer (current_pad);
          modif->id->out_pad = out_pad;
        }

        if (GST_PAD_IS_SRC (priv->applied_pad))
          srcpad = out_pad;
        else
          sinkpad = out_pad;

        /* Update the out pad*/
        if (current_pad == priv->out_pad)
        {
          g_debug ("out pad changed from %p to %p", priv->out_pad, out_pad);
          priv->out_pad = out_pad;
        }
      }

      g_debug ("Applied filter on pad %p got %p", current_pad, out_pad);

      // Link
      /* TODO: handle unable to link */
      gst_pad_link (srcpad, sinkpad);
    }

    /* Synchronize our applied_filters list with the filters list */
    if (modif->action == REPLACE)
      insert_position = g_list_index (priv->applied_filters, to_remove);
    else if (modif->action == INSERT)
      insert_position = modif->insert_position;

    if (insert_position != -1)
      priv->applied_filters = g_list_insert (priv->applied_filters,
          modif->id, insert_position);
    if (to_remove)
    {
      priv->applied_filters = g_list_remove (priv->applied_filters, to_remove);
      free_filter_id (to_remove);
    }


    g_slice_free (FilterModification, modif);
  }


  gst_pad_set_blocked_async (pad, FALSE, pad_block_do_nothing, NULL);
}

static void
new_modification (FsuFilterManager *self,
    ModificationAction action,
    FsuFilterId *id,
    gint insert_position,
    FsuFilterId *replace_id)
{
  FsuFilterManagerPrivate *priv = self->priv;
  FilterModification *modif = g_slice_new0 (FilterModification);

  modif->action = action;
  modif->id = id;
  modif->insert_position = insert_position;
  modif->replace_id  = replace_id;

  g_debug ("New modification, %s filter %p (%d - %p). blocking pad %p",
      action == REMOVE?"Removing":action == REPLACE?"Replacing":"Inserting",
      id->filter, insert_position, replace_id ? replace_id->filter: NULL,
      GST_PAD_IS_SRC (priv->applied_pad)?priv->applied_pad:priv->out_pad);

  /* TODO: mutex */
  g_queue_push_tail (priv->modifications, modif);

  if (GST_PAD_IS_SRC (priv->applied_pad))
  {
    gst_pad_set_blocked_async (priv->applied_pad, TRUE, apply_modifs, self);
  }
  else
  {
    GstPad *src_pad = gst_pad_get_peer (priv->out_pad);
    gst_pad_set_blocked_async (src_pad, TRUE, apply_modifs, self);
    gst_object_unref (src_pad);
  }
}

FsuFilterId *
fsu_filter_manager_prepend_filter (FsuFilterManager *self,
    FsuFilter *filter)
{
  return fsu_filter_manager_insert_filter (self, filter, 0);
}

FsuFilterId *
fsu_filter_manager_append_filter (FsuFilterManager *self,
    FsuFilter *filter)
{
  return fsu_filter_manager_insert_filter (self, filter, -1);
}

FsuFilterId *
fsu_filter_manager_insert_filter_before (FsuFilterManager *self,
    FsuFilter *filter,
    FsuFilterId *before)
{
  gint index = g_list_index (self->priv->filters, before);

  if (index < 0)
    return NULL;

  return fsu_filter_manager_insert_filter (self, filter, index);
}

FsuFilterId *
fsu_filter_manager_insert_filter_after (FsuFilterManager *self,
    FsuFilter *filter,
    FsuFilterId *after)
{
  gint index = g_list_index (self->priv->filters, after);

  if (index < 0)
    return NULL;

  return fsu_filter_manager_insert_filter (self, filter, index + 1);
}

FsuFilterId *
fsu_filter_manager_replace_filter (FsuFilterManager *self,
    FsuFilter *filter,
    FsuFilterId *replace)
{
  FsuFilterManagerPrivate *priv = self->priv;
  gint index = g_list_index (priv->filters, replace);
  FsuFilterId *id = NULL;

  if (index < 0)
    return NULL;

  id = g_slice_new0 (FsuFilterId);
  id->filter = g_object_ref (filter);

  priv->filters = g_list_remove (priv->filters, replace);
  priv->filters = g_list_insert (priv->filters, id, index);

  if (priv->applied_bin)
    new_modification (self, REPLACE, id, 0, replace);
  else
    free_filter_id (replace);

  return id;
}

FsuFilterId *
fsu_filter_manager_insert_filter (FsuFilterManager *self,
    FsuFilter *filter,
    gint position)
{
  FsuFilterManagerPrivate *priv = self->priv;
  FsuFilterId *id = g_slice_new0 (FsuFilterId);

  if (position < 0 || position > g_list_length (priv->filters))
    position = g_list_length (priv->filters);

  id->filter = g_object_ref (filter);

  priv->filters = g_list_insert (priv->filters, id, position);

  if (priv->applied_bin)
    new_modification (self, INSERT, id, position, NULL);

  return id;
}

GList *
fsu_filter_manager_list_filters (FsuFilterManager *self)
{
  GList *ret = g_list_copy (self->priv->filters);
  return ret;
}

gboolean
fsu_filter_manager_remove_filter (FsuFilterManager *self,
    FsuFilterId *id)
{
  FsuFilterManagerPrivate *priv = self->priv;
  gint index = g_list_index (priv->filters, id);

  if (index < 0)
    return FALSE;

  priv->filters = g_list_remove (priv->filters, id);

  if (priv->applied_bin)
    new_modification (self, REMOVE, id, 0, NULL);
  else
    free_filter_id (id);

  return TRUE;
}

FsuFilter *
fsu_filter_manager_get_filter_by_id (FsuFilterManager *self, FsuFilterId *id)
{
  FsuFilterManagerPrivate *priv = self->priv;
  gint index = g_list_index (self->priv->filters, id);

  if (index < 0)
    return NULL;

  return g_object_ref (id->filter);
}


GstPad *
fsu_filter_manager_apply (FsuFilterManager *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;

  if (priv->applied_bin)
  {
    g_debug ("Can only apply the filters once");
    return NULL;
  }
  if (!bin || !pad)
  {
    g_debug ("Bin and/or pad NULL. Cannot apply");
    return NULL;
  }

  g_debug ("Applying on filter manager %p", self);

  priv->applied_pad = pad;

  if (GST_PAD_IS_SRC (pad))
    i = priv->filters;
  else
    i = g_list_last (priv->filters);

  while (i)
  {
    FsuFilterId *id = i->data;
    GstPad *out_pad = fsu_filter_apply (id->filter, bin, pad);

    if (out_pad)
    {
      id->in_pad = gst_pad_get_peer (pad);
      id->out_pad = out_pad;
      pad = out_pad;
    }
    else
    {
      id->in_pad = id->out_pad = NULL;
    }

    if (GST_PAD_IS_SRC (pad))
      i = i->next;
    else
      i = i->prev;
  }

  g_debug ("Applied");

  priv->out_pad = pad;
  priv->applied_bin = bin;
  priv->applied_filters = g_list_copy (self->priv->filters);

  return pad;
}

GstPad *
fsu_filter_manager_revert (FsuFilterManager *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;


  if (!bin || !pad)
  {
    g_debug ("Bin and/or pad NULL. Cannot revert");
    return NULL;
  }
  if (!priv->applied_bin)
  {
    g_debug ("Can not revert unapplied filters");
    return NULL;
  }
  if (priv->applied_bin != bin || priv->out_pad != pad)
  {
    g_debug ("Cannot revert filters from a different bin/pad");
    return NULL;
  }

  g_debug ("Reverting on filter manager %p", self);


  if (g_queue_is_empty (priv->modifications))
  {
    if (GST_PAD_IS_SRC (priv->applied_pad))
    {
      gst_pad_set_blocked_async (priv->applied_pad, FALSE,
          pad_block_do_nothing, NULL);
    }
    else
    {
      GstPad *src_pad = gst_pad_get_peer (priv->out_pad);
      gst_pad_set_blocked_async (src_pad, FALSE, pad_block_do_nothing, NULL);
      gst_object_unref (src_pad);
    }

    while (!g_queue_is_empty (priv->modifications))
    {
      FilterModification *modif = g_queue_pop_head (priv->modifications);

      if (modif->action == REMOVE)
        free_filter_id (modif->id);
      else if (modif->action == REPLACE)
        free_filter_id (modif->replace_id);

      g_slice_free (FilterModification, modif);
    }
  }

  if (GST_PAD_IS_SRC (pad))
    i = g_list_last (priv->applied_filters);
  else
    i = priv->applied_filters;

  while (i)
  {
    FsuFilterId *id = i->data;
    GstPad *out_pad = NULL;

    if (id->in_pad)
    {
      out_pad = fsu_filter_revert (id->filter, bin, pad);

      id->in_pad = id->out_pad = NULL;

      if (out_pad)
        pad = out_pad;
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

  g_debug ("Reverted");

  priv->applied_pad = NULL;
  priv->applied_bin = NULL;
  priv->out_pad = NULL;
  if (priv->applied_filters)
    g_list_free (priv->applied_filters);
  priv->applied_filters = NULL;

  return pad;
}

gboolean
fsu_filter_manager_handle_message (FsuFilterManager *self,
    GstMessage *message)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;
  gboolean drop = FALSE;

  for (i = priv->applied_filters; i && !drop; i = i->next)
  {
    FsuFilterId *id = i->data;

    /* Only handle messages to successfully applied filters */
    if (id->in_pad)
      drop = fsu_filter_handle_message (id->filter, message);
  }

  return drop;
}
