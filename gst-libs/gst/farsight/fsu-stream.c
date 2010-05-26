/*
 * fsu-stream.c - Source for FsuStream
 *
 * Copyright (C) 2010 Collabora Ltd.
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

#include "fsu-stream.h"
#include "fsu-session-priv.h"


G_DEFINE_TYPE (FsuStream, fsu_stream, G_TYPE_OBJECT);

static void fsu_stream_constructed (GObject *object);
static void fsu_stream_dispose (GObject *object);
static void fsu_stream_finalize (GObject *object);
static void fsu_stream_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec);
static void fsu_stream_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec);

static void src_pad_added (FsStream *self, GstPad *pad,
    FsCodec *codec, gpointer user_data);

/* properties */
enum {
  PROP_CONFERENCE = 1,
  PROP_SESSION,
  PROP_STREAM,
  PROP_SINK,
  LAST_PROPERTY
};

struct _FsuStreamPrivate
{
  gboolean dispose_has_run;
  FsuConference *conference;
  FsuSession *session;
  FsStream *stream;
  GstElement *sink;
  gboolean receiving;
};

static void
fsu_stream_class_init (FsuStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuStreamPrivate));

  gobject_class->get_property = fsu_stream_get_property;
  gobject_class->set_property = fsu_stream_set_property;
  gobject_class->constructed = fsu_stream_constructed;
  gobject_class->dispose = fsu_stream_dispose;
  gobject_class->finalize = fsu_stream_finalize;

  g_object_class_install_property (gobject_class, PROP_CONFERENCE,
      g_param_spec_object ("conference", "Farsight-utils conference",
          "The FSU conference handling this session.",
          FSU_TYPE_CONFERENCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SESSION,
      g_param_spec_object ("session", "Farsight-utils session",
          "The FSU session containing this stream.",
          FSU_TYPE_SESSION,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STREAM,
      g_param_spec_object ("stream", "Farsight stream",
          "Farsight stream object.",
          FS_TYPE_STREAM,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SINK,
      g_param_spec_object ("sink", "Gstreamer sink",
          "The sink to use with the stream.",
          GST_TYPE_ELEMENT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}


static void
fsu_stream_init (FsuStream *self)
{
  FsuStreamPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_STREAM,
          FsuStreamPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
}

static void
fsu_stream_get_property (GObject *object,
    guint property_id, GValue *value, GParamSpec *pspec)
{
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONFERENCE:
      g_value_set_object (value, priv->conference);
      break;
    case PROP_SESSION:
      g_value_set_object (value, priv->session);
      break;
    case PROP_STREAM:
      g_value_set_object (value, priv->stream);
      break;
    case PROP_SINK:
      g_value_set_object (value, priv->sink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_stream_set_property (GObject *object,
    guint property_id, const GValue *value, GParamSpec *pspec)
{
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;

  switch (property_id) {
    case PROP_CONFERENCE:
      priv->conference = g_value_dup_object (value);
      break;
    case PROP_SESSION:
      priv->session = g_value_dup_object (value);
      break;
    case PROP_STREAM:
      priv->stream = g_value_dup_object (value);
      break;
    case PROP_SINK:
      priv->sink = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_stream_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
      G_OBJECT_CLASS (fsu_stream_parent_class)->constructed;
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  gchar *error = NULL;

  if (chain_up != NULL)
    chain_up (object);

  g_signal_connect (priv->stream, "src-pad-added",
        G_CALLBACK (src_pad_added), self);

  if (priv->sink) {
    g_object_get (priv->conference,
        "pipeline", &pipeline,
        NULL);
    if (gst_bin_add (GST_BIN (pipeline), priv->sink) == FALSE)  {
      error = "Could not add sink to pipeline";
      gst_object_unref (priv->sink);
      priv->sink = NULL;
      goto error;
    }
  }

  return;
 error:
  /* TODO: signal/other error */
  return;
}


static void
fsu_stream_dispose (GObject *object)
{
  FsuStream *self = (FsuStream *)object;
  FsuStreamPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->conference)
    gst_object_unref (priv->conference);
  priv->conference = NULL;

  if (priv->session)
    gst_object_unref (priv->session);
  priv->session = NULL;

  if (priv->stream)
    gst_object_unref (priv->stream);
  priv->stream = NULL;

  /* TODO: release requested pads */
  if (priv->sink)
    gst_object_unref (priv->sink);
  priv->sink = NULL;

  G_OBJECT_CLASS (fsu_stream_parent_class)->dispose (object);
}

static void
fsu_stream_finalize (GObject *object)
{
  FsuStream *self = (FsuStream *)object;

  (void)self;

  G_OBJECT_CLASS (fsu_stream_parent_class)->finalize (object);
}


FsuStream *
fsu_stream_new (FsuConference *conference, FsuSession *session,
    FsStream *stream, GstElement *sink)
{
  g_return_val_if_fail (conference != NULL, NULL);
  g_return_val_if_fail (session != NULL, NULL);
  g_return_val_if_fail (stream != NULL, NULL);

  return g_object_new (FSU_TYPE_STREAM,
      "conference", conference,
      "session", session,
      "stream", stream,
      "sink", sink,
      NULL);
}



static void
src_pad_added (FsStream *stream, GstPad *pad,
    FsCodec *codec, gpointer user_data)
{
  FsuStream *self = user_data;
  FsuStreamPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstElement *sink = priv->sink;
  GstPad *sinkpad = NULL;
  GstPadLinkReturn ret;
  gchar *error = NULL;

  if (sink == NULL) {
    g_object_get (priv->conference,
        "pipeline", &pipeline,
        NULL);

    sink = gst_element_factory_make ("fakesink", NULL);
    if (sink == NULL) {
      error = "No sink selected, and can't create fakesink";
      goto error;
    }
    if (gst_bin_add (GST_BIN (pipeline), sink) == FALSE)  {
      error = "Could not add fakesink to pipeline";
      gst_object_unref (sink);
      goto error;
    }
  }

  sinkpad = gst_element_get_static_pad (sink, "sink");
  if (sinkpad == NULL)
    sinkpad = gst_element_get_request_pad (sink, "sink%d");
  /* TODO: keep track if we requested a pad or not, so we can release it */

  if (sinkpad == NULL) {
    error = "Could not request sink pad from Sink";
    goto error;
  }

  ret = gst_pad_link (pad, sinkpad);
  gst_object_unref (sinkpad);

  if (ret != GST_PAD_LINK_OK)  {
    error = "Could not link colorspace to fsrtpconference sink pad";
    goto error;
  }

  if (gst_element_set_state (sink, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE) {
    error = "Unable to set audio_sink to PLAYING";
    goto error;
  }

  return;
 error:
  /* TODO: signal error*/
  return;
}

gboolean
fsu_stream_start_sending (FsuStream *self)
{
  return fsu_session_start_sending (self->priv->session);
}

void
fsu_stream_stop_sending (FsuStream *self)
{
  return fsu_session_stop_sending (self->priv->session);
}

gboolean
fsu_stream_start_receiving (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  gboolean ret = TRUE;

  if (priv->receiving == FALSE) {
    GstIterator *iter = NULL;
    gboolean done = FALSE;

    priv->receiving = TRUE;
    iter = fs_stream_get_src_pads_iterator (priv->stream);
    while (!done) {
      gpointer pad;
      switch (gst_iterator_next (iter, &pad)) {
        case GST_ITERATOR_OK:
          src_pad_added (priv->stream, pad, NULL, self);
          /* TODO: check for errors */
          gst_object_unref (pad);
          break;
        case GST_ITERATOR_RESYNC:
          fsu_stream_start_receiving (self);
          priv->receiving = TRUE;
          gst_iterator_resync (iter);
          break;
        case GST_ITERATOR_ERROR:
          done = TRUE;
          ret = FALSE;
          break;
        case GST_ITERATOR_DONE:
          done = TRUE;
          break;
      }
    }
    gst_iterator_free (iter);
  }

  return ret;
}

void
fsu_stream_stop_receiving (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  GstIterator *iter = NULL;
  gboolean done = FALSE;

  priv->receiving = FALSE;
  iter = fs_stream_get_src_pads_iterator (priv->stream);
  while (!done) {
    gpointer pad;
    switch (gst_iterator_next (iter, &pad)) {
      case GST_ITERATOR_OK:
        {
          GstPad *peer_pad = gst_pad_get_peer (pad);
          if (peer_pad != NULL) {
            gst_pad_unlink (pad, peer_pad);
            gst_object_unref (peer_pad);
          }

          gst_object_unref (pad);
        }
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        done = TRUE;
        break;
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
    }
  }
  gst_iterator_free (iter);
}
