/*
 * fsu-maxframerate-filter.h - Header for FsuMaxFramerateFilter
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

#ifndef __FSU_MAXFRAMERATE_FILTER_H__
#define __FSU_MAXFRAMERATE_FILTER_H__

#include <gst/filters/fsu-filter.h>


G_BEGIN_DECLS

#define FSU_TYPE_MAXFRAMERATE_FILTER              \
  (fsu_maxframerate_filter_get_type())
#define FSU_MAXFRAMERATE_FILTER(obj)              \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_MAXFRAMERATE_FILTER,               \
      FsuMaxFramerateFilter))
#define FSU_MAXFRAMERATE_FILTER_CLASS(klass)      \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_MAXFRAMERATE_FILTER,               \
      FsuMaxFramerateFilterClass))
#define FSU_IS_MAXFRAMERATE_FILTER(obj)           \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_MAXFRAMERATE_FILTER))
#define FSU_IS_MAXFRAMERATE_FILTER_CLASS(klass)   \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_MAXFRAMERATE_FILTER))
#define FSU_MAXFRAMERATE_FILTER_GET_CLASS(obj)    \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_MAXFRAMERATE_FILTER,               \
      FsuMaxFramerateFilterClass))

typedef struct _FsuMaxFramerateFilter      FsuMaxFramerateFilter;
typedef struct _FsuMaxFramerateFilterClass FsuMaxFramerateFilterClass;
typedef struct _FsuMaxFramerateFilterPrivate FsuMaxFramerateFilterPrivate;

struct _FsuMaxFramerateFilterClass
{
  FsuFilterClass parent_class;
};

/**
 * FsuMaxFramerateFilter:
 *
 * A video maxframerate filter
 */
struct _FsuMaxFramerateFilter
{
  FsuFilter parent;
  /*< private >*/
  FsuMaxFramerateFilterPrivate *priv;
};

GType fsu_maxframerate_filter_get_type (void) G_GNUC_CONST;

FsuMaxFramerateFilter *fsu_maxframerate_filter_new (guint fps);

G_END_DECLS

#endif /* __FSU_MAXFRAMERATE_FILTER_H__ */
