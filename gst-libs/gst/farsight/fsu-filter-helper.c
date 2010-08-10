/*
 * fsu-filter-helper.c - Source for helper functions for FsuFilter
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


#include <gst/farsight/fsu-filter-helper.h>

/**
 * fsu_filter_add_element:
 * @bin: The #GstBin to add the element to
 * @pad: The #GstPad to link the element to
 * @element: The #GstElement to add
 * @element_pad: The #GstPad from the @element to link to the @pad
 *
 * This helper function will add an element to the @bin and link it's
 * @element_pad with the @pad.
 * Make sure the @pad and @element_pad pads have the correct directions and
 * can be linked together.
 * This will sink the reference on the element on success.
 *
 * Returns: #TRUE if successful, #FALSE if an error occured
 */
gboolean
fsu_filter_add_element (GstBin *bin,
    GstPad *pad,
    GstElement *element,
    GstPad *element_pad)
{
  gboolean floating = GST_OBJECT_IS_FLOATING (GST_OBJECT (element));

  g_return_val_if_fail (element, FALSE);
  g_return_val_if_fail (element_pad, FALSE);
  g_return_val_if_fail (
      (GST_PAD_IS_SRC (pad) && GST_PAD_IS_SINK (element_pad)) ||
      (GST_PAD_IS_SINK (pad) && GST_PAD_IS_SRC (element_pad)),
      FALSE);

  if (!gst_bin_add (bin, element))
  {
    return FALSE;
  }

  if ((GST_PAD_IS_SRC (pad) &&
          gst_pad_link(pad, element_pad) != GST_PAD_LINK_OK) ||
      (GST_PAD_IS_SINK (pad) &&
          gst_pad_link(element_pad, pad) != GST_PAD_LINK_OK))
  {
    gst_object_ref (element);
    gst_bin_remove (bin, element);
    if (floating)
      GST_OBJECT_FLAG_SET (GST_OBJECT (element), GST_OBJECT_FLOATING);
    return FALSE;
  }
  if (!gst_element_sync_state_with_parent (element))
  {
    gst_pad_unlink (GST_PAD_IS_SRC (pad) ? pad : element_pad,
        GST_PAD_IS_SRC (pad) ? element_pad : pad);
    gst_object_ref (element);
    gst_bin_remove (bin, element);
    if (floating)
      GST_OBJECT_FLAG_SET (GST_OBJECT (element), GST_OBJECT_FLOATING);
    return FALSE;
  }

  return TRUE;
}


/**
 * fsu_filter_add_element_by_name:
 * @bin: The #GstBin to add the element to
 * @pad: The #GstPad to link the element to
 * @element_name: The name of the element to create and add
 * @pad_name: The name of the pad to get from the @element to link to the @pad
 * @out_pad: The output pad from the added element. Can be set to #NULL if no
 * output pad is needed
 * @out_pad_name: The name of the @out_pad to get from the element
 *
 * This helper function will create a new #GstElement by name (@element_name)
 * and add it to the @bin and link it's pad (@pad_name) with the @pad. It will
 * also return the output pad from the element through @out_pad if set by using
 * the @out_pad_name.
 * Make sure the @pad_name specifies the correct pad direction to link with @pad
 *
 * Returns: The #GstElement created or #NULL if an error occured.
 * gst_object_unref() the returned element if not needed.
 */
GstElement *
fsu_filter_add_element_by_name (GstBin *bin,
    GstPad *pad,
    const gchar *element_name,
    const gchar *pad_name,
    GstPad **out_pad,
    const gchar *out_pad_name)
{
  GstElement *element = gst_element_factory_make (element_name, NULL);
  GstPad *element_pad = NULL;

  if (!element)
    return NULL;


  element_pad = gst_element_get_static_pad (element, pad_name);

  if (out_pad)
    *out_pad = gst_element_get_static_pad (element, out_pad_name);

  if (!element_pad || (out_pad && !*out_pad))
  {
    if (element)
      gst_object_unref (element);
    if (element_pad)
      gst_object_unref (element_pad);
    if (out_pad && *out_pad)
    {
      gst_object_unref (*out_pad);
      *out_pad = NULL;
    }
    g_debug ("Failed trying to add element %s", element_name);
    return NULL;
  }

  if (!fsu_filter_add_element (bin, pad, element, element_pad))
  {
    gst_object_unref (element);
    gst_object_unref (element_pad);
    if (out_pad)
    {
      gst_object_unref (*out_pad);
      *out_pad = NULL;
    }
    g_debug ("Failed trying to add element %s", element_name);
    return NULL;
  }

  gst_object_unref (element_pad);
  return gst_object_ref (element);
}

/**
 * fsu_filter_add_element_by_description:
 * @bin: The #GstBin to add the element to
 * @pad: The #GstPad to link the element to
 * @description: The pipeline description of the bin to create and add
 * @out_pad: The output pad from the created bin. Can be set to #NULL if no
 * output pad is needed
 *
 * This helper function will create a new #GstElement described the the pipeline
 * description in @description and add it to the @bin and link it's pad
 * with the @pad. It will also return the output pad from the element through
 * @out_pad if set.
 * Since gst_parse_bin_from_description() is used, it already knows the name of
 * the source and sink pads, so it is unnecessary to specify them. It will also
 * use the pad direction from @pad to know in which direction to link.
 *
 * Returns: The #GstElement created or #NULL if an error occured.
 * gst_object_unref() the returned element if not needed.
 */
GstElement *
fsu_filter_add_element_by_description (GstBin *bin,
    GstPad *pad,
    const gchar *description,
    GstPad **out_pad)
{
  GstElement *filter = NULL;
  GstPad *src_pad = NULL;
  GstPad *sink_pad = NULL;
  GError *error = NULL;

  filter = gst_parse_bin_from_description (description, TRUE, &error);

  if (!filter)
    return NULL;

  src_pad = gst_element_get_static_pad (filter, "src");
  sink_pad = gst_element_get_static_pad (filter, "sink");
  if (!src_pad || !sink_pad)
  {
    if (src_pad)
      gst_object_unref (src_pad);
    if (sink_pad)
      gst_object_unref (sink_pad);
    gst_object_unref (filter);
    return NULL;
  }
  else
  {
    if (GST_PAD_IS_SRC (pad) &&
        fsu_filter_add_element (bin, pad, filter, sink_pad))
    {
      gst_object_unref (sink_pad);
      if (out_pad)
        *out_pad = src_pad;
      else
        gst_object_unref (src_pad);
    }
    else if (GST_PAD_IS_SINK (pad) &&
        fsu_filter_add_element (bin, pad, filter, src_pad))
    {
      gst_object_unref (src_pad);
      if (out_pad)
        *out_pad = sink_pad;
      else
        gst_object_unref (sink_pad);
    }
    else
    {
      gst_object_unref (src_pad);
      gst_object_unref (sink_pad);
      gst_object_unref (filter);
      return NULL;
    }
  }

  return gst_object_ref (filter);
}

/**
 * fsu_filter_add_standard_element:
 * @bin: The #GstBin to add the element to
 * @pad: The #GstPad to link the element to
 * @element_name: The name of the standard element to create and add
 * @element: A pointer to store the created element. Can be #NULL if the element
 * is not needed.
 * @elements: A list to which to add the created element or #NULL if not needed.
 * If the element is added to the list, it will hold a reference to it.
 *
 * This helper function will create a new #GstElement by its name and add it
 * to the @bin and link it's appropriate pad with the @pad.
 * The difference between this and fsu_filter_add_element_by_name() is that this
 * expects the created element to have 'standard' pads, which means a 'src' pad
 * for the source pad and a 'sink' pad for the sink pad. Both pads would need to
 * be ALWAYS pads.
 *
 * Returns: The output #GstPad from the element or #NULL if an error occured.
 * gst_object_unref() the returned pad if not needed as well as the returned
 * element if @element is set.
 */
GstPad *
fsu_filter_add_standard_element (GstBin *bin,
    GstPad *pad,
    const gchar *element_name,
    GstElement **element,
    GList **elements)
{
  GstElement *elem = NULL;
  GstPad *out_pad = NULL;


  elem = fsu_filter_add_element_by_name (bin, pad,
      element_name, GST_PAD_IS_SRC (pad) ? "sink" : "src",
      &out_pad, GST_PAD_IS_SRC (pad) ? "src" : "sink");

  if (elem)
  {
    if (elements)
    {
      *elements = g_list_prepend (*elements, elem);
      gst_object_ref (elem);
    }
    if (element)
      *element = elem;
    else
      gst_object_unref (elem);

    return out_pad;
  }

  g_debug ("****Failed trying to add standard element %s", element_name);
  return NULL;
}

/**
 * fsu_filter_revert_standard_element:
 * @bin: The #GstBin to revert the element from
 * @pad: The #GstPad from the element to revert
 * @elements: A list to which the element was previously added in order to
 * gst_object_unref it and remove it from the list. Can be set to #NULL if not
 * needed.
 *
 * This helper function will automatically revert a 'standard' #GstElement from
 * the @bin and return the expected output pad.
 * This expects the element to have 'standard' pads, which means a 'src' pad
 * for the source pad and a 'sink' pad for the sink pad. Both pads would need to
 * be ALWAYS pads.
 *
 * Returns: The output #GstPad from the reverted element or #NULL if an error
 * occured.
 * gst_object_unref() the returned pad if not needed.
 */
GstPad *
fsu_filter_revert_standard_element (GstBin *bin,
    GstPad *pad,
    GList **elements)
{
  GstElement *element = GST_ELEMENT (gst_pad_get_parent (pad));
  GstPad *other_pad = NULL;
  GstPad *out_pad = NULL;

  if (elements && g_list_find (*elements, element))
  {
    *elements = g_list_remove (*elements, element);
    gst_object_unref (element);
  }

  other_pad = gst_element_get_static_pad (element,
      GST_PAD_IS_SRC (pad) ? "sink" : "src");
  out_pad = gst_pad_get_peer (other_pad);
  gst_object_unref (other_pad);

  gst_bin_remove (bin, element);
  gst_element_set_state (element, GST_STATE_NULL);
  gst_object_unref (element);

  return out_pad;
}

/**
 * fsu_filter_revert_bin:
 * @bin: The #GstBin to revert the bin from
 * @pad: The #GstPad from the bin to revert
 *
 * This helper function will automatically revert a bin previously created by
 * fsu_filter_add_element_by_description() from the @bin and return the expected
 * output pad.
 *
 * Returns: The output #GstPad from the reverted bin or #NULL if an error
 * occured.
 * gst_object_unref() the returned pad if not needed.
 */
GstPad *
fsu_filter_revert_bin (GstBin *bin,
    GstPad *pad)
{
  GstElement *filter_bin = GST_ELEMENT (gst_pad_get_parent (pad));
  GstPad *other_pad = NULL;
  GstPad *out_pad = NULL;


  other_pad = gst_element_get_static_pad (filter_bin,
      GST_PAD_IS_SRC (pad) ? "sink" : "src");
  out_pad = gst_pad_get_peer (other_pad);
  gst_object_unref (other_pad);

  gst_bin_remove (bin, filter_bin);
  gst_element_set_state (filter_bin, GST_STATE_NULL);
  gst_object_unref (filter_bin);

  return out_pad;
}
