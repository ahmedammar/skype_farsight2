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


/**
 * SECTION:fsu-stream
 * @short_description: A wrapper class around #FsStream
 *
 * This class is part of Farsight-Utils. It is meant to wrap the
 * #FsStream and link the sink with the #FsConference.
 * It also provides access to a #FsuFilterManager that sits between the source
 * pads of the #FsStream and the sink.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/farsight/fsu-stream.h>
#include <gst/farsight/fsu-stream-priv.h>
#include <gst/farsight/fsu-session-priv.h>
#include <gst/filters/fsu-multi-filter-manager.h>
#include "fs-marshal.h"

G_DEFINE_TYPE (FsuStream, fsu_stream, G_TYPE_OBJECT);

static void fsu_stream_constructed (GObject *object);
static void fsu_stream_dispose (GObject *object);
static void fsu_stream_finalize (GObject *object);
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

/* signals  */
enum {
  SIGNAL_NO_SINKS_AVAILABLE,
  SIGNAL_SINK_CHOSEN,
  SIGNAL_SINK_DESTROYED,
  SIGNAL_SINK_ERROR,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};


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
  FsuConference *conference;
  FsuSession *session;
  FsStream *stream;
  FsuSink *sink;
  gboolean receiving;
  gboolean sending;
  FsuFilterManager *filters;
  GMutex *mutex;
  GMutex *pipeline_mutex;
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

  /**
   * FsuStream::no-sinks-available:
   *
   * This signal is sent when the sink cannot find any suitable sink
   * device for capture.
   */
  signals[SIGNAL_NO_SINKS_AVAILABLE] = g_signal_new ("no-sinks-available",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * FsuStream::sink-chosen:
   * @sink_element: The #GstElement of the chosen sink
   * @sink_name: The name of the chosen element
   * @sink_device: The chosen device
   * @sink_device_name: The chosen user-friendly device name
   *
   * This signal is sent when the sink has been chosen and correctly opened
   * and ready for capture.
   */
  signals[SIGNAL_SINK_CHOSEN] = g_signal_new ("sink-chosen",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      _fs_marshal_VOID__OBJECT_STRING_STRING_STRING,
      G_TYPE_NONE, 4,
      GST_TYPE_ELEMENT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * FsuStream::sink-destroyed:
   *
   * This signal is sent when the sink has been destroyed.
   */
  signals[SIGNAL_SINK_DESTROYED] = g_signal_new ("sink-destroyed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__VOID,
      G_TYPE_NONE, 0);

  /**
   * FsuStream::sink-error:
   * @error: The #GError received from the sink
   *
   * This signal is sent when the sink received a Resink error. This will
   * usually only be sent when the device is no longer available. It will
   * shortly be followed by a #FsuSink::sink-destroyed signal then either
   * a #FsuSink::sink-chosen or #FsuSink::no-sinks-available signal.
   */
  signals[SIGNAL_SINK_ERROR] = g_signal_new ("sink-error",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      g_cclosure_marshal_VOID__BOXED,
      G_TYPE_NONE, 1,
      GST_TYPE_G_ERROR);
}


static void
fsu_stream_init (FsuStream *self)
{
  FsuStreamPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_STREAM,
          FsuStreamPrivate);

  self->priv = priv;
  priv->filters = fsu_multi_filter_manager_new ();
  priv->mutex = g_mutex_new ();
  priv->pipeline_mutex = g_mutex_new ();
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
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;
  GstElement *pipeline = NULL;

  if (G_OBJECT_CLASS (fsu_stream_parent_class)->constructed)
    G_OBJECT_CLASS (fsu_stream_parent_class)->constructed (object);

  g_signal_connect_object (priv->stream, "src-pad-added",
      G_CALLBACK (src_pad_added), self, 0);

  if (priv->sink)
  {
    GstObject *parent = gst_object_get_parent (GST_OBJECT (priv->sink));

    g_object_get (priv->conference,
        "pipeline", &pipeline,
        NULL);

    /* If the sink has a parent and it's the pipeline, then skip it,
     * if it has a parent and it's not a pipeline, then unref/NULL it
     * otherwise, try to add it and unref/NULL it if it failed
     */
    if ((!parent || parent != GST_OBJECT (pipeline)) &&
        ((parent && parent != GST_OBJECT (pipeline)) ||
            !gst_bin_add (GST_BIN (pipeline), GST_ELEMENT (priv->sink))))
    {
      gst_object_unref (GST_OBJECT (priv->sink));
      priv->sink = NULL;
    }
    gst_object_unref (pipeline);

    if (parent)
      gst_object_unref (parent);
  }

}


static void
fsu_stream_dispose (GObject *object)
{
  FsuStream *self = FSU_STREAM (object);
  FsuStreamPrivate *priv = self->priv;

  fsu_stream_stop_sending (self);
  fsu_stream_stop_receiving (self);

  g_mutex_lock (priv->mutex);

  if (priv->sink)
  {
    if (GST_OBJECT_REFCOUNT(priv->sink) == 1)
    {
      GstElement *pipeline = NULL;

      g_object_get (priv->conference,
          "pipeline", &pipeline,
          NULL);

      gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (priv->sink));
      gst_object_unref (pipeline);
      gst_element_set_state (GST_ELEMENT (priv->sink), GST_STATE_NULL);
    }

    gst_object_unref (GST_OBJECT (priv->sink));
  }
  priv->sink = NULL;

  if (priv->filters)
    g_object_unref (priv->filters);
  priv->filters = NULL;

  if (priv->stream)
    gst_object_unref (priv->stream);
  priv->stream = NULL;

  if (priv->session)
    gst_object_unref (priv->session);
  priv->session = NULL;

  if (priv->conference)
    gst_object_unref (priv->conference);
  priv->conference = NULL;

  g_mutex_unlock (priv->mutex);

  G_OBJECT_CLASS (fsu_stream_parent_class)->dispose (object);
}

static void
fsu_stream_finalize (GObject *object)
{
  FsuStream *self = FSU_STREAM (object);

  g_mutex_free (self->priv->mutex);
  g_mutex_free (self->priv->pipeline_mutex);

  G_OBJECT_CLASS (fsu_stream_parent_class)->finalize (object);
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
  g_return_val_if_fail (!sink || FSU_IS_SINK (sink), NULL);

  return g_object_new (FSU_TYPE_STREAM,
      "fsu-conference", conference,
      "fsu-session", session,
      "fs-stream", stream,
      "sink", sink,
      NULL);
}

static void
src_pad_unlinked (GstPad  *pad,
    GstPad  *filter_pad,
    gpointer user_data)
{
  FsuStream *self = FSU_STREAM (user_data);
  FsuStreamPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstPad *sink_pad = NULL;

  g_object_get (priv->conference,
      "pipeline", &pipeline,
      NULL);

  g_mutex_lock (priv->pipeline_mutex);

  sink_pad = fsu_filter_manager_revert (priv->filters,
      GST_BIN (pipeline), filter_pad);

  if (!sink_pad)
    sink_pad = gst_object_ref (filter_pad);

  if (priv->sink)
  {
    gst_element_release_request_pad (GST_ELEMENT (priv->sink),
        sink_pad);
    gst_object_unref (sink_pad);
  }
  else
  {
    GstObject *sink = gst_object_get_parent (GST_OBJECT (sink_pad));
    gst_bin_remove (GST_BIN (pipeline), GST_ELEMENT (sink));
    gst_object_unref (sink);
    gst_object_unref (sink_pad);
  }

  gst_object_unref (pipeline);

  g_mutex_unlock (priv->pipeline_mutex);

  g_signal_handlers_disconnect_by_func(pad, src_pad_unlinked, user_data);
}


static void
src_pad_added (FsStream *stream,
    GstPad *pad,
    FsCodec *codec,
    gpointer user_data)
{
  FsuStream *self = FSU_STREAM (user_data);
  FsuStreamPrivate *priv = self->priv;
  GstElement *pipeline = NULL;
  GstElement *sink = NULL;
  GstPad *sink_pad = NULL;
  GstPad *filter_pad = NULL;

  g_object_get (priv->conference,
      "pipeline", &pipeline,
      NULL);

  g_mutex_lock (priv->mutex);
  if (!priv->receiving)
  {
    g_mutex_unlock (priv->mutex);
    return;
  }
  g_mutex_unlock (priv->mutex);

  g_mutex_lock (priv->pipeline_mutex);

  if (priv->sink)
  {
    sink = GST_ELEMENT (priv->sink);
    sink_pad = gst_element_get_request_pad (sink, "sink%d");
  }
  else
  {
    sink = gst_element_factory_make ("fakesink", NULL);
    if (!sink)
    {
      goto error;
    }
    if (!gst_bin_add (GST_BIN (pipeline), sink))
    {
      gst_object_unref (sink);
      goto error;
    }
    sink_pad = gst_element_get_static_pad (sink, "sink");
  }

  if (!sink_pad)
    goto error_pad;

  filter_pad = fsu_filter_manager_apply (priv->filters,
      GST_BIN (pipeline), sink_pad);

  if (!filter_pad)
    filter_pad = gst_object_ref (sink_pad);
  gst_object_unref (sink_pad);

  if (GST_PAD_LINK_FAILED (gst_pad_link (pad, filter_pad)))
    goto error_link;

  if (gst_element_set_state (sink, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
    goto error_state;

  gst_object_unref (filter_pad);

  g_signal_connect_object (pad, "unlinked", (GCallback) src_pad_unlinked,
      self, 0);

  g_mutex_unlock (priv->pipeline_mutex);

  return;

 error_state:
  gst_pad_unlink (pad, filter_pad);
 error_link:
  sink_pad = fsu_filter_manager_revert (priv->filters,
      GST_BIN (pipeline), filter_pad);
  if (!sink_pad)
    sink_pad = gst_object_ref (filter_pad);
 error_pad:
  if (priv->sink)
    gst_element_release_request_pad (sink, sink_pad);
  else
    gst_bin_remove (GST_BIN (pipeline), sink);
  if (sink_pad)
    gst_object_unref (sink_pad);
  if (filter_pad)
    gst_object_unref (filter_pad);
 error:
  g_mutex_unlock (priv->pipeline_mutex);
  return;
}


/**
 * fsu_stream_start_sending:
 * @self: The #FsuStream
 *
 * This tells the #FsuStream that you want to start sending the data to this
 * stream. It will let the session know and the source will be activated.
 *
 * Returns: #TRUE on success, #FALSE otherwise
 */
gboolean
fsu_stream_start_sending (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);
  if (priv->sending)
  {
    g_mutex_unlock (priv->mutex);
    return TRUE;
  }

  priv->sending = TRUE;
  g_mutex_unlock (priv->mutex);

  return _fsu_session_start_sending (priv->session);
}

/**
 * fsu_stream_stop_sending:
 * @self: The #FsuStream
 *
 * This tells the #FsuStream that you want to stop sending the data to this
 * stream. It will let the session know and the source will be disabled if
 * unused.
 *
 */
void
fsu_stream_stop_sending (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;

  g_mutex_lock (priv->mutex);
  if (!priv->sending)
  {
    g_mutex_unlock (priv->mutex);
    return;
  }

  priv->sending = FALSE;
  g_mutex_unlock (priv->mutex);

  _fsu_session_stop_sending (priv->session);
}

/**
 * fsu_stream_start_receiving:
 * @self: The #FsuStream
 *
 * This tells the #FsuStream that you want to start receiving data from this
 * stream. It will link all the output pads from the conference and start the
 * sink if necessary. It will also start handling new source pads being created
 * by the conference.
 *
 * Returns: #TRUE on success, #FALSE otherwise
 */
gboolean
fsu_stream_start_receiving (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  gboolean ret = TRUE;

  g_mutex_lock (priv->mutex);
  if (!priv->receiving)
  {
    GstIterator *iter = NULL;
    gboolean done = FALSE;

    priv->receiving = TRUE;
    g_mutex_unlock (priv->mutex);

    iter = fs_stream_get_src_pads_iterator (priv->stream);
    while (!done)
    {
      gpointer pad;
      switch (gst_iterator_next (iter, &pad))
      {
        case GST_ITERATOR_OK:
          src_pad_added (priv->stream, pad, NULL, self);
          gst_object_unref (pad);
          break;
        case GST_ITERATOR_RESYNC:
          fsu_stream_stop_receiving (self);
          g_mutex_lock (priv->mutex);
          priv->receiving = TRUE;
          g_mutex_unlock (priv->mutex);
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
  else
  {
    g_mutex_unlock (priv->mutex);
  }

  return ret;
}

/**
 * fsu_stream_stop_receiving:
 * @self: The #FsuStream
 *
 * This tells the #FsuStream that you want to stop receiving data from this
 * stream. It will unlink all the output pads from the conference and stop the
 * sink if necessary.
 *
 */
void
fsu_stream_stop_receiving (FsuStream *self)
{
  FsuStreamPrivate *priv = self->priv;
  GstIterator *iter = NULL;
  gboolean done = FALSE;

  g_mutex_lock (priv->mutex);
  priv->receiving = FALSE;
  g_mutex_unlock (priv->mutex);

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
            /* The real unlinking will actually be done in the 'unlinked'
               signal handler */
            gst_pad_unlink (pad, filter_pad);
            gst_object_unref (filter_pad);
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

gboolean
_fsu_stream_handle_message (FsuStream *self,
    GstMessage *message)
{
  gboolean drop = FALSE;
  const GstStructure *s = gst_message_get_structure (message);

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT)
  {
    if (gst_structure_has_name (s, "fsusink-no-sinks-available"))
    {
      g_signal_emit (self, signals[SIGNAL_NO_SINKS_AVAILABLE], 0);
      drop = TRUE;
    }
    else if (gst_structure_has_name (s, "fsusink-sink-chosen"))
    {
      const GValue *value;
      const GstElement *element = NULL;
      const gchar *name = NULL;
      const gchar *device = NULL;
      const gchar *device_name = NULL;

      value = gst_structure_get_value (s, "sink");
      element = g_value_get_object (value);
      value = gst_structure_get_value (s, "sink-name");
      name = g_value_get_string (value);
      value = gst_structure_get_value (s, "sink-device");
      device = g_value_get_string (value);
      value = gst_structure_get_value (s, "sink-device-name");
      device_name = g_value_get_string (value);

      g_signal_emit (self, signals[SIGNAL_SINK_CHOSEN], 0, element,
          name, device, device_name);
      drop = TRUE;
    }
    else if (gst_structure_has_name (s, "fsusink-sink-destroyed"))
    {
      g_signal_emit (self, signals[SIGNAL_SINK_DESTROYED], 0);
      drop = TRUE;
    }
    else if (gst_structure_has_name (s, "fsusink-sink-error"))
    {
      const GValue *value;
      GError *error = NULL;

      value = gst_structure_get_value (s, "error");
      error = g_value_get_boxed (value);
      g_signal_emit (self, signals[SIGNAL_SINK_ERROR], 0, error);
      drop = TRUE;
    }
  }

  if (drop)
    return drop;
  else
    return fsu_filter_manager_handle_message (self->priv->filters, message);
}
