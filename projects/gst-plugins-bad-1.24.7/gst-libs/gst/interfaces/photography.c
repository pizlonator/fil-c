/* GStreamer
 *
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
 *
 * photography.c: photography interface for digital imaging
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "photography.h"

/**
 * SECTION:gstphotography
 * @short_description: Interface for digital image capture elements
 * @stability: Unstable
 *
 * The interface allows access to some common digital image capture parameters.
 *
 * > The GstPhotography interface is unstable API and may change in future.
 * > One can define GST_USE_UNSTABLE_API to acknowledge and avoid this warning.
 */

static void gst_photography_iface_base_init (GstPhotographyInterface * iface);
static void gst_photography_iface_class_init (gpointer g_class);

GType
gst_photography_get_type (void)
{
  static GType gst_photography_type = 0;

  if (!gst_photography_type) {
    static const GTypeInfo gst_photography_info = {
      sizeof (GstPhotographyInterface),
      (GBaseInitFunc) gst_photography_iface_base_init,  /* base_init */
      NULL,                     /* base_finalize */
      (GClassInitFunc) gst_photography_iface_class_init,        /* class_init */
      NULL,                     /* class_finalize */
      NULL,                     /* class_data */
      0,
      0,                        /* n_preallocs */
      NULL,                     /* instance_init */
    };

    gst_photography_type = g_type_register_static (G_TYPE_INTERFACE,
        "GstPhotography", &gst_photography_info, 0);
  }

  return gst_photography_type;
}

static void
gst_photography_iface_base_init (GstPhotographyInterface * iface)
{
  /* default virtual functions */
  iface->get_ev_compensation = NULL;
  iface->get_iso_speed = NULL;
  iface->get_aperture = NULL;
  iface->get_exposure = NULL;
  iface->get_white_balance_mode = NULL;
  iface->get_color_tone_mode = NULL;
  iface->get_scene_mode = NULL;
  iface->get_flash_mode = NULL;
  iface->get_noise_reduction = NULL;
  iface->get_zoom = NULL;
  iface->get_flicker_mode = NULL;
  iface->get_focus_mode = NULL;

  iface->set_ev_compensation = NULL;
  iface->set_iso_speed = NULL;
  iface->set_aperture = NULL;
  iface->set_exposure = NULL;
  iface->set_white_balance_mode = NULL;
  iface->set_color_tone_mode = NULL;
  iface->set_scene_mode = NULL;
  iface->set_flash_mode = NULL;
  iface->set_noise_reduction = NULL;
  iface->set_zoom = NULL;
  iface->set_flicker_mode = NULL;
  iface->set_focus_mode = NULL;

  iface->get_capabilities = NULL;
  iface->prepare_for_capture = NULL;
  iface->set_autofocus = NULL;
  iface->set_config = NULL;
  iface->get_config = NULL;
  iface->set_exposure_mode = NULL;
  iface->get_exposure_mode = NULL;
  iface->set_analog_gain = NULL;
  iface->get_analog_gain = NULL;
  iface->set_lens_focus = NULL;
  iface->get_lens_focus = NULL;
  iface->set_color_temperature = NULL;
  iface->get_color_temperature = NULL;
  iface->set_min_exposure_time = NULL;
  iface->get_min_exposure_time = NULL;
  iface->set_max_exposure_time = NULL;
  iface->get_max_exposure_time = NULL;
}

#define GST_PHOTOGRAPHY_FUNC_TEMPLATE(function_name, param_type) \
gboolean \
gst_photography_set_ ## function_name (GstPhotography * photo, param_type param) \
{ \
  GstPhotographyInterface *iface; \
  g_return_val_if_fail (photo != NULL, FALSE); \
  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo); \
  if (iface->set_ ## function_name) { \
    return iface->set_ ## function_name (photo, param); \
  } \
  return FALSE; \
} \
gboolean \
gst_photography_get_ ## function_name (GstPhotography * photo, param_type * param) \
{ \
  GstPhotographyInterface *iface; \
  g_return_val_if_fail (photo != NULL, FALSE); \
  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo); \
  if (iface->get_ ## function_name) { \
    return iface->get_ ## function_name (photo, param); \
  } \
  return FALSE; \
}


/**
 * gst_photography_set_ev_compensation:
 * @photo: #GstPhotography interface of a #GstElement
 * @ev_comp: ev compensation value to set
 *
 * Set the ev compensation value for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_ev_compensation:
 * @photo: #GstPhotography interface of a #GstElement
 * @ev_comp: ev compensation value to get
 *
 * Get the ev compensation value for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (ev_compensation, gfloat);

/**
 * gst_photography_set_iso_speed:
 * @photo: #GstPhotography interface of a #GstElement
 * @iso_speed: ISO speed value to set
 *
 * Set the ISO value (light sensivity) for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_iso_speed:
 * @photo: #GstPhotography interface of a #GstElement
 * @iso_speed: ISO speed value to get
 *
 * Get the ISO value (light sensivity) for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (iso_speed, guint);

/**
 * gst_photography_set_aperture:
 * @photo: #GstPhotography interface of a #GstElement
 * @aperture: aperture value to set
 *
 * Set the aperture value for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_aperture:
 * @photo: #GstPhotography interface of a #GstElement
 * @aperture: aperture value to get
 *
 * Get the aperture value for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (aperture, guint);

/**
 * gst_photography_set_exposure:
 * @photo: #GstPhotography interface of a #GstElement
 * @exposure: exposure time to set
 *
 * Set the fixed exposure time (in us) for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_exposure:
 * @photo: #GstPhotography interface of a #GstElement
 * @exposure: exposure time to get
 *
 * Get the fixed exposure time (in us) for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (exposure, guint32);

/**
 * gst_photography_set_white_balance_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @wb_mode: #GstPhotographyWhiteBalanceMode to set
 *
 * Set the white balance mode for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_white_balance_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @wb_mode: #GstPhotographyWhiteBalanceMode to get
 *
 * Get the white balance mode for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (white_balance_mode,
    GstPhotographyWhiteBalanceMode);

/**
 * gst_photography_set_color_tone_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @tone_mode: #GstPhotographyColorToneMode to set
 *
 * Set the color tone mode for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_color_tone_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @tone_mode: #GstPhotographyColorToneMode to get
 *
 * Get the color tone mode for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (color_tone_mode, GstPhotographyColorToneMode);

/**
 * gst_photography_set_scene_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @scene_mode: #GstPhotographySceneMode to set
 *
 * Set the scene mode for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_scene_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @scene_mode: #GstPhotographySceneMode to get
 *
 * Get the scene mode for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (scene_mode, GstPhotographySceneMode);

/**
 * gst_photography_set_flash_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @flash_mode: #GstPhotographyFlashMode to set
 *
 * Set the flash mode for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_flash_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @flash_mode: #GstPhotographyFlashMode to get
 *
 * Get the flash mode for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (flash_mode, GstPhotographyFlashMode);

/**
 * gst_photography_set_noise_reduction:
 * @photo: #GstPhotography interface of a #GstElement
 * @noise_reduction: #GstPhotographyNoiseReductionMode to set
 *
 * Set the noise reduction mode for the #GstElement
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_noise_reduction:
 * @photo: #GstPhotography interface of a #GstElement
 * @noise_reduction: #GstPhotographyNoiseReductionMode to get
 *
 * Get the noise reduction mode for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (noise_reduction, GstPhotographyNoiseReduction);

/**
 * gst_photography_set_zoom:
 * @photo: #GstPhotography interface of a #GstElement
 * @zoom: zoom value to set
 *
 * Set the zoom value for the #GstElement.
 * E.g. 1.0 to get original image and 3.0 for 3x zoom and so on.
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_zoom:
 * @photo: #GstPhotography interface of a #GstElement
 * @zoom: zoom value to get
 *
 * Get the zoom value for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (zoom, gfloat);

/**
 * gst_photography_set_flicker_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @flicker_mode: flicker mode value to set
 *
 * Set the flicker mode value for the #GstElement.
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_flicker_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @flicker_mode: flicker mode value to get
 *
 * Get the flicker mode value for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (flicker_mode,
    GstPhotographyFlickerReductionMode);

/**
 * gst_photography_set_focus_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @focus_mode: focus mode value to set
 *
 * Set the focus mode value for the #GstElement.
 *
 * Returns: %TRUE if setting the value succeeded, %FALSE otherwise
 */
/**
 * gst_photography_get_focus_mode:
 * @photo: #GstPhotography interface of a #GstElement
 * @focus_mode: focus_mode value to get
 *
 * Get the focus mode value for the #GstElement
 *
 * Returns: %TRUE if getting the value succeeded, %FALSE otherwise
 */
GST_PHOTOGRAPHY_FUNC_TEMPLATE (focus_mode, GstPhotographyFocusMode);

/**
 * gst_photography_get_capabilities:
 * @photo: #GstPhotography interface of a #GstElement
 *
 * Get #GstPhotographyCaps bitmask value that indicates what photography
 * interface features the #GstElement supports
 *
 * Returns: #GstPhotographyCaps value
 */
GstPhotographyCaps
gst_photography_get_capabilities (GstPhotography * photo)
{
  GstPhotographyInterface *iface;
  g_return_val_if_fail (photo != NULL, GST_PHOTOGRAPHY_CAPS_NONE);

  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo);
  if (iface->get_capabilities) {
    return iface->get_capabilities (photo);
  } else {
    return GST_PHOTOGRAPHY_CAPS_NONE;
  }
}

/**
 * gst_photography_prepare_for_capture:
 * @photo: #GstPhotography interface of a #GstElement
 * @func: callback that is called after capturing has been prepared
 * @capture_caps: #GstCaps defining the desired format of the captured image
 * @user_data: user data that will be passed to the callback @func
 *
 * Start preparations for capture. Preparations can take indeterminate
 * amount of time and @func callback is called after preparations are
 * done. Image capture will begin after callback returns.
 *
 * Returns: %TRUE if preparations were started (caps were OK), otherwise %FALSE.
 */
gboolean
gst_photography_prepare_for_capture (GstPhotography * photo,
    GstPhotographyCapturePrepared func, GstCaps * capture_caps,
    gpointer user_data)
{
  GstPhotographyInterface *iface;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo);
  if (iface->prepare_for_capture) {
    ret = iface->prepare_for_capture (photo, func, capture_caps, user_data);
  }

  return ret;
}

/**
 * gst_photography_set_autofocus:
 * @photo: #GstPhotography interface of a #GstElement
 * @on: %TRUE to start autofocusing, %FALSE to stop autofocusing
 *
 * Start or stop autofocusing. %GST_PHOTOGRAPHY_AUTOFOCUS_DONE
 * message is posted to bus when autofocusing has finished.
 */
void
gst_photography_set_autofocus (GstPhotography * photo, gboolean on)
{
  GstPhotographyInterface *iface;
  g_return_if_fail (photo != NULL);

  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo);
  if (iface->set_autofocus) {
    iface->set_autofocus (photo, on);
  }
}

/**
 * gst_photography_set_config:
 * @photo: #GstPhotography interface of a #GstElement
 * @config: #GstPhotographySettings containing the configuration
 *
 * Set all configuration settings at once.
 *
 * Returns: TRUE if configuration was set successfully, otherwise FALSE.
 */
gboolean
gst_photography_set_config (GstPhotography * photo,
    GstPhotographySettings * config)
{
  GstPhotographyInterface *iface;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo);
  if (iface->set_config) {
    ret = iface->set_config (photo, config);
  }

  return ret;
}

/**
 * gst_photography_get_config:
 * @photo: #GstPhotography interface of a #GstElement
 * @config: #GstPhotographySettings containing the configuration
 *
 * Get all configuration settings at once.
 *
 * Returns: TRUE if configuration was got successfully, otherwise FALSE.
 */
gboolean
gst_photography_get_config (GstPhotography * photo,
    GstPhotographySettings * config)
{
  GstPhotographyInterface *iface;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  iface = GST_PHOTOGRAPHY_GET_INTERFACE (photo);
  if (iface->get_config) {
    ret = iface->get_config (photo, config);
  }

  return ret;
}

/* Photography class initialization stuff */
static void
gst_photography_iface_class_init (gpointer g_class)
{
  /* create interface signals and properties here. */

  /* White balance */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_WB_MODE,
          "White balance mode property",
          "White balance affects the color temperature of the photo",
          GST_TYPE_PHOTOGRAPHY_WHITE_BALANCE_MODE,
          GST_PHOTOGRAPHY_WB_MODE_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Color tone */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_COLOR_TONE,
          "Color tone mode property",
          "Color tone setting changes color shading in the photo",
          GST_TYPE_PHOTOGRAPHY_COLOR_TONE_MODE,
          GST_PHOTOGRAPHY_COLOR_TONE_MODE_NORMAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Scene mode */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_SCENE_MODE,
          "Scene mode property",
          "Scene mode works as a preset for different photo shooting mode settings",
          GST_TYPE_PHOTOGRAPHY_SCENE_MODE,
          GST_PHOTOGRAPHY_SCENE_MODE_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Flash mode */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_FLASH_MODE,
          "Flash mode property",
          "Flash mode defines how the flash light should be used",
          GST_TYPE_PHOTOGRAPHY_FLASH_MODE,
          GST_PHOTOGRAPHY_FLASH_MODE_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Flicker reduction mode */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_FLICKER_MODE,
          "Flicker reduction mode property",
          "Flicker reduction mode defines a line frequency for flickering prevention",
          GST_TYPE_PHOTOGRAPHY_FLICKER_REDUCTION_MODE,
          GST_PHOTOGRAPHY_FLICKER_REDUCTION_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Focus mode */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_FOCUS_MODE,
          "Focus mode property",
          "Focus mode defines the range of focal lengths to use in autofocus search",
          GST_TYPE_PHOTOGRAPHY_FOCUS_MODE,
          GST_PHOTOGRAPHY_FOCUS_MODE_AUTO,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Capabilities */
  g_object_interface_install_property (g_class,
      g_param_spec_ulong (GST_PHOTOGRAPHY_PROP_CAPABILITIES,
          "Photo capabilities bitmask",
          "Tells the photo capabilities of the device",
          0, G_MAXULONG, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* EV_compensation */
  g_object_interface_install_property (g_class,
      g_param_spec_float (GST_PHOTOGRAPHY_PROP_EV_COMP,
          "EV compensation property",
          "EV compensation affects the brightness of the image",
          -2.5, 2.5, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* ISO value */
  g_object_interface_install_property (g_class,
      g_param_spec_uint (GST_PHOTOGRAPHY_PROP_ISO_SPEED,
          "ISO speed property",
          "ISO speed defines the light sensitivity (0 = auto)",
          0, 6400, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Aperture */
  g_object_interface_install_property (g_class,
      g_param_spec_uint (GST_PHOTOGRAPHY_PROP_APERTURE,
          "Aperture property",
          "Aperture defines the size of lens opening  (0 = auto)",
          0, G_MAXUINT8, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Exposure */
  g_object_interface_install_property (g_class,
      g_param_spec_uint (GST_PHOTOGRAPHY_PROP_EXPOSURE_TIME,
          "Exposure time in milliseconds",
          "Exposure time defines how long the shutter will stay open (0 = auto)",
          0, G_MAXUINT32, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:image-capture-supported-caps:
   *
   * Query caps that describe supported formats for image capture. Sometimes
   * element may support different formats for image capture than for video
   * streaming.
   */
  g_object_interface_install_property (g_class,
      g_param_spec_boxed (GST_PHOTOGRAPHY_PROP_IMAGE_CAPTURE_SUPPORTED_CAPS,
          "Image capture supported caps",
          "Caps describing supported image capture formats", GST_TYPE_CAPS,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:image-preview-supported-caps:
   *
   * Query caps that describe supported formats for preview image. Sometimes
   * element may support different formats for preview image than for video
   * streaming.
   */
  g_object_interface_install_property (g_class,
      g_param_spec_boxed (GST_PHOTOGRAPHY_PROP_IMAGE_PREVIEW_SUPPORTED_CAPS,
          "Image preview supported caps",
          "Caps describing supported image preview formats", GST_TYPE_CAPS,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /* Zoom */
  g_object_interface_install_property (g_class,
      g_param_spec_float (GST_PHOTOGRAPHY_PROP_ZOOM,
          "Zoom property",
          "How much the resulted image will be zoomed",
          1.0f, 10.0f, 1.0f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:color-temperature:
   *
   * Color temperature parameter for manual white balance.
   * Control color temperature in Kelvin units.
   */
  g_object_interface_install_property (g_class,
      g_param_spec_uint (GST_PHOTOGRAPHY_PROP_COLOR_TEMPERATURE,
          "Color temperature in Kelvin units",
          "Color temperature in Kelvin units for manual white balance",
          0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:white-point:
   *
   * White point parameter for manual white balance.
   * Describes the color "white" as raw values.
   *
   * FIXME: check and document correct representation for white point
   */
  g_object_interface_install_property (g_class,
      g_param_spec_value_array (GST_PHOTOGRAPHY_PROP_WHITE_POINT,
          "White point",
          "Describe color white as raw values",
          g_param_spec_uint ("raw-value", "Raw value",
              "Raw value", 0, G_MAXUINT, 0,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:analog-gain:
   *
   * Linear multiplicative value how much amplification is applied to the signal
   * before A-D conversion.
   */
  g_object_interface_install_property (g_class,
      g_param_spec_float (GST_PHOTOGRAPHY_PROP_ANALOG_GAIN,
          "Analog gain applied to the sensor",
          "Analog gain applied to the sensor",
          1.0f, G_MAXFLOAT, 1.0f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:lens-focus:
   *
   * Manual changing of lens focus in diopter units.
   * Intended use with GST_PHOTOGRAPHY_FOCUS_MODE_MANUAL focus mode, otherwise
   * to be ignored.
   *
   */
  g_object_interface_install_property (g_class,
      g_param_spec_float (GST_PHOTOGRAPHY_PROP_LENS_FOCUS,
          "Manual lens focus",
          "Focus point in diopter units",
          0.0f, G_MAXFLOAT, 0.0f, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:min-exposure-time:
   *
   * Minimum exposure time for automatic exposure mode.
   */
  g_object_interface_install_property (g_class,
      g_param_spec_uint (GST_PHOTOGRAPHY_PROP_MIN_EXPOSURE_TIME,
          "Minimum exposure time",
          "Minimum exposure time for automatic exposure mode",
          0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstPhotography:max-exposure-time:
   *
   * Maximum exposure time for automatic exposure mode.
   */
  g_object_interface_install_property (g_class,
      g_param_spec_uint (GST_PHOTOGRAPHY_PROP_MAX_EXPOSURE_TIME,
          "Maximum exposure time",
          "Maximum exposure time for automatic exposure mode",
          0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Noise Reduction, Bayer an YCC noise reduction are enabled by default */
  g_object_interface_install_property (g_class,
      g_param_spec_flags (GST_PHOTOGRAPHY_PROP_NOISE_REDUCTION,
          "Noise Reduction settings",
          "Which noise reduction modes are enabled (0 = disabled)",
          GST_TYPE_PHOTOGRAPHY_NOISE_REDUCTION,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Exposure mode */
  g_object_interface_install_property (g_class,
      g_param_spec_enum (GST_PHOTOGRAPHY_PROP_EXPOSURE_MODE,
          "Exposure mode property",
          "Exposure mode to either automatic or manual",
          GST_TYPE_PHOTOGRAPHY_EXPOSURE_MODE,
          0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}
