/* ASF muxer plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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
#ifndef __GST_ASF_OBJECTS_H__
#define __GST_ASF_OBJECTS_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <gst/base/gstcollectpads.h>

#define ASF_PAYLOAD_IS_KEYFRAME(pay) ((pay->stream_number & 0x80) != 0)
#define ASF_MILI_TO_100NANO(v) (v * 10000)
#define ASF_GUID_SIZE 16
#define ASF_GUID_OBJSIZE_SIZE 24

typedef struct _Guid
{
  guint32 v1;
  guint16 v2;
  guint16 v3;
  guint64 v4;
} Guid;

typedef struct _GstAsfFileInfo
{
  guint64 packets_count;
  guint32 packet_size;
  gboolean broadcast;
} GstAsfFileInfo;

typedef struct _GstAsfPacketInfo
{
  guint8 err_cor_len;
  gboolean multiple_payloads;
  guint8 padd_field_type;
  guint8 packet_field_type;  
  guint8 seq_field_type;

  guint32 packet_size;
  guint32 padding;
  guint32 send_time;
  guint16 duration;
  gboolean has_keyframe;
} GstAsfPacketInfo;

typedef struct _SimpleIndexEntry
{
  guint32 packet_number;
  guint16 packet_count;
} SimpleIndexEntry;

typedef struct _AsfPayload
{
  guint8 stream_number;
  guint8 media_obj_num;
  guint32 offset_in_media_obj;
  guint8 replicated_data_length;
  guint32 media_object_size;
  guint32 presentation_time;
  GstBuffer *data;

  GstCollectData *pad;

  /* simple index info */
  gboolean has_packet_info;
  guint32 packet_number;
  guint16 packet_count;
} AsfPayload;

void gst_asf_generate_file_id (Guid *guid);

gboolean gst_byte_reader_get_asf_var_size_field (GstByteReader * reader,
    guint8 field_type, guint32 * var);
guint32 gst_asf_read_var_size_field (guint8 * data, guint8 field_type);
guint gst_asf_get_var_size_field_len (guint8 field_type);

GstAsfFileInfo *gst_asf_file_info_new (void);
void gst_asf_file_info_reset (GstAsfFileInfo * info);
void gst_asf_file_info_free (GstAsfFileInfo * info);

guint32 gst_asf_payload_get_size (AsfPayload * payload);
void gst_asf_payload_free (AsfPayload * payload);

guint64 gst_asf_get_current_time (void);

gboolean gst_asf_match_guid (const guint8 * data, const Guid * g);

void gst_asf_put_i32 (guint8 * buf, gint32 data);
void gst_asf_put_time (guint8 * buf, guint64 time);
void gst_asf_put_guid (guint8 * buf, Guid guid);
void gst_asf_put_payload (guint8 * buf, AsfPayload * payload);
guint16 gst_asf_put_subpayload (guint8 * buf, AsfPayload * payload,
    guint16 size);

gboolean gst_asf_parse_packet (GstBuffer * buffer, GstAsfPacketInfo * packet,
    gboolean trust_delta_flag, guint packet_size);
gboolean gst_asf_parse_packet_from_data (guint8 * data, gsize size, GstBuffer * buffer, GstAsfPacketInfo * packet,
    gboolean trust_delta_flag, guint packet_size);
guint64 gst_asf_match_and_peek_obj_size (const guint8 * data,
    const Guid * guid);
guint64 gst_asf_match_and_peek_obj_size_buf (GstBuffer * buf,
    const Guid * guid);
gboolean gst_asf_parse_headers (GstBuffer * buffer, GstAsfFileInfo * file_info);
gboolean gst_asf_parse_headers_from_data (guint8 * data, guint size, GstAsfFileInfo * file_info);

/* ASF tags
 * found at http://msdn.microsoft.com/en-us/library/dd562330(VS.85).aspx
 */

#define ASF_TAG_TITLE "Title\0"
#define ASF_TAG_TITLE_SORTNAME "TitleSortOrder\0"

/* FIXME asf has no artist tag other than AlbumArtist, but it has Author
 * What to use here? */
#define ASF_TAG_ARTIST "WM/AlbumArtist\0"
#define ASF_TAG_ARTIST_SORTNAME "AlbumArtistSortOrder\0"

#define ASF_TAG_ALBUM_TITLE "WM/AlbumTitle\0"
#define ASF_TAG_ALBUM_TITLE_SORTNAME "AlbumTitleSortOrder\0"

#define ASF_TAG_GENRE "WM/Genre\0"
#define ASF_TAG_COMMENT "Comment\0"
#define ASF_TAG_TRACK_NUMBER "WM/TrackNumber\0"
#define ASF_TAG_COPYRIGHT "Copyright\0"
#define ASF_TAG_COMPOSER "WM/Composer\0"

const gchar *gst_asf_get_asf_tag (const gchar * gsttag);
guint gst_asf_get_tag_field_type (GValue * value);
gboolean gst_asf_tag_present_in_content_description (const gchar * tag);

/* ASF Objects Sizes */
#define ASF_HEADER_OBJECT_SIZE 30
#define ASF_FILE_PROPERTIES_OBJECT_SIZE 104
#define ASF_STREAM_PROPERTIES_OBJECT_SIZE 78
#define ASF_HEADER_EXTENSION_OBJECT_SIZE 46
#define ASF_AUDIO_SPECIFIC_DATA_SIZE 18
#define ASF_VIDEO_SPECIFIC_DATA_SIZE 51
#define ASF_DATA_OBJECT_SIZE 50
#define ASF_SINGLE_PAYLOAD_HEADER_SIZE 15
#define ASF_MULTIPLE_PAYLOAD_HEADER_SIZE 17
#define ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_SIZE 88
#define ASF_CONTENT_DESCRIPTION_OBJECT_SIZE 34
#define ASF_EXT_CONTENT_DESCRIPTION_OBJECT_SIZE 26
#define ASF_SIMPLE_INDEX_OBJECT_SIZE 56
#define ASF_SIMPLE_INDEX_ENTRY_SIZE 6
#define ASF_METADATA_OBJECT_SIZE 26
#define ASF_PADDING_OBJECT_SIZE 24

/* Field types for data object payload description */
#define ASF_FIELD_TYPE_NONE 0
#define ASF_FIELD_TYPE_BYTE 1
#define ASF_FIELD_TYPE_WORD 2
#define ASF_FIELD_TYPE_DWORD 3
#define ASF_FIELD_TYPE_MASK 3

/* tag types */
#define ASF_TAG_TYPE_UNICODE_STR 0
#define ASF_TAG_TYPE_BYTES 1
#define ASF_TAG_TYPE_BOOL 2
#define ASF_TAG_TYPE_DWORD 3
#define ASF_TAG_TYPE_QWORD 4
#define ASF_TAG_TYPE_WORD 5

/* GUID objects */

#define ASF_HEADER_OBJECT_INDEX 0
#define ASF_FILE_PROPERTIES_OBJECT_INDEX 1
#define ASF_STREAM_PROPERTIES_OBJECT_INDEX 2
#define ASF_AUDIO_MEDIA_INDEX 3
#define ASF_NO_ERROR_CORRECTION_INDEX 4
#define ASF_AUDIO_SPREAD_INDEX 5
#define ASF_HEADER_EXTENSION_OBJECT_INDEX 6
#define ASF_RESERVED_1_INDEX 7
#define ASF_DATA_OBJECT_INDEX 8
#define ASF_EXTENDED_STREAM_PROPERTIES_OBJECT_INDEX 9
#define ASF_VIDEO_MEDIA_INDEX 10
#define ASF_SIMPLE_INDEX_OBJECT_INDEX 11
#define ASF_CONTENT_DESCRIPTION_INDEX 12
#define ASF_EXT_CONTENT_DESCRIPTION_INDEX 13
#define ASF_METADATA_OBJECT_INDEX 14
#define ASF_PADDING_OBJECT_INDEX 15

extern const Guid guids[];

#endif
