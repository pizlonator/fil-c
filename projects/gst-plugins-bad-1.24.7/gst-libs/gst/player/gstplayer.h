/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_PLAYER_H__
#define __GST_PLAYER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/player/player-prelude.h>
#include <gst/player/gstplayer-types.h>
#include <gst/player/gstplayer-signal-dispatcher.h>
#include <gst/player/gstplayer-video-renderer.h>
#include <gst/player/gstplayer-media-info.h>

G_BEGIN_DECLS

GST_PLAYER_API
GType        gst_player_state_get_type                (void);
#define      GST_TYPE_PLAYER_STATE                    (gst_player_state_get_type ())

/**
 * GstPlayerState:
 * @GST_PLAYER_STATE_STOPPED: the player is stopped.
 * @GST_PLAYER_STATE_BUFFERING: the player is buffering.
 * @GST_PLAYER_STATE_PAUSED: the player is paused.
 * @GST_PLAYER_STATE_PLAYING: the player is currently playing a
 * stream.
 */
typedef enum
{
  GST_PLAYER_STATE_STOPPED,
  GST_PLAYER_STATE_BUFFERING,
  GST_PLAYER_STATE_PAUSED,
  GST_PLAYER_STATE_PLAYING
} GstPlayerState;

GST_PLAYER_API
const gchar *gst_player_state_get_name                (GstPlayerState state);

GST_PLAYER_API
GQuark       gst_player_error_quark                   (void);

GST_PLAYER_API
GType        gst_player_error_get_type                (void);
#define      GST_PLAYER_ERROR                         (gst_player_error_quark ())
#define      GST_TYPE_PLAYER_ERROR                    (gst_player_error_get_type ())

/**
 * GstPlayerError:
 * @GST_PLAYER_ERROR_FAILED: generic error.
 */
typedef enum {
  GST_PLAYER_ERROR_FAILED = 0
} GstPlayerError;

GST_PLAYER_API
const gchar *gst_player_error_get_name                (GstPlayerError error);

GST_PLAYER_API
GType gst_player_color_balance_type_get_type          (void);
#define GST_TYPE_PLAYER_COLOR_BALANCE_TYPE            (gst_player_color_balance_type_get_type ())

/**
 * GstPlayerColorBalanceType:
 * @GST_PLAYER_COLOR_BALANCE_BRIGHTNESS: brightness or black level.
 * @GST_PLAYER_COLOR_BALANCE_CONTRAST: contrast or luma gain.
 * @GST_PLAYER_COLOR_BALANCE_SATURATION: color saturation or chroma
 * gain.
 * @GST_PLAYER_COLOR_BALANCE_HUE: hue or color balance.
 */
typedef enum
{
  GST_PLAYER_COLOR_BALANCE_BRIGHTNESS,
  GST_PLAYER_COLOR_BALANCE_CONTRAST,
  GST_PLAYER_COLOR_BALANCE_SATURATION,
  GST_PLAYER_COLOR_BALANCE_HUE,
} GstPlayerColorBalanceType;

GST_PLAYER_API
const gchar *gst_player_color_balance_type_get_name   (GstPlayerColorBalanceType type);

#define GST_TYPE_PLAYER             (gst_player_get_type ())
#define GST_IS_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER))
#define GST_IS_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PLAYER))
#define GST_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PLAYER, GstPlayerClass))
#define GST_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER, GstPlayer))
#define GST_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PLAYER, GstPlayerClass))
#define GST_PLAYER_CAST(obj)        ((GstPlayer*)(obj))

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstPlayer, gst_object_unref)
#endif

GST_PLAYER_API
GType        gst_player_get_type                      (void);

GST_PLAYER_API
GstPlayer *  gst_player_new                           (GstPlayerVideoRenderer * video_renderer, GstPlayerSignalDispatcher * signal_dispatcher);

GST_PLAYER_API
void         gst_player_play                          (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_pause                         (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_stop                          (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_seek                          (GstPlayer    * player,
                                                       GstClockTime   position);

GST_PLAYER_API
void         gst_player_set_rate                      (GstPlayer    * player,
                                                       gdouble        rate);

GST_PLAYER_API
gdouble      gst_player_get_rate                      (GstPlayer    * player);

GST_PLAYER_API
gchar *      gst_player_get_uri                       (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_uri                       (GstPlayer    * player,
                                                       const gchar  * uri);

GST_PLAYER_API
gchar *      gst_player_get_subtitle_uri              (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_subtitle_uri              (GstPlayer    * player,
                                                       const gchar *uri);

GST_PLAYER_API
GstClockTime gst_player_get_position                  (GstPlayer    * player);

GST_PLAYER_API
GstClockTime gst_player_get_duration                  (GstPlayer    * player);

GST_PLAYER_API
gdouble      gst_player_get_volume                    (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_volume                    (GstPlayer    * player,
                                                       gdouble        val);

GST_PLAYER_API
gboolean     gst_player_get_mute                      (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_mute                      (GstPlayer    * player,
                                                       gboolean       val);

GST_PLAYER_API
GstElement * gst_player_get_pipeline                  (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_video_track_enabled       (GstPlayer    * player,
                                                       gboolean enabled);

GST_PLAYER_API
void         gst_player_set_audio_track_enabled       (GstPlayer    * player,
                                                       gboolean enabled);

GST_PLAYER_API
void         gst_player_set_subtitle_track_enabled    (GstPlayer    * player,
                                                       gboolean enabled);

GST_PLAYER_API
gboolean     gst_player_set_audio_track               (GstPlayer    *player,
                                                       gint stream_index);

GST_PLAYER_API
gboolean     gst_player_set_video_track               (GstPlayer    *player,
                                                       gint stream_index);

GST_PLAYER_API
gboolean     gst_player_set_subtitle_track            (GstPlayer    *player,
                                                       gint stream_index);

GST_PLAYER_API
GstPlayerMediaInfo *    gst_player_get_media_info     (GstPlayer * player);

GST_PLAYER_API
GstPlayerAudioInfo *    gst_player_get_current_audio_track (GstPlayer * player);

GST_PLAYER_API
GstPlayerVideoInfo *    gst_player_get_current_video_track (GstPlayer * player);

GST_PLAYER_API
GstPlayerSubtitleInfo * gst_player_get_current_subtitle_track (GstPlayer * player);

GST_PLAYER_API
gboolean     gst_player_set_visualization             (GstPlayer    * player,
                                                       const gchar *name);

GST_PLAYER_API
void         gst_player_set_visualization_enabled     (GstPlayer    * player,
                                                       gboolean enabled);

GST_PLAYER_API
gchar *      gst_player_get_current_visualization     (GstPlayer    * player);

GST_PLAYER_API
gboolean     gst_player_has_color_balance             (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_color_balance             (GstPlayer    * player,
                                                       GstPlayerColorBalanceType type,
                                                       gdouble value);

GST_PLAYER_API
gdouble      gst_player_get_color_balance             (GstPlayer    * player,
                                                       GstPlayerColorBalanceType type);


GST_PLAYER_API
GstVideoMultiviewFramePacking gst_player_get_multiview_mode (GstPlayer    * player);

GST_PLAYER_API
void                     gst_player_set_multiview_mode (GstPlayer    * player,
                                                        GstVideoMultiviewFramePacking mode);

GST_PLAYER_API
GstVideoMultiviewFlags  gst_player_get_multiview_flags  (GstPlayer  * player);

GST_PLAYER_API
void                    gst_player_set_multiview_flags  (GstPlayer  * player,
                                                         GstVideoMultiviewFlags flags);

GST_PLAYER_API
gint64       gst_player_get_audio_video_offset        (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_audio_video_offset        (GstPlayer    * player,
                                                       gint64 offset);

GST_PLAYER_API
gint64       gst_player_get_subtitle_video_offset        (GstPlayer    * player);

GST_PLAYER_API
void         gst_player_set_subtitle_video_offset        (GstPlayer    * player,
                                                          gint64 offset);

GST_PLAYER_API
gboolean       gst_player_set_config                  (GstPlayer * player,
                                                       GstStructure * config);

GST_PLAYER_API
GstStructure * gst_player_get_config                  (GstPlayer * player);

/* helpers for configuring the config structure */

GST_PLAYER_API
void           gst_player_config_set_user_agent       (GstStructure * config,
                                                       const gchar * agent);

GST_PLAYER_API
gchar *        gst_player_config_get_user_agent       (const GstStructure * config);

GST_PLAYER_API
void           gst_player_config_set_position_update_interval  (GstStructure * config,
                                                                guint          interval);

GST_PLAYER_API
guint          gst_player_config_get_position_update_interval  (const GstStructure * config);

GST_PLAYER_API
void           gst_player_config_set_seek_accurate (GstStructure * config, gboolean accurate);

GST_PLAYER_API
gboolean       gst_player_config_get_seek_accurate (const GstStructure * config);

typedef enum
{
  GST_PLAYER_THUMBNAIL_RAW_NATIVE = 0,
  GST_PLAYER_THUMBNAIL_RAW_xRGB,
  GST_PLAYER_THUMBNAIL_RAW_BGRx,
  GST_PLAYER_THUMBNAIL_JPG,
  GST_PLAYER_THUMBNAIL_PNG
} GstPlayerSnapshotFormat;

GST_PLAYER_API
GstSample * gst_player_get_video_snapshot (GstPlayer * player,
    GstPlayerSnapshotFormat format, const GstStructure * config);

G_END_DECLS

#endif /* __GST_PLAYER_H__ */
