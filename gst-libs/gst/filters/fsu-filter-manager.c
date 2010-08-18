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


/**
 * SECTION:fsu-filter-manager
 * @short_description: The filter manager to handle #FsuFilter objects
 *
 * This class acts as a manager for multiple #FsuFilter objects.
 * Its main purpose is to allow the user to easily add and remove filters from
 * a pipeline and it will take care of doing whatever is necessary to keep the
 * pipeline in a consistent state.
 * It will send signals to let the user know of what is happening in terms of
 * applying/reverting filters and whatever or not the apply/revert worked.
 *
 * See also #FsuFilter
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/filters/fsu-filter-manager.h>


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

/**
 * fsu_filter_manager_list_filters:
 * @self: The #FsuFilterManager
 *
 * List all the filters that are currently in the filter manager.
 *
 * Returns: The ordered list of filters as a #GList of #FsuFilterId
 * See also: fsu_filter_manager_get_filter_by_id()
 */
GList *
fsu_filter_manager_list_filters (FsuFilterManager *self)
{
  FsuFilterManagerInterface *iface =
      FSU_FILTER_MANAGER_GET_IFACE (self);

  g_return_val_if_fail (iface, NULL);
  g_return_val_if_fail (iface->list_filters, NULL);

  return iface->list_filters (self);
}

/**
 * fsu_filter_manager_prepend_filter:
 * @self: The #FsuFilterManager
 * @filter: The filter to prepend
 *
 * Add a filter to the beginning of the list of filters in the filter manager
 *
 * Returns: The #FsuFilterId of the inserted filter or #NULL in case of error
 */
FsuFilterId *
fsu_filter_manager_prepend_filter (FsuFilterManager *self,
    FsuFilter *filter)
{
  return fsu_filter_manager_insert_filter (self, filter, 0);
}

/**
 * fsu_filter_manager_apppend_filter:
 * @self: The #FsuFilterManager
 * @filter: The filter to append
 *
 * Add a filter to the end of the list of filters in the filter manager
 *
 * Returns: The #FsuFilterId of the inserted filter or #NULL in case of error
 */
FsuFilterId *
fsu_filter_manager_append_filter (FsuFilterManager *self,
    FsuFilter *filter)
{
  return fsu_filter_manager_insert_filter (self, filter, -1);
}

/**
 * fsu_filter_manager_insert_filter_before:
 * @self: The #FsuFilterManager
 * @filter: The filter to insert
 * @before: The #FsuFilterId of the filter before which to insert @filter
 *
 * Add a filter before @before in the list of filters in the filter manager
 *
 * Returns: The #FsuFilterId of the inserted filter or #NULL in case of error
 */
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

/**
 * fsu_filter_manager_insert_filter_after:
 * @self: The #FsuFilterManager
 * @filter: The filter to insert
 * @after: The #FsuFilterId of the filter after which to insert @filter
 *
 * Add a filter after @after in the list of filters in the filter manager
 *
 * Returns: The #FsuFilterId of the inserted filter or #NULL in case of error
 */
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


/**
 * fsu_filter_manager_replace_filter:
 * @self: The #FsuFilterManager
 * @filter: The filter to insert
 * @replace: The #FsuFilterId of the filter to replace by @filter
 *
 * Removes the filter identified by @replace and replace it with the filter
 * @filter in the same position in the filter manager
 *
 * <para>
   <note>
   This function is more optimal than calling fsu_filter_manager_remove_filter()
   followed by fsu_filter_manager_insert_filter(), because those two operations
   would cause an unlink/revert/link followed by unlink/apply/link while the
   fsu_filter_manager_replace_filter() operation only causes a
   unlink/revert/apply/link which will save you an unlinking and a linking of
   pads, which would cause a caps renegociation
   </note>
 * </para>
 *
 * Returns: The #FsuFilterId of the inserted filter or #NULL in case of error
 */
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


/**
 * fsu_filter_manager_insert_filter:
 * @self: The #FsuFilterManager
 * @filter: The filter to insert
 * @position: The insert position of the filter in the list
 *
 * Add a filter in the list of filters in the filter manager at the specified
 * position.
 *
 * Returns: The #FsuFilterId of the inserted filter or #NULL in case of error
 * See also: fsu_filter_manager_list_filters()
 */
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

/**
 * fsu_filter_manager_remove_filter:
 * @self: The #FsuFilterManager
 * @id: The id of the filter to remove
 *
 * Removes the filter identified by @id from the list of filters in the
 * filter manager
 *
 * Returns: #TRUE if the #FsuFilterId is valid and the filter was removed,
 * #FALSE if the @id is invalid
 */
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


/**
 * fsu_filter_manager_get_filter_by_id:
 * @self: The #FsuFilterManager
 * @id: The id of the filter
 *
 * Get the #FsuFilter identified by the @id #FsuFilterId from the list of
 * filters in the filter manager
 *
 * Returns: The #FsuFilter representing @id or #NULL if @id is invalid.
 * The returned filter is reffed before being returned, so call g_object_unref()
 * once the filter is not needed anymore.
 */
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

/**
 * fsu_filter_manager_apply:
 * @self: The #FsuFilterManager
 * @bin: The #GstBin to apply the filter manager to
 * @pad: The #GstPad to apply the filter manager to
 *
 * This will apply the filter manager to a bin on a specific pad. If the filter
 * manager already has some filters in it, they will automatically be applied
 * on that pad, otherwise, the filter manager will hook itself to that pad and
 * will add filters to it once the they get added to the filter manager.
 * The filter manager can be applied on either a source pad or a sink pad, it
 * will still work the same and the order of the filters will stay be respected
 * in a source-to-sink order.
 *
 * Returns: The new applied #GstPad to link with the rest of the pipeline
 * See also: fsu_filter_manager_revert()
 */
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

/**
 * fsu_filter_manager_revert:
 * @self: The #FsuFilterManager
 * @bin: The #GstBin to revert the filter manager from
 * @pad: The #GstPad to revert the filter manager from
 *
 * This will revert the filter manager from a bin on the specified pad.
 * The pad has to be the end part of the filter manager. This is necessarily the
 * same output pad from fsu_filter_manager_apply() because if new filters get
 * added to the start or end of the filter manager, that pad might change.
 * The best way to get the correct pad to revert from is to use get the peer pad
 * of the element that was linked after the filter manager.
 * In the case of #FsuSingleFilterManager, you can also get the out-pad by
 * querrying the filter manager's #FsuSingleFilterManager:out-pad property.
 *
 * Returns: The original #GstPad from which the filter manager was applied
 * See also: fsu_filter_manager_apply()
 */
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

/**
 * fsu_filter_manager_handle_message:
 * @self: The #FsuFilterManager
 * @message: The message to handle
 *
 * Dispatch a message originally received on the #GstBus to the filter manager
 * and to all its filters. Once a filter handles the message, it will stop
 * dispatching it to other filters and will return #TRUE
 *
 * Returns: #TRUE if the message has been handled and should be dropped,
 * #FALSE otherwise.
 */
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
