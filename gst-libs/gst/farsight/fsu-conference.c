/*
 * fsu-conference.c - Source for FsuConference
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


/**
 * SECTION:fsu-conference
 * @short_description: A wrapper class around #FsConference
 *
 * This class is the main class of Farsight-Utils. It is meant to wrap the
 * #FsConference and add it to the pipeline.
 * Farsight-utils is meant to act as a layer on top of Farsight and GStreamer
 * as it will allow you to set up complex pipelines without having to play too
 * much with the GStreamer API.
 * This class will wrap the #FsConference and take care of the GStreamer part of
 * the process so you can concentrate on the signaling part.
 */



#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/farsight/fsu-conference.h>
#include <gst/farsight/fsu-session-priv.h>

G_DEFINE_TYPE (FsuConference, fsu_conference, G_TYPE_OBJECT);

static void fsu_conference_constructed (GObject *object);
static void fsu_conference_dispose (GObject *object);
static void fsu_conference_finalize (GObject *object);
static void fsu_conference_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_conference_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static void remove_weakref (gpointer data,
    gpointer user_data);

/* properties */
enum
{
  PROP_CONFERENCE = 1,
  PROP_PIPELINE,
  LAST_PROPERTY
};

struct _FsuConferencePrivate
{
  GstElement *pipeline;
  GstElement *conference;
  gboolean created_pipeline;
  GList *sessions;
  gint sessions_id;
  GError *error;
  GMutex *mutex;
};

static void
fsu_conference_class_init (FsuConferenceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuConferencePrivate));

  gobject_class->get_property = fsu_conference_get_property;
  gobject_class->set_property = fsu_conference_set_property;
  gobject_class->constructed = fsu_conference_constructed;
  gobject_class->dispose = fsu_conference_dispose;
  gobject_class->finalize = fsu_conference_finalize;

  /**
   * FsuConference:fs-conference:
   *
   * The #FsConference element being wrapped by this object
   */
  g_object_class_install_property (gobject_class, PROP_CONFERENCE,
      g_param_spec_object ("fs-conference", "The farsight conference",
          "The farsight conference element",
          FS_TYPE_CONFERENCE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * FsuConference:pipeline:
   *
   * The pipeline in which the #FsConference resides
   */
  g_object_class_install_property (gobject_class, PROP_PIPELINE,
      g_param_spec_object ("pipeline", "The pipeline",
          "The pipeline holding the conference",
          GST_TYPE_ELEMENT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_conference_init (FsuConference *self)
{
  FsuConferencePrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_CONFERENCE,
          FsuConferencePrivate);

  self->priv = priv;
  priv->mutex = g_mutex_new ();
}

static void
fsu_conference_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuConference *self = FSU_CONFERENCE (object);
  FsuConferencePrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_CONFERENCE:
      g_value_set_object (value, priv->conference);
      break;
    case PROP_PIPELINE:
      g_value_set_object (value, priv->pipeline);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_conference_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuConference *self = FSU_CONFERENCE (object);
  FsuConferencePrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_PIPELINE:
      priv->pipeline = g_value_dup_object (value);
      break;
    case PROP_CONFERENCE:
      priv->conference = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_conference_constructed (GObject *object)
{
  FsuConference *self = FSU_CONFERENCE (object);
  FsuConferencePrivate *priv = self->priv;

  if (G_OBJECT_CLASS (fsu_conference_parent_class)->constructed)
     G_OBJECT_CLASS (fsu_conference_parent_class)->constructed (object);

  if (!priv->pipeline) {
    priv->pipeline = gst_pipeline_new ("fsu_pipeline");
    priv->created_pipeline = TRUE;
  }

  if (!priv->pipeline)
  {
    g_set_error_literal (&priv->error, FS_ERROR, FS_ERROR_CONSTRUCTION,
        "Couldn't create gstreamer pipeline");
    goto error;
  }

  if (!gst_bin_add (GST_BIN (priv->pipeline), priv->conference))
  {
    gst_object_unref (priv->pipeline);
    priv->pipeline = NULL;
    priv->created_pipeline = FALSE;
    g_set_error_literal (&priv->error, FS_ERROR, FS_ERROR_CONSTRUCTION,
        "Couldn't add fsrtpconference to the pipeline");
    goto error;
  }

  if (gst_element_set_state (priv->pipeline, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
  {
    gst_element_set_state (priv->conference, GST_STATE_NULL);
    gst_bin_remove (GST_BIN (priv->pipeline), priv->conference);
    priv->conference = NULL;
    g_set_error_literal (&priv->error, FS_ERROR, FS_ERROR_CONSTRUCTION,
        "Unable to set pipeline to PLAYING");
    goto error;
  }

 error:
  return;
}

static void
fsu_conference_dispose (GObject *object)
{
  FsuConference *self = FSU_CONFERENCE (object);
  FsuConferencePrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);

  if (priv->sessions)
  {
    g_list_foreach (priv->sessions, remove_weakref, self);
    g_list_free (priv->sessions);
  }
  priv->sessions = NULL;
  priv->sessions_id = -1;

  if (priv->conference)
  {
    gst_element_set_state (priv->conference, GST_STATE_NULL);
    if (priv->pipeline)
      gst_bin_remove (GST_BIN (priv->pipeline), priv->conference);
    else
      gst_object_unref (priv->conference);
  }
  priv->conference = NULL;

  if (priv->pipeline)
  {
    if (priv->created_pipeline)
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
    gst_object_unref (priv->pipeline);
  }
  priv->pipeline = NULL;

  g_mutex_unlock (priv->mutex);

  G_OBJECT_CLASS (fsu_conference_parent_class)->dispose (object);
}

static void
fsu_conference_finalize (GObject *object)
{
  FsuConference *self = FSU_CONFERENCE (object);

  g_mutex_free (self->priv->mutex);

  G_OBJECT_CLASS (fsu_conference_parent_class)->finalize (object);
}


/**
 * fsu_conference_new:
 * @conference: The #FsConference to wrap
 * @pipeline: A Gstreamer pipeline in which to add the @conference.
 * Can be #NULL if you want the #FsuConference to create the pipeline for you.
 * @error: A GError to be returned in case an error occurs while trying to
 * create the pipeline or add the @conference in the @pipeline.
 *
 * This will create a new #FsuConference wrapping @conference and adding it to
 * the @pipeline. Set the pipeline to #NULL to have it created for you, you can
 * then access it through the #FsuConference:pipeline property.
 *
 * Returns: A new #FsuConference or #NULL if an error occured.
 */
FsuConference *
fsu_conference_new (FsConference *conference,
    GstElement *pipeline,
    GError **error)
{
  FsuConference *self = NULL;

  g_return_val_if_fail (conference, NULL);
  g_return_val_if_fail (FS_IS_CONFERENCE (conference), NULL);
  g_return_val_if_fail (!pipeline || GST_IS_PIPELINE (pipeline), NULL);

  self = g_object_new (FSU_TYPE_CONFERENCE,
      "pipeline", pipeline,
      "fs-conference", conference,
      NULL);

  if (self->priv->error != NULL)
  {
    g_propagate_error (error, self->priv->error);
    g_object_unref (self);
    return NULL;
  }

  return self;
}

static void
session_destroyed (gpointer data,
    GObject *destroyed_session)
{
  FsuConference *self = FSU_CONFERENCE (data);
  FsuConferencePrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);
  priv->sessions = g_list_remove (priv->sessions, destroyed_session);
  priv->sessions_id++;
  g_mutex_unlock (priv->mutex);
}

static void
remove_weakref (gpointer data,
    gpointer user_data)
{
  g_object_weak_unref (G_OBJECT (data), session_destroyed, user_data);
}

/**
 * fsu_conference_handle_session:
 * @self: The #FsuConference
 * @session: The #FsSession to handle
 * @source: The #FsuSource to use with the session
 *
 * This will let the #FsuConference handle a new session with the corresponding
 * #FsuSource.
 * The @session will take care of linking the @source when needed.
 *
 * Returns: A new #FsuSession object.
 */
FsuSession *
fsu_conference_handle_session (FsuConference *self,
    FsSession *session,
    FsuSource *source)
{
  FsuConferencePrivate *priv = self->priv;
  FsuSession *sess = _fsu_session_new (self, session, source);

  if (sess)
  {
    g_mutex_lock (priv->mutex);
    priv->sessions = g_list_prepend (priv->sessions, sess);
    priv->sessions_id++;
    g_mutex_unlock (priv->mutex);
    g_object_weak_ref (G_OBJECT (sess), session_destroyed, self);
  }

  return sess;
}

/**
 * fsu_conference_handle_message:
 * @self: The #FsuConference
 * @message: The message to handle
 *
 * This will try to handle a message received on the Gstreamer bus. It will pass
 * the message to all sessions and streams being handled and will dispatch the
 * message to all the #FsuFilterManager objects.
 *
 * Returns: #TRUE if the message was handled and needs to be dropped. #FALSE if
 * the message could not be handled by any session or stream.
 */
gboolean
fsu_conference_handle_message (FsuConference *self,
    GstMessage *message)
{
  FsuConferencePrivate *priv = self->priv;
  GList *i;
  gboolean drop = FALSE;
  gint list_id = 0;

  g_mutex_lock (priv->mutex);
 retry:
  for (i = priv->sessions, list_id = priv->sessions_id;
       i && !drop && list_id == priv->sessions_id;
       i = i->next)
  {
    FsuSession *session = i->data;
    g_mutex_unlock (priv->mutex);
    drop = _fsu_session_handle_message (session, message);
    g_mutex_lock (priv->mutex);
  }
  if (list_id != priv->sessions_id)
    goto retry;
  g_mutex_unlock (priv->mutex);

  return drop;
}
