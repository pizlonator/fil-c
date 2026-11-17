/*
 * Copyright (C) 2013 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#ifdef HAVE_IBUS
#include "cc-ibus-utils.h"

gchar *
engine_get_display_name (IBusEngineDesc *engine_desc)
{
        const gchar *name;
        const gchar *language_code;
        const gchar *language;
        const gchar *textdomain;
        gchar *display_name;

        name = ibus_engine_desc_get_longname (engine_desc);
        language_code = ibus_engine_desc_get_language (engine_desc);
        language = ibus_get_language_name (language_code);
        textdomain = ibus_engine_desc_get_textdomain (engine_desc);
        if (*textdomain != '\0' && *name != '\0')
                name = g_dgettext (textdomain, name);
        display_name = g_strdup_printf ("%s (%s)", language, name);

        return display_name;
}

#endif /* HAVE_IBUS */
