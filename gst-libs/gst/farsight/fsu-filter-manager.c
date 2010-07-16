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


G_DEFINE_INTERFACE (FsuFilterManager, fsu_filter_manager, G_TYPE_OBJECT);


static void
fsu_filter_manager_default_init (FsuFilterManagerInterface *iface)
{
  iface->list_filters = NULL;
  iface->insert_filter_before = NULL;
  iface->insert_filter_after = NULL;
  iface->replace_filter = NULL;
  iface->insert_filter = NULL;
  iface->remove_filter = NULL;
  iface->get_filter_by_id = NULL;
  iface->apply = NULL;
  iface->revert = NULL;
  iface->handle_message = NULL;
}


GList *
fsu_filter_manager_list_filters (FsuFilterManager *self)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->list_filters, NULL);

  return iface->list_filters (self);
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
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->insert_filter_before, NULL);

  return iface->insert_filter_before (self, filter, before);
}


FsuFilterId *
fsu_filter_manager_insert_filter_after (FsuFilterManager *self,
    FsuFilter *filter,
    FsuFilterId *after)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->insert_filter_after, NULL);

  return iface->insert_filter_after (self, filter, after);
}


FsuFilterId *
fsu_filter_manager_replace_filter (FsuFilterManager *self,
    FsuFilter *filter,
    FsuFilterId *replace)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->replace_filter, NULL);

  return iface->replace_filter (self, filter, replace);
}


FsuFilterId *
fsu_filter_manager_insert_filter (FsuFilterManager *self,
    FsuFilter *filter,
    gint position)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->insert_filter, NULL);

  return iface->insert_filter (self, filter, position);
}


gboolean
fsu_filter_manager_remove_filter (FsuFilterManager *self,
    FsuFilterId *id)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, FALSE);
  g_return_val_if_fail (iface->remove_filter, FALSE);

  return iface->remove_filter (self, id);
}


FsuFilter *
fsu_filter_manager_get_filter_by_id (FsuFilterManager *self,
    FsuFilterId *id)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->get_filter_by_id, NULL);

  return iface->get_filter_by_id (self, id);
}

GstPad *
fsu_filter_manager_apply (FsuFilterManager *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->apply, NULL);

  return iface->apply (self, bin, pad);
}

GstPad *
fsu_filter_manager_revert (FsuFilterManager *self,
    GstBin *bin,
    GstPad *pad)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->revert, NULL);

  return iface->revert (self, bin, pad);
}

gboolean
fsu_filter_manager_handle_message (FsuFilterManager *self,
    GstMessage *message)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, FALSE);
  g_return_val_if_fail (iface->handle_message, FALSE);

  return iface->handle_message (self, message);
}
