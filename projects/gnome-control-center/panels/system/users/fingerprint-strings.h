/*
 * Helper functions to translate statuses and actions to strings
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 * 
 * Experimental code. This will be moved out of fprintd into it's own
 * package once the system has matured.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

struct {
	const char *dbus_name;
	const char *place_str_generic;
	const char *place_str_specific;
	const char *swipe_str_generic;
	const char *swipe_str_specific;
} fingers[] = {
	{ "any",
	  N_("Place your finger on the fingerprint reader"),
	  N_("Place your finger on %s"),
	  N_("Swipe your finger across the fingerprint reader"),
	  N_("Swipe your finger across %s") },
	{ "left-thumb",
	  N_("Place your left thumb on the fingerprint reader"),
	  N_("Place your left thumb on %s"),
	  N_("Swipe your left thumb across the fingerprint reader"),
	  N_("Swipe your left thumb across %s") },
	{ "left-index-finger",
	  N_("Place your left index finger on the fingerprint reader"),
	  N_("Place your left index finger on %s"),
	  N_("Swipe your left index finger across the fingerprint reader"),
	  N_("Swipe your left index finger across %s") },
	{ "left-middle-finger",
	  N_("Place your left middle finger on the fingerprint reader"),
	  N_("Place your left middle finger on %s"),
	  N_("Swipe your left middle finger across the fingerprint reader"),
	  N_("Swipe your left middle finger across %s") },
	{ "left-ring-finger",
	  N_("Place your left ring finger on the fingerprint reader"),
	  N_("Place your left ring finger on %s"),
	  N_("Swipe your left ring finger across the fingerprint reader"),
	  N_("Swipe your left ring finger across %s") },
	{ "left-little-finger",
	  N_("Place your left little finger on the fingerprint reader"),
	  N_("Place your left little finger on %s"),
	  N_("Swipe your left little finger across the fingerprint reader"),
	  N_("Swipe your left little finger across %s") },
	{ "right-thumb",
	  N_("Place your right thumb on the fingerprint reader"),
	  N_("Place your right thumb on %s"),
	  N_("Swipe your right thumb across the fingerprint reader"),
	  N_("Swipe your right thumb across %s") },
	{ "right-index-finger",
	  N_("Place your right index finger on the fingerprint reader"),
	  N_("Place your right index finger on %s"),
	  N_("Swipe your right index finger across the fingerprint reader"),
	  N_("Swipe your right index finger across %s") },
	{ "right-middle-finger",
	  N_("Place your right middle finger on the fingerprint reader"),
	  N_("Place your right middle finger on %s"),
	  N_("Swipe your right middle finger across the fingerprint reader"),
	  N_("Swipe your right middle finger across %s") },
	{ "right-ring-finger",
	  N_("Place your right ring finger on the fingerprint reader"),
	  N_("Place your right ring finger on %s"),
	  N_("Swipe your right ring finger across the fingerprint reader"),
	  N_("Swipe your right ring finger across %s") },
	{ "right-little-finger",
	  N_("Place your right little finger on the fingerprint reader"),
	  N_("Place your right little finger on %s"),
	  N_("Swipe your right little finger across the fingerprint reader"),
	  N_("Swipe your right little finger across %s") },
	{ NULL, NULL, NULL, NULL, NULL }
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

G_GNUC_UNUSED static char *finger_str_to_msg(const char *finger_name, const char *driver_name, gboolean is_swipe)
{
	int i;

	if (finger_name == NULL)
		return NULL;

	for (i = 0; fingers[i].dbus_name != NULL; i++) {
		if (g_str_equal (fingers[i].dbus_name, finger_name)) {
			if (is_swipe == FALSE) {
				if (driver_name)
					return g_strdup_printf (TR (fingers[i].place_str_specific), driver_name);
				else
					return g_strdup (TR (fingers[i].place_str_generic));
			} else {
				if (driver_name)
					return g_strdup_printf (TR (fingers[i].swipe_str_specific), driver_name);
				else
					return g_strdup (TR (fingers[i].swipe_str_generic));
			}
		}
	}

	return NULL;
}

#pragma GCC diagnostic pop

/* Cases not handled:
 * verify-no-match
 * verify-match
 * verify-unknown-error
 */
G_GNUC_UNUSED static const char *verify_result_str_to_msg(const char *result, gboolean is_swipe)
{
	if (result == NULL)
		return NULL;

	if (strcmp (result, "verify-retry-scan") == 0) {
		if (is_swipe == FALSE)
			return TR (N_("Place your finger on the reader again"));
		else
			return TR (N_("Swipe your finger again"));
	}
	if (strcmp (result, "verify-swipe-too-short") == 0)
		return TR (N_("Swipe was too short, try again"));
	if (strcmp (result, "verify-finger-not-centered") == 0)
		return TR (N_("Your finger was not centered, try swiping your finger again"));
	if (strcmp (result, "verify-remove-and-retry") == 0)
		return TR (N_("Remove your finger, and try swiping your finger again"));

	return NULL;
}

/* Cases not handled:
 * enroll-completed
 * enroll-failed
 * enroll-unknown-error
 */
G_GNUC_UNUSED static const char *enroll_result_str_to_msg(const char *result, gboolean is_swipe)
{
	if (result == NULL)
		return NULL;

	if (strcmp (result, "enroll-retry-scan") == 0 || strcmp (result, "enroll-stage-passed") == 0) {
		if (is_swipe == FALSE)
			return TR (N_("Place your finger on the reader again"));
		else
			return TR (N_("Swipe your finger again"));
	}
	if (strcmp (result, "enroll-swipe-too-short") == 0)
		return TR (N_("Swipe was too short, try again"));
	if (strcmp (result, "enroll-finger-not-centered") == 0)
		return TR (N_("Your finger was not centered, try swiping your finger again"));
	if (strcmp (result, "enroll-remove-and-retry") == 0)
		return TR (N_("Remove your finger, and try swiping your finger again"));

	return NULL;
}

