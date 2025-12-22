/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2019-2020 Stephan Hesse <stephan@emliri.com>
 * Copyright (C) 2020 Philippe Normand <philn@igalia.com>
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

#ifndef __GST_PLAY_H__
#define __GST_PLAY_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/play/play-prelude.h>
#include <gst/play/gstplay-types.h>
#include <gst/play/gstplay-video-renderer.h>
#include <gst/play/gstplay-media-info.h>

G_BEGIN_DECLS

GST_PLAY_API
GType        gst_play_state_get_type                (void);

/**
 * GST_TYPE_PLAY_STATE:
 * Since: 1.20
 */
#define      GST_TYPE_PLAY_STATE                    (gst_play_state_get_type ())

GST_PLAY_API
GType        gst_play_message_get_type              (void);

/**
 * GST_TYPE_PLAY_MESSAGE:
 * Since: 1.20
 */
#define      GST_TYPE_PLAY_MESSAGE                  (gst_play_message_get_type ())

/**
 * GstPlayState:
 * @GST_PLAY_STATE_STOPPED: the play is stopped.
 * @GST_PLAY_STATE_BUFFERING: the play is buffering.
 * @GST_PLAY_STATE_PAUSED: the play is paused.
 * @GST_PLAY_STATE_PLAYING: the play is currently playing a
 * stream.
 *
 * Since: 1.20
 */
typedef enum
{
  GST_PLAY_STATE_STOPPED,
  GST_PLAY_STATE_BUFFERING,
  GST_PLAY_STATE_PAUSED,
  GST_PLAY_STATE_PLAYING
} GstPlayState;

/**
 * GstPlayMessage:
 * @GST_PLAY_MESSAGE_URI_LOADED: Source element was initalized for set URI
 * @GST_PLAY_MESSAGE_POSITION_UPDATED: Sink position changed
 * @GST_PLAY_MESSAGE_DURATION_CHANGED: Duration of stream changed
 * @GST_PLAY_MESSAGE_STATE_CHANGED: State changed, see #GstPlayState
 * @GST_PLAY_MESSAGE_BUFFERING: Pipeline is in buffering state, message contains the percentage value of the decoding buffer
 * @GST_PLAY_MESSAGE_END_OF_STREAM: Sink has received EOS
 * @GST_PLAY_MESSAGE_ERROR: Message contains an error
 * @GST_PLAY_MESSAGE_WARNING: Message contains an error
 * @GST_PLAY_MESSAGE_VIDEO_DIMENSIONS_CHANGED: Video sink received format in different dimensions than before
 * @GST_PLAY_MESSAGE_MEDIA_INFO_UPDATED: A media-info property has changed, message contains current #GstPlayMediaInfo
 * @GST_PLAY_MESSAGE_VOLUME_CHANGED: The volume of the audio ouput has changed
 * @GST_PLAY_MESSAGE_MUTE_CHANGED: Audio muting flag has been toggled
 * @GST_PLAY_MESSAGE_SEEK_DONE: Any pending seeking operation has been completed
 *
 * Since: 1.20
 *
 * Types of messages that will be posted on the play API bus.
 *
 * See also #gst_play_get_message_bus()
 *
 */
typedef enum
{
  GST_PLAY_MESSAGE_URI_LOADED,
  GST_PLAY_MESSAGE_POSITION_UPDATED,
  GST_PLAY_MESSAGE_DURATION_CHANGED,
  GST_PLAY_MESSAGE_STATE_CHANGED,
  GST_PLAY_MESSAGE_BUFFERING,
  GST_PLAY_MESSAGE_END_OF_STREAM,
  GST_PLAY_MESSAGE_ERROR,
  GST_PLAY_MESSAGE_WARNING,
  GST_PLAY_MESSAGE_VIDEO_DIMENSIONS_CHANGED,
  GST_PLAY_MESSAGE_MEDIA_INFO_UPDATED,
  GST_PLAY_MESSAGE_VOLUME_CHANGED,
  GST_PLAY_MESSAGE_MUTE_CHANGED,
  GST_PLAY_MESSAGE_SEEK_DONE
} GstPlayMessage;

GST_PLAY_API
const gchar *gst_play_state_get_name                (GstPlayState state);

GST_PLAY_API
const gchar *gst_play_message_get_name              (GstPlayMessage message_type);

GST_PLAY_API
GQuark       gst_play_error_quark                   (void);

GST_PLAY_API
GType        gst_play_error_get_type                (void);

/**
 * GST_PLAY_ERROR:
 *
 * Since: 1.20
 */
#define      GST_PLAY_ERROR                         (gst_play_error_quark ())

/**
 * GST_TYPE_PLAY_ERROR:
 *
 * Since: 1.20
 */
#define      GST_TYPE_PLAY_ERROR                    (gst_play_error_get_type ())

/**
 * GstPlayError:
 * @GST_PLAY_ERROR_FAILED: generic error.
 *
 * Since: 1.20
 */
typedef enum {
  GST_PLAY_ERROR_FAILED = 0
} GstPlayError;

GST_PLAY_API
const gchar *gst_play_error_get_name                (GstPlayError error);

GST_PLAY_API
GType gst_play_color_balance_type_get_type          (void);

/**
 * GST_TYPE_PLAY_COLOR_BALANCE_TYPE:
 *
 * Since: 1.20
 */
#define GST_TYPE_PLAY_COLOR_BALANCE_TYPE            (gst_play_color_balance_type_get_type ())

/**
 * GstPlayColorBalanceType:
 * @GST_PLAY_COLOR_BALANCE_BRIGHTNESS: brightness or black level.
 * @GST_PLAY_COLOR_BALANCE_CONTRAST: contrast or luma gain.
 * @GST_PLAY_COLOR_BALANCE_SATURATION: color saturation or chroma
 * gain.
 * @GST_PLAY_COLOR_BALANCE_HUE: hue or color balance.
 *
 * Since: 1.20
 */
typedef enum
{
  GST_PLAY_COLOR_BALANCE_BRIGHTNESS,
  GST_PLAY_COLOR_BALANCE_CONTRAST,
  GST_PLAY_COLOR_BALANCE_SATURATION,
  GST_PLAY_COLOR_BALANCE_HUE,
} GstPlayColorBalanceType;

GST_PLAY_API
const gchar *gst_play_color_balance_type_get_name   (GstPlayColorBalanceType type);

#define GST_TYPE_PLAY             (gst_play_get_type ())
#define GST_IS_PLAY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAY))
#define GST_IS_PLAY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PLAY))
#define GST_PLAY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PLAY, GstPlayClass))
#define GST_PLAY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAY, GstPlay))
#define GST_PLAY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PLAY, GstPlayClass))

/**
 * GST_PLAY_CAST:
 * Since: 1.20
 */
#define GST_PLAY_CAST(obj)        ((GstPlay*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstPlay, gst_object_unref)
#endif

GST_PLAY_API
GType        gst_play_get_type                      (void);

GST_PLAY_API
GstPlay *    gst_play_new                           (GstPlayVideoRenderer * video_renderer);

GST_PLAY_API
GstBus *     gst_play_get_message_bus               (GstPlay    * play);

GST_PLAY_API
void         gst_play_play                          (GstPlay    * play);

GST_PLAY_API
void         gst_play_pause                         (GstPlay    * play);

GST_PLAY_API
void         gst_play_stop                          (GstPlay    * play);

GST_PLAY_API
void         gst_play_seek                          (GstPlay    * play,
                                                     GstClockTime   position);

GST_PLAY_API
void         gst_play_set_rate                      (GstPlay    * play,
                                                     gdouble        rate);

GST_PLAY_API
gdouble      gst_play_get_rate                      (GstPlay    * play);

GST_PLAY_API
gchar *      gst_play_get_uri                       (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_uri                       (GstPlay    * play,
                                                     const gchar  * uri);

GST_PLAY_API
gchar *      gst_play_get_subtitle_uri              (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_subtitle_uri              (GstPlay    * play,
                                                     const gchar *uri);

GST_PLAY_API
GstClockTime gst_play_get_position                  (GstPlay    * play);

GST_PLAY_API
GstClockTime gst_play_get_duration                  (GstPlay    * play);

GST_PLAY_API
gdouble      gst_play_get_volume                    (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_volume                    (GstPlay    * play,
                                                     gdouble        val);

GST_PLAY_API
gboolean     gst_play_get_mute                      (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_mute                      (GstPlay    * play,
                                                     gboolean       val);

GST_PLAY_API
GstElement * gst_play_get_pipeline                  (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_video_track_enabled       (GstPlay    * play,
                                                     gboolean enabled);

GST_PLAY_API
void         gst_play_set_audio_track_enabled       (GstPlay    * play,
                                                     gboolean enabled);

GST_PLAY_API
void         gst_play_set_subtitle_track_enabled    (GstPlay    * play,
                                                     gboolean enabled);

GST_PLAY_API
gboolean     gst_play_set_audio_track               (GstPlay    *play,
                                                     gint stream_index);

GST_PLAY_API
gboolean     gst_play_set_video_track               (GstPlay    *play,
                                                     gint stream_index);

GST_PLAY_API
gboolean     gst_play_set_subtitle_track            (GstPlay    *play,
                                                     gint stream_index);

GST_PLAY_API
GstPlayMediaInfo *    gst_play_get_media_info     (GstPlay * play);

GST_PLAY_API
GstPlayAudioInfo *    gst_play_get_current_audio_track (GstPlay * play);

GST_PLAY_API
GstPlayVideoInfo *    gst_play_get_current_video_track (GstPlay * play);

GST_PLAY_API
GstPlaySubtitleInfo * gst_play_get_current_subtitle_track (GstPlay * play);

GST_PLAY_API
gboolean     gst_play_set_visualization             (GstPlay    * play,
                                                     const gchar *name);

GST_PLAY_API
void         gst_play_set_visualization_enabled     (GstPlay    * play,
                                                     gboolean enabled);

GST_PLAY_API
gchar *      gst_play_get_current_visualization     (GstPlay    * play);

GST_PLAY_API
gboolean     gst_play_has_color_balance             (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_color_balance             (GstPlay    * play,
                                                     GstPlayColorBalanceType type,
                                                     gdouble value);

GST_PLAY_API
gdouble      gst_play_get_color_balance             (GstPlay    * play,
                                                     GstPlayColorBalanceType type);


GST_PLAY_API
GstVideoMultiviewFramePacking gst_play_get_multiview_mode (GstPlay    * play);

GST_PLAY_API
void                     gst_play_set_multiview_mode (GstPlay    * play,
                                                      GstVideoMultiviewFramePacking mode);

GST_PLAY_API
GstVideoMultiviewFlags  gst_play_get_multiview_flags  (GstPlay  * play);

GST_PLAY_API
void                    gst_play_set_multiview_flags  (GstPlay  * play,
                                                       GstVideoMultiviewFlags flags);

GST_PLAY_API
gint64       gst_play_get_audio_video_offset        (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_audio_video_offset        (GstPlay    * play,
                                                     gint64 offset);

GST_PLAY_API
gint64       gst_play_get_subtitle_video_offset        (GstPlay    * play);

GST_PLAY_API
void         gst_play_set_subtitle_video_offset        (GstPlay    * play,
                                                        gint64 offset);

GST_PLAY_API
gboolean       gst_play_set_config                  (GstPlay * play,
                                                     GstStructure * config);

GST_PLAY_API
GstStructure * gst_play_get_config                  (GstPlay * play);

/* helpers for configuring the config structure */

GST_PLAY_API
void           gst_play_config_set_user_agent       (GstStructure * config,
                                                     const gchar * agent);

GST_PLAY_API
gchar *        gst_play_config_get_user_agent       (const GstStructure * config);

GST_PLAY_API
void           gst_play_config_set_position_update_interval  (GstStructure * config,
                                                              guint          interval);

GST_PLAY_API
guint          gst_play_config_get_position_update_interval  (const GstStructure * config);

GST_PLAY_API
void           gst_play_config_set_seek_accurate (GstStructure * config, gboolean accurate);

GST_PLAY_API
gboolean       gst_play_config_get_seek_accurate (const GstStructure * config);

GST_PLAY_API
void           gst_play_config_set_pipeline_dump_in_error_details (GstStructure * config,
                                                                   gboolean       value);

GST_PLAY_API
gboolean       gst_play_config_get_pipeline_dump_in_error_details (const GstStructure * config);

/**
 * GstPlaySnapshotFormat:
 * @GST_PLAY_THUMBNAIL_RAW_NATIVE: raw native format.
 * @GST_PLAY_THUMBNAIL_RAW_xRGB: raw xRGB format.
 * @GST_PLAY_THUMBNAIL_RAW_BGRx: raw BGRx format.
 * @GST_PLAY_THUMBNAIL_JPG: jpeg format.
 * @GST_PLAY_THUMBNAIL_PNG: png format.
 *
 * Since: 1.20
 */
typedef enum
{
  GST_PLAY_THUMBNAIL_RAW_NATIVE = 0,
  GST_PLAY_THUMBNAIL_RAW_xRGB,
  GST_PLAY_THUMBNAIL_RAW_BGRx,
  GST_PLAY_THUMBNAIL_JPG,
  GST_PLAY_THUMBNAIL_PNG
} GstPlaySnapshotFormat;

GST_PLAY_API
GstSample * gst_play_get_video_snapshot (GstPlay * play,
    GstPlaySnapshotFormat format, const GstStructure * config);

GST_PLAY_API
gboolean       gst_play_is_play_message                        (GstMessage *msg);

GST_PLAY_API
void           gst_play_message_parse_type                       (GstMessage *msg, GstPlayMessage *type);

GST_PLAY_API
void           gst_play_message_parse_duration_updated           (GstMessage *msg, GstClockTime *duration);

GST_PLAY_API
void           gst_play_message_parse_position_updated           (GstMessage *msg, GstClockTime *position);

GST_PLAY_API
void           gst_play_message_parse_state_changed              (GstMessage *msg, GstPlayState *state);

GST_PLAY_API
void           gst_play_message_parse_buffering_percent          (GstMessage *msg, guint *percent);

GST_PLAY_API
void           gst_play_message_parse_error                      (GstMessage *msg, GError **error, GstStructure **details);

GST_PLAY_API
void           gst_play_message_parse_warning                    (GstMessage *msg, GError **error, GstStructure **details);

GST_PLAY_API
void           gst_play_message_parse_video_dimensions_changed   (GstMessage *msg, guint *width, guint *height);

GST_PLAY_API
void           gst_play_message_parse_media_info_updated         (GstMessage *msg, GstPlayMediaInfo **info);

GST_PLAY_API
void           gst_play_message_parse_volume_changed             (GstMessage *msg, gdouble *volume);

GST_PLAY_API
void           gst_play_message_parse_muted_changed              (GstMessage *msg, gboolean *muted);

G_END_DECLS

#endif /* __GST_PLAY_H__ */
