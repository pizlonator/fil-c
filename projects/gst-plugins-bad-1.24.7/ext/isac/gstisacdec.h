/* iSAC decoder
 *
 * Copyright (C) 2020 Collabora Ltd.
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>, Collabora Ltd.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifndef __GST_ISAC_DEC_H__
#define __GST_ISAC_DEC_H__

#include <gst/audio/audio.h>

G_BEGIN_DECLS

#define GST_TYPE_ISACDEC gst_isacdec_get_type ()
G_DECLARE_FINAL_TYPE(GstIsacDec, gst_isacdec, GST, ISACDEC, GstAudioDecoder)

GST_ELEMENT_REGISTER_DECLARE (isacdec);

G_END_DECLS

#endif /* __GST_ISAC_DEC_H__ */
