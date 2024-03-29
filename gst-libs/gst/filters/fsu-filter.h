/*
 * fsu-filter.h - Header for FsuFilter
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

#ifndef __FSU_FILTER_H__
#define __FSU_FILTER_H__

#include <gst/gst.h>


G_BEGIN_DECLS

#define FSU_TYPE_FILTER                         \
  (fsu_filter_get_type())
#define FSU_FILTER(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_FILTER,                          \
      FsuFilter))
#define FSU_FILTER_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_FILTER,                          \
      FsuFilterClass))
#define FSU_IS_FILTER(obj)                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_FILTER))
#define FSU_IS_FILTER_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_FILTER))
#define FSU_FILTER_GET_CLASS(obj)               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_FILTER,                          \
      FsuFilterClass))

typedef struct _FsuFilter      FsuFilter;
typedef struct _FsuFilterClass FsuFilterClass;
typedef struct _FsuFilterPrivate FsuFilterPrivate;

/**
 * FsuFilterClass:
 * @parent_class: The parent class
 * @apply: Apply the filter on the #GstPad
 * @revert: Revert the filter from the #GstPad
 * @handle_message: Handle the #GstMessage
 * @name: The name of the filter
 */
struct _FsuFilterClass
{
  GObjectClass parent_class;
  GstPad *(*apply) (FsuFilter *self,
      GstBin *bin,
      GstPad *pad);
  GstPad *(*revert) (FsuFilter *self,
      GstBin *bin,
      GstPad *pad);
  gboolean (*handle_message) (FsuFilter *self,
      GstMessage *message);
  gchar *name;
};

/**
 * FsuFilter:
 *
 * A Filter
 */
struct _FsuFilter
{
  GObject parent;
  /*< private >*/
  FsuFilterPrivate *priv;
};

GType fsu_filter_get_type (void) G_GNUC_CONST;

/**
 * FSU_DEFINE_FILTER:
 * @type: The type of the class to be defined
 * @filter_name: The name of the filter to define
 *
 * This will define a new #FsuFilter derived class by calling G_DEFINE_TYPE()
 * and by declaring and setting the apply, revert and name fields of the
 * #FsuFilter parent class. The defined apply/revert methods will be
 * fsu_$(filter_name)_filter_apply and fsu_$(filter_name)_filter_revert
 */
#define FSU_DEFINE_FILTER(type, filter_name) \
  G_DEFINE_TYPE (type, fsu_##filter_name##_filter, FSU_TYPE_FILTER);    \
                                                                        \
  static GstPad *fsu_##filter_name##_filter_apply (FsuFilter *filter,   \
      GstBin *bin,                                                      \
      GstPad *pad);                                                     \
  static GstPad *fsu_##filter_name##_filter_revert (FsuFilter *filter,  \
      GstBin *bin,                                                      \
      GstPad *pad);                                                     \
                                                                        \
  static void                                                           \
  fsu_##filter_name##_filter_class_init (type##Class *klass)            \
  {                                                                     \
    FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);         \
                                                                        \
    fsufilter_class->apply = fsu_##filter_name##_filter_apply;          \
    fsufilter_class->revert = fsu_##filter_name##_filter_revert;        \
    fsufilter_class->name = #filter_name;                               \
  }

GstPad *fsu_filter_apply (FsuFilter *self,
    GstBin *bin,
    GstPad *pad);
GstPad *fsu_filter_revert (FsuFilter *self,
    GstBin *bin,
    GstPad *pad);
GstPad *fsu_filter_follow (FsuFilter *self,
    GstPad *pad);
gboolean fsu_filter_handle_message (FsuFilter *self,
    GstMessage *message);

G_END_DECLS

#endif /* __FSU_FILTER_H__ */
