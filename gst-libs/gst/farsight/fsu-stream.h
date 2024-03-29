/*
 * fsu-stream.h - Header for FsuStream
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

#ifndef __FSU_STREAM_H__
#define __FSU_STREAM_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define FSU_TYPE_STREAM                         \
  (fsu_stream_get_type())
#define FSU_STREAM(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_STREAM,                          \
      FsuStream))
#define FSU_STREAM_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_STREAM,                          \
      FsuStreamClass))
#define FSU_IS_STREAM(obj)                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_STREAM))
#define FSU_IS_STREAM_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_STREAM))
#define FSU_STREAM_GET_CLASS(obj)               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_STREAM,                          \
      FsuStreamClass))

typedef struct _FsuStream      FsuStream;
typedef struct _FsuStreamClass FsuStreamClass;
typedef struct _FsuStreamPrivate FsuStreamPrivate;

struct _FsuStreamClass
{
  GObjectClass parent_class;
};

/**
 * FsuStream:
 *
 * A wrapper class around #FsStream
 */
struct _FsuStream
{
  GObject parent;
  /*< private >*/
  FsuStreamPrivate *priv;
};

GType fsu_stream_get_type (void) G_GNUC_CONST;

gboolean fsu_stream_start_sending (FsuStream *self);
void fsu_stream_stop_sending (FsuStream *self);
gboolean fsu_stream_start_receiving (FsuStream *self);
void fsu_stream_stop_receiving (FsuStream *self);

G_END_DECLS

#endif /* __FSU_STREAM_H__ */
