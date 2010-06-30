/*
 * fsu-audio-source.h - Header for FsuAudioSource
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

#ifndef __FSU_AUDIO_SOURCE_H__
#define __FSU_AUDIO_SOURCE_H__

#include <gst/farsight/fsu-source-class.h>


G_BEGIN_DECLS

#define FSU_TYPE_AUDIO_SOURCE                   \
  (fsu_audio_source_get_type())
#define FSU_AUDIO_SOURCE(obj)                   \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_AUDIO_SOURCE,                    \
      FsuAudioSource))
#define FSU_AUDIO_SOURCE_CLASS(klass)           \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_AUDIO_SOURCE,                    \
      FsuAudioSourceClass))
#define FSU_IS_AUDIO_SOURCE(obj)                \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_AUDIO_SOURCE))
#define FSU_IS_AUDIO_SOURCE_CLASS(klass)        \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_AUDIO_SOURCE))
#define FSU_AUDIO_SOURCE_GET_CLASS(obj)         \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_AUDIO_SOURCE,                    \
      FsuAudioSourceClass))

typedef struct _FsuAudioSource      FsuAudioSource;
typedef struct _FsuAudioSourceClass FsuAudioSourceClass;

struct _FsuAudioSourceClass
{
  FsuSourceClass parent_class;
};

struct _FsuAudioSource
{
  FsuSource parent;
};

GType fsu_audio_source_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __FSU_AUDIO_SOURCE_H__ */
