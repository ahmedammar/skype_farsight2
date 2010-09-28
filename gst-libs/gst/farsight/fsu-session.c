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

/**
 * SECTION:fsu-session
 * @short_description: A wrapper class around #FsSession
 *
 * This class is part of Farsight-Utils. It is meant to wrap the
 * #FsSession and link the source with the #FsConference.
 * It also provides access to a #FsuFilterManager that sits between the source
 * and the sink pad of the #FsSession in the conference.
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/farsight/fsu-session.h>
#include <gst/farsight/fsu-session-priv.h>
#include <gst/farsight/fsu-stream-priv.h>
#include <gst/filters/fsu-single-filter-manager.h>
#include "fs-marshal.h"

G_DEFINE_TYPE (FsuSession, fsu_session, G_TYPE_OBJECT);

static void fsu_session_constructed (GObject *object);
static void fsu_session_dispose (GObject *object);
static void fsu_session_finalize (GObject *object);
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

/* signals  */
enum {
  SIGNAL_NO_SOURCES_AVAILABLE,
  SIGNAL_SOURCE_CHOSEN,
  SIGNAL_SOURCE_DESTROYED,
  SIGNAL_SOURCE_ERROR,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

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
  FsuConference *conference;
  FsSession *session;
  FsuSource *source;
  GList *streams;
  gint streams_id;
  FsuFilterManager *filters;
  gint sending;
  GMutex *mutex;
  GMutex *pipeline_mutex;
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

  /**
   * FsuSession::no-sources-available:
   *
   * This signal is sent when the source cannot find any suitable source
   * device for capture.
   */
  signals[SIGNAL_NO_SOURCES_AVAILABLE] = g_signal_new ("no-sources-available",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * FsuSession::source-chosen:
   * @source_element: The #GstElement of the chosen source
   * @source_name: The name of the chosen element
   * @source_device: The chosen device
   * @source_device_name: The chosen user-friendly device name
   *
   * This signal is sent when the source has been chosen and correctly opened
   * and ready for capture.
   */
  signals[SIGNAL_SOURCE_CHOSEN] = g_signal_new ("source-chosen",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _fs_marshal_VOID__OBJECT_STRING_STRING_STRING,
      G_TYPE_NONE, 4,
      GST_TYPE_ELEMENT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * FsuSession::source-destroyed:
   *
   * This signal is sent when the source has been destroyed.
   */
  signals[SIGNAL_SOURCE_DESTROYED] = g_signal_new ("source-destroyed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * FsuSession::source-error:
   * @error: The #GError received from the source
   *
   * This signal is sent when the source received a Resource error. This will
   * usually only be sent when the device is no longer available.
   */
  signals[SIGNAL_SOURCE_ERROR] = g_signal_new ("source-error",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__BOXED,
      G_TYPE_NONE, 1,
      GST_TYPE_G_ERROR);
}

static void
fsu_session_init (FsuSession *self)
{
  FsuSessionPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_SESSION,
          FsuSessionPrivate);

  self->priv = priv;
  priv->filters = fsu_single_filter_manager_new ();
  priv->mutex = g_mutex_new ();
  priv->pipeline_mutex = g_mutex_new ();
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
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;
  GstElement *pipeline = NULL;

  if (G_OBJECT_CLASS (fsu_session_parent_class)->constructed)
    G_OBJECT_CLASS (fsu_session_parent_class)->constructed (object);

  if (priv->source)
  {
    GstObject *parent = gst_object_get_parent (GST_OBJECT (priv->source));

    g_object_get (priv->conference,
        "pipeline", &pipeline,
        NULL);

    /* If the source has a parent and it's the pipeline, then skip it,
     * if it has a parent and it's not a pipeline, then unref/NULL it
     * otherwise, try to add it and unref/NULL it if it failed
     */
    if ((!parent || parent != GST_OBJECT (pipeline)) &&
        ((parent && parent != GST_OBJECT (pipeline)) ||
            !gst_bin_add (GST_BIN (pipeline), GST_ELEMENT (priv->source))))
    {
      gst_object_unref (GST_OBJECT (priv->source));
      priv->source = NULL;
    }

    gst_object_unref (pipeline);

    if (parent)
      gst_object_unref (parent);
  }

}

static void
fsu_session_dispose (GObject *object)
{
  FsuSession *self = FSU_SESSION (object);
  FsuSessionPrivate *priv = self->priv;

  while (_fsu_session_stop_sending (self));

  g_mutex_lock (priv->mutex);

  if (priv->source)
  {
    if (GST_OBJECT_REFCOUNT(priv->source) == 1)
    {
      GstElement *pipeline = NULL;

      g_object_get (priv->conference,
          "pipeline", &pipeline,
          NULL);

      gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (priv->source));
      gst_object_unref (pipeline);
      gst_element_set_state (GST_ELEMENT (priv->source), GST_STATE_NULL);
    }

    gst_object_unref (GST_OBJECT (priv->source));
  }
  priv->source = NULL;

  if (priv->filters)
    g_object_unref (priv->filters);
  priv->filters = NULL;

  if (priv->streams)
  {
    g_list_foreach (priv->streams, remove_weakref, self);
    g_list_free (priv->streams);
  }
  priv->streams = NULL;
  priv->streams_id = -1;

  if (priv->session)
    gst_object_unref (priv->session);
  priv->session = NULL;

  if (priv->conference)
    gst_object_unref (priv->conference);
  priv->conference = NULL;

  g_mutex_unlock (priv->mutex);

  G_OBJECT_CLASS (fsu_session_parent_class)->dispose (object);
}

static void
fsu_session_finalize (GObject *object)
{
  FsuSession *self = FSU_SESSION (object);

  g_mutex_free (self->priv->mutex);
  g_mutex_free (self->priv->pipeline_mutex);

  G_OBJECT_CLASS (fsu_session_parent_class)->finalize (object);
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
  g_return_val_if_fail (!source || FSU_IS_SOURCE (source), NULL);

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

  g_mutex_lock (priv->mutex);
  priv->streams = g_list_remove (priv->streams, destroyed_stream);
  priv->streams_id++;
  g_mutex_unlock (priv->mutex);
}

static void
remove_weakref (gpointer data,
    gpointer user_data)
{
  g_object_weak_unref (G_OBJECT (data), stream_destroyed, user_data);
}

/**
 * fsu_session_handle_stream:
 * @self: The #FsuSession
 * @stream: The #FsStream to handle
 * @sink: The #FsuSink to use with the stream
 *
 * This will let the #FsuSession handle a new stream with the corresponding
 * #FsuSink.
 * The @stream will take care of linking the @sink when needed
 *
 * Returns: A new #FsuStream object.
 */
FsuStream *
fsu_session_handle_stream (FsuSession *self,
    FsStream *stream,
    FsuSink *sink)
{
  FsuSessionPrivate *priv = self->priv;
  FsuStream *str = _fsu_stream_new (priv->conference, self, stream, sink);

  if (str)
  {
    g_mutex_lock (priv->mutex);
    priv->streams = g_list_prepend (priv->streams, str);
    priv->streams_id++;
    g_mutex_unlock (priv->mutex);
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
  GstState state = GST_STATE_NULL;

  if (!priv->source)
    goto no_source;

  if (g_atomic_int_get (&priv->sending) > 0)
    goto done;

  g_object_get (priv->conference,
      "pipeline", &pipeline,
      NULL);

  g_mutex_lock (priv->pipeline_mutex);

  srcpad = gst_element_get_request_pad (GST_ELEMENT (priv->source), "src%d");

  if (!srcpad)
    goto no_source;

  filter_pad = fsu_filter_manager_apply (priv->filters,
      GST_BIN (pipeline), srcpad);

  if (!filter_pad)
    filter_pad = gst_object_ref (srcpad);
  gst_object_unref (srcpad);

  g_object_get (priv->session,
      "sink-pad", &sinkpad,
      NULL);

  if (GST_PAD_LINK_FAILED (gst_pad_link (filter_pad, sinkpad)))
    goto error_link;

  if (gst_element_set_state (GST_ELEMENT (priv->source), GST_STATE_READY) !=
      GST_STATE_CHANGE_SUCCESS)
    goto error_state;

  gst_element_get_state (pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  if (state > GST_STATE_NULL &&
      !gst_element_sync_state_with_parent (GST_ELEMENT (priv->source)))
    goto error_state;

  gst_object_unref (filter_pad);
  gst_object_unref (sinkpad);

 done:
  g_mutex_unlock (priv->pipeline_mutex);

  g_atomic_int_inc (&priv->sending);

  return TRUE;

 error_state:
  gst_pad_unlink (filter_pad, sinkpad);
 error_link:
  srcpad = fsu_filter_manager_revert (priv->filters, GST_BIN (pipeline),
      filter_pad);
  if (srcpad)
  {
    gst_element_release_request_pad (GST_ELEMENT (priv->source), srcpad);
    gst_object_unref (srcpad);
  }
  gst_object_unref (sinkpad);
  gst_object_unref (filter_pad);

 no_source:
  g_mutex_unlock (priv->pipeline_mutex);

  return FALSE;
}

gboolean
_fsu_session_stop_sending (FsuSession *self)
{
  FsuSessionPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstPad *filter_pad = NULL;
  GstPad *srcpad = NULL;
  GstPad *sinkpad = NULL;

  if (g_atomic_int_get (&priv->sending) > 0)
  {
    if (g_atomic_int_dec_and_test (&priv->sending))
    {
      g_object_get (priv->conference,
          "pipeline", &pipeline,
          NULL);
      g_object_get (priv->session,
          "sink-pad", &sinkpad,
          NULL);

      g_mutex_lock (priv->pipeline_mutex);

      filter_pad = gst_pad_get_peer (sinkpad);
      gst_pad_unlink (filter_pad, sinkpad);
      srcpad = fsu_filter_manager_revert (priv->filters,
          GST_BIN (pipeline), filter_pad);
      gst_element_release_request_pad (GST_ELEMENT (priv->source), srcpad);
      gst_object_unref (srcpad);
      gst_object_unref (filter_pad);
      gst_object_unref (sinkpad);
      gst_object_unref (pipeline);

      g_mutex_unlock (priv->pipeline_mutex);
    }
    return TRUE;
  }
  return FALSE;
}

gboolean
_fsu_session_handle_message (FsuSession *self,
    GstMessage *message)
{
  FsuSessionPrivate *priv = self->priv;
  GList *i;
  gboolean drop = FALSE;
  gint list_id = 0;
  const GstStructure *s = gst_message_get_structure (message);

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT)
  {
    if (gst_structure_has_name (s, "fsusource-no-sources-available"))
    {
      g_signal_emit (self, signals[SIGNAL_NO_SOURCES_AVAILABLE], 0);
      drop = TRUE;
    }
    else if (gst_structure_has_name (s, "fsusource-source-chosen"))
    {
      const GValue *value;
      const GstElement *element = NULL;
      const gchar *name = NULL;
      const gchar *device = NULL;
      const gchar *device_name = NULL;

      value = gst_structure_get_value (s, "source");
      element = g_value_get_object (value);
      value = gst_structure_get_value (s, "source-name");
      name = g_value_get_string (value);
      value = gst_structure_get_value (s, "source-device");
      device = g_value_get_string (value);
      value = gst_structure_get_value (s, "source-device-name");
      device_name = g_value_get_string (value);

      g_signal_emit (self, signals[SIGNAL_SOURCE_CHOSEN], 0, element,
          name, device, device_name);
      drop = TRUE;
    }
    else if (gst_structure_has_name (s, "fsusource-source-destroyed"))
    {
      g_signal_emit (self, signals[SIGNAL_SOURCE_DESTROYED], 0);
      drop = TRUE;
    }
    else if (gst_structure_has_name (s, "fsusource-source-error"))
    {
      const GValue *value;
      GError *error = NULL;

      value = gst_structure_get_value (s, "error");
      error = g_value_get_boxed (value);
      g_signal_emit (self, signals[SIGNAL_SOURCE_ERROR], 0, error);
      drop = TRUE;
    }
  }

  if (drop)
    return drop;

  drop = fsu_filter_manager_handle_message (priv->filters, message);

  g_mutex_lock (priv->mutex);
 retry:
  for (i = priv->streams, list_id = priv->streams_id;
       i && !drop && list_id == priv->streams_id;
       i = i->next)
  {
    FsuStream *stream = i->data;
    g_mutex_unlock (priv->mutex);
    drop = _fsu_stream_handle_message (stream, message);
    g_mutex_lock (priv->mutex);
  }
  if (list_id != priv->streams_id)
    goto retry;
  g_mutex_unlock (priv->mutex);


  return drop;
}
