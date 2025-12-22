/*
 * mpegts.h -
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
 */

#ifndef _GST_MPEGTS_PRIVATE_H_
#define _GST_MPEGTS_PRIVATE_H_

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_EXTERN (mpegts_debug);
#define GST_CAT_DEFAULT mpegts_debug

G_GNUC_INTERNAL void __initialize_sections (void);
G_GNUC_INTERNAL void __initialize_descriptors (void);
G_GNUC_INTERNAL guint32 _calc_crc32 (const guint8 *data, guint datalen);
G_GNUC_INTERNAL gchar *get_encoding_and_convert (const gchar *text, guint length);
G_GNUC_INTERNAL gchar *convert_lang_code (guint8 * data);
G_GNUC_INTERNAL guint8 *dvb_text_from_utf8 (const gchar * text, gsize *out_size);
G_GNUC_INTERNAL GstMpegtsDescriptor *_new_descriptor (guint8 tag, guint8 length);
G_GNUC_INTERNAL GstMpegtsDescriptor *_new_descriptor_with_extension (guint8 tag,
    guint8 tag_extension, guint8 length);
G_GNUC_INTERNAL void _packetize_descriptor_array (GPtrArray * array,
    guint8 ** out_data);
G_GNUC_INTERNAL GstMpegtsSection *_gst_mpegts_section_init (guint16 pid, guint8 table_id);
G_GNUC_INTERNAL void _packetize_common_section (GstMpegtsSection * section, gsize length);

typedef gpointer (*GstMpegtsParseFunc) (GstMpegtsSection *section);
G_GNUC_INTERNAL gpointer __common_section_checks (GstMpegtsSection *section,
						  guint minsize,
						  GstMpegtsParseFunc parsefunc,
						  GDestroyNotify destroynotify);

#define __common_desc_check_base(desc, tagtype, retval)			\
  if (G_UNLIKELY ((desc)->data == NULL)) {				\
    GST_WARNING ("Descriptor is empty (data field == NULL)");		\
    return retval;							\
  }									\
  if (G_UNLIKELY ((desc)->tag != (tagtype))) {				\
    GST_WARNING ("Wrong descriptor type (Got 0x%02x, expected 0x%02x)",	\
		 (desc)->tag, tagtype);					\
    return retval;							\
  }									\

#define __common_desc_checks(desc, tagtype, minlen, retval)		\
  __common_desc_check_base(desc, tagtype, retval);			\
  if (G_UNLIKELY ((desc)->length < (minlen))) {				\
    GST_WARNING ("Descriptor too small (Got %d, expected at least %d)",	\
		 (desc)->length, minlen);				\
    return retval;							\
  }
#define __common_desc_checks_exact(desc, tagtype, len, retval)		\
  __common_desc_check_base(desc, tagtype, retval);			\
  if (G_UNLIKELY ((desc)->length != (len))) {				\
    GST_WARNING ("Wrong descriptor size (Got %d, expected %d)",		\
		 (desc)->length, len);					\
    return retval;							\
  }

#define __common_desc_ext_check_base(desc, tagexttype, retval)          \
  if (G_UNLIKELY ((desc)->data == NULL)) {                              \
    GST_WARNING ("Descriptor is empty (data field == NULL)");           \
    return retval;                                                      \
  }                                                                     \
  if (G_UNLIKELY ((desc)->tag != 0x7f) ||                               \
    ((desc)->tag_extension != (tagexttype))) {                          \
    GST_WARNING ("Wrong descriptor type (Got 0x%02x, expected 0x%02x)", \
                 (desc)->tag_extension, tagexttype);                    \
    return retval;                                                      \
  }
#define __common_desc_ext_checks(desc, tagexttype, minlen, retval)      \
  __common_desc_ext_check_base(desc, tagexttype, retval);                    \
  if (G_UNLIKELY ((desc)->length < (minlen))) {                         \
    GST_WARNING ("Descriptor too small (Got %d, expected at least %d)", \
                 (desc)->length, minlen);                               \
    return retval;                                                      \
  }
#define __common_desc_ext_checks_exact(desc, tagexttype, len, retval)   \
  __common_desc_ext_check_base(desc, tagexttype, retval);               \
  if (G_UNLIKELY ((desc)->length != (len))) {                           \
     GST_WARNING ("Wrong descriptor size (Got %d, expected %d)",        \
                  (desc)->length, len);                                 \
     return retval;                                                     \
  }

G_END_DECLS

#endif	/* _GST_MPEGTS_PRIVATE_H_ */
