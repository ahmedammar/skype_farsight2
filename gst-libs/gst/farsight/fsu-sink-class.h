/*
 * fsu-sink-class.h - Header containing Class information for FsuSink
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

#ifndef __FSU_SINK_CLASS_H__
#define __FSU_SINK_CLASS_H__

#include <gst/farsight/fsu-sink.h>
#include <gst/farsight/fsu-multi-filter-manager.h>

struct _FsuSinkClass
{
  GstBinClass parent_class;
  gchar *auto_sink_name;
  GstElement *(*create_auto_sink) (FsuSink *self);
  gchar *(*need_mixer) (FsuSink *self,
      GstElement *sink);
  void (*add_filters) (FsuSink *self,
      FsuFilterManager *manager);
};

/**
 * FsuSink:
 *
 * A Gstreamer Element that derives from FsuSink.
 * Currently, either 'fsuaudiosink' or 'fsuvideosink'
 */
struct _FsuSink
{
  GstBin parent;
  /*< private >*/
  FsuSinkPrivate *priv;
};


void fsu_sink_element_added (FsuSink *self,
    GstBin *bin,
    GstElement *sink);

#endif /* __FSU_SINK_CLASS_H__ */
