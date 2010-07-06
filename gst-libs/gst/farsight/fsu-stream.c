/*
 * fsu-stream.c - Source for FsuStream
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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/farsight/fsu-stream.h>
#include <gst/farsight/fsu-stream-priv.h>
#include <gst/farsight/fsu-session-priv.h>
#include <gst/farsight/fsu-multi-filter-manager.h>

G_DEFINE_TYPE (FsuStream, fsu_stream, G_TYPE_OBJECT);

static void fsu_stream_constructed (GObject *object);
static void fsu_stream_dispose (GObject *object);
static void fsu_stream_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_stream_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

static void src_pad_added (FsStream *self,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data);

/* properties */
enum
{
  PROP_CONFERENCE = 1,
  PROP_SESSION,
  PROP_STREAM,
  PROP_SINK,
  PROP_FILTER_MANAGER,
  LAST_PROPERTY
};

struct _FsuStreamPrivate
{
  gboolean dispose_has_run;
  FsuConference *conference;
  FsuSession *session;
  FsStream *stream;
  FsuSink *sink;
  gboolean receiving;
  gboolean sending;
  FsuFilterManager *filters;
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

  g_object_class_install_property (gobject_class, PROP_CONFERENCE,
      g_param_spec_object ("fsu-conference", "Farsight-utils conference",
          "The FSU conference handling this session.",
          FSU_TYPE_CONFERENCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SESSION,
      g_param_spec_object ("fsu-session", "Farsight-utils session",
          "The FSU session containing this stream.",
          FSU_TYPE_SESSION,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STREAM,
      g_param_spec_object ("fs-stream", "Farsight stream",
          "Farsight stream object.",
          FS_TYPE_STREAM,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SINK,
      g_param_spec_object ("sink", "FsuSink",
          "The Farsight-utils sink to use with the stream.",
          FSU_TYPE_SINK,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FILTER_MANAGER,
      g_param_spec_object ("filter-manager", "Filter manager",
          "The filter manager applied on the stream",
          FSU_TYPE_FILTER_MANAGER,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}


static void
fsu_stream_init (FsuStream *self)
{
  FsuStreamPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_STREAM,
          FsuStreamPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->filters = fsu_multi_filter_manager_new ();
}

static void
fsu_stream_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;

  switch (property_id)
  {
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
    case PROP_FILTER_MANAGER:
      g_value_set_object (value, priv->filters);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_stream_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;

  switch (property_id)
  {
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

  if (chain_up)
    chain_up (object);

  g_signal_connect (priv->stream, "src-pad-added",
        G_CALLBACK (src_pad_added), self);

  if (priv->sink)
  {
    g_object_get (priv->conference,
        "pipeline", &pipeline,
        NULL);
    if (!gst_bin_add (GST_BIN (pipeline), GST_ELEMENT (priv->sink)))
    {
      error = "Could not add sink to pipeline";
      gst_object_unref (GST_OBJECT (priv->sink));
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
    gst_object_unref (GST_OBJECT (priv->sink));
  priv->sink = NULL;

  G_OBJECT_CLASS (fsu_stream_parent_class)->dispose (object);
}

FsuStream *
_fsu_stream_new (FsuConference *conference,
    FsuSession *session,
    FsStream *stream,
    FsuSink *sink)
{
  g_return_val_if_fail (conference, NULL);
  g_return_val_if_fail (FSU_IS_CONFERENCE (conference), NULL);
  g_return_val_if_fail (session, NULL);
  g_return_val_if_fail (FSU_IS_SESSION (session), NULL);
  g_return_val_if_fail (stream, NULL);
  g_return_val_if_fail (FS_IS_STREAM (stream), NULL);

  return g_object_new (FSU_TYPE_STREAM,
      "fsu-conference", conference,
      "fsu-session", session,
      "fs-stream", stream,
      "sink", sink,
      NULL);
}



static void
src_pad_added (FsStream *stream,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data)
{
  FsuStream *self = user_data;
  FsuStreamPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstElement *sink = GST_ELEMENT (priv->sink);
  GstPad *sink_pad = NULL;
  GstPad *filter_pad = NULL;
  GstPadLinkReturn ret;
  gchar *error = NULL;

  g_object_get (priv->conference,
      "pipeline", &pipeline,
      NULL);

  if (!sink)
  {
    sink = gst_element_factory_make ("fakesink", NULL);
    if (!sink)
    {
      error = "No sink selected, and can't create fakesink";
      goto error;
    }
    if (!gst_bin_add (GST_BIN (pipeline), sink))
    {
      error = "Could not add fakesink to pipeline";
      gst_object_unref (sink);
      goto error;
    }
    sink_pad = gst_element_get_static_pad (sink, "sink");
  }
  else
  {
    sink_pad = gst_element_get_request_pad (sink, "sink%d");
  }

  if (!sink_pad)
  {
    error = "Could not request sink pad from Sink";
    goto error;
  }

  filter_pad = fsu_filter_manager_apply (priv->filters,
      GST_BIN (pipeline), sink_pad);

  if (!filter_pad)
    filter_pad = sink_pad;

  ret = gst_pad_link (pad, filter_pad);
  gst_object_unref (filter_pad);

  if (ret != GST_PAD_LINK_OK)
  {
    error = "Could not link colorspace to fsrtpconference sink pad";
    goto error;
  }

  if (gst_element_set_state (sink, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
  {
    error = "Unable to set audio_sink to PLAYING";
    goto error;
  }

  /* TODO: signal_connect("unlinked") on the pad to destroy the pipeline if the
     src pad gets unlinked (to be removed) */

  return;
 error:
  /* TODO: signal error*/
  return;
}

gboolean
fsu_stream_start_sending (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  FsStreamDirection direction;
  FsStreamDirection new_direction;

  if (priv->sending)
    return TRUE;

  priv->sending = TRUE;
  g_object_get (priv->stream, "direction", &direction, NULL);

  new_direction = (FsStreamDirection)(direction  | FS_DIRECTION_SEND);

  if (new_direction != direction)
    g_object_set (priv->stream, "direction", new_direction, NULL);

  return _fsu_session_start_sending (priv->session);
}

void
fsu_stream_stop_sending (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  FsStreamDirection direction;
  FsStreamDirection new_direction;

  if (!priv->sending)
    return;

  priv->sending = FALSE;
  g_object_get (priv->stream, "direction", &direction, NULL);

  new_direction = (FsStreamDirection)(direction & ~FS_DIRECTION_SEND);

  if (new_direction != direction)
    g_object_set (priv->stream, "direction", new_direction, NULL);

  _fsu_session_stop_sending (priv->session);
}

gboolean
fsu_stream_start_receiving (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  FsStreamDirection direction;
  FsStreamDirection new_direction;
  gboolean ret = TRUE;

  if (!priv->receiving)
  {
    GstIterator *iter = NULL;
    gboolean done = FALSE;

    priv->receiving = TRUE;
    iter = fs_stream_get_src_pads_iterator (priv->stream);
    while (!done)
    {
      gpointer pad;
      switch (gst_iterator_next (iter, &pad))
      {
        case GST_ITERATOR_OK:
          src_pad_added (priv->stream, pad, NULL, self);
          /* TODO: check for errors */
          gst_object_unref (pad);
          break;
        case GST_ITERATOR_RESYNC:
          fsu_stream_stop_receiving (self);
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


  g_object_get (priv->stream, "direction", &direction, NULL);

  new_direction = (FsStreamDirection)(direction | FS_DIRECTION_RECV);

  if (new_direction != direction)
    g_object_set (priv->stream, "direction", new_direction, NULL);

  return ret;
}

void
fsu_stream_stop_receiving (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  FsStreamDirection direction;
  FsStreamDirection new_direction;
  GstIterator *iter = NULL;
  gboolean done = FALSE;

  priv->receiving = FALSE;
  iter = fs_stream_get_src_pads_iterator (priv->stream);
  while (!done)
  {
    gpointer pad;
    switch (gst_iterator_next (iter, &pad))
    {
      case GST_ITERATOR_OK:
        {
          GstPad *filter_pad = gst_pad_get_peer (pad);
          if (filter_pad)
          {
            GstElement *pipeline = NULL;
            GstPad *sink_pad = NULL;

            g_object_get (priv->conference,
                "pipeline", &pipeline,
                NULL);
            gst_pad_unlink (pad, filter_pad);
            sink_pad = fsu_filter_manager_revert (priv->filters,
                GST_BIN (pipeline), filter_pad);

            if (!sink_pad)
              sink_pad = filter_pad;
            else
              gst_object_unref (filter_pad);

            if (priv->sink)
            {
              gst_element_release_request_pad (GST_ELEMENT (priv->sink),
                  sink_pad);
              gst_object_unref (sink_pad);
            }
            else if (!priv->sink)
            {
              GstObject *sink = gst_object_get_parent (GST_OBJECT (sink_pad));
              gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (sink));
              gst_object_unref (sink);
              gst_object_unref (sink_pad);
            }
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

  g_object_get (priv->stream, "direction", &direction, NULL);

  new_direction = (FsStreamDirection)(direction & ~FS_DIRECTION_RECV);

  if (new_direction != direction)
    g_object_set (priv->stream, "direction", new_direction, NULL);

  gst_iterator_free (iter);
}

gboolean
_fsu_stream_handle_message (FsuStream *self,
    GstMessage *message)
{
  FsuStreamPrivate *priv = self->priv;
  return fsu_filter_manager_handle_message (priv->filters, message);
}
