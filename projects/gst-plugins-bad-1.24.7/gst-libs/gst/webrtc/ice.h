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

#ifndef __GST_WEBRTC_ICE_H__
#define __GST_WEBRTC_ICE_H__

#include <gst/webrtc/webrtc_fwd.h>

G_BEGIN_DECLS

GST_WEBRTC_API
GType gst_webrtc_ice_get_type(void);
#define GST_TYPE_WEBRTC_ICE            (gst_webrtc_ice_get_type())
#define GST_WEBRTC_ICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_WEBRTC_ICE,GstWebRTCICE))
#define GST_IS_WEBRTC_ICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_WEBRTC_ICE))
#define GST_WEBRTC_ICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_WEBRTC_ICE,GstWebRTCICEClass))
#define GST_IS_WEBRTC_ICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_WEBRTC_ICE))
#define GST_WEBRTC_ICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_WEBRTC_ICE,GstWebRTCICEClass))

struct _GstWebRTCICE
{
  GstObject                          parent;

  GstWebRTCICEGatheringState         ice_gathering_state;
  GstWebRTCICEConnectionState        ice_connection_state;

  /*< protected >*/
  guint                              min_rtp_port;
  guint                              max_rtp_port;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstWebRTCICECandidateStats
{
  gchar                            *ipaddr;
  guint                             port;
  guint                             stream_id;
  const gchar                      *type;
  const gchar                      *proto;
  const gchar                      *relay_proto;
  guint                             prio;
  gchar                            *url;

  gpointer _gst_reserved[GST_PADDING_LARGE];
};

/**
 * GstWebRTCICEOnCandidateFunc:
 * @ice: The #GstWebRTCICE
 * @stream_id: The stream id
 * @candidate: The discovered candidate
 * @user_data: User data that was set by #gst_webrtc_ice_set_on_ice_candidate
 *
 * Callback function to be triggered on discovery of a new candidate
 * Since: 1.22
 */
typedef void (*GstWebRTCICEOnCandidateFunc) (GstWebRTCICE * ice, guint stream_id, const gchar * candidate, gpointer user_data);

struct _GstWebRTCICEClass {
  GstObjectClass parent_class;
  GstWebRTCICEStream * (*add_stream)                   (GstWebRTCICE * ice,
                                                        guint session_id);
  GstWebRTCICETransport * (*find_transport)            (GstWebRTCICE * ice,
                                                        GstWebRTCICEStream * stream,
                                                        GstWebRTCICEComponent component);
  gboolean (*gather_candidates)                        (GstWebRTCICE * ice,
                                                        GstWebRTCICEStream * stream);
  void (*add_candidate)                                (GstWebRTCICE * ice,
                                                        GstWebRTCICEStream * stream,
                                                        const gchar * candidate,
                                                        GstPromise * promise);
  gboolean (*set_local_credentials)                    (GstWebRTCICE * ice,
                                                        GstWebRTCICEStream * stream,
                                                        const gchar * ufrag,
                                                        const gchar * pwd);
  gboolean (*set_remote_credentials)                   (GstWebRTCICE * ice,
                                                        GstWebRTCICEStream * stream,
                                                        const gchar * ufrag,
                                                        const gchar * pwd);
  gboolean (*add_turn_server)                          (GstWebRTCICE * ice,
                                                        const gchar * uri);
  void (*set_is_controller)                            (GstWebRTCICE * ice,
                                                        gboolean controller);
  gboolean (*get_is_controller)                        (GstWebRTCICE * ice);
  void (*set_force_relay)                              (GstWebRTCICE * ice,
                                                        gboolean force_relay);
  void (*set_stun_server)                              (GstWebRTCICE * ice,
                                                        const gchar * uri);
  gchar * (*get_stun_server)                           (GstWebRTCICE * ice);
  void (*set_turn_server)                              (GstWebRTCICE * ice,
                                                        const gchar * uri);
  gchar * (*get_turn_server)                           (GstWebRTCICE * ice);

  /**
   * GstWebRTCICEClass::set_http_proxy:
   * @ice: a #GstWebRTCICE
   * @uri: (transfer none): URI of the HTTP proxy of the form
   *   http://[username:password@]hostname[:port]
   *
   * Set HTTP Proxy to be used when connecting to TURN server.
   *
   * Since: 1.22
   */
  void (*set_http_proxy)                               (GstWebRTCICE * ice,
                                                        const gchar * uri);

  /**
   * GstWebRTCICEClass::get_http_proxy:
   * @ice: a #GstWebRTCICE
   *
   * Get HTTP Proxy to be used when connecting to TURN server.
   *
   * Returns: (transfer full): URI of the HTTP proxy of the form
   *   http://[username:password@]hostname[:port]
   *
   * Since: 1.22
   */
  gchar * (*get_http_proxy)                            (GstWebRTCICE * ice);

  void (*set_tos)                                      (GstWebRTCICE * ice,
                                                        GstWebRTCICEStream * stream,
                                                        guint tos);
  void (*set_on_ice_candidate)                         (GstWebRTCICE * ice,
                                                        GstWebRTCICEOnCandidateFunc func,
                                                        gpointer user_data,
                                                        GDestroyNotify notify);
  GstWebRTCICECandidateStats** (*get_local_candidates)(GstWebRTCICE * ice,
                                                       GstWebRTCICEStream * stream);
  GstWebRTCICECandidateStats**(*get_remote_candidates)(GstWebRTCICE * ice,
                                                       GstWebRTCICEStream * stream);
  gboolean (*get_selected_pair)                       (GstWebRTCICE * ice,
                                                       GstWebRTCICEStream * stream,
                                                       GstWebRTCICECandidateStats ** local_stats,
                                                       GstWebRTCICECandidateStats ** remote_stats);
  gpointer _gst_reserved[GST_PADDING];
};

GST_WEBRTC_API
GstWebRTCICEStream *        gst_webrtc_ice_add_stream               (GstWebRTCICE * ice,
                                                                     guint session_id);

GST_WEBRTC_API
GstWebRTCICETransport *     gst_webrtc_ice_find_transport           (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream,
                                                                     GstWebRTCICEComponent component);


GST_WEBRTC_API
gboolean                    gst_webrtc_ice_gather_candidates        (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream);

/* FIXME: GstStructure-ize the candidate */
GST_WEBRTC_API
void                        gst_webrtc_ice_add_candidate            (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream,
                                                                     const gchar * candidate,
                                                                     GstPromise * promise);

GST_WEBRTC_API
gboolean                    gst_webrtc_ice_set_local_credentials    (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream,
                                                                     const gchar * ufrag,
                                                                     const gchar * pwd);

GST_WEBRTC_API
gboolean                    gst_webrtc_ice_set_remote_credentials   (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream,
                                                                     const gchar * ufrag,
                                                                     const gchar * pwd);

GST_WEBRTC_API
gboolean                    gst_webrtc_ice_add_turn_server          (GstWebRTCICE * ice,
                                                                     const gchar * uri);


GST_WEBRTC_API
void                        gst_webrtc_ice_set_is_controller        (GstWebRTCICE * ice,
                                                                     gboolean controller);

GST_WEBRTC_API
gboolean                    gst_webrtc_ice_get_is_controller        (GstWebRTCICE * ice);

GST_WEBRTC_API
void                        gst_webrtc_ice_set_force_relay          (GstWebRTCICE * ice,
                                                                     gboolean force_relay);

GST_WEBRTC_API
void                        gst_webrtc_ice_set_stun_server          (GstWebRTCICE * ice,
                                                                     const gchar * uri);

GST_WEBRTC_API
gchar *                     gst_webrtc_ice_get_stun_server          (GstWebRTCICE * ice);

GST_WEBRTC_API
void                        gst_webrtc_ice_set_turn_server          (GstWebRTCICE * ice,
                                                                     const gchar * uri);

GST_WEBRTC_API
gchar *                     gst_webrtc_ice_get_turn_server          (GstWebRTCICE * ice);

GST_WEBRTC_API
void                        gst_webrtc_ice_set_http_proxy           (GstWebRTCICE * ice,
                                                                     const gchar * uri);

GST_WEBRTC_API
gchar *                     gst_webrtc_ice_get_http_proxy           (GstWebRTCICE * ice);

GST_WEBRTC_API
void                        gst_webrtc_ice_set_on_ice_candidate     (GstWebRTCICE * ice,
                                                                     GstWebRTCICEOnCandidateFunc func,
                                                                     gpointer user_data,
                                                                     GDestroyNotify notify);

GST_WEBRTC_API
void                        gst_webrtc_ice_set_tos                  (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream,
                                                                     guint tos);

GST_WEBRTC_API
GstWebRTCICECandidateStats** gst_webrtc_ice_get_local_candidates    (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream);

GST_WEBRTC_API
GstWebRTCICECandidateStats** gst_webrtc_ice_get_remote_candidates   (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream);

GST_WEBRTC_API
gboolean                    gst_webrtc_ice_get_selected_pair        (GstWebRTCICE * ice,
                                                                     GstWebRTCICEStream * stream,
                                                                     GstWebRTCICECandidateStats ** local_stats,
                                                                     GstWebRTCICECandidateStats ** remote_stats);

GST_WEBRTC_API
void                        gst_webrtc_ice_candidate_stats_free     (GstWebRTCICECandidateStats * stats);

GST_WEBRTC_API
GType                       gst_webrtc_ice_candidate_stats_get_type (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstWebRTCICE, gst_object_unref)

GST_WEBRTC_API
GstWebRTCICECandidateStats * gst_webrtc_ice_candidate_stats_copy   (GstWebRTCICECandidateStats *stats);

G_END_DECLS

#endif /* __GST_WEBRTC_ICE_H__ */
