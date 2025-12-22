/*
 * gstmpegtsdescriptor.h -
 * Copyright (C) 2013 Edward Hervey
 *
 * Authors:
 *   Edward Hervey <edward@collabora.com>
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
 *
 * Some parts of this code come from the Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 */

#ifndef GST_MPEGTS_DESCRIPTOR_H
#define GST_MPEGTS_DESCRIPTOR_H

#include "gst-metadata-descriptor.h"
#include <gst/gst.h>
#include <gst/mpegts/mpegts-prelude.h>

G_BEGIN_DECLS

/*
 * descriptor_tag TS  PS                      Identification
 *        0       n/a n/a Reserved
 *        1       n/a n/a Reserved
 *        2        X   X  video_stream_descriptor
 *        3        X   X  audio_stream_descriptor
 *        4        X   X  hierarchy_descriptor
 *        5        X   X  registration_descriptor
 *        6        X   X  data_stream_alignment_descriptor
 *        7        X   X  target_background_grid_descriptor
 *        8        X   X  video_window_descriptor
 *        9        X   X  CA_descriptor
 *       10        X   X  ISO_639_language_descriptor
 *       11        X   X  system_clock_descriptor
 *       12        X   X  multiplex_buffer_utilization_descriptor
 *       13        X   X  copyright_descriptor
 *       14        X      maximum bitrate descriptor
 *       15        X   X  private data indicator descriptor
 *       16        X   X  smoothing buffer descriptor
 *       17        X      STD_descriptor
 *       18        X   X  IBP descriptor
 *      19-63     n/a n/a ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved
 *     64-255     n/a n/a User Private
 */

/**
 * GstMpegtsDescriptorType:
 *
 * The type of #GstMpegtsDescriptor
 *
 * These values correspond to the registered descriptor type from
 * the base MPEG-TS specifications (ITU H.222.0 | ISO/IEC 13818-1).
 *
 * Consult the relevant specifications for more details.
 */
typedef enum {
  /* 0-18 ISO/IEC 13818-1 (H222.0 06/2012) */
  GST_MTS_DESC_RESERVED_00                      = 0x00,
  GST_MTS_DESC_RESERVED_01                      = 0x01,
  GST_MTS_DESC_VIDEO_STREAM                     = 0x02,
  GST_MTS_DESC_AUDIO_STREAM                     = 0x03,
  GST_MTS_DESC_HIERARCHY                        = 0x04,
  GST_MTS_DESC_REGISTRATION                     = 0x05,
  GST_MTS_DESC_DATA_STREAM_ALIGNMENT            = 0x06,
  GST_MTS_DESC_TARGET_BACKGROUND_GRID           = 0x07,
  GST_MTS_DESC_VIDEO_WINDOW                     = 0x08,
  GST_MTS_DESC_CA                               = 0x09,
  GST_MTS_DESC_ISO_639_LANGUAGE                 = 0x0A,
  GST_MTS_DESC_SYSTEM_CLOCK                     = 0x0B,
  GST_MTS_DESC_MULTIPLEX_BUFFER_UTILISATION     = 0x0C,
  GST_MTS_DESC_COPYRIGHT                        = 0x0D,
  GST_MTS_DESC_MAXIMUM_BITRATE                  = 0x0E,
  GST_MTS_DESC_PRIVATE_DATA_INDICATOR           = 0x0F,
  GST_MTS_DESC_SMOOTHING_BUFFER                 = 0x10,
  GST_MTS_DESC_STD                              = 0x11,
  GST_MTS_DESC_IBP                              = 0x12,

  /* 19-26 Defined in ISO/IEC 13818-6 (Extensions for DSM-CC) */
  GST_MTS_DESC_DSMCC_CAROUSEL_IDENTIFIER        = 0x13,
  GST_MTS_DESC_DSMCC_ASSOCIATION_TAG            = 0x14,
  GST_MTS_DESC_DSMCC_DEFERRED_ASSOCIATION_TAG   = 0x15,
  /* 0x16 is reserved (so far) */
  GST_MTS_DESC_DSMCC_NPT_REFERENCE              = 0x17,
  GST_MTS_DESC_DSMCC_NPT_ENDPOINT               = 0x18,
  GST_MTS_DESC_DSMCC_STREAM_MODE                = 0x19,
  GST_MTS_DESC_DSMCC_STREAM_EVENT               = 0x1A,

  /* 27-54 Later additions to ISO/IEC 13818-1 (H222.0 06/2012) */
  GST_MTS_DESC_MPEG4_VIDEO                      = 0x1B,
  GST_MTS_DESC_MPEG4_AUDIO                      = 0x1C,
  GST_MTS_DESC_IOD                              = 0x1D,
  GST_MTS_DESC_SL                               = 0x1E,
  GST_MTS_DESC_FMC                              = 0x1F,
  GST_MTS_DESC_EXTERNAL_ES_ID                   = 0x20,
  GST_MTS_DESC_MUX_CODE                         = 0x21,
  GST_MTS_DESC_FMX_BUFFER_SIZE                  = 0x22,
  GST_MTS_DESC_MULTIPLEX_BUFFER                 = 0x23,
  GST_MTS_DESC_CONTENT_LABELING                 = 0x24,
  GST_MTS_DESC_METADATA_POINTER                 = 0x25,
  GST_MTS_DESC_METADATA                         = 0x26,
  GST_MTS_DESC_METADATA_STD                     = 0x27,
  GST_MTS_DESC_AVC_VIDEO                        = 0x28,
  /* defined in ISO/IEC 13818-11, MPEG-2 IPMP */
  GST_MTS_DESC_IPMP                             = 0x29,
  GST_MTS_DESC_AVC_TIMING_AND_HRD               = 0x2A,
  GST_MTS_DESC_MPEG2_AAC_AUDIO                  = 0x2B,
  GST_MTS_DESC_FLEX_MUX_TIMING                  = 0x2C,
  GST_MTS_DESC_MPEG4_TEXT                       = 0x2D,
  GST_MTS_DESC_MPEG4_AUDIO_EXTENSION            = 0x2E,
  GST_MTS_DESC_AUXILIARY_VIDEO_STREAM           = 0x2F,
  GST_MTS_DESC_SVC_EXTENSION                    = 0x30,
  GST_MTS_DESC_MVC_EXTENSION                    = 0x31,
  GST_MTS_DESC_J2K_VIDEO                        = 0x32,
  GST_MTS_DESC_MVC_OPERATION_POINT              = 0x33,
  GST_MTS_DESC_MPEG2_STEREOSCOPIC_VIDEO_FORMAT  = 0x34,
  GST_MTS_DESC_STEREOSCOPIC_PROGRAM_INFO        = 0x35,
  GST_MTS_DESC_STEREOSCOPIC_VIDEO_INFO          = 0x36,

  /* 55-63 ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved */
} GstMpegtsDescriptorType;

/**
 * GstMpegtsMiscDescriptorType:
 *
 * The type of #GstMpegtsDescriptor
 *
 * These values correspond to miscellaneous descriptor types that are
 * not yet identified from known specifications.
 */
typedef enum {
  /* 0x80 - 0xFE are user defined */
  GST_MTS_DESC_DTG_LOGICAL_CHANNEL              = 0x83,    /* from DTG D-Book, only present in NIT */
} GstMpegtsMiscDescriptorType;

/**
 * GstMpegtsSCTEDescriptorType:
 *
 * These values correspond to the ones defined by SCTE (amongst other in ANSI/SCTE 57)
 *
 * Since: 1.20
 */
typedef enum {
  GST_MTS_DESC_SCTE_STUFFING                    = 0x80,
  GST_MTS_DESC_SCTE_AC3				= 0x81,
  GST_MTS_DESC_SCTE_FRAME_RATE			= 0x82,
  GST_MTS_DESC_SCTE_EXTENDED_VIDEO		= 0x83,
  GST_MTS_DESC_SCTE_COMPONENT_NAME		= 0x84,
  GST_MTS_DESC_SCTE_FREQUENCY_SPEC		= 0x90,
  GST_MTS_DESC_SCTE_MODULATION_PARAMS		= 0x91,
  GST_MTS_DESC_SCTE_TRANSPORT_STREAM_ID		= 0x92
} GstMpegtsSCTEDescriptorType;



typedef struct _GstMpegtsDescriptor GstMpegtsDescriptor;

#define GST_TYPE_MPEGTS_DESCRIPTOR (gst_mpegts_descriptor_get_type())
GST_MPEGTS_API
GType gst_mpegts_descriptor_get_type (void);

/**
 * GstMpegtsDescriptor:
 * @tag: the type of descriptor
 * @tag_extension: the extended type (if @descriptor_tag is 0x7f)
 * @length: the length of the descriptor content (excluding tag/length field)
 * @data: the full descriptor data (including tag, extension, length). The first
 * two bytes are the @tag and @length.
 *
 * Mpeg-TS descriptor (ISO/IEC 13818-1).
 */
struct _GstMpegtsDescriptor
{
  guint8 tag;
  guint8 tag_extension;
  guint8 length;
  guint8 *data;

  /*< private >*/
  /* Padding for future extension */
  gpointer _gst_reserved[GST_PADDING];
};

GST_MPEGTS_API
void       gst_mpegts_descriptor_free (GstMpegtsDescriptor *desc);

GST_MPEGTS_API
GPtrArray *gst_mpegts_parse_descriptors (guint8 * buffer, gsize buf_len);

GST_MPEGTS_API
const GstMpegtsDescriptor * gst_mpegts_find_descriptor (GPtrArray *descriptors,
							guint8 tag);

GST_MPEGTS_API
const GstMpegtsDescriptor * gst_mpegts_find_descriptor_with_extension (GPtrArray *descriptors,
							guint8 tag, guint8 tag_extension);
/**
 * GstMpegtsRegistrationId:
 * @GST_MTS_REGISTRATION_0: Undefined registration id
 * @GST_MTS_REGISTRATION_AC_3: Audio AC-3, ATSC A/52
 * @GST_MTS_REGISTRATION_AC_4: Audio AC-4, ETSI 103 190-2
 * @GST_MTS_REGISTRATION_CUEI: SCTE 35, "Digital Program Insertion Cueing Message"
 * @GST_MTS_REGISTRATION_drac: Dirac Video codec
 * @GST_MTS_REGISTRATION_DTS1: DTS Audio
 * @GST_MTS_REGISTRATION_DTS2: DTS Audio
 * @GST_MTS_REGISTRATION_DTS3: DTS Audio
 * @GST_MTS_REGISTRATION_EAC3: Enhanced AC-3 (i.e. EAC3)
 * @GST_MTS_REGISTRATION_ETV1: Cablelabs ETV
 * @GST_MTS_REGISTRATION_BSSD: SMPTE 302M, Mapping of AES3 Data in mpeg-ts
 * @GST_MTS_REGISTRATION_GA94: ATSC A/53 compliant stream (i.e. ATSC)
 * @GST_MTS_REGISTRATION_HDMV: Blu-ray, "System Description Blu-ray Disc
 *             Read-Only Format part 3 Audio Visual Basic Specifications" 
 * @GST_MTS_REGISTRATION_KLVA: SMPTE RP217 : Non-synchronized Mapping of KLV
 *             Packets in mpeg-ts
 * @GST_MTS_REGISTRATION_OPUS: Opus Audio
 * @GST_MTS_REGISTRATION_TSHV: HDV (Sony)
 * @GST_MTS_REGISTRATION_VC_1: Video VC-1, SMPTE RP227 "VC-1 Bitstream Transport Encodings"
 * @GST_MTS_REGISTRATION_OTHER_HEVC: HEVC / h265
 *
 * Well-known registration ids, expressed as native-endian 32bit integers. These
 * are used in descriptors of type %GST_MTS_DESC_REGISTRATION. Unless specified
 * otherwise (by use of the "OTHER" prefix), they are all registered by the
 * [SMPTE Registration Authority](https://smpte-ra.org/) or specified in
 * "official" documentation for the given format.
 *
 * Since: 1.20
 */

/**
 * REG_TO_UINT32: (skip) (attributes doc.skip=true)
 */
#define REG_TO_UINT32(a,b,c,d)((a) << 24 | (b) << 16 | (c) << 8 | (d))

typedef enum {
  GST_MTS_REGISTRATION_0 = 0,

  /* SMPTE-RA registered */
  GST_MTS_REGISTRATION_AC_3 = REG_TO_UINT32 ('A', 'C', '-', '3'),
  GST_MTS_REGISTRATION_CUEI = REG_TO_UINT32 ('C', 'U', 'E', 'I'),
  GST_MTS_REGISTRATION_drac = REG_TO_UINT32 ('d', 'r', 'a', 'c'),
  GST_MTS_REGISTRATION_DTS1 = REG_TO_UINT32 ('D', 'T', 'S', '1'),
  GST_MTS_REGISTRATION_DTS2 = REG_TO_UINT32 ('D', 'T', 'S', '2'),
  GST_MTS_REGISTRATION_DTS3 = REG_TO_UINT32 ('D', 'T', 'S', '3'),
  GST_MTS_REGISTRATION_BSSD = REG_TO_UINT32 ('B', 'S', 'S', 'D'),
  GST_MTS_REGISTRATION_EAC3 = REG_TO_UINT32 ('E', 'A', 'C', '3'),
  GST_MTS_REGISTRATION_ETV1 = REG_TO_UINT32 ('E', 'T', 'V', '1'),
  GST_MTS_REGISTRATION_GA94 = REG_TO_UINT32 ('G', 'A', '9', '4'),
  GST_MTS_REGISTRATION_HDMV = REG_TO_UINT32 ('H', 'D', 'M', 'V'),
  GST_MTS_REGISTRATION_KLVA = REG_TO_UINT32 ('K', 'L', 'V', 'A'),
  GST_MTS_REGISTRATION_OPUS = REG_TO_UINT32 ('O', 'P', 'U', 'S'),
  GST_MTS_REGISTRATION_TSHV = REG_TO_UINT32 ('T', 'S', 'H', 'V'),
  GST_MTS_REGISTRATION_VC_1 = REG_TO_UINT32 ('V', 'C', '-', '1'),

  /* Self-registered by formats, but not in SMPTE-RA registry */
  GST_MTS_REGISTRATION_AC_4 = REG_TO_UINT32 ('A', 'C', '-', '4'),

  /* Found elsewhere */
  GST_MTS_REGISTRATION_OTHER_HEVC = REG_TO_UINT32 ('H', 'E', 'V', 'C')
} GstMpegtsRegistrationId;

/* GST_MTS_DESC_REGISTRATION (0x05) */

GST_MPEGTS_API
GstMpegtsDescriptor *gst_mpegts_descriptor_from_registration (
    const gchar *format_identifier,
    guint8 *additional_info, gsize additional_info_length);

GST_MPEGTS_API
gboolean gst_mpegts_descriptor_parse_registration(GstMpegtsDescriptor *descriptor,
						  guint32 *registration_id,
						  guint8 **additional_info,
						  gsize *additional_info_length);

/* GST_MTS_DESC_CA (0x09) */

GST_MPEGTS_API
gboolean  gst_mpegts_descriptor_parse_ca (GstMpegtsDescriptor *descriptor,
					  guint16 *ca_system_id,
					  guint16 *ca_pid,
					  const guint8 **private_data,
					  gsize *private_data_size);

/* GST_MTS_DESC_ISO_639_LANGUAGE (0x0A) */
/**
 * GstMpegtsISO639AudioType:
 *
 * Type of audio streams
 *
 * Defined in ITU H.222.0 Table 2-60
 */
typedef enum {
  GST_MPEGTS_AUDIO_TYPE_UNDEFINED = 0,
  GST_MPEGTS_AUDIO_TYPE_CLEAN_EFFECTS,
  GST_MPEGTS_AUDIO_TYPE_HEARING_IMPAIRED,
  GST_MPEGTS_AUDIO_TYPE_VISUAL_IMPAIRED_COMMENTARY
} GstMpegtsIso639AudioType;

typedef struct _GstMpegtsISO639LanguageDescriptor GstMpegtsISO639LanguageDescriptor;
struct _GstMpegtsISO639LanguageDescriptor
{
  guint                    nb_language;
  gchar                    *language[64];
  GstMpegtsIso639AudioType audio_type[64];
};

#define GST_TYPE_MPEGTS_ISO_639_LANGUAGE (gst_mpegts_iso_639_language_get_type ())
GST_MPEGTS_API
GType gst_mpegts_iso_639_language_get_type (void);

GST_MPEGTS_API
void gst_mpegts_iso_639_language_descriptor_free (GstMpegtsISO639LanguageDescriptor * desc);

GST_MPEGTS_API
gboolean gst_mpegts_descriptor_parse_iso_639_language (const GstMpegtsDescriptor *descriptor,
						       GstMpegtsISO639LanguageDescriptor **res);

GST_MPEGTS_API
gboolean gst_mpegts_descriptor_parse_iso_639_language_idx (const GstMpegtsDescriptor *descriptor,
                                                           guint idx, gchar **lang,
                                                           GstMpegtsIso639AudioType *audio_type);

GST_MPEGTS_API
guint gst_mpegts_descriptor_parse_iso_639_language_nb (const GstMpegtsDescriptor *descriptor);

GST_MPEGTS_API
GstMpegtsDescriptor * gst_mpegts_descriptor_from_iso_639_language (const gchar * language);

GST_MPEGTS_API
gboolean gst_mpegts_descriptor_parse_metadata (const GstMpegtsDescriptor *descriptor, GstMpegtsMetadataDescriptor **res);

GST_MPEGTS_API
gboolean gst_mpegts_descriptor_parse_metadata_std (const GstMpegtsDescriptor *descriptor,
                                                   guint32 *metadata_input_leak_rate,
                                                   guint32 *metadata_buffer_size,
                                                   guint32 *metadata_output_leak_rate);

/* GST_MTS_DESC_DTG_LOGICAL_CHANNEL (0x83) */
typedef struct _GstMpegtsLogicalChannelDescriptor GstMpegtsLogicalChannelDescriptor;
typedef struct _GstMpegtsLogicalChannel GstMpegtsLogicalChannel;

struct _GstMpegtsLogicalChannel
{
  guint16   service_id;
  gboolean  visible_service;
  guint16   logical_channel_number;
};

struct _GstMpegtsLogicalChannelDescriptor
{
  guint                   nb_channels;
  GstMpegtsLogicalChannel channels[64];
};

#define GST_TYPE_MPEGTS_LOGICAL_CHANNEL_DESCRIPTOR (gst_mpegts_logical_channel_descriptor_get_type())

GST_MPEGTS_API
GType gst_mpegts_logical_channel_descriptor_get_type(void);

#define GST_TYPE_MPEGTS_LOGICAL_CHANNEL (gst_mpegts_logical_channel_get_type())

GST_MPEGTS_API
GType gst_mpegts_logical_channel_get_type(void);

/* FIXME : Maybe make two methods. One for getting the number of channels,
 * and the other for getting the content for one channel ? */
GST_MPEGTS_API
gboolean
gst_mpegts_descriptor_parse_logical_channel (const GstMpegtsDescriptor *descriptor,
					     GstMpegtsLogicalChannelDescriptor *res);

GST_MPEGTS_API
GstMpegtsDescriptor *
gst_mpegts_descriptor_from_custom (guint8 tag, const guint8 *data, gsize length);


GST_MPEGTS_API
GstMpegtsDescriptor *
gst_mpegts_descriptor_from_custom_with_extension (guint8 tag, guint8 tag_extension, const guint8 *data, gsize length);

G_END_DECLS

#endif				/* GST_MPEGTS_DESCRIPTOR_H */
