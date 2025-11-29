/* GStreamer data:// uri source element
 * Copyright (C) 2009 Igalia S.L
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*<private>
 * # Data URLs
 *
 * These function allow encoding and decoding of data: URLs, see
 * [RFC 2397](http://tools.ietf.org/html/rfc2397) for more information.
 */

#include "config.h"

#include "gtkcssdataurlprivate.h"

#include <glib/gi18n-lib.h>

#include <string.h>

/*<private>
 * gtk_css_data_url_parse:
 * @url: the URL to parse
 * @out_mimetype: (out nullable optional): Return location to set the contained
 *   mime type to. If no mime type was specified, this value is set to %NULL.
 * @error: error location
 *
 * Decodes a data URL according to RFC2397 and returns the decoded data.
 *
 * Returns: a new `GBytes` with the decoded data
 */
GBytes *
gtk_css_data_url_parse (const char  *url,
                        char       **out_mimetype,
                        GError     **error)
{
  char *mimetype = NULL;
  const char *parameters_start;
  const char *data_start;
  GBytes *bytes;
  gboolean base64 = FALSE;
  char *charset = NULL;
  gpointer bdata;
  gsize bsize;

  /* url must be an URI as defined in RFC 2397
   * data:[<mediatype>][;base64],<data>
   */
  if (g_ascii_strncasecmp ("data:", url, 5) != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_FILENAME,
                   _("Not a data: URL"));
      return NULL;
    }

  url += 5;

  parameters_start = strchr (url, ';');
  data_start = strchr (url, ',');
  if (data_start == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_FILENAME,
                   _("Malformed data: URL"));
      return NULL;
    }
  if (parameters_start > data_start)
    parameters_start = NULL;

  if (data_start != url && parameters_start != url)
    {
      mimetype = g_strndup (url,
                            (parameters_start ? parameters_start
                                              : data_start) - url);
    }
  else
    {
      mimetype = NULL;
    }

  if (parameters_start != NULL)
    {
      char *parameters_str;
      char **parameters;
      guint i;

      parameters_str = g_strndup (parameters_start + 1, data_start - parameters_start - 1);
      parameters = g_strsplit (parameters_str, ";", -1);

      for (i = 0; parameters[i] != NULL; i++)
        {
          if (g_ascii_strcasecmp ("base64", parameters[i]) == 0)
            {
              base64 = TRUE;
            }
          else if (g_ascii_strncasecmp ("charset=", parameters[i], 8) == 0)
            {
              g_free (charset);
              charset = g_strdup (parameters[i] + 8);
            }
        }
    g_free (parameters_str);
    g_strfreev (parameters);
  }

  /* Skip comma */
  data_start += 1;
  if (base64)
    {
      bdata = g_base64_decode (data_start, &bsize);
    }
  else
    {
      /* URI encoded, i.e. "percent" encoding */
      /* XXX: This doesn't allow nul bytes */
      bdata = g_uri_unescape_string (data_start, NULL);
      if (bdata == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_FILENAME,
                       _("Could not unescape string"));
          g_free (mimetype);
          return NULL;
        }
      bsize = strlen (bdata);
    }

  /* Convert to UTF8 */
  if ((mimetype == NULL || g_ascii_strcasecmp ("text/plain", mimetype) == 0) &&
      charset && g_ascii_strcasecmp ("US-ASCII", charset) != 0
      && g_ascii_strcasecmp ("UTF-8", charset) != 0)
    {
      gsize read;
      gsize written;
      gpointer data;
      GError *local_error = NULL;

      data = g_convert_with_fallback (bdata, bsize,
                                      "UTF-8", charset, 
                                      (char *) "*",
                                      &read, &written, &local_error);
      g_free (bdata);

      if (local_error)
        {
          g_propagate_error (error, local_error);
          g_free (charset);
          g_free (data);
          g_free (mimetype);
          return NULL;
        }


      bdata = data;
      bsize = written;
    }
  bytes = g_bytes_new_take (bdata, bsize);

  g_free (charset);
  if (out_mimetype)
    *out_mimetype = mimetype;
  else
    g_free (mimetype);

  return bytes;
}
