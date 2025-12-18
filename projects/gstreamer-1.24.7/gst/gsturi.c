/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 * Copyright (C) 2011 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2014 David Waring, British Broadcasting Corporation
 *                        <david.waring@rd.bbc.co.uk>
 *
 * gsturi.c: register URI handlers and IETF RFC 3986 URI manipulations.
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

/**
 * SECTION:gsturihandler
 * @title: GstUriHandler
 * @short_description: Interface to ease URI handling in plugins.
 *
 * The #GstURIHandler is an interface that is implemented by Source and Sink
 * #GstElement to unify handling of URI.
 *
 * An application can use the following functions to quickly get an element
 * that handles the given URI for reading or writing
 * (gst_element_make_from_uri()).
 *
 * Source and Sink plugins should implement this interface when possible.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define GST_DISABLE_MINIOBJECT_INLINE_FUNCTIONS
#include "gst_private.h"
#include "gst.h"
#include "gsturi.h"
#include "gstinfo.h"
#include "gstregistry.h"

#include <glib/gi18n-lib.h>

#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

GST_DEBUG_CATEGORY_STATIC (gst_uri_handler_debug);
#define GST_CAT_DEFAULT gst_uri_handler_debug

#ifndef HAVE_STRCASESTR
#define strcasestr _gst_ascii_strcasestr

/* From https://github.com/freebsd/freebsd/blob/master/contrib/file/src/strcasestr.c
 * Updated to use GLib types and GLib string functions
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Find the first occurrence of find in s, ignore case.
 */

static gchar *
_gst_ascii_strcasestr (const gchar * s, const gchar * find)
{
  gchar c, sc;
  gsize len;

  if ((c = *find++) != 0) {
    c = g_ascii_tolower (c);
    len = strlen (find);
    do {
      do {
        if ((sc = *s++) == 0)
          return (NULL);
      } while (g_ascii_tolower (sc) != c);
    } while (g_ascii_strncasecmp (s, find, len) != 0);
    s--;
  }
  return (gchar *) (gintptr) (s);
}
#endif

GType
gst_uri_handler_get_type (void)
{
  static gpointer urihandler_type = 0;

  if (g_once_init_enter_pointer (&urihandler_type)) {
    GType _type;
    static const GTypeInfo urihandler_info = {
      sizeof (GstURIHandlerInterface),
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      0,
      0,
      NULL,
      NULL
    };

    _type = g_type_register_static (G_TYPE_INTERFACE,
        "GstURIHandler", &urihandler_info, 0);

    GST_DEBUG_CATEGORY_INIT (gst_uri_handler_debug, "GST_URI", GST_DEBUG_BOLD,
        "handling of URIs");
    g_once_init_leave_pointer (&urihandler_type, _type);
  }
  return urihandler_type;
}

GQuark
gst_uri_error_quark (void)
{
  return g_quark_from_static_string ("gst-uri-error-quark");
}

#define HEX_ESCAPE '%'

#ifndef GST_REMOVE_DEPRECATED
static const guchar acceptable[96] = {  /* X0   X1   X2   X3   X4   X5   X6   X7   X8   X9   XA   XB   XC   XD   XE   XF */
  0x00, 0x3F, 0x20, 0x20, 0x20, 0x00, 0x2C, 0x3F, 0x3F, 0x3F, 0x3F, 0x22, 0x20, 0x3F, 0x3F, 0x1C,       /* 2X  !"#$%&'()*+,-./   */
  0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x38, 0x20, 0x20, 0x2C, 0x20, 0x2C,       /* 3X 0123456789:;<=>?   */
  0x30, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,       /* 4X @ABCDEFGHIJKLMNO   */
  0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x20, 0x20, 0x20, 0x20, 0x3F,       /* 5X PQRSTUVWXYZ[\]^_   */
  0x20, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,       /* 6X `abcdefghijklmno   */
  0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x20, 0x20, 0x20, 0x3F, 0x20        /* 7X pqrstuvwxyz{|}~DEL */
};

typedef enum
{
  UNSAFE_ALL = 0x1,             /* Escape all unsafe characters   */
  UNSAFE_ALLOW_PLUS = 0x2,      /* Allows '+'  */
  UNSAFE_PATH = 0x4,            /* Allows '/' and '?' and '&' and '='  */
  UNSAFE_DOS_PATH = 0x8,        /* Allows '/' and '?' and '&' and '=' and ':' */
  UNSAFE_HOST = 0x10,           /* Allows '/' and ':' and '@' */
  UNSAFE_SLASHES = 0x20         /* Allows all characters except for '/' and '%' */
} UnsafeCharacterSet;

/*  Escape undesirable characters using %
 *  -------------------------------------
 *
 * This function takes a pointer to a string in which
 * some characters may be unacceptable unescaped.
 * It returns a string which has these characters
 * represented by a '%' character followed by two hex digits.
 *
 * This routine returns a g_malloced string.
 */

static const gchar hex[16] = "0123456789ABCDEF";

static gchar *
escape_string_internal (const gchar * string, UnsafeCharacterSet mask)
{
#define ACCEPTABLE_CHAR(a) ((a)>=32 && (a)<128 && (acceptable[(a)-32] & use_mask))

  const gchar *p;
  gchar *q;
  gchar *result;
  guchar c;
  gint unacceptable;
  UnsafeCharacterSet use_mask;

  g_return_val_if_fail (mask == UNSAFE_ALL
      || mask == UNSAFE_ALLOW_PLUS
      || mask == UNSAFE_PATH
      || mask == UNSAFE_DOS_PATH
      || mask == UNSAFE_HOST || mask == UNSAFE_SLASHES, NULL);

  if (string == NULL) {
    return NULL;
  }

  unacceptable = 0;
  use_mask = mask;
  for (p = string; *p != '\0'; p++) {
    c = *p;
    if (!ACCEPTABLE_CHAR (c)) {
      unacceptable++;
    }
    if ((use_mask == UNSAFE_HOST) && (unacceptable || (c == '/'))) {
      /* when escaping a host, if we hit something that needs to be escaped, or we finally
       * hit a path separator, revert to path mode (the host segment of the url is over).
       */
      use_mask = UNSAFE_PATH;
    }
  }

  result = g_malloc (p - string + unacceptable * 2 + 1);

  use_mask = mask;
  for (q = result, p = string; *p != '\0'; p++) {
    c = *p;

    if (!ACCEPTABLE_CHAR (c)) {
      *q++ = HEX_ESCAPE;        /* means hex coming */
      *q++ = hex[c >> 4];
      *q++ = hex[c & 15];
    } else {
      *q++ = c;
    }
    if ((use_mask == UNSAFE_HOST) && (!ACCEPTABLE_CHAR (c) || (c == '/'))) {
      use_mask = UNSAFE_PATH;
    }
  }

  *q = '\0';

  return result;
}
#endif

static int
hex_to_int (gchar c)
{
  return c >= '0' && c <= '9' ? c - '0'
      : c >= 'A' && c <= 'F' ? c - 'A' + 10
      : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static int
unescape_character (const char *scanner)
{
  int first_digit;
  int second_digit;

  first_digit = hex_to_int (*scanner++);
  if (first_digit < 0) {
    return -1;
  }

  second_digit = hex_to_int (*scanner);
  if (second_digit < 0) {
    return -1;
  }

  return (first_digit << 4) | second_digit;
}

/* unescape_string:
 * @escaped_string: an escaped URI, path, or other string
 * @illegal_characters: a string containing a sequence of characters
 * considered "illegal", '\0' is automatically in this list.
 *
 * Decodes escaped characters (i.e. PERCENTxx sequences) in @escaped_string.
 * Characters are encoded in PERCENTxy form, where xy is the ASCII hex code
 * for character 16x+y.
 *
 * Return value: (nullable): a newly allocated string with the
 * unescaped equivalents, or %NULL if @escaped_string contained one of
 * the characters in @illegal_characters.
 **/
static char *
unescape_string (const gchar * escaped_string, const gchar * illegal_characters)
{
  const gchar *in;
  gchar *out, *result;
  gint character;

  if (escaped_string == NULL) {
    return NULL;
  }

  result = g_malloc (strlen (escaped_string) + 1);

  out = result;
  for (in = escaped_string; *in != '\0'; in++) {
    character = *in;
    if (*in == HEX_ESCAPE) {
      character = unescape_character (in + 1);

      /* Check for an illegal character. We consider '\0' illegal here. */
      if (character <= 0
          || (illegal_characters != NULL
              && strchr (illegal_characters, (char) character) != NULL)) {
        g_free (result);
        return NULL;
      }
      in += 2;
    }
    *out++ = (char) character;
  }

  *out = '\0';
  g_assert ((gsize) (out - result) <= strlen (escaped_string));
  return result;

}


static void
gst_uri_protocol_check_internal (const gchar * uri, gchar ** endptr)
{
  gchar *check = (gchar *) uri;

  g_assert (uri != NULL);
  g_assert (endptr != NULL);

  if (g_ascii_isalpha (*check)) {
    check++;
    while (g_ascii_isalnum (*check) || *check == '+'
        || *check == '-' || *check == '.')
      check++;
  }

  *endptr = check;
}

/**
 * gst_uri_protocol_is_valid:
 * @protocol: A string
 *
 * Tests if the given string is a valid protocol identifier. Protocols
 * must consist of alphanumeric characters, '+', '-' and '.' and must
 * start with a alphabetic character. See RFC 3986 Section 3.1.
 *
 * Returns: %TRUE if the string is a valid protocol identifier, %FALSE otherwise.
 */
gboolean
gst_uri_protocol_is_valid (const gchar * protocol)
{
  gchar *endptr;

  g_return_val_if_fail (protocol != NULL, FALSE);

  gst_uri_protocol_check_internal (protocol, &endptr);

  return *endptr == '\0' && ((gsize) (endptr - protocol)) >= 2;
}

/**
 * gst_uri_is_valid:
 * @uri: A URI string
 *
 * Tests if the given string is a valid URI identifier. URIs start with a valid
 * scheme followed by ":" and maybe a string identifying the location.
 *
 * Returns: %TRUE if the string is a valid URI
 */
gboolean
gst_uri_is_valid (const gchar * uri)
{
  gchar *endptr;

  g_return_val_if_fail (uri != NULL, FALSE);

  gst_uri_protocol_check_internal (uri, &endptr);

  return *endptr == ':' && ((gsize) (endptr - uri)) >= 2;
}

/**
 * gst_uri_get_protocol:
 * @uri: A URI string
 *
 * Extracts the protocol out of a given valid URI. The returned string must be
 * freed using g_free().
 *
 * Returns: (nullable): The protocol for this URI.
 */
gchar *
gst_uri_get_protocol (const gchar * uri)
{
  gchar *colon;

  if (!gst_uri_is_valid (uri))
    return NULL;

  colon = strstr (uri, ":");

  return g_ascii_strdown (uri, colon - uri);
}

/**
 * gst_uri_has_protocol:
 * @uri: a URI string
 * @protocol: a protocol string (e.g. "http")
 *
 * Checks if the protocol of a given valid URI matches @protocol.
 *
 * Returns: %TRUE if the protocol matches.
 */
gboolean
gst_uri_has_protocol (const gchar * uri, const gchar * protocol)
{
  gchar *colon;

  g_return_val_if_fail (protocol != NULL, FALSE);

  if (!gst_uri_is_valid (uri)) {
    return FALSE;
  }

  colon = strstr (uri, ":");

  if (colon == NULL)
    return FALSE;

  return (g_ascii_strncasecmp (uri, protocol, (gsize) (colon - uri)) == 0);
}

/**
 * gst_uri_get_location:
 * @uri: A URI string
 *
 * Extracts the location out of a given valid URI, ie. the protocol and "://"
 * are stripped from the URI, which means that the location returned includes
 * the hostname if one is specified. The returned string must be freed using
 * g_free().
 *
 * Free-function: g_free
 *
 * Returns: (transfer full) (nullable): the location for this URI. Returns
 *     %NULL if the URI isn't valid. If the URI does not contain a location, an
 *     empty string is returned.
 */
gchar *
gst_uri_get_location (const gchar * uri)
{
  const gchar *colon;
  gchar *unescaped = NULL;

  if (!gst_uri_is_valid (uri)) {
    return NULL;
  }

  colon = strstr (uri, "://");
  if (!colon)
    return NULL;

  unescaped = unescape_string (colon + 3, "/");

  /* On Windows an URI might look like file:///c:/foo/bar.txt or
   * file:///c|/foo/bar.txt (some Netscape versions) and we want to
   * return c:/foo/bar.txt as location rather than /c:/foo/bar.txt.
   * Can't use g_filename_from_uri() here because it will only handle the
   * file:// protocol */
#ifdef G_OS_WIN32
  if (unescaped != NULL && unescaped[0] == '/' &&
      g_ascii_isalpha (unescaped[1]) &&
      (unescaped[2] == ':' || unescaped[2] == '|')) {
    unescaped[2] = ':';
    memmove (unescaped, unescaped + 1, strlen (unescaped + 1) + 1);
  }
#endif

  GST_LOG ("extracted location '%s' from URI '%s'", GST_STR_NULL (unescaped),
      uri);
  return unescaped;
}

/**
 * gst_uri_construct:
 * @protocol: Protocol for URI
 * @location: (transfer none): Location for URI
 *
 * Constructs a URI for a given valid protocol and location.
 *
 * Free-function: g_free
 *
 * Returns: (transfer full): a new string for this URI.
 *
 * Deprecated: Use GstURI instead.
 */
#ifndef GST_REMOVE_DEPRECATED
gchar *
gst_uri_construct (const gchar * protocol, const gchar * location)
{
  char *escaped, *proto_lowercase;
  char *retval;

  g_return_val_if_fail (gst_uri_protocol_is_valid (protocol), NULL);
  g_return_val_if_fail (location != NULL, NULL);

  proto_lowercase = g_ascii_strdown (protocol, -1);
  escaped = escape_string_internal (location, UNSAFE_PATH);
  retval = g_strdup_printf ("%s://%s", proto_lowercase, escaped);
  g_free (escaped);
  g_free (proto_lowercase);

  return retval;
}
#endif

typedef struct
{
  GstURIType type;
  const gchar *protocol;
}
SearchEntry;

static gboolean
search_by_entry (GstPluginFeature * feature, gpointer search_entry)
{
  const gchar *const *protocols;
  GstElementFactory *factory;
  SearchEntry *entry = (SearchEntry *) search_entry;

  if (!GST_IS_ELEMENT_FACTORY (feature))
    return FALSE;
  factory = GST_ELEMENT_FACTORY_CAST (feature);

  if (factory->uri_type != entry->type)
    return FALSE;

  protocols = gst_element_factory_get_uri_protocols (factory);

  if (protocols == NULL) {
    g_warning ("Factory '%s' implements GstUriHandler interface but returned "
        "no supported protocols!", gst_plugin_feature_get_name (feature));
    return FALSE;
  }

  while (*protocols != NULL) {
    if (g_ascii_strcasecmp (*protocols, entry->protocol) == 0)
      return TRUE;
    protocols++;
  }
  return FALSE;
}

static gint
sort_by_rank (GstPluginFeature * first, GstPluginFeature * second)
{
  int diff = gst_plugin_feature_get_rank (second) -
      gst_plugin_feature_get_rank (first);
  if (diff == 0)
    diff = g_strcmp0 (gst_plugin_feature_get_name (first),
        gst_plugin_feature_get_name (second));
  return diff;
}

static GList *
get_element_factories_from_uri_protocol (const GstURIType type,
    const gchar * protocol)
{
  GList *possibilities;
  SearchEntry entry;

  g_return_val_if_fail (protocol, NULL);

  entry.type = type;
  entry.protocol = protocol;
  possibilities = gst_registry_feature_filter (gst_registry_get (),
      search_by_entry, FALSE, &entry);

  return possibilities;
}

/**
 * gst_uri_protocol_is_supported:
 * @type: Whether to check for a source or a sink
 * @protocol: Protocol that should be checked for (e.g. "http" or "smb")
 *
 * Checks if an element exists that supports the given URI protocol. Note
 * that a positive return value does not imply that a subsequent call to
 * gst_element_make_from_uri() is guaranteed to work.
 *
 * Returns: %TRUE
*/
gboolean
gst_uri_protocol_is_supported (const GstURIType type, const gchar * protocol)
{
  GList *possibilities;

  g_return_val_if_fail (protocol, FALSE);

  possibilities = get_element_factories_from_uri_protocol (type, protocol);

  if (possibilities) {
    g_list_free (possibilities);
    return TRUE;
  } else
    return FALSE;
}

/**
 * gst_element_make_from_uri:
 * @type: Whether to create a source or a sink
 * @uri: URI to create an element for
 * @elementname: (nullable): Name of created element, can be %NULL.
 * @error: address where to store error information, or %NULL.
 *
 * Creates an element for handling the given URI.
 *
 * Returns: (transfer floating): a new element or %NULL if none
 * could be created
 */
GstElement *
gst_element_make_from_uri (const GstURIType type, const gchar * uri,
    const gchar * elementname, GError ** error)
{
  GList *possibilities, *walk;
  gchar *protocol;
  GstElement *ret = NULL;

  g_return_val_if_fail (gst_is_initialized (), NULL);
  g_return_val_if_fail (GST_URI_TYPE_IS_VALID (type), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (!gst_uri_is_valid (uri)) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        _("Invalid URI: %s"), uri);
    return NULL;
  }

  GST_DEBUG ("type:%d, uri:%s, elementname:%s", type, uri, elementname);

  protocol = gst_uri_get_protocol (uri);
  possibilities = get_element_factories_from_uri_protocol (type, protocol);

  if (!possibilities) {
    GST_DEBUG ("No %s for URI '%s'", type == GST_URI_SINK ? "sink" : "source",
        uri);
    /* The error message isn't great, but we don't expect applications to
     * show that error to users, but call the missing plugins functions */
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_UNSUPPORTED_PROTOCOL,
        _("No URI handler for the %s protocol found"), protocol);
    g_free (protocol);
    return NULL;
  }
  g_free (protocol);

  possibilities = g_list_sort (possibilities, (GCompareFunc) sort_by_rank);
  walk = possibilities;
  while (walk) {
    GstElementFactory *factory = walk->data;
    GError *uri_err = NULL;

    ret = gst_element_factory_create (factory, elementname);
    if (ret != NULL) {
      GstURIHandler *handler = GST_URI_HANDLER (ret);

      if (gst_uri_handler_set_uri (handler, uri, &uri_err))
        break;

      GST_WARNING ("%s didn't accept URI '%s': %s", GST_OBJECT_NAME (ret), uri,
          uri_err->message);

      if (error != NULL && *error == NULL)
        g_propagate_error (error, uri_err);
      else
        g_error_free (uri_err);

      gst_object_unref (ret);
      ret = NULL;
    }
    walk = walk->next;
  }
  gst_plugin_feature_list_free (possibilities);

  GST_LOG_OBJECT (ret, "created %s for URL '%s'",
      type == GST_URI_SINK ? "sink" : "source", uri);

  /* if the first handler didn't work, but we found another one that works */
  if (ret != NULL)
    g_clear_error (error);

  return ret;
}

/**
 * gst_uri_handler_get_uri_type:
 * @handler: A #GstURIHandler.
 *
 * Gets the type of the given URI handler
 *
 * Returns: the #GstURIType of the URI handler.
 * Returns #GST_URI_UNKNOWN if the @handler isn't implemented correctly.
 */
GstURIType
gst_uri_handler_get_uri_type (GstURIHandler * handler)
{
  GstURIHandlerInterface *iface;
  GstURIType ret;

  g_return_val_if_fail (GST_IS_URI_HANDLER (handler), GST_URI_UNKNOWN);

  iface = GST_URI_HANDLER_GET_INTERFACE (handler);
  g_return_val_if_fail (iface != NULL, GST_URI_UNKNOWN);
  g_return_val_if_fail (iface->get_type != NULL, GST_URI_UNKNOWN);

  ret = iface->get_type (G_OBJECT_TYPE (handler));
  g_return_val_if_fail (GST_URI_TYPE_IS_VALID (ret), GST_URI_UNKNOWN);

  return ret;
}

/**
 * gst_uri_handler_get_protocols:
 * @handler: A #GstURIHandler.
 *
 * Gets the list of protocols supported by @handler. This list may not be
 * modified.
 *
 * Returns: (transfer none) (element-type utf8) (nullable) (array zero-terminated=1): the
 *     supported protocols.  Returns %NULL if the @handler isn't
 *     implemented properly, or the @handler doesn't support any
 *     protocols.
 */
const gchar *const *
gst_uri_handler_get_protocols (GstURIHandler * handler)
{
  GstURIHandlerInterface *iface;
  const gchar *const *ret;

  g_return_val_if_fail (GST_IS_URI_HANDLER (handler), NULL);

  iface = GST_URI_HANDLER_GET_INTERFACE (handler);
  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->get_protocols != NULL, NULL);

  ret = iface->get_protocols (G_OBJECT_TYPE (handler));
  g_return_val_if_fail (ret != NULL, NULL);

  return ret;
}

/**
 * gst_uri_handler_get_uri:
 * @handler: A #GstURIHandler
 *
 * Gets the currently handled URI.
 *
 * Returns: (transfer full) (nullable): the URI currently handled by
 *   the @handler.  Returns %NULL if there are no URI currently
 *   handled. The returned string must be freed with g_free() when no
 *   longer needed.
 */
gchar *
gst_uri_handler_get_uri (GstURIHandler * handler)
{
  GstURIHandlerInterface *iface;
  gchar *ret;

  g_return_val_if_fail (GST_IS_URI_HANDLER (handler), NULL);

  iface = GST_URI_HANDLER_GET_INTERFACE (handler);
  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->get_uri != NULL, NULL);
  ret = iface->get_uri (handler);
  if (ret != NULL)
    g_return_val_if_fail (gst_uri_is_valid (ret), NULL);

  return ret;
}

/**
 * gst_uri_handler_set_uri:
 * @handler: A #GstURIHandler
 * @uri: URI to set
 * @error: address where to store a #GError in case of
 *    an error, or %NULL
 *
 * Tries to set the URI of the given handler.
 *
 * Returns: %TRUE if the URI was set successfully, else %FALSE.
 */
gboolean
gst_uri_handler_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstURIHandlerInterface *iface;
  gboolean ret;
  gchar *protocol;

  g_return_val_if_fail (GST_IS_URI_HANDLER (handler), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  iface = GST_URI_HANDLER_GET_INTERFACE (handler);
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->set_uri != NULL, FALSE);

  if (!gst_uri_is_valid (uri)) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        _("Invalid URI: %s"), uri);
    return FALSE;
  }

  protocol = gst_uri_get_protocol (uri);

  if (iface->get_protocols) {
    const gchar *const *protocols;
    const gchar *const *p;
    gboolean found_protocol = FALSE;

    protocols = iface->get_protocols (G_OBJECT_TYPE (handler));
    if (protocols != NULL) {
      for (p = protocols; *p != NULL; ++p) {
        if (g_ascii_strcasecmp (protocol, *p) == 0) {
          found_protocol = TRUE;
          break;
        }
      }

      if (!found_protocol) {
        g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_UNSUPPORTED_PROTOCOL,
            _("URI scheme '%s' not supported"), protocol);
        g_free (protocol);
        return FALSE;
      }
    }
  }

  ret = iface->set_uri (handler, uri, error);

  g_free (protocol);

  return ret;
}

static gchar *
gst_file_utils_canonicalise_path (const gchar * path)
{
  gchar **parts, **p, *clean_path;

#ifdef G_OS_WIN32
  {
    GST_WARNING ("FIXME: canonicalise win32 path");
    return g_strdup (path);
  }
#endif

  parts = g_strsplit (path, "/", -1);

  p = parts;
  while (*p != NULL) {
    if (strcmp (*p, ".") == 0) {
      /* just move all following parts on top of this, incl. NUL terminator */
      g_free (*p);
      memmove (p, p + 1, (g_strv_length (p + 1) + 1) * sizeof (gchar *));
      /* re-check the new current part again in the next iteration */
      continue;
    } else if (strcmp (*p, "..") == 0 && p > parts) {
      /* just move all following parts on top of the previous part, incl.
       * NUL terminator */
      g_free (*(p - 1));
      g_free (*p);
      memmove (p - 1, p + 1, (g_strv_length (p + 1) + 1) * sizeof (gchar *));
      /* re-check the new current part again in the next iteration */
      --p;
      continue;
    }
    ++p;
  }
  if (*path == '/') {
    guint num_parts;

    num_parts = g_strv_length (parts) + 1;      /* incl. terminator */
    parts = g_renew (gchar *, parts, num_parts + 1);
    memmove (parts + 1, parts, num_parts * sizeof (gchar *));
    parts[0] = g_strdup ("/");
  }

  clean_path = g_build_filenamev (parts);
  g_strfreev (parts);
  return clean_path;
}

static gboolean
file_path_contains_relatives (const gchar * path)
{
  return (strstr (path, "/./") != NULL || strstr (path, "/../") != NULL ||
      strstr (path, G_DIR_SEPARATOR_S "." G_DIR_SEPARATOR_S) != NULL ||
      strstr (path, G_DIR_SEPARATOR_S ".." G_DIR_SEPARATOR_S) != NULL);
}

/**
 * gst_filename_to_uri:
 * @filename: (type filename): absolute or relative file name path
 * @error: pointer to error, or %NULL
 *
 * Similar to g_filename_to_uri(), but attempts to handle relative file paths
 * as well. Before converting @filename into an URI, it will be prefixed by
 * the current working directory if it is a relative path, and then the path
 * will be canonicalised so that it doesn't contain any './' or '../' segments.
 *
 * On Windows @filename should be in UTF-8 encoding.
 *
 * Returns: (nullable): newly-allocated URI string, or NULL on error. The caller must
 *   free the URI string with g_free() when no longer needed.
 */
gchar *
gst_filename_to_uri (const gchar * filename, GError ** error)
{
  gchar *abs_location = NULL;
  gchar *uri, *abs_clean;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (g_path_is_absolute (filename)) {
    if (!file_path_contains_relatives (filename)) {
      uri = g_filename_to_uri (filename, NULL, error);
      goto beach;
    }

    abs_location = g_strdup (filename);
  } else {
    gchar *cwd;

    cwd = g_get_current_dir ();
    abs_location = g_build_filename (cwd, filename, NULL);
    g_free (cwd);

    if (!file_path_contains_relatives (abs_location)) {
      uri = g_filename_to_uri (abs_location, NULL, error);
      goto beach;
    }
  }

  /* path is now absolute, but contains '.' or '..' */
  abs_clean = gst_file_utils_canonicalise_path (abs_location);
  GST_LOG ("'%s' -> '%s' -> '%s'", filename, abs_location, abs_clean);
  uri = g_filename_to_uri (abs_clean, NULL, error);
  g_free (abs_clean);

beach:

  g_free (abs_location);
  GST_DEBUG ("'%s' -> '%s'", filename, uri);
  return uri;
}

/****************************************************************************
 * GstUri - GstMiniObject to parse and merge URIs according to IETF RFC 3986
 ****************************************************************************/

/**
 * SECTION:gsturi
 * @title: GstUri
 * @short_description: URI parsing and manipulation.
 *
 * A #GstUri object can be used to parse and split a URI string into its
 * constituent parts. Two #GstUri objects can be joined to make a new #GstUri
 * using the algorithm described in RFC3986.
 */

/* Definition for GstUri object */
struct _GstUri
{
  /*< private > */
  GstMiniObject mini_object;
  gchar *scheme;
  gchar *userinfo;
  gchar *host;
  guint port;
  GList *path;
  GHashTable *query;
  gchar *fragment;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstUri, gst_uri);

static GstUri *_gst_uri_copy (const GstUri * uri);
static void _gst_uri_free (GstUri * uri);
static GstUri *_gst_uri_new (void);
static GList *_remove_dot_segments (GList * path);

/* private GstUri functions */

static GstUri *
_gst_uri_new (void)
{
  GstUri *uri;

  g_return_val_if_fail (gst_is_initialized (), NULL);

  uri = g_new0 (GstUri, 1);

  if (uri)
    gst_mini_object_init (GST_MINI_OBJECT_CAST (uri), 0, gst_uri_get_type (),
        (GstMiniObjectCopyFunction) _gst_uri_copy, NULL,
        (GstMiniObjectFreeFunction) _gst_uri_free);

  return uri;
}

static void
_gst_uri_free (GstUri * uri)
{
  g_return_if_fail (GST_IS_URI (uri));

  g_free (uri->scheme);
  g_free (uri->userinfo);
  g_free (uri->host);
  g_list_free_full (uri->path, g_free);
  if (uri->query)
    g_hash_table_unref (uri->query);
  g_free (uri->fragment);

#ifdef USE_POISONING
  memset (uri, 0xff, sizeof (*uri));
#endif

  g_free (uri);
}

static GHashTable *
_gst_uri_copy_query_table (GHashTable * orig)
{
  GHashTable *new = NULL;

  if (orig != NULL) {
    GHashTableIter iter;
    gpointer key, value;
    new = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_iter_init (&iter, orig);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
      g_hash_table_insert (new, g_strdup (key), g_strdup (value));
    }
  }

  return new;
}

static GstUri *
_gst_uri_copy (const GstUri * orig_uri)
{
  GstUri *new_uri;

  g_return_val_if_fail (GST_IS_URI (orig_uri), NULL);

  new_uri = _gst_uri_new ();

  if (new_uri) {
    new_uri->scheme = g_strdup (orig_uri->scheme);
    new_uri->userinfo = g_strdup (orig_uri->userinfo);
    new_uri->host = g_strdup (orig_uri->host);
    new_uri->port = orig_uri->port;
    new_uri->path = g_list_copy_deep (orig_uri->path, (GCopyFunc) g_strdup,
        NULL);
    new_uri->query = _gst_uri_copy_query_table (orig_uri->query);
    new_uri->fragment = g_strdup (orig_uri->fragment);
  }

  return new_uri;
}

/*
 * _gst_uri_compare_lists:
 *
 * Compare two lists for equality. This compares the two lists, item for item,
 * comparing items in the same position in the two lists. If @first is
 * considered less than @second the result will be negative. If @first is
 * considered to be more than @second then the result will be positive. If the
 * lists are considered to be equal then the result will be 0. If two lists
 * have the same items, but one list is shorter than the other, then the
 * shorter list is considered to be less than the longer list.
 */
static gint
_gst_uri_compare_lists (GList * first, GList * second, GCompareFunc cmp_fn)
{
  GList *itr1, *itr2;
  gint result;

  for (itr1 = first, itr2 = second;
      itr1 != NULL || itr2 != NULL; itr1 = itr1->next, itr2 = itr2->next) {
    if (itr1 == NULL)
      return -1;
    if (itr2 == NULL)
      return 1;
    result = cmp_fn (itr1->data, itr2->data);
    if (result != 0)
      return result;
  }
  return 0;
}

typedef enum
{
  _GST_URI_NORMALIZE_LOWERCASE = 1,
  _GST_URI_NORMALIZE_UPPERCASE = 2
} _GstUriNormalizations;

/*
 * Find the first character that hasn't been normalized according to the @flags.
 */
static gchar *
_gst_uri_first_non_normalized_char (gchar * str, guint flags)
{
  gchar *pos;

  if (str == NULL)
    return NULL;

  for (pos = str; *pos; pos++) {
    if ((flags & _GST_URI_NORMALIZE_UPPERCASE) && g_ascii_islower (*pos))
      return pos;
    if ((flags & _GST_URI_NORMALIZE_LOWERCASE) && g_ascii_isupper (*pos))
      return pos;
  }
  return NULL;
}

static gboolean
_gst_uri_normalize_lowercase (gchar * str)
{
  gchar *pos;
  gboolean ret = FALSE;

  for (pos = _gst_uri_first_non_normalized_char (str,
          _GST_URI_NORMALIZE_LOWERCASE);
      pos != NULL;
      pos = _gst_uri_first_non_normalized_char (pos + 1,
          _GST_URI_NORMALIZE_LOWERCASE)) {
    *pos = g_ascii_tolower (*pos);
    ret = TRUE;
  }

  return ret;
}

#define _gst_uri_normalize_scheme _gst_uri_normalize_lowercase
#define _gst_uri_normalize_hostname _gst_uri_normalize_lowercase

static gboolean
_gst_uri_normalize_path (GList ** path)
{
  GList *new_path;

  new_path = _remove_dot_segments (*path);
  if (_gst_uri_compare_lists (new_path, *path, (GCompareFunc) g_strcmp0) != 0) {
    g_list_free_full (*path, g_free);
    *path = new_path;
    return TRUE;
  }
  g_list_free_full (new_path, g_free);

  return FALSE;
}

static gboolean
_gst_uri_normalize_str_noop (gchar * str)
{
  return FALSE;
}

static gboolean
_gst_uri_normalize_table_noop (GHashTable * table)
{
  return FALSE;
}

#define _gst_uri_normalize_userinfo _gst_uri_normalize_str_noop
#define _gst_uri_normalize_query _gst_uri_normalize_table_noop
#define _gst_uri_normalize_fragment _gst_uri_normalize_str_noop

/* RFC 3986 functions */

static GList *
_merge (GList * base, GList * path)
{
  GList *ret, *path_copy, *last;

  path_copy = g_list_copy_deep (path, (GCopyFunc) g_strdup, NULL);
  /* if base is NULL make path absolute */
  if (base == NULL) {
    if (path_copy != NULL && path_copy->data != NULL) {
      path_copy = g_list_prepend (path_copy, NULL);
    }
    return path_copy;
  }

  ret = g_list_copy_deep (base, (GCopyFunc) g_strdup, NULL);
  last = g_list_last (ret);
  ret = g_list_remove_link (ret, last);
  g_list_free_full (last, g_free);
  ret = g_list_concat (ret, path_copy);

  return ret;
}

static GList *
_remove_dot_segments (GList * path)
{
  GList *out, *elem, *next;

  if (path == NULL)
    return NULL;

  out = g_list_copy_deep (path, (GCopyFunc) g_strdup, NULL);

  for (elem = out; elem; elem = next) {
    next = elem->next;
    if (elem->data == NULL && elem != out && next != NULL) {
      out = g_list_delete_link (out, elem);
    } else if (g_strcmp0 (elem->data, ".") == 0) {
      g_free (elem->data);
      out = g_list_delete_link (out, elem);
    } else if (g_strcmp0 (elem->data, "..") == 0) {
      GList *prev = g_list_previous (elem);
      if (prev && (prev != out || prev->data != NULL)) {
        g_free (prev->data);
        out = g_list_delete_link (out, prev);
      }
      g_free (elem->data);
      if (next != NULL) {
        out = g_list_delete_link (out, elem);
      } else {
        /* path ends in '/..' We need to keep the last '/' */
        elem->data = NULL;
      }
    }
  }

  return out;
}

static gchar *
_gst_uri_escape_userinfo (const gchar * userinfo)
{
  return g_uri_escape_string (userinfo,
      G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO, FALSE);
}

static gchar *
_gst_uri_escape_host (const gchar * host)
{
  return g_uri_escape_string (host,
      G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
}

static gchar *
_gst_uri_escape_host_colon (const gchar * host)
{
  return g_uri_escape_string (host,
      G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS ":", FALSE);
}

static gchar *
_gst_uri_escape_path_segment (const gchar * segment)
{
  return g_uri_escape_string (segment,
      G_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT, FALSE);
}

static gchar *
_gst_uri_escape_http_query_element (const gchar * element)
{
  gchar *ret, *c;

  ret = g_uri_escape_string (element, "!$'()*,;:@/?= ", FALSE);
  for (c = ret; *c; c++)
    if (*c == ' ')
      *c = '+';
  return ret;
}

static gchar *
_gst_uri_escape_fragment (const gchar * fragment)
{
  return g_uri_escape_string (fragment,
      G_URI_RESERVED_CHARS_ALLOWED_IN_PATH "?", FALSE);
}

static GList *
_gst_uri_string_to_list (const gchar * str, const gchar * sep, gboolean convert,
    gboolean unescape)
{
  GList *new_list = NULL;

  if (str) {
    guint pct_sep_len = 0;
    gchar *pct_sep = NULL;
    gchar **split_str;

    if (convert && !unescape) {
      pct_sep = g_strdup_printf ("%%%2.2X", (guint) (*sep));
      pct_sep_len = 3;
    }

    split_str = g_strsplit (str, sep, -1);
    if (split_str) {
      gchar **next_elem;
      for (next_elem = split_str; *next_elem; next_elem += 1) {
        gchar *elem = *next_elem;
        if (*elem == '\0') {
          new_list = g_list_prepend (new_list, NULL);
        } else {
          if (convert && !unescape) {
            gchar *next_sep;
            for (next_sep = strcasestr (elem, pct_sep); next_sep;
                next_sep = strcasestr (next_sep + 1, pct_sep)) {
              *next_sep = *sep;
              memmove (next_sep + 1, next_sep + pct_sep_len,
                  strlen (next_sep + pct_sep_len) + 1);
            }
          }
          if (unescape) {
            *next_elem = g_uri_unescape_string (elem, NULL);
            g_free (elem);
            elem = *next_elem;
          }
          new_list = g_list_prepend (new_list, g_strdup (elem));
        }
      }
    }
    g_strfreev (split_str);
    if (convert && !unescape)
      g_free (pct_sep);
  }

  return g_list_reverse (new_list);
}

static GHashTable *
_gst_uri_string_to_table (const gchar * str, const gchar * part_sep,
    const gchar * kv_sep, gboolean convert, gboolean unescape)
{
  GHashTable *new_table = NULL;

  if (str) {
    gchar *pct_part_sep = NULL, *pct_kv_sep = NULL;
    gchar **split_parts;

    new_table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    if (convert && !unescape) {
      pct_part_sep = g_strdup_printf ("%%%2.2X", (guint) (*part_sep));
      pct_kv_sep = g_strdup_printf ("%%%2.2X", (guint) (*kv_sep));
    }

    split_parts = g_strsplit (str, part_sep, -1);
    if (split_parts) {
      gchar **next_part;
      for (next_part = split_parts; *next_part; next_part += 1) {
        gchar *part = *next_part;
        gchar *kv_sep_pos;
        gchar *key, *value;
        /* if we are converting percent encoded versions of separators then
         *  substitute the part separator now. */
        if (convert && !unescape) {
          gchar *next_sep;
          for (next_sep = strcasestr (part, pct_part_sep); next_sep;
              next_sep = strcasestr (next_sep + 1, pct_part_sep)) {
            *next_sep = *part_sep;
            memmove (next_sep + 1, next_sep + 3, strlen (next_sep + 3) + 1);
          }
        }
        /* find the key/value separator within the part */
        kv_sep_pos = g_strstr_len (part, -1, kv_sep);
        if (kv_sep_pos == NULL) {
          if (unescape) {
            key = g_uri_unescape_string (part, NULL);
          } else {
            key = g_strdup (part);
          }
          value = NULL;
        } else {
          if (unescape) {
            key = g_uri_unescape_segment (part, kv_sep_pos, NULL);
            value = g_uri_unescape_string (kv_sep_pos + 1, NULL);
          } else {
            key = g_strndup (part, kv_sep_pos - part);
            value = g_strdup (kv_sep_pos + 1);
          }
        }
        /* if we are converting percent encoded versions of separators then
         *  substitute the key/value separator in both key and value now. */
        if (convert && !unescape) {
          gchar *next_sep;
          for (next_sep = strcasestr (key, pct_kv_sep); next_sep;
              next_sep = strcasestr (next_sep + 1, pct_kv_sep)) {
            *next_sep = *kv_sep;
            memmove (next_sep + 1, next_sep + 3, strlen (next_sep + 3) + 1);
          }
          if (value) {
            for (next_sep = strcasestr (value, pct_kv_sep); next_sep;
                next_sep = strcasestr (next_sep + 1, pct_kv_sep)) {
              *next_sep = *kv_sep;
              memmove (next_sep + 1, next_sep + 3, strlen (next_sep + 3) + 1);
            }
          }
        }
        /* add value to the table */
        g_hash_table_insert (new_table, key, value);
      }
    }
    /* tidy up */
    g_strfreev (split_parts);
    if (convert && !unescape) {
      g_free (pct_part_sep);
      g_free (pct_kv_sep);
    }
  }

  return new_table;
}


/*
 * Method definitions.
 */

/**
 * gst_uri_new:
 * @scheme: (nullable): The scheme for the new URI.
 * @userinfo: (nullable): The user-info for the new URI.
 * @host: (nullable): The host name for the new URI.
 * @port: The port number for the new URI or %GST_URI_NO_PORT.
 * @path: (nullable): The path for the new URI with '/' separating path
 *                      elements.
 * @query: (nullable): The query string for the new URI with '&' separating
 *                       query elements. Elements containing '&' characters
 *                       should encode them as "&percnt;26".
 * @fragment: (nullable): The fragment name for the new URI.
 *
 * Creates a new #GstUri object with the given URI parts. The path and query
 * strings will be broken down into their elements. All strings should not be
 * escaped except where indicated.
 *
 * Returns: (transfer full): A new #GstUri object.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_new (const gchar * scheme, const gchar * userinfo, const gchar * host,
    guint port, const gchar * path, const gchar * query, const gchar * fragment)
{
  GstUri *new_uri;

  new_uri = _gst_uri_new ();
  if (new_uri) {
    new_uri->scheme = g_strdup (scheme);
    new_uri->userinfo = g_strdup (userinfo);
    new_uri->host = g_strdup (host);
    new_uri->port = port;
    new_uri->path = _gst_uri_string_to_list (path, "/", FALSE, FALSE);
    new_uri->query = _gst_uri_string_to_table (query, "&", "=", TRUE, FALSE);
    new_uri->fragment = g_strdup (fragment);
  }

  return new_uri;
}

/**
 * gst_uri_new_with_base:
 * @base: (transfer none)(nullable): The base URI to join the new URI to.
 * @scheme: (nullable): The scheme for the new URI.
 * @userinfo: (nullable): The user-info for the new URI.
 * @host: (nullable): The host name for the new URI.
 * @port: The port number for the new URI or %GST_URI_NO_PORT.
 * @path: (nullable): The path for the new URI with '/' separating path
 *                      elements.
 * @query: (nullable): The query string for the new URI with '&' separating
 *                       query elements. Elements containing '&' characters
 *                       should encode them as "&percnt;26".
 * @fragment: (nullable): The fragment name for the new URI.
 *
 * Like gst_uri_new(), but joins the new URI onto a base URI.
 *
 * Returns: (transfer full): The new URI joined onto @base.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_new_with_base (GstUri * base, const gchar * scheme,
    const gchar * userinfo, const gchar * host, guint port, const gchar * path,
    const gchar * query, const gchar * fragment)
{
  GstUri *new_rel_uri;
  GstUri *new_uri;

  g_return_val_if_fail (base == NULL || GST_IS_URI (base), NULL);

  new_rel_uri = gst_uri_new (scheme, userinfo, host, port, path, query,
      fragment);
  new_uri = gst_uri_join (base, new_rel_uri);
  gst_uri_unref (new_rel_uri);

  return new_uri;
}

static GstUri *
_gst_uri_from_string_internal (const gchar * uri, gboolean unescape)
{
  const gchar *orig_uri = uri;
  GstUri *uri_obj;

  uri_obj = _gst_uri_new ();

  if (uri_obj && uri != NULL) {
    int i = 0;

    /* be helpful and skip initial white space */
    while (*uri == '\v' || g_ascii_isspace (*uri))
      uri++;

    if (g_ascii_isalpha (uri[i])) {
      /* find end of scheme name */
      i++;
      while (g_ascii_isalnum (uri[i]) || uri[i] == '+' || uri[i] == '-' ||
          uri[i] == '.')
        i++;
    }
    if (i > 0 && uri[i] == ':') {
      /* get scheme */
      uri_obj->scheme = g_strndup (uri, i);
      uri += i + 1;
    }
    if (uri[0] == '/' && uri[1] == '/') {
      const gchar *eoa, *eoui, *eoh, *reoh;
      /* get authority [userinfo@]host[:port] */
      uri += 2;
      /* find end of authority */
      eoa = uri + strcspn (uri, "/?#");

      /* find end of userinfo */
      eoui = strchr (uri, '@');
      if (eoui != NULL && eoui < eoa) {
        if (unescape)
          uri_obj->userinfo = g_uri_unescape_segment (uri, eoui, NULL);
        else
          uri_obj->userinfo = g_strndup (uri, eoui - uri);
        uri = eoui + 1;
      }
      /* find end of host */
      if (uri[0] == '[') {
        eoh = strchr (uri, ']');
        if (eoh == NULL || eoh > eoa) {
          GST_DEBUG ("Unable to parse the host part of the URI '%s'.",
              orig_uri);
          gst_uri_unref (uri_obj);
          return NULL;
        }
        reoh = eoh + 1;
        uri++;
      } else {
        reoh = eoh = strchr (uri, ':');
        if (eoh == NULL || eoh > eoa)
          reoh = eoh = eoa;
      }
      /* don't capture empty host strings */
      if (eoh != uri) {
        /* always unescape hostname */
        uri_obj->host = g_uri_unescape_segment (uri, eoh, NULL);
      }

      uri = reoh;
      if (uri < eoa) {
        /* if port number is malformed then we can't parse this */
        if (uri[0] != ':' || strspn (uri + 1, "0123456789") != eoa - uri - 1) {
          GST_DEBUG ("Unable to parse host/port part of the URI '%s'.",
              orig_uri);
          gst_uri_unref (uri_obj);
          return NULL;
        }
        /* otherwise treat port as unsigned decimal number */
        uri++;
        while (uri < eoa) {
          uri_obj->port = uri_obj->port * 10 + g_ascii_digit_value (*uri);
          uri++;
        }
      }
      uri = eoa;
    }
    if (uri != NULL && uri[0] != '\0') {
      /* get path */
      size_t len;
      len = strcspn (uri, "?#");
      if (uri[len] == '\0') {
        uri_obj->path = _gst_uri_string_to_list (uri, "/", FALSE, TRUE);
        uri = NULL;
      } else {
        if (len > 0) {
          gchar *path_str = g_strndup (uri, len);
          uri_obj->path = _gst_uri_string_to_list (path_str, "/", FALSE, TRUE);
          g_free (path_str);
        }
        uri += len;
      }
    }
    if (uri != NULL && uri[0] == '?') {
      /* get query */
      gchar *eoq;
      eoq = strchr (++uri, '#');
      if (eoq == NULL) {
        uri_obj->query = _gst_uri_string_to_table (uri, "&", "=", TRUE, TRUE);
        uri = NULL;
      } else {
        if (eoq != uri) {
          gchar *query_str = g_strndup (uri, eoq - uri);
          uri_obj->query = _gst_uri_string_to_table (query_str, "&", "=", TRUE,
              TRUE);
          g_free (query_str);
        }
        uri = eoq;
      }
    }
    if (uri != NULL && uri[0] == '#') {
      if (unescape)
        uri_obj->fragment = g_uri_unescape_string (uri + 1, NULL);
      else
        uri_obj->fragment = g_strdup (uri + 1);
    }
  }

  return uri_obj;
}

/**
 * gst_uri_from_string:
 * @uri: The URI string to parse.
 *
 * Parses a URI string into a new #GstUri object. Will return NULL if the URI
 * cannot be parsed.
 *
 * Returns: (transfer full) (nullable): A new #GstUri object, or NULL.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_from_string (const gchar * uri)
{
  return _gst_uri_from_string_internal (uri, TRUE);
}

/**
 * gst_uri_from_string_escaped:
 * @uri: The URI string to parse.
 *
 * Parses a URI string into a new #GstUri object. Will return NULL if the URI
 * cannot be parsed. This is identical to gst_uri_from_string() except that
 * the userinfo and fragment components of the URI will not be unescaped while
 * parsing.
 *
 * Use this when you need to extract a username and password from the userinfo
 * such as https://user:password@example.com since either may contain
 * a URI-escaped ':' character. gst_uri_from_string() will unescape the entire
 * userinfo component, which will make it impossible to know which ':'
 * delineates the username and password.
 *
 * The same applies to the fragment component of the URI, such as
 * https://example.com/path#fragment which may contain a URI-escaped '#'.
 *
 * Returns: (transfer full) (nullable): A new #GstUri object, or NULL.
 *
 * Since: 1.18
 */
GstUri *
gst_uri_from_string_escaped (const gchar * uri)
{
  return _gst_uri_from_string_internal (uri, FALSE);
}

/**
 * gst_uri_from_string_with_base:
 * @base: (transfer none)(nullable): The base URI to join the new URI with.
 * @uri: The URI string to parse.
 *
 * Like gst_uri_from_string() but also joins with a base URI.
 *
 * Returns: (transfer full) (nullable): A new #GstUri object.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_from_string_with_base (GstUri * base, const gchar * uri)
{
  GstUri *new_rel_uri;
  GstUri *new_uri;

  g_return_val_if_fail (base == NULL || GST_IS_URI (base), NULL);

  new_rel_uri = gst_uri_from_string (uri);
  if (!new_rel_uri)
    return NULL;

  new_uri = gst_uri_join (base, new_rel_uri);
  gst_uri_unref (new_rel_uri);

  return new_uri;
}

/**
 * gst_uri_equal:
 * @first: First #GstUri to compare.
 * @second: Second #GstUri to compare.
 *
 * Compares two #GstUri objects to see if they represent the same normalized
 * URI.
 *
 * Returns: %TRUE if the normalized versions of the two URI's would be equal.
 *
 * Since: 1.6
 */
gboolean
gst_uri_equal (const GstUri * first, const GstUri * second)
{
  gchar *first_norm = NULL, *second_norm = NULL;
  GList *first_norm_list = NULL, *second_norm_list = NULL;
  const gchar *first_cmp, *second_cmp;
  GHashTableIter table_iter;
  gpointer key, value;
  int result;

  g_return_val_if_fail ((first == NULL || GST_IS_URI (first)) &&
      (second == NULL || GST_IS_URI (second)), FALSE);

  if (first == second)
    return TRUE;

  if (first == NULL || second == NULL)
    return FALSE;

  if (first->port != second->port)
    return FALSE;

/* work out a version of field value (normalized or not) to compare.
 * first_cmp, second_cmp will be the values to compare later.
 * first_norm, second_norm will be non-NULL if normalized versions are used,
 *  and need to be freed later.
 */
#define GST_URI_NORMALIZED_FIELD(pos, field, norm_fn, flags) \
  pos##_cmp = pos->field; \
  if (_gst_uri_first_non_normalized_char ((gchar*)pos##_cmp, flags) != NULL) { \
    pos##_norm = g_strdup (pos##_cmp); \
    norm_fn (pos##_norm); \
    pos##_cmp = pos##_norm; \
  }

/* compare two string values, normalizing if needed */
#define GST_URI_NORMALIZED_CMP_STR(field, norm_fn, flags) \
  GST_URI_NORMALIZED_FIELD (first, field, norm_fn, flags) \
  GST_URI_NORMALIZED_FIELD (second, field, norm_fn, flags) \
  result = g_strcmp0 (first_cmp, second_cmp); \
  g_free (first_norm); \
  first_norm = NULL; \
  g_free (second_norm); \
  second_norm = NULL; \
  if (result != 0) return FALSE

/* compare two string values */
#define GST_URI_CMP_STR(field) \
  if (g_strcmp0 (first->field, second->field) != 0) return FALSE

/* compare two GLists, normalize lists if needed before comparison */
#define GST_URI_NORMALIZED_CMP_LIST(field, norm_fn, copy_fn, cmp_fn, free_fn) \
  first_norm_list = g_list_copy_deep (first->field, (GCopyFunc) copy_fn, NULL); \
  norm_fn (&first_norm_list); \
  second_norm_list = g_list_copy_deep (second->field, (GCopyFunc) copy_fn, NULL); \
  norm_fn (&second_norm_list); \
  result = _gst_uri_compare_lists (first_norm_list, second_norm_list, (GCompareFunc) cmp_fn); \
  g_list_free_full (first_norm_list, free_fn); \
  g_list_free_full (second_norm_list, free_fn); \
  if (result != 0) return FALSE

  GST_URI_CMP_STR (userinfo);

  GST_URI_CMP_STR (fragment);

  GST_URI_NORMALIZED_CMP_STR (scheme, _gst_uri_normalize_scheme,
      _GST_URI_NORMALIZE_LOWERCASE);

  GST_URI_NORMALIZED_CMP_STR (host, _gst_uri_normalize_hostname,
      _GST_URI_NORMALIZE_LOWERCASE);

  GST_URI_NORMALIZED_CMP_LIST (path, _gst_uri_normalize_path, g_strdup,
      g_strcmp0, g_free);

  if (first->query == NULL && second->query != NULL)
    return FALSE;
  if (first->query != NULL && second->query == NULL)
    return FALSE;
  if (first->query != NULL) {
    if (g_hash_table_size (first->query) != g_hash_table_size (second->query))
      return FALSE;

    g_hash_table_iter_init (&table_iter, first->query);
    while (g_hash_table_iter_next (&table_iter, &key, &value)) {
      if (!g_hash_table_contains (second->query, key))
        return FALSE;
      result = g_strcmp0 (g_hash_table_lookup (second->query, key), value);
      if (result != 0)
        return FALSE;
    }
  }
#undef GST_URI_NORMALIZED_CMP_STR
#undef GST_URI_CMP_STR
#undef GST_URI_NORMALIZED_CMP_LIST
#undef GST_URI_NORMALIZED_FIELD

  return TRUE;
}

/**
 * gst_uri_join:
 * @base_uri: (transfer none) (nullable): The base URI to join another to.
 * @ref_uri: (transfer none) (nullable): The reference URI to join onto the
 *                                       base URI.
 *
 * Join a reference URI onto a base URI using the method from RFC 3986.
 * If either URI is %NULL then the other URI will be returned with the ref count
 * increased.
 *
 * Returns: (transfer full) (nullable): A #GstUri which represents the base
 *                                      with the reference URI joined on.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_join (GstUri * base_uri, GstUri * ref_uri)
{
  const gchar *r_scheme;
  GstUri *t;

  g_return_val_if_fail ((base_uri == NULL || GST_IS_URI (base_uri)) &&
      (ref_uri == NULL || GST_IS_URI (ref_uri)), NULL);

  if (base_uri == NULL && ref_uri == NULL)
    return NULL;
  if (base_uri == NULL) {
    g_return_val_if_fail (GST_IS_URI (ref_uri), NULL);
    return gst_uri_ref (ref_uri);
  }
  if (ref_uri == NULL) {
    g_return_val_if_fail (GST_IS_URI (base_uri), NULL);
    return gst_uri_ref (base_uri);
  }

  g_return_val_if_fail (GST_IS_URI (base_uri) && GST_IS_URI (ref_uri), NULL);

  t = _gst_uri_new ();

  if (t == NULL)
    return t;

  /* process according to RFC3986 */
  r_scheme = ref_uri->scheme;
  if (r_scheme != NULL && g_strcmp0 (base_uri->scheme, r_scheme) == 0) {
    r_scheme = NULL;
  }
  if (r_scheme != NULL) {
    t->scheme = g_strdup (r_scheme);
    t->userinfo = g_strdup (ref_uri->userinfo);
    t->host = g_strdup (ref_uri->host);
    t->port = ref_uri->port;
    t->path = _remove_dot_segments (ref_uri->path);
    t->query = _gst_uri_copy_query_table (ref_uri->query);
  } else {
    if (ref_uri->host != NULL) {
      t->userinfo = g_strdup (ref_uri->userinfo);
      t->host = g_strdup (ref_uri->host);
      t->port = ref_uri->port;
      t->path = _remove_dot_segments (ref_uri->path);
      t->query = _gst_uri_copy_query_table (ref_uri->query);
    } else {
      if (ref_uri->path == NULL) {
        t->path = g_list_copy_deep (base_uri->path, (GCopyFunc) g_strdup, NULL);
        if (ref_uri->query != NULL)
          t->query = _gst_uri_copy_query_table (ref_uri->query);
        else
          t->query = _gst_uri_copy_query_table (base_uri->query);
      } else {
        if (ref_uri->path->data == NULL)
          t->path = _remove_dot_segments (ref_uri->path);
        else {
          GList *mrgd = _merge (base_uri->path, ref_uri->path);
          t->path = _remove_dot_segments (mrgd);
          g_list_free_full (mrgd, g_free);
        }
        t->query = _gst_uri_copy_query_table (ref_uri->query);
      }
      t->userinfo = g_strdup (base_uri->userinfo);
      t->host = g_strdup (base_uri->host);
      t->port = base_uri->port;
    }
    t->scheme = g_strdup (base_uri->scheme);
  }
  t->fragment = g_strdup (ref_uri->fragment);

  return t;
}

/**
 * gst_uri_join_strings:
 * @base_uri: The percent-encoded base URI.
 * @ref_uri: The percent-encoded reference URI to join to the @base_uri.
 *
 * This is a convenience function to join two URI strings and return the result.
 * The returned string should be g_free()'d after use.
 *
 * Returns: (transfer full) (nullable): A string representing the percent-encoded join of
 *          the two URIs.
 *
 * Since: 1.6
 */
gchar *
gst_uri_join_strings (const gchar * base_uri, const gchar * ref_uri)
{
  GstUri *base, *result;
  gchar *result_uri;

  base = gst_uri_from_string (base_uri);
  if (!base)
    return NULL;

  result = gst_uri_from_string_with_base (base, ref_uri);
  if (!result) {
    gst_uri_unref (base);
    return NULL;
  }

  result_uri = gst_uri_to_string (result);
  gst_uri_unref (base);
  gst_uri_unref (result);

  return result_uri;
}

/**
 * gst_uri_is_writable:
 * @uri: The #GstUri object to test.
 *
 * Check if it is safe to write to this #GstUri.
 *
 * Check if the refcount of @uri is exactly 1, meaning that no other
 * reference exists to the #GstUri and that the #GstUri is therefore writable.
 *
 * Modification of a #GstUri should only be done after verifying that it is
 * writable.
 *
 * Returns: %TRUE if it is safe to write to the object.
 *
 * Since: 1.6
 */
gboolean
gst_uri_is_writable (const GstUri * uri)
{
  g_return_val_if_fail (GST_IS_URI (uri), FALSE);
  return gst_mini_object_is_writable (GST_MINI_OBJECT_CAST (uri));
}

/**
 * gst_uri_make_writable:
 * @uri: (transfer full): The #GstUri object to make writable.
 *
 * Make the #GstUri writable.
 *
 * Checks if @uri is writable, and if so the original object is returned. If
 * not, then a writable copy is made and returned. This gives away the
 * reference to @uri and returns a reference to the new #GstUri.
 * If @uri is %NULL then %NULL is returned.
 *
 * Returns: (transfer full): A writable version of @uri.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_make_writable (GstUri * uri)
{
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  return
      GST_URI_CAST (gst_mini_object_make_writable (GST_MINI_OBJECT_CAST (uri)));
}

/**
 * gst_uri_to_string_with_keys:
 * @uri: (nullable): This #GstUri to convert to a string.
 * @keys: (transfer none) (nullable) (element-type utf8): A GList containing
 *   the query argument key strings.
 *
 * Convert the URI to a string, with the query arguments in a specific order.
 * Only the keys in the @keys list will be added to the resulting string.
 *
 * Returns the URI as held in this object as a #gchar* nul-terminated string.
 * The caller should g_free() the string once they are finished with it.
 * The string is put together as described in RFC 3986.
 *
 * Returns: (transfer full): The string version of the URI.
 *
 * Since: 1.24
 */
gchar *
gst_uri_to_string_with_keys (const GstUri * uri, const GList * keys)
{
  GString *uri_str;
  gchar *escaped;

  g_return_val_if_fail (GST_IS_URI (uri), NULL);

  uri_str = g_string_new (NULL);

  if (uri->scheme != NULL)
    g_string_append_printf (uri_str, "%s:", uri->scheme);

  if (uri->userinfo != NULL || uri->host != NULL ||
      uri->port != GST_URI_NO_PORT)
    g_string_append (uri_str, "//");

  if (uri->userinfo != NULL) {
    escaped = _gst_uri_escape_userinfo (uri->userinfo);
    g_string_append_printf (uri_str, "%s@", escaped);
    g_free (escaped);
  }

  if (uri->host != NULL) {
    if (strchr (uri->host, ':') != NULL) {
      escaped = _gst_uri_escape_host_colon (uri->host);
      g_string_append_printf (uri_str, "[%s]", escaped);
      g_free (escaped);
    } else {
      escaped = _gst_uri_escape_host (uri->host);
      g_string_append (uri_str, escaped);
      g_free (escaped);
    }
  }

  if (uri->port != GST_URI_NO_PORT)
    g_string_append_printf (uri_str, ":%u", uri->port);

  if (uri->path != NULL) {
    escaped = gst_uri_get_path_string (uri);
    g_string_append (uri_str, escaped);
    g_free (escaped);
  }

  if (uri->query) {
    if (keys != NULL)
      escaped = gst_uri_get_query_string_ordered (uri, keys);
    else
      escaped = gst_uri_get_query_string (uri);
    if (escaped) {
      g_string_append (uri_str, "?");
      g_string_append (uri_str, escaped);
      g_free (escaped);
    }
  }

  if (uri->fragment != NULL) {
    escaped = _gst_uri_escape_fragment (uri->fragment);
    g_string_append_printf (uri_str, "#%s", escaped);
    g_free (escaped);
  }

  return g_string_free (uri_str, FALSE);
}

/**
 * gst_uri_to_string:
 * @uri: This #GstUri to convert to a string.
 *
 * Convert the URI to a string.
 *
 * Returns the URI as held in this object as a #gchar* nul-terminated string.
 * The caller should g_free() the string once they are finished with it.
 * The string is put together as described in RFC 3986.
 *
 * Returns: (transfer full): The string version of the URI.
 *
 * Since: 1.6
 */
gchar *
gst_uri_to_string (const GstUri * uri)
{
  return gst_uri_to_string_with_keys (uri, NULL);
}

/**
 * gst_uri_is_normalized:
 * @uri: (nullable): The #GstUri to test to see if it is normalized.
 *
 * Tests the @uri to see if it is normalized. A %NULL @uri is considered to be
 * normalized.
 *
 * Returns: TRUE if the URI is normalized or is %NULL.
 *
 * Since: 1.6
 */
gboolean
gst_uri_is_normalized (const GstUri * uri)
{
  GList *new_path;
  gboolean ret;

  if (uri == NULL)
    return TRUE;

  g_return_val_if_fail (GST_IS_URI (uri), FALSE);

  /* check for non-normalized characters in uri parts */
  if (_gst_uri_first_non_normalized_char (uri->scheme,
          _GST_URI_NORMALIZE_LOWERCASE) != NULL ||
      /*_gst_uri_first_non_normalized_char (uri->userinfo,
          _GST_URI_NORMALIZE_PERCENTAGES) != NULL || */
      _gst_uri_first_non_normalized_char (uri->host,
          _GST_URI_NORMALIZE_LOWERCASE /*| _GST_URI_NORMALIZE_PERCENTAGES */ )
      != NULL
      /*|| _gst_uri_first_non_normalized_char (uri->path,
         _GST_URI_NORMALIZE_PERCENTAGES) != NULL
         || _gst_uri_first_non_normalized_char (uri->query,
         _GST_URI_NORMALIZE_PERCENTAGES) != NULL
         || _gst_uri_first_non_normalized_char (uri->fragment,
         _GST_URI_NORMALIZE_PERCENTAGES) != NULL */ )
    return FALSE;

  /* also check path has had dot segments removed */
  new_path = _remove_dot_segments (uri->path);
  ret =
      (_gst_uri_compare_lists (new_path, uri->path,
          (GCompareFunc) g_strcmp0) == 0);
  g_list_free_full (new_path, g_free);
  return ret;
}

/**
 * gst_uri_normalize:
 * @uri: (transfer none): The #GstUri to normalize.
 *
 * Normalization will remove extra path segments ("." and "..") from the URI. It
 * will also convert the scheme and host name to lower case and any
 * percent-encoded values to uppercase.
 *
 * The #GstUri object must be writable. Check with gst_uri_is_writable() or use
 * gst_uri_make_writable() first.
 *
 * Returns: TRUE if the URI was modified.
 *
 * Since: 1.6
 */
gboolean
gst_uri_normalize (GstUri * uri)
{
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  return _gst_uri_normalize_scheme (uri->scheme) |
      _gst_uri_normalize_userinfo (uri->userinfo) |
      _gst_uri_normalize_hostname (uri->host) |
      _gst_uri_normalize_path (&uri->path) |
      _gst_uri_normalize_query (uri->query) |
      _gst_uri_normalize_fragment (uri->fragment);
}

/**
 * gst_uri_get_scheme:
 * @uri: (nullable): This #GstUri object.
 *
 * Get the scheme name from the URI or %NULL if it doesn't exist.
 * If @uri is %NULL then returns %NULL.
 *
 * Returns: (nullable): The scheme from the #GstUri object or %NULL.
 */
const gchar *
gst_uri_get_scheme (const GstUri * uri)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), NULL);
  return (uri ? uri->scheme : NULL);
}

/**
 * gst_uri_set_scheme:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @scheme: The new scheme to set or %NULL to unset the scheme.
 *
 * Set or unset the scheme for the URI.
 *
 * Returns: %TRUE if the scheme was set/unset successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_scheme (GstUri * uri, const gchar * scheme)
{
  if (!uri)
    return scheme == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (uri->scheme == scheme)
    return TRUE;

  g_free (uri->scheme);
  uri->scheme = g_strdup (scheme);

  return TRUE;
}

/**
 * gst_uri_get_userinfo:
 * @uri: (nullable): This #GstUri object.
 *
 * Get the userinfo (usually in the form "username:password") from the URI
 * or %NULL if it doesn't exist. If @uri is %NULL then returns %NULL.
 *
 * Returns: (nullable): The userinfo from the #GstUri object or %NULL.
 *
 * Since: 1.6
 */
const gchar *
gst_uri_get_userinfo (const GstUri * uri)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), NULL);
  return (uri ? uri->userinfo : NULL);
}

/**
 * gst_uri_set_userinfo:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @userinfo: The new user-information string to set or %NULL to unset.
 *
 * Set or unset the user information for the URI.
 *
 * Returns: %TRUE if the user information was set/unset successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_userinfo (GstUri * uri, const gchar * userinfo)
{
  if (!uri)
    return userinfo == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (uri->userinfo == userinfo)
    return TRUE;
  g_free (uri->userinfo);
  uri->userinfo = g_strdup (userinfo);

  return TRUE;
}

/**
 * gst_uri_get_host:
 * @uri: (nullable): This #GstUri object.
 *
 * Get the host name from the URI or %NULL if it doesn't exist.
 * If @uri is %NULL then returns %NULL.
 *
 * Returns: (nullable): The host name from the #GstUri object or %NULL.
 *
 * Since: 1.6
 */
const gchar *
gst_uri_get_host (const GstUri * uri)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), NULL);
  return (uri ? uri->host : NULL);
}

/**
 * gst_uri_set_host:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @host: The new host string to set or %NULL to unset.
 *
 * Set or unset the host for the URI.
 *
 * Returns: %TRUE if the host was set/unset successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_host (GstUri * uri, const gchar * host)
{
  if (!uri)
    return host == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (uri->host == host)
    return TRUE;

  g_free (uri->host);
  uri->host = g_strdup (host);

  return TRUE;
}

/**
 * gst_uri_get_port:
 * @uri: (nullable): This #GstUri object.
 *
 * Get the port number from the URI or %GST_URI_NO_PORT if it doesn't exist.
 * If @uri is %NULL then returns %GST_URI_NO_PORT.
 *
 * Returns: The port number from the #GstUri object or %GST_URI_NO_PORT.
 *
 * Since: 1.6
 */
guint
gst_uri_get_port (const GstUri * uri)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), GST_URI_NO_PORT);
  return (uri ? uri->port : GST_URI_NO_PORT);
}

/**
 * gst_uri_set_port:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @port: The new port number to set or %GST_URI_NO_PORT to unset.
 *
 * Set or unset the port number for the URI.
 *
 * Returns: %TRUE if the port number was set/unset successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_port (GstUri * uri, guint port)
{
  if (!uri)
    return port == GST_URI_NO_PORT;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  uri->port = port;

  return TRUE;
}

/**
 * gst_uri_get_path:
 * @uri: (nullable): The #GstUri to get the path from.
 *
 * Extract the path string from the URI object.
 *
 * Returns: (transfer full) (nullable): The path from the URI. Once finished
 *                                      with the string should be g_free()'d.
 *
 * Since: 1.6
 */
gchar *
gst_uri_get_path (const GstUri * uri)
{
  GList *path_segment;
  const gchar *sep = "";
  GString *ret;

  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->path)
    return NULL;

  ret = g_string_new (NULL);

  for (path_segment = uri->path; path_segment;
      path_segment = path_segment->next) {
    g_string_append (ret, sep);
    if (path_segment->data) {
      g_string_append (ret, path_segment->data);
    }
    sep = "/";
  }

  return g_string_free (ret, FALSE);
}

/**
 * gst_uri_set_path:
 * @uri: (transfer none) (nullable): The #GstUri to modify.
 * @path: (nullable): The new path to set with path segments separated by '/', or use %NULL
 *        to unset the path.
 *
 * Sets or unsets the path in the URI.
 *
 * Returns: %TRUE if the path was set successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_path (GstUri * uri, const gchar * path)
{
  if (!uri)
    return path == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  g_list_free_full (uri->path, g_free);
  uri->path = _gst_uri_string_to_list (path, "/", FALSE, FALSE);

  return TRUE;
}

/**
 * gst_uri_get_path_string:
 * @uri: (nullable): The #GstUri to get the path from.
 *
 * Extract the path string from the URI object as a percent encoded URI path.
 *
 * Returns: (transfer full) (nullable): The path from the URI. Once finished
 *                                      with the string should be g_free()'d.
 *
 * Since: 1.6
 */
gchar *
gst_uri_get_path_string (const GstUri * uri)
{
  GList *path_segment;
  const gchar *sep = "";
  GString *ret;
  gchar *escaped;

  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->path)
    return NULL;

  ret = g_string_new (NULL);

  for (path_segment = uri->path; path_segment;
      path_segment = path_segment->next) {
    g_string_append (ret, sep);
    if (path_segment->data) {
      escaped = _gst_uri_escape_path_segment (path_segment->data);
      g_string_append (ret, escaped);
      g_free (escaped);
    }
    sep = "/";
  }

  return g_string_free (ret, FALSE);
}

/**
 * gst_uri_set_path_string:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @path: The new percent encoded path to set with path segments separated by
 * '/', or use %NULL to unset the path.
 *
 * Sets or unsets the path in the URI.
 *
 * Returns: %TRUE if the path was set successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_path_string (GstUri * uri, const gchar * path)
{
  if (!uri)
    return path == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  g_list_free_full (uri->path, g_free);
  uri->path = _gst_uri_string_to_list (path, "/", FALSE, TRUE);
  return TRUE;
}

/**
 * gst_uri_get_path_segments:
 * @uri: (nullable): The #GstUri to get the path from.
 *
 * Get a list of path segments from the URI.
 *
 * Returns: (transfer full) (element-type gchar*): A #GList of path segment
 *          strings or %NULL if no path segments are available. Free the list
 *          when no longer needed with g_list_free_full(list, g_free).
 *
 * Since: 1.6
 */
GList *
gst_uri_get_path_segments (const GstUri * uri)
{
  GList *ret = NULL;

  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), NULL);

  if (uri) {
    ret = g_list_copy_deep (uri->path, (GCopyFunc) g_strdup, NULL);
  }

  return ret;
}

/**
 * gst_uri_set_path_segments:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @path_segments: (transfer full)(nullable)(element-type gchar*): The new
 *                 path list to set.
 *
 * Replace the path segments list in the URI.
 *
 * Returns: %TRUE if the path segments were set successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_path_segments (GstUri * uri, GList * path_segments)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), FALSE);

  if (!uri) {
    if (path_segments)
      g_list_free_full (path_segments, g_free);
    return path_segments == NULL;
  }

  g_return_val_if_fail (gst_uri_is_writable (uri), FALSE);

  g_list_free_full (uri->path, g_free);
  uri->path = path_segments;
  return TRUE;
}

/**
 * gst_uri_append_path:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @relative_path: (nullable): Relative path to append to the end of the current path.
 *
 * Append a path onto the end of the path in the URI. The path is not
 * normalized, call #gst_uri_normalize() to normalize the path.
 *
 * Returns: %TRUE if the path was appended successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_append_path (GstUri * uri, const gchar * relative_path)
{
  GList *rel_path_list;

  if (!uri)
    return relative_path == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);
  if (!relative_path)
    return TRUE;

  if (uri->path) {
    GList *last_elem = g_list_last (uri->path);
    if (last_elem->data == NULL) {
      uri->path = g_list_delete_link (uri->path, last_elem);
    }
  }
  rel_path_list = _gst_uri_string_to_list (relative_path, "/", FALSE, FALSE);
  /* if path was absolute, make it relative by removing initial NULL element */
  if (rel_path_list && rel_path_list->data == NULL) {
    rel_path_list = g_list_delete_link (rel_path_list, rel_path_list);
  }
  uri->path = g_list_concat (uri->path, rel_path_list);
  return TRUE;
}

/**
 * gst_uri_append_path_segment:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @path_segment: (nullable): The path segment string to append to the URI path.
 *
 * Append a single path segment onto the end of the URI path.
 *
 * Returns: %TRUE if the path was appended successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_append_path_segment (GstUri * uri, const gchar * path_segment)
{
  if (!uri)
    return path_segment == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);
  if (!path_segment)
    return TRUE;

  /* if base path ends in a directory (i.e. last element is NULL), remove it */
  if (uri->path && g_list_last (uri->path)->data == NULL) {
    uri->path = g_list_delete_link (uri->path, g_list_last (uri->path));
  }
  uri->path = g_list_append (uri->path, g_strdup (path_segment));
  return TRUE;
}

/**
 * gst_uri_get_query_string:
 * @uri: (nullable): The #GstUri to get the query string from.
 *
 * Get a percent encoded URI query string from the @uri.
 *
 * Returns: (transfer full) (nullable): A percent encoded query string. Use
 *                                      g_free() when no longer needed.
 *
 * Since: 1.6
 */
gchar *
gst_uri_get_query_string (const GstUri * uri)
{
  GHashTableIter iter;
  gpointer key, value;
  const gchar *sep = "";
  gchar *escaped;
  GString *ret;

  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->query)
    return NULL;

  ret = g_string_new (NULL);
  g_hash_table_iter_init (&iter, uri->query);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    g_string_append (ret, sep);
    escaped = _gst_uri_escape_http_query_element (key);
    g_string_append (ret, escaped);
    g_free (escaped);
    if (value) {
      escaped = _gst_uri_escape_http_query_element (value);
      g_string_append_printf (ret, "=%s", escaped);
      g_free (escaped);
    }
    sep = "&";
  }

  return g_string_free (ret, FALSE);
}

/**
 * gst_uri_set_query_string:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @query: (nullable): The new percent encoded query string to use to populate the query
 *        table, or use %NULL to unset the query table.
 *
 * Sets or unsets the query table in the URI.
 *
 * Returns: %TRUE if the query table was set successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_query_string (GstUri * uri, const gchar * query)
{
  if (!uri)
    return query == NULL;

  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (uri->query)
    g_hash_table_unref (uri->query);
  uri->query = _gst_uri_string_to_table (query, "&", "=", TRUE, TRUE);

  return TRUE;
}

/**
 * gst_uri_get_query_string_ordered:
 * @uri: (nullable): The #GstUri to get the query string from.
 * @keys: (transfer none) (nullable) (element-type utf8): A GList containing the
 *   query argument key strings.
 *
 * Get a percent encoded URI query string from the @uri, with query parameters
 * in the order provided by the @keys list. Only parameter keys in the list will
 * be added to the resulting URI string. This method can be used by retrieving
 * the keys with gst_uri_get_query_keys() and then sorting the list, for
 * example.
 *
 * Returns: (transfer full) (nullable): A percent encoded query string. Use
 * g_free() when no longer needed.
 *
 * Since: 1.24
 */
gchar *
gst_uri_get_query_string_ordered (const GstUri * uri, const GList * keys)
{
  const gchar *sep = "";
  gchar *escaped;
  GString *ret = NULL;
  const GList *key;

  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->query)
    return NULL;

  for (key = keys; key; key = key->next) {
    const gchar *query_key = key->data;
    const gchar *arg;

    /* Key isn't present, skip */
    if (!g_hash_table_contains (uri->query, query_key))
      continue;

    if (ret == NULL)
      ret = g_string_new (NULL);

    /* Append the key */
    g_string_append (ret, sep);
    escaped = _gst_uri_escape_http_query_element (query_key);
    g_string_append (ret, escaped);
    g_free (escaped);

    if ((arg = g_hash_table_lookup (uri->query, query_key))) {
      /* Append the argument */
      escaped = _gst_uri_escape_http_query_element (arg);
      g_string_append_printf (ret, "=%s", escaped);
      g_free (escaped);
    }
    sep = "&";
  }

  /* If no keys were seen, return NULL string instead of empty string */
  return ret ? g_string_free (ret, FALSE) : NULL;
}

/**
 * gst_uri_get_query_table:
 * @uri: (nullable): The #GstUri to get the query table from.
 *
 * Get the query table from the URI. Keys and values in the table are freed
 * with g_free when they are deleted. A value may be %NULL to indicate that
 * the key should appear in the query string in the URI, but does not have a
 * value. Free the returned #GHashTable with #g_hash_table_unref() when it is
 * no longer required. Modifying this hash table will modify the query in the
 * URI.
 *
 * Returns: (transfer full) (element-type gchar* gchar*) (nullable): The query
 *          hash table from the URI.
 *
 * Since: 1.6
 */
GHashTable *
gst_uri_get_query_table (const GstUri * uri)
{
  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->query)
    return NULL;

  return g_hash_table_ref (uri->query);
}

/**
 * gst_uri_set_query_table:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @query_table: (transfer none)(nullable)(element-type gchar* gchar*): The new
 *               query table to use.
 *
 * Set the query table to use in the URI. The old table is unreferenced and a
 * reference to the new one is used instead. A value if %NULL for @query_table
 * will remove the query string from the URI.
 *
 * Returns: %TRUE if the new table was successfully used for the query table.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_query_table (GstUri * uri, GHashTable * query_table)
{
  GHashTable *old_table = NULL;

  if (!uri)
    return query_table == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (uri->query == query_table)
    return TRUE;

  old_table = uri->query;
  if (query_table)
    uri->query = g_hash_table_ref (query_table);
  else
    uri->query = NULL;
  if (old_table)
    g_hash_table_unref (old_table);

  return TRUE;
}

/**
 * gst_uri_set_query_value:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @query_key: (transfer none): The key for the query entry.
 * @query_value: (transfer none)(nullable): The value for the key.
 *
 * This inserts or replaces a key in the query table. A @query_value of %NULL
 * indicates that the key has no associated value, but will still be present in
 * the query string.
 *
 * Returns: %TRUE if the query table was successfully updated.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_query_value (GstUri * uri, const gchar * query_key,
    const gchar * query_value)
{
  if (!uri)
    return FALSE;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (!uri->query) {
    uri->query = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
        g_free);
  }
  g_hash_table_insert (uri->query, g_strdup (query_key),
      g_strdup (query_value));

  return TRUE;
}

/**
 * gst_uri_remove_query_key:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @query_key: The key to remove.
 *
 * Remove an entry from the query table by key.
 *
 * Returns: %TRUE if the key existed in the table and was removed.
 *
 * Since: 1.6
 */
gboolean
gst_uri_remove_query_key (GstUri * uri, const gchar * query_key)
{
  gboolean result;

  if (!uri)
    return FALSE;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);
  if (!uri->query)
    return FALSE;

  result = g_hash_table_remove (uri->query, query_key);
  /* if this was the last query entry, remove the query string completely */
  if (result && g_hash_table_size (uri->query) == 0) {
    g_hash_table_unref (uri->query);
    uri->query = NULL;
  }
  return result;
}

/**
 * gst_uri_query_has_key:
 * @uri: (nullable): The #GstUri to examine.
 * @query_key: The key to lookup.
 *
 * Check if there is a query table entry for the @query_key key.
 *
 * Returns: %TRUE if @query_key exists in the URI query table.
 *
 * Since: 1.6
 */
gboolean
gst_uri_query_has_key (const GstUri * uri, const gchar * query_key)
{
  if (!uri)
    return FALSE;
  g_return_val_if_fail (GST_IS_URI (uri), FALSE);
  if (!uri->query)
    return FALSE;

  return g_hash_table_contains (uri->query, query_key);
}

/**
 * gst_uri_get_query_value:
 * @uri: (nullable): The #GstUri to examine.
 * @query_key: The key to lookup.
 *
 * Get the value associated with the @query_key key. Will return %NULL if the
 * key has no value or if the key does not exist in the URI query table. Because
 * %NULL is returned for both missing keys and keys with no value, you should
 * use gst_uri_query_has_key() to determine if a key is present in the URI
 * query.
 *
 * Returns: (nullable): The value for the given key, or %NULL if not found.
 *
 * Since: 1.6
 */
const gchar *
gst_uri_get_query_value (const GstUri * uri, const gchar * query_key)
{
  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->query)
    return NULL;

  return g_hash_table_lookup (uri->query, query_key);
}

/**
 * gst_uri_get_query_keys:
 * @uri: (nullable): The #GstUri to examine.
 *
 * Get a list of the query keys from the URI.
 *
 * Returns: (transfer container) (element-type gchar*): A list of keys from
 *          the URI query. Free the list with g_list_free().
 *
 * Since: 1.6
 */
GList *
gst_uri_get_query_keys (const GstUri * uri)
{
  if (!uri)
    return NULL;
  g_return_val_if_fail (GST_IS_URI (uri), NULL);
  if (!uri->query)
    return NULL;

  return g_hash_table_get_keys (uri->query);
}

/**
 * gst_uri_get_fragment:
 * @uri: (nullable): This #GstUri object.
 *
 * Get the fragment name from the URI or %NULL if it doesn't exist.
 * If @uri is %NULL then returns %NULL.
 *
 * Returns: (nullable): The host name from the #GstUri object or %NULL.
 *
 * Since: 1.6
 */
const gchar *
gst_uri_get_fragment (const GstUri * uri)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), NULL);
  return (uri ? uri->fragment : NULL);
}

/**
 * gst_uri_set_fragment:
 * @uri: (transfer none)(nullable): The #GstUri to modify.
 * @fragment: (nullable): The fragment string to set.
 *
 * Sets the fragment string in the URI. Use a value of %NULL in @fragment to
 * unset the fragment string.
 *
 * Returns: %TRUE if the fragment was set/unset successfully.
 *
 * Since: 1.6
 */
gboolean
gst_uri_set_fragment (GstUri * uri, const gchar * fragment)
{
  if (!uri)
    return fragment == NULL;
  g_return_val_if_fail (GST_IS_URI (uri) && gst_uri_is_writable (uri), FALSE);

  if (uri->fragment == fragment)
    return TRUE;

  g_free (uri->fragment);
  uri->fragment = g_strdup (fragment);
  return TRUE;
}

/**
 * gst_uri_get_media_fragment_table:
 * @uri: (nullable): The #GstUri to get the fragment table from.
 *
 * Get the media fragment table from the URI, as defined by "Media Fragments URI 1.0".
 * Hash table returned by this API is a list of "key-value" pairs, and the each
 * pair is generated by splitting "URI fragment" per "&" sub-delims, then "key"
 * and "value" are split by "=" sub-delims. The "key" returned by this API may
 * be undefined keyword by standard.
 * A value may be %NULL to indicate that the key should appear in the fragment
 * string in the URI, but does not have a value. Free the returned #GHashTable
 * with #g_hash_table_unref() when it is no longer required.
 * Modifying this hash table does not affect the fragment in the URI.
 *
 * See more about Media Fragments URI 1.0 (W3C) at https://www.w3.org/TR/media-frags/
 *
 * Returns: (transfer full) (element-type gchar* gchar*) (nullable): The
 *          fragment hash table from the URI.
 *
 * Since: 1.12
 */
GHashTable *
gst_uri_get_media_fragment_table (const GstUri * uri)
{
  g_return_val_if_fail (uri == NULL || GST_IS_URI (uri), NULL);

  if (!uri->fragment)
    return NULL;
  return _gst_uri_string_to_table (uri->fragment, "&", "=", TRUE, TRUE);
}

/**
 * gst_uri_copy:
 * @uri: This #GstUri object.
 *
 * Create a new #GstUri object with the same data as this #GstUri object.
 * If @uri is %NULL then returns %NULL.
 *
 * Returns: (transfer full): A new #GstUri object which is a copy of this
 *          #GstUri or %NULL.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_copy (const GstUri * uri)
{
  return GST_URI_CAST (gst_mini_object_copy (GST_MINI_OBJECT_CONST_CAST (uri)));
}

/**
 * gst_uri_ref:
 * @uri: (transfer none): This #GstUri object.
 *
 * Add a reference to this #GstUri object. See gst_mini_object_ref() for further
 * info.
 *
 * Returns: This object with the reference count incremented.
 *
 * Since: 1.6
 */
GstUri *
gst_uri_ref (GstUri * uri)
{
  return GST_URI_CAST (gst_mini_object_ref (GST_MINI_OBJECT_CAST (uri)));
}

/**
 * gst_uri_unref:
 * @uri: (transfer full): This #GstUri object.
 *
 * Decrement the reference count to this #GstUri object.
 *
 * If the reference count drops to 0 then finalize this object.
 *
 * See gst_mini_object_unref() for further info.
 *
 * Since: 1.6
 */
void
gst_uri_unref (GstUri * uri)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (uri));
}

/**
 * gst_clear_uri: (skip)
 * @uri_ptr: a pointer to a #GstUri reference
 *
 * Clears a reference to a #GstUri.
 *
 * @uri_ptr must not be %NULL.
 *
 * If the reference is %NULL then this function does nothing. Otherwise, the
 * reference count of the uri is decreased and the pointer is set to %NULL.
 *
 * Since: 1.18
 */
void
gst_clear_uri (GstUri ** uri_ptr)
{
  gst_clear_mini_object ((GstMiniObject **) uri_ptr);
}
