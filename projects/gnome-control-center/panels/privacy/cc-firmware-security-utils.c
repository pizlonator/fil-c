/* cc-firmware-security-utils.c
 *
 * Copyright (C) 2021 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Kate Hsuan <hpa@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "cc-firmware-security-utils.h"

const gchar *
fwupd_security_attr_result_to_string (FwupdSecurityAttrResult result)
{
  if (result == FWUPD_SECURITY_ATTR_RESULT_VALID)
    {
      /* TRANSLATORS: if the stauts is valid. For example security check is valid and key is valid. */
      return _("Valid");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_VALID)
    {
      /* TRANSLATORS: if the status or key is not valid. */
      return _("Not Valid");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_ENABLED)
    {
      /* TRANSLATORS: if the function is enabled through BIOS or OS settings. */
      return _("Enabled");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_ENABLED)
    {
      /* TRANSLATORS: if the function is not enabled through BIOS or OS settings. */
      return _("Not Enabled");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_LOCKED)
    {
      /* TRANSLATORS: the memory space or system mode is locked to prevent from malicious modification. */
      return _("Locked");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_LOCKED)
    {
      /* TRANSLATORS: the memory space or system mode is not locked. */
      return _("Not Locked");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_ENCRYPTED)
    {
      /* TRANSLATORS: The data is encrypted to prevent from malicious reading.  */
      return _("Encrypted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_ENCRYPTED)
    {
      /* TRANSLATORS: the data in memory is plane text. */
      return _("Not Encrypted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_TAINTED)
    {
      /* TRANSLATORS: Linux kernel is tainted by third party kernel module. */
      return _("Tainted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_TAINTED)
    {
      /* TRANSLATORS: All the loaded kernel module are licensed. */
      return _("Not Tainted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_FOUND)
    {
      /* TRANSLATORS: the feature can be detected. */
      return _("Found");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_FOUND)
    {
      /* TRANSLATORS: the feature can't be detected. */
      return _("Not Found");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_SUPPORTED)
    {
      /* TRANSLATORS: the function is supported by hardware. */
      return _("Supported");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_SUPPORTED)
    {
      /* TRANSLATORS: the function isn't supported by hardware. */
      return _("Not Supported");
    }
  return NULL;
}


/* ->summary and ->description are translated */
FwupdSecurityAttr *
fu_security_attr_new_from_variant (GVariantIter *iter)
{
  FwupdSecurityAttr *attr = g_new0 (FwupdSecurityAttr, 1);
  const gchar *key;
  GVariant *value;
  g_autofree gchar *name = NULL;

  while (g_variant_iter_next (iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, "AppstreamId") == 0)
        attr->appstream_id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "Flags") == 0)
        attr->flags = g_variant_get_uint64(value);
      else if (g_strcmp0 (key, "HsiLevel") == 0)
        attr->hsi_level = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "HsiResult") == 0)
        attr->result = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "HsiResultFallback") == 0)
        attr->result_fallback = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "Created") == 0)
        attr->timestamp = g_variant_get_uint64 (value);
      else if (g_strcmp0 (key, "Description") == 0)
        attr->description = g_strdup (dgettext ("fwupd", g_variant_get_string (value, NULL)));
      else if (g_strcmp0 (key, "Summary") == 0)
        attr->title = g_strdup (dgettext ("fwupd", g_variant_get_string (value, NULL)));
      else if (g_strcmp0 (key, "Name") == 0)
        name = g_variant_dup_string (value, NULL);
      g_variant_unref (value);
    }

  /* in fwupd <= 1.8.3 org.fwupd.hsi.Uefi.SecureBoot was incorrectly marked as HSI-0 */
  if (g_strcmp0 (attr->appstream_id, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT) == 0)
    attr->hsi_level = 1;

  /* fallback for older fwupd versions */
  if (attr->appstream_id != NULL && attr->title == NULL && name != NULL)
    attr->title = g_strdup (name);

  /* success */
  return attr;
}

void
fu_security_attr_free (FwupdSecurityAttr *attr)
{
  g_free (attr->appstream_id);
  g_free (attr->title);
  g_free (attr->description);
  g_free (attr);
}

gboolean
firmware_security_attr_has_flag (FwupdSecurityAttr     *attr,
                                 FwupdSecurityAttrFlags flag)
{
  return (attr->flags & flag) > 0;
}

void
load_custom_css (const char *path)
{
  g_autoptr (GtkCssProvider) provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, path);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void
hsi_report_title_print_padding(const gchar *title, GString *dst_string, gsize maxlen)
{
  gsize title_len;
  gsize maxpad = maxlen;

  if (maxlen == 0)
    maxpad = 50;

  if (title == NULL || dst_string == NULL)
    return;
  g_string_append_printf (dst_string, "%s", title);

  title_len = g_utf8_strlen (title, -1) + 1;
  for (gsize i = title_len; i < maxpad; i++)
    g_string_append (dst_string, " ");
}
