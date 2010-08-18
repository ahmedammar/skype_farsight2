/*
 * fsu-gnome-effect-filter.h - Header for FsuGnomeEffectFilter
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

#ifndef __FSU_GNOME_EFFECT_FILTER_H__
#define __FSU_GNOME_EFFECT_FILTER_H__

#include <gst/filters/fsu-filter.h>


G_BEGIN_DECLS

#define FSU_TYPE_GNOME_EFFECT_FILTER                 \
  (fsu_gnome_effect_filter_get_type())
#define FSU_GNOME_EFFECT_FILTER(obj)                 \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),           \
      FSU_TYPE_GNOME_EFFECT_FILTER,                  \
      FsuGnomeEffectFilter))
#define FSU_GNOME_EFFECT_FILTER_CLASS(klass)         \
  (G_TYPE_CHECK_CLASS_CAST ((klass),            \
      FSU_TYPE_GNOME_EFFECT_FILTER,                  \
      FsuGnomeEffectFilterClass))
#define FSU_IS_GNOME_EFFECT_FILTER(obj)              \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),           \
      FSU_TYPE_GNOME_EFFECT_FILTER))
#define FSU_IS_GNOME_EFFECT_FILTER_CLASS(klass)      \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),            \
      FSU_TYPE_GNOME_EFFECT_FILTER))
#define FSU_GNOME_EFFECT_FILTER_GET_CLASS(obj)       \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),            \
      FSU_TYPE_GNOME_EFFECT_FILTER,                  \
      FsuGnomeEffectFilterClass))

#define FSU_TYPE_GNOME_EFFECT_CATEGORIES_LIST \
  (fsu_gnome_effect_categories_list_get_type ())

typedef struct _FsuGnomeEffectFilter      FsuGnomeEffectFilter;
typedef struct _FsuGnomeEffectFilterClass FsuGnomeEffectFilterClass;
typedef struct _FsuGnomeEffectFilterPrivate FsuGnomeEffectFilterPrivate;

struct _FsuGnomeEffectFilterClass
{
  FsuFilterClass parent_class;
};

/**
 * FsuGnomeEffectFilter:
 *
 * A Gnome Video Effect filter
 */
struct _FsuGnomeEffectFilter
{
  FsuFilter parent;
  /*< private >*/
  FsuGnomeEffectFilterPrivate *priv;
};

GType fsu_gnome_effect_filter_get_type (void) G_GNUC_CONST;
GType fsu_gnome_effect_categories_list_get_type (void);

FsuGnomeEffectFilter *fsu_gnome_effect_filter_new (const gchar *filename,
    GError **error);

void fsu_gnome_effect_categories_list_destroy (GList *categories_list);
GList *fsu_gnome_effect_categories_list_copy (const GList *categories_list);
GList *fsu_gnome_effect_list_effects (const gchar *directory);
void fsu_gnome_effect_effects_list_destroy (GList *effects_list);

G_END_DECLS

#endif /* __FSU_GNOME_EFFECT_FILTER_H__ */
