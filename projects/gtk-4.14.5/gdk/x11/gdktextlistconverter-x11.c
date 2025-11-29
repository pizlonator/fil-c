/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2017 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdktextlistconverter-x11.h"

#include <glib/gi18n-lib.h>
#include "gdkprivate-x11.h"

#define GDK_X11_TEXT_LIST_CONVERTER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDK_TYPE_X11_TEXT_LIST_CONVERTER, GdkX11TextListConverterClass))
#define GDK_IS_X11_TEXT_LIST_CONVERTER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDK_TYPE_X11_TEXT_LIST_CONVERTER))
#define GDK_X11_TEXT_LIST_CONVERTER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_TEXT_LIST_CONVERTER, GdkX11TextListConverterClass))

typedef struct _GdkX11TextListConverterClass GdkX11TextListConverterClass;

struct _GdkX11TextListConverter
{
  GObject parent_instance;

  GdkDisplay *display;
  
  const char *encoding; /* interned */
  int format;

  guint encoder : 1;
};

struct _GdkX11TextListConverterClass
{
  GObjectClass parent_class;
};

static GConverterResult
write_output (void        *outbuf,
              gsize        outbuf_size,
              gsize       *bytes_written,
              const void  *data,
              gssize       len,
              GError     **error)
{
  if (len < 0)
    len = strlen (data) + 1;

  if (outbuf_size < len)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                           _("Not enough space in destination"));
      return G_CONVERTER_ERROR;
    }

  memcpy (outbuf, data, len);
  *bytes_written = len;
  return G_CONVERTER_FINISHED;
}

static GConverterResult
gdk_x11_text_list_converter_decode (GdkX11TextListConverter *conv,
                                    const void              *inbuf,
                                    gsize                    inbuf_size,
                                    void                    *outbuf,
                                    gsize                    outbuf_size,
                                    GConverterFlags          flags,
                                    gsize                   *bytes_read,
                                    gsize                   *bytes_written,
                                    GError                 **error)
{
  int count;
  char **list;

  if (!(flags & G_CONVERTER_INPUT_AT_END))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
                           _("Need complete input to do conversion"));
      return G_CONVERTER_ERROR;
    }

  count = _gdk_x11_display_text_property_to_utf8_list (conv->display,
                                                       conv->encoding,
                                                       conv->format,
                                                       inbuf,
                                                       inbuf_size,
                                                       &list);
  if (count < 0)
    {
      /* XXX: add error handling from gdkselection-x11.c */
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                           _("Not enough space in destination"));
      return G_CONVERTER_ERROR;
    }
  else if (count == 0)
    {
      *bytes_read = inbuf_size;
      return write_output (outbuf, outbuf_size, bytes_written, "", 1, error);
    }
  else
    {
      GConverterResult result;
      
      result = write_output (outbuf, outbuf_size, bytes_written, list[0], -1, error);
      g_strfreev (list);
      *bytes_read = inbuf_size;
      return result;
    }
}

/* The specifications for COMPOUND_TEXT and STRING specify that C0 and
 * C1 are not allowed except for \n and \t, however the X conversions
 * routines for COMPOUND_TEXT only enforce this in one direction,
 * causing cut-and-paste of \r and \r\n separated text to fail.
 * This routine strips out all non-allowed C0 and C1 characters
 * from the input string and also canonicalizes \r, and \r\n to \n
 */
char *
gdk_x11_utf8_to_string_target (const char *utf8_str,
                               gboolean    return_latin1)
{
  int len = strlen (utf8_str);
  GString *result = g_string_sized_new (len);
  const char *p = utf8_str;

  while (*p)
    {
      if (*p == '\r')
        {
          p++;
          if (*p == '\n')
            p++;

          g_string_append_c (result, '\n');
        }
      else
        {
          gunichar ch = g_utf8_get_char (p);

          if (!((ch < 0x20 && ch != '\t' && ch != '\n') || (ch >= 0x7f && ch < 0xa0)))
            {
              if (return_latin1)
                {
                  if (ch <= 0xff)
                    g_string_append_c (result, ch);
                  else
                    g_string_append_printf (result,
                                            ch < 0x10000 ? "\\u%04x" : "\\U%08x",
                                            ch);
                }
              else
                {
                  char buf[7];
                  int buflen;

                  buflen = g_unichar_to_utf8 (ch, buf);
                  g_string_append_len (result, buf, buflen);
                }
            }

          p = g_utf8_next_char (p);
        }
    }

  return g_string_free (result, FALSE);
}

static GConverterResult
gdk_x11_text_list_converter_encode (GdkX11TextListConverter *conv,
                                    const void              *inbuf,
                                    gsize                    inbuf_size,
                                    void                    *outbuf,
                                    gsize                    outbuf_size,
                                    GConverterFlags          flags,
                                    gsize                   *bytes_read,
                                    gsize                   *bytes_written,
                                    GError                 **error)
{
  if (!(flags & G_CONVERTER_INPUT_AT_END))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
                           _("Need complete input to do conversion"));
      return G_CONVERTER_ERROR;
    }

  if (g_str_equal (conv->encoding, "STRING") ||
      g_str_equal (conv->encoding, "TEXT"))
    {
      GConverterResult result;
      char *tmp, *latin1;

      tmp = g_strndup (inbuf, inbuf_size);
      latin1 = gdk_x11_utf8_to_string_target (tmp, TRUE);
      g_free (tmp);
      if (latin1)
        {
          result = write_output (outbuf, outbuf_size, bytes_written, latin1, -1, error);
          g_free (latin1);
        }
      else
        { 
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                               _("Invalid byte sequence in conversion input"));
          result = G_CONVERTER_ERROR;
        }
      return result;
    }
  else if (g_str_equal (conv->encoding, "COMPOUND_TEXT"))
    {
      GConverterResult result;
      guchar *text;
      const char *encoding;
      int format;
      int new_length;
      char *tmp;

      tmp = g_strndup (inbuf, inbuf_size);
      if (gdk_x11_display_utf8_to_compound_text (conv->display, tmp,
                                                 &encoding, &format, &text, &new_length))
        {
          if (g_str_equal (encoding, conv->encoding) &&
              format == conv->format)
            {
              result = write_output (outbuf, outbuf_size, bytes_written, text, new_length, error);
            }
          else
            {
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                                   _("Invalid formats in compound text conversion."));
              result = G_CONVERTER_ERROR;
            }
          gdk_x11_free_compound_text (text);
        }
      else
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                               _("Invalid byte sequence in conversion input"));
          result = G_CONVERTER_ERROR;
        }
      g_free (tmp);
      return result;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("Unsupported encoding “%s”"), conv->encoding);
      return G_CONVERTER_ERROR;
    }

  return FALSE;
}

static GConverterResult
gdk_x11_text_list_converter_convert (GConverter       *converter,
                                     const void       *inbuf,
                                     gsize             inbuf_size,
                                     void             *outbuf,
                                     gsize             outbuf_size,
                                     GConverterFlags   flags,
                                     gsize            *bytes_read,
                                     gsize            *bytes_written,
                                     GError          **error)
{
  GdkX11TextListConverter *conv = GDK_X11_TEXT_LIST_CONVERTER (converter);

  if (conv->encoder)
    {
      return gdk_x11_text_list_converter_encode (conv,
                                                 inbuf, inbuf_size,
                                                 outbuf, outbuf_size,
                                                 flags,
                                                 bytes_read, bytes_written,
                                                 error);
    }
  else
    {
      return gdk_x11_text_list_converter_decode (conv,
                                                 inbuf, inbuf_size,
                                                 outbuf, outbuf_size,
                                                 flags,
                                                 bytes_read, bytes_written,
                                                 error);
    }
}

static void
gdk_x11_text_list_converter_reset (GConverter *converter)
{
}

static void
gdk_x11_text_list_converter_iface_init (GConverterIface *iface)
{
  iface->convert = gdk_x11_text_list_converter_convert;
  iface->reset = gdk_x11_text_list_converter_reset;
}

G_DEFINE_TYPE_WITH_CODE (GdkX11TextListConverter, gdk_x11_text_list_converter, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
						gdk_x11_text_list_converter_iface_init))

static void
gdk_x11_text_list_converter_finalize (GObject *object)
{
  GdkX11TextListConverter *conv = GDK_X11_TEXT_LIST_CONVERTER (object);

  g_object_unref (conv->display);

  G_OBJECT_CLASS (gdk_x11_text_list_converter_parent_class)->finalize (object);
}

static void
gdk_x11_text_list_converter_class_init (GdkX11TextListConverterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_x11_text_list_converter_finalize;
}

static void
gdk_x11_text_list_converter_init (GdkX11TextListConverter *local)
{
}

GConverter *
gdk_x11_text_list_converter_to_utf8_new (GdkDisplay *display,
                                         const char *encoding,
                                         int         format)
{
  GdkX11TextListConverter *conv;

  conv = g_object_new (GDK_TYPE_X11_TEXT_LIST_CONVERTER, NULL);

  conv->display = g_object_ref (display);
  conv->encoding = g_intern_string (encoding);
  conv->format = format;

  return G_CONVERTER (conv);
}

GConverter *
gdk_x11_text_list_converter_from_utf8_new (GdkDisplay *display,
                                           const char *encoding,
                                           int         format)
{
  GdkX11TextListConverter *conv;

  conv = g_object_new (GDK_TYPE_X11_TEXT_LIST_CONVERTER, NULL);

  conv->display = g_object_ref (display);
  conv->encoding = g_intern_string (encoding);
  conv->format = format;
  conv->encoder = TRUE;

  return G_CONVERTER (conv);
}

