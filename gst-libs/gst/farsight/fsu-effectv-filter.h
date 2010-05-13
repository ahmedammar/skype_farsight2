/*
 * fsu-effectv-filter.h - Header for FsuEffectvFilter
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

#ifndef __FSU_EFFECTV_FILTER_H__
#define __FSU_EFFECTV_FILTER_H__

#include <gst/farsight/fsu-filter.h>


G_BEGIN_DECLS

#define FSU_TYPE_EFFECTV_FILTER                 \
  (fsu_effectv_filter_get_type())
#define FSU_EFFECTV_FILTER(obj)                 \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_EFFECTV_FILTER,                  \
      FsuEffectvFilter))
#define FSU_EFFECTV_FILTER_CLASS(klass)         \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_EFFECTV_FILTER,                  \
      FsuEffectvFilterClass))
#define IS_FSU_EFFECTV_FILTER(obj)              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_EFFECTV_FILTER))
#define IS_FSU_EFFECTV_FILTER_CLASS(klass)      \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_EFFECTV_FILTER))
#define FSU_EFFECTV_FILTER_GET_CLASS(obj)       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_EFFECTV_FILTER,                  \
      FsuEffectvFilterClass))

typedef struct _FsuEffectvFilter      FsuEffectvFilter;
typedef struct _FsuEffectvFilterClass FsuEffectvFilterClass;
typedef struct _FsuEffectvFilterPrivate FsuEffectvFilterPrivate;

struct _FsuEffectvFilterClass
{
  FsuFilterClass parent_class;
};

struct _FsuEffectvFilter
{
  FsuFilter parent;
  FsuEffectvFilterPrivate *priv;
};

GType fsu_effectv_filter_get_type (void) G_GNUC_CONST;

FsuEffectvFilter *fsu_effectv_filter_new (const gchar *effect);

G_END_DECLS

#endif /* __FSU_EFFECTV_FILTER_H__ */
