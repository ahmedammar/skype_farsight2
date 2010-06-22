/*
 * fsu-filter.h - Header for FsuFilter
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

#ifndef __FSU_FILTER_H__
#define __FSU_FILTER_H__

#include <glib-object.h>
#include <gst/gst.h>


G_BEGIN_DECLS

#define FSU_TYPE_FILTER                         \
  (fsu_filter_get_type())
#define FSU_FILTER(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_FILTER,                          \
      FsuFilter))
#define FSU_FILTER_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_FILTER,                          \
      FsuFilterClass))
#define IS_FSU_FILTER(obj)                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_FILTER))
#define IS_FSU_FILTER_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_FILTER))
#define FSU_FILTER_GET_CLASS(obj)               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_FILTER,                          \
      FsuFilterClass))

typedef struct _FsuFilter      FsuFilter;
typedef struct _FsuFilterClass FsuFilterClass;
typedef struct _FsuFilterPrivate FsuFilterPrivate;

struct _FsuFilterClass
{
  GObjectClass parent_class;
  GstPad *(*apply) (FsuFilter *self, GstBin *bin, GstPad *pad);
  GstPad *(*revert) (FsuFilter *self, GstBin *bin, GstPad *pad);
};

struct _FsuFilter
{
  GObject parent;
  FsuFilterPrivate *priv;
};

GType fsu_filter_get_type (void) G_GNUC_CONST;


GstPad *fsu_filter_apply (FsuFilter *self, GstBin *bin, GstPad *pad);
GstPad *fsu_filter_revert (FsuFilter *self, GstBin *bin, GstPad *pad);
GstPad *fsu_filter_follow (FsuFilter *self, GstPad *pad);

G_END_DECLS

#endif /* __FSU_FILTER_H__ */
