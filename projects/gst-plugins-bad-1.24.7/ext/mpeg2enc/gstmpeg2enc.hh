/* GStreamer mpeg2enc (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 * (c) 2006 Mark Nauwelaerts <manauw@skynet.be>
 *
 * gstmpeg2enc.hh: object definition
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

#ifndef __GST_MPEG2ENC_H__
#define __GST_MPEG2ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstmpeg2encoptions.hh"
#include "gstmpeg2encoder.hh"

G_BEGIN_DECLS

#define GST_TYPE_MPEG2ENC \
  (gst_mpeg2enc_get_type ())
#define GST_MPEG2ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MPEG2ENC, GstMpeg2enc))
#define GST_MPEG2ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MPEG2ENC, GstMpeg2enc))
#define GST_IS_MPEG2ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MPEG2ENC))
#define GST_IS_MPEG2ENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MPEG2ENC))

GST_DEBUG_CATEGORY_EXTERN (mpeg2enc_debug);
#define GST_CAT_DEFAULT mpeg2enc_debug

#define GST_MPEG2ENC_MUTEX_LOCK(m) G_STMT_START {                       \
  GST_LOG_OBJECT (m, "locking tlock from thread %p", g_thread_self ()); \
  g_mutex_lock (&m->tlock);                                              \
  GST_LOG_OBJECT (m, "locked tlock from thread %p", g_thread_self ());  \
} G_STMT_END

#define GST_MPEG2ENC_MUTEX_UNLOCK(m) G_STMT_START {                       \
  GST_LOG_OBJECT (m, "unlocking tlock from thread %p", g_thread_self ()); \
  g_mutex_unlock (&m->tlock);                                              \
} G_STMT_END

#define GST_MPEG2ENC_WAIT(m) G_STMT_START {                             \
  GST_LOG_OBJECT (m, "thread %p waiting", g_thread_self ());            \
  g_cond_wait (&m->cond, &m->tlock);                                      \
} G_STMT_END

#define GST_MPEG2ENC_SIGNAL(m) G_STMT_START {                           \
  GST_LOG_OBJECT (m, "signalling from thread %p", g_thread_self ());    \
  g_cond_signal (&m->cond);                                              \
} G_STMT_END

typedef struct _GstMpeg2enc {
  GstVideoEncoder base_video_encoder;

  /* options wrapper */
  GstMpeg2EncOptions *options;

  /* general encoding object (contains rest) */
  GstMpeg2Encoder *encoder;

  /* lock for syncing with encoding task */
  GMutex tlock;
  /* with TLOCK */
  /* signals counterpart thread that something changed;
   * frame ready for task or buffer has been processed */
  GCond cond;
  /* seen eos */
  gboolean eos;
  /* flowreturn obtained by encoding task */
  GstFlowReturn srcresult;

  gboolean started;

  GstVideoCodecState *input_state;
  GstVideoCodecFrame *pending_frame;
} GstMpeg2enc;

typedef struct _GstMpeg2encClass {
  GstVideoEncoderClass base_video_encoder_class;
} GstMpeg2encClass;

GType    gst_mpeg2enc_get_type    (void);

GST_ELEMENT_REGISTER_DECLARE (mpeg2enc);

G_END_DECLS

#endif /* __GST_MPEG2ENC_H__ */
