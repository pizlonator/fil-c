/*
 * Copyright (C) 2013, Fluendo S.A.
 *   Author: Andoni Morales <amorales@fluendo.com>
 *
 * Copyright (C) 2014,2018 Collabora Ltd.
 *   Author: Matthieu Bouron <matthieu.bouron@collabora.com>
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

#ifndef __GST_AMC_SURFACE_TEXTURE_JNI_H__
#define __GST_AMC_SURFACE_TEXTURE_JNI_H__

#include "../gstamcsurfacetexture.h"

G_BEGIN_DECLS

#define GST_TYPE_AMC_SURFACE_TEXTURE_JNI gst_amc_surface_texture_jni_get_type ()
G_DECLARE_FINAL_TYPE (GstAmcSurfaceTextureJNI, gst_amc_surface_texture_jni, GST, AMC_SURFACE_TEXTURE_JNI, GstAmcSurfaceTexture)

GstAmcSurfaceTextureJNI * gst_amc_surface_texture_jni_new (GError ** err);

jobject gst_amc_surface_texture_jni_get_jobject (GstAmcSurfaceTextureJNI * self);

G_END_DECLS

#endif
