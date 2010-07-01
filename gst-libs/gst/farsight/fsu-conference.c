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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/farsight/fsu-conference.h>
#include <gst/farsight/fsu-session-priv.h>

#include <gst/gst.h>

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


/* properties */
enum
{
  PROP_CONFERENCE = 1,
  PROP_PIPELINE,
  LAST_PROPERTY
};

struct _FsuConferencePrivate
{
  gboolean dispose_has_run;
  GstElement *pipeline;
  GstElement *conference;
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

  g_object_class_install_property (gobject_class, PROP_CONFERENCE,
      g_param_spec_object ("conference", "The farsight conference",
          "The farsight conference element",
          GST_TYPE_ELEMENT,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
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
  priv->dispose_has_run = FALSE;
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
  void (*chain_up) (GObject *) =
      G_OBJECT_CLASS (fsu_conference_parent_class)->constructed;
  FsuConference *self = FSU_CONFERENCE (object);
  FsuConferencePrivate *priv = self->priv;
  gchar *error = NULL;

  if (chain_up)
    chain_up (object);

  if (!priv->pipeline)
    priv->pipeline = gst_pipeline_new ("fsu_pipeline");

  if (!priv->pipeline)
  {
     error = "Couldn't create gstreamer pipeline";
     goto error;
  }

  if (!gst_bin_add (GST_BIN (priv->pipeline), priv->conference))
  {
    error = "Couldn't add fsrtpconference to the pipeline";
    goto error;
  }

  if (gst_element_set_state (priv->pipeline, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE)
  {
    error = "Unable to set pipeline to PLAYING";
    goto error;
  }

  return;
 error:
  /* TODO: signal/something the error */
  return;
}

static void
fsu_conference_dispose (GObject *object)
{
  FsuConference *self = (FsuConference *)object;
  FsuConferencePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (priv->conference)
    gst_object_unref (priv->conference);
  priv->conference = NULL;

  if (priv->pipeline)
    gst_object_unref (priv->pipeline);
  priv->pipeline = NULL;

  G_OBJECT_CLASS (fsu_conference_parent_class)->dispose (object);
}

static void
fsu_conference_finalize (GObject *object)
{
  FsuConference *self = (FsuConference *)object;

  (void)self;

  G_OBJECT_CLASS (fsu_conference_parent_class)->finalize (object);
}


FsuConference *
fsu_conference_new (GstElement *conference,
    GstElement *pipeline)
{
  g_return_val_if_fail (conference, NULL);

  return g_object_new (FSU_TYPE_CONFERENCE,
      "pipeline", pipeline,
      "conference", conference,
      NULL);
}

FsuSession *
fsu_conference_handle_session (FsuConference *self,
    FsSession *session,
    FsuSource *source)
{
  return _fsu_session_new (self, session, source);
}

