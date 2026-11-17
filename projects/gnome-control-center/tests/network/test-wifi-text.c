/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* utils.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#undef NDEBUG
#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS
#undef G_LOG_DOMAIN

#include <glib.h>

/* Including ‘.c’ file to test static functions */
#include "cc-wifi-panel.c"
#include "cc-qr-code.c"

static void
test_escape_qr_string (void)
{
  char *str;

  str = escape_string (NULL, TRUE);
  g_assert_null (str);

  str = escape_string ("Wifi's password:empty", TRUE);
  g_assert_cmpstr (str, ==, "\"Wifi\'s password\\:empty\"");
  g_free (str);

  str = escape_string ("random;string:;\\", TRUE);
  g_assert_cmpstr (str, ==, "\"random\\;string\\:\\;\\\\\"");
  g_free (str);

  str = escape_string ("random-string", TRUE);
  g_assert_cmpstr (str, ==, "\"random-string\"");
  g_free (str);

  str = escape_string ("random-string", FALSE);
  g_assert_cmpstr (str, ==, "random-string");
  g_free (str);

  str = escape_string ("വൈഫൈ", TRUE);
  g_assert_cmpstr (str, ==, "\"വൈഫൈ\"");
  g_free (str);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/wifi/escape-qr-string", test_escape_qr_string);

  return g_test_run ();
}
