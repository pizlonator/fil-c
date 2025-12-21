/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2001 Thomas <thomas@apestaart.org>
 *               2005,2006 Wim Taymans <wim@fluendo.com>
 *                    2013 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2020 Huawei Technologies Co., Ltd.
 *   @Author: Stéphane Cerveau <scerveau@collabora.com>
 *
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

#ifndef __GST_AUDIO_MIXER_ELEMENTS_H__
#define __GST_AUDIO_MIXER_ELEMENTS_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstaudioaggregator.h>

#include "gstaudiomixer.h"
#include "gstaudiointerleave.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL void audiomixer_element_init (GstPlugin * plugin);

GST_ELEMENT_REGISTER_DECLARE (audiomixer);
GST_ELEMENT_REGISTER_DECLARE (liveadder);
GST_ELEMENT_REGISTER_DECLARE (audiointerleave);


#endif /* __GST_AUDIO_MIXER_ELEMENTS_H__ */
