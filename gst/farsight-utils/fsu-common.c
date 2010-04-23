/*
 * fsu-common.c - Source for common functions for Farsight-utils
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


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "fsu-common.h"

#include <string.h>

#include "fsu-audio-source.h"
#include "fsu-video-source.h"
#include "fsu-audio-sink.h"
#include "fsu-video-sink.h"


gboolean
g_object_has_property (GObject *object, const gchar *property)
{
  GObjectClass *klass;

  klass = G_OBJECT_GET_CLASS (object);
  return NULL != g_object_class_find_property (klass, property);
}

static gboolean
klass_contains (const gchar *klass, const gchar *needle)
{
  gchar *found = strstr (klass, needle);

  if(!found)
    return FALSE;
  if (found != klass && *(found-1) != '/')
    return FALSE;
  if (found[strlen (needle)] != 0 &&
      found[strlen (needle)] != '/')
    return FALSE;
  return TRUE;
}

gboolean
is_audio_source (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sources that provide a non raw stream */
  return (klass_contains (klass, "Audio") &&
          klass_contains (klass, "Source"));
}

gboolean
is_audio_sink (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sinks that provide decoding */
  return (klass_contains (klass, "Audio") &&
          klass_contains (klass, "Sink"));
}

gboolean
is_video_source (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sources that provide a non raw stream */
  return (klass_contains (klass, "Video") &&
          klass_contains (klass, "Source"));
}

gboolean
is_video_sink (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sinks that provide decoding */
  return (klass_contains (klass, "Video") &&
          klass_contains (klass, "Sink"));
}

/* function used to sort element features */
/* Copy-pasted from decodebin */
static gint
compare_ranks (GstPluginFeature * f1, GstPluginFeature * f2)
{
  gint diff;
  const gchar *rname1, *rname2;

  diff =  gst_plugin_feature_get_rank (f2) - gst_plugin_feature_get_rank (f1);
  if (diff != 0)
    return diff;

  rname1 = gst_plugin_feature_get_name (f1);
  rname2 = gst_plugin_feature_get_name (f2);

  diff = strcmp (rname2, rname1);

  return diff;
}

GList *
get_plugins_filtered (klass_check check)
{
  GList *walk, *registry, *result = NULL;
  GstElementFactory *factory;

  registry = gst_registry_get_feature_list (gst_registry_get_default (),
          GST_TYPE_ELEMENT_FACTORY);

  registry = g_list_sort (registry, (GCompareFunc) compare_ranks);

  for (walk = registry; walk; walk = g_list_next (walk)) {
    factory = GST_ELEMENT_FACTORY (walk->data);

    if (check (factory)) {
      result = g_list_append (result, factory);
      gst_object_ref (factory);
    }
  }

  gst_plugin_feature_list_free (registry);

  return result;
}


static gboolean plugin_init (GstPlugin * plugin)
{
  gboolean ret = TRUE;
  ret &= gst_element_register (plugin, "fsuaudiosrc",
      GST_RANK_NONE, FSU_TYPE_AUDIO_SOURCE);
  ret &= ret && gst_element_register (plugin, "fsuvideosrc",
      GST_RANK_NONE, FSU_TYPE_VIDEO_SOURCE);
  ret &= ret && gst_element_register (plugin, "fsuaudiosink",
      GST_RANK_NONE, FSU_TYPE_AUDIO_SINK);
  ret &= ret && gst_element_register (plugin, "fsuvideosink",
      GST_RANK_NONE, FSU_TYPE_VIDEO_SINK);
  return ret;
}

GST_PLUGIN_DEFINE (
  GST_VERSION_MAJOR,
  GST_VERSION_MINOR,
  "fsutils",
  "Farsight utils plugin",
  plugin_init,
  VERSION,
  "LGPL",
  "Farsight",
  "http://farsight.freedesktop.org/"
)
