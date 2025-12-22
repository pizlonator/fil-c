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

#ifndef __WEBRTC_SDP_H__
#define __WEBRTC_SDP_H__

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include "fwd.h"

G_BEGIN_DECLS

typedef enum
{
  SDP_NONE,
  SDP_LOCAL,
  SDP_REMOTE,
} SDPSource;

G_GNUC_INTERNAL
const gchar *                       _sdp_source_to_string                   (SDPSource source);


G_GNUC_INTERNAL
gboolean                            validate_sdp                            (GstWebRTCSignalingState state,
                                                                             SDPSource source,
                                                                             GstWebRTCSessionDescription * sdp,
                                                                             GError ** error);

G_GNUC_INTERNAL
GstWebRTCRTPTransceiverDirection    _get_direction_from_media               (const GstSDPMedia * media);
G_GNUC_INTERNAL
GstWebRTCRTPTransceiverDirection    _intersect_answer_directions            (GstWebRTCRTPTransceiverDirection offer,
                                                                             GstWebRTCRTPTransceiverDirection answer);
G_GNUC_INTERNAL
void                                _media_replace_direction                (GstSDPMedia * media,
                                                                             GstWebRTCRTPTransceiverDirection direction);
G_GNUC_INTERNAL
GstWebRTCRTPTransceiverDirection    _get_final_direction                     (GstWebRTCRTPTransceiverDirection local_dir,
                                                                             GstWebRTCRTPTransceiverDirection remote_dir);

G_GNUC_INTERNAL
GstWebRTCDTLSSetup                  _get_dtls_setup_from_media              (const GstSDPMedia * media);
G_GNUC_INTERNAL
GstWebRTCDTLSSetup                  _get_dtls_setup_from_session            (const GstSDPMessage * sdp);
G_GNUC_INTERNAL
GstWebRTCDTLSSetup                  _intersect_dtls_setup                   (GstWebRTCDTLSSetup offer);
G_GNUC_INTERNAL
void                                _media_replace_setup                    (GstSDPMedia * media,
                                                                             GstWebRTCDTLSSetup setup);
G_GNUC_INTERNAL
GstWebRTCDTLSSetup                  _get_final_setup                        (GstWebRTCDTLSSetup local_setup,
                                                                             GstWebRTCDTLSSetup remote_setup);
G_GNUC_INTERNAL
gchar *                             _generate_fingerprint_from_certificate  (gchar * certificate,
                                                                             GChecksumType checksum_type);
G_GNUC_INTERNAL
void                                _generate_ice_credentials               (gchar ** ufrag,
                                                                             gchar ** password);

G_GNUC_INTERNAL
gboolean                            _media_has_attribute_key                (const GstSDPMedia * media,
                                                                             const gchar * key);

G_GNUC_INTERNAL
int                                 _get_sctp_port_from_media               (const GstSDPMedia * media);
G_GNUC_INTERNAL
guint64                             _get_sctp_max_message_size_from_media   (const GstSDPMedia * media);

G_GNUC_INTERNAL
void                                _get_ice_credentials_from_sdp_media     (const GstSDPMessage * sdp,
                                                                             guint media_idx,
                                                                             gchar ** ufrag,
                                                                             gchar ** pwd);
G_GNUC_INTERNAL
gboolean                            _message_media_is_datachannel           (const GstSDPMessage * msg,
                                                                             guint media_id);
G_GNUC_INTERNAL
guint                               _message_get_datachannel_index          (const GstSDPMessage * msg);
G_GNUC_INTERNAL
gboolean                            _message_has_attribute_key              (const GstSDPMessage * msg,
                                                                             const gchar * key);

G_GNUC_INTERNAL
gboolean                            _get_bundle_index                       (GstSDPMessage * sdp,
                                                                             GStrv bundled,
                                                                             guint * idx);
G_GNUC_INTERNAL
gboolean                            _parse_bundle                           (GstSDPMessage * sdp,
                                                                             GStrv * bundled,
                                                                             GError ** error);

G_GNUC_INTERNAL
const gchar *                       _media_get_ice_pwd                  (const GstSDPMessage * msg,
                                                                             guint media_idx);
G_GNUC_INTERNAL
const gchar *                       _media_get_ice_ufrag                (const GstSDPMessage * msg,
                                                                             guint media_idx);

G_GNUC_INTERNAL
gboolean                            _media_is_bundle_only               (const GstSDPMedia * sdp);

#endif /* __WEBRTC_UTILS_H__ */
