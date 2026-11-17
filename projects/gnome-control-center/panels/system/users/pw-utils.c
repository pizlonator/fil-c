/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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

#include "pw-utils.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <pwquality.h>

#include "shell/cc-object-storage.h"

static pwquality_settings_t *
get_pwq (void)
{
        g_autoptr(GObject) wrapper = NULL;
        pwquality_settings_t *settings = NULL;

        if (cc_object_storage_has_object (CC_OBJECT_PWQ_SETTINGS)) {
                wrapper = cc_object_storage_get_object (CC_OBJECT_PWQ_SETTINGS);
                settings = g_object_get_data (wrapper, CC_OBJECT_PWQ_SETTINGS);
        } else {
                gchar *err = NULL;
                gint rv = 0;

                settings = pwquality_default_settings ();
                pwquality_set_int_value (settings, PWQ_SETTING_MAX_SEQUENCE, 4);

                rv = pwquality_read_config (settings, NULL, (gpointer)&err);
                if (rv < 0) {
                        g_warning ("failed to read pwquality configuration: %s\n",
                                   pwquality_strerror (NULL, 0, rv, err));
                        pwquality_free_settings (settings);

                        /* Load just default settings in case of failure. */
                        settings = pwquality_default_settings ();
                        pwquality_set_int_value (settings, PWQ_SETTING_MAX_SEQUENCE, 4);
                }

                wrapper = g_object_new (G_TYPE_OBJECT, NULL);
                g_object_set_data_full (wrapper, CC_OBJECT_PWQ_SETTINGS, settings,
                                        (GDestroyNotify) pwquality_free_settings);
                cc_object_storage_add_object (CC_OBJECT_PWQ_SETTINGS, wrapper);
        }

        return settings;
}

gint
pw_min_length (void)
{
        gint value = 0;
        gint rv;

        rv = pwquality_get_int_value (get_pwq (), PWQ_SETTING_MIN_LENGTH, &value);
        if (rv < 0) {
                g_warning ("Failed to read pwquality setting: %s\n",
                           pwquality_strerror (NULL, 0, rv, NULL));
        }

        return value;
}

gchar *
pw_generate (void)
{
        gchar *res;
        gint rv;

        rv = pwquality_generate (get_pwq (), 0, &res);

        if (rv < 0) {
                g_warning ("Password generation failed: %s\n",
                           pwquality_strerror (NULL, 0, rv, NULL));
                return NULL;
        }

        return res;
}

static const gchar *
pw_error_hint (gint error)
{
        switch (error) {
        case PWQ_ERROR_SAME_PASSWORD:
                return C_("Password hint", "The new password needs to be different from the old one.");
        case PWQ_ERROR_CASE_CHANGES_ONLY:
                return C_("Password hint", "Try changing some letters and numbers.");
        case PWQ_ERROR_TOO_SIMILAR:
                return C_("Password hint", "Try changing the password a bit more.");
        case PWQ_ERROR_USER_CHECK:
                return C_("Password hint", "A password without your user name would be stronger.");
        case PWQ_ERROR_GECOS_CHECK:
                return C_("Password hint", "Try to avoid using your name in the password.");
        case PWQ_ERROR_BAD_WORDS:
                return C_("Password hint", "Try to avoid some of the words included in the password.");
        case PWQ_ERROR_ROTATED:
                return C_("Password hint", "Try changing the password a bit more.");
        case PWQ_ERROR_CRACKLIB_CHECK:
                return C_("Password hint", "Try to avoid common words.");
        case PWQ_ERROR_PALINDROME:
                return C_("Password hint", "Try to avoid reordering existing words.");
        case PWQ_ERROR_MIN_DIGITS:
                return C_("Password hint", "Try to use more numbers.");
        case PWQ_ERROR_MIN_UPPERS:
                return C_("Password hint", "Try to use more uppercase letters.");
        case PWQ_ERROR_MIN_LOWERS:
                return C_("Password hint", "Try to use more lowercase letters.");
        case PWQ_ERROR_MIN_OTHERS:
                return C_("Password hint", "Try to use more special characters, like punctuation.");
        case PWQ_ERROR_MIN_CLASSES:
                return C_("Password hint", "Try to use a mixture of letters, numbers and punctuation.");
        case PWQ_ERROR_MAX_CONSECUTIVE:
                return C_("Password hint", "Try to avoid repeating the same character.");
        case PWQ_ERROR_MAX_CLASS_REPEAT:
                return C_("Password hint", "Try to avoid repeating the same type of character: you need to mix up letters, numbers and punctuation.");
        case PWQ_ERROR_MAX_SEQUENCE:
                return C_("Password hint", "Try to avoid sequences like 1234 or abcd.");
        case PWQ_ERROR_MIN_LENGTH:
                return C_("Password hint", "Password needs to be longer. Try to add more letters, numbers and punctuation.");
        case PWQ_ERROR_EMPTY_PASSWORD:
                return C_("Password hint", "Mix uppercase and lowercase and try to use a number or two.");
        default:
                return C_("Password hint", "Adding more letters, numbers and punctuation will make the password stronger.");
        }
}

gdouble
pw_strength (const gchar  *password,
             const gchar  *old_password,
             const gchar  *username,
             const gchar **hint,
             gint         *strength_level)
{
        gint rv, level, length = 0;
        gdouble strength = 0.0;

        rv = pwquality_check (get_pwq (),
                              password, old_password, username,
                              NULL);

        if (password != NULL)
                length = strlen (password);

        strength = CLAMP (0.01 * rv, 0.0, 1.0);
        if (rv < 0) {
                level = (length > 0) ? 1 : 0;
        }
        else if (strength < 0.50) {
                level = 2;
        } else if (strength < 0.75) {
                level = 3;
        } else if (strength < 0.90) {
                level = 4;
        } else {
                level = 5;
        }

        if (length && length < pw_min_length())
                *hint = pw_error_hint (PWQ_ERROR_MIN_LENGTH);
        else
                *hint = pw_error_hint (rv);

        if (strength_level)
                *strength_level = level;

        return strength;
}
