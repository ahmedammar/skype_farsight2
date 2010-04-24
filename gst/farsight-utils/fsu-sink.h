/*
 * fsu-sink.h - Header for FsuSink
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

#ifndef __FSU_SINK_H__
#define __FSU_SINK_H__

#include <gst/gst.h>

#include "fsu-common.h"

G_BEGIN_DECLS

#define FSU_TYPE_SINK                           \
  (fsu_sink_get_type())
#define FSU_SINK(obj)                           \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_SINK,                            \
      FsuSink))
#define FSU_SINK_CLASS(klass)                   \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_SINK,                            \
      FsuSinkClass))
#define IS_FSU_SINK(obj)                        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_SINK))
#define IS_FSU_SINK_CLASS(klass)                \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_SINK))
#define FSU_SINK_GET_CLASS(obj)                 \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_SINK,                            \
      FsuSinkClass))

typedef struct _FsuSink      FsuSink;
typedef struct _FsuSinkClass FsuSinkClass;
typedef struct _FsuSinkPrivate FsuSinkPrivate;

struct _FsuSinkClass
{
  GstBinClass parent_class;
  gchar *auto_sink_name;
  GstElement *(*create_auto_sink) (FsuSink *self);
  gchar *(*need_mixer) (FsuSink *self, GstElement *sink);
  GstPad *(*add_converters) (FsuSink *self, GstPad *pad);
};

struct _FsuSink
{
  GstBin parent;
  FsuSinkPrivate *priv;
};

GType fsu_sink_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __FSU_SINK_H__ */