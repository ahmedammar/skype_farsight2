/*
 * fsu-session.c - Source for FsuSession
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

#include "fsu-session.h"


G_DEFINE_TYPE (FsuSession, fsu_session, G_TYPE_OBJECT);

static void fsu_session_constructed (GObject *object);
static void fsu_session_dispose (GObject *object);
static void fsu_session_finalize (GObject *object);
static void fsu_session_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_session_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);

/* properties */
enum {
  PROP_CONFERENCE = 1,
  PROP_SESSION,
  PROP_SOURCE,
  LAST_PROPERTY
};

struct _FsuSessionPrivate
{
  gboolean dispose_has_run;
  FsuConference *conference;
  FsSession *session;
  GstElement *source;
};

static void
fsu_session_class_init (FsuSessionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuSessionPrivate));

  gobject_class->get_property = fsu_session_get_property;
  gobject_class->set_property = fsu_session_set_property;
  gobject_class->constructed = fsu_session_constructed;
  gobject_class->dispose = fsu_session_dispose;
  gobject_class->finalize = fsu_session_finalize;

  g_object_class_install_property (gobject_class, PROP_SESSION,
      g_param_spec_object ("conference", "Farsight-utils conference",
          "The FSU conference handling this session.",
          FSU_TYPE_CONFERENCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SESSION,
      g_param_spec_object ("session", "Farsight session",
          "Farsight session object.",
          FS_TYPE_SESSION,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOURCE,
      g_param_spec_object ("source", "Gstreamer source",
          "The source to use with the session.",
          GST_TYPE_ELEMENT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_session_init (FsuSession *self)
{
  FsuSessionPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SESSION,
          FsuSessionPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
}

static void
fsu_session_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONFERENCE:
      g_value_set_object (value, priv->conference);
      break;
    case PROP_SESSION:
      g_value_set_object (value, priv->session);
      break;
    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_session_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONFERENCE:
      priv->conference = g_value_dup_object (value);
      break;
    case PROP_SESSION:
      priv->session = g_value_dup_object (value);
      break;
    case PROP_SOURCE:
      priv->source = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_session_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
      G_OBJECT_CLASS (fsu_session_parent_class)->constructed;
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstPad *srcpad = NULL;
  GstPad *sinkpad = NULL;
  gchar *error;

  if (chain_up != NULL)
    chain_up (object);

  g_object_get (priv->conference,
      "pipeline", &pipeline,
      NULL);

  if (priv->source == NULL)
    goto no_source;

  if (gst_bin_add (GST_BIN (pipeline), priv->source) == FALSE) {
    error = "Couldn't add audio_source to pipeline";
    goto no_source;
  }

  srcpad = gst_element_get_static_pad (priv->source, "src");
  if (srcpad == NULL)
    srcpad = gst_element_get_request_pad (priv->source, "src%d");
  /* TODO: keep track if we requested a pad or not, so we can release it */

  if (srcpad == NULL) {
    error = "Couldn't request pad from Source";
    goto no_source;
  }

  g_object_get (priv->session,
      "sink-pad", &sinkpad,
      NULL);

  if (gst_pad_link (srcpad, sinkpad) != GST_PAD_LINK_OK) {
    gst_object_unref (sinkpad);
    gst_object_unref (srcpad);
    error = "Couldn't link the volume/level/src to fsrtpconference";
    goto no_source;
  }

  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);

  //fsu_source_start (priv->source);

  return;
 no_source:
  /* TODO: signal/other error */
  if (priv->source != NULL)
    gst_object_unref (priv->source);
  priv->source = NULL;

}
static void
fsu_session_dispose (GObject *object)
{
  FsuSession *self = (FsuSession *)object;
  FsuSessionPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->conference)
    gst_object_unref (priv->conference);
  priv->conference = NULL;

  if (priv->session)
    gst_object_unref (priv->session);
  priv->session = NULL;

  /* TODO: release requested pads */

  if (priv->source) {
    //fsu_source_stop (priv->source);
    gst_object_unref (priv->source);
  }
  priv->source = NULL;

  G_OBJECT_CLASS (fsu_session_parent_class)->dispose (object);
}

static void
fsu_session_finalize (GObject *object)
{
  FsuSession *self = (FsuSession *)object;

  (void)self;
  G_OBJECT_CLASS (fsu_session_parent_class)->finalize (object);
}


FsuSession *
fsu_session_new (FsuConference *conference,
    FsSession *session, GstElement *source)
{

  g_return_val_if_fail (conference != NULL, NULL);
  g_return_val_if_fail (session != NULL, NULL);

  return g_object_new (FSU_TYPE_SESSION,
      "conference", conference,
      "session", session,
      "source", source,
      NULL);
}

FsuStream *
fsu_session_handle_stream (FsuSession *self, FsStream *stream, GstElement *sink)
{
  return fsu_stream_new (self->priv->conference, self, stream, sink);
}
