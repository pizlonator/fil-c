/*
 * Copyright (C) 2011 Red Hat, Inc
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
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include "hostname-helper.h"

static char *
allowed_chars (void)
{
	GString *s;
	char i;

	s = g_string_new (NULL);
	for (i = 'a'; i <= 'z'; i++)
		g_string_append_c (s, i);
	for (i = 'A'; i <= 'Z'; i++)
		g_string_append_c (s, i);
	for (i = '0'; i <= '9'; i++)
		g_string_append_c (s, i);
	g_string_append_c (s, '-');

	return g_string_free (s, FALSE);
}

static char *
remove_leading_dashes (char *input)
{
	char *start;

	for (start = input; *start && (*start == '-'); start++)
		;

	memmove (input, start, strlen (start) + 1);

	return input;
}

static gboolean
is_empty (const char *input)
{
	if (input == NULL ||
	    *input == '\0')
		return TRUE;
	return FALSE;
}

static char *
remove_trailing_dashes (char *input)
{
	int len;

	len = strlen (input);
	while (len--) {
		if (input[len] == '-')
			input[len] = '\0';
		else
			break;
	}
	return input;
}

static char *
remove_apostrophes (char *input)
{
	char *apo;

	while ((apo = strchr (input, '\'')) != NULL)
		memmove (apo, apo + 1, strlen (apo));
	return input;
}

static char *
remove_duplicate_dashes (char *input)
{
	char *dashes;

	while ((dashes = strstr (input, "--")) != NULL)
		memmove (dashes, dashes + 1, strlen (dashes));
	return input;
}

static gboolean
is_valid_fqdn_label (const char *label)
{
	int label_len, i;

	/* Each label must be at least 1 alphanum/hyphen chars long,
	   and can't start/end with a hyphen */
	label_len = strlen (label);
	if (label_len == 0)
		return FALSE;

	if (label[0] == '-' || label[label_len - 1] == '-')
		return FALSE;

	for (i = 0; i < label_len; i++)
		if (!g_ascii_isalnum (label[i]) && label[i] != '-')
			return FALSE;

	return TRUE;
}

static gboolean
is_valid_fqdn (const char *input)
{
	g_auto(GStrv) split_input = NULL;
	int i;

	split_input = g_strsplit (input, ".", -1);

	if (split_input[0] == NULL)
		return FALSE;

	for (i = 0; split_input[i] != NULL; i++)
		if (!is_valid_fqdn_label (split_input[i]))
			return FALSE;

	return TRUE;
}

#define CHECK	if (is_empty (result)) return g_strdup ("localhost")

char *
pretty_hostname_to_static (const char *pretty,
			   gboolean    for_display)
{
	g_autofree gchar *result = NULL;
	g_autofree gchar *valid_chars = NULL;
	g_autofree gchar *composed = NULL;

	g_return_val_if_fail (pretty != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (pretty, -1, NULL), NULL);

	g_debug ("Input: '%s'", pretty);

	/* We allow setting static fqdn which have periods in them. Upper case is kept, as
	   resolving is case-insensitive */
	if (is_valid_fqdn (pretty))
		return g_strdup (pretty);

	composed = g_utf8_normalize (pretty, -1, G_NORMALIZE_ALL_COMPOSE);
	g_debug ("\tcomposed: '%s'", composed);
	/* Transform the pretty hostname to ASCII */
	result = g_str_to_ascii (composed, NULL);
	g_debug ("\ttranslit: '%s'", result);

	CHECK;

	/* Remove apostrophes */
	remove_apostrophes (result);
	g_debug ("\tapostrophes: '%s'", result);

	CHECK;

	/* Remove all the not-allowed chars */
	valid_chars = allowed_chars ();
	g_strcanon (result, valid_chars, '-');
	g_debug ("\tcanon: '%s'", result);

	CHECK;

	/* Remove the leading dashes */
	remove_leading_dashes (result);
	g_debug ("\tleading: '%s'", result);

	CHECK;

	/* Remove trailing dashes */
	remove_trailing_dashes (result);
	g_debug ("\ttrailing: '%s'", result);

	CHECK;

	/* Remove duplicate dashes */
	remove_duplicate_dashes (result);
	g_debug ("\tduplicate: '%s'", result);

	CHECK;

	/* Lower case */
	if (!for_display)
		return g_ascii_strdown (result, -1);

	return g_steal_pointer (&result);
}
#undef CHECK

/* Max length of an SSID in bytes */
#define SSID_MAX_LEN 32
char *
pretty_hostname_to_ssid (const char *pretty)
{
	const char *p, *prev;

	if (pretty == NULL || *pretty == '\0') {
		pretty = g_get_host_name ();
		if (g_strcmp0 (pretty, "localhost") == 0)
			pretty = NULL;
	}

	if (pretty == NULL) {
		/* translators: This is the default hotspot name, need to be less than 32-bytes */
		gchar *ret = g_strdup (C_("hotspot", "Hotspot"));
		g_assert (strlen (ret) <= SSID_MAX_LEN);
		return ret;
	}

	g_return_val_if_fail (g_utf8_validate (pretty, -1, NULL), NULL);

	p = pretty;
	prev = NULL;
	while ((p = g_utf8_find_next_char (p, NULL)) != NULL) {
		if (p == prev)
			break;

		if (p - pretty > SSID_MAX_LEN) {
			return g_strndup (pretty, prev - pretty);
		}
		if (p - pretty == SSID_MAX_LEN) {
			return g_strndup (pretty, p - pretty);
		}

		if (*p == '\0')
			break;

		prev = p;
	}

	return g_strdup (pretty);
}
