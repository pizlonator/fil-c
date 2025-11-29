/*
 * Copyright © 2019 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "../../gtk/css/gtkcssdataurlprivate.h"

#include <locale.h>

typedef struct _Test Test;

struct _Test
{
  const char *name;
  const char *url;
  const char *mimetype;
  const char *contents;
  gsize contents_len;
  const char *charset;
};

#define CONTENTS(data) (data), sizeof(data) - 1
Test tests[] = {
  { "simple", "data:,HelloWorld", NULL, CONTENTS("HelloWorld"), NULL },
  { "nodata", "data:,", NULL, CONTENTS(""), NULL },
  { "case_sensitive", "dATa:,HelloWorld", NULL, CONTENTS("HelloWorld"), NULL },
  { "semicolon_after_comma", "data:,;base64", NULL, CONTENTS(";base64"), NULL },
  { "mimetype", "data:image/png,nopng", "image/png", CONTENTS("nopng"), NULL },
  { "charset", "data:text/plain;charset=ISO-8859-1,Timm B\344der", "text/plain", CONTENTS("Timm Bäder"), "ISO-8859-1" },
  { "charset_escaped", "data:text/plain;charset=ISO-8859-1,Timm%20B%E4der", "text/plain", CONTENTS("Timm Bäder"), "ISO-8859-1" },
  { "charset_base64", "data:text/plain;charset=ISO-8859-5;base64,wOPh29DdILjW0ePb0OLe0g==", "text/plain", CONTENTS("Руслан Ижбулатов"), "ISO-8859-5" },
  { "wrong_scheme", "duda:,Hello", NULL, NULL, 0, NULL },
  { "missing_comma", "data:text/plain;charset=ISO-8859-1:bla", NULL, NULL, 0, "ISO-8859-1" },
  { "bad_escape", "data:,abc%00", NULL, NULL, -1, NULL },
};

static void
test_parse (gconstpointer data)
{
  const Test *test = data;
  GError *error = NULL;
  char *mimetype = NULL;
  GBytes *bytes;

  if (test->charset)
    {
      GIConv iconv;

      iconv = g_iconv_open ("UTF-8", test->charset);
      if (iconv == (GIConv) -1)
        {
          g_test_skip_printf ("Conversion from %s to UTF-8 not supported", test->charset);
          return;
        }

      g_iconv_close (iconv);
    }

  bytes = gtk_css_data_url_parse (test->url, &mimetype, &error);

  if (test->contents)
    {
      g_assert_no_error (error);
      g_assert_nonnull (bytes);
      if (test->mimetype == NULL)
        g_assert_null (mimetype);
      else
        g_assert_cmpstr (mimetype, ==, test->mimetype);

      g_assert_cmpmem (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes),
                       test->contents, test->contents_len);
      g_bytes_unref (bytes);
    }
  else
    {
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME);
      g_assert_null (bytes);
      g_error_free (error);
    }
}

int
main (int argc, char *argv[])
{
  guint i;

  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      char *name = g_strdup_printf ("/css/data/load/%s", tests[i].name);
      g_test_add_data_func (name, &tests[i], test_parse);
      g_free (name);
    }

  return g_test_run ();
}

