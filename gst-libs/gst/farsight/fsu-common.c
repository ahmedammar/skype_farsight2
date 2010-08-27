/*
 * fsu-common.c - Source for common functions for Farsight-utils
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

#include <gst/farsight/fsu-common.h>

#include <string.h>


gboolean
_fsu_g_object_has_property (GObject *object,
    const gchar *property)
{
  GObjectClass *klass;

  klass = G_OBJECT_GET_CLASS (object);
  return g_object_class_find_property (klass, property) ? TRUE : FALSE;
}

/* function used to sort element features */
/* Copy-pasted from decodebin */
static gint
compare_ranks (GstPluginFeature * f1,
    GstPluginFeature * f2)
{
  gint diff;
  const gchar *rname1, *rname2;

  diff =  gst_plugin_feature_get_rank (f2) - gst_plugin_feature_get_rank (f1);
  if (diff != 0)
    return diff;

  rname1 = gst_plugin_feature_get_name (f1);
  rname2 = gst_plugin_feature_get_name (f2);

  diff = g_strcmp0 (rname2, rname1);

  return diff;
}

GList *
_fsu_get_plugins_filtered (klass_check check)
{
  GList *walk, *registry, *result = NULL;
  GstElementFactory *factory;

  registry = gst_registry_get_feature_list (gst_registry_get_default (),
          GST_TYPE_ELEMENT_FACTORY);

  registry = g_list_sort (registry, (GCompareFunc) compare_ranks);

  for (walk = registry; walk; walk = g_list_next (walk))
  {
    factory = GST_ELEMENT_FACTORY (walk->data);

    if (check (factory))
    {
      result = g_list_append (result, factory);
      gst_object_ref (factory);
    }
  }

  gst_plugin_feature_list_free (registry);

  return result;
}


static gboolean
klass_contains (const gchar *klass,
    const gchar *needle)
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
_fsu_is_audio_source (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sources that provide a non raw stream */
  return (klass_contains (klass, "Audio") &&
          klass_contains (klass, "Source"));
}

gboolean
_fsu_is_audio_sink (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sinks that provide decoding */
  return (klass_contains (klass, "Audio") &&
          klass_contains (klass, "Sink"));
}

gboolean
_fsu_is_video_source (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sources that provide a non raw stream */
  return (klass_contains (klass, "Video") &&
          klass_contains (klass, "Source"));
}

gboolean
_fsu_is_video_sink (GstElementFactory *factory)
{
  const gchar *klass = gst_element_factory_get_klass (factory);
  /* we might have some sinks that provide decoding */
  return (klass_contains (klass, "Video") &&
          klass_contains (klass, "Sink"));
}


const gchar *
_fsu_get_device_property_name (GstElement *element)
{
  if (_fsu_g_object_has_property (G_OBJECT (element), "device") &&
      !_fsu_g_object_has_property (G_OBJECT (element), "device-name"))
  {
    return "device";
  }
  else if (!_fsu_g_object_has_property (G_OBJECT (element), "device") &&
      _fsu_g_object_has_property (G_OBJECT (element), "device-name"))
  {
    return "device-name";
  }
  else if (_fsu_g_object_has_property (G_OBJECT (element), "device") &&
      _fsu_g_object_has_property (G_OBJECT (element), "device-name"))
  {
    if (!g_strcmp0 (GST_ELEMENT_NAME (element), "dshowaudiosrc") ||
        !g_strcmp0 (GST_ELEMENT_NAME (element), "dshowvideosrc"))
      return "device-name";
    else
      return "device";
  }
  else
  {
    return NULL;
  }
}
