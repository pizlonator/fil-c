/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <fontconfig/fontconfig.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "cc-common-language.h"
#include "shell/cc-object-storage.h"

static char *get_lang_for_user_object_path (const char *path);

static gboolean
iter_for_language (GtkTreeModel *model,
                   const gchar  *lang,
                   GtkTreeIter  *iter,
                   gboolean      region)
{
        g_autofree gchar *name = NULL;

        g_assert (gtk_tree_model_get_iter_first (model, iter));
        do {
                g_autofree gchar *l = NULL;
                gtk_tree_model_get (model, iter, LOCALE_COL, &l, -1);
                if (g_strcmp0 (l, lang) == 0)
                        return TRUE;
        } while (gtk_tree_model_iter_next (model, iter));

        name = gnome_normalize_locale (lang);
        if (name != NULL) {
                g_autofree gchar *language = NULL;

                if (region) {
                        language = gnome_get_country_from_locale (name, NULL);
                }
                else {
                        language = gnome_get_language_from_locale (name, NULL);
                }

                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                                   iter,
                                                   -1,
                                                   LOCALE_COL, name,
                                                   DISPLAY_LOCALE_COL, language,
                                                   -1);
                return TRUE;
        }

        return FALSE;
}

gboolean
cc_common_language_get_iter_for_language (GtkTreeModel *model,
                                          const gchar  *lang,
                                          GtkTreeIter  *iter)
{
  return iter_for_language (model, lang, iter, FALSE);
}

gboolean
cc_common_language_has_font (const gchar *locale)
{
        const FcCharSet  *charset;
        FcPattern        *pattern;
        FcObjectSet      *object_set;
        FcFontSet        *font_set;
        g_autofree gchar *language_code = NULL;
        gboolean          is_displayable;

        is_displayable = FALSE;
        pattern = NULL;
        object_set = NULL;
        font_set = NULL;

        if (!gnome_parse_locale (locale, &language_code, NULL, NULL, NULL))
                return FALSE;

        charset = FcLangGetCharSet ((FcChar8 *) language_code);
        if (!charset) {
                /* fontconfig does not know about this language */
                is_displayable = TRUE;
        }
        else {
                /* see if any fonts support rendering it */
                pattern = FcPatternBuild (NULL, FC_LANG, FcTypeString, language_code, NULL);

                if (pattern == NULL)
                        goto done;

                object_set = FcObjectSetCreate ();

                if (object_set == NULL)
                        goto done;

                font_set = FcFontList (NULL, pattern, object_set);

                if (font_set == NULL)
                        goto done;

                is_displayable = (font_set->nfont > 0);
        }

 done:
        if (font_set != NULL)
                FcFontSetDestroy (font_set);

        if (object_set != NULL)
                FcObjectSetDestroy (object_set);

        if (pattern != NULL)
                FcPatternDestroy (pattern);

        return is_displayable;
}

gchar *
cc_common_language_get_current_language (void)
{
        gchar *language;
        g_autofree gchar *path = NULL;
        const gchar *locale;

	path = g_strdup_printf ("/org/freedesktop/Accounts/User%d", getuid ());
        language = get_lang_for_user_object_path (path);
        if (language != NULL && *language != '\0')
                return language;

        locale = (const gchar *) setlocale (LC_MESSAGES, NULL);
        if (locale)
                language = gnome_normalize_locale (locale);
        else
                language = NULL;

        return language;
}

static char *
get_lang_for_user_object_path (const char *path)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GDBusProxy) user = NULL;
	g_autoptr(GVariant) props = NULL;
	char *lang;

	user = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SYSTEM,
							 G_DBUS_PROXY_FLAGS_NONE,
							 "org.freedesktop.Accounts",
							 path,
							 "org.freedesktop.Accounts.User",
							 NULL,
							 &error);
	if (user == NULL) {
		g_warning ("Failed to get proxy for user '%s': %s",
			   path, error->message);
		return NULL;
	}

	props = g_dbus_proxy_get_cached_property (user, "Language");
	if (props == NULL)
		return NULL;
	lang = g_variant_dup_string (props, NULL);

	return lang;
}

/*
 * Note that @lang needs to be formatted like the locale strings
 * returned by gnome_get_all_locales().
 */
static void
insert_language (GHashTable *ht,
                 const char *lang)
{
        g_autofree gchar *label_own_lang = NULL;
        g_autofree gchar *label_current_lang = NULL;
        g_autofree gchar *label_untranslated = NULL;

        label_own_lang = gnome_get_language_from_locale (lang, lang);
        label_current_lang = gnome_get_language_from_locale (lang, NULL);
        label_untranslated = gnome_get_language_from_locale (lang, "C");

        /* We don't have a translation for the label in
         * its own language? */
        if (g_strcmp0 (label_own_lang, label_untranslated) == 0) {
                if (g_strcmp0 (label_current_lang, label_untranslated) == 0)
                        g_hash_table_insert (ht, g_strdup (lang), g_strdup (label_untranslated));
                else
                        g_hash_table_insert (ht, g_strdup (lang), g_strdup (label_current_lang));
        } else {
                g_hash_table_insert (ht, g_strdup (lang), g_strdup (label_own_lang));
        }
}

GHashTable *
cc_common_language_get_initial_languages (void)
{
        GHashTable *ht;

        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

        insert_language (ht, "en_US.UTF-8");
        insert_language (ht, "en_GB.UTF-8");
        insert_language (ht, "de_DE.UTF-8");
        insert_language (ht, "fr_FR.UTF-8");
        insert_language (ht, "es_ES.UTF-8");
        insert_language (ht, "zh_CN.UTF-8");
        insert_language (ht, "ja_JP.UTF-8");
        insert_language (ht, "ru_RU.UTF-8");
        insert_language (ht, "ar_EG.UTF-8");

        return ht;
}

static void
foreach_user_lang_cb (gpointer key,
                      gpointer value,
                      gpointer user_data)
{
        GtkListStore *store = (GtkListStore *) user_data;
        const char *locale = (const char *) key;
        const char *display_locale = (const char *) value;
        GtkTreeIter iter;

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            LOCALE_COL, locale,
                            DISPLAY_LOCALE_COL, display_locale,
                            -1);
}

void
cc_common_language_add_user_languages (GtkTreeModel *model)
{
        g_autofree gchar *name = NULL;
        GtkTreeIter iter;
        GtkListStore *store = GTK_LIST_STORE (model);
        GHashTable *user_langs;
        const char *display;

        gtk_list_store_clear (store);

        user_langs = cc_common_language_get_initial_languages ();

        /* Add the current locale first */
        name = cc_common_language_get_current_language ();
        display = g_hash_table_lookup (user_langs, name);
        if (!display) {
                g_autofree gchar *language = NULL;
                g_autofree gchar *country = NULL;
                g_autofree gchar *codeset = NULL;

                gnome_parse_locale (name, &language, &country, &codeset, NULL);

                if (!codeset || !g_str_equal (codeset, "UTF-8"))
                        g_warning ("Current user locale codeset isn't UTF-8");

                g_free (name);
                name = g_strdup_printf ("%s_%s.UTF-8", language, country);

                insert_language (user_langs, name);
                display = g_hash_table_lookup (user_langs, name);
        }

        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, name, DISPLAY_LOCALE_COL, display, -1);
        g_hash_table_remove (user_langs, name);

        /* The rest of the languages */
        g_hash_table_foreach (user_langs, (GHFunc) foreach_user_lang_cb, store);

        /* And now the "Other…" selection */
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, LOCALE_COL, NULL, DISPLAY_LOCALE_COL, _("Other…"), -1);

        g_hash_table_destroy (user_langs);
}

