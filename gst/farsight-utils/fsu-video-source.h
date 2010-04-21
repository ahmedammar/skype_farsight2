/*
 * fsu-video-source.h - Header for FsuVideoSource
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

#ifndef __FSU_VIDEO_SOURCE_H__
#define __FSU_VIDEO_SOURCE_H__

#include "fsu-source.h"


G_BEGIN_DECLS

#define FSU_TYPE_VIDEO_SOURCE                   \
  (fsu_video_source_get_type())
#define FSU_VIDEO_SOURCE(obj)                   \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_VIDEO_SOURCE,                    \
      FsuVideoSource))
#define FSU_VIDEO_SOURCE_CLASS(klass)           \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_VIDEO_SOURCE,                    \
      FsuVideoSourceClass))
#define IS_FSU_VIDEO_SOURCE(obj)                \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_VIDEO_SOURCE))
#define IS_FSU_VIDEO_SOURCE_CLASS(klass)        \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_VIDEO_SOURCE))
#define FSU_VIDEO_SOURCE_GET_CLASS(obj)         \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_VIDEO_SOURCE,                    \
      FsuVideoSourceClass))

typedef struct _FsuVideoSource      FsuVideoSource;
typedef struct _FsuVideoSourceClass FsuVideoSourceClass;

struct _FsuVideoSourceClass
{
  FsuSourceClass parent_class;
};

struct _FsuVideoSource
{
  FsuSource parent;
};

GType fsu_video_source_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __FSU_VIDEO_SOURCE_H__ */
