/*
 * fsu-probe.h - Header for Fsu Probing helpers
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

#ifndef __FSU_PROBE_H__
#define __FSU_PROBE_H__

G_BEGIN_DECLS

/**
 * FsuProbeDevice:
 * @device: The device value to set in #FsuSource:source-device or in
 * #FsuSink:sink-device. It usually represents the 'device' property of the
 * element.
 * @device_name: A user-friendly name to represent the device. It usually
 * represents the 'device-name' property of the element.
 *
 * This structure represents a device with its user-friendly name
 */
typedef struct {
  gchar *device;
  gchar *device_name;
} FsuProbeDevice;

/**
 * FsuProbeDeviceType:
 * @FSU_AUDIO_SOURCE_DEVICE: An audio source
 * @FSU_VIDEO_SOURCE_DEVICE: A video source
 * @FSU_AUDIO_SINK_DEVICE: An audio sink
 * @FSU_VIDEO_SINK_DEVICE: A video sink
 *
 * An enum to specify which type of device it is
 */
typedef enum {
  FSU_AUDIO_SOURCE_DEVICE,
  FSU_VIDEO_SOURCE_DEVICE,
  FSU_AUDIO_SINK_DEVICE,
  FSU_VIDEO_SINK_DEVICE,
} FsuProbeDeviceType;

/**
 * FsuProbeDeviceElement:
 * @type: The type of element
 * @element: The name of the element to set in #FsuSource:source-name or
 * in #FsuSink:sink-name
 * @name: A user-friendly name for the element
 * @description: A long description of the source or the sink
 * @devices: A #GList of #FsuProbeDevice items for every available device of
 * this element.
 *
 * A structure to represent the sources and sinks available as well as
 * their devices.
 * See also: fsu_probe_devices()
 */
typedef struct {
  FsuProbeDeviceType type;
  const gchar *element;
  const gchar *name;
  const gchar *description;
  GList *devices;
} FsuProbeDeviceElement;

FsuProbeDeviceElement *fsu_probe_element (const gchar *element_name);
GList *fsu_probe_devices (gboolean full);
void fsu_probe_devices_list_free (GList *devices);
void fsu_probe_device_element_free (FsuProbeDeviceElement *probe_element);
G_END_DECLS

#endif /* __FSU_PROBE_H__ */
