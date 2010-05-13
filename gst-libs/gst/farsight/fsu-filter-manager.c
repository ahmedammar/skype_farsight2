/*
 * fsu-filter-manager.c - Source for FsuFilterManager
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

#include "fsu-filter-manager.h"


G_DEFINE_TYPE (FsuFilterManager, fsu_filter_manager, G_TYPE_OBJECT);

static void fsu_filter_manager_constructed (GObject *object);
static void fsu_filter_manager_dispose (GObject *object);
static void fsu_filter_manager_finalize (GObject *object);

typedef enum {INSERT, REMOVE} ModificationAction;

typedef struct {
  ModificationAction action;
  FsuFilter *filter;
  gint position;
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

  if (chain_up != NULL)
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
pad_block_do_nothing (GstPad *pad, gboolean blocked, gpointer user_data)
{
  g_debug ("Pad unblocked");
}

/*
 * Two use cases :
 * * fitler manager applied on a SRC pad :
 * applied_pad                                             out_pad
 *    0                 1                  2                  3
 *   [ ]-> [ filter1 ] [ ] -> [ filter2 ] [ ] -> [ filter3 ] [ ] ->
 *
 * REMOVE filter2 means we have position 1 and start at out_pad with position 3
 * then we follow once to pad 2, and again to pad 1 for the position 1.
 * This means the pad's parent is filter1 not filter2, and we need to revert from
 * sink to source so we need to actually stop at pad 2 in order to revert filter2
 * For INSERTING a filter between filter1 and filter2, we have position 1. We
 * start at pad 3 and follow twice to pad 1, which we can apply source to sink to
 * get the desired effect.
 *
 * to pad 2
 *
 * * filter manager applied on a SINK pad :
 *    out_pad                                                 applied_pad
 *       0                  1                  2                  3
 *   -> [ ] [ filter1 ] -> [ ] [ filter2 ] -> [ ] [ filter3 ] -> [ ]
 *
 * REMOVE filter2 means we have position 1 and start at out_pad with position 0
 * then we follow once to pad 1. Since we revert from source to sink, the revert
 * will work.
 * For INSERTING a filter between filter1 and filter2, we have position 1. We
 * start at pad 0 and follow once to pad 1, which we can apply sink to source to
 * get the desired effect.
 */

static void
apply_modifs (GstPad *pad, gboolean blocked, gpointer user_data)
{
  FsuFilterManager *self = user_data;
  FsuFilterManagerPrivate *priv = self->priv;

  g_debug ("Pad blocked, now modifying pipeline with %d modifications",
      g_queue_get_length (priv->modifications));

  g_debug ("Currently : applied pad %p and output pad %p", priv->applied_pad, priv->out_pad);

  /* TODO: mutex */
  while (g_queue_is_empty (priv->modifications) == FALSE) {
    FilterModification *modif = g_queue_pop_head (priv->modifications);
    GstPad *out_pad = NULL;
    FsuFilter *current_filter = NULL;
    GstPad *current_pad = priv->out_pad;
    FsuFilter *previous_filter = NULL;
    GstPad *previous_pad = NULL;
    GstPad *srcpad, *sinkpad;
    gint current_position = 0;
    gint low, high;
    gint target_position = modif->position;

    if (GST_PAD_IS_SRC (priv->applied_pad) == TRUE) {
      low = 1;
      high = g_list_length (priv->applied_filters);
      current_position = high;
    } else {
      low = 0;
      high = g_list_length (priv->applied_filters) - 1;
      current_position = low;
    }

    /* If we want to remove a filter where the apply is source to sink
       then we need to find the output pad of the filter in order to revert it.
       For sink to source, we get the sink pad already of the filter since the
       revert will go source to sink. See function header for explanation */
    if (GST_PAD_IS_SRC (priv->applied_pad) == TRUE && modif->action == REMOVE)
      target_position++;

    g_debug ("trying to find proper pad. %s %p at %d (%d <= %d <= %d)",
        modif->action == REMOVE? "Removing":"Inserting", modif->filter, modif->position,
        low, target_position, high);

    while (current_position >= low &&
        current_position <= high &&
        target_position != current_position) {

      /* In a source to sink apply, the current position of the pad is shifted
         by one from the position of the filter in the list */
      current_filter = g_list_nth_data (priv->applied_filters,
          GST_PAD_IS_SRC (priv->applied_pad) ?
          current_position -1 : current_position);

      if (current_filter != NULL)
        out_pad = fsu_filter_follow (current_filter, current_pad);

      g_debug ("Followed filter %p from pos %d got %p->%p",
          current_filter, current_position, current_pad, out_pad);

      if (out_pad != NULL) {
        previous_pad = current_pad;
        current_pad = out_pad;
      }

      if (GST_PAD_IS_SRC (priv->applied_pad) == TRUE)
        current_position--;
      else
        current_position++;
      g_debug ("Next position to test : %d", current_position);
    }

    if (target_position != current_position)
      g_warning ("Unable to locate position of filter to modify");
    g_assert (target_position == current_position);

    previous_filter = current_filter;

    /* In a source to sink apply, the current position of the pad is shifted
       by one from the position of the filter in the list */
    current_filter = g_list_nth_data (priv->applied_filters,
        GST_PAD_IS_SRC (priv->applied_pad) ?
        current_position -1 : current_position);

    if (modif->action == REMOVE && current_filter != modif->filter)
      g_warning ("Unable to locate the proper filter for removal");
    g_assert (modif->action != REMOVE || current_filter == modif->filter);

    if (GST_PAD_IS_SRC (priv->applied_pad) == TRUE) {
      srcpad = current_pad;
      sinkpad = gst_pad_get_peer (srcpad);
    } else {
      sinkpad = current_pad;
      srcpad = gst_pad_get_peer (sinkpad);
    }

    g_debug ("Unlinking : %d", gst_pad_unlink (srcpad, sinkpad));

    /* TODO unref pads*/
    g_debug ("%s new filter %p at position %d", modif->action == INSERT?
        "Inserting":"Removing", modif->filter, current_position);

    if (modif->action == INSERT)
      out_pad = fsu_filter_apply (modif->filter, priv->applied_bin, current_pad);
    else
      out_pad = fsu_filter_revert (modif->filter, priv->applied_bin, current_pad);
    if (out_pad != NULL) {
      if (GST_PAD_IS_SRC (priv->applied_pad) == TRUE)
        srcpad = out_pad;
      else
        sinkpad = out_pad;

      if (current_pad == priv->out_pad) {
        g_debug ("out pad changed from %p to %p", priv->out_pad, out_pad);
        priv->out_pad = out_pad;
      }
      if (previous_filter != NULL && previous_pad != NULL) {
        g_debug ("Updating previous filter %p for %p from %p to %p",
            previous_filter, previous_pad, current_pad, out_pad);
        fsu_filter_update_link (previous_filter, previous_pad,
            current_pad, out_pad);
      }
    }
    g_debug ("%s filter on pad %p got %p", modif->action == INSERT?
        "Applied":"Reverted", srcpad, out_pad);

    g_debug ("Linking : %d", gst_pad_link (srcpad, sinkpad));

    if (modif->action == INSERT) {
      priv->applied_filters = g_list_insert (priv->applied_filters,
          modif->filter, current_position);
    } else {
      GList *link = NULL;

      g_object_unref (modif->filter);
      link = g_list_nth (priv->applied_filters, modif->position);
      g_assert (link != NULL);
      g_assert (link->data == modif->filter);
      priv->applied_filters = g_list_delete_link (priv->applied_filters, link);
    }

    g_slice_free (FilterModification, modif);
  }


  gst_pad_set_blocked_async (pad, FALSE, pad_block_do_nothing, NULL);
}

static void
new_modification (FsuFilterManager *self, ModificationAction action,
    FsuFilter *filter, gint position)
{
  FsuFilterManagerPrivate *priv = self->priv;
  FilterModification *modif = g_slice_new0 (FilterModification);

  modif->action = action;
  modif->filter = filter;
  modif->position = position;

  g_debug ("New modification, %s filter %p at position %d. blocking pad %p",
      action == REMOVE?"Removing":"Inserting", filter, position,
      GST_PAD_IS_SRC (priv->applied_pad)?priv->applied_pad:priv->out_pad);

  /* TODO: mutex */
  g_queue_push_tail (priv->modifications, modif);

  if (GST_PAD_IS_SRC (priv->applied_pad) == TRUE) {
    gst_pad_set_blocked_async (priv->applied_pad, TRUE, apply_modifs, self);
  } else {
    GstPad *src_pad = gst_pad_get_peer (priv->out_pad);
    gst_pad_set_blocked_async (src_pad, TRUE, apply_modifs, self);
    gst_object_unref (src_pad);
  }
}

gboolean
fsu_filter_manager_insert_filter (FsuFilterManager *self,
    FsuFilter *filter, gint position)
{
  FsuFilterManagerPrivate *priv = self->priv;

  g_object_ref (filter);

  if (position < 0 || position > g_list_length (priv->filters))
    position = g_list_length (priv->filters);

  priv->filters = g_list_insert (priv->filters, filter, (gint) position);

  if (priv->applied_bin != NULL)
    new_modification (self, INSERT, filter, position);

  return TRUE;
}

GList *
fsu_filter_manager_list_filters (FsuFilterManager *self)
{
  GList *ret = g_list_copy (self->priv->filters);
  g_list_foreach (ret, (GFunc) g_object_ref, NULL);
  return ret;
}

gboolean
fsu_filter_manager_remove_filter (FsuFilterManager *self,
    FsuFilter *filter, guint position)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *link = NULL;

  g_object_unref (filter);

  if (position < 0)
    position = g_list_index (priv->filters, filter);

  link = g_list_nth (priv->filters, position);

  if (link == NULL) {
    g_debug ("Could not find filter at position %d", position);
    return FALSE;
  }
  if (link->data != filter) {
    g_debug ("Wrong filter at position %d", position);
    return FALSE;
  }
  priv->filters = g_list_delete_link (priv->filters, link);

  if (priv->applied_bin != NULL)
    new_modification (self, REMOVE, filter, position);

  return TRUE;
}

GstPad *
fsu_filter_manager_apply (FsuFilterManager *self,
    GstBin *bin, GstPad *pad, FsuFilter **failing_filter)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;

  if (priv->applied_bin != NULL) {
    g_debug ("Can only apply the filters once");
    *failing_filter = NULL;
    return NULL;
  }
  if (bin == NULL || pad == NULL) {
    g_debug ("Bin and/or pad NULL. Cannot apply");
    *failing_filter = NULL;
    return NULL;
  }

  g_debug ("Applying on filter manager %p", self);

  priv->applied_pad = pad;
  if (GST_PAD_IS_SRC (pad))
    i = priv->filters;
  else
    i = g_list_last (priv->filters);

  for (; i != NULL;) {
    FsuFilter *filter = i->data;
    GstPad *out_pad = fsu_filter_apply (filter, bin, pad);
    if (out_pad == NULL) {
      gboolean can_fail = FALSE;
      g_object_get (filter, "can-fail", &can_fail, NULL);
      if (can_fail == FALSE) {
        g_debug ("Failable filter failed to apply");
        priv->applied_pad = NULL;
        *failing_filter = filter;
        return NULL;
      }
    } else {
      pad = out_pad;
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
  g_list_foreach (priv->applied_filters, (GFunc) g_object_ref, NULL);

  return pad;
}

GstPad *
fsu_filter_manager_revert (FsuFilterManager *self,
    GstBin *bin, GstPad *pad)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;

  if (bin == NULL || pad == NULL) {
    g_debug ("Bin and/or pad NULL. Cannot revert");
    return NULL;
  }
  if (priv->applied_bin == NULL) {
    g_debug ("Can not revert unapplied filters");
    return NULL;
  }
  if (priv->applied_bin != bin || priv->out_pad != pad) {
    g_debug ("Cannot revert filters from a different bin/pad");
    return NULL;
  }

  g_debug ("Reverting on filter manager %p", self);

  if (GST_PAD_IS_SRC (pad))
    i = g_list_last (priv->applied_filters);
  else
    i = priv->applied_filters;

  for (; i != NULL;) {
    FsuFilter *filter = i->data;
    GstPad *out_pad = fsu_filter_revert (filter, bin, pad);
    if (out_pad != NULL)
      pad = out_pad;
    if (GST_PAD_IS_SRC (pad))
      i = i->prev;
    else
      i = i->next;
  }

  if (priv->applied_pad != pad) {
    g_debug ("Reverting failed, result pad different from applied pad");
    pad = priv->applied_pad;
  }

  g_debug ("Reverted");

  priv->applied_pad = NULL;
  priv->applied_bin = NULL;
  priv->out_pad = NULL;
  if (priv->applied_filters != NULL) {
    g_list_foreach (priv->applied_filters, (GFunc) g_object_unref, NULL);
    g_list_free (priv->applied_filters);
  }
  priv->applied_filters = NULL;

  return pad;
}

