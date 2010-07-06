/*
 * fsu-session.c - Source for FsuSession
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


#include <gst/farsight/fsu-session.h>
#include <gst/farsight/fsu-session-priv.h>
#include <gst/farsight/fsu-stream-priv.h>
#include <gst/farsight/fsu-single-filter-manager.h>

G_DEFINE_TYPE (FsuSession, fsu_session, G_TYPE_OBJECT);

static void fsu_session_constructed (GObject *object);
static void fsu_session_dispose (GObject *object);
static void fsu_session_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_session_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);

static void remove_weakref (gpointer data,
    gpointer user_data);

/* properties */
enum
{
  PROP_CONFERENCE = 1,
  PROP_SESSION,
  PROP_SOURCE,
  PROP_FILTER_MANAGER,
  LAST_PROPERTY
};

struct _FsuSessionPrivate
{
  gboolean dispose_has_run;
  FsuConference *conference;
  FsSession *session;
  FsuSource *source;
  GList *streams;
  FsuFilterManager *filters;
  guint sending;
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

  g_object_class_install_property (gobject_class, PROP_CONFERENCE,
      g_param_spec_object ("fsu-conference", "Farsight-utils conference",
          "The FSU conference handling this session.",
          FSU_TYPE_CONFERENCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SESSION,
      g_param_spec_object ("fs-session", "Farsight session",
          "Farsight session object.",
          FS_TYPE_SESSION,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOURCE,
      g_param_spec_object ("source", "FsuSource",
          "The Farsight-utils source to use with the session.",
          FSU_TYPE_SOURCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FILTER_MANAGER,
      g_param_spec_object ("filter-manager", "Filter manager",
          "The filter manager applied on the session",
          FSU_TYPE_FILTER_MANAGER,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
fsu_session_init (FsuSession *self)
{
  FsuSessionPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SESSION,
          FsuSessionPrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
  priv->filters = fsu_single_filter_manager_new ();
}

static void
fsu_session_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_CONFERENCE:
      g_value_set_object (value, priv->conference);
      break;
    case PROP_SESSION:
      g_value_set_object (value, priv->session);
      break;
    case PROP_SOURCE:
      g_value_set_object (value, priv->source);
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
fsu_session_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;

  switch (property_id)
  {
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

  if (chain_up)
    chain_up (object);

  if (priv->source)
  {
    g_object_get (priv->conference,
        "pipeline", &pipeline,
        NULL);

    if (!gst_bin_add (GST_BIN (pipeline), GST_ELEMENT (priv->source)))
    {
      /* TODO: signal/other error */
      gst_object_unref (GST_OBJECT (priv->source));
      priv->source = NULL;
    }
  }

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

  if (priv->streams)
  {
    g_list_foreach (priv->streams, remove_weakref, self);
    g_list_free (priv->streams);
  }
  priv->streams = NULL;

  /* TODO: release requested pads */

  if (priv->source)
  {
    //fsu_source_stop (priv->source);
    gst_object_unref (GST_OBJECT (priv->source));
  }
  priv->source = NULL;

  G_OBJECT_CLASS (fsu_session_parent_class)->dispose (object);
}


FsuSession *
_fsu_session_new (FsuConference *conference,
    FsSession *session,
    FsuSource *source)
{

  g_return_val_if_fail (conference, NULL);
  g_return_val_if_fail (FSU_IS_CONFERENCE (conference), NULL);
  g_return_val_if_fail (session, NULL);
  g_return_val_if_fail (FS_IS_SESSION (session), NULL);

  return g_object_new (FSU_TYPE_SESSION,
      "fsu-conference", conference,
      "fs-session", session,
      "source", source,
      NULL);
}

static void
stream_destroyed (gpointer data,
    GObject *destroyed_stream)
{
  FsuSession *self = FSU_SESSION (data);
  FsuSessionPrivate *priv = self->priv;

  priv->streams = g_list_remove (priv->streams, destroyed_stream);
}

static void
remove_weakref (gpointer data,
    gpointer user_data)
{
  g_object_weak_unref (G_OBJECT (data), stream_destroyed, user_data);
}

FsuStream *
fsu_session_handle_stream (FsuSession *self,
    FsStream *stream,
    FsuSink *sink)
{
  FsuSessionPrivate *priv = self->priv;
  FsuStream *str = _fsu_stream_new (priv->conference, self, stream, sink);

  if (str)
  {
    priv->streams = g_list_prepend (priv->streams, str);
    g_object_weak_ref (G_OBJECT (str), stream_destroyed, self);
  }

  return str;
}


gboolean
_fsu_session_start_sending (FsuSession *self)
{
  FsuSessionPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstPad *filter_pad = NULL;
  GstPad *srcpad = NULL;
  GstPad *sinkpad = NULL;
  gchar *error;

  if (!priv->source)
    goto no_source;

  if (priv->sending > 0)
    return TRUE;

  g_object_get (priv->conference,
      "pipeline", &pipeline,
      NULL);

  srcpad = gst_element_get_request_pad (GST_ELEMENT (priv->source), "src%d");

  if (!srcpad)
  {
    error = "Couldn't request pad from Source";
    gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (priv->source));
    goto no_source;
  }

  filter_pad = fsu_filter_manager_apply (priv->filters,
      GST_BIN (pipeline), srcpad);
  if (!filter_pad)
  {
    error = "Couldn't add filter manager";
    gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (priv->source));
    /* TODO: release pad if requested */
    goto no_source;
  }
  gst_object_unref (srcpad);

  g_object_get (priv->session,
      "sink-pad", &sinkpad,
      NULL);

  if (gst_pad_link (filter_pad, sinkpad) != GST_PAD_LINK_OK)
  {
    gst_object_unref (sinkpad);
    gst_object_unref (filter_pad);
    gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (priv->source));
    error = "Couldn't link the volume/level/src to fsrtpconference";
    goto no_source;
  }

  gst_object_unref (sinkpad);

  if (gst_element_set_state (GST_ELEMENT (priv->source), GST_STATE_READY) !=
      GST_STATE_CHANGE_SUCCESS)
  {
    error = "Couldn't set source to READY";
    goto no_source;
  }
  if (GST_STATE (pipeline) > GST_STATE_NULL)
    gst_element_sync_state_with_parent (GST_ELEMENT (priv->source));

  priv->sending++;

  return TRUE;
 no_source:
  /* TODO: signal/other error */
  return FALSE;
}

void
_fsu_session_stop_sending (FsuSession *self)
{
  FsuSessionPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstPad *filter_pad = NULL;
  GstPad *srcpad = NULL;
  GstPad *sinkpad = NULL;

  if (priv->sending > 0)
  {
    priv->sending--;
    if (priv->sending == 0)
    {
      g_object_get (priv->conference,
          "pipeline", &pipeline,
          NULL);
      g_object_get (priv->session,
          "sink-pad", &sinkpad,
          NULL);

      filter_pad = gst_pad_get_peer (sinkpad);
      gst_pad_unlink (filter_pad, sinkpad);
      srcpad = fsu_filter_manager_revert (priv->filters,
          GST_BIN (pipeline), filter_pad);
      /* TODO: release request pad */
      gst_element_set_state (GST_ELEMENT (priv->source), GST_STATE_NULL);
    }
  }
}

gboolean
_fsu_session_handle_message (FsuSession *self,
    GstMessage *message)
{
  FsuSessionPrivate *priv = self->priv;
  GList *i;
  gboolean drop = FALSE;

  drop = fsu_filter_manager_handle_message (priv->filters, message);

  for (i = priv->streams; i && !drop; i = i->next)
  {
    FsuStream *stream = i->data;
    drop = _fsu_stream_handle_message (stream, message);
  }

  return drop;
}
