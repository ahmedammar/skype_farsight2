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


#include <gst/farsight/fsu-single-filter-manager.h>


static void fsu_single_filter_manager_interface_init (
    FsuFilterManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (FsuSingleFilterManager,
    fsu_single_filter_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (FSU_TYPE_FILTER_MANAGER,
        fsu_single_filter_manager_interface_init));

static void fsu_single_filter_manager_dispose (GObject *object);
static void fsu_single_filter_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_single_filter_manager_set_property (GObject *object,
    guint property_id,
    const GValue *value,
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
fsu_single_filter_manager_class_init (FsuSingleFilterManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuSingleFilterManagerPrivate));

  gobject_class->dispose = fsu_single_filter_manager_dispose;
  gobject_class->get_property = fsu_single_filter_manager_get_property;
  gobject_class->set_property = fsu_single_filter_manager_set_property;

  g_object_class_install_property (gobject_class, PROP_APPLIED,
      g_param_spec_boolean ("applied", "Applied status",
          "Whether the filter manager has been applied",
          FALSE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_APPLIED_BIN,
      g_param_spec_object ("applied-bin", "Applied bin",
          "If applied, the bin it was applied on",
          GST_TYPE_BIN,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_APPLIED_PAD,
      g_param_spec_object ("applied-pad", "Applied pad",
          "If applied, the pad it was applied on",
          GST_TYPE_PAD,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

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
  priv->dispose_has_run = FALSE;
  priv->modifications = g_queue_new ();
}


static void
fsu_single_filter_manager_dispose (GObject *object)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (object);
  FsuSingleFilterManagerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

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
  g_queue_free (priv->modifications);

  if (priv->applied_filters)
    g_list_free (priv->applied_filters);
  priv->applied_filters = NULL;
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

  G_OBJECT_CLASS (fsu_single_filter_manager_parent_class)->dispose (object);
}

static void
fsu_single_filter_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (object);
  FsuSingleFilterManagerPrivate *priv = self->priv;

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
}

static void
fsu_single_filter_manager_set_property (GObject *object,
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

FsuFilterManager *fsu_single_filter_manager_new (void)
{
  return g_object_new (FSU_TYPE_SINGLE_FILTER_MANAGER, NULL);
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
      // unlink
      gst_pad_unlink (srcpad, sinkpad);

      if (remove)
        out_pad = fsu_filter_revert (to_remove->filter, priv->applied_bin,
            current_pad);
      else if (insert)
        out_pad = fsu_filter_apply (modif->id->filter, priv->applied_bin,
            current_pad);

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
          g_debug ("out pad changed from %p to %p", priv->out_pad, out_pad);
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

      g_debug ("Applied filter on pad %p got %p", current_pad, out_pad);
      gst_object_unref (current_pad);

      // Link
      /* TODO: handle unable to link */
      gst_pad_link (srcpad, sinkpad);
      gst_object_unref (srcpad);
      gst_object_unref (sinkpad);
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

static FsuFilterId *
fsu_single_filter_manager_insert_filter_before (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *before)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  gint index = g_list_index (self->priv->filters, before);

  if (index < 0)
    return NULL;

  return fsu_filter_manager_insert_filter (iface, filter, index);
}

static FsuFilterId *
fsu_single_filter_manager_insert_filter_after (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *after)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  gint index = g_list_index (self->priv->filters, after);

  if (index < 0)
    return NULL;

  return fsu_filter_manager_insert_filter (iface, filter, index + 1);
}

static FsuFilterId *
fsu_single_filter_manager_replace_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *replace)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
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

static FsuFilterId *
fsu_single_filter_manager_insert_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    gint position)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
  FsuFilterId *id = g_slice_new0 (FsuFilterId);

  if (position < 0 || position > g_list_length (priv->filters))
    position = g_list_length (priv->filters);

  id->filter = g_object_ref (filter);

  priv->filters = g_list_insert (priv->filters, id, position);

  if (priv->applied_bin)
    new_modification (self, INSERT, id, position, NULL);

  return id;
}

static GList *
fsu_single_filter_manager_list_filters (FsuFilterManager *iface)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  return g_list_copy (self->priv->filters);
}

static gboolean
fsu_single_filter_manager_remove_filter (FsuFilterManager *iface,
    FsuFilterId *id)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
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

static FsuFilter *
fsu_single_filter_manager_get_filter_by_id (FsuFilterManager *iface,
    FsuFilterId *id)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  gint index = g_list_index (self->priv->filters, id);

  if (index < 0)
    return NULL;

  return g_object_ref (id->filter);
}


static GstPad *
fsu_single_filter_manager_apply (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  FsuSingleFilterManager *self = FSU_SINGLE_FILTER_MANAGER (iface);
  FsuSingleFilterManagerPrivate *priv = self->priv;
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

  priv->applied_pad = gst_object_ref (pad);

  if (GST_PAD_IS_SRC (pad))
    i = priv->filters;
  else
    i = g_list_last (priv->filters);

  pad = gst_object_ref (pad);
  while (i)
  {
    FsuFilterId *id = i->data;
    GstPad *out_pad = fsu_filter_apply (id->filter, bin, pad);

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

  g_debug ("Applied");

  priv->out_pad = gst_object_ref (pad);
  priv->applied_bin = gst_object_ref (bin);
  priv->applied_filters = g_list_copy (self->priv->filters);

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

  pad = gst_object_ref (pad);

  while (i)
  {
    FsuFilterId *id = i->data;
    GstPad *out_pad = NULL;

    if (id->in_pad)
    {
      out_pad = fsu_filter_revert (id->filter, bin, pad);

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

  g_debug ("Reverted");

  gst_object_unref (priv->applied_pad);
  priv->applied_pad = NULL;
  gst_object_unref (priv->applied_bin);
  priv->applied_bin = NULL;
  gst_object_unref (priv->out_pad);
  priv->out_pad = NULL;

  if (priv->applied_filters)
    g_list_free (priv->applied_filters);
  priv->applied_filters = NULL;

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

  for (i = priv->applied_filters; i && !drop; i = i->next)
  {
    FsuFilterId *id = i->data;

    /* Only handle messages to successfully applied filters */
    if (id->in_pad)
      drop = fsu_filter_handle_message (id->filter, message);
  }

  return drop;
}
