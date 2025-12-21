/*
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Julian Bouzas <julian.bouzas@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GST_GL_ELEMENTS_H__
#define __GST_GL_ELEMENTS_H__

#include <gst/gst.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void gl_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (glimagesink);
GST_ELEMENT_REGISTER_DECLARE (glimagesinkelement);
GST_ELEMENT_REGISTER_DECLARE (glupload);
GST_ELEMENT_REGISTER_DECLARE (gldownload);
GST_ELEMENT_REGISTER_DECLARE (glcolorconvert);
GST_ELEMENT_REGISTER_DECLARE (glcolorbalance);
GST_ELEMENT_REGISTER_DECLARE (glfilterbin);
GST_ELEMENT_REGISTER_DECLARE (glsinkbin);
GST_ELEMENT_REGISTER_DECLARE (glsrcbin);
GST_ELEMENT_REGISTER_DECLARE (glmixerbin);
GST_ELEMENT_REGISTER_DECLARE (glfiltercube);
GST_ELEMENT_REGISTER_DECLARE (gltransformation);
GST_ELEMENT_REGISTER_DECLARE (glvideoflip);
GST_ELEMENT_REGISTER_DECLARE (gleffects);
GST_ELEMENT_REGISTER_DECLARE (glcolorscale);
GST_ELEMENT_REGISTER_DECLARE (glvideomixer);
GST_ELEMENT_REGISTER_DECLARE (glvideomixerelement);
GST_ELEMENT_REGISTER_DECLARE (glshader);
GST_ELEMENT_REGISTER_DECLARE (glfilterapp);
GST_ELEMENT_REGISTER_DECLARE (glviewconvert);
GST_ELEMENT_REGISTER_DECLARE (glstereosplit);
GST_ELEMENT_REGISTER_DECLARE (glstereomix);
GST_ELEMENT_REGISTER_DECLARE (gltestsrc);
GST_ELEMENT_REGISTER_DECLARE (gldeinterlace);
GST_ELEMENT_REGISTER_DECLARE (glalpha);
GST_ELEMENT_REGISTER_DECLARE (gloverlaycompositor);
GST_ELEMENT_REGISTER_DECLARE (gloverlay);
GST_ELEMENT_REGISTER_DECLARE (glfilterglass);
GST_ELEMENT_REGISTER_DECLARE (glfilterreflectedscreen);
GST_ELEMENT_REGISTER_DECLARE (glmosaic);
GST_ELEMENT_REGISTER_DECLARE (gldifferencematte);
GST_ELEMENT_REGISTER_DECLARE (glbumper);
GST_ELEMENT_REGISTER_DECLARE (caopengllayersink);

G_END_DECLS

#endif /* __GST_GL_ELEMENTS_H__ */
