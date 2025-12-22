/*
 * Copyright (C) 2018 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
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

#ifndef __GST_AMC_SURFACE_TEXTURE_ML_H__
#define __GST_AMC_SURFACE_TEXTURE_ML_H__

#include "../gstamcsurfacetexture.h"
#include <ml_api.h>

G_BEGIN_DECLS

#define GST_TYPE_AMC_SURFACE_TEXTURE_ML gst_amc_surface_texture_ml_get_type ()
G_DECLARE_FINAL_TYPE (GstAmcSurfaceTextureML, gst_amc_surface_texture_ml, GST, AMC_SURFACE_TEXTURE_ML, GstAmcSurfaceTexture)

GstAmcSurfaceTextureML * gst_amc_surface_texture_ml_new (GError ** err);
MLHandle gst_amc_surface_texture_ml_get_handle (GstAmcSurfaceTextureML * self);

G_END_DECLS

#endif
