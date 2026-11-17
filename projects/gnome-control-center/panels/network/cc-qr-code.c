/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-qr-code.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-qr-code"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cc-qr-code.h"
#include "qrcodegen.c"

/**
 * SECTION: cc-qr_code
 * @title: CcQrCode
 * @short_description: A Simple QR Code wrapper around libqrencode
 * @include: "cc-qr-code.h"
 *
 * Generate a QR image from a given text.
 */

#define BYTES_PER_R8G8B8 3

struct _CcQrCode
{
  GObject     parent_instance;

  gchar      *text;
  GdkTexture *texture;
  gint        size;
};

G_DEFINE_TYPE (CcQrCode, cc_qr_code, G_TYPE_OBJECT)

static void
cc_qr_code_finalize (GObject *object)
{
  CcQrCode *self = (CcQrCode *) object;

  g_clear_object (&self->texture);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (cc_qr_code_parent_class)->finalize (object);
}

static void
cc_qr_code_class_init (CcQrCodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_qr_code_finalize;
}

static void
cc_qr_code_init (CcQrCode *self)
{
}

CcQrCode *
cc_qr_code_new (void)
{
  return g_object_new (CC_TYPE_QR_CODE, NULL);
}

gboolean
cc_qr_code_set_text (CcQrCode    *self,
                     const gchar *text)
{
  g_return_val_if_fail (CC_IS_QR_CODE (self), FALSE);
  g_return_val_if_fail (!text || *text, FALSE);

  if (g_strcmp0 (text, self->text) == 0)
    return FALSE;

  g_clear_object (&self->texture);
  g_free (self->text);
  self->text = g_strdup (text);

  return TRUE;
}

static void
cc_fill_pixel (GByteArray *array,
               guint8     value,
               int        pixel_size)
{
  guint i;

  for (i = 0; i < pixel_size; i++)
    {
      g_byte_array_append (array, &value, 1); /* R */
      g_byte_array_append (array, &value, 1); /* G */
      g_byte_array_append (array, &value, 1); /* B */
    }
}

GdkPaintable *
cc_qr_code_get_paintable (CcQrCode *self,
                          gint      size)
{
  uint8_t qr_code[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  uint8_t temp_buf[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  g_autoptr (GBytes) bytes = NULL;
  GByteArray *qr_matrix;
  gint pixel_size, qr_size, total_size;
  gint column, row, i;
  gboolean success = FALSE;

  g_return_val_if_fail (CC_IS_QR_CODE (self), NULL);
  g_return_val_if_fail (size > 0, NULL);

  if (!self->text || !*self->text)
    {
      g_warn_if_reached ();
      cc_qr_code_set_text (self, "invalid text");
    }

  if (self->texture && self->size == size)
    return GDK_PAINTABLE (self->texture);

  self->size = size;

  success = qrcodegen_encodeText (self->text,
                                  temp_buf,
                                  qr_code,
                                  qrcodegen_Ecc_LOW,
                                  qrcodegen_VERSION_MIN,
                                  qrcodegen_VERSION_MAX,
                                  qrcodegen_Mask_AUTO,
                                  FALSE);

  if (!success)
    return NULL;

  qr_size = qrcodegen_getSize (qr_code);
  pixel_size = MAX (1, size / (qr_size));
  total_size = qr_size * pixel_size;
  qr_matrix = g_byte_array_sized_new (total_size * total_size * pixel_size * BYTES_PER_R8G8B8);

  for (column = 0; column < total_size; column++)
    {
      for (i = 0; i < pixel_size; i++)
        {
          for (row = 0; row < total_size / pixel_size; row++)
            {
              if (qrcodegen_getModule (qr_code, column, row))
                cc_fill_pixel (qr_matrix, 0x00, pixel_size);
              else
                cc_fill_pixel (qr_matrix, 0xff, pixel_size);
            }
        }
    }

  bytes = g_byte_array_free_to_bytes (qr_matrix);

  g_clear_object (&self->texture);
  self->texture = gdk_memory_texture_new (total_size,
                                          total_size,
                                          GDK_MEMORY_R8G8B8,
                                          bytes,
                                          total_size * BYTES_PER_R8G8B8);

  return GDK_PAINTABLE (self->texture);
}

static gchar *
escape_string (const gchar *str,
               gboolean     quote)
{
  GString *string;
  const char *next;

  if (!str)
    return NULL;

  string = g_string_new ("");
  if (quote)
    g_string_append_c (string, '"');

  while ((next = strpbrk (str, "\\;,:\"")))
    {
      g_string_append_len (string, str, next - str);
      g_string_append_c (string, '\\');
      g_string_append_c (string, *next);
      str = next + 1;
    }

  g_string_append (string, str);
  if (quote)
    g_string_append_c (string, '"');

  return g_string_free (string, FALSE);
}

static const gchar *
get_connection_security_type (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt;

  g_return_val_if_fail (c, "nopass");

  setting = nm_connection_get_setting_wireless_security (c);

  if (!setting)
    return "nopass";

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);

  /* No IEEE 802.1x */
  if (g_strcmp0 (key_mgmt, "none") == 0)
    return "WEP";

  if (g_strcmp0 (key_mgmt, "wpa-psk") == 0)
    return "WPA";

  if (g_strcmp0 (key_mgmt, "sae") == 0)
    return "SAE";

  return "nopass";
}

gboolean
is_qr_code_supported (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt;
  NMSettingConnection *s_con;
  guint64 timestamp;

  g_return_val_if_fail (c, TRUE);

  s_con = nm_connection_get_setting_connection (c);
  timestamp = nm_setting_connection_get_timestamp (s_con);

  /* Check timestamp to determine if connection was successful in the past */
  if (timestamp == 0)
    return FALSE;

  setting = nm_connection_get_setting_wireless_security (c);

  if (!setting)
    return TRUE;

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);

  if (g_str_equal (key_mgmt, "none") ||
      g_str_equal (key_mgmt, "wpa-psk") ||
      g_str_equal (key_mgmt, "sae"))
    return TRUE;

  return FALSE;
}

gchar *
get_wifi_password (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const gchar *sec_type, *password;
  gint wep_index;

  sec_type = get_connection_security_type (c);
  setting = nm_connection_get_setting_wireless_security (c);

  if (g_str_equal (sec_type, "nopass"))
    return NULL;

  if (g_str_equal (sec_type, "WEP"))
    {
      wep_index = nm_setting_wireless_security_get_wep_tx_keyidx (setting);
      password = nm_setting_wireless_security_get_wep_key (setting, wep_index);
    }
  else
    {
      password = nm_setting_wireless_security_get_psk (setting);
    }

  return g_strdup (password);
}

/* Generate a string representing the connection
 * An example generated text:
 *     WIFI:S:ssid;T:WPA;P:my-valid-pass;H:true;
 * Where,
 *   S = ssid, T = security, P = password, H = hidden (Optional)
 *
 * See https://github.com/zxing/zxing/wiki/Barcode-Contents#wi-fi-network-config-android-ios-11
 */
gchar *
get_qr_string_for_connection (NMConnection *c)
{
  NMSettingWireless *setting;
  g_autofree char *ssid_text = NULL;
  g_autofree char *escaped_ssid = NULL;
  g_autofree char *password_str = NULL;
  g_autofree char *escaped_password = NULL;
  GString *string;
  GBytes *ssid;
  gboolean hidden;

  setting = nm_connection_get_setting_wireless (c);
  ssid = nm_setting_wireless_get_ssid (setting);

  if (!ssid)
    return NULL;

  string = g_string_new ("WIFI:S:");

  /* SSID */
  ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL),
                                     g_bytes_get_size (ssid));
  escaped_ssid = escape_string (ssid_text, FALSE);
  g_string_append (string, escaped_ssid);
  g_string_append_c (string, ';');

  /* Security type */
  g_string_append (string, "T:");
  g_string_append (string, get_connection_security_type (c));
  g_string_append_c (string, ';');

  /* Password */
  g_string_append (string, "P:");
  password_str = get_wifi_password (c);
  escaped_password = escape_string (password_str, FALSE);
  if (escaped_password)
    g_string_append (string, escaped_password);
  g_string_append_c (string, ';');

  /* WiFi Hidden */
  hidden = nm_setting_wireless_get_hidden (setting);
  if (hidden)
    g_string_append (string, "H:true");
  g_string_append_c (string, ';');

  return g_string_free (string, FALSE);
}
