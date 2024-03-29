/*
 * fsu-single-filter-manager.h - Header for FsuSingleFilterManager
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

#ifndef __FSU_SINGLE_FILTER_MANAGER_H__
#define __FSU_SINGLE_FILTER_MANAGER_H__

#include <gst/filters/fsu-filter-manager.h>


G_BEGIN_DECLS

#define FSU_TYPE_SINGLE_FILTER_MANAGER                 \
  (fsu_single_filter_manager_get_type())
#define FSU_SINGLE_FILTER_MANAGER(obj)                 \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_SINGLE_FILTER_MANAGER,                  \
      FsuSingleFilterManager))
#define FSU_SINGLE_FILTER_MANAGER_CLASS(klass)         \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_SINGLE_FILTER_MANAGER,                  \
      FsuSingleFilterManagerClass))
#define FSU_IS_SINGLE_FILTER_MANAGER(obj)              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_SINGLE_FILTER_MANAGER))
#define FSU_IS_SINGLE_FILTER_MANAGER_CLASS(klass)      \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_SINGLE_FILTER_MANAGER))
#define FSU_SINGLE_FILTER_MANAGER_GET_CLASS(obj)       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_SINGLE_FILTER_MANAGER,                  \
      FsuSingleFilterManagerClass))

typedef struct _FsuSingleFilterManager      FsuSingleFilterManager;
typedef struct _FsuSingleFilterManagerClass FsuSingleFilterManagerClass;
typedef struct _FsuSingleFilterManagerPrivate FsuSingleFilterManagerPrivate;

struct _FsuSingleFilterManagerClass
{
  GObjectClass parent_class;
};

/**
 * FsuSingleFilterManager:
 *
 * A filter manager that can only be applied once at a time
 */
struct _FsuSingleFilterManager
{
  GObject parent;
  /*< private >*/
  FsuSingleFilterManagerPrivate *priv;
};

GType fsu_single_filter_manager_get_type (void) G_GNUC_CONST;

FsuFilterManager *fsu_single_filter_manager_new (void);

G_END_DECLS

#endif /* __FSU_FILTER_MANAGER_H__ */
