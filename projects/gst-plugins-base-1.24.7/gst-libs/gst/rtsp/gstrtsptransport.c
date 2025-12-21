/* GStreamer
 * Copyright (C) <2005,2006,2007> Wim Taymans <wim@fluendo.com>
 *               <2007> Peter Kjellerstedt  <pkj at axis com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * SECTION:gstrtsptransport
 * @title: GstRTSPRange
 * @short_description: dealing with RTSP transports
 *
 * Provides helper functions to deal with RTSP transport strings.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "gstrtsptransport.h"
#include "gstrtsp-enumtypes.h"

#define MAX_MANAGERS	2

typedef enum
{
  RTSP_TRANSPORT_DELIVERY = 1 << 0,     /* multicast | unicast */
  RTSP_TRANSPORT_DESTINATION = 1 << 1,
  RTSP_TRANSPORT_SOURCE = 1 << 2,
  RTSP_TRANSPORT_INTERLEAVED = 1 << 3,
  RTSP_TRANSPORT_APPEND = 1 << 4,
  RTSP_TRANSPORT_TTL = 1 << 5,
  RTSP_TRANSPORT_LAYERS = 1 << 6,
  RTSP_TRANSPORT_PORT = 1 << 7,
  RTSP_TRANSPORT_CLIENT_PORT = 1 << 8,
  RTSP_TRANSPORT_SERVER_PORT = 1 << 9,
  RTSP_TRANSPORT_SSRC = 1 << 10,
  RTSP_TRANSPORT_MODE = 1 << 11,
} RTSPTransportParameter;

typedef struct
{
  const gchar *name;
  const GstRTSPTransMode mode;
  const GstRTSPProfile profile;
  const GstRTSPLowerTrans ltrans;
  const gchar *media_type;
  const gchar *manager[MAX_MANAGERS];
} GstRTSPTransMap;

static const GstRTSPTransMap transports[] = {
  {"rtp", GST_RTSP_TRANS_RTP, GST_RTSP_PROFILE_AVP,
        GST_RTSP_LOWER_TRANS_UDP_MCAST, "application/x-rtp",
      {"rtpbin", "rtpdec"}},
  {"srtp", GST_RTSP_TRANS_RTP, GST_RTSP_PROFILE_SAVP,
        GST_RTSP_LOWER_TRANS_UDP_MCAST, "application/x-srtp",
      {"rtpbin", "rtpdec"}},
  {"rtpf", GST_RTSP_TRANS_RTP, GST_RTSP_PROFILE_AVPF,
        GST_RTSP_LOWER_TRANS_UDP_MCAST, "application/x-rtp",
      {"rtpbin", "rtpdec"}},
  {"srtpf", GST_RTSP_TRANS_RTP, GST_RTSP_PROFILE_SAVPF,
        GST_RTSP_LOWER_TRANS_UDP_MCAST, "application/x-srtp",
      {"rtpbin", "rtpdec"}},
  /* FIXME: these two are no longer supported (the rdtmanager element and
   * related code has been removed from the realmedia plugin), but let's keep
   * this around for now so it can still be added back via plugins if needed.
   * FIXME 2.0: remove */
  {"x-real-rdt", GST_RTSP_TRANS_RDT, GST_RTSP_PROFILE_AVP,
        GST_RTSP_LOWER_TRANS_UNKNOWN, "application/x-rdt",
      {"rdtmanager", NULL}},
  {"x-pn-tng", GST_RTSP_TRANS_RDT, GST_RTSP_PROFILE_AVP,
        GST_RTSP_LOWER_TRANS_UNKNOWN, "application/x-rdt",
      {"rdtmanager", NULL}},
  {NULL, GST_RTSP_TRANS_UNKNOWN, GST_RTSP_PROFILE_UNKNOWN,
      GST_RTSP_LOWER_TRANS_UNKNOWN, NULL, {NULL, NULL}}
};

typedef struct
{
  const gchar *name;
  const GstRTSPProfile profile;
} RTSPProfileMap;

static const RTSPProfileMap profiles[] = {
  {"avp", GST_RTSP_PROFILE_AVP},
  {"savp", GST_RTSP_PROFILE_SAVP},
  {"avpf", GST_RTSP_PROFILE_AVPF},
  {"savpf", GST_RTSP_PROFILE_SAVPF},
  {NULL, GST_RTSP_PROFILE_UNKNOWN}
};

typedef struct
{
  const gchar *name;
  const GstRTSPLowerTrans ltrans;
} RTSPLTransMap;

static const RTSPLTransMap ltrans[] = {
  {"udp", GST_RTSP_LOWER_TRANS_UDP},
  {"mcast", GST_RTSP_LOWER_TRANS_UDP_MCAST},
  {"tcp", GST_RTSP_LOWER_TRANS_TCP},
  {NULL, GST_RTSP_LOWER_TRANS_UNKNOWN}
};

#define RTSP_TRANSPORT_PARAMETER_IS_UNIQUE(param) \
G_STMT_START {                                    \
  if ((transport_params & (param)) != 0)          \
    goto invalid_transport;                       \
  transport_params |= (param);                    \
} G_STMT_END

/**
 * gst_rtsp_transport_new:
 * @transport: (out) (transfer full): location to hold the new #GstRTSPTransport
 *
 * Allocate a new initialized #GstRTSPTransport. Use gst_rtsp_transport_free()
 * after usage.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_transport_new (GstRTSPTransport ** transport)
{
  GstRTSPTransport *trans;

  g_return_val_if_fail (transport != NULL, GST_RTSP_EINVAL);

  trans = g_new0 (GstRTSPTransport, 1);

  *transport = trans;

  return gst_rtsp_transport_init (trans);
}

/**
 * gst_rtsp_transport_init:
 * @transport: (out caller-allocates): a #GstRTSPTransport
 *
 * Initialize @transport so that it can be used.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_transport_init (GstRTSPTransport * transport)
{
  g_return_val_if_fail (transport != NULL, GST_RTSP_EINVAL);

  g_free (transport->destination);
  g_free (transport->source);

  memset (transport, 0, sizeof (GstRTSPTransport));

  transport->trans = GST_RTSP_TRANS_RTP;
  transport->profile = GST_RTSP_PROFILE_AVP;
  transport->lower_transport = GST_RTSP_LOWER_TRANS_UDP_MCAST;
  transport->mode_play = TRUE;
  transport->mode_record = FALSE;
  transport->interleaved.min = -1;
  transport->interleaved.max = -1;
  transport->port.min = -1;
  transport->port.max = -1;
  transport->client_port.min = -1;
  transport->client_port.max = -1;
  transport->server_port.min = -1;
  transport->server_port.max = -1;

  return GST_RTSP_OK;
}

#ifndef GST_REMOVE_DEPRECATED
/**
 * gst_rtsp_transport_get_mime:
 * @trans: a #GstRTSPTransMode
 * @mime: (out) (transfer none): location to hold the result
 *
 * Get the mime type of the transport mode @trans. This mime type is typically
 * used to generate #GstCaps events.
 *
 * Deprecated: This functions only deals with the GstRTSPTransMode and only
 *    returns the mime type for #GST_RTSP_PROFILE_AVP. Use
 *    gst_rtsp_transport_get_media_type() instead.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_transport_get_mime (GstRTSPTransMode trans, const gchar ** mime)
{
  gint i;

  g_return_val_if_fail (mime != NULL, GST_RTSP_EINVAL);

  for (i = 0; transports[i].name; i++)
    if (transports[i].mode == trans
        && transports[i].profile == GST_RTSP_PROFILE_AVP)
      break;
  *mime = transports[i].media_type;

  return GST_RTSP_OK;
}
#endif

/**
 * gst_rtsp_transport_get_media_type:
 * @transport: a #GstRTSPTransport
 * @media_type: (out) (transfer none): media type of @transport
 *
 * Get the media type of @transport. This media type is typically
 * used to generate #GstCaps events.
 *
 * Since: 1.4
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_transport_get_media_type (GstRTSPTransport * transport,
    const gchar ** media_type)
{
  gint i;

  g_return_val_if_fail (transport != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (media_type != NULL, GST_RTSP_EINVAL);

  for (i = 0; transports[i].name; i++)
    if (transports[i].mode == transport->trans
        && transports[i].profile == transport->profile)
      break;
  *media_type = transports[i].media_type;

  return GST_RTSP_OK;
}

static GstRTSPLowerTrans
get_default_lower_trans (GstRTSPTransport * transport)
{
  gint i;

  for (i = 0; transports[i].name; i++)
    if (transports[i].mode == transport->trans
        && transports[i].profile == transport->profile)
      break;

  return transports[i].ltrans;
}

/**
 * gst_rtsp_transport_get_manager:
 * @trans: a #GstRTSPTransMode
 * @manager: (out) (nullable) (transfer none): location to hold the result
 * @option: option index.
 *
 * Get the #GstElement that can handle the buffers transported over @trans.
 *
 * It is possible that there are several managers available, use @option to
 * selected one.
 *
 * @manager will contain an element name or %NULL when no manager is
 * needed/available for @trans.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_transport_get_manager (GstRTSPTransMode trans, const gchar ** manager,
    guint option)
{
  gint i;

  g_return_val_if_fail (manager != NULL, GST_RTSP_EINVAL);

  for (i = 0; transports[i].name; i++)
    if (transports[i].mode == trans)
      break;

  if (option < MAX_MANAGERS)
    *manager = transports[i].manager[option];
  else
    *manager = NULL;

  return GST_RTSP_OK;
}

static void
parse_mode (GstRTSPTransport * transport, const gchar * str)
{
  transport->mode_play = (strstr (str, "play") != NULL);
  transport->mode_record = (strstr (str, "record") != NULL);
}

static gboolean
check_range (const gchar * str, gchar ** tmp, gint * range)
{
  glong range_val;

  range_val = strtol (str, tmp, 10);
  if (range_val >= G_MININT && range_val <= G_MAXINT) {
    *range = range_val;
    return TRUE;
  } else {
    return FALSE;
  }
}

static gboolean
parse_range (const gchar * str, GstRTSPRange * range)
{
  gchar *minus;
  gchar *tmp;

  /* even though strtol() allows white space, plus and minus in front of
   * the number, we do not allow it
   */
  if (g_ascii_isspace (*str) || *str == '+' || *str == '-')
    goto invalid_range;

  minus = strstr (str, "-");
  if (minus) {
    if (g_ascii_isspace (minus[1]) || minus[1] == '+' || minus[1] == '-')
      goto invalid_range;

    if (!check_range (str, &tmp, &range->min) || str == tmp || tmp != minus)
      goto invalid_range;

    if (!check_range (minus + 1, &tmp, &range->max) || (*tmp && *tmp != ';'))
      goto invalid_range;
  } else {
    if (!check_range (str, &tmp, &range->min) || str == tmp ||
        (*tmp && *tmp != ';'))
      goto invalid_range;

    range->max = -1;
  }

  return TRUE;

invalid_range:
  {
    range->min = -1;
    range->max = -1;
    return FALSE;
  }
}

static gchar *
range_as_text (const GstRTSPRange * range)
{
  if (range->min < 0)
    return NULL;
  else if (range->max < 0)
    return g_strdup_printf ("%d", range->min);
  else
    return g_strdup_printf ("%d-%d", range->min, range->max);
}

static const gchar *
rtsp_transport_mode_as_text (const GstRTSPTransport * transport)
{
  gint i;

  for (i = 0; transports[i].name; i++)
    if (transports[i].mode == transport->trans)
      return transports[i].name;

  return NULL;
}

static const gchar *
rtsp_transport_profile_as_text (const GstRTSPTransport * transport)
{
  gint i;

  for (i = 0; profiles[i].name; i++)
    if (profiles[i].profile == transport->profile)
      return profiles[i].name;

  return NULL;
}

static const gchar *
rtsp_transport_ltrans_as_text (const GstRTSPTransport * transport)
{
  gint i;

  /* need to special case GST_RTSP_LOWER_TRANS_UDP_MCAST */
  if (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP_MCAST)
    return "udp";

  for (i = 0; ltrans[i].name; i++)
    if (ltrans[i].ltrans == transport->lower_transport)
      return ltrans[i].name;

  return NULL;
}

#define IS_VALID_PORT_RANGE(range) \
    (range.min >= 0 && range.min < 65536 && range.max < 65536)

#define IS_VALID_INTERLEAVE_RANGE(range) \
    (range.min >= 0 && range.min < 256 && range.max < 256)

/**
 * gst_rtsp_transport_parse:
 * @str: a transport string
 * @transport: (out caller-allocates): a #GstRTSPTransport
 *
 * Parse the RTSP transport string @str into @transport.
 *
 * Returns: a #GstRTSPResult.
 */
GstRTSPResult
gst_rtsp_transport_parse (const gchar * str, GstRTSPTransport * transport)
{
  gchar **split, *down, **transp = NULL;
  guint transport_params = 0;
  gint i, count;

  g_return_val_if_fail (transport != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (str != NULL, GST_RTSP_EINVAL);

  gst_rtsp_transport_init (transport);

  /* case insensitive */
  down = g_ascii_strdown (str, -1);

  split = g_strsplit (down, ";", 0);
  g_free (down);

  /* First field contains the transport/profile/lower_transport */
  if (split[0] == NULL)
    goto invalid_transport;

  transp = g_strsplit (split[0], "/", 0);

  if (transp[0] == NULL || transp[1] == NULL)
    goto invalid_transport;

  for (i = 0; transports[i].name; i++)
    if (strcmp (transp[0], transports[i].name) == 0)
      break;
  transport->trans = transports[i].mode;

  if (transport->trans != GST_RTSP_TRANS_RDT) {
    for (i = 0; profiles[i].name; i++)
      if (strcmp (transp[1], profiles[i].name) == 0)
        break;
    transport->profile = profiles[i].profile;
    count = 2;
  } else {
    /* RDT has transport/lower_transport */
    transport->profile = GST_RTSP_PROFILE_AVP;
    count = 1;
  }

  if (transp[count] != NULL) {
    for (i = 0; ltrans[i].name; i++)
      if (strcmp (transp[count], ltrans[i].name) == 0)
        break;
    transport->lower_transport = ltrans[i].ltrans;
  } else {
    /* specifying the lower transport is optional */
    transport->lower_transport = get_default_lower_trans (transport);
  }

  g_strfreev (transp);
  transp = NULL;

  if (transport->trans == GST_RTSP_TRANS_UNKNOWN ||
      transport->profile == GST_RTSP_PROFILE_UNKNOWN ||
      transport->lower_transport == GST_RTSP_LOWER_TRANS_UNKNOWN)
    goto unsupported_transport;

  i = 1;
  while (split[i]) {
    if (strcmp (split[i], "multicast") == 0) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_DELIVERY);
      if (transport->lower_transport == GST_RTSP_LOWER_TRANS_TCP)
        goto invalid_transport;
      transport->lower_transport = GST_RTSP_LOWER_TRANS_UDP_MCAST;
    } else if (strcmp (split[i], "unicast") == 0) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_DELIVERY);
      if (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP_MCAST)
        transport->lower_transport = GST_RTSP_LOWER_TRANS_UDP;
    } else if (g_str_has_prefix (split[i], "destination=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_DESTINATION);
      transport->destination = g_strdup (split[i] + 12);
    } else if (g_str_has_prefix (split[i], "source=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_SOURCE);
      transport->source = g_strdup (split[i] + 7);
    } else if (g_str_has_prefix (split[i], "layers=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_LAYERS);
      transport->layers = strtoul (split[i] + 7, NULL, 10);
    } else if (g_str_has_prefix (split[i], "mode=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_MODE);
      parse_mode (transport, split[i] + 5);
      if (!transport->mode_play && !transport->mode_record)
        goto invalid_transport;
    } else if (strcmp (split[i], "append") == 0) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_APPEND);
      transport->append = TRUE;
    } else if (g_str_has_prefix (split[i], "interleaved=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_INTERLEAVED);
      parse_range (split[i] + 12, &transport->interleaved);
      if (!IS_VALID_INTERLEAVE_RANGE (transport->interleaved))
        goto invalid_transport;
    } else if (g_str_has_prefix (split[i], "ttl=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_TTL);
      transport->ttl = strtoul (split[i] + 4, NULL, 10);
      if (transport->ttl >= 256)
        goto invalid_transport;
    } else if (g_str_has_prefix (split[i], "port=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_PORT);
      if (parse_range (split[i] + 5, &transport->port)) {
        if (!IS_VALID_PORT_RANGE (transport->port))
          goto invalid_transport;
      }
    } else if (g_str_has_prefix (split[i], "client_port=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_CLIENT_PORT);
      if (parse_range (split[i] + 12, &transport->client_port)) {
        if (!IS_VALID_PORT_RANGE (transport->client_port))
          goto invalid_transport;
      }
    } else if (g_str_has_prefix (split[i], "server_port=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_SERVER_PORT);
      if (parse_range (split[i] + 12, &transport->server_port)) {
        if (!IS_VALID_PORT_RANGE (transport->server_port))
          goto invalid_transport;
      }
    } else if (g_str_has_prefix (split[i], "ssrc=")) {
      RTSP_TRANSPORT_PARAMETER_IS_UNIQUE (RTSP_TRANSPORT_SSRC);
      transport->ssrc = strtoul (split[i] + 5, NULL, 16);
    } else {
      /* unknown field... */
      if (strlen (split[i]) > 0) {
        g_warning ("unknown transport field \"%s\"", split[i]);
      }
    }
    i++;
  }
  g_strfreev (split);

  return GST_RTSP_OK;

unsupported_transport:
  {
    g_strfreev (split);
    return GST_RTSP_ERROR;
  }
invalid_transport:
  {
    g_strfreev (transp);
    g_strfreev (split);
    return GST_RTSP_EINVAL;
  }
}

/**
 * gst_rtsp_transport_as_text:
 * @transport: a #GstRTSPTransport
 *
 * Convert @transport into a string that can be used to signal the transport in
 * an RTSP SETUP response.
 *
 * Returns: (transfer full) (nullable): a string describing the RTSP transport
 * or %NULL when the transport is invalid.
 */
gchar *
gst_rtsp_transport_as_text (GstRTSPTransport * transport)
{
  GPtrArray *strs;
  gchar *res;
  const gchar *tmp;

  g_return_val_if_fail (transport != NULL, NULL);

  strs = g_ptr_array_new ();

  /* add the transport specifier */
  if ((tmp = rtsp_transport_mode_as_text (transport)) == NULL)
    goto invalid_transport;
  g_ptr_array_add (strs, g_ascii_strup (tmp, -1));

  g_ptr_array_add (strs, g_strdup ("/"));

  if ((tmp = rtsp_transport_profile_as_text (transport)) == NULL)
    goto invalid_transport;
  g_ptr_array_add (strs, g_ascii_strup (tmp, -1));

  if (transport->trans != GST_RTSP_TRANS_RTP ||
      (transport->profile != GST_RTSP_PROFILE_AVP &&
          transport->profile != GST_RTSP_PROFILE_SAVP &&
          transport->profile != GST_RTSP_PROFILE_AVPF &&
          transport->profile != GST_RTSP_PROFILE_SAVPF) ||
      transport->lower_transport == GST_RTSP_LOWER_TRANS_TCP) {
    g_ptr_array_add (strs, g_strdup ("/"));

    if ((tmp = rtsp_transport_ltrans_as_text (transport)) == NULL)
      goto invalid_transport;

    g_ptr_array_add (strs, g_ascii_strup (tmp, -1));
  }

  /*
   * the order of the following parameters is the same as the one specified in
   * RFC 2326 to please some weird RTSP clients that require it
   */

  /* add the unicast/multicast parameter */
  if (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP_MCAST)
    g_ptr_array_add (strs, g_strdup (";multicast"));
  else
    g_ptr_array_add (strs, g_strdup (";unicast"));

  /* add the destination parameter */
  if (transport->destination != NULL) {
    g_ptr_array_add (strs, g_strdup (";destination="));
    g_ptr_array_add (strs, g_strdup (transport->destination));
  }

  /* add the source parameter */
  if (transport->source != NULL) {
    g_ptr_array_add (strs, g_strdup (";source="));
    g_ptr_array_add (strs, g_strdup (transport->source));
  }

  /* add the interleaved parameter */
  if (transport->lower_transport == GST_RTSP_LOWER_TRANS_TCP &&
      transport->interleaved.min >= 0) {
    if (transport->interleaved.min < 256 && transport->interleaved.max < 256) {
      g_ptr_array_add (strs, g_strdup (";interleaved="));
      g_ptr_array_add (strs, range_as_text (&transport->interleaved));
    } else
      goto invalid_transport;
  }

  /* add the append parameter */
  if (transport->mode_record && transport->append)
    g_ptr_array_add (strs, g_strdup (";append"));

  /* add the ttl parameter */
  if (transport->lower_transport == GST_RTSP_LOWER_TRANS_UDP_MCAST &&
      transport->ttl != 0) {
    if (transport->ttl < 256) {
      g_ptr_array_add (strs, g_strdup (";ttl="));
      g_ptr_array_add (strs, g_strdup_printf ("%u", transport->ttl));
    } else
      goto invalid_transport;
  }

  /* add the layers parameter */
  if (transport->layers != 0) {
    g_ptr_array_add (strs, g_strdup (";layers="));
    g_ptr_array_add (strs, g_strdup_printf ("%u", transport->layers));
  }

  /* add the port parameter */
  if (transport->lower_transport != GST_RTSP_LOWER_TRANS_TCP) {
    if (transport->trans == GST_RTSP_TRANS_RTP && transport->port.min >= 0) {
      if (transport->port.min < 65536 && transport->port.max < 65536) {
        g_ptr_array_add (strs, g_strdup (";port="));
        g_ptr_array_add (strs, range_as_text (&transport->port));
      } else
        goto invalid_transport;
    }

    /* add the client_port parameter */
    if (transport->trans == GST_RTSP_TRANS_RTP
        && transport->client_port.min >= 0) {
      if (transport->client_port.min < 65536
          && transport->client_port.max < 65536) {
        g_ptr_array_add (strs, g_strdup (";client_port="));
        if (transport->client_port.max > 0)
          g_ptr_array_add (strs, range_as_text (&transport->client_port));
        else
          g_ptr_array_add (strs, g_strdup_printf ("%d",
                  transport->client_port.min));
      } else
        goto invalid_transport;
    }

    /* add the server_port parameter */
    if (transport->trans == GST_RTSP_TRANS_RTP
        && transport->server_port.min >= 0) {
      if (transport->server_port.min < 65536
          && transport->server_port.max < 65536) {
        g_ptr_array_add (strs, g_strdup (";server_port="));
        if (transport->server_port.max > 0)
          g_ptr_array_add (strs, range_as_text (&transport->server_port));
        else
          g_ptr_array_add (strs, g_strdup_printf ("%d",
                  transport->server_port.min));
      } else
        goto invalid_transport;
    }
  }

  /* add the ssrc parameter */
  if (transport->lower_transport != GST_RTSP_LOWER_TRANS_UDP_MCAST &&
      transport->ssrc != 0) {
    g_ptr_array_add (strs, g_strdup (";ssrc="));
    g_ptr_array_add (strs, g_strdup_printf ("%08X", transport->ssrc));
  }

  /* add the mode parameter */
  if (transport->mode_play && transport->mode_record)
    g_ptr_array_add (strs, g_strdup (";mode=\"PLAY,RECORD\""));
  else if (transport->mode_record)
    g_ptr_array_add (strs, g_strdup (";mode=\"RECORD\""));
  else if (transport->mode_play)
    g_ptr_array_add (strs, g_strdup (";mode=\"PLAY\""));

  /* add a terminating NULL */
  g_ptr_array_add (strs, NULL);

  res = g_strjoinv (NULL, (gchar **) strs->pdata);
  g_strfreev ((gchar **) g_ptr_array_free (strs, FALSE));

  return res;

invalid_transport:
  {
    g_ptr_array_add (strs, NULL);
    g_strfreev ((gchar **) g_ptr_array_free (strs, FALSE));
    return NULL;
  }
}

/**
 * gst_rtsp_transport_free:
 * @transport: a #GstRTSPTransport
 *
 * Free the memory used by @transport.
 *
 * Returns: #GST_RTSP_OK.
 */
GstRTSPResult
gst_rtsp_transport_free (GstRTSPTransport * transport)
{
  g_return_val_if_fail (transport != NULL, GST_RTSP_EINVAL);

  gst_rtsp_transport_init (transport);
  g_free (transport);

  return GST_RTSP_OK;
}
