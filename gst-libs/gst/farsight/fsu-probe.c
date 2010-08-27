/*
 * fsu-probe.c - Source for Fsu Probing helpers
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
 * SECTION:fsu-probe
 * @short_description: Utility function to probe and discover devices
 *
 * These utility functions will allow you to easily probe the GStreamer registry
 * for available plugins and will give you all the information you need to
 * build a proper UI for audio/video setup.
 *
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


#include <gst/gst.h>

#include <string.h>
#include <gst/farsight/fsu-probe.h>
#include "fsu-common.h"
#include <gst/interfaces/propertyprobe.h>



/**
 * fsu_probe_devices:
 * @full: Set to #TRUE to get all possible sources and sinks available on
 * the system. Or set to #FALSE to get only the most relevant sources
 * and sinks.
 *
 * This function will probe the GStreamer elements to find all (or most
 * relevant) possible sources and sinks as well as list all their
 * available devices.
 *
 * Returns: A #GList of #FsuProbeDeviceElemnt with the result of the probe.
 * Must be freed with fsu_probe_free()
 */
GList *
fsu_probe_devices (gboolean full)
{
  GList *audio_sources, *video_sources, *audio_sinks, *video_sinks;
  GList *walk, *list;
  GList *result = NULL;
  gint si;

  audio_sources = _fsu_get_plugins_filtered (_fsu_is_audio_source);
  video_sources = _fsu_get_plugins_filtered (_fsu_is_video_source);
  audio_sinks = _fsu_get_plugins_filtered (_fsu_is_audio_sink);
  video_sinks = _fsu_get_plugins_filtered (_fsu_is_video_sink);

  for (si = 0; si < 4; si++) {
    FsuProbeDeviceType type;

    switch (si) {
      case 0:
        list = audio_sources;
        type = FSU_AUDIO_SOURCE_DEVICE;
        break;
      case 1:
        list = audio_sinks;
        type = FSU_AUDIO_SINK_DEVICE;
        break;
      case 2:
        list = video_sources;
        type = FSU_VIDEO_SOURCE_DEVICE;
        break;
      case 3:
        list = video_sinks;
        type = FSU_VIDEO_SINK_DEVICE;
        break;
      default:
        break;
    }
    for (walk = list; walk; walk = g_list_next (walk)) {
      GstPropertyProbe *probe;
      GValueArray *arr;
      GstElement *element;
      GstElementFactory *factory = GST_ELEMENT_FACTORY(walk->data);
      FsuProbeDeviceElement *item = NULL;

      if (!full &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "pulsesrc") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "alsasrc") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "dshowaudiosrc") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "directsoundsrc")  &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "osxaudiosrc")  &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "pulsesink") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "alsasink") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "osxaudiosink") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "v4l2src") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "dshowvideosrc") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "ksvideosrc") &&
          g_strcmp0 (GST_PLUGIN_FEATURE_NAME(factory), "xvimagesink"))
        continue;

      element = gst_element_factory_create (factory, NULL);
      if (element == NULL)
        continue;

      item = g_slice_new0 (FsuProbeDeviceElement);
      item->type = type;
      item->element = GST_PLUGIN_FEATURE_NAME(factory);
      item->name = gst_element_factory_get_longname (factory);
      item->description = gst_element_factory_get_description (factory);
      item->devices = NULL;

      if (GST_IS_PROPERTY_PROBE (element)) {
        probe = GST_PROPERTY_PROBE (element);
        if (probe && _fsu_get_device_property_name(element)) {
          const gchar *property_name = _fsu_get_device_property_name(element);
          const GList *properties = NULL;
          const GList *prop_walk;

          arr = NULL;
          properties = gst_property_probe_get_properties (probe);
          for (prop_walk = properties; prop_walk; prop_walk = prop_walk->next)
          {
            GParamSpec *spec = prop_walk->data;
            if (!g_strcmp0 (property_name, g_param_spec_get_name (spec)))
            {
              arr = gst_property_probe_probe_and_get_values (probe, spec);
              break;
            }
          }

          if (arr) {
            guint i;
            for (i = 0; i < arr->n_values; ++i) {
              const gchar *device;
              GValue *val;
              FsuProbeDevice *probe_device = NULL;

              val = g_value_array_get_nth (arr, i);
              if (val == NULL || !G_VALUE_HOLDS_STRING (val))
                continue;

              device = g_value_get_string (val);
              if (device == NULL)
                continue;

              probe_device = g_slice_new0 (FsuProbeDevice);
              probe_device->device = g_strdup (device);
              if (_fsu_g_object_has_property (G_OBJECT (element), "device") &&
                  _fsu_g_object_has_property (G_OBJECT (element),
                      "device-name"))
              {
                gchar *device_name = NULL;

                g_object_set (element,
                    _fsu_get_device_property_name(element),
                    device,
                    NULL);
                g_object_get (element,
                    "device-name", &device_name,
                    NULL);

                if (device_name)
                  probe_device->device_name = g_strdup (device_name);
                else
                  probe_device->device_name = g_strdup (device);
                g_free (device_name);
              }
              else
              {
                probe_device->device_name = g_strdup (device);
              }
              item->devices = g_list_append (item->devices, probe_device);
            }
            g_value_array_free (arr);
          }
        }
      }

      result = g_list_append (result, item);

      gst_object_unref (element);
    }
    for (walk = list; walk; walk = g_list_next (walk)) {
      if (walk->data)
        gst_object_unref (GST_ELEMENT_FACTORY (walk->data));
    }
    g_list_free (list);
  }

  return result;
}

/**
 * fsu_probe_free:
 * @devices: The devices probed
 *
 * Correctly frees the list of probed devices returned by fsu_probe_devices()
 */
void
fsu_probe_free (GList *devices)
{
  GList *walk, *walk2;

  for (walk = devices; walk; walk = walk->next)
  {
    FsuProbeDeviceElement *item = walk->data;

    for (walk2 = item->devices; walk2; walk2 = walk2->next)
    {
      FsuProbeDevice *device = walk2->data;
      g_free (device->device);
      g_free (device->device_name);
      g_slice_free (FsuProbeDevice, device);
    }
    g_list_free (item->devices);
    g_slice_free (FsuProbeDeviceElement, item);
  }
  g_list_free (devices);
}
