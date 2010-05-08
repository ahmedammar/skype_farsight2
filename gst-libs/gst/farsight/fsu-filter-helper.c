/*
 * fsu-filter-helper.c - Source for helper functions for FsuFilter
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

#include "fsu-filter-helper.h"

gboolean
fsu_filter_add_element (GstBin *bin, GstPad *pad,
    GstElement *element, GstPad *element_pad)
{
  gboolean floating = GST_OBJECT_IS_FLOATING (GST_OBJECT (element));

  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (element_pad != NULL, FALSE);
  g_return_val_if_fail ((GST_PAD_IS_SRC (pad) && GST_PAD_IS_SINK (element_pad)) ||
      (GST_PAD_IS_SINK (pad) && GST_PAD_IS_SRC (element_pad)), FALSE);

  if (gst_bin_add (bin, element) == FALSE) {
    return FALSE;
  }

  if ((GST_PAD_IS_SRC (pad) && gst_pad_link(pad, element_pad) != GST_PAD_LINK_OK) ||
      (GST_PAD_IS_SINK (pad) && gst_pad_link(element_pad, pad))) {
    gst_object_ref (element);
    gst_bin_remove (bin, element);
    if (floating)
      GST_OBJECT_FLAG_SET (GST_OBJECT (element), GST_OBJECT_FLOATING);
    return FALSE;
  }

  return TRUE;
}


GstElement *
fsu_filter_add_element_by_name (GstBin *bin, GstPad *pad,
    const gchar *element_name, const gchar *pad_name,
    GstPad **out_pad, const gchar *out_pad_name)
{
  GstElement *element = gst_element_factory_make (element_name, NULL);
  GstPad *element_pad = NULL;

  if (element == NULL)
    return NULL;


  element_pad = gst_element_get_static_pad (element, pad_name);

  if (out_pad != NULL)
    *out_pad = gst_element_get_static_pad (element, out_pad_name);

  if (element_pad == NULL || (out_pad != NULL && *out_pad == NULL)) {
    if (element != NULL)
      gst_object_unref (element);
    if (element_pad != NULL)
      gst_object_unref (element_pad);
    if (out_pad != NULL && *out_pad != NULL)
      gst_object_unref (*out_pad);
    return NULL;
  }

  if (fsu_filter_add_element (bin, pad, element, element_pad) == FALSE) {
    gst_object_unref (element);
    gst_object_unref (element_pad);
    if (out_pad != NULL)
      gst_object_unref (*out_pad);
    return NULL;
  }

  gst_object_unref (element_pad);
  return element;
}

GstElement *
fsu_filter_add_element_by_description (GstBin *bin, GstPad *pad,
    const gchar *description, GstPad **out_pad)
{
  GstElement *filter = NULL;
  GstPad *src_pad = NULL;
  GstPad *sink_pad = NULL;
  GError *error = NULL;

  filter = gst_parse_bin_from_description (description, TRUE, &error);

  if (filter == NULL) {
    return NULL;
  } else {
    src_pad = gst_element_get_static_pad (filter, "src");
    sink_pad = gst_element_get_static_pad (filter, "sink");
    if (src_pad == NULL || sink_pad == NULL) {
      if (src_pad)
        gst_object_unref (src_pad);
      if (sink_pad)
        gst_object_unref (sink_pad);
      gst_object_unref (filter);
      return NULL;
    } else {
      if (GST_PAD_IS_SRC (pad) &&
          fsu_filter_add_element (bin, pad, filter, sink_pad)) {
        gst_object_unref (sink_pad);
        if (out_pad != NULL)
          *out_pad = src_pad;
        else
          gst_object_unref (src_pad);
      } else if (GST_PAD_IS_SINK (pad) &&
          fsu_filter_add_element (bin, pad, filter, src_pad)) {
        gst_object_unref (src_pad);
        if (out_pad != NULL)
          *out_pad = sink_pad;
        else
          gst_object_unref (sink_pad);
      } else {
        gst_object_unref (src_pad);
        gst_object_unref (sink_pad);
        gst_object_unref (filter);
        return NULL;
      }
    }
    return filter;
  }
}

GstPad *
fsu_filter_add_standard_element (GstBin *bin, GstPad *pad,
    const gchar *element_name, GstElement **element, GList **elements)
{
  GstElement *elem = NULL;
  GstPad *out_pad = NULL;


  elem = fsu_filter_add_element_by_name (bin, pad,
      element_name, GST_PAD_IS_SRC (pad) ? "sink" : "src",
      &out_pad, GST_PAD_IS_SRC (pad) ? "src" : "sink");

  if (elem != NULL) {
    gst_object_unref (pad);

    if (elements != NULL) {
      *elements = g_list_prepend (*elements, elem);
      gst_object_ref (elem);
    }
    if (element != NULL)
      *element = elem;
  }

  return out_pad;
}

GstPad *
fsu_filter_revert_standard_element (GstBin *bin, GstPad *pad, GList **elements)
{
  GstElement *element = GST_ELEMENT (gst_pad_get_parent (pad));
  GstPad *other_pad = NULL;
  GstPad *out_pad = NULL;

  if (elements != NULL && g_list_find (*elements, element) != NULL) {
    *elements = g_list_remove (*elements, element);
    gst_object_unref (element);
  }

  other_pad = gst_element_get_static_pad (element,
      GST_PAD_IS_SRC (pad) ? "sink" : "src");
  out_pad = gst_pad_get_peer (other_pad);
  gst_object_unref (other_pad);

  gst_bin_remove (bin, element);
  gst_object_unref (element);

  return out_pad;
}

