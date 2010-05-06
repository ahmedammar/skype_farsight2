/*
 * fsu-filter-manager.h - Header for FsuFilterManager
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

#ifndef __FSU_FILTER_MANAGER_H__
#define __FSU_FILTER_MANAGER_H__

#include <glib-object.h>
#include <gst/gst.h>
#include <gst/farsight/fsu-filter.h>


G_BEGIN_DECLS

#define FSU_TYPE_FILTER_MANAGER                 \
  (fsu_filter_manager_get_type())
#define FSU_FILTER_MANAGER(obj)                 \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_FILTER_MANAGER,                  \
      FsuFilterManager))
#define FSU_FILTER_MANAGER_CLASS(klass)         \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_FILTER_MANAGER,                  \
      FsuFilterManagerClass))
#define IS_FSU_FILTER_MANAGER(obj)              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_FILTER_MANAGER))
#define IS_FSU_FILTER_MANAGER_CLASS(klass)      \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_FILTER_MANAGER))
#define FSU_FILTER_MANAGER_GET_CLASS(obj)       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_FILTER_MANAGER,                  \
      FsuFilterManagerClass))

typedef struct _FsuFilterManager      FsuFilterManager;
typedef struct _FsuFilterManagerClass FsuFilterManagerClass;
typedef struct _FsuFilterManagerPrivate FsuFilterManagerPrivate;

struct _FsuFilterManagerClass
{
  GObjectClass parent_class;
};

struct _FsuFilterManager
{
  GObject parent;
  FsuFilterManagerPrivate *priv;
};

GType fsu_filter_manager_get_type (void) G_GNUC_CONST;

FsuFilterManager *fsu_filter_manager_new (void);
gboolean fsu_filter_manager_insert_filter (FsuFilterManager *self,
    FsuFilter *filter, guint position);
GList *fsu_filter_manager_list_filters (FsuFilterManager *self);
gboolean fsu_filter_manager_remove_filter (FsuFilterManager *self,
    FsuFilter *filter, guint position);

GstPad *fsu_filter_manager_apply (FsuFilterManager *self,
    GstBin *bin, GstPad *pad, FsuFilter **failing_filter);
GstPad *fsu_filter_manager_revert (FsuFilterManager *self,
    GstBin *bin, GstPad *pad);

G_END_DECLS

#endif /* __FSU_FILTER_MANAGER_H__ */
