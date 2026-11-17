/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "cc-language-row.h"
#include "cc-common-resources.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

struct _CcLanguageRow {
  GtkListBoxRow parent_instance;

  GtkImage *check_image;
  GtkLabel *country_label;
  GtkLabel *language_label;

  gchar *locale_id;
  gchar *language;
  gchar *language_local;
  gchar *country;
  gchar *country_local;

  gboolean is_extra;
};

G_DEFINE_TYPE (CcLanguageRow, cc_language_row, GTK_TYPE_LIST_BOX_ROW)

static gchar *
get_language_label (const gchar *language_code,
                    const gchar *modifier,
                    const gchar *locale_id)
{
  g_autofree gchar *language = NULL;

  language = gnome_get_language_from_code (language_code, locale_id);

  if (modifier == NULL)
    return g_steal_pointer (&language);
  else
    {
      g_autofree gchar *t_mod = gnome_get_translated_modifier (modifier, locale_id);
      return g_strdup_printf ("%s — %s", language, t_mod);
    }
}

static void
cc_language_row_dispose (GObject *object)
{
  CcLanguageRow *self = CC_LANGUAGE_ROW (object);

  g_clear_pointer (&self->locale_id, g_free);
  g_clear_pointer (&self->country, g_free);
  g_clear_pointer (&self->country_local, g_free);
  g_clear_pointer (&self->language, g_free);
  g_clear_pointer (&self->language_local, g_free);

  G_OBJECT_CLASS (cc_language_row_parent_class)->dispose (object);
}

void
cc_language_row_class_init (CcLanguageRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_language_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-language-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcLanguageRow, check_image);
  gtk_widget_class_bind_template_child (widget_class, CcLanguageRow, country_label);
  gtk_widget_class_bind_template_child (widget_class, CcLanguageRow, language_label);
}

void
cc_language_row_init (CcLanguageRow *self)
{
  g_resources_register (cc_common_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}

CcLanguageRow *
cc_language_row_new (const gchar *locale_id)
{
  CcLanguageRow *self;
  g_autofree gchar *language_code = NULL;
  g_autofree gchar *country_code = NULL;
  g_autofree gchar *modifier = NULL;

  self = CC_LANGUAGE_ROW (g_object_new (CC_TYPE_LANGUAGE_ROW, NULL));
  self->locale_id = g_strdup (locale_id);

  gnome_parse_locale (locale_id, &language_code, &country_code, NULL, &modifier);

  self->language = get_language_label (language_code, modifier, locale_id);
  self->language_local = get_language_label (language_code, modifier, NULL);
  gtk_label_set_label (self->language_label, self->language);

  if (country_code == NULL)
    {
      self->country = NULL;
      self->country_local = NULL;
    }
  else
    {
      self->country = gnome_get_country_from_code (country_code, locale_id);
      self->country_local = gnome_get_country_from_code (country_code, NULL);
      gtk_label_set_label (self->country_label, self->country);
    }

  return self;
}

const gchar *
cc_language_row_get_locale_id (CcLanguageRow *self)
{
  g_return_val_if_fail (CC_IS_LANGUAGE_ROW (self), NULL);
  return self->locale_id;
}

const gchar *
cc_language_row_get_language (CcLanguageRow *self)
{
  g_return_val_if_fail (CC_IS_LANGUAGE_ROW (self), NULL);
  return self->language;
}

const gchar *
cc_language_row_get_language_local (CcLanguageRow *self)
{
  g_return_val_if_fail (CC_IS_LANGUAGE_ROW (self), NULL);
  return self->language_local;
}

const gchar *
cc_language_row_get_country (CcLanguageRow *self)
{
  g_return_val_if_fail (CC_IS_LANGUAGE_ROW (self), NULL);
  return self->country;
}

const gchar *
cc_language_row_get_country_local (CcLanguageRow *self)
{
  g_return_val_if_fail (CC_IS_LANGUAGE_ROW (self), NULL);
  return self->country_local;
}

void
cc_language_row_set_checked (CcLanguageRow *self, gboolean checked)
{
  g_return_if_fail (CC_IS_LANGUAGE_ROW (self));
  gtk_widget_set_visible (GTK_WIDGET (self->check_image), checked);
}

void
cc_language_row_set_is_extra (CcLanguageRow *self, gboolean is_extra)
{
  g_return_if_fail (CC_IS_LANGUAGE_ROW (self));
  self->is_extra = is_extra;
}

gboolean
cc_language_row_get_is_extra (CcLanguageRow *self)
{
  g_return_val_if_fail (CC_IS_LANGUAGE_ROW (self), FALSE);
  return self->is_extra;
}
