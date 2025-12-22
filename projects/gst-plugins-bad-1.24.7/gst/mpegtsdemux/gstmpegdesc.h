/* 
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
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 *
 */

#ifndef __GST_MPEG_DESC_H__
#define __GST_MPEG_DESC_H__

#include <glib.h>


/* Others */


/* 0xFF is forbidden */

/* common for all descriptors */
#define DESC_TAG(desc) 		(desc[0])
#define DESC_LENGTH(desc) 	(desc[1])

/* video_stream_descriptor */
#define DESC_VIDEO_STREAM_multiple_framerate_flag(desc) 	(((desc)[2] & 0x80) == 0x80)
#define DESC_VIDEO_STREAM_frame_rate_code(desc) 		(((desc)[2] & 0x38) >> 3)
#define DESC_VIDEO_STREAM_MPEG_1_only_flag(desc) 		(((desc)[2] & 0x04) == 0x04)
#define DESC_VIDEO_STREAM_constrained_parameter_flag(desc) 	(((desc)[2] & 0x02) == 0x02)
#define DESC_VIDEO_STREAM_still_picture_flag(desc) 	 	(((desc)[2] & 0x01) == 0x01)
/* if (MPEG_1_only_flag == 1) */
#define DESC_VIDEO_STREAM_profile_and_level_indication(desc)	((desc)[3])
#define DESC_VIDEO_STREAM_chroma_format(desc)			(((desc)[4] & 0xc0) >> 6)
#define DESC_VIDEO_STREAM_frame_rate_extension_flag(desc)	(((desc)[4] & 0x20) == 0x20)

/* audio_stream_descriptor */
#define DESC_AUDIO_STREAM_free_format_flag(desc) 		(((desc)[2] & 0x80) == 0x80)
#define DESC_AUDIO_STREAM_ID(desc) 				(((desc)[2] & 0x40) == 0x40)
#define DESC_AUDIO_STREAM_layer(desc) 				(((desc)[2] & 0x30) >> 4)
#define DESC_AUDIO_STREAM_variable_rate_audio_indicator(desc) 	(((desc)[2] & 0x08) == 0x08)

/* hierarchy_descriptor */
#define DESC_HIERARCHY_hierarchy_type(desc)			(((desc)[2] & 0x0f))
#define DESC_HIERARCHY_hierarchy_layer_index(desc)		(((desc)[3] & 0x3f))
#define DESC_HIERARCHY_hierarchy_embedded_layer_index(desc)	(((desc)[4] & 0x3f))
#define DESC_HIERARCHY_hierarchy_channel(desc)			(((desc)[5] & 0x3f))

/* registration_descriptor */
#define DESC_REGISTRATION_format_identifier(desc)		(GST_READ_UINT32_BE ((desc)+2))
#define DESC_REGISTRATION_additional_ident_info_len(desc)	((desc)[1] - 4)
#define DESC_REGISTRATION_additional_ident_info(desc)		(&(desc)[6])

/* data_stream_alignment_descriptor */
#define DESC_DATA_STREAM_ALIGNMENT_alignment_type(desc)		((desc)[2])

/* target_background_grid_descriptor */
#define DESC_TARGET_BACKGROUND_GRID_horizontal_size(desc)	(GST_READ_UINT16_BE ((desc)+2) >> 2)
#define DESC_TARGET_BACKGROUND_GRID_vertical_size(desc)		((GST_READ_UINT32_BE ((desc)+2) & 0x0003fff0) >> 4)
#define DESC_TARGET_BACKGROUND_GRID_aspect_ratio_information(desc) ((desc)[5] & 0x0f)

/* video_window_descriptor */
#define DESC_VIDEO_WINDOW_horizontal_offset(desc)		(GST_READ_UINT16_BE ((desc)+2) >> 2)
#define DESC_VIDEO_WINDOW_vertical_offset(desc)			((GST_READ_UINT32_BE ((desc)+2) & 0x0003fff0) >> 4)
#define DESC_VIDEO_WINDOW_window_priority(desc)	 		((desc)[5] & 0x0f)

/* CA_descriptor */
#define DESC_CA_system_ID(desc)					(GST_READ_UINT16_BE ((desc)+2))
#define DESC_CA_PID(desc)					(GST_READ_UINT16_BE ((desc)+2) & 0x1fff)

/* ISO_639_language_descriptor */
#define DESC_ISO_639_LANGUAGE_codes_n(desc)			((desc[1]) >> 2)
#define DESC_ISO_639_LANGUAGE_language_code_nth(desc,i)		(&(desc[2 + (4*i)]))
#define DESC_ISO_639_LANGUAGE_audio_type_nth(desc,i)		((desc)[5 + (4*i)])

/* system_clock_descriptor */
#define DESC_SYSTEM_CLOCK_external_clock_reference_indicator(desc) (((desc)[2] & 0x80) == 0x80)
#define DESC_SYSTEM_CLOCK_clock_accuracy_integer(desc)		((desc)[2] & 0x3f)
#define DESC_SYSTEM_CLOCK_clock_accuracy_exponent(desc)		(((desc)[3] & 0xe0) >> 5)

/* multiplex_buffer_utilization_descriptor */
#define DESC_MULTIPLEX_BUFFER_UTILISATION_bound_valid_flag(desc)	(((desc)[2] & 0x80) == 0x80)
#define DESC_MULTIPLEX_BUFFER_UTILISATION_LTW_offset_lower_bound(desc)	(GST_READ_UINT16_BE ((desc)+2) & 0x7fff)
#define DESC_MULTIPLEX_BUFFER_UTILISATION_LTW_offset_upper_bound(desc)	(GST_READ_UINT16_BE ((desc)+4) & 0x7fff)

/* copyright_descriptor */
#define DESC_COPYRIGHT_copyright_identifier(desc)		(GST_READ_UINT32_BE ((desc)+2))
#define DESC_COPYRIGHT_additional_copyright_info_len(desc)	((desc)[1] - 4)
#define DESC_COPYRIGHT_additional_copyright_info(desc)		(&(desc)[6])

/* maximum_bitrate_descriptor */
#define DESC_MAXIMUM_BITRAT_maximum_bitrate(desc)		(((((guint32)desc[2]) & 0x3f) << 16) | \
								 GST_READ_UINT16_BE ((desc)+3))

/* private_data_indicator_descriptor */
#define DESC_PRIVATE_DATA_INDICATOR_indicator(desc)		(GST_READ_UINT32_BE(&desc[2]))

/* smoothing_buffer_descriptor */
#define DESC_SMOOTHING_BUFFER_sb_leak_rate(desc)		(((((guint32)desc[2]) & 0x3f) << 16) | \
                                                                 GST_READ_UINT16_BE ((desc)+3))
#define DESC_SMOOTHING_BUFFER_sb_size(desc)			(((((guint32)desc[5]) & 0x3f) << 16) | \
                                                                 GST_READ_UINT16_BE ((desc)+6))
/* STD_descriptor */
#define DESC_STD_leak_valid_flag(desc)				(((desc)[2] & 0x01) == 0x01)

/* ibp_descriptor */
#define DESC_IBP_closed_gop_flag(desc)				(((desc)[2] & 0x80) == 0x80)
#define DESC_IBP_identical_gop_flag(desc)			(((desc)[2] & 0x40) == 0x40)
#define DESC_IBP_max_gop_length(desc)				(GST_READ_UINT16_BE ((desc)+6) & 0x3fff)

/* time_code descriptor */
#define DESC_TIMECODE_video_pid(desc)                           (GST_READ_UINT16_BE ((desc) + 2) & 0x1fff)

/* Stream identifier descriptor */
#define DESC_DVB_STREAM_IDENTIFIER_component_tag(desc)  (desc[2])

/* DVB Network Name descriptor */
#define DESC_DVB_NETWORK_NAME_length(desc)  (GST_READ_UINT8((desc)+1))
#define DESC_DVB_NETWORK_NAME_text(desc)    (desc+2)

/* DVB Service Descriptor */
#define DESC_DVB_SERVICE_type(desc) (desc[2])
#define DESC_DVB_SERVICE_provider_name_length(desc) (desc[3])
#define DESC_DVB_SERVICE_provider_name_text(desc)   (desc+4)
#define DESC_DVB_SERVICE_name_length(desc)  (desc[4 + DESC_DVB_SERVICE_provider_name_length(desc)])
#define DESC_DVB_SERVICE_name_text(desc)    (desc + 5 + DESC_DVB_SERVICE_provider_name_length(desc))

/* DVB Component Descriptor */
#define DESC_DVB_COMPONENT_stream_content(desc) (desc[2] & 0x0F)
#define DESC_DVB_COMPONENT_type(desc)   (desc[3])
#define DESC_DVB_COMPONENT_tag(desc)    (desc[4])
#define DESC_DVB_COMPONENT_language(desc)   (desc + 5)

/* DVB Bouquet Name Descriptor */
#define DESC_DVB_BOUQUET_NAME_text(desc)    (desc + 2)

/* DVB Short Event Descriptor */
#define DESC_DVB_SHORT_EVENT_name_text(desc)	(desc + 6)
#define DESC_DVB_SHORT_EVENT_name_length(desc)	(desc[5])
#define DESC_DVB_SHORT_EVENT_description_text(desc) (desc + 6 + DESC_DVB_SHORT_EVENT_name_length(desc) + 1)
#define DESC_DVB_SHORT_EVENT_description_length(desc)	(desc[6 + DESC_DVB_SHORT_EVENT_name_length(desc)])

/* DVB Extended Event Descriptor */
#define DESC_DVB_EXTENDED_EVENT_descriptor_number(desc) ((desc[2] & 0xF0) >> 4)
#define DESC_DVB_EXTENDED_EVENT_last_descriptor_number(desc) (desc[2] & 0x0F)
#define DESC_DVB_EXTENDED_EVENT_iso639_language_code(desc) (desc + 3)
#define DESC_DVB_EXTENDED_EVENT_items_length(desc) (desc[6])
#define DESC_DVB_EXTENDED_EVENT_items(desc) (desc + 7)
#define DESC_DVB_EXTENDED_EVENT_text_length(desc) (desc[7 + DESC_DVB_EXTENDED_EVENT_items_length(desc)])
#define DESC_DVB_EXTENDED_EVENT_text(desc) (desc + 7 + DESC_DVB_EXTENDED_EVENT_items_length(desc) + 1)

/* DVB Satellite Delivery System Descriptor */
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_frequency(desc)	(desc + 2)
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_orbital_position(desc)	(desc + 6)
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_west_east_flag(desc)	((desc[8] & 0x80) == 0x80)
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_polarization(desc)	((desc[8] >> 5) & 0x3)
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_modulation(desc)	(desc[8] & 0x3)
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_symbol_rate(desc)	(desc + 9)
#define DESC_DVB_SATELLITE_DELIVERY_SYSTEM_fec_inner(desc)	(desc[12] & 0x0F)

/* DVB Terrestrial Delivery System Descriptor */
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_frequency(desc)	(GST_READ_UINT32_BE((desc) + 2))
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_bandwidth(desc)	((desc[6] >> 5) & 0x7)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_constellation(desc)	((desc[7] >> 6) & 0x3)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_hierarchy(desc)	((desc[7] >> 3) & 0x7)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_code_rate_hp(desc)	(desc[7] & 0x7)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_code_rate_lp(desc)	((desc[8] >> 5) & 0x7)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_guard_interval(desc)	((desc[8] >> 3) & 0x3)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_transmission_mode(desc)	((desc[8] >> 1) & 0x3)
#define DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM_other_frequency(desc)	((desc[8] & 0x01) == 0x01)

/* DVB Cable Delivery System Descriptor */
#define DESC_DVB_CABLE_DELIVERY_SYSTEM_frequency(desc)		(desc + 2)
#define DESC_DVB_CABLE_DELIVERY_SYSTEM_fec_outer(desc)		(desc[7] & 0x0F)
#define DESC_DVB_CABLE_DELIVERY_SYSTEM_modulation(desc)		(desc[8])
#define DESC_DVB_CABLE_DELIVERY_SYSTEM_symbol_rate(desc)	(desc + 9)
#define DESC_DVB_CABLE_DELIVERY_SYSTEM_fec_inner(desc)		(desc[12] & 0x0F)

/* DVB Data Broadcast Descriptor */
#define DESC_DVB_DATA_BROADCAST_data_broadcast_id(desc)		(GST_READ_UINT16_BE((desc) + 2))
#define DESC_DVB_DATA_BROADCAST_component_tag(desc)		(desc[4])
#define DESC_DVB_DATA_BROADCAST_selector_length(desc)		(desc[5])
#define DESC_DVB_DATA_BROADCAST_selector(desc)			(desc + 6)
#define DESC_DVB_DATA_BROADCAST_iso639_language_code(desc)	(desc + 6 + DESC_DVB_DATA_BROADCAST_selector_length(desc))
#define DESC_DVB_DATA_BROADCAST_text_length(desc)		(desc + 9 + DESC_DVB_DATA_BROADCAST_selector_length(desc))
#define DESC_DVB_DATA_BROADCAST_text(desc)			(desc + 10 + DESC_DVB_DATA_BROADCAST_selector_length(desc))

/* DVB Data Broadcast Id Descriptor */
#define DESC_DVB_DATA_BROADCAST_ID_data_broadcast_id(desc)	(GST_READ_UINT16_BE((desc) + 2))
#define DESC_DVB_DATA_BROADCAST_ID_id_selector_byte(desc)	(desc + 4)

/* DVB Carousel Identifier Descriptor */
#define DESC_DVB_CAROUSEL_IDENTIFIER_carousel_id(desc)		(GST_READ_UINT32_BE((desc) + 2))

/* AC3_audio_stream_descriptor */
#define DESC_AC_AUDIO_STREAM_bsid(desc)             ((desc)[2] & 0x1f)
#define DESC_AC_AUDIO_STREAM_channels(desc)         ((desc[4] >> 1) & 0x0f)
#define DESC_AC_AUDIO_STREAM_textlen(desc)          ((desc[7 + (DESC_AC_AUDIO_STREAM_channels(desc)==0)] >> 1) & 0x7f)
#define DESC_AC_AUDIO_STREAM_has_lang1(desc)        ((desc[8 + DESC_AC_AUDIO_STREAM_textlen(desc)] >> 7) & 0x01)
#define DESC_AC_AUDIO_STREAM_lang1_code(desc)       (desc + 9 + DESC_AC_AUDIO_STREAM_textlen(desc))

/* FIXME : Move list of well know registration ids to an enum
 * in the mpegts library.
 *
 * See http://www.smpte-ra.org/mpegreg/mpegreg.html for a full list
 * */

/* registration_descriptor format IDs */
#define DRF_ID_HDMV       0x48444d56
#define DRF_ID_VC1        0x56432D31   /* defined in RP227 */
#define DRF_ID_DTS1       0x44545331
#define DRF_ID_DTS2       0x44545332
#define DRF_ID_DTS3       0x44545333
#define DRF_ID_S302M      0x42535344
#define DRF_ID_TSHV       0x54534856
#define DRF_ID_AC3        0x41432d33
#define DRF_ID_GA94       0x47413934
#define DRF_ID_CUEI       0x43554549
#define DRF_ID_ETV1       0x45545631
#define DRF_ID_HEVC       0x48455643
#define DRF_ID_KLVA       0x4b4c5641   /* defined in RP217 */
#define DRF_ID_OPUS       0x4f707573
#define DRF_ID_EAC3       0x45414333   /* defined in A/52 Annex G */
#define DRF_ID_AC4        0x41432D34   /* defined in ETSI TS 103 190-2 Annex D */

#endif /* __GST_MPEG_DESC_H__ */
