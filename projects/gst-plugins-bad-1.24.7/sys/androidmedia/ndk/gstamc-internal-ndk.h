/*
 * Copyright (C) 2012,2018 Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
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

#include "../gstamc-format.h"

#include <stdbool.h>
#include <media/NdkMediaFormat.h>

struct _GstAmcFormat
{
    AMediaFormat * ndk_media_format;
};

GstAmcFormat *
gst_amc_format_ndk_new(GError **err);
GstAmcFormat *
gst_amc_format_ndk_from_a_media_format (AMediaFormat * ndk_media_format);
