/*
 * fsu-filter-helper.h - Header for helper functions for FsuFilter
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

#ifndef __FSU_FILTER_HELPER_H__
#define __FSU_FILTER_HELPER_H__

#include <gst/gst.h>


G_BEGIN_DECLS

gboolean fsu_filter_add_element (GstBin *bin, GstPad *pad,
    GstElement *element, GstPad *element_pad);
GstElement *fsu_filter_add_element_by_name (GstBin *bin, GstPad *pad,
    const gchar *element_name, const gchar *pad_name,
    GstPad **out_pad, const gchar *out_pad_name);
GstElement *fsu_filter_add_element_by_description (GstBin *bin, GstPad *pad,
    const gchar *description, GstPad **out_pad);
GstPad *fsu_filter_add_standard_element (GstBin *bin, GstPad *pad,
    const gchar *element_name, GstElement **element, GList **elements);
GstPad *fsu_filter_revert_standard_element (GstBin *bin,
    GstPad *pad, GList **elements);
GstPad *fsu_filter_revert_bin (GstBin *bin, GstPad *pad);

G_END_DECLS

#endif /* __FSU_FILTER__HELPER_H__ */
