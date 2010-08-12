/*
 * fsu-multi-filter-manager.c - Source for FsuMultiFilterManager
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

#include <gst/farsight/fsu-multi-filter-manager.h>
#include <gst/farsight/fsu-single-filter-manager.h>

static void fsu_multi_filter_manager_interface_init (
    FsuFilterManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (FsuMultiFilterManager,
    fsu_multi_filter_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (FSU_TYPE_FILTER_MANAGER,
        fsu_multi_filter_manager_interface_init));

static void fsu_multi_filter_manager_dispose (GObject *object);
static void free_filter_id (FsuFilterId *id);

static GList *fsu_multi_filter_manager_list_filters (
    FsuFilterManager *iface);
static FsuFilterId *fsu_multi_filter_manager_insert_filter_before (
    FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *before);
static FsuFilterId *fsu_multi_filter_manager_insert_filter_after (
    FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *after);
static FsuFilterId *fsu_multi_filter_manager_replace_filter (
    FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *replace);
static FsuFilterId *fsu_multi_filter_manager_insert_filter (
    FsuFilterManager *iface,
    FsuFilter *filter,
    gint position);
static gboolean fsu_multi_filter_manager_remove_filter (
    FsuFilterManager *iface,
    FsuFilterId *id);
static FsuFilter *fsu_multi_filter_manager_get_filter_by_id (
    FsuFilterManager *iface,
    FsuFilterId *id);
static GstPad *fsu_multi_filter_manager_apply (
    FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_multi_filter_manager_revert (
    FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad);
static gboolean fsu_multi_filter_manager_handle_message (
    FsuFilterManager *iface,
    GstMessage *message);

struct _FsuMultiFilterManagerPrivate
{
  GList *filters;
  GList *filter_managers;
  GMutex *mutex;
};

struct _FsuFilterId
{
  FsuFilter *filter;
  /* SingleFilterManager->FilterId*/
  GHashTable *associations;
};


static void
fsu_multi_filter_manager_class_init (FsuMultiFilterManagerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuMultiFilterManagerPrivate));

  gobject_class->dispose = fsu_multi_filter_manager_dispose;
}

static void
fsu_multi_filter_manager_interface_init (FsuFilterManagerInterface *iface)
{
  iface->list_filters = fsu_multi_filter_manager_list_filters;
  iface->insert_filter_before = fsu_multi_filter_manager_insert_filter_before;
  iface->insert_filter_after = fsu_multi_filter_manager_insert_filter_after;
  iface->replace_filter = fsu_multi_filter_manager_replace_filter;
  iface->insert_filter = fsu_multi_filter_manager_insert_filter;
  iface->remove_filter = fsu_multi_filter_manager_remove_filter;
  iface->get_filter_by_id = fsu_multi_filter_manager_get_filter_by_id;
  iface->apply = fsu_multi_filter_manager_apply;
  iface->revert = fsu_multi_filter_manager_revert;
  iface->handle_message = fsu_multi_filter_manager_handle_message;
}

static void
fsu_multi_filter_manager_init (FsuMultiFilterManager *self)
{
  FsuMultiFilterManagerPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_MULTI_FILTER_MANAGER,
          FsuMultiFilterManagerPrivate);

  self->priv = priv;
  priv->mutex = g_mutex_new ();
}


static void
fsu_multi_filter_manager_dispose (GObject *object)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (object);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *i;


  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *fm = i->data;

    g_object_unref (fm);
  }
  g_list_free (priv->filter_managers);
  priv->filter_managers = NULL;

  for (i = priv->filters; i; i = i->next)
  {
    FsuFilterId *id = i->data;

    free_filter_id (id);
  }
  g_list_free (priv->filters);
  priv->filters = NULL;

  if (priv->mutex)
    g_mutex_free (priv->mutex);
  priv->mutex = NULL;

  G_OBJECT_CLASS (fsu_multi_filter_manager_parent_class)->dispose (object);
}

/**
 * fsu_multi_filter_manager_new:
 *
 * Creates a Multi filter manager.
 * The Multi filter manager is a filter manager that can be applied on multiple
 * pads on multiple bins. It will create a new #FsuSingleFilterManager each time
 * fsu_filter_manager_apply() is called and will make sure to sync up the
 * filters between it and all the filter managers it creates
 *
 * Returns: A new #FsuFilterManager
 */
FsuFilterManager *
fsu_multi_filter_manager_new (void)
{
  return g_object_new (FSU_TYPE_MULTI_FILTER_MANAGER, NULL);
}

static void
free_filter_id (FsuFilterId *id)
{
  g_hash_table_destroy (id->associations);
  g_object_unref (id->filter);
  g_slice_free (FsuFilterId, id);
}

static GList *
fsu_multi_filter_manager_list_filters (FsuFilterManager *iface)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *ret = NULL;

  g_mutex_lock (priv->mutex);
  ret = g_list_copy (priv->filters);
  g_mutex_unlock (priv->mutex);

  return ret;
}


static FsuFilterId *
fsu_multi_filter_manager_insert_filter_before (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *before)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *pos = NULL;
  FsuFilterId *id = NULL;
  GList *i = NULL;

  g_mutex_lock (priv->mutex);
  pos = g_list_find (priv->filters, before);
  g_mutex_unlock (priv->mutex);

  if (!pos)
    return NULL;

  id = g_slice_new0 (FsuFilterId);
  id->filter = g_object_ref (filter);
  id->associations = g_hash_table_new (NULL, NULL);

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *sub_fm = i->data;
    FsuFilterId *before_sub_id = NULL;
    FsuFilterId *sub_id = NULL;

    g_mutex_unlock (priv->mutex);

    before_sub_id = g_hash_table_lookup (before->associations, sub_fm);
    if (before_sub_id)
    {
      sub_id = fsu_filter_manager_insert_filter_before (sub_fm, filter,
          before_sub_id);
      g_hash_table_insert (id->associations, sub_fm, sub_id);
    }
    g_mutex_lock (priv->mutex);
  }

  priv->filters = g_list_insert_before (priv->filters, pos, id);
  g_mutex_unlock (priv->mutex);

  return id;
}


static FsuFilterId *
fsu_multi_filter_manager_insert_filter_after (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *after)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *pos = NULL;
  FsuFilterId *id = NULL;
  GList *i = NULL;

  g_mutex_lock (priv->mutex);
  pos = g_list_find (priv->filters, after);
  g_mutex_unlock (priv->mutex);

  if (!pos)
    return NULL;

  id = g_slice_new0 (FsuFilterId);
  id->filter = g_object_ref (filter);
  id->associations = g_hash_table_new (NULL, NULL);

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *sub_fm = i->data;
    FsuFilterId *after_sub_id = NULL;
    FsuFilterId *sub_id = NULL;

    g_mutex_unlock (priv->mutex);

    after_sub_id = g_hash_table_lookup (after->associations, sub_fm);
    if (after_sub_id)
    {
      sub_id = fsu_filter_manager_insert_filter_after (sub_fm, filter,
          after_sub_id);
      g_hash_table_insert (id->associations, sub_fm, sub_id);
    }
    g_mutex_lock (priv->mutex);
  }

  priv->filters = g_list_insert_before (priv->filters, pos->next, id);
  g_mutex_unlock (priv->mutex);

  return id;
}


static FsuFilterId *
fsu_multi_filter_manager_replace_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *replace)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  gint index = -1;
  FsuFilterId *id = NULL;
  GList *i = NULL;

  g_mutex_lock (priv->mutex);
  index = g_list_index (priv->filters, replace);
  g_mutex_unlock (priv->mutex);

  if (index < 0)
    return NULL;

  id = g_slice_new0 (FsuFilterId);
  id->filter = g_object_ref (filter);
  id->associations = g_hash_table_new (NULL, NULL);

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *sub_fm = i->data;
    FsuFilterId *replace_sub_id = NULL;
    FsuFilterId *sub_id = NULL;

    g_mutex_unlock (priv->mutex);

    replace_sub_id = g_hash_table_lookup (replace->associations, sub_fm);
    if (replace_sub_id)
    {
      sub_id = fsu_filter_manager_replace_filter (sub_fm, filter,
          replace_sub_id);
      g_hash_table_insert (id->associations, sub_fm, sub_id);
    }
    g_mutex_lock (priv->mutex);
  }

  priv->filters = g_list_remove (priv->filters, replace);
  priv->filters = g_list_insert (priv->filters, id, index);
  g_mutex_unlock (priv->mutex);

  free_filter_id (replace);

  return id;
}


static FsuFilterId *
fsu_multi_filter_manager_insert_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    gint position)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  FsuFilterId *id = NULL;
  GList *i = NULL;

  id = g_slice_new0 (FsuFilterId);
  id->filter = g_object_ref (filter);
  id->associations = g_hash_table_new (NULL, NULL);

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *sub_fm = i->data;
    FsuFilterId *sub_id = NULL;

    g_mutex_unlock (priv->mutex);
    sub_id = fsu_filter_manager_insert_filter (sub_fm, filter, position);
    g_hash_table_insert (id->associations, sub_fm, sub_id);
    g_mutex_lock (priv->mutex);
  }

  priv->filters = g_list_insert (priv->filters, id, position);
  g_mutex_unlock (priv->mutex);

  return id;
}


static gboolean
fsu_multi_filter_manager_remove_filter (FsuFilterManager *iface,
    FsuFilterId *id)
{

  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *pos = NULL;
  GList *i = NULL;

  g_mutex_lock (priv->mutex);
  pos = g_list_find (priv->filters, id);
  g_mutex_unlock (priv->mutex);

  if (!pos)
    return FALSE;

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *sub_fm = i->data;
    FsuFilterId *remove_sub_id = NULL;
    gboolean ret = TRUE;

    g_mutex_unlock (priv->mutex);

    remove_sub_id = g_hash_table_lookup (id->associations, sub_fm);
    if (remove_sub_id)
      ret = fsu_filter_manager_remove_filter (sub_fm, remove_sub_id);
    g_assert (ret == TRUE);
    g_mutex_lock (priv->mutex);
  }

  priv->filters = g_list_remove (priv->filters, id);
  g_mutex_unlock (priv->mutex);

  free_filter_id (id);

  return TRUE;
}


static FsuFilter *
fsu_multi_filter_manager_get_filter_by_id (FsuFilterManager *iface,
    FsuFilterId *id)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *find = NULL;

  g_mutex_lock (priv->mutex);
  find = g_list_find (self->priv->filters, id);
  g_mutex_unlock (priv->mutex);

  if (!find)
    return NULL;

  return g_object_ref (id->filter);
}


static GstPad *
fsu_multi_filter_manager_apply (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  FsuFilterManager *fm = fsu_single_filter_manager_new ();
  GstPad * ret = NULL;
  GList *i = NULL;

  g_mutex_lock (priv->mutex);
  priv->filter_managers = g_list_append (priv->filter_managers, fm);
  for (i = priv->filters; i; i = i->next)
  {
    FsuFilterId *id = i->data;
    FsuFilterId *sub_id = NULL;

    g_mutex_unlock (priv->mutex);
    sub_id = fsu_filter_manager_append_filter (fm, id->filter);
    g_hash_table_insert (id->associations, fm, sub_id);
    g_mutex_lock (priv->mutex);
  }
  g_mutex_unlock (priv->mutex);

  ret = fsu_filter_manager_apply (fm, bin, pad);

  if (!ret)
  {
    g_mutex_lock (priv->mutex);
    for (i = priv->filters; i; i = i->next)
    {
      FsuFilterId *id = i->data;
      g_mutex_unlock (priv->mutex);
      g_hash_table_remove (id->associations, fm);
      g_mutex_lock (priv->mutex);
    }
    g_mutex_unlock (priv->mutex);
    g_object_unref (fm);
    return NULL;
  }

  return ret;
}

static GstPad *
fsu_multi_filter_manager_revert (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  FsuFilterManager *revert_fm = NULL;
  GList *i = NULL;
  GstPad *ret = NULL;

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i; i = i->next)
  {
    FsuFilterManager *fm = i->data;
    GstBin *applied_bin = NULL;
    GstPad *out_pad = NULL;

    g_mutex_unlock (priv->mutex);

    g_object_get (fm,
        "applied-bin", &applied_bin,
        "out-pad", &out_pad,
        NULL);
    if (applied_bin == bin && out_pad == pad)
    {
      revert_fm = fm;
      gst_object_unref (applied_bin);
      gst_object_unref (out_pad);
      g_mutex_lock (priv->mutex);
      break;
    }
    gst_object_unref (applied_bin);
    gst_object_unref (out_pad);
    g_mutex_lock (priv->mutex);
  }
  g_mutex_unlock (priv->mutex);

  if (revert_fm == NULL)
  {
    g_debug ("Could not found the single filter manager to revert");
    return NULL;
  }

  g_mutex_lock (priv->mutex);
  priv->filter_managers = g_list_remove (priv->filter_managers, revert_fm);
  for (i = priv->filters; i; i = i->next)
  {
    FsuFilterId *id = i->data;
    g_mutex_unlock (priv->mutex);
    g_hash_table_remove (id->associations, revert_fm);
    g_mutex_lock (priv->mutex);
  }
  g_mutex_unlock (priv->mutex);

  ret = fsu_filter_manager_revert (revert_fm, bin, pad);
  g_object_unref (revert_fm);

  return ret;
}

static gboolean
fsu_multi_filter_manager_handle_message (FsuFilterManager *iface,
    GstMessage *message)
{
  FsuMultiFilterManager *self = FSU_MULTI_FILTER_MANAGER (iface);
  FsuMultiFilterManagerPrivate *priv = self->priv;
  GList *i = NULL;
  gboolean ret = FALSE;

  g_mutex_lock (priv->mutex);
  for (i = priv->filter_managers; i && !ret; i = i->next)
  {
    FsuFilterManager *sub_fm = i->data;

    g_mutex_unlock (priv->mutex);
    ret = fsu_filter_manager_handle_message (sub_fm, message);
    g_mutex_lock (priv->mutex);
  }
  g_mutex_unlock (priv->mutex);

  return ret;
}
