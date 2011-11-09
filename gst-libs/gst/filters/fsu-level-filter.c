/*
 * fsu-level-filter.c - Source for FsuLevelFilter
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


#include <gst/filters/fsu-level-filter.h>
#include <gst/filters/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuLevelFilter, fsu_level_filter, FSU_TYPE_FILTER);

static void fsu_level_filter_dispose (GObject *object);

static GstPad *fsu_level_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_level_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static gboolean fsu_level_filter_handle_message (FsuFilter *self,
    GstMessage *message);

/* Signals */
enum
{
  LEVEL_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _FsuLevelFilterPrivate
{
  /* a list of GstElement * */
  GList *elements;
};

static void
fsu_level_filter_class_init (FsuLevelFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuLevelFilterPrivate));

  gobject_class->dispose = fsu_level_filter_dispose;

  fsufilter_class->apply = fsu_level_filter_apply;
  fsufilter_class->revert = fsu_level_filter_revert;
  fsufilter_class->handle_message = fsu_level_filter_handle_message;
  fsufilter_class->name = "level";

  /**
   * FsuLevelFilter::level:
   * @self: #FsuLevelFilter that emitted the signal
   * @level: The RMS level value in dB
   *
   * This signal is emitted when sound is transmitted and
   * determines the level of sound in dB.
   *
   */
  signals[LEVEL_SIGNAL] = g_signal_new ("level",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST,
      0,
      NULL,
      NULL,
      g_cclosure_marshal_VOID__DOUBLE,
      G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

static void
fsu_level_filter_init (FsuLevelFilter *self)
{
  FsuLevelFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_LEVEL_FILTER,
          FsuLevelFilterPrivate);

  self->priv = priv;
}


static void
fsu_level_filter_dispose (GObject *object)
{
  FsuLevelFilter *self = FSU_LEVEL_FILTER (object);
  FsuLevelFilterPrivate *priv = self->priv;
  GList *i;

  for (i = priv->elements; i; i = i->next)
    gst_object_unref (i->data);
  g_list_free (priv->elements);
  priv->elements = NULL;

  G_OBJECT_CLASS (fsu_level_filter_parent_class)->dispose (object);
}


/**
 * fsu_level_filter_new:
 *
 * Creates a new level filter.
 * This filter will add a 'level' element to the pipeline and will transform
 * its result from the #GstMessage on the bus into a signal with the RMS average
 * for all channels.
 *
 * Returns: A new #FsuLevelFilter
 * See also: #FsuLevelFilter::level
 * See also: fsu_filter_handle_message()
 */
FsuLevelFilter *
fsu_level_filter_new (void)
{
  return g_object_new (FSU_TYPE_LEVEL_FILTER, NULL);
}

static GstPad *
fsu_level_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuLevelFilter *self = FSU_LEVEL_FILTER (filter);

  return fsu_filter_add_standard_element (bin, pad, "level",
      NULL, &self->priv->elements);
  //return pad;
}

static GstPad *
fsu_level_filter_revert (FsuFilter *filter,
    GstBin *bin,
 GstPad *pad)
{
  FsuLevelFilter *self = FSU_LEVEL_FILTER (filter);

  return fsu_filter_revert_standard_element (bin, pad, &self->priv->elements);
}

static gboolean
fsu_level_filter_handle_message (FsuFilter *filter,
    GstMessage *message)
{
  FsuLevelFilter *self = FSU_LEVEL_FILTER (filter);
  const GstStructure *s = gst_message_get_structure (message);

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ELEMENT &&
      gst_structure_has_name (s, "level") &&
      g_list_find (self->priv->elements, GST_MESSAGE_SRC (message)))
  {
    gint channels;
    gdouble rms_dB;
    gdouble rms;
    const GValue *list;
    const GValue *value;
    gint i;
    gdouble level;

    /* we can get the number of channels as the length of any of the value
     * lists */
    list = gst_structure_get_value (s, "rms");
    channels = gst_value_list_get_size (list);

    rms = 0;
    for (i = 0; i < channels; ++i) {
      value = gst_value_list_get_value (list, i);
      rms_dB = g_value_get_double (value);

      rms += rms_dB;
    }
    level = rms / channels;

    g_signal_emit (self, signals[LEVEL_SIGNAL], 0, level);

    return TRUE;
  }

  return FALSE;
}
