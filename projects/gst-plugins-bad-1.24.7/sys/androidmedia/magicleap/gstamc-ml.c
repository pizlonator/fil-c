/*
 * Copyright (C) 2023, Ratchanan Srirattanamet <peathot@hotmail.com>
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

#include "../gstamc.h"
#include "gstamc-internal-ml.h"

gboolean
gst_amc_static_init (void)
{
  /*
   * MagicLeap doesn't require any static initialization. All required
   * functions are in C and are linked into the plugin directly.
   */

  gst_amc_codec_vtable = &gst_amc_codec_ml_vtable;
  gst_amc_format_vtable = &gst_amc_format_ml_vtable;

  return TRUE;
}
