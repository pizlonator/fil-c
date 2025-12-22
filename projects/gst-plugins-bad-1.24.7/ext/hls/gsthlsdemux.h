/* GStreamer
 * Copyright (C) 2010 Marc-Andre Lureau <marcandre.lureau@gmail.com>
 * Copyright (C) 2010 Andoni Morales Alastruey <ylatuya@gmail.com>
 * Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
 *
 * gsthlsdemux.h:
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


#ifndef __GST_HLS_DEMUX_H__
#define __GST_HLS_DEMUX_H__

#include <gst/gst.h>
#include "m3u8.h"
#include <gst/adaptivedemux/gstadaptivedemux.h>
#if defined(HAVE_OPENSSL)
#include <openssl/evp.h>
#elif defined(HAVE_NETTLE)
#include <nettle/aes.h>
#include <nettle/cbc.h>
#elif defined(HAVE_LIBGCRYPT)
#include <gcrypt.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_HLS_DEMUX \
  (gst_hls_demux_get_type())
#define GST_HLS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HLS_DEMUX,GstHLSDemux))
#define GST_HLS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HLS_DEMUX,GstHLSDemuxClass))
#define GST_IS_HLS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HLS_DEMUX))
#define GST_IS_HLS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HLS_DEMUX))
#define GST_HLS_DEMUX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),GST_TYPE_HLS_DEMUX,GstHLSDemuxClass))
#define GST_HLS_DEMUX_CAST(obj) \
  ((GstHLSDemux *)obj)

typedef struct _GstHLSDemux GstHLSDemux;
typedef struct _GstHLSDemuxClass GstHLSDemuxClass;
typedef struct _GstHLSDemuxStream GstHLSDemuxStream;
typedef struct _GstHLSTSReader GstHLSTSReader;

#define GST_HLS_DEMUX_STREAM_CAST(stream) ((GstHLSDemuxStream *)(stream))

typedef enum {
  GST_HLS_TSREADER_NONE,
  GST_HLS_TSREADER_MPEGTS,
  GST_HLS_TSREADER_ID3
} GstHLSTSReaderType;

struct _GstHLSTSReader
{
  GstHLSTSReaderType rtype;
  gboolean have_id3;

  gint packet_size;
  gint pmt_pid;
  gint pcr_pid;

  GstClockTime last_pcr;
  GstClockTime first_pcr;
};

struct _GstHLSDemuxStream
{
  GstAdaptiveDemuxStream adaptive_demux_stream;

  GstHLSTSReaderType stream_type;

  GstM3U8 *playlist;
  gboolean is_primary_playlist;

  gboolean do_typefind;         /* Whether we need to typefind the next buffer */
  GstBuffer *pending_typefind_buffer; /* for collecting data until typefind succeeds */

  GstAdapter *pending_encrypted_data;  /* for chunking data into 16 byte multiples for decryption */
  GstBuffer *pending_decrypted_buffer; /* last decrypted buffer for pkcs7 unpadding.
                                          We only know that it is the last at EOS */
  guint64 current_offset;              /* offset we're currently at */
  gboolean reset_pts;

  /* decryption tooling */
#if defined(HAVE_OPENSSL)
# if OPENSSL_VERSION_NUMBER < 0x10100000L
  EVP_CIPHER_CTX aes_ctx;
# else
  EVP_CIPHER_CTX *aes_ctx;
# endif
#elif defined(HAVE_NETTLE)
  struct CBC_CTX (struct aes128_ctx, AES_BLOCK_SIZE) aes_ctx;
#elif defined(HAVE_LIBGCRYPT)
  gcry_cipher_hd_t aes_ctx;
#endif

  gchar     *current_key;
  guint8    *current_iv;

  /* Accumulator for reading PAT/PMT/PCR from
   * the stream so we can set timestamps/segments
   * and switch cleanly */
  GstBuffer *pending_pcr_buffer;

  GstHLSTSReader tsreader;
};

typedef struct {
  guint8 data[16];
} GstHLSKey;

/**
 * GstHLSDemux:
 *
 * Opaque #GstHLSDemux data structure.
 */
struct _GstHLSDemux
{
  GstAdaptiveDemux parent;

  gint srcpad_counter;

  /* Decryption key cache: url => GstHLSKey */
  GHashTable *keys;
  GMutex      keys_lock;

  GstDateTime *prog_dt;

  /* FIXME: check locking, protected automatically by manifest_lock already? */
  /* The master playlist with the available variant streams */
  GstHLSMasterPlaylist *master;

  GstHLSVariantStream  *current_variant;
  /* The previous variant, used to transition streams over */
  GstHLSVariantStream  *previous_variant;

  gboolean streams_aware;
};

struct _GstHLSDemuxClass
{
  GstAdaptiveDemuxClass parent_class;
};


void gst_hlsdemux_tsreader_init (GstHLSTSReader *r);
void gst_hlsdemux_tsreader_set_type (GstHLSTSReader *r, GstHLSTSReaderType rtype);

gboolean gst_hlsdemux_tsreader_find_pcrs (GstHLSTSReader *r, GstBuffer **buffer,
    GstClockTime *first_pcr, GstClockTime *last_pcr, GstTagList **tags);

GType gst_hls_demux_get_type (void);

G_END_DECLS
#endif /* __GST_HLS_DEMUX_H__ */
