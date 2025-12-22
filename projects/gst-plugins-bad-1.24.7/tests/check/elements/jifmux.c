/* GStreamer
 *
 * unit test for jifmux
 *
 * Copyright (C) <2010> Thiago Santos <thiago.sousa.santos@collabora.co.uk>
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
#  include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include <gst/check/gstcheck.h>
#include <gst/tag/tag.h>

#include <libexif/exif-data.h>
#include <libexif/exif-loader.h>

typedef struct
{
  gboolean result;
  const GstTagList *taglist;
  gint map_index;
} ExifTagCheckData;

/* taken from the exif helper lib in -base */
/* Exif tag types */
#define EXIF_TYPE_BYTE       1
#define EXIF_TYPE_ASCII      2
#define EXIF_TYPE_SHORT      3
#define EXIF_TYPE_LONG       4
#define EXIF_TYPE_RATIONAL   5
#define EXIF_TYPE_UNDEFINED  7
#define EXIF_TYPE_SLONG      9
#define EXIF_TYPE_SRATIONAL 10

typedef struct _GstExifTagMatch GstExifTagMatch;
typedef void (*CompareFunc) (ExifEntry * entry, ExifTagCheckData * testdata);

struct _GstExifTagMatch
{
  const gchar *gst_tag;
  guint16 exif_tag;
  guint16 exif_type;
  CompareFunc compare_func;
};

/* compare funcs */

/* Copied over from gst-libs/gst/tag/gsttagedittingprivate.c from -base */
static gint
__exif_tag_image_orientation_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "rotate-0") == 0)
    return 1;
  else if (strcmp (str, "flip-rotate-0") == 0)
    return 2;
  else if (strcmp (str, "rotate-180") == 0)
    return 3;
  else if (strcmp (str, "flip-rotate-180") == 0)
    return 4;
  else if (strcmp (str, "flip-rotate-270") == 0)
    return 5;
  else if (strcmp (str, "rotate-90") == 0)
    return 6;
  else if (strcmp (str, "flip-rotate-90") == 0)
    return 7;
  else if (strcmp (str, "rotate-270") == 0)
    return 8;

end:
  GST_WARNING ("Invalid image orientation tag: %s", str);
  return -1;
}

static gint
__exif_tag_capture_exposure_program_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "undefined") == 0)
    return 0;
  else if (strcmp (str, "manual") == 0)
    return 1;
  else if (strcmp (str, "normal") == 0)
    return 2;
  else if (strcmp (str, "aperture-priority") == 0)
    return 3;
  else if (strcmp (str, "shutter-priority") == 0)
    return 4;
  else if (strcmp (str, "creative") == 0)
    return 5;
  else if (strcmp (str, "action") == 0)
    return 6;
  else if (strcmp (str, "portrait") == 0)
    return 7;
  else if (strcmp (str, "landscape") == 0)
    return 8;

end:
  GST_WARNING ("Invalid capture exposure program tag: %s", str);
  return -1;
}

static gint
__exif_tag_capture_exposure_mode_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "auto-exposure") == 0)
    return 0;
  else if (strcmp (str, "manual-exposure") == 0)
    return 1;
  else if (strcmp (str, "auto-bracket") == 0)
    return 2;

end:
  GST_WARNING ("Invalid capture exposure mode tag: %s", str);
  return -1;
}

static gint
__exif_tag_capture_scene_capture_type_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "standard") == 0)
    return 0;
  else if (strcmp (str, "landscape") == 0)
    return 1;
  else if (strcmp (str, "portrait") == 0)
    return 2;
  else if (strcmp (str, "night-scene") == 0)
    return 3;

end:
  GST_WARNING ("Invalid capture scene capture type: %s", str);
  return -1;
}

static gint
__exif_tag_capture_gain_adjustment_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "none") == 0)
    return 0;
  else if (strcmp (str, "low-gain-up") == 0)
    return 1;
  else if (strcmp (str, "high-gain-up") == 0)
    return 2;
  else if (strcmp (str, "low-gain-down") == 0)
    return 3;
  else if (strcmp (str, "high-gain-down") == 0)
    return 4;

end:
  GST_WARNING ("Invalid capture gain adjustment type: %s", str);
  return -1;
}

static gint
__exif_tag_capture_white_balance_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "auto") == 0)
    return 0;
  else                          /* everything else is just manual */
    return 1;

end:
  GST_WARNING ("Invalid white balance: %s", str);
  return -1;
}

static gint
__exif_tag_capture_contrast_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "normal") == 0)
    return 0;
  else if (strcmp (str, "soft") == 0)
    return 1;
  else if (strcmp (str, "hard") == 0)
    return 2;

end:
  GST_WARNING ("Invalid contrast type: %s", str);
  return -1;
}

static gint
__exif_tag_capture_sharpness_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "normal") == 0)
    return 0;
  else if (strcmp (str, "soft") == 0)
    return 1;
  else if (strcmp (str, "hard") == 0)
    return 2;

end:
  GST_WARNING ("Invalid sharpness type: %s", str);
  return -1;
}

static gint
__exif_tag_capture_saturation_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "normal") == 0)
    return 0;
  else if (strcmp (str, "low-saturation") == 0)
    return 1;
  else if (strcmp (str, "high-saturation") == 0)
    return 2;

end:
  GST_WARNING ("Invalid saturation type: %s", str);
  return -1;
}

static gint
__exif_tag_capture_metering_mode_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "unknown") == 0)
    return 0;
  else if (strcmp (str, "average") == 0)
    return 1;
  else if (strcmp (str, "center-weighted-average") == 0)
    return 2;
  else if (strcmp (str, "spot") == 0)
    return 3;
  else if (strcmp (str, "multi-spot") == 0)
    return 4;
  else if (strcmp (str, "pattern") == 0)
    return 5;
  else if (strcmp (str, "partial") == 0)
    return 6;
  else if (strcmp (str, "other") == 0)
    return 255;

end:
  GST_WARNING ("Invalid metering mode type: %s", str);
  return -1;
}

static gint
__exif_tag_capture_source_to_exif_value (const gchar * str)
{
  if (str == NULL)
    goto end;

  if (strcmp (str, "dsc") == 0)
    return 3;
  else if (strcmp (str, "other") == 0)
    return 0;
  else if (strcmp (str, "transparent-scanner") == 0)
    return 1;
  else if (strcmp (str, "reflex-scanner") == 0)
    return 2;

end:
  GST_WARNING ("Invalid capturing source type: %s", str);
  return -1;
}

#define GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC(gst_tag,name)           \
static void                                                                   \
compare_ ## name (ExifEntry * entry, ExifTagCheckData * testdata)             \
{                                                                             \
  gchar *str_tag = NULL;                                                      \
  gint exif_value = -1;                                                       \
  gint value = -1;                                                            \
                                                                              \
  if (!gst_tag_list_get_string_index (testdata->taglist,                      \
          gst_tag, 0, &str_tag)) {                                            \
    /* fail the test if we can't get the tag */                               \
    fail ("Failed to get %s from taglist", gst_tag);                          \
  }                                                                           \
                                                                              \
  value = __exif_tag_ ## name ## _to_exif_value (str_tag);                    \
                                                                              \
  if (value == -1) {                                                          \
    fail ("Invalid %s tag value: %s", gst_tag, str_tag);                      \
  }                                                                           \
                                                                              \
  if (entry->format == EXIF_TYPE_SHORT)                                       \
    exif_value = (gint) exif_get_short (entry->data,                          \
        exif_data_get_byte_order (entry->parent->parent));                    \
  else if (entry->format == EXIF_TYPE_UNDEFINED)                              \
    exif_value = (gint) entry->data[0];                                       \
                                                                              \
  if (value != exif_value) {                                                  \
    fail ("Gstreamer tag value (%d) is different from libexif (%d)",          \
        value, exif_value);                                                   \
  }                                                                           \
                                                                              \
  testdata->result = TRUE;                                                    \
                                                                              \
  g_free (str_tag);                                                           \
}

GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC (GST_TAG_IMAGE_ORIENTATION,
    image_orientation);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC
    (GST_TAG_CAPTURING_EXPOSURE_PROGRAM, capture_exposure_program);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC (GST_TAG_CAPTURING_EXPOSURE_MODE,
    capture_exposure_mode);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC (GST_TAG_CAPTURING_WHITE_BALANCE,
    capture_white_balance);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC (GST_TAG_CAPTURING_CONTRAST,
    capture_contrast);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC
    (GST_TAG_CAPTURING_GAIN_ADJUSTMENT, capture_gain_adjustment);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC (GST_TAG_CAPTURING_SATURATION,
    capture_saturation);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC
    (GST_TAG_CAPTURING_SHARPNESS, capture_sharpness);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC
    (GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE, capture_scene_capture_type);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC
    (GST_TAG_CAPTURING_METERING_MODE, capture_metering_mode);
GST_COMPARE_GST_STRING_TAG_TO_EXIF_SHORT_FUNC
    (GST_TAG_CAPTURING_SOURCE, capture_source);

static void
compare_date_time (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gint year = 0, month = 1, day = 1, hour = 0, minute = 0, second = 0;
  GstDateTime *exif_datetime;
  GstDateTime *datetime;
  const gchar *str;

  if (!gst_tag_list_get_date_time_index (testdata->taglist, GST_TAG_DATE_TIME,
          0, &datetime)) {
    GST_WARNING ("Failed to get datetime from taglist");
    return;
  }

  str = (gchar *) entry->data;

  sscanf (str, "%04d:%02d:%02d %02d:%02d:%02d", &year, &month, &day,
      &hour, &minute, &second);
  exif_datetime = gst_date_time_new_local_time (year, month, day, hour, minute,
      second);
  fail_if (exif_datetime == NULL);

  fail_unless (gst_date_time_get_year (datetime) ==
      gst_date_time_get_year (exif_datetime)
      && gst_date_time_get_month (datetime) ==
      gst_date_time_get_month (exif_datetime)
      && gst_date_time_get_day (datetime) ==
      gst_date_time_get_day (exif_datetime)
      && gst_date_time_get_hour (datetime) ==
      gst_date_time_get_hour (exif_datetime)
      && gst_date_time_get_minute (datetime) ==
      gst_date_time_get_minute (exif_datetime)
      && gst_date_time_get_second (datetime) ==
      gst_date_time_get_second (exif_datetime));

  gst_date_time_unref (exif_datetime);
  gst_date_time_unref (datetime);

  testdata->result = TRUE;
}

static void
compare_shutter_speed (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble gst_num, exif_num;
  ExifSRational rational;
  GValue exif_value = { 0 };
  const GValue *gst_value = NULL;

  gst_value = gst_tag_list_get_value_index (testdata->taglist,
      GST_TAG_CAPTURING_SHUTTER_SPEED, 0);
  if (gst_value == NULL) {
    GST_WARNING ("Failed to get shutter-speed from taglist");
    return;
  }

  rational = exif_get_srational (entry->data,
      exif_data_get_byte_order (entry->parent->parent));

  g_value_init (&exif_value, GST_TYPE_FRACTION);
  gst_value_set_fraction (&exif_value, rational.numerator,
      rational.denominator);
  gst_util_fraction_to_double (gst_value_get_fraction_numerator (&exif_value),
      gst_value_get_fraction_denominator (&exif_value), &exif_num);
  g_value_unset (&exif_value);

  gst_util_fraction_to_double (gst_value_get_fraction_numerator (gst_value),
      gst_value_get_fraction_denominator (gst_value), &gst_num);

  exif_num = pow (2, -exif_num);

  GST_LOG ("Shutter speed in gst=%lf and in exif=%lf", gst_num, exif_num);
  fail_unless (ABS (gst_num - exif_num) < 0.001);
  testdata->result = TRUE;
}

static void
compare_aperture_value (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble gst_value, exif_value;
  ExifSRational rational;
  GValue value = { 0 };

  if (!gst_tag_list_get_double_index (testdata->taglist,
          GST_TAG_CAPTURING_FOCAL_RATIO, 0, &gst_value)) {
    GST_WARNING ("Failed to get focal ratio from taglist");
    return;
  }

  rational = exif_get_srational (entry->data,
      exif_data_get_byte_order (entry->parent->parent));

  g_value_init (&value, GST_TYPE_FRACTION);
  gst_value_set_fraction (&value, rational.numerator, rational.denominator);
  gst_util_fraction_to_double (gst_value_get_fraction_numerator (&value),
      gst_value_get_fraction_denominator (&value), &exif_value);
  g_value_unset (&value);

  exif_value = pow (2, exif_value / 2);

  GST_LOG ("Aperture value in gst=%lf and in exif=%lf", gst_value, exif_value);
  fail_unless (ABS (gst_value - exif_value) < 0.001);
  testdata->result = TRUE;
}

static void
compare_flash (ExifEntry * entry, ExifTagCheckData * testdata)
{
  guint16 flags;
  gboolean flash_fired;
  const gchar *flash_mode;

  flags = (gint) exif_get_short (entry->data,
      exif_data_get_byte_order (entry->parent->parent));

  if (!gst_tag_list_get_boolean_index (testdata->taglist,
          GST_TAG_CAPTURING_FLASH_FIRED, 0, &flash_fired)) {
    GST_WARNING ("Failed to get %s tag", GST_TAG_CAPTURING_FLASH_FIRED);
    return;
  }
  if (!gst_tag_list_peek_string_index (testdata->taglist,
          GST_TAG_CAPTURING_FLASH_MODE, 0, &flash_mode)) {
    GST_WARNING ("Failed to get %s tag", GST_TAG_CAPTURING_FLASH_MODE);
    return;
  }

  if (flash_fired)
    fail_unless ((flags & 1) == 1);
  else
    fail_unless ((flags & 1) == 0);

  if (strcmp (flash_mode, "auto") == 0) {
    fail_unless (((flags >> 3) & 0x3) == 3);
  } else if (strcmp (flash_mode, "always") == 0) {
    fail_unless (((flags >> 3) & 0x3) == 1);
  } else if (strcmp (flash_mode, "never") == 0) {
    fail_unless (((flags >> 3) & 0x3) == 2);
  } else {
    fail ("unexpected flash mode");
  }
  testdata->result = TRUE;
}

static void
compare_geo_elevation (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble altitude = 0, gst_value;
  ExifRational rational;

  fail_unless (gst_tag_list_get_double_index (testdata->taglist,
          GST_TAG_GEO_LOCATION_ELEVATION, 0, &gst_value));

  fail_unless (entry->components == 1);

  rational = exif_get_rational (entry->data,
      exif_data_get_byte_order (entry->parent->parent));
  gst_util_fraction_to_double (rational.numerator, rational.denominator,
      &altitude);

  gst_value = ABS (gst_value);
  fail_unless (ABS (gst_value - altitude) < 0.001);
  testdata->result = TRUE;
}

static void
compare_geo_elevation_ref (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble gst_value;

  fail_unless (gst_tag_list_get_double_index (testdata->taglist,
          GST_TAG_GEO_LOCATION_ELEVATION, 0, &gst_value));

  fail_unless (entry->components == 1);

  if (gst_value >= 0) {
    fail_unless (entry->data[0] == 0);
  } else {
    fail_unless (entry->data[0] == 1);
  }
  testdata->result = TRUE;
}

static void
compare_speed (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble speed = 0, gst_value;
  ExifRational rational;

  fail_unless (gst_tag_list_get_double_index (testdata->taglist,
          GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, 0, &gst_value));

  fail_unless (entry->components == 1);

  rational = exif_get_rational (entry->data,
      exif_data_get_byte_order (entry->parent->parent));
  gst_util_fraction_to_double (rational.numerator, rational.denominator,
      &speed);

  speed = speed / 3.6;

  fail_unless (ABS (gst_value - speed) < 0.001);
  testdata->result = TRUE;
}

static void
compare_speed_ref (ExifEntry * entry, ExifTagCheckData * testdata)
{
  fail_unless (entry->components == 2);
  fail_unless (entry->data[0] == 'K');
  testdata->result = TRUE;
}

static void
compare_geo_coordinate (ExifEntry * entry, ExifTagCheckData * testdata);

static void
compare_geo_coordinate_ref (ExifEntry * entry, ExifTagCheckData * testdata);

static void
compare_geo_direction (ExifEntry * entry, ExifTagCheckData * testdata);

static void
compare_geo_direction_ref (ExifEntry * entry, ExifTagCheckData * testdata);

static const GstExifTagMatch tag_map[] = {
  {GST_TAG_DESCRIPTION, EXIF_TAG_IMAGE_DESCRIPTION, EXIF_TYPE_ASCII,
      NULL},
  {GST_TAG_DEVICE_MANUFACTURER, EXIF_TAG_MAKE, EXIF_TYPE_ASCII,
      NULL},
  {GST_TAG_DEVICE_MODEL, EXIF_TAG_MODEL, EXIF_TYPE_ASCII, NULL},
  {GST_TAG_IMAGE_ORIENTATION, EXIF_TAG_ORIENTATION, EXIF_TYPE_SHORT,
      compare_image_orientation},
  {GST_TAG_IMAGE_HORIZONTAL_PPI, EXIF_TAG_X_RESOLUTION, EXIF_TYPE_RATIONAL,
      NULL},
  {GST_TAG_IMAGE_VERTICAL_PPI, EXIF_TAG_Y_RESOLUTION, EXIF_TYPE_RATIONAL, NULL},
  {GST_TAG_APPLICATION_NAME, EXIF_TAG_SOFTWARE, EXIF_TYPE_ASCII,
      NULL},
  {GST_TAG_DATE_TIME, EXIF_TAG_DATE_TIME, EXIF_TYPE_ASCII,
      compare_date_time},
  {GST_TAG_ARTIST, EXIF_TAG_ARTIST, EXIF_TYPE_ASCII, NULL},
  {GST_TAG_COPYRIGHT, EXIF_TAG_COPYRIGHT, EXIF_TYPE_ASCII, NULL},
  {GST_TAG_CAPTURING_SHUTTER_SPEED, EXIF_TAG_EXPOSURE_TIME,
      EXIF_TYPE_RATIONAL, NULL},
  {GST_TAG_CAPTURING_FOCAL_RATIO, EXIF_TAG_FNUMBER, EXIF_TYPE_RATIONAL,
      NULL},
  {GST_TAG_CAPTURING_EXPOSURE_PROGRAM, EXIF_TAG_EXPOSURE_PROGRAM,
      EXIF_TYPE_SHORT, compare_capture_exposure_program},

  /* This is called PhotographicSensitivity in 2.3 */
  {GST_TAG_CAPTURING_ISO_SPEED, EXIF_TAG_ISO_SPEED_RATINGS,
      EXIF_TYPE_SHORT, NULL},

  {GST_TAG_CAPTURING_SHUTTER_SPEED, EXIF_TAG_SHUTTER_SPEED_VALUE,
      EXIF_TYPE_SRATIONAL, compare_shutter_speed},
  {GST_TAG_CAPTURING_FOCAL_RATIO, EXIF_TAG_APERTURE_VALUE, EXIF_TYPE_RATIONAL,
      compare_aperture_value},
  {GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, EXIF_TAG_EXPOSURE_BIAS_VALUE,
      EXIF_TYPE_SRATIONAL},
  {GST_TAG_CAPTURING_FLASH_FIRED, EXIF_TAG_FLASH, EXIF_TYPE_SHORT,
      compare_flash},
  {GST_TAG_CAPTURING_FLASH_MODE, EXIF_TAG_FLASH, EXIF_TYPE_SHORT,
      compare_flash},
  {GST_TAG_CAPTURING_FOCAL_LENGTH, EXIF_TAG_FOCAL_LENGTH, EXIF_TYPE_RATIONAL,
      NULL},
  {GST_TAG_APPLICATION_DATA, EXIF_TAG_MAKER_NOTE, EXIF_TYPE_UNDEFINED, NULL},
  {GST_TAG_CAPTURING_EXPOSURE_MODE, EXIF_TAG_EXPOSURE_MODE, EXIF_TYPE_SHORT,
      compare_capture_exposure_mode},
  {GST_TAG_CAPTURING_WHITE_BALANCE, EXIF_TAG_WHITE_BALANCE, EXIF_TYPE_SHORT,
      compare_capture_white_balance},
  {GST_TAG_CAPTURING_DIGITAL_ZOOM_RATIO, EXIF_TAG_DIGITAL_ZOOM_RATIO,
      EXIF_TYPE_RATIONAL, NULL},
  {GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE, EXIF_TAG_SCENE_CAPTURE_TYPE,
      EXIF_TYPE_SHORT, compare_capture_scene_capture_type},
  {GST_TAG_CAPTURING_GAIN_ADJUSTMENT, EXIF_TAG_GAIN_CONTROL,
      EXIF_TYPE_SHORT, compare_capture_gain_adjustment},
  {GST_TAG_CAPTURING_CONTRAST, EXIF_TAG_CONTRAST, EXIF_TYPE_SHORT,
      compare_capture_contrast},
  {GST_TAG_CAPTURING_SATURATION, EXIF_TAG_SATURATION, EXIF_TYPE_SHORT,
      compare_capture_saturation},
  {GST_TAG_CAPTURING_SHARPNESS, EXIF_TAG_SHARPNESS, EXIF_TYPE_SHORT,
      compare_capture_sharpness},
  {GST_TAG_CAPTURING_METERING_MODE, EXIF_TAG_METERING_MODE, EXIF_TYPE_SHORT,
      compare_capture_metering_mode},
  {GST_TAG_CAPTURING_SOURCE, EXIF_TAG_FILE_SOURCE, EXIF_TYPE_UNDEFINED,
      compare_capture_source},

  /* gps tags */
  {GST_TAG_GEO_LOCATION_LATITUDE, EXIF_TAG_GPS_LATITUDE, EXIF_TYPE_RATIONAL,
      compare_geo_coordinate},
  {GST_TAG_GEO_LOCATION_LATITUDE, EXIF_TAG_GPS_LATITUDE_REF, EXIF_TYPE_ASCII,
      compare_geo_coordinate_ref},
  {GST_TAG_GEO_LOCATION_LONGITUDE, EXIF_TAG_GPS_LONGITUDE, EXIF_TYPE_RATIONAL,
      compare_geo_coordinate},
  {GST_TAG_GEO_LOCATION_LONGITUDE, EXIF_TAG_GPS_LONGITUDE_REF, EXIF_TYPE_ASCII,
      compare_geo_coordinate_ref},
  {GST_TAG_GEO_LOCATION_ELEVATION, EXIF_TAG_GPS_ALTITUDE, EXIF_TYPE_RATIONAL,
      compare_geo_elevation},
  {GST_TAG_GEO_LOCATION_ELEVATION, EXIF_TAG_GPS_ALTITUDE_REF, EXIF_TYPE_BYTE,
      compare_geo_elevation_ref},
  {GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, EXIF_TAG_GPS_SPEED, EXIF_TYPE_RATIONAL,
      compare_speed},
  {GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, EXIF_TAG_GPS_SPEED_REF, EXIF_TYPE_ASCII,
      compare_speed_ref},
  {GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, EXIF_TAG_GPS_TRACK,
      EXIF_TYPE_RATIONAL, compare_geo_direction},
  {GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, EXIF_TAG_GPS_TRACK_REF,
      EXIF_TYPE_ASCII, compare_geo_direction_ref},
  {GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, EXIF_TAG_GPS_IMG_DIRECTION,
      EXIF_TYPE_RATIONAL, compare_geo_direction},
  {GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, EXIF_TAG_GPS_IMG_DIRECTION_REF,
      EXIF_TYPE_ASCII, compare_geo_direction_ref}

/*
 * libexif doesn't have these tags
 *  {GST_TAG_CAPTURING_ISO_SPEED, EXIF_TAG_ISO_SPEED, EXIF_TYPE_LONG, NULL},
 *  {GST_TAG_CAPTURING_ISO_SPEED, EXIF_TAG_SENSITIVITY_TYPE,
 *     EXIF_TYPE_SHORT, compare_sensitivity_type},
 */
};

static void
compare_geo_coordinate (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble coordinate = 0, aux, gst_value;
  ExifRational rational;

  fail_unless (gst_tag_list_get_double_index (testdata->taglist,
          tag_map[testdata->map_index].gst_tag, 0, &gst_value));

  fail_unless (entry->components == 3);

  rational = exif_get_rational (entry->data,
      exif_data_get_byte_order (entry->parent->parent));
  gst_util_fraction_to_double (rational.numerator, rational.denominator, &aux);
  coordinate += aux;

  rational = exif_get_rational (entry->data + 8,
      exif_data_get_byte_order (entry->parent->parent));
  gst_util_fraction_to_double (rational.numerator, rational.denominator, &aux);
  coordinate += aux / 60.0;

  rational = exif_get_rational (entry->data + 16,
      exif_data_get_byte_order (entry->parent->parent));
  gst_util_fraction_to_double (rational.numerator, rational.denominator, &aux);
  coordinate += aux / 3600.0;

  gst_value = ABS (gst_value);
  fail_unless (ABS (gst_value - coordinate) < 0.001);
  testdata->result = TRUE;
}

static void
compare_geo_coordinate_ref (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble gst_value;
  const gchar *tag;

  tag = tag_map[testdata->map_index].gst_tag;

  fail_unless (gst_tag_list_get_double_index (testdata->taglist, tag, 0,
          &gst_value));

  fail_unless (entry->components == 2);

  if (strcmp (tag, GST_TAG_GEO_LOCATION_LATITUDE) == 0) {
    if (gst_value >= 0) {
      fail_unless (entry->data[0] == 'N');
    } else {
      fail_unless (entry->data[0] == 'S');
    }
  } else {
    if (gst_value >= 0) {
      fail_unless (entry->data[0] == 'E');
    } else {
      fail_unless (entry->data[0] == 'W');
    }
  }
  testdata->result = TRUE;
}

static void
compare_geo_direction (ExifEntry * entry, ExifTagCheckData * testdata)
{
  gdouble direction = 0, gst_value;
  ExifRational rational;

  fail_unless (gst_tag_list_get_double_index (testdata->taglist,
          tag_map[testdata->map_index].gst_tag, 0, &gst_value));

  fail_unless (entry->components == 1);

  rational = exif_get_rational (entry->data,
      exif_data_get_byte_order (entry->parent->parent));
  gst_util_fraction_to_double (rational.numerator, rational.denominator,
      &direction);

  fail_unless (ABS (gst_value - direction) < 0.001);
  testdata->result = TRUE;
}

static void
compare_geo_direction_ref (ExifEntry * entry, ExifTagCheckData * testdata)
{
  fail_unless (entry->components == 2);
  fail_unless (entry->data[0] == 'T');
  testdata->result = TRUE;
}

static void
check_content (ExifContent * content, void *user_data)
{
  ExifTagCheckData *test_data = (ExifTagCheckData *) user_data;
  guint16 tagindex;
  ExifEntry *entry;
  GType gst_tag_type;

  tagindex = test_data->map_index;
  gst_tag_type = gst_tag_get_type (tag_map[tagindex].gst_tag);

  GST_DEBUG ("Got tagindex %u for gsttag %s with type %s", tagindex,
      tag_map[tagindex].gst_tag, g_type_name (gst_tag_type));

  /* search for the entry */
  entry = exif_content_get_entry (content, tag_map[tagindex].exif_tag);
  GST_DEBUG ("Entry found at %p", entry);
  if (entry == NULL)
    return;

  fail_unless (entry->format == tag_map[tagindex].exif_type);

  if (tag_map[tagindex].compare_func) {
    tag_map[tagindex].compare_func (entry, test_data);
    return;
  }

  switch (entry->format) {
    case EXIF_TYPE_ASCII:{
      const gchar *str;
      gchar *taglist_str;

      str = (gchar *) entry->data;
      fail_unless (gst_tag_list_get_string (test_data->taglist,
              tag_map[tagindex].gst_tag, &taglist_str));

      fail_unless (strcmp (str, taglist_str) == 0);
      test_data->result = TRUE;
      g_free (taglist_str);
    }
      break;
    case EXIF_TYPE_SRATIONAL:
    case EXIF_TYPE_RATIONAL:{
      GValue exif_value = { 0 };

      g_value_init (&exif_value, GST_TYPE_FRACTION);
      if (entry->format == EXIF_TYPE_RATIONAL) {
        ExifRational exif_rational = exif_get_rational (entry->data,
            exif_data_get_byte_order (entry->parent->parent));

        gst_value_set_fraction (&exif_value, exif_rational.numerator,
            exif_rational.denominator);
      } else {
        ExifSRational exif_rational = exif_get_srational (entry->data,
            exif_data_get_byte_order (entry->parent->parent));

        gst_value_set_fraction (&exif_value, exif_rational.numerator,
            exif_rational.denominator);
      }

      if (gst_tag_type == GST_TYPE_FRACTION) {
        const GValue *value = gst_tag_list_get_value_index (test_data->taglist,
            tag_map[tagindex].gst_tag, 0);

        fail_unless (value != NULL);
        fail_unless (G_VALUE_TYPE (value) == GST_TYPE_FRACTION);

        fail_unless (gst_value_get_fraction_numerator (value) ==
            gst_value_get_fraction_numerator (&exif_value) &&
            gst_value_get_fraction_denominator (value) ==
            gst_value_get_fraction_denominator (&exif_value));

        test_data->result = TRUE;
      } else if (gst_tag_type == G_TYPE_DOUBLE) {
        gdouble gst_num;
        gdouble exif_num;

        gst_util_fraction_to_double (gst_value_get_fraction_numerator
            (&exif_value), gst_value_get_fraction_denominator (&exif_value),
            &exif_num);

        fail_unless (gst_tag_list_get_double_index (test_data->taglist,
                tag_map[tagindex].gst_tag, 0, &gst_num));

        fail_unless (gst_num == exif_num);
        test_data->result = TRUE;
      } else {
        GST_WARNING ("Unhandled type for rational tag(%X): %s",
            entry->tag, g_type_name (gst_tag_type));
      }
      g_value_unset (&exif_value);
    }
      break;
    case EXIF_TYPE_SHORT:
    case EXIF_TYPE_LONG:{
      gint gst_num;
      gint exif_num = -1;

      if (entry->format == EXIF_TYPE_LONG) {
        exif_num = (gint) exif_get_long (entry->data,
            exif_data_get_byte_order (entry->parent->parent));
      } else if (entry->format == EXIF_TYPE_SHORT) {
        exif_num = (gint) exif_get_short (entry->data,
            exif_data_get_byte_order (entry->parent->parent));
      }

      fail_unless (gst_tag_list_get_int_index (test_data->taglist,
              tag_map[tagindex].gst_tag, 0, &gst_num));

      fail_unless (exif_num == gst_num);
      test_data->result = TRUE;
    }
      break;
    case EXIF_TYPE_UNDEFINED:{
      GstMapInfo map;
      GstBuffer *buf;
      GstSample *sample;
      gint i;

      if (!gst_tag_list_get_sample_index (test_data->taglist,
              tag_map[tagindex].gst_tag, 0, &sample)) {
        return;
      }
      buf = gst_sample_get_buffer (sample);
      gst_buffer_map (buf, &map, GST_MAP_READ);
      fail_unless (entry->size, map.size);
      for (i = 0; i < map.size; i++) {
        fail_unless (map.data[i] == (guint8) entry->data[i]);
      }
      gst_buffer_unmap (buf, &map);

      test_data->result = TRUE;
      gst_sample_unref (sample);
    }
      break;
    default:
      fail ("unexpected exif type %d", entry->format);
  }
}

/*
 * Iterates over the exif data searching for the mapping pointed by index
 */
static void
libexif_check_tag_exists (const GstTagList * taglist, gint index, gpointer data)
{
  ExifData *exif_data = (ExifData *) data;
  ExifTagCheckData test_data;

  test_data.result = FALSE;
  test_data.taglist = taglist;
  test_data.map_index = index;

  exif_data_foreach_content (exif_data, check_content, &test_data);

  fail_unless (test_data.result);
}

static void
generate_jif_file_with_tags_from_taglist (GstTagList * taglist,
    const gchar * filepath)
{
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;
  gchar *launchline;
  GstElement *jifmux;
  GstTagSetter *setter;

  launchline = g_strdup_printf ("videotestsrc num-buffers=1 ! jpegenc ! "
      "jifmux name=jifmux0 ! filesink location=%s", filepath);

  pipeline = gst_parse_launch (launchline, NULL);
  fail_unless (pipeline != NULL);
  g_free (launchline);

  jifmux = gst_bin_get_by_name (GST_BIN (pipeline), "jifmux0");
  fail_unless (jifmux != NULL);
  setter = GST_TAG_SETTER (jifmux);
  gst_tag_setter_merge_tags (setter, taglist, GST_TAG_MERGE_REPLACE);
  gst_object_unref (jifmux);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  fail_if (gst_element_set_state (pipeline, GST_STATE_PLAYING) ==
      GST_STATE_CHANGE_FAILURE);

  msg = gst_bus_timed_pop_filtered (bus, GST_SECOND * 10, GST_MESSAGE_EOS |
      GST_MESSAGE_ERROR);
  fail_if (!msg);
  fail_if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);

  gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

static void
generate_jif_file_with_tags (const gchar * tags, const gchar * filepath)
{
  GstTagList *taglist;

  taglist = gst_tag_list_new_from_string (tags);
  generate_jif_file_with_tags_from_taglist (taglist, filepath);

  gst_tag_list_unref (taglist);
}

static void
libexif_check_tags_from_taglist (GstTagList * taglist, const gchar * filepath)
{
  ExifData *exif_data;
  gint i;

  fail_unless (taglist != NULL);
  exif_data = exif_data_new_from_file (filepath);

  /* iterate over our tag mapping */
  for (i = 0; i < G_N_ELEMENTS (tag_map); i++) {
    if (gst_tag_list_get_value_index (taglist, tag_map[i].gst_tag, 0)) {
      /* we have added this field to the taglist, check if it was written in
       * exif */
      libexif_check_tag_exists (taglist, i, exif_data);
    }
  }

  exif_data_unref (exif_data);
}

static void
libexif_check_tags (const gchar * tags, const gchar * filepath)
{
  GstTagList *taglist;

  taglist = gst_tag_list_new_from_string (tags);
  fail_unless (taglist != NULL);

  libexif_check_tags_from_taglist (taglist, filepath);

  gst_tag_list_unref (taglist);
}

GST_START_TEST (test_jifmux_tags)
{
  gchar *tmpfile;
  gchar *tmp;
  GstTagList *taglist;
  GstDateTime *datetime;
  GstBuffer *buffer;
  GstSample *sample;
  GstMapInfo map;
  gint i;

  gst_tag_register_musicbrainz_tags ();

  tmp = g_strdup_printf ("%s%d", "gst-check-xmp-test-", g_random_int ());
  tmpfile = g_build_filename (g_get_tmp_dir (), tmp, NULL);
  g_free (tmp);

  datetime = gst_date_time_new_local_time (2000, 10, 5, 8, 45, 13);
  buffer = gst_buffer_new_and_alloc (100);
  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  for (i = 0; i < 100; i++) {
    map.data[i] = i;
  }
  gst_buffer_unmap (buffer, &map);

  sample = gst_sample_new (buffer, NULL, NULL, NULL);
  gst_buffer_unref (buffer);

  taglist = gst_tag_list_new (GST_TAG_ARTIST, "some artist",
      GST_TAG_COPYRIGHT, "My copyright notice",
      GST_TAG_DEVICE_MANUFACTURER, "MyFavoriteBrand",
      GST_TAG_DEVICE_MODEL, "123v42.1",
      GST_TAG_DESCRIPTION, "some description",
      GST_TAG_APPLICATION_NAME, "jifmux-test v1.2b",
      GST_TAG_CAPTURING_SHUTTER_SPEED, 1, 30,
      GST_TAG_CAPTURING_FOCAL_RATIO, 2.0,
      GST_TAG_CAPTURING_ISO_SPEED, 800, GST_TAG_DATE_TIME, datetime,
      GST_TAG_CAPTURING_FOCAL_LENGTH, 22.5,
      GST_TAG_CAPTURING_DIGITAL_ZOOM_RATIO, 5.25,
      GST_TAG_CAPTURING_EXPOSURE_COMPENSATION, -2.5,
      GST_TAG_APPLICATION_DATA, sample,
      GST_TAG_CAPTURING_FLASH_FIRED, TRUE,
      GST_TAG_CAPTURING_FLASH_MODE, "auto",
      GST_TAG_CAPTURING_SOURCE, "dsc",
      GST_TAG_CAPTURING_METERING_MODE, "multi-spot",
      GST_TAG_CAPTURING_SHARPNESS, "normal",
      GST_TAG_CAPTURING_SATURATION, "normal",
      GST_TAG_CAPTURING_CONTRAST, "normal",
      GST_TAG_GEO_LOCATION_LATITUDE, -32.375,
      GST_TAG_GEO_LOCATION_LONGITUDE, 76.0125,
      GST_TAG_GEO_LOCATION_ELEVATION, 300.85,
      GST_TAG_GEO_LOCATION_MOVEMENT_SPEED, 3.6,
      GST_TAG_GEO_LOCATION_MOVEMENT_DIRECTION, 35.4,
      GST_TAG_GEO_LOCATION_CAPTURE_DIRECTION, 12.345,
      GST_TAG_IMAGE_HORIZONTAL_PPI, 300.0,
      GST_TAG_IMAGE_VERTICAL_PPI, 96.0, NULL);
  gst_date_time_unref (datetime);
  gst_sample_unref (sample);
  generate_jif_file_with_tags_from_taglist (taglist, tmpfile);
  libexif_check_tags_from_taglist (taglist, tmpfile);
  gst_tag_list_unref (taglist);

#define IMAGE_ORIENTATION_TAG(t) "taglist," GST_TAG_IMAGE_ORIENTATION "=" t
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG ("rotate-0"), tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("rotate-0"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG ("flip-rotate-0"),
      tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("flip-rotate-0"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG ("rotate-180"), tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("rotate-180"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG ("flip-rotate-180"),
      tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("flip-rotate-180"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG
      ("flip-rotate-270"), tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("flip-rotate-270"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG ("rotate-90"), tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("rotate-90"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG
      ("flip-rotate-90"), tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("flip-rotate-90"), tmpfile);
  generate_jif_file_with_tags (IMAGE_ORIENTATION_TAG ("rotate-270"), tmpfile);
  libexif_check_tags (IMAGE_ORIENTATION_TAG ("rotate-270"), tmpfile);

#define EXPOSURE_PROGRAM_TAG(t) "taglist," GST_TAG_CAPTURING_EXPOSURE_PROGRAM \
    "=" t
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("undefined"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("undefined"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("manual"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("manual"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("normal"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("normal"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("aperture-priority"),
      tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("aperture-priority"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("shutter-priority"),
      tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("shutter-priority"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("creative"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("creative"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("action"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("action"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("portrait"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("portrait"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_PROGRAM_TAG ("landscape"), tmpfile);
  libexif_check_tags (EXPOSURE_PROGRAM_TAG ("landscape"), tmpfile);

#define EXPOSURE_MODE_TAG(t) "taglist," GST_TAG_CAPTURING_EXPOSURE_MODE "=" t
  generate_jif_file_with_tags (EXPOSURE_MODE_TAG ("auto-exposure"), tmpfile);
  libexif_check_tags (EXPOSURE_MODE_TAG ("auto-exposure"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_MODE_TAG ("manual-exposure"), tmpfile);
  libexif_check_tags (EXPOSURE_MODE_TAG ("manual-exposure"), tmpfile);
  generate_jif_file_with_tags (EXPOSURE_MODE_TAG ("auto-bracket"), tmpfile);
  libexif_check_tags (EXPOSURE_MODE_TAG ("auto-bracket"), tmpfile);

#define SCENE_CAPTURE_TYPE_TAG(t) "taglist," GST_TAG_CAPTURING_SCENE_CAPTURE_TYPE\
    "=" t
  generate_jif_file_with_tags (SCENE_CAPTURE_TYPE_TAG ("standard"), tmpfile);
  libexif_check_tags (SCENE_CAPTURE_TYPE_TAG ("standard"), tmpfile);
  generate_jif_file_with_tags (SCENE_CAPTURE_TYPE_TAG ("landscape"), tmpfile);
  libexif_check_tags (SCENE_CAPTURE_TYPE_TAG ("landscape"), tmpfile);
  generate_jif_file_with_tags (SCENE_CAPTURE_TYPE_TAG ("portrait"), tmpfile);
  libexif_check_tags (SCENE_CAPTURE_TYPE_TAG ("portrait"), tmpfile);
  generate_jif_file_with_tags (SCENE_CAPTURE_TYPE_TAG ("night-scene"), tmpfile);
  libexif_check_tags (SCENE_CAPTURE_TYPE_TAG ("night-scene"), tmpfile);

#define WHITE_BALANCE_TAG(t) "taglist," GST_TAG_CAPTURING_WHITE_BALANCE "=" t
  generate_jif_file_with_tags (WHITE_BALANCE_TAG ("auto"), tmpfile);
  libexif_check_tags (WHITE_BALANCE_TAG ("auto"), tmpfile);
  generate_jif_file_with_tags (WHITE_BALANCE_TAG ("manual"), tmpfile);
  libexif_check_tags (WHITE_BALANCE_TAG ("manual"), tmpfile);

#define GAIN_ADJUSTMENT_TAG(t) "taglist," GST_TAG_CAPTURING_GAIN_ADJUSTMENT "=" t
  generate_jif_file_with_tags (GAIN_ADJUSTMENT_TAG ("none"), tmpfile);
  libexif_check_tags (GAIN_ADJUSTMENT_TAG ("none"), tmpfile);
  generate_jif_file_with_tags (GAIN_ADJUSTMENT_TAG ("high-gain-up"), tmpfile);
  libexif_check_tags (GAIN_ADJUSTMENT_TAG ("high-gain-up"), tmpfile);
  generate_jif_file_with_tags (GAIN_ADJUSTMENT_TAG ("low-gain-up"), tmpfile);
  libexif_check_tags (GAIN_ADJUSTMENT_TAG ("low-gain-up"), tmpfile);
  generate_jif_file_with_tags (GAIN_ADJUSTMENT_TAG ("high-gain-down"), tmpfile);
  libexif_check_tags (GAIN_ADJUSTMENT_TAG ("high-gain-down"), tmpfile);
  generate_jif_file_with_tags (GAIN_ADJUSTMENT_TAG ("low-gain-down"), tmpfile);
  libexif_check_tags (GAIN_ADJUSTMENT_TAG ("low-gain-down"), tmpfile);

#define CONTRAST_TAG(t) "taglist," GST_TAG_CAPTURING_CONTRAST "=" t
  generate_jif_file_with_tags (CONTRAST_TAG ("normal"), tmpfile);
  libexif_check_tags (CONTRAST_TAG ("normal"), tmpfile);
  generate_jif_file_with_tags (CONTRAST_TAG ("soft"), tmpfile);
  libexif_check_tags (CONTRAST_TAG ("soft"), tmpfile);
  generate_jif_file_with_tags (CONTRAST_TAG ("hard"), tmpfile);
  libexif_check_tags (CONTRAST_TAG ("hard"), tmpfile);

#define SATURATION_TAG(t) "taglist," GST_TAG_CAPTURING_SATURATION "=" t
  generate_jif_file_with_tags (SATURATION_TAG ("normal"), tmpfile);
  libexif_check_tags (SATURATION_TAG ("normal"), tmpfile);
  generate_jif_file_with_tags (SATURATION_TAG ("low-saturation"), tmpfile);
  libexif_check_tags (SATURATION_TAG ("low-saturation"), tmpfile);
  generate_jif_file_with_tags (SATURATION_TAG ("high-saturation"), tmpfile);
  libexif_check_tags (SATURATION_TAG ("high-saturation"), tmpfile);

#define SHARPNESS_TAG(t) "taglist," GST_TAG_CAPTURING_SHARPNESS "=" t
  generate_jif_file_with_tags (SHARPNESS_TAG ("normal"), tmpfile);
  libexif_check_tags (SHARPNESS_TAG ("normal"), tmpfile);
  generate_jif_file_with_tags (SHARPNESS_TAG ("soft"), tmpfile);
  libexif_check_tags (SHARPNESS_TAG ("soft"), tmpfile);
  generate_jif_file_with_tags (SHARPNESS_TAG ("hard"), tmpfile);
  libexif_check_tags (SHARPNESS_TAG ("hard"), tmpfile);

#define METERING_MODE_TAG(t) "taglist," GST_TAG_CAPTURING_METERING_MODE "=" t
  generate_jif_file_with_tags (METERING_MODE_TAG ("unknown"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("unknown"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("average"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("average"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("center-weighted-average"),
      tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("center-weighted-average"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("spot"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("spot"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("multi-spot"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("multi-spot"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("pattern"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("pattern"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("partial"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("partial"), tmpfile);
  generate_jif_file_with_tags (METERING_MODE_TAG ("other"), tmpfile);
  libexif_check_tags (METERING_MODE_TAG ("other"), tmpfile);

#define FILE_SOURCE_TAG(t) "taglist," GST_TAG_CAPTURING_SOURCE "=" t
  generate_jif_file_with_tags (FILE_SOURCE_TAG ("dsc"), tmpfile);
  libexif_check_tags (FILE_SOURCE_TAG ("dsc"), tmpfile);
  generate_jif_file_with_tags (FILE_SOURCE_TAG ("other"), tmpfile);
  libexif_check_tags (FILE_SOURCE_TAG ("other"), tmpfile);
  generate_jif_file_with_tags (FILE_SOURCE_TAG ("reflex-scanner"), tmpfile);
  libexif_check_tags (FILE_SOURCE_TAG ("reflex-scanner"), tmpfile);
  generate_jif_file_with_tags (FILE_SOURCE_TAG ("transparent-scanner"),
      tmpfile);
  libexif_check_tags (FILE_SOURCE_TAG ("transparent-scanner"), tmpfile);

  g_unlink (tmpfile);
  g_free (tmpfile);
}

GST_END_TEST;

#define HAVE_ELEMENT(name) \
  gst_registry_check_feature_version (gst_registry_get (), name,\
      GST_VERSION_MAJOR, GST_VERSION_MINOR, 0)

static Suite *
jifmux_suite (void)
{
  Suite *s = suite_create ("jifmux");
  TCase *tc_chain = tcase_create ("general");

  if (HAVE_ELEMENT ("taginject") && HAVE_ELEMENT ("jpegenc")) {
    tcase_add_test (tc_chain, test_jifmux_tags);
  } else {
    GST_WARNING ("jpegenc or taginject element not available, skipping tests");
  }

  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (jifmux);
