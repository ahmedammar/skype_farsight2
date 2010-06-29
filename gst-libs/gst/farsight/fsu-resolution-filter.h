/*
 * fsu-resolution-filter.h - Header for FsuResolutionFilter
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

#ifndef __FSU_RESOLUTION_FILTER_H__
#define __FSU_RESOLUTION_FILTER_H__

#include <gst/farsight/fsu-filter.h>


G_BEGIN_DECLS

#define FSU_TYPE_RESOLUTION_FILTER              \
  (fsu_resolution_filter_get_type())
#define FSU_RESOLUTION_FILTER(obj)              \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_RESOLUTION_FILTER,               \
      FsuResolutionFilter))
#define FSU_RESOLUTION_FILTER_CLASS(klass)      \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_RESOLUTION_FILTER,               \
      FsuResolutionFilterClass))
#define IS_FSU_RESOLUTION_FILTER(obj)           \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_RESOLUTION_FILTER))
#define IS_FSU_RESOLUTION_FILTER_CLASS(klass)   \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_RESOLUTION_FILTER))
#define FSU_RESOLUTION_FILTER_GET_CLASS(obj)    \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_RESOLUTION_FILTER,               \
      FsuResolutionFilterClass))

typedef struct _FsuResolutionFilter      FsuResolutionFilter;
typedef struct _FsuResolutionFilterClass FsuResolutionFilterClass;
typedef struct _FsuResolutionFilterPrivate FsuResolutionFilterPrivate;

struct _FsuResolutionFilterClass
{
  FsuFilterClass parent_class;
};

struct _FsuResolutionFilter
{
  FsuFilter parent;
  FsuResolutionFilterPrivate *priv;
};

GType fsu_resolution_filter_get_type (void) G_GNUC_CONST;

FsuResolutionFilter *fsu_resolution_filter_new (guint width, guint height);

G_END_DECLS

#endif /* __FSU_RESOLUTION_FILTER_H__ */
