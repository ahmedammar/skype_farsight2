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

static void fsu_multi_filter_manager_interface_init (
    FsuFilterManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (FsuMultiFilterManager,
    fsu_multi_filter_manager, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (FSU_TYPE_FILTER_MANAGER,
        fsu_multi_filter_manager_interface_init));

static void fsu_multi_filter_manager_dispose (GObject *object);

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
  gboolean dispose_has_run;
};

struct _FsuFilterId
{
  FsuFilter *filter;
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
  priv->dispose_has_run = FALSE;
}


static void
fsu_multi_filter_manager_dispose (GObject *object)
{
  FsuMultiFilterManager *self = (FsuMultiFilterManager *)object;
  FsuMultiFilterManagerPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  G_OBJECT_CLASS (fsu_multi_filter_manager_parent_class)->dispose (object);
}


FsuFilterManager *fsu_multi_filter_manager_new (void)
{
  return g_object_new (FSU_TYPE_MULTI_FILTER_MANAGER, NULL);
}


static GList *
fsu_multi_filter_manager_list_filters (FsuFilterManager *iface)
{
  return NULL;
}


static FsuFilterId *
fsu_multi_filter_manager_insert_filter_before (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *before)
{
  return NULL;
}


static FsuFilterId *
fsu_multi_filter_manager_insert_filter_after (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *after)
{
  return NULL;
}


static FsuFilterId *
fsu_multi_filter_manager_replace_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    FsuFilterId *replace)
{
  return NULL;
}


static FsuFilterId *
fsu_multi_filter_manager_insert_filter (FsuFilterManager *iface,
    FsuFilter *filter,
    gint position)
{
  return NULL;
}


static gboolean
fsu_multi_filter_manager_remove_filter (FsuFilterManager *iface,
    FsuFilterId *id)
{
  return FALSE;
}


static FsuFilter *
fsu_multi_filter_manager_get_filter_by_id (FsuFilterManager *iface,
    FsuFilterId *id)
{
  return NULL;
}


static GstPad *
fsu_multi_filter_manager_apply (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  return NULL;
}

static GstPad *
fsu_multi_filter_manager_revert (FsuFilterManager *iface,
    GstBin *bin,
    GstPad *pad)
{
  return NULL;
}

static gboolean
fsu_multi_filter_manager_handle_message (FsuFilterManager *iface,
    GstMessage *message)
{
  return FALSE;
}
