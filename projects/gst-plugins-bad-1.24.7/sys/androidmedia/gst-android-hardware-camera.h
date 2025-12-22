/*
 * Copyright (C) 2012, Collabora Ltd.
 * Copyright (C) 2012, Cisco Systems, Inc.
 *   Author: Youness Alaoui <youness.alaoui@collabora.co.uk>
 *
 * Copyright (C) 2015, Collabora Ltd.
 *   Author: Justin Kim <justin.kim@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef __GST_ANDROID_HARDWARE_CAMERA_H__
#define __GST_ANDROID_HARDWARE_CAMERA_H__

#include <gst/gst.h>
#include <jni.h>

#include "gst-android-graphics-imageformat.h"
#include "jni/gstamcsurfacetexture-jni.h"

G_BEGIN_DECLS

typedef struct _GstAHCamera GstAHCamera;
typedef struct _GstAHCCameraInfo GstAHCCameraInfo;
typedef struct _GstAHCSize GstAHCSize;
typedef struct _GstAHCParameters GstAHCParameters;

/* android.hardware.Camera */
struct _GstAHCamera
{
  /* < private > */
  jobject object;               /* global reference */
};

/* android.hardware.Camera.CameraInfo */
struct _GstAHCCameraInfo
{
  gint facing;
  gint orientation;
};
extern gint CameraInfo_CAMERA_FACING_BACK;
extern gint CameraInfo_CAMERA_FACING_FRONT;

/* android.hardware.Camera.Size */
struct _GstAHCSize
{
  gint width;
  gint height;
};

/* android.hardware.Camera.Parameters */
struct _GstAHCParameters
{
  /* < private > */
  jobject object;               /* global reference */
};
extern const gchar *Parameters_WHITE_BALANCE_AUTO;
extern const gchar *Parameters_WHITE_BALANCE_INCANDESCENT;
extern const gchar *Parameters_WHITE_BALANCE_FLUORESCENT;
extern const gchar *Parameters_WHITE_BALANCE_WARM_FLUORESCENT;
extern const gchar *Parameters_WHITE_BALANCE_DAYLIGHT;
extern const gchar *Parameters_WHITE_BALANCE_CLOUDY_DAYLIGHT;
extern const gchar *Parameters_WHITE_BALANCE_TWILIGHT;
extern const gchar *Parameters_WHITE_BALANCE_SHADE;
extern const gchar *Parameters_EFFECT_NONE;
extern const gchar *Parameters_EFFECT_MONO;
extern const gchar *Parameters_EFFECT_NEGATIVE;
extern const gchar *Parameters_EFFECT_SOLARIZE;
extern const gchar *Parameters_EFFECT_SEPIA;
extern const gchar *Parameters_EFFECT_POSTERIZE;
extern const gchar *Parameters_EFFECT_WHITEBOARD;
extern const gchar *Parameters_EFFECT_BLACKBOARD;
extern const gchar *Parameters_EFFECT_AQUA;
/* EMBOSS, SKETCH and NEON are not standard Android API
 * but are available only on some Qualcomm devices */
extern const gchar *Parameters_EFFECT_EMBOSS;
extern const gchar *Parameters_EFFECT_SKETCH;
extern const gchar *Parameters_EFFECT_NEON;
extern const gchar *Parameters_ANTIBANDING_AUTO;
extern const gchar *Parameters_ANTIBANDING_50HZ;
extern const gchar *Parameters_ANTIBANDING_60HZ;
extern const gchar *Parameters_ANTIBANDING_OFF;
extern const gchar *Parameters_FLASH_MODE_OFF;
extern const gchar *Parameters_FLASH_MODE_AUTO;
extern const gchar *Parameters_FLASH_MODE_ON;
extern const gchar *Parameters_FLASH_MODE_RED_EYE;
extern const gchar *Parameters_FLASH_MODE_TORCH;
extern const gchar *Parameters_SCENE_MODE_AUTO;
extern const gchar *Parameters_SCENE_MODE_ACTION;
extern const gchar *Parameters_SCENE_MODE_PORTRAIT;
extern const gchar *Parameters_SCENE_MODE_LANDSCAPE;
extern const gchar *Parameters_SCENE_MODE_NIGHT;
extern const gchar *Parameters_SCENE_MODE_NIGHT_PORTRAIT;
extern const gchar *Parameters_SCENE_MODE_THEATRE;
extern const gchar *Parameters_SCENE_MODE_BEACH;
extern const gchar *Parameters_SCENE_MODE_SNOW;
extern const gchar *Parameters_SCENE_MODE_SUNSET;
extern const gchar *Parameters_SCENE_MODE_STEADYPHOTO;
extern const gchar *Parameters_SCENE_MODE_FIREWORKS;
extern const gchar *Parameters_SCENE_MODE_SPORTS;
extern const gchar *Parameters_SCENE_MODE_PARTY;
extern const gchar *Parameters_SCENE_MODE_CANDLELIGHT;
extern const gchar *Parameters_SCENE_MODE_BARCODE;
/* BACKLIGHT, FLOWERS and AR are not standard Android API
 * but are available only on some Qualcomm devices. */
extern const gchar *Parameters_SCENE_MODE_BACKLIGHT;
extern const gchar *Parameters_SCENE_MODE_FLOWERS;
extern const gchar *Parameters_SCENE_MODE_AR;
/* HDR is available in API 17+ (Android 4.2) */
extern const gchar *Parameters_SCENE_MODE_HDR;
extern const gchar *Parameters_FOCUS_MODE_AUTO;
extern const gchar *Parameters_FOCUS_MODE_INFINITY;
extern const gchar *Parameters_FOCUS_MODE_MACRO;
extern const gchar *Parameters_FOCUS_MODE_FIXED;
extern const gchar *Parameters_FOCUS_MODE_EDOF;
extern const gchar *Parameters_FOCUS_MODE_CONTINUOUS_VIDEO;
extern const gchar *Parameters_FOCUS_MODE_CONTINUOUS_PICTURE;

/* android.hardware.Camera.ErrorCallback */
typedef void (*GstAHCErrorCallback) (gint error, gpointer user_data);

/* android.hardware.Camera.PreviewCallback */
typedef void (*GstAHCPreviewCallback) (jbyteArray data, gpointer user_data);

/* android.hardware.Camera.AutoFocusCallback */
typedef void (*GstAHCAutoFocusCallback) (gboolean success, gpointer user_data);

gboolean gst_android_hardware_camera_init (void);
void gst_android_hardware_camera_deinit (void);

/* android.hardware.Camera */
void gst_ah_camera_add_callback_buffer (GstAHCamera * self, jbyteArray buffer);
gboolean gst_ah_camera_auto_focus (GstAHCamera * self,
    GstAHCAutoFocusCallback cb, gpointer user_data);
gboolean gst_ah_camera_cancel_auto_focus (GstAHCamera * self);
gboolean gst_ah_camera_get_camera_info (gint camera_id,
    GstAHCCameraInfo * camera_info);
gint gst_ah_camera_get_number_of_cameras (void);
GstAHCParameters *gst_ah_camera_get_parameters (GstAHCamera * self);
gboolean gst_ah_camera_lock (GstAHCamera * self);
GstAHCamera *gst_ah_camera_open (gint camera_id);
gboolean gst_ah_camera_reconnect (GstAHCamera * self);
void gst_ah_camera_release (GstAHCamera * self);
void gst_ah_camera_free (GstAHCamera * self);
gboolean gst_ah_camera_set_parameters (GstAHCamera * self,
    GstAHCParameters * params);
gboolean gst_ah_camera_set_error_callback (GstAHCamera * self,
    GstAHCErrorCallback cb, gpointer user_data);
gboolean gst_ah_camera_set_preview_callback_with_buffer (GstAHCamera * self,
    GstAHCPreviewCallback cb, gpointer user_data);
void gst_ah_camera_set_preview_texture (GstAHCamera * self,
    GstAmcSurfaceTextureJNI * surfaceTexture);
gboolean gst_ah_camera_start_preview (GstAHCamera * self);
gboolean gst_ah_camera_start_smooth_zoom (GstAHCamera * self, gint value);
gboolean gst_ah_camera_stop_preview (GstAHCamera * self);
gboolean gst_ah_camera_stop_smooth_zoom (GstAHCamera * self);
gboolean gst_ah_camera_unlock (GstAHCamera * self);

/* android.hardware.Camera.Size */
GstAHCSize *gst_ahc_size_new (gint width, gint height);
void gst_ahc_size_free (GstAHCSize * self);

/* android.hardware.Camera.Parameters */
gchar *gst_ahc_parameters_flatten (GstAHCParameters * self);
const gchar *gst_ahc_parameters_get_antibanding (GstAHCParameters * self);
const gchar *gst_ahc_parameters_get_color_effect (GstAHCParameters * self);
gint gst_ahc_parameters_get_exposure_compensation (GstAHCParameters * self);
gfloat gst_ahc_parameters_get_exposure_compensation_step (GstAHCParameters
    * self);
const gchar *gst_ahc_parameters_get_flash_mode (GstAHCParameters * self);
gfloat gst_ahc_parameters_get_focal_length (GstAHCParameters * self);
const gchar *gst_ahc_parameters_get_focus_mode (GstAHCParameters * self);
gfloat gst_ahc_parameters_get_horizontal_view_angle (GstAHCParameters * self);
gint gst_ahc_parameters_get_max_exposure_compensation (GstAHCParameters * self);
gint gst_ahc_parameters_get_max_zoom (GstAHCParameters * self);
gint gst_ahc_parameters_get_min_exposure_compensation (GstAHCParameters * self);
gint gst_ahc_parameters_get_preview_format (GstAHCParameters * self);
gboolean gst_ahc_parameters_get_preview_fps_range (GstAHCParameters * self,
    gint * min, gint * max);
GstAHCSize *gst_ahc_parameters_get_preview_size (GstAHCParameters * self);
const gchar *gst_ahc_parameters_get_scene_mode (GstAHCParameters * self);
/* GList <const gchar *> */
GList *gst_ahc_parameters_get_supported_antibanding (GstAHCParameters * self);
void gst_ahc_parameters_supported_antibanding_free (GList * list);
/* GList <const gchar *> */
GList *gst_ahc_parameters_get_supported_color_effects (GstAHCParameters * self);
void gst_ahc_parameters_supported_color_effects_free (GList * list);
/* GList <const gchar *> */
GList *gst_ahc_parameters_get_supported_flash_modes (GstAHCParameters * self);
void gst_ahc_parameters_supported_flash_modes_free (GList * list);
/* GList <const gchar *> */
GList *gst_ahc_parameters_get_supported_focus_modes (GstAHCParameters * self);
void gst_ahc_parameters_supported_focus_modes_free (GList * list);
/* GList <int> */
GList *gst_ahc_parameters_get_supported_preview_formats (GstAHCParameters
    * self);
void gst_ahc_parameters_supported_preview_formats_free (GList * list);
/* GList <int [2]> */
GList *gst_ahc_parameters_get_supported_preview_fps_range (GstAHCParameters
    * self);
void gst_ahc_parameters_supported_preview_fps_range_free (GList * list);
/* GList <GstAHCSize *> */
GList *gst_ahc_parameters_get_supported_preview_sizes (GstAHCParameters * self);
void gst_ahc_parameters_supported_preview_sizes_free (GList * list);
/* GList <const gchar *> */
GList *gst_ahc_parameters_get_supported_scene_modes (GstAHCParameters * self);
void gst_ahc_parameters_supported_scene_modes_free (GList * list);
/* GList <const gchar *> */
GList *gst_ahc_parameters_get_supported_white_balance (GstAHCParameters * self);
void gst_ahc_parameters_supported_white_balance_free (GList * list);
gfloat gst_ahc_parameters_get_vertical_view_angle (GstAHCParameters * self);
gboolean gst_ahc_parameters_get_video_stabilization (GstAHCParameters * self);
const gchar *gst_ahc_parameters_get_white_balance (GstAHCParameters * self);
gint gst_ahc_parameters_get_zoom (GstAHCParameters * self);
/* GList <int> */
GList *gst_ahc_parameters_get_zoom_ratios (GstAHCParameters * self);
void gst_ahc_parameters_zoom_ratios_free (GList * list);
gboolean gst_ahc_parameters_is_smooth_zoom_supported (GstAHCParameters * self);
gboolean gst_ahc_parameters_is_video_stabilization_supported (
    GstAHCParameters * self);
gboolean gst_ahc_parameters_is_zoom_supported (GstAHCParameters * self);
gboolean gst_ahc_parameters_set_antibanding (GstAHCParameters * self,
    const gchar * antibanding);
gboolean gst_ahc_parameters_set_color_effect (GstAHCParameters * self,
    const gchar * value);
gboolean gst_ahc_parameters_set_exposure_compensation (GstAHCParameters * self,
    gint value);
gboolean gst_ahc_parameters_set_flash_mode (GstAHCParameters * self,
    const gchar * value);
gboolean gst_ahc_parameters_set_focus_mode (GstAHCParameters * self,
    const gchar * value);
gboolean gst_ahc_parameters_set_preview_format (GstAHCParameters * self,
    gint format);
gboolean gst_ahc_parameters_set_preview_fps_range (GstAHCParameters * self,
    gint min, gint max);
gboolean gst_ahc_parameters_set_preview_size (GstAHCParameters * self,
    gint width, gint height);
gboolean gst_ahc_parameters_set_scene_mode (GstAHCParameters * self,
    const gchar * value);
gboolean gst_ahc_parameters_set_white_balance (GstAHCParameters * self,
    const gchar * value);
gboolean gst_ahc_parameters_set_video_stabilization (GstAHCParameters * self,
    gboolean toggle);
gboolean gst_ahc_parameters_set_zoom (GstAHCParameters * self, gint value);
gboolean gst_ahc_parameters_unflatten (GstAHCParameters * self,
    const gchar * flattened);
void gst_ahc_parameters_free (GstAHCParameters * self);

G_END_DECLS

#endif /* __GST_ANDROID_HARDWARE_CAMERA_H__ */
