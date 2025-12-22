/*  GStreamer LDAC audio encoder
 *  Copyright (C) 2020 Asymptotic <sanchayan@asymptotic.io>
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
 */

#include <gst/gst.h>
#include <gst/audio/audio.h>

#include <ldacBT.h>

G_BEGIN_DECLS

#define GST_TYPE_LDAC_ENC \
	(gst_ldac_enc_get_type())
#define GST_LDAC_ENC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LDAC_ENC,GstLdacEnc))
#define GST_LDAC_ENC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LDAC_ENC,GstLdacEncClass))
#define GST_IS_LDAC_ENC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LDAC_ENC))
#define GST_IS_LDAC_ENC_CLASS(obj) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LDAC_ENC))

typedef struct _GstLdacEnc GstLdacEnc;
typedef struct _GstLdacEncClass GstLdacEncClass;

typedef enum
{
  GST_LDAC_EQMID_HQ = 0,
  GST_LDAC_EQMID_SQ,
  GST_LDAC_EQMID_MQ
} GstLdacEqmid;

struct _GstLdacEnc {
    GstAudioEncoder audio_encoder;
    GstLdacEqmid eqmid;

    guint rate;
    guint channels;
    guint channel_mode;
    gboolean init_done;

    LDACBT_SMPL_FMT_T ldac_fmt;
    HANDLE_LDAC_BT ldac;
};

struct _GstLdacEncClass {
    GstAudioEncoderClass audio_encoder_class;
};

GType gst_ldac_enc_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (ldacenc);

G_END_DECLS
