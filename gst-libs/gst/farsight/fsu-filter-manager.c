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


struct _FsuFilterManagerPrivate
{
  gboolean dispose_has_run;
  GList *filters;
  GstBin *applied_bin;
  GstPad *applied_pad;
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

gboolean
fsu_filter_manager_insert_filter (FsuFilterManager *self,
    FsuFilter *filter, guint position)
{
  FsuFilterManagerPrivate *priv = self->priv;

  if (priv->applied_bin != NULL) {
    g_debug ("Cannot modify filters after being applied (for now)");
    return FALSE;
  }

  g_object_ref (filter);
  if (position < 0)
    priv->filters = g_list_append (priv->filters, filter);
  else
    /* FIXME: doc says if position < 0, then it appends, but it takes a gint? */
    priv->filters = g_list_insert (priv->filters, filter, (gint) position);

  return TRUE;
}

GList *
fsu_filter_manager_list_filters (FsuFilterManager *self)
{
  return g_list_copy (self->priv->filters);
}

gboolean
fsu_filter_manager_remove_filter (FsuFilterManager *self,
    FsuFilter *filter, guint position)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *link = NULL;


  if (priv->applied_bin != NULL) {
    g_debug ("Cannot modify filters after being applied (for now)");
    return FALSE;
  }

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
    }
    pad = out_pad;
    if (GST_PAD_IS_SRC (pad))
      i = i->next;
    else
      i = i->prev;
  }

  g_debug ("Applied");

  priv->out_pad = pad;
  priv->applied_bin = bin;

  return pad;
}

GstPad *
fsu_filter_manager_revert (FsuFilterManager *self,
    GstBin *bin, GstPad *pad)
{
  FsuFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;

  /* TODO: blocked pad ? */

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
    i = g_list_last (priv->filters);
  else
    i = priv->filters;

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

  return pad;
}
