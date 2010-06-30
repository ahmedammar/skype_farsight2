/*
 * fsu-common.h - Header for common functions for Farsight-utils
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

#ifndef __FSU_COMMON_H__
#define __FSU_COMMON_H__

#include <gst/gst.h>


G_BEGIN_DECLS

typedef gboolean (*klass_check) (GstElementFactory *factory);

gboolean g_object_has_property (GObject *object, const gchar *property);
GList * get_plugins_filtered (klass_check check);

G_END_DECLS

#endif /* __FSU_COMMON_H__ */
