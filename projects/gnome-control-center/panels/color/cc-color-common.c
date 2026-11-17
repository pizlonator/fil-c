/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "cc-color-common.h"

gchar *
cc_color_device_get_title (CdDevice *device)
{
  const gchar *tmp;
  GString *string;

  string = g_string_new ("");

  /* is laptop panel */
  if (cd_device_get_kind (device) == CD_DEVICE_KIND_DISPLAY &&
      cd_device_get_embedded (device))
    {
      /* TRANSLATORS: This refers to the TFT display on a laptop */
      g_string_append (string, _("Laptop Screen"));
      goto out;
    }

  /* is internal webcam */
  if (cd_device_get_kind (device) == CD_DEVICE_KIND_WEBCAM &&
      cd_device_get_embedded (device))
    {
      /* TRANSLATORS: This refers to the embedded webcam on a laptop */
      g_string_append (string, _("Built-in Webcam"));
      goto out;
    }

  /* get the display model, falling back to something sane */
  tmp = cd_device_get_model (device);
  if (tmp == NULL)
    tmp = cd_device_get_vendor (device);
  if (tmp == NULL)
    tmp = cd_device_get_id (device);

  switch (cd_device_get_kind (device)) {
    case CD_DEVICE_KIND_DISPLAY:
      /* TRANSLATORS: an externally connected display, where %s is either the
       * model, vendor or ID, e.g. 'LP2480zx Monitor' */
      g_string_append_printf (string, _("%s Monitor"), tmp);
      break;
    case CD_DEVICE_KIND_SCANNER:
      /* TRANSLATORS: a flatbed scanner device, e.g. 'Epson Scanner' */
      g_string_append_printf (string, _("%s Scanner"), tmp);
      break;
    case CD_DEVICE_KIND_CAMERA:
      /* TRANSLATORS: a camera device, e.g. 'Nikon D60 Camera' */
      g_string_append_printf (string, _("%s Camera"), tmp);
      break;
    case CD_DEVICE_KIND_PRINTER:
      /* TRANSLATORS: a printer device, e.g. 'Epson Photosmart Printer' */
      g_string_append_printf (string, _("%s Printer"), tmp);
      break;
    case CD_DEVICE_KIND_WEBCAM:
      /* TRANSLATORS: a webcam device, e.g. 'Philips HiDef Camera' */
      g_string_append_printf (string, _("%s Webcam"), tmp);
      break;
    default:
      g_string_append (string, tmp);
      break;
  }
out:
  return g_string_free (string, FALSE);
}

static const gchar *
cc_color_device_kind_to_sort (CdDevice *device)
{
  CdDeviceKind kind = cd_device_get_kind (device);
  if (kind == CD_DEVICE_KIND_DISPLAY)
    return "4";
  if (kind == CD_DEVICE_KIND_SCANNER)
    return "3";
  if (kind == CD_DEVICE_KIND_CAMERA)
    return "2";
  if (kind == CD_DEVICE_KIND_WEBCAM)
    return "1";
  if (kind == CD_DEVICE_KIND_PRINTER)
    return "0";
  return "9";
}

gchar *
cc_color_device_get_sortable_base (CdDevice *device)
{
  g_autofree gchar *title = cc_color_device_get_title (device);
  return g_strdup_printf ("%s-%s-%s",
                          cc_color_device_kind_to_sort (device),
                          cd_device_get_id (device),
                          title);
}
