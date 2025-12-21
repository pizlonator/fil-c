/* GStreamer
 * Copyright (C) 2023 Carlos Rafael Giani <crg7475@mailbox.org>
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

#pragma once

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/audio/gstdsdformat.h>

G_BEGIN_DECLS

/**
 * GST_DSD_MEDIA_TYPE:
 *
 * The GStreamer media type for DSD.
 *
 * Since: 1.24
 */
#define GST_DSD_MEDIA_TYPE    "audio/x-dsd"

/**
 * GST_DSD_CAPS_MAKE:
 * @format: string format that describes the DSD bits grouping,
 *     as string (e.g. "DSDU32BE", "DSDU8", etc.)
 *
 * Generic caps string for DSD audio, for use in pad templates.
 *
 * Since: 1.24
 */
#define GST_DSD_CAPS_MAKE(format)                          \
  GST_DSD_MEDIA_TYPE ", "                                  \
  "format = (string) " format ", "                         \
  "rate = " GST_AUDIO_RATE_RANGE ", "                      \
  "layout = (string) { interleaved, non-interleaved }, "   \
  "reversed-bytes = (gboolean) { false, true }, "          \
  "channels = " GST_AUDIO_CHANNELS_RANGE

/**
 * GST_DSD_MAKE_DSD_RATE_44x:
 *
 * Calculates a valid DSD-44x rate (in bytes) from commonly used rate
 * multiplier specifications like DSD64, DSD128 etc.
 *
 * For example, to get the rate for DSD64-44x, use 64 as the multiplier
 * argument.
 *
 * Since: 1.24
 */
#define GST_DSD_MAKE_DSD_RATE_44x(multiplier) \
    ((gint) ((gint64) multiplier) * 44100 / 8)

/**
 * GST_DSD_MAKE_DSD_RATE_48x:
 *
 * Calculates a valid DSD-48x rate (in bytes) from commonly used rate
 * multiplier specifications like DSD64, DSD128 etc.
 *
 * For example, to get the rate for DSD64-48x, use 64 as the multiplier
 * argument.
 *
 * Since: 1.24
 */
#define GST_DSD_MAKE_DSD_RATE_48x(multiplier) \
    ((gint) ((gint64) multiplier) * 48000 / 8)
/**
 * GST_DSD_SILENCE_PATTERN_BYTE:
 *
 * Silence pattern for DSD data.
 *
 * In DSD, a nullbyte does not correspond to silence. To fill memory regions
 * with "DSD silence", these regions must be filled with byte 0x69 instead
 * (this is the DSD silence pattern). This constant provides that pattern
 * in a more readable fashion.
 *
 * Since: 1.24
 */
#define GST_DSD_SILENCE_PATTERN_BYTE       (0x69)

typedef struct _GstDsdInfo GstDsdInfo;

/**
 * GstDsdInfo:
 * @format: DSD grouping format
 * @rate: DSD rate
 * @channels: number of channels (must be at least 1)
 * @layout: audio layout
 * @reversed_bytes: true if the DSD bits in the data bytes are reversed,
 *   that is, the least significant bit comes first
 * @positions: positions for each channel
 *
 * Information describing DSD audio properties.
 *
 * In DSD, the "sample format" is the bit. Unlike PCM, there are no further
 * "sample formats" in DSD. However, in software, DSD bits are grouped into
 * bytes (since dealing with individual bits is impractical), and these bytes
 * in turn are grouped into words. This becomes relevant when interleaving
 * channels and transmitting DSD data through audio APIs. The different
 * types of grouping DSD bytes are referred to as the "DSD grouping forma"
 * or just "DSD format". #GstDsdFormat has a list of valid ways of grouping
 * DSD bytes into words.
 *
 * DSD rates are equivalent to PCM sample rates, except that they specify
 * how many DSD bytes are consumed per second. This refers to the bytes per
 * second _per channel_; the rate does not change when the number of channel
 * changes. (Strictly speaking, it would be more correct to measure the
 * *bits* per second, since the bit is the DSD "sample format", but it is
 * more practical to use bytes.) In DSD, bit rates are always an integer
 * multiple of the CD audio rate (44100) or the DAT rate (48000). DSD64-44x
 * is 44100 * 64 = 2822400 bits per second, or 352800 bytes per second
 * (the latter would be used in this info structure). DSD64-48x is
 * 48000 * 64 = 3072000 bits per second, or 384000 bytes per second.
 * #GST_DSD_MAKE_DSD_RATE_44x can be used for specifying DSD-44x rates,
 * *and #GST_DSD_MAKE_DSD_RATE_48x can be used for specifying DSD-48x ones.
 * Also, since DSD-48x is less well known, when the multiplier is given
 * without the 44x/48x specifier, 44x is typically implied.
 *
 * It is important to know that in DSD, different format widths correspond
 * to different playtimes. That is, a word with 32 DSD bits covers two times
 * as much playtime as a word with 16 DSD bits. This is in contrast to PCM,
 * where one word (= one PCM sample) always covers a time period of 1/samplerate,
 * no matter how many bits a PCM sample is made of. For this reason, DSD
 * and PCM widths and strides cannot be used the same way.
 *
 * Multiple channels are arranged in DSD data either interleaved or non-
 * interleaved. This is similar to PCM. Interleaved layouts rotate between
 * channels and words. First, word 0 of channel 0 is present. Then word
 * 0 of channel 1 follows. Then word 0 of channel 2 etc. until all
 * channels are through, then comes word 1 of channel 0 etc.
 *
 * Non-interleaved data is planar. First, all words of channel 0 are
 * present, then all words of channel 1 etc. Unlike interleaved data,
 * non-interleaved data can be sparse, that is, there can be space in
 * between the planes. the @positions array specifies the plane offsets.
 *
 * In uncommon cases, the DSD bits in the data bytes can be stored in reverse
 * order. For example, normally, in DSDU8, the first byte contains DSD bits
 * 0 to 7, and the most significant bit of that byte is DSD bit 0. If this
 * order is reversed, then bit 7 is the first one instead. In that ase,
 * @reversed_bytes is set to TRUE.
 *
 * Use the provided macros to access the info in this structure.
 *
 * Since: 1.24
 */
struct _GstDsdInfo {
  GstDsdFormat            format;
  gint                    rate;
  gint                    channels;
  GstAudioLayout          layout;
  gboolean                reversed_bytes;
  GstAudioChannelPosition positions[64];
  GstAudioFlags           flags;

  /*< private >*/
  gpointer _gst_reserved[GST_PADDING];
};

#define GST_TYPE_DSD_INFO                  (gst_dsd_info_get_type ())
GST_AUDIO_API
GType gst_dsd_info_get_type                (void);

#define GST_DSD_INFO_IS_VALID(i)           ((i)->format < GST_NUM_DSD_FORMATS && (i)->rate > 0 && (i)->channels > 0)

#define GST_DSD_INFO_FORMAT(info)          ((info)->format)
#define GST_DSD_INFO_RATE(info)            ((info)->rate)
#define GST_DSD_INFO_CHANNELS(info)        ((info)->channels)
#define GST_DSD_INFO_LAYOUT(info)          ((info)->layout)
#define GST_DSD_INFO_REVERSED_BYTES(info)  ((info)->reversed_bytes)
#define GST_DSD_INFO_POSITION(info,c)      ((info)->position[c])

/**
 * GST_DSD_INFO_STRIDE:
 *
 * Calculates the stride for a given #GstDsdInfo.
 *
 * Note that this is only useful if the info's audio layout
 * is GST_AUDIO_LAYOUT_INTERLEAVED.
 *
 * Since: 1.24
 */
#define GST_DSD_INFO_STRIDE(info)          (gst_dsd_format_get_width((info)->format) * (info)->channels)

/*** GstDsdPlaneOffsetMeta ***/

#define GST_DSD_PLANE_OFFSET_META_API_TYPE (gst_dsd_plane_offset_meta_api_get_type())
#define GST_DSD_PLANE_OFFSET_META_INFO (gst_dsd_plane_offset_meta_get_info())

/**
 * GST_META_TAG_DSD_PLANE_OFFSETS_STR:
 *
 * This metadata stays relevant as long as the DSD plane offsets are unchanged.
 *
 * Since: 1.24
 */
#define GST_META_TAG_DSD_PLANE_OFFSETS_STR "dsdplaneoffsets"

typedef struct _GstDsdPlaneOffsetMeta GstDsdPlaneOffsetMeta;

/**
 * GstDsdPlaneOffsetMeta:
 * @meta: parent #GstMeta
 * @num_channels: number of channels in the DSD data
 * @num_bytes_per_channel: the number of valid bytes per channel in the buffer
 * @offsets: the offsets (in bytes) where each channel plane starts in the buffer
 *
 * Buffer metadata describing planar DSD contents in the buffer. This is not needed
 * for interleaved DSD data, and is required for non-interleaved (= planar) data.
 *
 * The different channels in @offsets are always in the GStreamer channel order.
 * Zero-copy channel reordering can be implemented by swapping the values in
 * @offsets.
 *
 * It is not allowed for channels to overlap in memory,
 * i.e. for each i in [0, channels), the range
 * [@offsets[i], @offsets[i] + @num_bytes_per_channel) must not overlap
 * with any other such range.
 *
 * It is, however, allowed to have parts of the buffer memory unused, by using
 * @offsets and @num_bytes_per_channel in such a way that leave gaps on it.
 * This is used to implement zero-copy clipping in non-interleaved buffers.
 *
 * Obviously, due to the above, it is not safe to infer the
 * number of valid bytes from the size of the buffer. You should always
 * use the @num_bytes_per_channel variable of this metadata.
 *
 * Since: 1.24
 */
struct _GstDsdPlaneOffsetMeta {
  GstMeta      meta;
  gint         num_channels;
  gsize        num_bytes_per_channel;
  gsize        *offsets;

  /*< private >*/
  gsize        priv_offsets_arr[8];
  gpointer     _gst_reserved[GST_PADDING];
};

GST_AUDIO_API
GType gst_dsd_plane_offset_meta_api_get_type (void);

GST_AUDIO_API
const GstMetaInfo * gst_dsd_plane_offset_meta_get_info (void);

#define gst_buffer_get_dsd_plane_offset_meta(b) \
    ((GstDsdPlaneOffsetMeta*)gst_buffer_get_meta((b), GST_DSD_PLANE_OFFSET_META_API_TYPE))

GST_AUDIO_API
GstDsdPlaneOffsetMeta * gst_buffer_add_dsd_plane_offset_meta (GstBuffer *buffer,
                                                              gint num_channels,
                                                              gsize num_bytes_per_channel,
                                                              gsize offsets[]);

GST_AUDIO_API
GstDsdInfo *  gst_dsd_info_new           (void);

GST_AUDIO_API
GstDsdInfo *  gst_dsd_info_new_from_caps (const GstCaps * caps);

GST_AUDIO_API
void          gst_dsd_info_init          (GstDsdInfo * info);

GST_AUDIO_API
void          gst_dsd_info_set_format    (GstDsdInfo * info, 
                                          GstDsdFormat format,
                                          gint rate, 
                                          gint channels,
                                          const GstAudioChannelPosition * positions);

GST_AUDIO_API
GstDsdInfo *  gst_dsd_info_copy          (const GstDsdInfo * info);

GST_AUDIO_API
void          gst_dsd_info_free          (GstDsdInfo * info);

GST_AUDIO_API
gboolean      gst_dsd_info_from_caps     (GstDsdInfo *info, 
                                          const GstCaps *caps);

GST_AUDIO_API
GstCaps *     gst_dsd_info_to_caps       (const GstDsdInfo *info);

GST_AUDIO_API
gboolean      gst_dsd_info_is_equal      (const GstDsdInfo *info,
                                          const GstDsdInfo *other);

GST_AUDIO_API
void          gst_dsd_convert            (const guint8 *input_data, 
                                          guint8 *output_data,
                                          GstDsdFormat input_format, 
                                          GstDsdFormat output_format,
                                          GstAudioLayout input_layout, 
                                          GstAudioLayout output_layout,
                                          const gsize *input_plane_offsets, 
                                          const gsize *output_plane_offsets,
                                          gsize num_dsd_bytes, 
                                          gint num_channels, 
                                          gboolean reverse_byte_bits);

/**
 * gst_dsd_format_is_le:
 * @format: The format.
 *
 * Useful for determining whether a format is a little-endian.
 * GST_DSD_FORMAT_U8 and GST_DSD_FORMAT_UNKNOWN
 * are not considered little-endian.
 *
 * Returns: TRUE if the format is a little-endian one.
 */
static inline gboolean 
gst_dsd_format_is_le (GstDsdFormat format)
{
  switch (format) {
    case GST_DSD_FORMAT_U16LE:
    case GST_DSD_FORMAT_U32LE:
      return TRUE;
    default:
      return FALSE;
  }
}

G_END_DECLS
