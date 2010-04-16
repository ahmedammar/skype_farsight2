/*
 * fsu-audio-source.c - Source for FsuAudioSource
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

#include "fsu-audio-source.h"

GST_DEBUG_CATEGORY_STATIC (fsu_audio_source_debug);
#define GST_CAT_DEFAULT fsu_audio_source_debug
#define DEBUG(args...) GST_DEBUG_OBJECT (self, ## args)
#define WARNING(args...) GST_WARNING_OBJECT (self, ## args)

static const GstElementDetails fsu_audio_source_details =
GST_ELEMENT_DETAILS(
  "Farsight-Utils Magic Audio Source",
  "Audio Source",
  "FsuAudioSource class",
  "Youness Alaoui <youness.alaoui@collabora.co.uk>");

static void
_do_init (GType type)
{
  GST_DEBUG_CATEGORY_INIT
    (fsu_audio_source_debug, "fsuaudiosource", 0, "fsuaudiosource element");
}

GST_BOILERPLATE_FULL (FsuAudioSource, fsu_audio_source,
    FsuSource, FSU_TYPE_SOURCE, _do_init)

static void fsu_audio_source_constructed (GObject *object);
static void fsu_audio_source_dispose (GObject *object);
static void fsu_audio_source_finalize (GObject *object);

static const gchar *audio_priority_sources[] = {"dshowaudiosrc",
                                                "directsoundsrc",
                                                "osxaudiosrc",
                                                "gconfaudiosrc",
                                                "pulsesrc",
                                                "alsasrc",
                                                "oss4src",
                                                "osssrc",
                                                NULL};

static const gchar *audio_blacklisted_sources[] = {"audiotestsrc",
                                                   "autoaudiosrc",
                                                   "dtmfsrc",
                                                   NULL};

/* properties */
enum {
  LAST_PROPERTY
};

struct _FsuAudioSourcePrivate
{
  gboolean dispose_has_run;
};

static void
fsu_audio_source_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (gstelement_class, &fsu_audio_source_details);
}

static void
fsu_audio_source_class_init (FsuAudioSourceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuSourceClass *fsu_source_class = FSU_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuAudioSourcePrivate));

  gobject_class->constructed = fsu_audio_source_constructed;
  gobject_class->dispose = fsu_audio_source_dispose;
  gobject_class->finalize = fsu_audio_source_finalize;

  fsu_source_class->priority_sources = audio_priority_sources;
  fsu_source_class->blacklisted_sources = audio_blacklisted_sources;
  fsu_source_class->audio = TRUE;
}

static void
fsu_audio_source_init (FsuAudioSource *self, FsuAudioSourceClass *klass)
{
  FsuAudioSourcePrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_AUDIO_SOURCE,
          FsuAudioSourcePrivate);

  self->priv = priv;
  priv->dispose_has_run = FALSE;
}

static void
fsu_audio_source_constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
      G_OBJECT_CLASS (parent_class)->constructed;
  FsuAudioSource *self = FSU_AUDIO_SOURCE (object);
  FsuAudioSourcePrivate *priv = self->priv;

  (void)priv;

  if (chain_up != NULL)
    chain_up (object);

}

static void
fsu_audio_source_dispose (GObject *object)
{
  FsuAudioSource *self = (FsuAudioSource *)object;
  FsuAudioSourcePrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
fsu_audio_source_finalize (GObject *object)
{
  FsuAudioSource *self = (FsuAudioSource *)object;

  (void)self;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

