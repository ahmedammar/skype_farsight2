#include <gst/gst.h>
#include <gst/farsight/fsu-probe.h>

int main (int argc, char *argv[]) {
  GList *devices, *walk, *walk2;
  int i;

  gst_init (&argc, &argv);

  for (i = 0; i < 2; i ++)
  {
    if (i == 0)
    {
      devices = fsu_probe_devices (TRUE);
      g_debug ("********** FULL device probe *********");
    }
    else
    {
      devices = fsu_probe_devices (FALSE);
      g_debug ("********** Recommended device probe *********");
    }

    for (walk = devices; walk; walk = walk->next)
    {
      FsuProbeDeviceElement *item = walk->data;
      g_debug ("*** Element : %s", item->element);
      g_debug ("Type : %s",
          item->type == FSU_AUDIO_SOURCE_DEVICE?
          "audio source":
          item->type == FSU_AUDIO_SINK_DEVICE?
          "audio sink":
          item->type == FSU_VIDEO_SOURCE_DEVICE?
          "video source":
          item->type == FSU_VIDEO_SINK_DEVICE?
          "video sink":
          "unknown device type");
      g_debug ("Name: %s", item->name);
      g_debug ("Description : %s", item->description);
      g_debug ("Devices : %s", item->devices == NULL? "None available": "");
      for (walk2 = item->devices; walk2; walk2 = walk2->next)
      {
        FsuProbeDevice *device = walk2->data;
        g_debug ("   Device : %s", device->device);
        g_debug ("   Device name : %s", device->device_name);
      }
      g_debug (" ");
    }
    fsu_probe_free (devices);
  }

  return 0;
}
