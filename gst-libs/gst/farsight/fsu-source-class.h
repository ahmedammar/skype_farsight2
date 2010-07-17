/*
 * fsu-source-class.h - Header containing Class information for FsuSource
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

#ifndef __FSU_SOURCE_CLASS_H__
#define __FSU_SOURCE_CLASS_H__

#include <gst/farsight/fsu-source.h>
#include <gst/farsight/fsu-multi-filter-manager.h>

G_BEGIN_DECLS

struct _FsuSourceClass
{
  GstBinClass parent_class;
  const gchar **priority_sources;
  const gchar **blacklisted_sources;
  klass_check klass_check;
  void (*add_filters) (FsuSource *self,
      FsuFilterManager *manager);
};

struct _FsuSource
{
  GstBin parent;
  /*< private >*/
  FsuSourcePrivate *priv;
};

G_END_DECLS

#endif /* __FSU_SOURCE_CLASS_H__ */
