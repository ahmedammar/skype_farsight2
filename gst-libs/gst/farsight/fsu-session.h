/*
 * fsu-session.h - Header for FsuSession
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

#ifndef __FSU_SESSION_H__
#define __FSU_SESSION_H__

#include <glib-object.h>
#include <gst/farsight/fs-conference-iface.h>

typedef struct _FsuSession      FsuSession;

#include "fsu-conference.h"
#include "fsu-stream.h"
#include "fsu-filter.h"
#include "fsu-source.h"
#include "fsu-sink.h"


G_BEGIN_DECLS

#define FSU_TYPE_SESSION                        \
  (fsu_session_get_type())
#define FSU_SESSION(obj)                        \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_SESSION,                         \
      FsuSession))
#define FSU_SESSION_CLASS(klass)                \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_SESSION,                         \
      FsuSessionClass))
#define FSU_IS_SESSION(obj)                     \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_SESSION))
#define FSU_IS_SESSION_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_SESSION))
#define FSU_SESSION_GET_CLASS(obj)              \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_SESSION,                         \
      FsuSessionClass))

typedef struct _FsuSessionClass FsuSessionClass;
typedef struct _FsuSessionPrivate FsuSessionPrivate;

struct _FsuSessionClass
{
  GObjectClass parent_class;
};

struct _FsuSession
{
  GObject parent;
  FsuSessionPrivate *priv;
};

GType fsu_session_get_type (void) G_GNUC_CONST;

FsuSession *fsu_session_new (FsuConference *conference,
    FsSession *session, FsuSource *source);
FsuStream *fsu_session_handle_stream (FsuSession *self,
    FsStream *stream, FsuSink *sink);

G_END_DECLS

#endif /* __FSU_SESSION_H__ */
