/*  GStreamer SBC audio decoder
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2013       Tim-Philipp Müller <tim centricular net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <gst/gst.h>
#include <gst/audio/audio.h>

#include <sbc/sbc.h>

G_BEGIN_DECLS

#define GST_TYPE_SBC_DEC \
    (gst_sbc_dec_get_type())
#define GST_SBC_DEC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SBC_DEC,GstSbcDec))
#define GST_SBC_DEC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SBC_DEC,GstSbcDecClass))
#define GST_IS_SBC_DEC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SBC_DEC))
#define GST_IS_SBC_DEC_CLASS(obj) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SBC_DEC))

typedef struct _GstSbcDec GstSbcDec;
typedef struct _GstSbcDecClass GstSbcDecClass;

struct _GstSbcDec {
    GstAudioDecoder audio_decoder;

    /*< private >*/
    sbc_t           sbc;

    gsize           frame_len;
    gsize           samples_per_frame; /* for all channels */
};

struct _GstSbcDecClass {
    GstAudioDecoderClass audio_decoder_class;
};

GType gst_sbc_dec_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (sbcdec);

G_END_DECLS
