/*
 * fsu-video-sink.h - Header for FsuVideoSink
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

#ifndef __FSU_VIDEO_SINK_H__
#define __FSU_VIDEO_SINK_H__

#include <gst/farsight/fsu-sink-class.h>


G_BEGIN_DECLS

#define FSU_TYPE_VIDEO_SINK                   \
  (fsu_video_sink_get_type())
#define FSU_VIDEO_SINK(obj)                   \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_VIDEO_SINK,                    \
      FsuVideoSink))
#define FSU_VIDEO_SINK_CLASS(klass)           \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_VIDEO_SINK,                    \
      FsuVideoSinkClass))
#define FSU_IS_VIDEO_SINK(obj)                \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_VIDEO_SINK))
#define FSU_IS_VIDEO_SINK_CLASS(klass)        \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_VIDEO_SINK))
#define FSU_VIDEO_SINK_GET_CLASS(obj)         \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_VIDEO_SINK,                    \
      FsuVideoSinkClass))

typedef struct _FsuVideoSink      FsuVideoSink;
typedef struct _FsuVideoSinkClass FsuVideoSinkClass;

struct _FsuVideoSinkClass
{
  FsuSinkClass parent_class;
};

struct _FsuVideoSink
{
  FsuSink parent;
};

GType fsu_video_sink_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __FSU_VIDEO_SINK_H__ */
