/*
 * fsu-gnome_effect-filter.c - Source for FsuGnomeEffectFilter
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

#include <gst/farsight/fsu-gnome-effect-filter.h>
#include <gst/farsight/fsu-filter-helper.h>


G_DEFINE_TYPE (FsuGnomeEffectFilter, fsu_gnome_effect_filter, FSU_TYPE_FILTER);

static void fsu_gnome_effect_filter_constructed (GObject *object);
static void fsu_gnome_effect_filter_dispose (GObject *object);
static void fsu_gnome_effect_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);
static void fsu_gnome_effect_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec);
static GstPad *fsu_gnome_effect_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);
static GstPad *fsu_gnome_effect_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad);


/* properties */
enum
{
  PROP_FILENAME = 1,
  PROP_NAME,
  PROP_PIPELINE_DESCRIPTION,
  PROP_CATEGORIES,
  LAST_PROPERTY
};



struct _FsuGnomeEffectFilterPrivate
{
  gchar *filename;
  gchar *name;
  gchar *pipeline_description;
  GList *categories;
  GError *error;
};

static void
fsu_gnome_effect_filter_class_init (FsuGnomeEffectFilterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FsuFilterClass *fsufilter_class = FSU_FILTER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (FsuGnomeEffectFilterPrivate));

  gobject_class->constructed = fsu_gnome_effect_filter_constructed;
  gobject_class->get_property = fsu_gnome_effect_filter_get_property;
  gobject_class->set_property = fsu_gnome_effect_filter_set_property;
  gobject_class->dispose = fsu_gnome_effect_filter_dispose;

  fsufilter_class->apply = fsu_gnome_effect_filter_apply;
  fsufilter_class->revert = fsu_gnome_effect_filter_revert;
  fsufilter_class->name = "gnome-effect";

  g_object_class_install_property (gobject_class, PROP_FILENAME,
      g_param_spec_string ("filename", "The filename of the Gnome effect",
          "The filename of the Gnome effect to read and build",
          NULL,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NAME,
      g_param_spec_string ("name", "The name of the effect",
          "The name of the Gnome effect taken from the file",
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PIPELINE_DESCRIPTION,
      g_param_spec_string ("pipeline-description",
          "The pipeline description of the effect",
          "The pipeline description of the Gnome effect taken from the file",
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CATEGORIES,
      g_param_spec_boxed ("categories",
          "The categories of the effect",
          "The categories of the Gnome effect taken from the file",
          FSU_TYPE_GNOME_EFFECT_CATEGORIES_LIST,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}

static void
fsu_gnome_effect_filter_init (FsuGnomeEffectFilter *self)
{
  FsuGnomeEffectFilterPrivate *priv =
      G_TYPE_INSTANCE_GET_PRIVATE (self, FSU_TYPE_GNOME_EFFECT_FILTER,
          FsuGnomeEffectFilterPrivate);

  self->priv = priv;
}

static void
fsu_gnome_effect_filter_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  FsuGnomeEffectFilter *self = FSU_GNOME_EFFECT_FILTER (object);
  FsuGnomeEffectFilterPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_FILENAME:
      g_value_set_string (value, priv->filename);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_PIPELINE_DESCRIPTION:
      g_value_set_string (value, priv->pipeline_description);
      break;
    case PROP_CATEGORIES:
      g_value_set_boxed (value, priv->categories);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_gnome_effect_filter_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  FsuGnomeEffectFilter *self = FSU_GNOME_EFFECT_FILTER (object);
  FsuGnomeEffectFilterPrivate *priv = self->priv;

  switch (property_id)
  {
    case PROP_FILENAME:
      priv->filename = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
fsu_gnome_effect_filter_constructed (GObject *object)
{
  FsuGnomeEffectFilter *self = FSU_GNOME_EFFECT_FILTER (object);
  FsuGnomeEffectFilterPrivate *priv = self->priv;
  GKeyFile *key_file = g_key_file_new ();
  gchar ** categories = NULL;

  if (!g_key_file_load_from_file (key_file, priv->filename,
          G_KEY_FILE_NONE, &priv->error))
    goto done;

  priv->name = g_key_file_get_locale_string (key_file, "Effect",
      "Name", NULL, &priv->error);
  if (priv->error)
    goto done;

  priv->pipeline_description = g_key_file_get_string (key_file, "Effect",
      "PipelineDescription", &priv->error);
  if (priv->error)
    goto done;

  categories = g_key_file_get_string_list (key_file, "Effect",
      "Category", NULL, NULL);
  if (categories)
  {
    gchar **ptr = categories;
    while (*ptr)
    {
      priv->categories = g_list_append (priv->categories, g_strdup (*ptr));
      ptr++;
    }
    g_strfreev(categories);
  }

 done:
  g_key_file_free (key_file);
  return;
}

static void
fsu_gnome_effect_filter_dispose (GObject *object)
{
  FsuGnomeEffectFilter *self = FSU_GNOME_EFFECT_FILTER (object);
  FsuGnomeEffectFilterPrivate *priv = self->priv;

  g_free (priv->filename);
  priv->filename = NULL;
  g_free (priv->name);
  priv->name = NULL;
  g_free (priv->pipeline_description);
  priv->pipeline_description = NULL;
  g_list_foreach (priv->categories, (GFunc) g_free, NULL);
  g_list_free (priv->categories);
  priv->categories = NULL;


  G_OBJECT_CLASS (fsu_gnome_effect_filter_parent_class)->dispose (object);
}


/**
 * fsu_gnome_effect_filter_new:
 *
 * Creates a new Gnome effect filter
 * Returns: A new #FsuGnomeEffectFilter
 */
FsuGnomeEffectFilter *
fsu_gnome_effect_filter_new (const gchar *filename,
    GError **error)
{
  FsuGnomeEffectFilter *self = NULL;

  g_return_val_if_fail (filename, NULL);
  g_return_val_if_fail (g_file_test (filename,
          G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR), NULL);

  self = g_object_new (FSU_TYPE_GNOME_EFFECT_FILTER,
      "filename", filename,
      NULL);

  if (self->priv->error)
  {
    g_propagate_error (error, self->priv->error);
    g_object_unref (self);
    return NULL;
  }

  return self;
}


static GstPad *
fsu_gnome_effect_filter_apply (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  FsuGnomeEffectFilter *self = FSU_GNOME_EFFECT_FILTER (filter);
  FsuGnomeEffectFilterPrivate *priv = self->priv;
  GstElement *effect = NULL;
  GstPad *out_pad = NULL;
  gchar *description = NULL;

  description = g_strdup_printf ("ffmpegcolorspace ! %s",
      priv->pipeline_description);

  effect = fsu_filter_add_element_by_description (bin, pad,
      description, &out_pad);

  g_free (description);

  if (effect)
    gst_object_unref (effect);

  return out_pad;
}

static GstPad *
fsu_gnome_effect_filter_revert (FsuFilter *filter,
    GstBin *bin,
    GstPad *pad)
{
  return fsu_filter_revert_bin (bin, pad);
}




void
fsu_gnome_effect_categories_list_destroy (GList *categories_list)
{
  g_list_foreach (categories_list, (GFunc) g_free, NULL);
  g_list_free (categories_list);
}

GList *
fsu_gnome_effect_categories_list_copy (const GList *categories_list)
{
  GList *copy = NULL;
  const GList *lp;

  for (lp = categories_list; lp; lp = g_list_next (lp)) {
    /* prepend then reverse the list for efficiency */
    copy = g_list_prepend (copy, g_strdup (lp->data));
  }
  copy = g_list_reverse (copy);
  return copy;
}

GType
fsu_gnome_effect_categories_list_get_type (void)
{
  static GType categories_list_type = 0;
  if (categories_list_type == 0)
  {
    categories_list_type = g_boxed_type_register_static (
        "FsuGnomeEffectCategoriesGList",
        (GBoxedCopyFunc)fsu_gnome_effect_categories_list_copy,
        (GBoxedFreeFunc)fsu_gnome_effect_categories_list_destroy);
  }

  return categories_list_type;
}


GList *
fsu_gnome_effect_list_effects (const gchar *directory)
{
  GDir *dir = NULL;
  GList *effects = NULL;

  g_return_val_if_fail (directory, NULL);
  g_return_val_if_fail (g_file_test (directory,
          G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR), NULL);

  dir = g_dir_open (directory, 0, NULL);

  if (dir)
  {
    const gchar *name = g_dir_read_name (dir);

    while (name)
    {
      if (g_str_has_suffix (name, ".effect"))
        effects = g_list_append (effects,
            g_strdup_printf ("%s/%s", directory, name));
      name = g_dir_read_name (dir);
    }
    g_dir_close (dir);
  }
  return effects;
}


void
fsu_gnome_effect_effects_list_destroy (GList *effects_list)
{
  g_list_foreach (effects_list, (GFunc) g_free, NULL);
  g_list_free (effects_list);
}
