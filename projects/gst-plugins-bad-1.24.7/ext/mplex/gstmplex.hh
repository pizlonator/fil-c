/* GStreamer mplex (mjpegtools) wrapper
 * (c) 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * gstmplex.hh: gstreamer mplex wrapper
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

#ifndef __GST_MPLEX_H__
#define __GST_MPLEX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <multiplexor.hpp>
#include "gstmplexibitstream.hh"
#include "gstmplexjob.hh"

G_BEGIN_DECLS

#define GST_TYPE_MPLEX \
  (gst_mplex_get_type ())
#define GST_MPLEX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MPLEX, GstMplex))
#define GST_MPLEX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_MPLEX, GstMplex))
#define GST_IS_MPLEX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MPLEX))
#define GST_IS_MPLEX_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MPLEX))

GST_DEBUG_CATEGORY_EXTERN (mplex_debug);
#define GST_CAT_DEFAULT mplex_debug

#define GST_MPLEX_MUTEX_LOCK(m) G_STMT_START {                          \
  GST_LOG_OBJECT (m, "locking tlock from thread %p", g_thread_self ()); \
  g_mutex_lock (&(m)->tlock);                                            \
  GST_LOG_OBJECT (m, "locked tlock from thread %p", g_thread_self ());  \
} G_STMT_END

#define GST_MPLEX_MUTEX_UNLOCK(m) G_STMT_START {                          \
  GST_LOG_OBJECT (m, "unlocking tlock from thread %p", g_thread_self ()); \
  g_mutex_unlock (&(m)->tlock);                                            \
} G_STMT_END

#define GST_MPLEX_WAIT(m, p) G_STMT_START {                          \
  GST_LOG_OBJECT (m, "thread %p waiting", g_thread_self ());         \
  g_cond_wait (&(p)->cond, &(m)->tlock);                               \
} G_STMT_END

#define GST_MPLEX_SIGNAL(m, p) G_STMT_START {                           \
  GST_LOG_OBJECT (m, "signalling from thread %p", g_thread_self ());    \
  g_cond_signal (&(p)->cond);                                            \
} G_STMT_END

#define GST_MPLEX_SIGNAL_ALL(m) G_STMT_START {                        \
  GST_LOG_OBJECT (m, "signalling all from thread %p", g_thread_self ());    \
  GSList *walk = m->pads;                                                   \
  while (walk) {                                                            \
  	GST_MPLEX_SIGNAL (m, (GstMplexPad *) walk->data);                          \
  	walk = walk->next;                                                      \
  }                                                                         \
} G_STMT_END

typedef struct _GstMplexPad
{
  /* associated pad */
  GstPad *pad;
  /* with mplex TLOCK */
  /* adapter collecting buffers for this pad */
  GstAdapter *adapter;
  /* no more to expect on this pad */
  gboolean eos;
  /* signals counterpart thread to have a look */
  GCond cond;
  /* amount needed by mplex on this stream */
  guint needed;
  /* bitstream for this pad */
  GstMplexIBitStream *bs;
} GstMplexPad;

typedef struct _GstMplex {
  GstElement parent;

  /* pads */
  GSList *pads;
  GstPad *srcpad;
  guint num_apads, num_vpads;

  /* options wrapper */
  GstMplexJob *job;

  /* lock for syncing */
  GMutex tlock;
  /* with TLOCK */
  /* muxer writer generated eos */
  gboolean eos;
  /* flowreturn obtained by muxer task */
  GstFlowReturn srcresult;
} GstMplex;

typedef struct _GstMplexClass {
  GstElementClass parent;
} GstMplexClass;

GType    gst_mplex_get_type    (void);

GST_ELEMENT_REGISTER_DECLARE (mplex);

G_END_DECLS

#endif /* __GST_MPLEX_H__ */
