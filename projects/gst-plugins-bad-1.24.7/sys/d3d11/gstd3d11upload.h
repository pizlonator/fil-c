/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
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

#ifndef __GST_D3D11_UPLOAD_H__
#define __GST_D3D11_UPLOAD_H__

#include "gstd3d11basefilter.h"

G_BEGIN_DECLS

#define GST_TYPE_D3D11_UPLOAD (gst_d3d11_upload_get_type())
G_DECLARE_FINAL_TYPE (GstD3D11Upload,
    gst_d3d11_upload, GST, D3D11_UPLOAD, GstD3D11BaseFilter);

G_END_DECLS

#endif /* __GST_D3D11_UPLOAD_H__ */
