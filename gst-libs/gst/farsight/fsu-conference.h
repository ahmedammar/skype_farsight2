/*
 * fsu-conference.h - Header for FsuConference
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

#ifndef __FSU_CONFERENCE_H__
#define __FSU_CONFERENCE_H__

#include <gst/gst.h>
#include <gst/farsight/fs-conference-iface.h>
#include <gst/farsight/fsu-session.h>
#include <gst/farsight/fsu-source.h>

G_BEGIN_DECLS

#define FSU_TYPE_CONFERENCE                     \
  (fsu_conference_get_type())
#define FSU_CONFERENCE(obj)                     \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_CONFERENCE,                      \
      FsuConference))
#define FSU_CONFERENCE_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_CONFERENCE,                      \
      FsuConferenceClass))
#define FSU_IS_CONFERENCE(obj)                  \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_CONFERENCE))
#define FSU_IS_CONFERENCE_CLASS(klass)          \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_CONFERENCE))
#define FSU_CONFERENCE_GET_CLASS(obj)           \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_CONFERENCE,                      \
      FsuConferenceClass))

typedef struct _FsuConference      FsuConference;
typedef struct _FsuConferenceClass FsuConferenceClass;
typedef struct _FsuConferencePrivate FsuConferencePrivate;

struct _FsuConferenceClass
{
  GObjectClass parent_class;
};

/**
 * FsuConference:
 *
 * A wrapper class around #FsConference
 */
struct _FsuConference
{
  GObject parent;
  /*< private >*/
  FsuConferencePrivate *priv;
};

GType fsu_conference_get_type (void) G_GNUC_CONST;

FsuConference *fsu_conference_new (FsConference *conference,
    GstElement *pipeline,
    GError **error);
FsuSession *fsu_conference_handle_session (FsuConference *self,
    FsSession *session,
    FsuSource *source);
gboolean fsu_conference_handle_message (FsuConference *self,
    GstMessage *message);

G_END_DECLS

#endif /* __FSU_CONFERENCE_H__ */
