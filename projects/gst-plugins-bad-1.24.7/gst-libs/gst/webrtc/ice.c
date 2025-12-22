/* GStreamer
 * Copyright (C) 2017 Matthew Waters <matthew@centricular.com>
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
 * SECTION:gstwebrtcice
 * @title: GstWebRTCICE
 * @short_description: Base class WebRTC ICE handling
 * @symbols:
 * - GstWebRTCICE
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ice.h"
#include "icestream.h"

#include "webrtc-priv.h"

#define GST_CAT_DEFAULT gst_webrtc_ice_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  SIGNAL_0,
  ADD_LOCAL_IP_ADDRESS_SIGNAL,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_MIN_RTP_PORT,
  PROP_MAX_RTP_PORT,
};

static guint gst_webrtc_ice_signals[LAST_SIGNAL] = { 0 };

#define gst_webrtc_ice_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstWebRTCICE, gst_webrtc_ice,
    GST_TYPE_OBJECT, GST_DEBUG_CATEGORY_INIT (gst_webrtc_ice_debug,
        "webrtcice", 0, "webrtcice"););

/**
 * gst_webrtc_ice_add_stream:
 * @ice: The #GstWebRTCICE
 * @session_id: The session id
 *
 * Returns: (transfer full) (nullable): The #GstWebRTCICEStream, or %NULL
 *
 * Since: 1.22
 */
GstWebRTCICEStream *
gst_webrtc_ice_add_stream (GstWebRTCICE * ice, guint session_id)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->add_stream);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->add_stream (ice, session_id);
}

/**
 * gst_webrtc_ice_find_transport:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * @component: The #GstWebRTCICEComponent
 *
 * Returns: (transfer full) (nullable): The #GstWebRTCICETransport, or %NULL
 *
 * Since: 1.22
 */
GstWebRTCICETransport *
gst_webrtc_ice_find_transport (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, GstWebRTCICEComponent component)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->find_transport);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->find_transport (ice, stream,
      component);
}

/**
 * gst_webrtc_ice_add_candidate:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * @candidate: The ICE candidate
 * @promise: (nullable): A #GstPromise for task notifications (Since: 1.24)
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_add_candidate (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, const gchar * candidate, GstPromise * promise)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->add_candidate);

  GST_WEBRTC_ICE_GET_CLASS (ice)->add_candidate (ice, stream, candidate,
      promise);
}

/**
 * gst_webrtc_ice_set_remote_credentials:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * @ufrag: ICE username
 * @pwd: ICE password
 *
 * Returns: FALSE on error, TRUE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_set_remote_credentials (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, const gchar * ufrag, const gchar * pwd)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), FALSE);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_remote_credentials);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->set_remote_credentials (ice, stream,
      ufrag, pwd);
}

/**
 * gst_webrtc_ice_add_turn_server:
 * @ice: The #GstWebRTCICE
 * @uri: URI of the TURN server
 *
 * Returns: FALSE on error, TRUE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_add_turn_server (GstWebRTCICE * ice, const gchar * uri)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), FALSE);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->add_turn_server);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->add_turn_server (ice, uri);
}

/**
 * gst_webrtc_ice_set_local_credentials:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * @ufrag: ICE username
 * @pwd: ICE password
 *
 * Returns: FALSE on error, TRUE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_set_local_credentials (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, const gchar * ufrag, const gchar * pwd)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), FALSE);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_local_credentials);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->set_local_credentials (ice, stream,
      ufrag, pwd);
}

/**
 * gst_webrtc_ice_gather_candidates:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * Returns: FALSE on error, TRUE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_gather_candidates (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), FALSE);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->gather_candidates);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->gather_candidates (ice, stream);
}

/**
 * gst_webrtc_ice_set_is_controller:
 * @ice: The #GstWebRTCICE
 * @controller: TRUE to set as controller
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_is_controller (GstWebRTCICE * ice, gboolean controller)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_is_controller);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_is_controller (ice, controller);
}

/**
 * gst_webrtc_ice_get_is_controller:
 * @ice: The #GstWebRTCICE
 * Returns: TRUE if set as controller, FALSE otherwise
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_get_is_controller (GstWebRTCICE * ice)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), FALSE);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_is_controller);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_is_controller (ice);
}

/**
 * gst_webrtc_ice_set_force_relay:
 * @ice: The #GstWebRTCICE
 * @force_relay: TRUE to enable force relay
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_force_relay (GstWebRTCICE * ice, gboolean force_relay)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_force_relay);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_force_relay (ice, force_relay);
}

/**
 * gst_webrtc_ice_set_tos:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * @tos: ToS to be set
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_tos (GstWebRTCICE * ice, GstWebRTCICEStream * stream,
    guint tos)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_tos);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_tos (ice, stream, tos);
}


/**
 * gst_webrtc_ice_get_local_candidates:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * Returns: (transfer full)(array zero-terminated=1): List of local candidates
 *
 * Since: 1.22
 */
GstWebRTCICECandidateStats **
gst_webrtc_ice_get_local_candidates (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_local_candidates);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_local_candidates (ice, stream);
}


/**
 * gst_webrtc_ice_get_remote_candidates:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * Returns: (transfer full) (array zero-terminated=1): List of remote candidates
 *
 * Since: 1.22
 */
GstWebRTCICECandidateStats **
gst_webrtc_ice_get_remote_candidates (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_remote_candidates);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_remote_candidates (ice, stream);
}

/**
 * gst_webrtc_ice_get_selected_pair:
 * @ice: The #GstWebRTCICE
 * @stream: The #GstWebRTCICEStream
 * @local_stats: (out) (transfer full): A pointer to #GstWebRTCICECandidateStats for local candidate
 * @remote_stats: (out) (transfer full): pointer to #GstWebRTCICECandidateStats for remote candidate
 *
 * Returns: FALSE on failure, otherwise @local_stats @remote_stats will be set
 *
 * Since: 1.22
 */
gboolean
gst_webrtc_ice_get_selected_pair (GstWebRTCICE * ice,
    GstWebRTCICEStream * stream, GstWebRTCICECandidateStats ** local_stats,
    GstWebRTCICECandidateStats ** remote_stats)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), FALSE);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_selected_pair);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_selected_pair (ice, stream,
      local_stats, remote_stats);
}

/**
 * gst_webrtc_ice_candidate_stats_free:
 * @stats: The #GstWebRTCICECandidateStats to be free'd
 *
 * Helper function to free #GstWebRTCICECandidateStats
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_candidate_stats_free (GstWebRTCICECandidateStats * stats)
{
  if (stats) {
    g_free (stats->ipaddr);
    g_free (stats->url);
  }

  g_free (stats);
}

/**
 * gst_webrtc_ice_candidate_stats_copy:
 * @stats: The #GstWebRTCICE
 *
 * Returns: (transfer full): A copy of @stats
 *
 * Since: 1.22
 */
GstWebRTCICECandidateStats *
gst_webrtc_ice_candidate_stats_copy (GstWebRTCICECandidateStats * stats)
{
  GstWebRTCICECandidateStats *copy =
      g_malloc (sizeof (GstWebRTCICECandidateStats));

  *copy = *stats;

  copy->ipaddr = g_strdup (stats->ipaddr);
  copy->url = g_strdup (stats->url);

  return copy;
}

G_DEFINE_BOXED_TYPE (GstWebRTCICECandidateStats, gst_webrtc_ice_candidate_stats,
    (GBoxedCopyFunc) gst_webrtc_ice_candidate_stats_copy,
    (GBoxedFreeFunc) gst_webrtc_ice_candidate_stats_free);

/**
 * gst_webrtc_ice_set_on_ice_candidate:
 * @ice: The #GstWebRTCICE
 * @func: The #GstWebRTCICEOnCandidateFunc callback function
 * @user_data: User data passed to the callback function
 * @notify: a #GDestroyNotify when the candidate is no longer needed
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_on_ice_candidate (GstWebRTCICE * ice,
    GstWebRTCICEOnCandidateFunc func, gpointer user_data, GDestroyNotify notify)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_on_ice_candidate);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_on_ice_candidate (ice, func, user_data,
      notify);
}

/**
 * gst_webrtc_ice_set_stun_server:
 * @ice: The #GstWebRTCICE
 * @uri: (nullable): URI of the STUN server
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_stun_server (GstWebRTCICE * ice, const gchar * uri_s)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_stun_server);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_stun_server (ice, uri_s);
}

/**
 * gst_webrtc_ice_get_stun_server:
 * @ice: The #GstWebRTCICE
 *
 * Returns: (nullable): URI of the STUN sever
 *
 * Since: 1.22
 */
gchar *
gst_webrtc_ice_get_stun_server (GstWebRTCICE * ice)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_stun_server);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_stun_server (ice);
}

/**
 * gst_webrtc_ice_set_turn_server:
 * @ice: The #GstWebRTCICE
 * @uri: (nullable): URI of the TURN sever
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_turn_server (GstWebRTCICE * ice, const gchar * uri_s)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_turn_server);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_turn_server (ice, uri_s);
}

/**
 * gst_webrtc_ice_get_turn_server:
 * @ice: The #GstWebRTCICE
 *
 * Returns: (nullable): URI of the TURN sever
 *
 * Since: 1.22
 */
gchar *
gst_webrtc_ice_get_turn_server (GstWebRTCICE * ice)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_turn_server);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_turn_server (ice);
}

/**
 * gst_webrtc_ice_set_http_proxy:
 * @ice: The #GstWebRTCICE
 * @uri: (transfer none): URI of the HTTP proxy of the form
 *   http://[username:password@]hostname[:port][?alpn=<alpn>]
 *
 * Set HTTP Proxy to be used when connecting to TURN server.
 *
 * Since: 1.22
 */
void
gst_webrtc_ice_set_http_proxy (GstWebRTCICE * ice, const gchar * uri_s)
{
  g_return_if_fail (GST_IS_WEBRTC_ICE (ice));
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->set_http_proxy);

  GST_WEBRTC_ICE_GET_CLASS (ice)->set_http_proxy (ice, uri_s);
}

/**
 * gst_webrtc_ice_get_http_proxy:
 * @ice: The #GstWebRTCICE
 *
 * Returns: (transfer full): URI of the HTTP proxy of the form
 *   http://[username:password@]hostname[:port][?alpn=<alpn>]
 *
 * Get HTTP Proxy to be used when connecting to TURN server.
 *
 * Since: 1.22
 */
gchar *
gst_webrtc_ice_get_http_proxy (GstWebRTCICE * ice)
{
  g_return_val_if_fail (GST_IS_WEBRTC_ICE (ice), NULL);
  g_assert (GST_WEBRTC_ICE_GET_CLASS (ice)->get_http_proxy);

  return GST_WEBRTC_ICE_GET_CLASS (ice)->get_http_proxy (ice);
}


static void
gst_webrtc_ice_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebRTCICE *ice = GST_WEBRTC_ICE (object);

  switch (prop_id) {
    case PROP_MIN_RTP_PORT:
      ice->min_rtp_port = g_value_get_uint (value);
      if (ice->min_rtp_port > ice->max_rtp_port)
        g_warning ("Set min-rtp-port to %u which is larger than"
            " max-rtp-port %u", ice->min_rtp_port, ice->max_rtp_port);
      break;
    case PROP_MAX_RTP_PORT:
      ice->max_rtp_port = g_value_get_uint (value);
      if (ice->min_rtp_port > ice->max_rtp_port)
        g_warning ("Set max-rtp-port to %u which is smaller than"
            " min-rtp-port %u", ice->max_rtp_port, ice->min_rtp_port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_ice_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstWebRTCICE *ice = GST_WEBRTC_ICE (object);

  switch (prop_id) {
    case PROP_MIN_RTP_PORT:
      g_value_set_uint (value, ice->min_rtp_port);
      break;
    case PROP_MAX_RTP_PORT:
      g_value_set_uint (value, ice->max_rtp_port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webrtc_ice_class_init (GstWebRTCICEClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  klass->add_stream = NULL;
  klass->find_transport = NULL;
  klass->gather_candidates = NULL;
  klass->add_candidate = NULL;
  klass->set_local_credentials = NULL;
  klass->set_remote_credentials = NULL;
  klass->add_turn_server = NULL;
  klass->set_is_controller = NULL;
  klass->get_is_controller = NULL;
  klass->set_force_relay = NULL;
  klass->set_stun_server = NULL;
  klass->get_stun_server = NULL;
  klass->set_turn_server = NULL;
  klass->get_turn_server = NULL;
  klass->get_http_proxy = NULL;
  klass->set_http_proxy = NULL;
  klass->set_tos = NULL;
  klass->set_on_ice_candidate = NULL;
  klass->get_local_candidates = NULL;
  klass->get_remote_candidates = NULL;
  klass->get_selected_pair = NULL;

  gobject_class->get_property = gst_webrtc_ice_get_property;
  gobject_class->set_property = gst_webrtc_ice_set_property;

  /**
   * GstWebRTCICE:min-rtp-port:
   *
   * Minimum port for local rtp port range.
   * min-rtp-port must be <= max-rtp-port
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_MIN_RTP_PORT,
      g_param_spec_uint ("min-rtp-port", "ICE RTP candidate min port",
          "Minimum port for local rtp port range. "
          "min-rtp-port must be <= max-rtp-port",
          0, 65535, 0,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCICE:max-rtp-port:
   *
   * Maximum port for local rtp port range.
   * min-rtp-port must be <= max-rtp-port
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class,
      PROP_MAX_RTP_PORT,
      g_param_spec_uint ("max-rtp-port", "ICE RTP candidate max port",
          "Maximum port for local rtp port range. "
          "max-rtp-port must be >= min-rtp-port",
          0, 65535, 65535,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebRTCICE::add-local-ip-address:
   * @object: the #GstWebRTCICE
   * @address: The local IP address
   *
   * Add a local IP address to use for ICE candidate gathering.  If none
   * are supplied, they will be discovered automatically. Calling this signal
   * stops automatic ICE gathering.
   *
   * Returns: whether the address could be added.
   */
  gst_webrtc_ice_signals[ADD_LOCAL_IP_ADDRESS_SIGNAL] =
      g_signal_new_class_handler ("add-local-ip-address",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      NULL, NULL, NULL,
      g_cclosure_marshal_generic, G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
}

static void
gst_webrtc_ice_init (GstWebRTCICE * ice)
{
}
