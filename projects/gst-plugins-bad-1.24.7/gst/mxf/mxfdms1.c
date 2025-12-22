/* GStreamer
 * Copyright (C) 2008-2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

/* Implementation of SMPTE S380M - Descriptive Metadata Scheme-1 */

/* TODO:
 *   - What are the "locators"?
 *   - Create sensible tags from this
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#include "mxfdms1.h"
#include "mxftypes.h"

GST_DEBUG_CATEGORY_EXTERN (mxf_debug);
#define GST_CAT_DEFAULT mxf_debug

G_DEFINE_ABSTRACT_TYPE (MXFDMS1, mxf_dms1, MXF_TYPE_DESCRIPTIVE_METADATA);

static gboolean
mxf_dms1_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1 *self = MXF_DMS1 (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 instance_uid_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x15, 0x02, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 generation_uid_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x05,
    0x20, 0x07, 0x01, 0x08, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &instance_uid_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;
    memcpy (&MXF_METADATA_BASE (self)->instance_uid, tag_data, 16);
    GST_DEBUG ("  instance uid = %s",
        mxf_uuid_to_string (&MXF_METADATA_BASE (self)->instance_uid, str));
  } else if (memcmp (tag_ul, &generation_uid_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;
    memcpy (&MXF_METADATA_BASE (self)->generation_uid, tag_data, 16);
    GST_DEBUG ("  generation uid = %s",
        mxf_uuid_to_string (&MXF_METADATA_BASE (self)->generation_uid, str));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_init (MXFDMS1 * self)
{
}

static void
mxf_dms1_class_init (MXFDMS1Class * klass)
{
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  metadatabase_class->handle_tag = mxf_dms1_handle_tag;
  dm_class->scheme = 0x01;
}

G_DEFINE_ABSTRACT_TYPE (MXFDMS1TextLanguage, mxf_dms1_text_language,
    MXF_TYPE_DMS1);

static gboolean
mxf_dms1_text_language_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1TextLanguage *self = MXF_DMS1_TEXT_LANGUAGE (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 extended_text_language_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x01, 0x01, 0x02, 0x02, 0x11, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &extended_text_language_code_ul, 16) == 0) {
    if (tag_size > 12)
      goto error;

    memcpy (self->extended_text_language_code, tag_data, tag_size);
    GST_DEBUG ("  extended text language code = %s",
        self->extended_text_language_code);
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_text_language_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 text language local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_text_language_init (MXFDMS1TextLanguage * self)
{
}

static void
mxf_dms1_text_language_class_init (MXFDMS1TextLanguageClass * klass)
{
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;

  metadatabase_class->handle_tag = mxf_dms1_text_language_handle_tag;
}

G_DEFINE_ABSTRACT_TYPE (MXFDMS1Thesaurus, mxf_dms1_thesaurus,
    MXF_TYPE_DMS1_TEXT_LANGUAGE);

static void
mxf_dms1_thesaurus_finalize (GObject * object)
{
  MXFDMS1Thesaurus *self = MXF_DMS1_THESAURUS (object);

  g_free (self->thesaurus_name);
  self->thesaurus_name = NULL;

  G_OBJECT_CLASS (mxf_dms1_thesaurus_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_thesaurus_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Thesaurus *self = MXF_DMS1_THESAURUS (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 thesaurus_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x01, 0x02, 0x02, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &thesaurus_name_ul, 16) == 0) {
    self->thesaurus_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  thesaurus name  = %s", GST_STR_NULL (self->thesaurus_name));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_thesaurus_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_thesaurus_init (MXFDMS1Thesaurus * self)
{
}

static void
mxf_dms1_thesaurus_class_init (MXFDMS1ThesaurusClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;

  object_class->finalize = mxf_dms1_thesaurus_finalize;
  metadatabase_class->handle_tag = mxf_dms1_thesaurus_handle_tag;
}

static void
mxf_dms1_framework_interface_init (gpointer g_iface, gpointer iface_data)
{
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (MXFDMS1Framework, mxf_dms1_framework,
    MXF_TYPE_DMS1,
    G_IMPLEMENT_INTERFACE (MXF_TYPE_DESCRIPTIVE_METADATA_FRAMEWORK,
        mxf_dms1_framework_interface_init));

static void
mxf_dms1_framework_finalize (GObject * object)
{
  MXFDMS1Framework *self = MXF_DMS1_FRAMEWORK (object);

  g_free (self->framework_thesaurus_name);
  self->framework_thesaurus_name = NULL;

  g_free (self->framework_title);
  self->framework_title = NULL;

  g_free (self->metadata_server_locators_uids);
  self->metadata_server_locators_uids = NULL;

  g_free (self->titles_sets_uids);
  self->titles_sets_uids = NULL;

  g_free (self->titles_sets);
  self->titles_sets = NULL;

  g_free (self->annotation_sets_uids);
  self->annotation_sets_uids = NULL;

  g_free (self->annotation_sets);
  self->annotation_sets = NULL;

  g_free (self->participant_sets_uids);
  self->participant_sets_uids = NULL;

  g_free (self->participant_sets);
  self->participant_sets = NULL;

  g_free (self->location_sets_uids);
  self->location_sets_uids = NULL;

  g_free (self->location_sets);
  self->location_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_framework_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_framework_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Framework *self = MXF_DMS1_FRAMEWORK (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->titles_sets)
    memset (self->titles_sets, 0, sizeof (gpointer) * self->n_titles_sets);
  else
    self->titles_sets = g_new0 (MXFDMS1Titles *, self->n_titles_sets);

  if (self->annotation_sets)
    memset (self->annotation_sets, 0,
        sizeof (gpointer) * self->n_annotation_sets);
  else
    self->annotation_sets =
        g_new0 (MXFDMS1Annotation *, self->n_annotation_sets);

  if (self->participant_sets)
    memset (self->participant_sets, 0,
        sizeof (gpointer) * self->n_participant_sets);
  else
    self->participant_sets =
        g_new0 (MXFDMS1Participant *, self->n_participant_sets);

  if (self->location_sets)
    memset (self->location_sets, 0, sizeof (gpointer) * self->n_location_sets);
  else
    self->location_sets = g_new0 (MXFDMS1Location *, self->n_location_sets);

  for (i = 0; i < self->n_titles_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->titles_sets_uids[i]);

    if (current && MXF_IS_DMS1_TITLES (current)) {
      self->titles_sets[i] = MXF_DMS1_TITLES (current);
    }
  }

  for (i = 0; i < self->n_annotation_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->annotation_sets_uids[i]);
    if (current && MXF_IS_DMS1_ANNOTATION (current)) {
      self->annotation_sets[i] = MXF_DMS1_ANNOTATION (current);
    }
  }

  for (i = 0; i < self->n_participant_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->participant_sets_uids[i]);
    if (current && MXF_IS_DMS1_PARTICIPANT (current)) {
      self->participant_sets[i] = MXF_DMS1_PARTICIPANT (current);
    }
  }

  current = g_hash_table_lookup (metadata, &self->contacts_list_set_uid);
  if (current && MXF_IS_DMS1_CONTACTS_LIST (current)) {
    self->contacts_list_set = MXF_DMS1_CONTACTS_LIST (current);
  }

  for (i = 0; i < self->n_location_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->location_sets_uids[i]);
    if (current && MXF_IS_DMS1_LOCATION (current)) {
      self->location_sets[i] = MXF_DMS1_LOCATION (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_framework_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_framework_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Framework *self = MXF_DMS1_FRAMEWORK (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 framework_extended_text_language_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x01, 0x01, 0x02, 0x02, 0x13, 0x00, 0x00
  };
  static const guint8 framework_thesaurus_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x02, 0x15, 0x01, 0x00, 0x00
  };
  static const guint8 framework_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x05, 0x0f, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 primary_extended_spoken_language_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x01, 0x01, 0x02, 0x03, 0x11, 0x00, 0x00
  };
  static const guint8 secondary_extended_spoken_language_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x01, 0x01, 0x02, 0x03, 0x12, 0x00, 0x00
  };
  static const guint8 original_extended_spoken_language_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x01, 0x01, 0x02, 0x03, 0x13, 0x00, 0x00
  };
  static const guint8 metadata_server_locators_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x06, 0x0C, 0x00, 0x00
  };
  static const guint8 titles_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x04, 0x00
  };
  static const guint8 annotation_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0d, 0x00
  };
  static const guint8 participant_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x13, 0x00
  };
  static const guint8 contacts_list_set_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x02, 0x40, 0x22, 0x00
  };
  static const guint8 location_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x16, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &framework_extended_text_language_code_ul, 16) == 0) {
    if (tag_size > 12)
      goto error;
    memcpy (&self->framework_extended_text_language_code, tag_data, tag_size);
    GST_DEBUG ("  framework extended text language code = %s",
        self->framework_extended_text_language_code);
  } else if (memcmp (tag_ul, &framework_thesaurus_name_ul, 16) == 0) {
    self->framework_thesaurus_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  framework thesaurus name = %s",
        GST_STR_NULL (self->framework_thesaurus_name));
  } else if (memcmp (tag_ul, &framework_title_ul, 16) == 0) {
    self->framework_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  framework title = %s", GST_STR_NULL (self->framework_title));
  } else if (memcmp (tag_ul, &primary_extended_spoken_language_code_ul,
          16) == 0) {
    if (tag_size > 12)
      goto error;
    memcpy (&self->primary_extended_spoken_language_code, tag_data, tag_size);
    GST_DEBUG ("  primary extended spoken language code = %s",
        self->primary_extended_spoken_language_code);
  } else if (memcmp (tag_ul, &secondary_extended_spoken_language_code_ul,
          16) == 0) {
    if (tag_size > 12)
      goto error;
    memcpy (&self->secondary_extended_spoken_language_code, tag_data, tag_size);
    GST_DEBUG ("  secondary extended spoken language code = %s",
        self->secondary_extended_spoken_language_code);
  } else if (memcmp (tag_ul, &original_extended_spoken_language_code_ul,
          16) == 0) {
    if (tag_size > 12)
      goto error;
    memcpy (&self->original_extended_spoken_language_code, tag_data, tag_size);
    GST_DEBUG ("  original extended spoken language code = %s",
        self->original_extended_spoken_language_code);
  } else if (memcmp (tag_ul, &metadata_server_locators_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->metadata_server_locators_uids,
            &self->n_metadata_server_locators, tag_data, tag_size))
      goto error;

    GST_DEBUG ("  number of metadata server locators = %u",
        self->n_metadata_server_locators);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_metadata_server_locators; i++) {
        GST_DEBUG ("    metadata server locator %u = %s", i,
            mxf_uuid_to_string (&self->metadata_server_locators_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &titles_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->titles_sets_uids, &self->n_titles_sets,
            tag_data, tag_size))
      goto error;

    GST_DEBUG ("  number of titles sets = %u", self->n_titles_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_titles_sets; i++) {
        GST_DEBUG ("    titles sets %u = %s", i,
            mxf_uuid_to_string (&self->titles_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &annotation_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->annotation_sets_uids,
            &self->n_annotation_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of annotation sets = %u", self->n_annotation_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_annotation_sets; i++) {
        GST_DEBUG ("    annotation sets %u = %s", i,
            mxf_uuid_to_string (&self->annotation_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &participant_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->participant_sets_uids,
            &self->n_participant_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of participant sets = %u", self->n_participant_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_participant_sets; i++) {
        GST_DEBUG ("    participant sets %u = %s", i,
            mxf_uuid_to_string (&self->participant_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &contacts_list_set_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->contacts_list_set_uid, tag_data, 16);
    GST_DEBUG ("  contacts list = %s",
        mxf_uuid_to_string (&self->contacts_list_set_uid, str));
  } else if (memcmp (tag_ul, &location_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->location_sets_uids,
            &self->n_location_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of location sets = %u", self->n_location_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_location_sets; i++) {
        GST_DEBUG ("    location sets %u = %s", i,
            mxf_uuid_to_string (&self->location_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_framework_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 framework local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_framework_init (MXFDMS1Framework * self)
{
}

static void
mxf_dms1_framework_class_init (MXFDMS1FrameworkClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;

  object_class->finalize = mxf_dms1_framework_finalize;
  metadatabase_class->handle_tag = mxf_dms1_framework_handle_tag;
  metadatabase_class->resolve = mxf_dms1_framework_resolve;
}

G_DEFINE_ABSTRACT_TYPE (MXFDMS1ProductionClipFramework,
    mxf_dms1_production_clip_framework, MXF_TYPE_DMS1_FRAMEWORK);

static void
mxf_dms1_production_clip_framework_finalize (GObject * object)
{
  MXFDMS1ProductionClipFramework *self =
      MXF_DMS1_PRODUCTION_CLIP_FRAMEWORK (object);

  g_free (self->captions_description_sets_uids);
  self->captions_description_sets_uids = NULL;

  g_free (self->captions_description_sets);
  self->captions_description_sets = NULL;

  g_free (self->contract_sets_uids);
  self->contract_sets_uids = NULL;

  g_free (self->contract_sets);
  self->contract_sets = NULL;

  G_OBJECT_CLASS
      (mxf_dms1_production_clip_framework_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_production_clip_framework_resolve (MXFMetadataBase * m,
    GHashTable * metadata)
{
  MXFDMS1ProductionClipFramework *self = MXF_DMS1_PRODUCTION_CLIP_FRAMEWORK (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->captions_description_sets)
    memset (self->captions_description_sets, 0,
        sizeof (gpointer) * self->n_captions_description_sets);
  else
    self->captions_description_sets =
        g_new0 (MXFDMS1CaptionsDescription *,
        self->n_captions_description_sets);

  if (self->contract_sets)
    memset (self->contract_sets, 0,
        sizeof (gpointer) * self->n_captions_description_sets);
  else
    self->contract_sets = g_new0 (MXFDMS1Contract *, self->n_contract_sets);

  current = g_hash_table_lookup (metadata, &self->picture_format_set_uid);
  if (current && MXF_IS_DMS1_PICTURE_FORMAT (current)) {
    self->picture_format = MXF_DMS1_PICTURE_FORMAT (current);
  }

  for (i = 0; i < self->n_captions_description_sets; i++) {
    current =
        g_hash_table_lookup (metadata,
        &self->captions_description_sets_uids[i]);
    if (current && MXF_IS_DMS1_CAPTIONS_DESCRIPTION (current)) {
      self->captions_description_sets[i] =
          MXF_DMS1_CAPTIONS_DESCRIPTION (current);
    }
  }

  for (i = 0; i < self->n_contract_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->contract_sets_uids[i]);
    if (current && MXF_IS_DMS1_CONTRACT (current)) {
      self->contract_sets[i] = MXF_DMS1_CONTRACT (current);
    }
  }

  current = g_hash_table_lookup (metadata, &self->project_set_uid);
  if (current && MXF_IS_DMS1_PROJECT (current)) {
    self->project_set = MXF_DMS1_PROJECT (current);
  }

  return
      MXF_METADATA_BASE_CLASS
      (mxf_dms1_production_clip_framework_parent_class)->resolve (m, metadata);
}

static gboolean
mxf_dms1_production_clip_framework_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1ProductionClipFramework *self =
      MXF_DMS1_PRODUCTION_CLIP_FRAMEWORK (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 picture_format_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x02, 0x40, 0x1d, 0x00
  };
  static const guint8 captions_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0c, 0x00
  };
  static const guint8 contract_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x19, 0x00
  };
  static const guint8 project_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x02, 0x40, 0x21, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &picture_format_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->picture_format_set_uid, tag_data, 16);
    GST_DEBUG ("  picture format set = %s",
        mxf_uuid_to_string (&self->picture_format_set_uid, str));
  } else if (memcmp (tag_ul, &captions_description_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->captions_description_sets_uids,
            &self->n_captions_description_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of captions description sets = %u",
        self->n_captions_description_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_captions_description_sets; i++) {
        GST_DEBUG ("    captions description sets %u = %s", i,
            mxf_uuid_to_string (&self->captions_description_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &contract_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->contract_sets_uids,
            &self->n_contract_sets, tag_data, tag_size))
      goto error;

    GST_DEBUG ("  number of contract sets = %u", self->n_contract_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_contract_sets; i++) {
        GST_DEBUG ("    contract sets %u = %s", i,
            mxf_uuid_to_string (&self->contract_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &project_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->project_set_uid, tag_data, 16);
    GST_DEBUG ("  project set = %s", mxf_uuid_to_string (&self->project_set_uid,
            str));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_production_clip_framework_parent_class)->handle_tag (metadata,
        primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR
      ("Invalid DMS1 production-clip framework local tag 0x%04x of size %u",
      tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_production_clip_framework_init (MXFDMS1ProductionClipFramework * self)
{
}

static void
    mxf_dms1_production_clip_framework_class_init
    (MXFDMS1ProductionClipFrameworkClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;

  object_class->finalize = mxf_dms1_production_clip_framework_finalize;
  metadatabase_class->handle_tag =
      mxf_dms1_production_clip_framework_handle_tag;
  metadatabase_class->resolve = mxf_dms1_production_clip_framework_resolve;
}

G_DEFINE_TYPE (MXFDMS1ProductionFramework, mxf_dms1_production_framework,
    MXF_TYPE_DMS1_PRODUCTION_CLIP_FRAMEWORK);

static void
mxf_dms1_production_framework_finalize (GObject * object)
{
  MXFDMS1ProductionFramework *self = MXF_DMS1_PRODUCTION_FRAMEWORK (object);

  g_free (self->integration_indication);
  self->integration_indication = NULL;

  g_free (self->identification_sets_uids);
  self->identification_sets_uids = NULL;

  g_free (self->identification_sets);
  self->identification_sets = NULL;

  g_free (self->group_relationship_sets_uids);
  self->group_relationship_sets_uids = NULL;

  g_free (self->group_relationship_sets);
  self->group_relationship_sets = NULL;

  g_free (self->branding_sets_uids);
  self->branding_sets_uids = NULL;

  g_free (self->branding_sets);
  self->branding_sets = NULL;

  g_free (self->event_sets_uids);
  self->event_sets_uids = NULL;

  g_free (self->event_sets);
  self->event_sets = NULL;

  g_free (self->award_sets_uids);
  self->award_sets_uids = NULL;

  g_free (self->award_sets);
  self->award_sets = NULL;

  g_free (self->setting_period_sets_uids);
  self->setting_period_sets_uids = NULL;

  g_free (self->setting_period_sets);
  self->setting_period_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_production_framework_parent_class)->finalize
      (object);
}

static gboolean
mxf_dms1_production_framework_resolve (MXFMetadataBase * m,
    GHashTable * metadata)
{
  MXFDMS1ProductionFramework *self = MXF_DMS1_PRODUCTION_FRAMEWORK (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->identification_sets)
    memset (self->identification_sets, 0,
        sizeof (gpointer) * self->n_identification_sets);
  else
    self->identification_sets =
        g_new0 (MXFDMS1Identification *, self->n_identification_sets);

  if (self->group_relationship_sets)
    memset (self->group_relationship_sets, 0,
        sizeof (gpointer) * self->n_group_relationship_sets);
  else
    self->group_relationship_sets =
        g_new0 (MXFDMS1GroupRelationship *, self->n_group_relationship_sets);

  if (self->branding_sets)
    memset (self->branding_sets, 0, sizeof (gpointer) * self->n_branding_sets);
  else
    self->branding_sets = g_new0 (MXFDMS1Branding *, self->n_branding_sets);

  if (self->event_sets)
    memset (self->event_sets, 0, sizeof (gpointer) * self->n_event_sets);
  else
    self->event_sets = g_new0 (MXFDMS1Event *, self->n_event_sets);

  if (self->award_sets)
    memset (self->award_sets, 0, sizeof (gpointer) * self->n_award_sets);
  else
    self->award_sets = g_new0 (MXFDMS1Award *, self->n_award_sets);

  if (self->setting_period_sets)
    memset (self->setting_period_sets, 0,
        sizeof (gpointer) * self->n_setting_period_sets);
  else
    self->setting_period_sets =
        g_new0 (MXFDMS1SettingPeriod *, self->n_setting_period_sets);

  for (i = 0; i < self->n_identification_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->identification_sets_uids[i]);
    if (current && MXF_IS_DMS1_IDENTIFICATION (current)) {
      self->identification_sets[i] = MXF_DMS1_IDENTIFICATION (current);
    }
  }

  for (i = 0; i < self->n_group_relationship_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->group_relationship_sets_uids[i]);
    if (current && MXF_IS_DMS1_GROUP_RELATIONSHIP (current)) {
      self->group_relationship_sets[i] = MXF_DMS1_GROUP_RELATIONSHIP (current);
    }
  }

  for (i = 0; i < self->n_branding_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->branding_sets_uids[i]);
    if (current && MXF_IS_DMS1_BRANDING (current)) {
      self->branding_sets[i] = MXF_DMS1_BRANDING (current);
    }
  }

  for (i = 0; i < self->n_event_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->event_sets_uids[i]);
    if (current && MXF_IS_DMS1_EVENT (current)) {
      self->event_sets[i] = MXF_DMS1_EVENT (current);
    }
  }

  for (i = 0; i < self->n_award_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->award_sets_uids[i]);
    if (current && MXF_IS_DMS1_AWARD (current)) {
      self->award_sets[i] = MXF_DMS1_AWARD (current);
    }
  }

  for (i = 0; i < self->n_setting_period_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->setting_period_sets_uids[i]);
    if (current && MXF_IS_DMS1_SETTING_PERIOD (current)) {
      self->setting_period_sets[i] = MXF_DMS1_SETTING_PERIOD (current);
    }
  }

  return
      MXF_METADATA_BASE_CLASS
      (mxf_dms1_production_framework_parent_class)->resolve (m, metadata);
}

static gboolean
mxf_dms1_production_framework_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1ProductionFramework *self = MXF_DMS1_PRODUCTION_FRAMEWORK (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 integration_indication_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x05,
    0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 identification_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x06, 0x00
  };
  static const guint8 group_relationship_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x05, 0x00
  };
  static const guint8 branding_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x08, 0x00
  };
  static const guint8 event_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x09, 0x00
  };
  static const guint8 award_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0b, 0x00
  };
  static const guint8 setting_period_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0e, 0x01
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &integration_indication_ul, 16) == 0) {
    self->integration_indication = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  integration indication = %s",
        GST_STR_NULL (self->integration_indication));
  } else if (memcmp (tag_ul, &identification_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->identification_sets_uids,
            &self->n_identification_sets, tag_data, tag_size))
      goto error;

    GST_DEBUG ("  number of identification sets = %u",
        self->n_identification_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_identification_sets; i++) {
        GST_DEBUG ("    identification sets %u = %s", i,
            mxf_uuid_to_string (&self->identification_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &group_relationship_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->group_relationship_sets_uids,
            &self->n_group_relationship_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of group relationship sets = %u",
        self->n_group_relationship_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_group_relationship_sets; i++) {
        GST_DEBUG ("    group relationship sets %u = %s", i,
            mxf_uuid_to_string (&self->group_relationship_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &branding_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->branding_sets_uids,
            &self->n_branding_sets, tag_data, tag_size))
      goto error;

    GST_DEBUG ("  number of branding sets = %u", self->n_branding_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_branding_sets; i++) {
        GST_DEBUG ("    branding sets %u = %s", i,
            mxf_uuid_to_string (&self->branding_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &event_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->event_sets_uids, &self->n_event_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of event sets = %u", self->n_event_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_event_sets; i++) {
        GST_DEBUG ("    event sets %u = %s", i,
            mxf_uuid_to_string (&self->event_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &award_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->award_sets_uids, &self->n_award_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of award sets = %u", self->n_award_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_award_sets; i++) {
        GST_DEBUG ("    award sets %u = %s", i,
            mxf_uuid_to_string (&self->award_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &setting_period_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->setting_period_sets_uids,
            &self->n_setting_period_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of setting period sets = %u",
        self->n_setting_period_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_setting_period_sets; i++) {
        GST_DEBUG ("    setting period sets %u = %s", i,
            mxf_uuid_to_string (&self->setting_period_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_production_framework_parent_class)->handle_tag (metadata,
        primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 production framework local tag 0x%04x of size %u",
      tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_production_framework_init (MXFDMS1ProductionFramework * self)
{
}

static void
mxf_dms1_production_framework_class_init (MXFDMS1ProductionFrameworkClass *
    klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_production_framework_finalize;
  metadatabase_class->handle_tag = mxf_dms1_production_framework_handle_tag;
  metadatabase_class->resolve = mxf_dms1_production_framework_resolve;

  dm_class->type = 0x010100;
}

G_DEFINE_TYPE (MXFDMS1ClipFramework, mxf_dms1_clip_framework,
    MXF_TYPE_DMS1_PRODUCTION_CLIP_FRAMEWORK);

static void
mxf_dms1_clip_framework_finalize (GObject * object)
{
  MXFDMS1ClipFramework *self = MXF_DMS1_CLIP_FRAMEWORK (object);

  g_free (self->clip_kind);
  self->clip_kind = NULL;

  g_free (self->slate_information);
  self->slate_information = NULL;

  g_free (self->scripting_sets_uids);
  self->scripting_sets_uids = NULL;

  g_free (self->scripting_sets);
  self->scripting_sets = NULL;

  g_free (self->shot_sets_uids);
  self->shot_sets_uids = NULL;

  g_free (self->shot_sets);
  self->shot_sets = NULL;

  g_free (self->device_parameters_sets_uids);
  self->device_parameters_sets_uids = NULL;

  g_free (self->device_parameters_sets);
  self->device_parameters_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_clip_framework_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_clip_framework_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1ClipFramework *self = MXF_DMS1_CLIP_FRAMEWORK (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->scripting_sets)
    memset (self->scripting_sets, 0,
        sizeof (gpointer) * self->n_scripting_sets);
  else
    self->scripting_sets = g_new0 (MXFDMS1Scripting *, self->n_scripting_sets);

  if (self->shot_sets)
    memset (self->shot_sets, 0, sizeof (gpointer) * self->n_shot_sets);
  else
    self->shot_sets = g_new0 (MXFDMS1Shot *, self->n_shot_sets);

  if (self->device_parameters_sets)
    memset (self->device_parameters_sets, 0,
        sizeof (gpointer) * self->n_device_parameters_sets);
  else
    self->device_parameters_sets =
        g_new0 (MXFDMS1DeviceParameters *, self->n_device_parameters_sets);

  for (i = 0; i < self->n_scripting_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->scripting_sets_uids[i]);

    if (current && MXF_IS_DMS1_SCRIPTING (current)) {
      self->scripting_sets[i] = MXF_DMS1_SCRIPTING (current);
    }
  }

  for (i = 0; i < self->n_shot_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->shot_sets_uids[i]);
    if (current && MXF_IS_DMS1_SHOT (current)) {
      self->shot_sets[i] = MXF_DMS1_SHOT (current);
    }
  }

  for (i = 0; i < self->n_device_parameters_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->device_parameters_sets_uids[i]);
    if (current && MXF_IS_DMS1_DEVICE_PARAMETERS (current)) {
      self->device_parameters_sets[i] = MXF_DMS1_DEVICE_PARAMETERS (current);
    }
  }

  current = g_hash_table_lookup (metadata, &self->processing_set_uid);
  if (current && MXF_IS_DMS1_PROCESSING (current)) {
    self->processing_set = MXF_DMS1_PROCESSING (current);
  }

  return
      MXF_METADATA_BASE_CLASS (mxf_dms1_clip_framework_parent_class)->resolve
      (m, metadata);
}

static gboolean
mxf_dms1_clip_framework_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1ClipFramework *self = MXF_DMS1_CLIP_FRAMEWORK (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[96];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 clip_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 clip_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x05, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 extended_clip_id_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x01,
    0x01, 0x15, 0x09, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 clip_creation_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x07,
    0x02, 0x01, 0x10, 0x01, 0x04, 0x00, 0x00
  };
  static const guint8 take_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x05, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 slate_information_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x05, 0x03, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 scripting_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0f, 0x00
  };
  static const guint8 shot_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x11, 0x02
  };
  static const guint8 device_parameters_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x1e, 0x00
  };
  static const guint8 processing_set_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x02, 0x40, 0x20, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &clip_kind_ul, 16) == 0) {
    self->clip_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  clip kind = %s", GST_STR_NULL (self->clip_kind));
  } else if (memcmp (tag_ul, &clip_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->clip_number, tag_data, tag_size);
    GST_DEBUG ("  clip number = %s", self->clip_number);
  } else if (memcmp (tag_ul, &extended_clip_id_ul, 16) == 0) {
    if (tag_size != 32 && tag_size != 64)
      goto error;

    memcpy (self->extended_clip_id, tag_data, tag_size);
    self->extended_clip_id_full = (tag_size == 64);

    GST_DEBUG ("  extended clip id (1) = %s",
        mxf_umid_to_string ((MXFUMID *) & self->extended_clip_id, str));
    if (tag_size == 64)
      GST_DEBUG ("  extended clip id (2) = %s",
          mxf_umid_to_string ((MXFUMID *) & self->extended_clip_id[32], str));
  } else if (memcmp (tag_ul, &clip_creation_date_and_time_ul, 16) == 0) {
    if (!mxf_timestamp_parse (&self->clip_creation_date_and_time, tag_data,
            tag_size))
      goto error;
    GST_DEBUG ("  clip creation date and time = %s",
        mxf_timestamp_to_string (&self->clip_creation_date_and_time, str));
  } else if (memcmp (tag_ul, &take_number_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->take_number = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  take number = %u", self->take_number);
  } else if (memcmp (tag_ul, &slate_information_ul, 16) == 0) {
    self->slate_information = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  slate information = %s",
        GST_STR_NULL (self->slate_information));
  } else if (memcmp (tag_ul, &scripting_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->scripting_sets_uids,
            &self->n_scripting_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of scripting sets = %u", self->n_scripting_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_scripting_sets; i++) {
        GST_DEBUG ("    scripting sets %u = %s", i,
            mxf_uuid_to_string (&self->scripting_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &shot_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->shot_sets_uids, &self->n_shot_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of shot sets = %u", self->n_shot_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_shot_sets; i++) {
        GST_DEBUG ("    shot sets %u = %s", i,
            mxf_uuid_to_string (&self->shot_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &device_parameters_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->device_parameters_sets_uids,
            &self->n_device_parameters_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of device parameters sets = %u",
        self->n_device_parameters_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_device_parameters_sets; i++) {
        GST_DEBUG ("    device parameters sets %u = %s", i,
            mxf_uuid_to_string (&self->device_parameters_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &processing_set_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->processing_set_uid, tag_data, 16);
    GST_DEBUG ("  processing set = %s",
        mxf_uuid_to_string (&self->processing_set_uid, str));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_clip_framework_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 clip framework local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_clip_framework_init (MXFDMS1ClipFramework * self)
{
}

static void
mxf_dms1_clip_framework_class_init (MXFDMS1ClipFrameworkClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_clip_framework_finalize;
  metadatabase_class->handle_tag = mxf_dms1_clip_framework_handle_tag;
  metadatabase_class->resolve = mxf_dms1_clip_framework_resolve;
  dm_class->type = 0x010200;
}

G_DEFINE_TYPE (MXFDMS1SceneFramework, mxf_dms1_scene_framework,
    MXF_TYPE_DMS1_FRAMEWORK);

static void
mxf_dms1_scene_framework_finalize (GObject * object)
{
  MXFDMS1SceneFramework *self = MXF_DMS1_SCENE_FRAMEWORK (object);

  g_free (self->setting_period_sets_uids);
  self->setting_period_sets_uids = NULL;

  g_free (self->setting_period_sets);
  self->setting_period_sets = NULL;

  g_free (self->shot_scene_sets_uids);
  self->shot_scene_sets_uids = NULL;

  g_free (self->shot_scene_sets);
  self->shot_scene_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_scene_framework_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_scene_framework_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1SceneFramework *self = MXF_DMS1_SCENE_FRAMEWORK (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->setting_period_sets)
    memset (self->setting_period_sets, 0,
        sizeof (gpointer) * self->n_setting_period_sets);
  else
    self->setting_period_sets =
        g_new0 (MXFDMS1SettingPeriod *, self->n_setting_period_sets);

  if (self->shot_scene_sets)
    memset (self->shot_scene_sets, 0,
        sizeof (gpointer) * self->n_shot_scene_sets);
  else
    self->shot_scene_sets = g_new0 (MXFDMS1Shot *, self->n_shot_scene_sets);

  for (i = 0; i < self->n_setting_period_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->setting_period_sets_uids[i]);
    if (current && MXF_IS_DMS1_SETTING_PERIOD (current)) {
      self->setting_period_sets[i] = MXF_DMS1_SETTING_PERIOD (current);
    }
  }

  for (i = 0; i < self->n_shot_scene_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->shot_scene_sets_uids[i]);
    if (current && MXF_IS_DMS1_SHOT (current)) {
      self->shot_scene_sets[i] = MXF_DMS1_SHOT (current);
    }
  }

  return
      MXF_METADATA_BASE_CLASS (mxf_dms1_scene_framework_parent_class)->resolve
      (m, metadata);
}

static gboolean
mxf_dms1_scene_framework_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1SceneFramework *self = MXF_DMS1_SCENE_FRAMEWORK (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 scene_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 setting_period_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0e, 0x02
  };
  static const guint8 shot_scene_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x11, 0x01
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &scene_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->scene_number, tag_data, tag_size);
    GST_DEBUG ("  scene number = %s", self->scene_number);
  } else if (memcmp (tag_ul, &setting_period_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->setting_period_sets_uids,
            &self->n_setting_period_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of setting period sets = %u",
        self->n_setting_period_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_setting_period_sets; i++) {
        GST_DEBUG ("    setting period sets %u = %s", i,
            mxf_uuid_to_string (&self->setting_period_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &shot_scene_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->shot_scene_sets_uids,
            &self->n_shot_scene_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of shot sets = %u", self->n_shot_scene_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_shot_scene_sets; i++) {
        GST_DEBUG ("    shot sets %u = %s", i,
            mxf_uuid_to_string (&self->shot_scene_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_scene_framework_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 scene framework local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_scene_framework_init (MXFDMS1SceneFramework * self)
{
}

static void
mxf_dms1_scene_framework_class_init (MXFDMS1SceneFrameworkClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_scene_framework_finalize;
  metadatabase_class->handle_tag = mxf_dms1_scene_framework_handle_tag;
  metadatabase_class->resolve = mxf_dms1_scene_framework_resolve;
  dm_class->type = 0x010300;
}

G_DEFINE_TYPE (MXFDMS1Titles, mxf_dms1_titles, MXF_TYPE_DMS1_TEXT_LANGUAGE);

static void
mxf_dms1_titles_finalize (GObject * object)
{
  MXFDMS1Titles *self = MXF_DMS1_TITLES (object);

  g_free (self->main_title);
  self->main_title = NULL;

  g_free (self->secondary_title);
  self->secondary_title = NULL;

  g_free (self->working_title);
  self->working_title = NULL;

  g_free (self->original_title);
  self->original_title = NULL;

  g_free (self->version_title);
  self->version_title = NULL;

  G_OBJECT_CLASS (mxf_dms1_titles_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_titles_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Titles *self = MXF_DMS1_TITLES (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 main_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x01,
    0x05, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 secondary_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x01,
    0x05, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 working_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x05, 0x0a, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 original_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x05, 0x0b, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 version_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x01,
    0x05, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &main_title_ul, 16) == 0) {
    self->main_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  main title = %s", GST_STR_NULL (self->main_title));
  } else if (memcmp (tag_ul, &secondary_title_ul, 16) == 0) {
    self->secondary_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  secondary title = %s", GST_STR_NULL (self->secondary_title));
  } else if (memcmp (tag_ul, &working_title_ul, 16) == 0) {
    self->working_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  working title = %s", GST_STR_NULL (self->working_title));
  } else if (memcmp (tag_ul, &original_title_ul, 16) == 0) {
    self->original_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  original title = %s", GST_STR_NULL (self->original_title));
  } else if (memcmp (tag_ul, &version_title_ul, 16) == 0) {
    self->version_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  version title = %s", GST_STR_NULL (self->version_title));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_titles_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_titles_init (MXFDMS1Titles * self)
{
}

static void
mxf_dms1_titles_class_init (MXFDMS1TitlesClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_titles_finalize;
  metadatabase_class->handle_tag = mxf_dms1_titles_handle_tag;
  dm_class->type = 0x100100;
}

G_DEFINE_TYPE (MXFDMS1Identification, mxf_dms1_identification,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_identification_finalize (GObject * object)
{
  MXFDMS1Identification *self = MXF_DMS1_IDENTIFICATION (object);

  g_free (self->identifier_value);
  self->identifier_value = NULL;

  g_free (self->identification_issuing_authority);
  self->identification_issuing_authority = NULL;

  G_OBJECT_CLASS (mxf_dms1_identification_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_identification_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Identification *self = MXF_DMS1_IDENTIFICATION (metadata);
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 identifier_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 identifier_value_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 identification_locator_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 identification_issuing_authority_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x0a, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &identifier_kind_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->identifier_kind, tag_data, tag_size);
    GST_DEBUG ("  identifier kind = %s", self->identifier_kind);
  } else if (memcmp (tag_ul, &identifier_value_ul, 16) == 0) {
    self->identifier_value = g_memdup2 (tag_data, tag_size);
    self->identifier_value_length = tag_size;
    GST_DEBUG ("  identifier value length = %u", tag_size);
  } else if (memcmp (tag_ul, &identification_locator_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->identification_locator, tag_data, 16);

    GST_DEBUG ("  identification locator = %s",
        mxf_uuid_to_string (&self->identification_locator, str));
  } else if (memcmp (tag_ul, &identification_issuing_authority_ul, 16) == 0) {
    self->identification_issuing_authority =
        mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  identification issuing authority = %s",
        GST_STR_NULL (self->identification_issuing_authority));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_identification_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 identification local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_identification_init (MXFDMS1Identification * self)
{
}

static void
mxf_dms1_identification_class_init (MXFDMS1IdentificationClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_identification_finalize;
  metadatabase_class->handle_tag = mxf_dms1_identification_handle_tag;
  dm_class->type = 0x110100;
}

G_DEFINE_TYPE (MXFDMS1GroupRelationship, mxf_dms1_group_relationship,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_group_relationship_finalize (GObject * object)
{
  MXFDMS1GroupRelationship *self = MXF_DMS1_GROUP_RELATIONSHIP (object);

  g_free (self->programming_group_kind);
  self->programming_group_kind = NULL;

  g_free (self->programming_group_title);
  self->programming_group_title = NULL;

  g_free (self->group_synopsis);
  self->group_synopsis = NULL;

  G_OBJECT_CLASS (mxf_dms1_group_relationship_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_group_relationship_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1GroupRelationship *self = MXF_DMS1_GROUP_RELATIONSHIP (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 programming_group_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 programming_group_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x02, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 group_synopsis_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x08, 0x01, 0x00, 0x00
  };
  static const guint8 numerical_position_in_sequence_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x06,
    0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 total_number_in_the_sequence_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x10, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 episodic_start_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 episodic_end_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x02, 0x05, 0x02, 0x03, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &programming_group_kind_ul, 16) == 0) {
    self->programming_group_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  programming group kind = %s",
        GST_STR_NULL (self->programming_group_kind));
  } else if (memcmp (tag_ul, &programming_group_title_ul, 16) == 0) {
    self->programming_group_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  programming group title = %s",
        GST_STR_NULL (self->programming_group_title));
  } else if (memcmp (tag_ul, &group_synopsis_ul, 16) == 0) {
    self->group_synopsis = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  group synopsis = %s", GST_STR_NULL (self->group_synopsis));
  } else if (memcmp (tag_ul, &numerical_position_in_sequence_ul, 16) == 0) {
    if (tag_size != 4)
      goto error;

    self->numerical_position_in_sequence = GST_READ_UINT32_BE (tag_data);
    GST_DEBUG ("  numerical position in sequence = %u",
        self->numerical_position_in_sequence);
  } else if (memcmp (tag_ul, &total_number_in_the_sequence_ul, 16) == 0) {
    if (tag_size != 4)
      goto error;

    self->total_number_in_the_sequence = GST_READ_UINT32_BE (tag_data);
    GST_DEBUG ("  total number in the sequence = %u",
        self->total_number_in_the_sequence);
  } else if (memcmp (tag_ul, &episodic_start_number_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->episodic_start_number = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  episodic start number = %u", self->episodic_start_number);
  } else if (memcmp (tag_ul, &episodic_end_number_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->episodic_end_number = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  episodic end number = %u", self->episodic_end_number);
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_group_relationship_parent_class)->handle_tag (metadata,
        primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 group relationship local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_group_relationship_init (MXFDMS1GroupRelationship * self)
{
}

static void
mxf_dms1_group_relationship_class_init (MXFDMS1GroupRelationshipClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_group_relationship_finalize;
  metadatabase_class->handle_tag = mxf_dms1_group_relationship_handle_tag;
  dm_class->type = 0x120100;
}

G_DEFINE_TYPE (MXFDMS1Branding, mxf_dms1_branding, MXF_TYPE_DMS1_TEXT_LANGUAGE);

static void
mxf_dms1_branding_finalize (GObject * object)
{
  MXFDMS1Branding *self = MXF_DMS1_BRANDING (object);

  g_free (self->brand_main_title);
  self->brand_main_title = NULL;

  g_free (self->brand_original_title);
  self->brand_original_title = NULL;

  G_OBJECT_CLASS (mxf_dms1_branding_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_branding_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Branding *self = MXF_DMS1_BRANDING (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 brand_main_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x05, 0x0D, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 brand_original_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x05, 0x0E, 0x01, 0x00, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &brand_main_title_ul, 16) == 0) {
    self->brand_main_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  brand main title = %s",
        GST_STR_NULL (self->brand_main_title));
  } else if (memcmp (tag_ul, &brand_original_title_ul, 16) == 0) {
    self->brand_original_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  brand original title = %s",
        GST_STR_NULL (self->brand_original_title));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_branding_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_branding_init (MXFDMS1Branding * self)
{
}

static void
mxf_dms1_branding_class_init (MXFDMS1BrandingClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_branding_finalize;
  metadatabase_class->handle_tag = mxf_dms1_branding_handle_tag;
  dm_class->type = 0x130100;
}

G_DEFINE_TYPE (MXFDMS1Event, mxf_dms1_event, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_event_finalize (GObject * object)
{
  MXFDMS1Event *self = MXF_DMS1_EVENT (object);

  g_free (self->event_indication);
  self->event_indication = NULL;

  g_free (self->publication_sets_uids);
  self->publication_sets_uids = NULL;

  g_free (self->publication_sets);
  self->publication_sets = NULL;

  g_free (self->annotation_sets_uids);
  self->annotation_sets_uids = NULL;

  g_free (self->annotation_sets);
  self->annotation_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_event_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_event_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Event *self = MXF_DMS1_EVENT (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->publication_sets)
    memset (self->publication_sets, 0,
        sizeof (gpointer) * self->n_publication_sets);
  else
    self->publication_sets =
        g_new0 (MXFDMS1Publication *, self->n_publication_sets);

  if (self->annotation_sets)
    memset (self->annotation_sets, 0,
        sizeof (gpointer) * self->n_annotation_sets);
  else
    self->annotation_sets =
        g_new0 (MXFDMS1Annotation *, self->n_annotation_sets);

  for (i = 0; i < self->n_publication_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->publication_sets_uids[i]);
    if (current && MXF_IS_DMS1_PUBLICATION (current)) {
      self->publication_sets[i] = MXF_DMS1_PUBLICATION (current);
    }
  }

  for (i = 0; i < self->n_annotation_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->annotation_sets_uids[i]);
    if (current && MXF_IS_DMS1_ANNOTATION (current)) {
      self->annotation_sets[i] = MXF_DMS1_ANNOTATION (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_event_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_event_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Event *self = MXF_DMS1_EVENT (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 event_indication_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x05,
    0x01, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 event_start_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x07,
    0x02, 0x01, 0x02, 0x07, 0x02, 0x00, 0x00
  };
  static const guint8 event_end_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x07,
    0x02, 0x01, 0x02, 0x09, 0x02, 0x00, 0x00
  };
  static const guint8 publication_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0a, 0x00
  };
  static const guint8 annotation_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x08, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x0d, 0x01
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &event_indication_ul, 16) == 0) {
    self->event_indication = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  event indication = %s",
        GST_STR_NULL (self->event_indication));
  } else if (memcmp (tag_ul, &event_start_date_and_time_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->event_start_date_and_time, tag_data, tag_size);
    GST_DEBUG ("  event start date and time = %s",
        self->event_start_date_and_time);
  } else if (memcmp (tag_ul, &event_end_date_and_time_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->event_end_date_and_time, tag_data, tag_size);
    GST_DEBUG ("  event end date and time = %s", self->event_end_date_and_time);
  } else if (memcmp (tag_ul, &publication_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->publication_sets_uids,
            &self->n_publication_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of publication sets = %u", self->n_publication_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_publication_sets; i++) {
        GST_DEBUG ("    publication sets %u = %s", i,
            mxf_uuid_to_string (&self->publication_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &annotation_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->annotation_sets_uids,
            &self->n_annotation_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of annotation sets = %u", self->n_annotation_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_annotation_sets; i++) {
        GST_DEBUG ("    annotation sets %u = %s", i,
            mxf_uuid_to_string (&self->annotation_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_event_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 event local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_event_init (MXFDMS1Event * self)
{
}

static void
mxf_dms1_event_class_init (MXFDMS1EventClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_event_finalize;
  metadatabase_class->handle_tag = mxf_dms1_event_handle_tag;
  metadatabase_class->resolve = mxf_dms1_event_resolve;
  dm_class->type = 0x140100;
}

G_DEFINE_TYPE (MXFDMS1Publication, mxf_dms1_publication, MXF_TYPE_DMS1);

static void
mxf_dms1_publication_finalize (GObject * object)
{
  MXFDMS1Publication *self = MXF_DMS1_PUBLICATION (object);

  g_free (self->publication_organisation_name);
  self->publication_organisation_name = NULL;

  g_free (self->publication_service_name);
  self->publication_service_name = NULL;

  g_free (self->publication_medium);
  self->publication_medium = NULL;

  g_free (self->publication_region);
  self->publication_region = NULL;

  G_OBJECT_CLASS (mxf_dms1_publication_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_publication_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Publication *self = MXF_DMS1_PUBLICATION (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 publication_organisation_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x10, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00
  };
  static const guint8 publication_service_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x10, 0x02, 0x01, 0x02, 0x01, 0x00, 0x00
  };
  static const guint8 publication_medium_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x10, 0x02, 0x01, 0x03, 0x01, 0x00, 0x00
  };
  static const guint8 publication_region_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x10, 0x02, 0x01, 0x04, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &publication_organisation_name_ul, 16) == 0) {
    self->publication_organisation_name =
        mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  publication organisation name = %s",
        GST_STR_NULL (self->publication_organisation_name));
  } else if (memcmp (tag_ul, &publication_service_name_ul, 16) == 0) {
    self->publication_service_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG (" publication service name = %s",
        GST_STR_NULL (self->publication_service_name));
  } else if (memcmp (tag_ul, &publication_medium_ul, 16) == 0) {
    self->publication_medium = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG (" publication medium = %s",
        GST_STR_NULL (self->publication_medium));
  } else if (memcmp (tag_ul, &publication_region_ul, 16) == 0) {
    self->publication_region = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG (" publication region = %s",
        GST_STR_NULL (self->publication_region));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_publication_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_publication_init (MXFDMS1Publication * self)
{
}

static void
mxf_dms1_publication_class_init (MXFDMS1PublicationClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_publication_finalize;
  metadatabase_class->handle_tag = mxf_dms1_publication_handle_tag;
  dm_class->type = 0x140200;
}

G_DEFINE_TYPE (MXFDMS1Award, mxf_dms1_award, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_award_finalize (GObject * object)
{
  MXFDMS1Award *self = MXF_DMS1_AWARD (object);

  g_free (self->festival);
  self->festival = NULL;

  g_free (self->award_name);
  self->award_name = NULL;

  g_free (self->award_classification);
  self->award_classification = NULL;

  g_free (self->nomination_category);
  self->nomination_category = NULL;

  g_free (self->participant_sets_uids);
  self->participant_sets_uids = NULL;

  g_free (self->participant_sets);
  self->participant_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_award_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_award_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Award *self = MXF_DMS1_AWARD (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->participant_sets)
    memset (self->participant_sets, 0,
        sizeof (gpointer) * self->n_participant_sets);
  else
    self->participant_sets =
        g_new0 (MXFDMS1Participant *, self->n_participant_sets);

  for (i = 0; i < self->n_participant_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->participant_sets_uids[i]);
    if (current && MXF_IS_DMS1_PARTICIPANT (current)) {
      self->participant_sets[i] = MXF_DMS1_PARTICIPANT (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_award_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_award_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Award *self = MXF_DMS1_AWARD (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 festival_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x02, 0x01, 0x03, 0x01, 0x00, 0x00
  };
  static const guint8 festival_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x07,
    0x02, 0x01, 0x02, 0x07, 0x10, 0x01, 0x00
  };
  static const guint8 award_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x02, 0x01, 0x04, 0x01, 0x00, 0x00
  };
  static const guint8 award_classification_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x02, 0x01, 0x05, 0x01, 0x00, 0x00
  };
  static const guint8 nomination_category_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x02, 0x01, 0x06, 0x01, 0x00, 0x00
  };
  static const guint8 participant_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x13, 0x01
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &festival_ul, 16) == 0) {
    self->festival = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  festival = %s", GST_STR_NULL (self->festival));
  } else if (memcmp (tag_ul, &festival_date_and_time_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->festival_date_and_time, tag_data, tag_size);
    GST_DEBUG ("  festival date and time = %s",
        GST_STR_NULL (self->festival_date_and_time));
  } else if (memcmp (tag_ul, &award_name_ul, 16) == 0) {
    self->award_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  award name = %s", GST_STR_NULL (self->award_name));
  } else if (memcmp (tag_ul, &award_classification_ul, 16) == 0) {
    self->award_classification = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  award classification = %s",
        GST_STR_NULL (self->award_classification));
  } else if (memcmp (tag_ul, &nomination_category_ul, 16) == 0) {
    self->nomination_category = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  nomination category = %s",
        GST_STR_NULL (self->nomination_category));
  } else if (memcmp (tag_ul, &participant_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->participant_sets_uids,
            &self->n_participant_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of participant sets = %u", self->n_participant_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_participant_sets; i++) {
        GST_DEBUG ("    participant sets %u = %s", i,
            mxf_uuid_to_string (&self->participant_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_award_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 award local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_award_init (MXFDMS1Award * self)
{
}

static void
mxf_dms1_award_class_init (MXFDMS1AwardClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_award_finalize;
  metadatabase_class->handle_tag = mxf_dms1_award_handle_tag;
  metadatabase_class->resolve = mxf_dms1_award_resolve;
  dm_class->type = 0x150100;
}

G_DEFINE_TYPE (MXFDMS1CaptionsDescription, mxf_dms1_captions_description,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_captions_description_finalize (GObject * object)
{
  MXFDMS1CaptionsDescription *self = MXF_DMS1_CAPTIONS_DESCRIPTION (object);

  g_free (self->caption_kind);
  self->caption_kind = NULL;

  G_OBJECT_CLASS (mxf_dms1_captions_description_parent_class)->finalize
      (object);
}

static gboolean
mxf_dms1_captions_description_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1CaptionsDescription *self = MXF_DMS1_CAPTIONS_DESCRIPTION (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 extended_captions_language_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x01, 0x01, 0x02, 0x02, 0x12, 0x00, 0x00
  };
  static const guint8 caption_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x04,
    0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &extended_captions_language_code_ul, 16) == 0) {
    if (tag_size > 12)
      goto error;

    memcpy (self->extended_captions_language_code, tag_data, tag_size);
    GST_DEBUG ("  extended captions language code = %s",
        self->extended_captions_language_code);
  } else if (memcmp (tag_ul, &caption_kind_ul, 16) == 0) {
    self->caption_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  caption kind = %s", GST_STR_NULL (self->caption_kind));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_captions_description_parent_class)->handle_tag (metadata,
        primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 captions description local tag 0x%04x of size %u",
      tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_captions_description_init (MXFDMS1CaptionsDescription * self)
{
}

static void
mxf_dms1_captions_description_class_init (MXFDMS1CaptionsDescriptionClass *
    klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_captions_description_finalize;
  metadatabase_class->handle_tag = mxf_dms1_captions_description_handle_tag;
  dm_class->type = 0x160100;
}

G_DEFINE_TYPE (MXFDMS1Annotation, mxf_dms1_annotation, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_annotation_finalize (GObject * object)
{
  MXFDMS1Annotation *self = MXF_DMS1_ANNOTATION (object);

  g_free (self->annotation_kind);
  self->annotation_kind = NULL;

  g_free (self->annotation_synopsis);
  self->annotation_synopsis = NULL;

  g_free (self->annotation_description);
  self->annotation_description = NULL;

  g_free (self->related_material_description);
  self->related_material_description = NULL;

  g_free (self->classification_sets_uids);
  self->classification_sets_uids = NULL;

  g_free (self->classification_sets);
  self->classification_sets = NULL;

  g_free (self->related_material_locators);
  self->related_material_locators = NULL;

  g_free (self->participant_sets_uids);
  self->participant_sets_uids = NULL;

  g_free (self->participant_sets);
  self->participant_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_annotation_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_annotation_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Annotation *self = MXF_DMS1_ANNOTATION (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->classification_sets)
    memset (self->classification_sets, 0,
        sizeof (gpointer) * self->n_classification_sets);
  else
    self->classification_sets =
        g_new0 (MXFDMS1Classification *, self->n_classification_sets);

  if (self->participant_sets)
    memset (self->participant_sets, 0,
        sizeof (gpointer) * self->n_participant_sets);
  else
    self->participant_sets =
        g_new0 (MXFDMS1Participant *, self->n_participant_sets);

  for (i = 0; i < self->n_classification_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->classification_sets_uids[i]);
    if (current && MXF_IS_DMS1_CLASSIFICATION (current)) {
      self->classification_sets[i] = MXF_DMS1_CLASSIFICATION (current);
    }
  }

  current = g_hash_table_lookup (metadata, &self->cue_words_set_uid);
  if (current && MXF_IS_DMS1_CUE_WORDS (current)) {
    self->cue_words_set = MXF_DMS1_CUE_WORDS (current);
  }

  for (i = 0; i < self->n_participant_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->participant_sets_uids[i]);
    if (current && MXF_IS_DMS1_PARTICIPANT (current)) {
      self->participant_sets[i] = MXF_DMS1_PARTICIPANT (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_annotation_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_annotation_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Annotation *self = MXF_DMS1_ANNOTATION (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 annotation_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x0e, 0x01, 0x00, 0x00
  };
  static const guint8 annotation_synopsis_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x09, 0x01, 0x00, 0x00
  };
  static const guint8 annotation_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x0a, 0x01, 0x00, 0x00
  };
  static const guint8 related_material_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x0f, 0x01, 0x00, 0x00
  };
  static const guint8 classification_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x10, 0x00
  };
  static const guint8 cue_words_set_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x02, 0x40, 0x23, 0x01
  };
  static const guint8 related_material_locators_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x06, 0x0d, 0x00, 0x00
  };
  static const guint8 participant_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x13, 0x03
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &annotation_kind_ul, 16) == 0) {
    self->annotation_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  annotation kind = %s", GST_STR_NULL (self->annotation_kind));
  } else if (memcmp (tag_ul, &annotation_synopsis_ul, 16) == 0) {
    self->annotation_synopsis = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  annotation synopsis = %s",
        GST_STR_NULL (self->annotation_synopsis));
  } else if (memcmp (tag_ul, &annotation_description_ul, 16) == 0) {
    self->annotation_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  annotation description = %s",
        GST_STR_NULL (self->annotation_description));
  } else if (memcmp (tag_ul, &related_material_description_ul, 16) == 0) {
    self->related_material_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  related material description = %s",
        GST_STR_NULL (self->related_material_description));
  } else if (memcmp (tag_ul, &classification_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->classification_sets_uids,
            &self->n_classification_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of classification sets = %u",
        self->n_classification_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_classification_sets; i++) {
        GST_DEBUG ("    classification sets %u = %s", i,
            mxf_uuid_to_string (&self->classification_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &cue_words_set_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->cue_words_set_uid, tag_data, 16);
    GST_DEBUG ("  cue words set = %s",
        mxf_uuid_to_string (&self->cue_words_set_uid, str));
  } else if (memcmp (tag_ul, &related_material_locators_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->related_material_locators,
            &self->n_related_material_locators, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of related material locators = %u",
        self->n_related_material_locators);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_related_material_locators; i++) {
        GST_DEBUG ("    related material locators %u = %s", i,
            mxf_uuid_to_string (&self->related_material_locators[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &participant_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->participant_sets_uids,
            &self->n_participant_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of participant sets = %u", self->n_participant_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_participant_sets; i++) {
        GST_DEBUG ("    participant sets %u = %s", i,
            mxf_uuid_to_string (&self->participant_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_annotation_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 annotation local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_annotation_init (MXFDMS1Annotation * self)
{
}

static void
mxf_dms1_annotation_class_init (MXFDMS1AnnotationClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_annotation_finalize;
  metadatabase_class->handle_tag = mxf_dms1_annotation_handle_tag;
  metadatabase_class->resolve = mxf_dms1_annotation_resolve;
  dm_class->type = 0x170100;
}

G_DEFINE_TYPE (MXFDMS1SettingPeriod, mxf_dms1_setting_period,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_setting_period_finalize (GObject * object)
{
  MXFDMS1SettingPeriod *self = MXF_DMS1_SETTING_PERIOD (object);

  g_free (self->time_period_keyword);
  self->time_period_keyword = NULL;

  g_free (self->setting_period_description);
  self->setting_period_description = NULL;

  G_OBJECT_CLASS (mxf_dms1_setting_period_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_setting_period_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1SettingPeriod *self = MXF_DMS1_SETTING_PERIOD (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[32];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 setting_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x02, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00
  };
  static const guint8 time_period_keyword_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x02, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00
  };
  static const guint8 setting_period_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x02, 0x01, 0x08, 0x03, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &setting_date_and_time_ul, 16) == 0) {
    if (!mxf_timestamp_parse (&self->setting_date_and_time, tag_data, tag_size))
      goto error;

    GST_DEBUG ("  last modified date = %s",
        mxf_timestamp_to_string (&self->setting_date_and_time, str));
  } else if (memcmp (tag_ul, &time_period_keyword_ul, 16) == 0) {
    self->time_period_keyword = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  time period keyword = %s",
        GST_STR_NULL (self->time_period_keyword));
  } else if (memcmp (tag_ul, &setting_period_description_ul, 16) == 0) {
    self->setting_period_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  setting period description = %s",
        GST_STR_NULL (self->setting_period_description));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_setting_period_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 setting period local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_setting_period_init (MXFDMS1SettingPeriod * self)
{
}

static void
mxf_dms1_setting_period_class_init (MXFDMS1SettingPeriodClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_setting_period_finalize;
  metadatabase_class->handle_tag = mxf_dms1_setting_period_handle_tag;
  dm_class->type = 0x170200;
}

G_DEFINE_TYPE (MXFDMS1Scripting, mxf_dms1_scripting, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_scripting_finalize (GObject * object)
{
  MXFDMS1Scripting *self = MXF_DMS1_SCRIPTING (object);

  g_free (self->scripting_kind);
  self->scripting_kind = NULL;

  g_free (self->scripting_text);
  self->scripting_text = NULL;

  g_free (self->scripting_locators);
  self->scripting_locators = NULL;

  G_OBJECT_CLASS (mxf_dms1_scripting_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_scripting_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Scripting *self = MXF_DMS1_SCRIPTING (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 scripting_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x0b, 0x01, 0x00, 0x00
  };
  static const guint8 scripting_text_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x0c, 0x01, 0x00, 0x00
  };
  static const guint8 scripting_locators_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x08, 0x06,
    0x01, 0x01, 0x04, 0x06, 0x0e, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &scripting_kind_ul, 16) == 0) {
    self->scripting_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  scripting kind = %s", GST_STR_NULL (self->scripting_kind));
  } else if (memcmp (tag_ul, &scripting_text_ul, 16) == 0) {
    self->scripting_text = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  scripting description = %s",
        GST_STR_NULL (self->scripting_text));
  } else if (memcmp (tag_ul, &scripting_locators_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->scripting_locators,
            &self->n_scripting_locators, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of scripting locators = %u",
        self->n_scripting_locators);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_scripting_locators; i++) {
        GST_DEBUG ("   scripting locators %u = %s", i,
            mxf_uuid_to_string (&self->scripting_locators[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_scripting_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 scripting local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_scripting_init (MXFDMS1Scripting * self)
{
}

static void
mxf_dms1_scripting_class_init (MXFDMS1ScriptingClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_scripting_finalize;
  metadatabase_class->handle_tag = mxf_dms1_scripting_handle_tag;
  dm_class->type = 0x170300;
}

G_DEFINE_TYPE (MXFDMS1Classification, mxf_dms1_classification,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_classification_finalize (GObject * object)
{
  MXFDMS1Classification *self = MXF_DMS1_CLASSIFICATION (object);

  g_free (self->name_value_sets_uids);
  self->name_value_sets_uids = NULL;

  g_free (self->name_value_sets);
  self->name_value_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_classification_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_classification_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Classification *self = MXF_DMS1_CLASSIFICATION (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->name_value_sets)
    memset (self->name_value_sets, 0,
        sizeof (gpointer) * self->n_name_value_sets);
  else
    self->name_value_sets =
        g_new0 (MXFDMS1NameValue *, self->n_name_value_sets);

  for (i = 0; i < self->n_name_value_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->name_value_sets_uids[i]);
    if (current && MXF_IS_DMS1_NAME_VALUE (current)) {
      self->name_value_sets[i] = MXF_DMS1_NAME_VALUE (current);
    }
  }

  return
      MXF_METADATA_BASE_CLASS (mxf_dms1_classification_parent_class)->resolve
      (m, metadata);
}

static gboolean
mxf_dms1_classification_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Classification *self = MXF_DMS1_CLASSIFICATION (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 content_classification_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x03,
    0x02, 0x01, 0x03, 0x04, 0x00, 0x00, 0x00
  };
  static const guint8 name_value_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x1f, 0x01
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &content_classification_ul, 16) == 0) {
    if (tag_size > 127)
      goto error;

    memcpy (self->content_classification, tag_data, tag_size);
    GST_DEBUG ("  content classification = %s", self->content_classification);
  } else if (memcmp (tag_ul, &name_value_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->name_value_sets_uids,
            &self->n_name_value_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of name-value sets = %u", self->n_name_value_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_name_value_sets; i++) {
        GST_DEBUG ("    name-value sets %u = %s", i,
            mxf_uuid_to_string (&self->name_value_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_classification_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 classification local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_classification_init (MXFDMS1Classification * self)
{
}

static void
mxf_dms1_classification_class_init (MXFDMS1ClassificationClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_classification_finalize;
  metadatabase_class->handle_tag = mxf_dms1_classification_handle_tag;
  metadatabase_class->resolve = mxf_dms1_classification_resolve;
  dm_class->type = 0x170400;
}

G_DEFINE_TYPE (MXFDMS1Shot, mxf_dms1_shot, MXF_TYPE_DMS1_TEXT_LANGUAGE);

static void
mxf_dms1_shot_finalize (GObject * object)
{
  MXFDMS1Shot *self = MXF_DMS1_SHOT (object);

  g_free (self->shot_track_ids);
  self->shot_track_ids = NULL;

  g_free (self->shot_description);
  self->shot_description = NULL;

  g_free (self->shot_comment_kind);
  self->shot_comment_kind = NULL;

  g_free (self->shot_comment);
  self->shot_comment = NULL;

  g_free (self->key_point_sets_uids);
  self->key_point_sets_uids = NULL;

  g_free (self->key_point_sets);
  self->key_point_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_shot_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_shot_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Shot *self = MXF_DMS1_SHOT (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->key_point_sets)
    memset (self->key_point_sets, 0,
        sizeof (gpointer) * self->n_key_point_sets);
  else
    self->key_point_sets = g_new0 (MXFDMS1KeyPoint *, self->n_key_point_sets);

  current = g_hash_table_lookup (metadata, &self->cue_words_set_uid);
  if (current && MXF_IS_DMS1_CUE_WORDS (current)) {
    self->cue_words_set = MXF_DMS1_CUE_WORDS (current);
  }

  for (i = 0; i < self->n_key_point_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->key_point_sets_uids[i]);
    if (current && MXF_IS_DMS1_KEY_POINT (current)) {
      self->key_point_sets[i] = MXF_DMS1_KEY_POINT (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_shot_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_shot_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Shot *self = MXF_DMS1_SHOT (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 shot_start_position_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x07,
    0x02, 0x01, 0x03, 0x01, 0x09, 0x00, 0x00
  };
  static const guint8 shot_duration_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x07,
    0x02, 0x02, 0x01, 0x02, 0x04, 0x00, 0x00
  };
  static const guint8 shot_track_ids_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x07, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 shot_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x02, 0x01, 0x06, 0x0d, 0x01, 0x00, 0x00
  };
  static const guint8 shot_comment_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x02, 0x05, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 shot_comment_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x03,
    0x02, 0x05, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 cue_words_set_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x02, 0x40, 0x23, 0x01
  };
  static const guint8 key_point_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x12, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &shot_start_position_ul, 16) == 0) {
    if (tag_size != 8)
      goto error;

    self->shot_start_position = GST_READ_UINT64_BE (tag_data);
    GST_DEBUG ("  shot start position = %" G_GINT64_FORMAT,
        self->shot_start_position);
  } else if (memcmp (tag_ul, &shot_duration_ul, 16) == 0) {
    if (tag_size != 8)
      goto error;

    self->shot_duration = GST_READ_UINT64_BE (tag_data);
    GST_DEBUG ("  shot duration = %" G_GINT64_FORMAT, self->shot_duration);
  } else if (memcmp (tag_ul, &shot_track_ids_ul, 16) == 0) {
    guint32 len, i;

    len = GST_READ_UINT32_BE (tag_data);
    GST_DEBUG ("  number of shot track ids = %u", len);
    if (len == 0)
      return ret;

    if (GST_READ_UINT32_BE (tag_data + 4) != 4)
      goto error;
    tag_data += 8;
    tag_size -= 8;

    if (tag_size / 4 < len)
      goto error;

    self->n_shot_track_ids = len;
    self->shot_track_ids = g_new0 (guint32, len);

    for (i = 0; i < len; i++) {
      self->shot_track_ids[i] = GST_READ_UINT32_BE (tag_data);
      GST_DEBUG ("    shot track ids %u = %u", i, self->shot_track_ids[i]);
      tag_data += 4;
      tag_size -= 4;
    }
  } else if (memcmp (tag_ul, &shot_description_ul, 16) == 0) {
    self->shot_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  shot description = %s",
        GST_STR_NULL (self->shot_description));
  } else if (memcmp (tag_ul, &shot_comment_kind_ul, 16) == 0) {
    self->shot_comment_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  shot comment kind = %s",
        GST_STR_NULL (self->shot_comment_kind));
  } else if (memcmp (tag_ul, &shot_comment_ul, 16) == 0) {
    self->shot_comment = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  shot comment = %s", GST_STR_NULL (self->shot_comment));
  } else if (memcmp (tag_ul, &cue_words_set_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->cue_words_set_uid, tag_data, 16);
    GST_DEBUG ("  cue words set = %s",
        mxf_uuid_to_string (&self->cue_words_set_uid, str));
  } else if (memcmp (tag_ul, &key_point_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->key_point_sets_uids,
            &self->n_key_point_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of key point sets = %u", self->n_key_point_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_key_point_sets; i++) {
        GST_DEBUG ("    key point sets %u = %s", i,
            mxf_uuid_to_string (&self->key_point_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_shot_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 shot local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_shot_init (MXFDMS1Shot * self)
{
}

static void
mxf_dms1_shot_class_init (MXFDMS1ShotClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_shot_finalize;
  metadatabase_class->handle_tag = mxf_dms1_shot_handle_tag;
  metadatabase_class->resolve = mxf_dms1_shot_resolve;
  dm_class->type = 0x170500;
}

G_DEFINE_TYPE (MXFDMS1KeyPoint, mxf_dms1_key_point, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_key_point_finalize (GObject * object)
{
  MXFDMS1KeyPoint *self = MXF_DMS1_KEY_POINT (object);

  g_free (self->keypoint_kind);
  self->keypoint_kind = NULL;

  g_free (self->keypoint_value);
  self->keypoint_value = NULL;

  G_OBJECT_CLASS (mxf_dms1_key_point_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_key_point_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1KeyPoint *self = MXF_DMS1_KEY_POINT (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 keypoint_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x01, 0x02, 0x10, 0x01, 0x00, 0x00
  };
  static const guint8 keypoint_value_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x01, 0x02, 0x11, 0x01, 0x00, 0x00
  };
  static const guint8 keypoint_position_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x02, 0x01, 0x03, 0x01, 0x07, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &keypoint_kind_ul, 16) == 0) {
    self->keypoint_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  keypoint kind = %s", GST_STR_NULL (self->keypoint_kind));
  } else if (memcmp (tag_ul, &keypoint_value_ul, 16) == 0) {
    self->keypoint_value = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  keypoint value = %s", GST_STR_NULL (self->keypoint_value));
  } else if (memcmp (tag_ul, &keypoint_position_ul, 16) == 0) {
    if (tag_size != 8)
      goto error;

    self->keypoint_position = GST_READ_UINT64_BE (tag_data);
    GST_DEBUG ("  keypoint position = %" G_GINT64_FORMAT,
        self->keypoint_position);
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_key_point_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 key point local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_key_point_init (MXFDMS1KeyPoint * self)
{
}

static void
mxf_dms1_key_point_class_init (MXFDMS1KeyPointClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_key_point_finalize;
  metadatabase_class->handle_tag = mxf_dms1_key_point_handle_tag;
  dm_class->type = 0x170600;
}

G_DEFINE_TYPE (MXFDMS1Participant, mxf_dms1_participant,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_participant_finalize (GObject * object)
{
  MXFDMS1Participant *self = MXF_DMS1_PARTICIPANT (object);

  g_free (self->contribution_status);
  self->contribution_status = NULL;

  g_free (self->job_function);
  self->job_function = NULL;

  g_free (self->role_or_identity_name);
  self->role_or_identity_name = NULL;

  g_free (self->person_sets_uids);
  self->person_sets_uids = NULL;

  g_free (self->person_sets);
  self->person_sets = NULL;

  g_free (self->organisation_sets_uids);
  self->organisation_sets_uids = NULL;

  g_free (self->organisation_sets);
  self->organisation_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_participant_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_participant_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Participant *self = MXF_DMS1_PARTICIPANT (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->person_sets)
    memset (self->person_sets, 0, sizeof (gpointer) * self->n_person_sets);
  else
    self->person_sets = g_new0 (MXFDMS1Person *, self->n_person_sets);

  if (self->organisation_sets)
    memset (self->organisation_sets, 0,
        sizeof (gpointer) * self->n_organisation_sets);
  else
    self->organisation_sets =
        g_new0 (MXFDMS1Organisation *, self->n_organisation_sets);

  for (i = 0; i < self->n_person_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->person_sets_uids[i]);
    if (current && MXF_IS_DMS1_PERSON (current)) {
      self->person_sets[i] = MXF_DMS1_PERSON (current);
    }
  }

  for (i = 0; i < self->n_organisation_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->organisation_sets_uids[i]);
    if (current && MXF_IS_DMS1_ORGANISATION (current)) {
      self->organisation_sets[i] = MXF_DMS1_ORGANISATION (current);
    }
  }

  return
      MXF_METADATA_BASE_CLASS (mxf_dms1_participant_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_participant_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Participant *self = MXF_DMS1_PARTICIPANT (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 participant_uid_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x08, 0x01,
    0x01, 0x15, 0x40, 0x01, 0x01, 0x00, 0x00
  };
  static const guint8 contribution_status_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00
  };
  static const guint8 job_function_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x05, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 job_function_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x05, 0x01, 0x02, 0x00, 0x00, 0x00
  };
  static const guint8 role_or_identity_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x05, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 person_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x14, 0x00
  };
  static const guint8 organisation_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x15, 0x02
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &participant_uid_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->participant_uid, tag_data, 16);
    GST_DEBUG ("  participant uid = %s",
        mxf_uuid_to_string (&self->participant_uid, str));
  } else if (memcmp (tag_ul, &contribution_status_ul, 16) == 0) {
    self->contribution_status = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  contribution status = %s",
        GST_STR_NULL (self->contribution_status));
  } else if (memcmp (tag_ul, &job_function_ul, 16) == 0) {
    self->job_function = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  job function = %s", GST_STR_NULL (self->job_function));
  } else if (memcmp (tag_ul, &job_function_code_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->job_function_code, tag_data, tag_size);
    GST_DEBUG ("  job function code = %s", self->job_function_code);
  } else if (memcmp (tag_ul, &role_or_identity_name_ul, 16) == 0) {
    self->role_or_identity_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  role or identity name = %s",
        GST_STR_NULL (self->role_or_identity_name));
  } else if (memcmp (tag_ul, &person_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->person_sets_uids, &self->n_person_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of person sets = %u", self->n_person_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_person_sets; i++) {
        GST_DEBUG ("    person sets %u = %s", i,
            mxf_uuid_to_string (&self->person_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &organisation_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->organisation_sets_uids,
            &self->n_organisation_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of organisation sets = %u", self->n_organisation_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_organisation_sets; i++) {
        GST_DEBUG ("    organisation sets %u = %s", i,
            mxf_uuid_to_string (&self->organisation_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_participant_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 participant local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_participant_init (MXFDMS1Participant * self)
{
}

static void
mxf_dms1_participant_class_init (MXFDMS1ParticipantClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_participant_finalize;
  metadatabase_class->handle_tag = mxf_dms1_participant_handle_tag;
  metadatabase_class->resolve = mxf_dms1_participant_resolve;
  dm_class->type = 0x180100;
}

G_DEFINE_ABSTRACT_TYPE (MXFDMS1Contact, mxf_dms1_contact,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_contact_finalize (GObject * object)
{
  MXFDMS1Contact *self = MXF_DMS1_CONTACT (object);

  g_free (self->name_value_sets_uids);
  self->name_value_sets_uids = NULL;

  g_free (self->name_value_sets);
  self->name_value_sets = NULL;

  g_free (self->address_sets_uids);
  self->address_sets_uids = NULL;

  g_free (self->address_sets);
  self->address_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_contact_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_contact_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Contact *self = MXF_DMS1_CONTACT (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->name_value_sets)
    memset (self->name_value_sets, 0,
        sizeof (gpointer) * self->n_name_value_sets);
  else
    self->name_value_sets =
        g_new0 (MXFDMS1NameValue *, self->n_name_value_sets);

  if (self->address_sets)
    memset (self->address_sets, 0, sizeof (gpointer) * self->n_address_sets);
  else
    self->address_sets = g_new0 (MXFDMS1Address *, self->n_address_sets);

  for (i = 0; i < self->n_name_value_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->name_value_sets_uids[i]);
    if (current && MXF_IS_DMS1_NAME_VALUE (current)) {
      self->name_value_sets[i] = MXF_DMS1_NAME_VALUE (current);
    }
  }

  for (i = 0; i < self->n_address_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->address_sets_uids[i]);
    if (current && MXF_IS_DMS1_ADDRESS (current)) {
      self->address_sets[i] = MXF_DMS1_ADDRESS (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_contact_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_contact_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Contact *self = MXF_DMS1_CONTACT (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 contact_uid_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x08, 0x01,
    0x01, 0x15, 0x40, 0x01, 0x02, 0x00, 0x00
  };
  static const guint8 name_value_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x1f, 0x02
  };
  static const guint8 address_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x17, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &contact_uid_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->contact_uid, tag_data, 16);
    GST_DEBUG ("  contact uid = %s", mxf_uuid_to_string (&self->contact_uid,
            str));
  } else if (memcmp (tag_ul, &name_value_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->name_value_sets_uids,
            &self->n_name_value_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of name-value sets = %u", self->n_name_value_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_name_value_sets; i++) {
        GST_DEBUG ("    name-value sets %u = %s", i,
            mxf_uuid_to_string (&self->name_value_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &address_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->address_sets_uids, &self->n_address_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of address sets = %u", self->n_address_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_address_sets; i++) {
        GST_DEBUG ("    address sets %u = %s", i,
            mxf_uuid_to_string (&self->address_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_contact_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 contact local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_contact_init (MXFDMS1Contact * self)
{
}

static void
mxf_dms1_contact_class_init (MXFDMS1ContactClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;

  object_class->finalize = mxf_dms1_contact_finalize;
  metadatabase_class->handle_tag = mxf_dms1_contact_handle_tag;
  metadatabase_class->resolve = mxf_dms1_contact_resolve;
}

G_DEFINE_TYPE (MXFDMS1Person, mxf_dms1_person, MXF_TYPE_DMS1_CONTACT);

static void
mxf_dms1_person_finalize (GObject * object)
{
  MXFDMS1Person *self = MXF_DMS1_PERSON (object);

  g_free (self->family_name);
  self->family_name = NULL;

  g_free (self->first_given_name);
  self->first_given_name = NULL;

  g_free (self->other_given_names);
  self->other_given_names = NULL;

  g_free (self->linking_name);
  self->linking_name = NULL;

  g_free (self->salutation);
  self->salutation = NULL;

  g_free (self->name_suffix);
  self->name_suffix = NULL;

  g_free (self->honours_qualifications);
  self->honours_qualifications = NULL;

  g_free (self->former_family_name);
  self->former_family_name = NULL;

  g_free (self->person_description);
  self->person_description = NULL;

  g_free (self->alternate_name);
  self->alternate_name = NULL;

  g_free (self->nationality);
  self->nationality = NULL;

  g_free (self->citizenship);
  self->citizenship = NULL;

  g_free (self->organisation_sets_uids);
  self->organisation_sets_uids = NULL;

  g_free (self->organisation_sets);
  self->organisation_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_person_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_person_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Person *self = MXF_DMS1_PERSON (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->organisation_sets)
    memset (self->organisation_sets, 0,
        sizeof (gpointer) * self->n_organisation_sets);
  else
    self->organisation_sets =
        g_new0 (MXFDMS1Organisation *, self->n_organisation_sets);

  for (i = 0; i < self->n_organisation_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->organisation_sets_uids[i]);
    if (current && MXF_IS_DMS1_ORGANISATION (current)) {
      self->organisation_sets[i] = MXF_DMS1_ORGANISATION (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_person_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_person_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Person *self = MXF_DMS1_PERSON (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 family_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x01, 0x01, 0x00
  };
  static const guint8 first_given_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x02, 0x01, 0x00
  };
  static const guint8 other_given_names_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x08, 0x01, 0x00
  };
  static const guint8 linking_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x0a, 0x01, 0x00
  };
  static const guint8 salutation_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x05, 0x01, 0x00
  };
  static const guint8 name_suffix_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x0b, 0x01, 0x00
  };
  static const guint8 honours_qualifications_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x06, 0x01, 0x00
  };
  static const guint8 former_family_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x0c, 0x01, 0x00
  };
  static const guint8 person_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x07, 0x01, 0x00
  };
  static const guint8 alternate_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x09, 0x01, 0x00
  };
  static const guint8 nationality_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x0d, 0x01, 0x00
  };
  static const guint8 citizenship_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x02,
    0x30, 0x06, 0x03, 0x01, 0x0e, 0x01, 0x00
  };
  static const guint8 organisation_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x15, 0x02
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &family_name_ul, 16) == 0) {
    self->family_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  family name = %s", GST_STR_NULL (self->family_name));
  } else if (memcmp (tag_ul, &first_given_name_ul, 16) == 0) {
    self->first_given_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  first given name = %s",
        GST_STR_NULL (self->first_given_name));
  } else if (memcmp (tag_ul, &other_given_names_ul, 16) == 0) {
    self->other_given_names = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  other given names = %s",
        GST_STR_NULL (self->other_given_names));
  } else if (memcmp (tag_ul, &linking_name_ul, 16) == 0) {
    self->linking_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  linking name = %s", GST_STR_NULL (self->linking_name));
  } else if (memcmp (tag_ul, &salutation_ul, 16) == 0) {
    self->salutation = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  salutation = %s", GST_STR_NULL (self->salutation));
  } else if (memcmp (tag_ul, &name_suffix_ul, 16) == 0) {
    self->name_suffix = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  name suffix = %s", GST_STR_NULL (self->name_suffix));
  } else if (memcmp (tag_ul, &honours_qualifications_ul, 16) == 0) {
    self->honours_qualifications = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  honours & qualifications = %s",
        GST_STR_NULL (self->honours_qualifications));
  } else if (memcmp (tag_ul, &former_family_name_ul, 16) == 0) {
    self->former_family_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  former family name = %s",
        GST_STR_NULL (self->former_family_name));
  } else if (memcmp (tag_ul, &person_description_ul, 16) == 0) {
    self->person_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  person description = %s",
        GST_STR_NULL (self->person_description));
  } else if (memcmp (tag_ul, &alternate_name_ul, 16) == 0) {
    self->alternate_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  alternate name = %s", GST_STR_NULL (self->alternate_name));
  } else if (memcmp (tag_ul, &nationality_ul, 16) == 0) {
    self->nationality = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  nationality = %s", GST_STR_NULL (self->nationality));
  } else if (memcmp (tag_ul, &citizenship_ul, 16) == 0) {
    self->citizenship = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  citizenship = %s", GST_STR_NULL (self->citizenship));
  } else if (memcmp (tag_ul, &organisation_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->organisation_sets_uids,
            &self->n_organisation_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of organisation sets = %u", self->n_organisation_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_organisation_sets; i++) {
        GST_DEBUG ("    organisation sets %u = %s", i,
            mxf_uuid_to_string (&self->organisation_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_person_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 person local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_person_init (MXFDMS1Person * self)
{
}

static void
mxf_dms1_person_class_init (MXFDMS1PersonClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_person_finalize;
  metadatabase_class->handle_tag = mxf_dms1_person_handle_tag;
  metadatabase_class->resolve = mxf_dms1_person_resolve;
  dm_class->type = 0x1a0200;
}

G_DEFINE_TYPE (MXFDMS1Organisation, mxf_dms1_organisation,
    MXF_TYPE_DMS1_CONTACT);

static void
mxf_dms1_organisation_finalize (GObject * object)
{
  MXFDMS1Organisation *self = MXF_DMS1_ORGANISATION (object);

  g_free (self->nature_of_organisation);
  self->nature_of_organisation = NULL;

  g_free (self->organisation_main_name);
  self->organisation_main_name = NULL;

  g_free (self->organisation_code);
  self->organisation_code = NULL;

  g_free (self->contact_department);
  self->contact_department = NULL;

  G_OBJECT_CLASS (mxf_dms1_organisation_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_organisation_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Organisation *self = MXF_DMS1_ORGANISATION (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 nature_of_organisation_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 organisation_main_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x06, 0x03, 0x03, 0x01, 0x01, 0x00
  };
  static const guint8 organisation_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x0a, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 contact_department_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x30, 0x06, 0x02, 0x01, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &nature_of_organisation_ul, 16) == 0) {
    self->nature_of_organisation = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  nature of organisation = %s",
        GST_STR_NULL (self->nature_of_organisation));
  } else if (memcmp (tag_ul, &organisation_main_name_ul, 16) == 0) {
    self->organisation_main_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  organisation main name = %s",
        GST_STR_NULL (self->organisation_main_name));
  } else if (memcmp (tag_ul, &organisation_code_ul, 16) == 0) {
    self->organisation_code = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  organisation code = %s",
        GST_STR_NULL (self->organisation_code));
  } else if (memcmp (tag_ul, &contact_department_ul, 16) == 0) {
    self->contact_department = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  contact department = %s",
        GST_STR_NULL (self->contact_department));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_organisation_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_organisation_init (MXFDMS1Organisation * self)
{
}

static void
mxf_dms1_organisation_class_init (MXFDMS1OrganisationClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_organisation_finalize;
  metadatabase_class->handle_tag = mxf_dms1_organisation_handle_tag;
  dm_class->type = 0x1a0300;
}

G_DEFINE_TYPE (MXFDMS1Location, mxf_dms1_location, MXF_TYPE_DMS1_CONTACT);

static void
mxf_dms1_location_finalize (GObject * object)
{
  MXFDMS1Location *self = MXF_DMS1_LOCATION (object);

  g_free (self->location_kind);
  self->location_kind = NULL;

  g_free (self->location_description);
  self->location_description = NULL;

  G_OBJECT_CLASS (mxf_dms1_location_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_location_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Location *self = MXF_DMS1_LOCATION (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 location_kind_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x01, 0x20, 0x02, 0x03, 0x01, 0x00, 0x00
  };
  static const guint8 location_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x01, 0x20, 0x02, 0x02, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &location_kind_ul, 16) == 0) {
    self->location_kind = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  location kind = %s", GST_STR_NULL (self->location_kind));
  } else if (memcmp (tag_ul, &location_description_ul, 16) == 0) {
    self->location_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  location description = %s",
        GST_STR_NULL (self->location_description));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_location_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_location_init (MXFDMS1Location * self)
{
}

static void
mxf_dms1_location_class_init (MXFDMS1LocationClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_location_finalize;
  metadatabase_class->handle_tag = mxf_dms1_location_handle_tag;
  dm_class->type = 0x1a0400;
}

G_DEFINE_TYPE (MXFDMS1Address, mxf_dms1_address, MXF_TYPE_DMS1);

static void
mxf_dms1_address_finalize (GObject * object)
{
  MXFDMS1Address *self = MXF_DMS1_ADDRESS (object);

  g_free (self->room_or_suite_number);
  self->room_or_suite_number = NULL;

  g_free (self->room_or_suite_name);
  self->room_or_suite_name = NULL;

  g_free (self->building_name);
  self->building_name = NULL;

  g_free (self->street_number);
  self->street_number = NULL;

  g_free (self->street_name);
  self->street_name = NULL;

  g_free (self->postal_town);
  self->postal_town = NULL;

  g_free (self->city);
  self->city = NULL;

  g_free (self->state_or_province_or_country);
  self->state_or_province_or_country = NULL;

  g_free (self->postal_code);
  self->postal_code = NULL;

  g_free (self->country);
  self->country = NULL;

  g_free (self->astronomical_body_name);
  self->astronomical_body_name = NULL;

  g_free (self->communications_sets_uids);
  self->communications_sets_uids = NULL;

  g_free (self->communications_sets);
  self->communications_sets = NULL;

  g_free (self->name_value_sets_uids);
  self->name_value_sets_uids = NULL;

  g_free (self->name_value_sets);
  self->name_value_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_address_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_address_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Address *self = MXF_DMS1_ADDRESS (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->communications_sets)
    memset (self->communications_sets, 0,
        sizeof (gpointer) * self->n_communications_sets);
  else
    self->communications_sets =
        g_new0 (MXFDMS1Communications *, self->n_communications_sets);

  if (self->name_value_sets)
    memset (self->name_value_sets, 0,
        sizeof (gpointer) * self->n_name_value_sets);
  else
    self->name_value_sets =
        g_new0 (MXFDMS1NameValue *, self->n_name_value_sets);

  for (i = 0; i < self->n_communications_sets; i++) {
    current =
        g_hash_table_lookup (metadata, &self->communications_sets_uids[i]);
    if (current && MXF_IS_DMS1_COMMUNICATIONS (current)) {
      self->communications_sets[i] = MXF_DMS1_COMMUNICATIONS (current);
    }
  }

  for (i = 0; i < self->n_name_value_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->name_value_sets_uids[i]);
    if (current && MXF_IS_DMS1_NAME_VALUE (current)) {
      self->name_value_sets[i] = MXF_DMS1_NAME_VALUE (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_address_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_address_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Address *self = MXF_DMS1_ADDRESS (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 room_or_suite_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x01, 0x01
  };
  static const guint8 room_or_suite_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x11, 0x01
  };
  static const guint8 building_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x12, 0x01
  };
  static const guint8 place_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x14, 0x01
  };
  static const guint8 street_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x02, 0x01
  };
  static const guint8 street_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x03, 0x01
  };
  static const guint8 postal_town_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x04, 0x01
  };
  static const guint8 city_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x05, 0x01
  };
  static const guint8 state_or_province_or_country_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x06, 0x01
  };
  static const guint8 postal_code_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x07, 0x01
  };
  static const guint8 country_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x08, 0x01
  };
  static const guint8 geographical_coordinate_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x15, 0x01
  };
  static const guint8 astronomical_body_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x07,
    0x01, 0x20, 0x01, 0x04, 0x01, 0x16, 0x01
  };
  static const guint8 communications_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x19, 0x00
  };
  static const guint8 name_value_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x07, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x1f, 0x04
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &room_or_suite_name_ul, 16) == 0) {
    self->room_or_suite_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  room or suite name = %s",
        GST_STR_NULL (self->room_or_suite_name));
  } else if (memcmp (tag_ul, &room_or_suite_number_ul, 16) == 0) {
    self->room_or_suite_number = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  room or suite number = %s",
        GST_STR_NULL (self->room_or_suite_number));
  } else if (memcmp (tag_ul, &building_name_ul, 16) == 0) {
    self->building_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  building name = %s", GST_STR_NULL (self->building_name));
  } else if (memcmp (tag_ul, &place_name_ul, 16) == 0) {
    self->place_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  place name = %s", GST_STR_NULL (self->place_name));
  } else if (memcmp (tag_ul, &street_number_ul, 16) == 0) {
    self->street_number = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  street number = %s", GST_STR_NULL (self->street_number));
  } else if (memcmp (tag_ul, &street_name_ul, 16) == 0) {
    self->street_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  street name = %s", GST_STR_NULL (self->street_name));
  } else if (memcmp (tag_ul, &postal_town_ul, 16) == 0) {
    self->postal_town = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  postal town = %s", GST_STR_NULL (self->postal_town));
  } else if (memcmp (tag_ul, &city_ul, 16) == 0) {
    self->city = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  city = %s", GST_STR_NULL (self->city));
  } else if (memcmp (tag_ul, &state_or_province_or_country_ul, 16) == 0) {
    self->state_or_province_or_country = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  state or province or country = %s",
        GST_STR_NULL (self->state_or_province_or_country));
  } else if (memcmp (tag_ul, &postal_code_ul, 16) == 0) {
    self->postal_code = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  postal code = %s", GST_STR_NULL (self->postal_code));
  } else if (memcmp (tag_ul, &country_ul, 16) == 0) {
    self->country = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  country = %s", GST_STR_NULL (self->country));
  } else if (memcmp (tag_ul, &geographical_coordinate_ul, 16) == 0) {
    if (tag_size != 12)
      goto error;

    memcpy (&self->geographical_coordinate, tag_data, 12);
    /* TODO implement */
  } else if (memcmp (tag_ul, &astronomical_body_name_ul, 16) == 0) {
    self->astronomical_body_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  astronomical body name = %s",
        GST_STR_NULL (self->astronomical_body_name));
  } else if (memcmp (tag_ul, &communications_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->communications_sets_uids,
            &self->n_communications_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of communications sets = %u",
        self->n_communications_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_communications_sets; i++) {
        GST_DEBUG ("    communications sets %u = %s", i,
            mxf_uuid_to_string (&self->communications_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &name_value_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->name_value_sets_uids,
            &self->n_name_value_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of name-value sets = %u", self->n_name_value_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_name_value_sets; i++) {
        GST_DEBUG ("    name-value sets %u = %s", i,
            mxf_uuid_to_string (&self->name_value_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_address_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 address local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_address_init (MXFDMS1Address * self)
{
}

static void
mxf_dms1_address_class_init (MXFDMS1AddressClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_address_finalize;
  metadatabase_class->handle_tag = mxf_dms1_address_handle_tag;
  metadatabase_class->resolve = mxf_dms1_address_resolve;
  dm_class->type = 0x1b0100;
}

G_DEFINE_TYPE (MXFDMS1Communications, mxf_dms1_communications, MXF_TYPE_DMS1);

static void
mxf_dms1_communications_finalize (GObject * object)
{
  MXFDMS1Communications *self = MXF_DMS1_COMMUNICATIONS (object);

  g_free (self->email_address);
  self->email_address = NULL;

  g_free (self->web_page);
  self->web_page = NULL;

  G_OBJECT_CLASS (mxf_dms1_communications_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_communications_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Communications *self = MXF_DMS1_COMMUNICATIONS (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 central_telephone_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x01, 0x20, 0x01, 0x10, 0x03, 0x04, 0x00
  };
  static const guint8 telephone_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x07,
    0x01, 0x20, 0x01, 0x10, 0x03, 0x01, 0x00
  };
  static const guint8 mobile_telephone_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x01, 0x20, 0x01, 0x10, 0x03, 0x05, 0x00
  };
  static const guint8 fax_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x07,
    0x01, 0x20, 0x01, 0x10, 0x03, 0x02, 0x00
  };
  static const guint8 email_address_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x10, 0x03, 0x03, 0x01
  };
  static const guint8 web_page_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x07,
    0x01, 0x20, 0x01, 0x10, 0x03, 0x06, 0x01
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &central_telephone_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;
    memcpy (self->central_telephone_number, tag_data, tag_size);

    GST_DEBUG ("  central telephone number = %s",
        self->central_telephone_number);
  } else if (memcmp (tag_ul, &telephone_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;
    memcpy (self->telephone_number, tag_data, tag_size);

    GST_DEBUG ("  telephone number = %s", self->telephone_number);
  } else if (memcmp (tag_ul, &mobile_telephone_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;
    memcpy (self->mobile_telephone_number, tag_data, tag_size);

    GST_DEBUG ("  mobile telephone number = %s", self->mobile_telephone_number);
  } else if (memcmp (tag_ul, &fax_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;
    memcpy (self->fax_number, tag_data, tag_size);

    GST_DEBUG ("  fax number = %s", self->fax_number);
  } else if (memcmp (tag_ul, &email_address_ul, 16) == 0) {
    self->email_address = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  email address = %s", GST_STR_NULL (self->email_address));
  } else if (memcmp (tag_ul, &web_page_ul, 16) == 0) {
    self->web_page = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  web page = %s", GST_STR_NULL (self->web_page));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_communications_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 communications local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_communications_init (MXFDMS1Communications * self)
{
}

static void
mxf_dms1_communications_class_init (MXFDMS1CommunicationsClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_communications_finalize;
  metadatabase_class->handle_tag = mxf_dms1_communications_handle_tag;
  dm_class->type = 0x1b0200;
}

G_DEFINE_TYPE (MXFDMS1Contract, mxf_dms1_contract, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_contract_finalize (GObject * object)
{
  MXFDMS1Contract *self = MXF_DMS1_CONTRACT (object);

  g_free (self->rights_sets_uids);
  self->rights_sets_uids = NULL;

  g_free (self->rights_sets);
  self->rights_sets = NULL;

  g_free (self->participant_sets_uids);
  self->participant_sets_uids = NULL;

  g_free (self->participant_sets);
  self->participant_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_contract_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_contract_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1Contract *self = MXF_DMS1_CONTRACT (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->rights_sets)
    memset (self->rights_sets, 0, sizeof (gpointer) * self->n_rights_sets);
  else
    self->rights_sets = g_new0 (MXFDMS1Rights *, self->n_rights_sets);

  if (self->participant_sets)
    memset (self->participant_sets, 0,
        sizeof (gpointer) * self->n_participant_sets);
  else
    self->participant_sets =
        g_new0 (MXFDMS1Participant *, self->n_participant_sets);

  for (i = 0; i < self->n_rights_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->rights_sets_uids[i]);
    if (current && MXF_IS_DMS1_RIGHTS (current)) {
      self->rights_sets[i] = MXF_DMS1_RIGHTS (current);
    }
  }

  for (i = 0; i < self->n_participant_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->participant_sets_uids[i]);
    if (current && MXF_IS_DMS1_PARTICIPANT (current)) {
      self->participant_sets[i] = MXF_DMS1_PARTICIPANT (current);
    }
  }

  return MXF_METADATA_BASE_CLASS (mxf_dms1_contract_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_contract_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Contract *self = MXF_DMS1_CONTRACT (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 supply_contract_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 rights_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x1a, 0x00
  };
  static const guint8 participant_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x13, 0x02
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &supply_contract_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->supply_contract_number, tag_data, tag_size);
    GST_DEBUG ("  supply contract number = %s", self->supply_contract_number);
  } else if (memcmp (tag_ul, &rights_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->rights_sets_uids, &self->n_rights_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of rights sets = %u", self->n_rights_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_rights_sets; i++) {
        GST_DEBUG ("    rights sets %u = %s", i,
            mxf_uuid_to_string (&self->rights_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &participant_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->participant_sets_uids,
            &self->n_participant_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of participant sets = %u", self->n_participant_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_participant_sets; i++) {
        GST_DEBUG ("    participant sets %u = %s", i,
            mxf_uuid_to_string (&self->participant_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_contract_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 contract local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_contract_init (MXFDMS1Contract * self)
{
}

static void
mxf_dms1_contract_class_init (MXFDMS1ContractClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_contract_finalize;
  metadatabase_class->handle_tag = mxf_dms1_contract_handle_tag;
  metadatabase_class->resolve = mxf_dms1_contract_resolve;
  dm_class->type = 0x1c0100;
}

G_DEFINE_TYPE (MXFDMS1Rights, mxf_dms1_rights, MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_rights_finalize (GObject * object)
{
  MXFDMS1Rights *self = MXF_DMS1_RIGHTS (object);

  g_free (self->copyright_owner);
  self->copyright_owner = NULL;

  g_free (self->rights_holder);
  self->rights_holder = NULL;

  g_free (self->rights_managment_authority);
  self->rights_managment_authority = NULL;

  g_free (self->region_or_area_of_ip_license);
  self->region_or_area_of_ip_license = NULL;

  g_free (self->intellectual_property_type);
  self->intellectual_property_type = NULL;

  g_free (self->right_condition);
  self->right_condition = NULL;

  g_free (self->right_remarks);
  self->right_remarks = NULL;

  g_free (self->intellectual_property_right);
  self->intellectual_property_right = NULL;

  G_OBJECT_CLASS (mxf_dms1_rights_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_rights_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Rights *self = MXF_DMS1_RIGHTS (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[32];
#endif
  static const guint8 copyright_owner_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x01, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 rights_holder_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 rights_managment_authority_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 region_or_area_of_ip_license_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x07,
    0x01, 0x20, 0x01, 0x03, 0x05, 0x01, 0x00
  };
  static const guint8 intellectual_property_type_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 right_condition_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x04, 0x03, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 right_remarks_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x08, 0x02,
    0x05, 0x04, 0x04, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 intellectual_property_right_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x02,
    0x05, 0x02, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 rights_start_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x02, 0x01, 0x20, 0x02, 0x00, 0x00, 0x00
  };
  static const guint8 rights_stop_date_and_time_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x07,
    0x02, 0x01, 0x20, 0x03, 0x00, 0x00, 0x00
  };
  static const guint8 maximum_number_of_usages_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x05, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &copyright_owner_ul, 16) == 0) {
    self->copyright_owner = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  copyright owner = %s", GST_STR_NULL (self->copyright_owner));
  } else if (memcmp (tag_ul, &rights_holder_ul, 16) == 0) {
    self->rights_holder = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  rights holder = %s", GST_STR_NULL (self->rights_holder));
  } else if (memcmp (tag_ul, &rights_managment_authority_ul, 16) == 0) {
    self->rights_managment_authority = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  rights management authority = %s",
        GST_STR_NULL (self->rights_managment_authority));
  } else if (memcmp (tag_ul, &region_or_area_of_ip_license_ul, 16) == 0) {
    self->region_or_area_of_ip_license = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  region or area of ip license = %s",
        GST_STR_NULL (self->region_or_area_of_ip_license));
  } else if (memcmp (tag_ul, &intellectual_property_type_ul, 16) == 0) {
    self->intellectual_property_type = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  intellectual property type = %s",
        GST_STR_NULL (self->intellectual_property_type));
  } else if (memcmp (tag_ul, &right_condition_ul, 16) == 0) {
    self->right_condition = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  right condition = %s", GST_STR_NULL (self->right_condition));
  } else if (memcmp (tag_ul, &right_remarks_ul, 16) == 0) {
    self->right_remarks = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  right remarks = %s", GST_STR_NULL (self->right_remarks));
  } else if (memcmp (tag_ul, &intellectual_property_right_ul, 16) == 0) {
    self->intellectual_property_right = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  intellectual property right = %s",
        GST_STR_NULL (self->intellectual_property_right));
  } else if (memcmp (tag_ul, &rights_start_date_and_time_ul, 16) == 0) {
    if (!mxf_timestamp_parse (&self->rights_start_date_and_time, tag_data,
            tag_size))
      goto error;

    GST_DEBUG ("  rights start date and time = %s",
        mxf_timestamp_to_string (&self->rights_start_date_and_time, str));
  } else if (memcmp (tag_ul, &rights_stop_date_and_time_ul, 16) == 0) {
    if (!mxf_timestamp_parse (&self->rights_stop_date_and_time, tag_data,
            tag_size))
      goto error;

    GST_DEBUG ("  rights stop date and time = %s",
        mxf_timestamp_to_string (&self->rights_stop_date_and_time, str));
  } else if (memcmp (tag_ul, &maximum_number_of_usages_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->maximum_number_of_usages = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  maximum number of usages = %u",
        self->maximum_number_of_usages);
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_rights_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 rights local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_rights_init (MXFDMS1Rights * self)
{
}

static void
mxf_dms1_rights_class_init (MXFDMS1RightsClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_rights_finalize;
  metadatabase_class->handle_tag = mxf_dms1_rights_handle_tag;
  dm_class->type = 0x1c0200;
}

G_DEFINE_TYPE (MXFDMS1PictureFormat, mxf_dms1_picture_format, MXF_TYPE_DMS1);

static void
mxf_dms1_picture_format_finalize (GObject * object)
{
  MXFDMS1PictureFormat *self = MXF_DMS1_PICTURE_FORMAT (object);

  g_free (self->colour_descriptor);
  self->colour_descriptor = NULL;

  G_OBJECT_CLASS (mxf_dms1_picture_format_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_picture_format_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1PictureFormat *self = MXF_DMS1_PICTURE_FORMAT (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 viewport_aspect_ratio_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x04,
    0x01, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00
  };
  static const guint8 perceived_display_format_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x04,
    0x01, 0x01, 0x01, 0x08, 0x00, 0x00, 0x00
  };
  static const guint8 colour_descriptor_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x03,
    0x02, 0x01, 0x06, 0x04, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &viewport_aspect_ratio_ul, 16) == 0) {
    if (!mxf_fraction_parse (&self->viewport_aspect_ratio, tag_data, tag_size))
      goto error;

    GST_DEBUG ("  viewport aspect ratio = %u/%u", self->viewport_aspect_ratio.n,
        self->viewport_aspect_ratio.d);
  } else if (memcmp (tag_ul, &perceived_display_format_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->perceived_display_format, tag_data, tag_size);
    GST_DEBUG ("  perceived display format = %s",
        self->perceived_display_format);
  } else if (memcmp (tag_ul, &colour_descriptor_ul, 16) == 0) {
    self->colour_descriptor = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  colour descriptor = %s",
        GST_STR_NULL (self->colour_descriptor));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_picture_format_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 picture format local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_picture_format_init (MXFDMS1PictureFormat * self)
{
}

static void
mxf_dms1_picture_format_class_init (MXFDMS1PictureFormatClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_picture_format_finalize;
  metadatabase_class->handle_tag = mxf_dms1_picture_format_handle_tag;
  dm_class->type = 0x1d0100;
}

G_DEFINE_TYPE (MXFDMS1DeviceParameters, mxf_dms1_device_parameters,
    MXF_TYPE_DMS1_THESAURUS);

static void
mxf_dms1_device_parameters_finalize (GObject * object)
{
  MXFDMS1DeviceParameters *self = MXF_DMS1_DEVICE_PARAMETERS (object);

  g_free (self->device_type);
  self->device_type = NULL;

  g_free (self->manufacturer);
  self->manufacturer = NULL;

  g_free (self->device_model);
  self->device_model = NULL;

  g_free (self->device_serial_number);
  self->device_serial_number = NULL;

  g_free (self->device_usage_description);
  self->device_usage_description = NULL;

  g_free (self->name_value_sets_uids);
  self->name_value_sets_uids = NULL;

  g_free (self->name_value_sets);
  self->name_value_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_device_parameters_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_device_parameters_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1DeviceParameters *self = MXF_DMS1_DEVICE_PARAMETERS (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->name_value_sets)
    memset (self->name_value_sets, 0,
        sizeof (gpointer) * self->n_name_value_sets);
  else
    self->name_value_sets =
        g_new0 (MXFDMS1NameValue *, self->n_name_value_sets);

  for (i = 0; i < self->n_name_value_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->name_value_sets_uids[i]);
    if (current && MXF_IS_DMS1_NAME_VALUE (current)) {
      self->name_value_sets[i] = MXF_DMS1_NAME_VALUE (current);
    }
  }

  return
      MXF_METADATA_BASE_CLASS (mxf_dms1_device_parameters_parent_class)->resolve
      (m, metadata);
}

static gboolean
mxf_dms1_device_parameters_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1DeviceParameters *self = MXF_DMS1_DEVICE_PARAMETERS (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 device_type_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x01, 0x20, 0x08, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 device_designation_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 device_asset_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x01, 0x20, 0x0c, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 ieee_device_identifier_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x01,
    0x01, 0x20, 0x05, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 manufacturer_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x01,
    0x0a, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 device_model_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x20, 0x03, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 device_serial_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x20, 0x04, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 device_usage_description_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x03, 0x03, 0x10, 0x01, 0x01, 0x00, 0x00
  };
  static const guint8 name_value_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x05, 0x40, 0x1f, 0x03
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &device_type_ul, 16) == 0) {
    self->device_type = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  device type = %s", GST_STR_NULL (self->device_type));
  } else if (memcmp (tag_ul, &device_designation_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->device_designation, tag_data, tag_size);
    GST_DEBUG ("  device designation = %s", self->device_designation);
  } else if (memcmp (tag_ul, &device_asset_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->device_asset_number, tag_data, tag_size);
    GST_DEBUG ("  device asset number = %s", self->device_asset_number);
  } else if (memcmp (tag_ul, &ieee_device_identifier_ul, 16) == 0) {
    if (tag_size != 6)
      goto error;

    memcpy (self->ieee_device_identifier, tag_data, 6);
    GST_DEBUG
        ("  IEEE device identifier = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
        self->ieee_device_identifier[0], self->ieee_device_identifier[1],
        self->ieee_device_identifier[2], self->ieee_device_identifier[3],
        self->ieee_device_identifier[4], self->ieee_device_identifier[5]);
  } else if (memcmp (tag_ul, &manufacturer_ul, 16) == 0) {
    self->manufacturer = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  manufacturer = %s", GST_STR_NULL (self->manufacturer));
  } else if (memcmp (tag_ul, &device_model_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->device_model, tag_data, tag_size);
    GST_DEBUG ("  device model = %s", self->device_model);
  } else if (memcmp (tag_ul, &device_serial_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->device_serial_number, tag_data, tag_size);
    GST_DEBUG ("  device serial number = %s", self->device_serial_number);
  } else if (memcmp (tag_ul, &device_usage_description_ul, 16) == 0) {
    self->device_usage_description = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  device usage description = %s",
        GST_STR_NULL (self->device_usage_description));
  } else if (memcmp (tag_ul, &name_value_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->name_value_sets_uids,
            &self->n_name_value_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of name-value sets = %u", self->n_name_value_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_name_value_sets; i++) {
        GST_DEBUG ("    name-value sets %u = %s", i,
            mxf_uuid_to_string (&self->name_value_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_device_parameters_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 device parameters local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_device_parameters_init (MXFDMS1DeviceParameters * self)
{
}

static void
mxf_dms1_device_parameters_class_init (MXFDMS1DeviceParametersClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_device_parameters_finalize;
  metadatabase_class->handle_tag = mxf_dms1_device_parameters_handle_tag;
  metadatabase_class->resolve = mxf_dms1_device_parameters_resolve;
  dm_class->type = 0x1e0100;
}

G_DEFINE_TYPE (MXFDMS1NameValue, mxf_dms1_name_value, MXF_TYPE_DMS1);

static void
mxf_dms1_name_value_finalize (GObject * object)
{
  MXFDMS1NameValue *self = MXF_DMS1_NAME_VALUE (object);

  g_free (self->item_name);
  self->item_name = NULL;

  g_free (self->item_value);
  self->item_value = NULL;

  G_OBJECT_CLASS (mxf_dms1_name_value_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_name_value_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1NameValue *self = MXF_DMS1_NAME_VALUE (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 item_name_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x01, 0x02, 0x0a, 0x01, 0x01, 0x00, 0x00
  };
  static const guint8 item_value_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x03,
    0x01, 0x02, 0x0a, 0x02, 0x01, 0x00, 0x00
  };
  static const guint8 smpte_universal_label_locator_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x01,
    0x02, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &item_name_ul, 16) == 0) {
    self->item_name = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  item name = %s", GST_STR_NULL (self->item_name));
  } else if (memcmp (tag_ul, &item_value_ul, 16) == 0) {
    self->item_value = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  item value = %s", GST_STR_NULL (self->item_value));
  } else if (memcmp (tag_ul, &smpte_universal_label_locator_ul, 16) == 0) {
    if (tag_size != 16)
      goto error;

    memcpy (&self->smpte_universal_label_locator, tag_data, 16);
    GST_DEBUG ("  SMPTE universal label locator = %s",
        mxf_uuid_to_string (&self->smpte_universal_label_locator, str));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_name_value_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 name-value local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_name_value_init (MXFDMS1NameValue * self)
{
}

static void
mxf_dms1_name_value_class_init (MXFDMS1NameValueClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_name_value_finalize;
  metadatabase_class->handle_tag = mxf_dms1_name_value_handle_tag;
  dm_class->type = 0x1f0100;
}

G_DEFINE_TYPE (MXFDMS1Processing, mxf_dms1_processing, MXF_TYPE_DMS1);

static void
mxf_dms1_processing_finalize (GObject * object)
{
  MXFDMS1Processing *self = MXF_DMS1_PROCESSING (object);

  g_free (self->descriptive_comment);
  self->descriptive_comment = NULL;

  g_free (self->graphic_usage_type);
  self->graphic_usage_type = NULL;

  G_OBJECT_CLASS (mxf_dms1_processing_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_processing_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1Processing *self = MXF_DMS1_PROCESSING (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 quality_flag_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x05,
    0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 descriptive_comment_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x03,
    0x02, 0x03, 0x02, 0x02, 0x01, 0x00, 0x00
  };
  static const guint8 logo_flag_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x03, 0x05,
    0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 graphic_usage_type_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x05,
    0x01, 0x01, 0x07, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 process_steps_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x05,
    0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 generation_copy_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x05,
    0x01, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00
  };
  static const guint8 generation_clone_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x05,
    0x01, 0x03, 0x03, 0x01, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &quality_flag_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;

    self->quality_flag = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  quality flag = %u", self->quality_flag);
  } else if (memcmp (tag_ul, &descriptive_comment_ul, 16) == 0) {
    self->descriptive_comment = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  descriptive comment = %s",
        GST_STR_NULL (self->descriptive_comment));
  } else if (memcmp (tag_ul, &logo_flag_ul, 16) == 0) {
    if (tag_size != 1)
      goto error;

    self->logo_flag = GST_READ_UINT8 (tag_data);
    GST_DEBUG ("  logo flag = %u", self->logo_flag);
  } else if (memcmp (tag_ul, &graphic_usage_type_ul, 16) == 0) {
    self->graphic_usage_type = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  graphic usage type = %s",
        GST_STR_NULL (self->graphic_usage_type));
  } else if (memcmp (tag_ul, &process_steps_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->process_steps = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  process steps = %u", self->process_steps);
  } else if (memcmp (tag_ul, &generation_copy_number_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->generation_copy_number = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  generation copy number = %u", self->generation_copy_number);
  } else if (memcmp (tag_ul, &generation_clone_number_ul, 16) == 0) {
    if (tag_size != 2)
      goto error;

    self->generation_clone_number = GST_READ_UINT16_BE (tag_data);
    GST_DEBUG ("  generation clone number = %u", self->generation_clone_number);
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_processing_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 processing local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_processing_init (MXFDMS1Processing * self)
{
}

static void
mxf_dms1_processing_class_init (MXFDMS1ProcessingClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_processing_finalize;
  metadatabase_class->handle_tag = mxf_dms1_processing_handle_tag;
  dm_class->type = 0x200100;
}

G_DEFINE_TYPE (MXFDMS1Project, mxf_dms1_project, MXF_TYPE_DMS1);

static void
mxf_dms1_project_finalize (GObject * object)
{
  MXFDMS1Project *self = MXF_DMS1_PROJECT (object);

  g_free (self->project_name_or_title);
  self->project_name_or_title = NULL;

  G_OBJECT_CLASS (mxf_dms1_project_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_project_handle_tag (MXFMetadataBase * metadata, MXFPrimerPack * primer,
    guint16 tag, const guint8 * tag_data, guint tag_size)
{
  MXFDMS1Project *self = MXF_DMS1_PROJECT (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 project_number_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x01,
    0x03, 0x01, 0x06, 0x00, 0x00, 0x00, 0x00
  };
  static const guint8 project_name_or_title_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x01,
    0x03, 0x01, 0x08, 0x01, 0x00, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &project_number_ul, 16) == 0) {
    if (tag_size > 32)
      goto error;

    memcpy (self->project_number, tag_data, tag_size);

    GST_DEBUG ("  project number = %s", self->project_number);
  } else if (memcmp (tag_ul, &project_name_or_title_ul, 16) == 0) {
    self->project_name_or_title = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  project name or title = %s",
        GST_STR_NULL (self->project_name_or_title));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_project_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 project local tag 0x%04x of size %u", tag, tag_size);

  return FALSE;
}

static void
mxf_dms1_project_init (MXFDMS1Project * self)
{
}

static void
mxf_dms1_project_class_init (MXFDMS1ProjectClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_project_finalize;
  metadatabase_class->handle_tag = mxf_dms1_project_handle_tag;
  dm_class->type = 0x200200;
}

G_DEFINE_TYPE (MXFDMS1ContactsList, mxf_dms1_contacts_list, MXF_TYPE_DMS1);

static void
mxf_dms1_contacts_list_finalize (GObject * object)
{
  MXFDMS1ContactsList *self = MXF_DMS1_CONTACTS_LIST (object);

  g_free (self->person_sets_uids);
  self->person_sets_uids = NULL;

  g_free (self->person_sets);
  self->person_sets = NULL;

  g_free (self->organisation_sets_uids);
  self->organisation_sets_uids = NULL;

  g_free (self->organisation_sets);
  self->organisation_sets = NULL;

  g_free (self->location_sets_uids);
  self->location_sets_uids = NULL;

  g_free (self->location_sets);
  self->location_sets = NULL;

  G_OBJECT_CLASS (mxf_dms1_contacts_list_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_contacts_list_resolve (MXFMetadataBase * m, GHashTable * metadata)
{
  MXFDMS1ContactsList *self = MXF_DMS1_CONTACTS_LIST (m);
  MXFMetadataBase *current = NULL;
  guint i;

  if (self->person_sets)
    memset (self->person_sets, 0, sizeof (gpointer) * self->n_person_sets);
  else
    self->person_sets = g_new0 (MXFDMS1Person *, self->n_person_sets);

  if (self->organisation_sets)
    memset (self->organisation_sets, 0,
        sizeof (gpointer) * self->n_organisation_sets);
  else
    self->organisation_sets =
        g_new0 (MXFDMS1Organisation *, self->n_organisation_sets);

  if (self->location_sets)
    memset (self->location_sets, 0, sizeof (gpointer) * self->n_location_sets);
  else
    self->location_sets = g_new0 (MXFDMS1Location *, self->n_location_sets);

  for (i = 0; i < self->n_person_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->person_sets_uids[i]);
    if (current && MXF_IS_DMS1_PERSON (current)) {
      self->person_sets[i] = MXF_DMS1_PERSON (current);
    }
  }

  for (i = 0; i < self->n_organisation_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->organisation_sets_uids[i]);
    if (current && MXF_IS_DMS1_ORGANISATION (current)) {
      self->organisation_sets[i] = MXF_DMS1_ORGANISATION (current);
    }
  }

  for (i = 0; i < self->n_location_sets; i++) {
    current = g_hash_table_lookup (metadata, &self->location_sets_uids[i]);
    if (current && MXF_IS_DMS1_LOCATION (current)) {
      self->location_sets[i] = MXF_DMS1_LOCATION (current);
    }
  }

  return
      MXF_METADATA_BASE_CLASS (mxf_dms1_contacts_list_parent_class)->resolve (m,
      metadata);
}

static gboolean
mxf_dms1_contacts_list_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1ContactsList *self = MXF_DMS1_CONTACTS_LIST (metadata);
  gboolean ret = TRUE;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif
  MXFUL *tag_ul = NULL;
  static const guint8 person_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x14, 0x00
  };
  static const guint8 organisation_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x15, 0x00
  };
  static const guint8 location_sets_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x05, 0x06,
    0x01, 0x01, 0x04, 0x03, 0x40, 0x16, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &person_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->person_sets_uids, &self->n_person_sets,
            tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of person sets = %u", self->n_person_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_person_sets; i++) {
        GST_DEBUG ("    person sets %u = %s", i,
            mxf_uuid_to_string (&self->person_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &organisation_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->organisation_sets_uids,
            &self->n_organisation_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of organisation sets = %u", self->n_organisation_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_organisation_sets; i++) {
        GST_DEBUG ("    organisation sets %u = %s", i,
            mxf_uuid_to_string (&self->organisation_sets_uids[i], str));
      }
    }
#endif
  } else if (memcmp (tag_ul, &location_sets_ul, 16) == 0) {
    if (!mxf_uuid_array_parse (&self->location_sets_uids,
            &self->n_location_sets, tag_data, tag_size))
      goto error;
    GST_DEBUG ("  number of location sets = %u", self->n_location_sets);
#ifndef GST_DISABLE_GST_DEBUG
    {
      guint i;
      for (i = 0; i < self->n_location_sets; i++) {
        GST_DEBUG ("    location sets %u = %s", i,
            mxf_uuid_to_string (&self->location_sets_uids[i], str));
      }
    }
#endif
  } else {
    ret =
        MXF_METADATA_BASE_CLASS
        (mxf_dms1_contacts_list_parent_class)->handle_tag (metadata, primer,
        tag, tag_data, tag_size);
  }

  return ret;

error:

  GST_ERROR ("Invalid DMS1 contacts list local tag 0x%04x of size %u", tag,
      tag_size);

  return FALSE;
}

static void
mxf_dms1_contacts_list_init (MXFDMS1ContactsList * self)
{
}

static void
mxf_dms1_contacts_list_class_init (MXFDMS1ContactsListClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_contacts_list_finalize;
  metadatabase_class->handle_tag = mxf_dms1_contacts_list_handle_tag;
  metadatabase_class->resolve = mxf_dms1_contacts_list_resolve;
  dm_class->type = 0x190100;
}

G_DEFINE_TYPE (MXFDMS1CueWords, mxf_dms1_cue_words,
    MXF_TYPE_DMS1_TEXT_LANGUAGE);

static void
mxf_dms1_cue_words_finalize (GObject * object)
{
  MXFDMS1CueWords *self = MXF_DMS1_CUE_WORDS (object);

  g_free (self->in_cue_words);
  self->in_cue_words = NULL;

  g_free (self->out_cue_words);
  self->out_cue_words = NULL;

  G_OBJECT_CLASS (mxf_dms1_cue_words_parent_class)->finalize (object);
}

static gboolean
mxf_dms1_cue_words_handle_tag (MXFMetadataBase * metadata,
    MXFPrimerPack * primer, guint16 tag, const guint8 * tag_data,
    guint tag_size)
{
  MXFDMS1CueWords *self = MXF_DMS1_CUE_WORDS (metadata);
  gboolean ret = TRUE;
  MXFUL *tag_ul = NULL;
  static const guint8 in_cue_words_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x01, 0x02, 0x0d, 0x01, 0x00, 0x00
  };
  static const guint8 out_cue_words_ul[] = {
    0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x04, 0x03,
    0x02, 0x01, 0x02, 0x0e, 0x01, 0x00, 0x00
  };

  if (!(tag_ul =
          (MXFUL *) g_hash_table_lookup (primer->mappings,
              GUINT_TO_POINTER (((guint) tag)))))
    return FALSE;

  if (memcmp (tag_ul, &in_cue_words_ul, 16) == 0) {
    self->in_cue_words = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  in cue words = %s", GST_STR_NULL (self->in_cue_words));
  } else if (memcmp (tag_ul, &out_cue_words_ul, 16) == 0) {
    self->out_cue_words = mxf_utf16_to_utf8 (tag_data, tag_size);
    GST_DEBUG ("  out cue words = %s", GST_STR_NULL (self->out_cue_words));
  } else {
    ret =
        MXF_METADATA_BASE_CLASS (mxf_dms1_cue_words_parent_class)->handle_tag
        (metadata, primer, tag, tag_data, tag_size);
  }

  return ret;
}

static void
mxf_dms1_cue_words_init (MXFDMS1CueWords * self)
{
}

static void
mxf_dms1_cue_words_class_init (MXFDMS1CueWordsClass * klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  MXFMetadataBaseClass *metadatabase_class = (MXFMetadataBaseClass *) klass;
  MXFDescriptiveMetadataClass *dm_class = (MXFDescriptiveMetadataClass *) klass;

  object_class->finalize = mxf_dms1_cue_words_finalize;
  metadatabase_class->handle_tag = mxf_dms1_cue_words_handle_tag;
  dm_class->type = 0x170800;
}

#define _add_dm_type(type) G_STMT_START { \
  GType tmp = type; \
  \
  g_array_append_val (dms1_sets, tmp); \
} G_STMT_END

void
mxf_dms1_initialize (void)
{
  GArray *dms1_sets = g_array_new (TRUE, TRUE, sizeof (GType));

  _add_dm_type (MXF_TYPE_DMS1_PRODUCTION_FRAMEWORK);
  _add_dm_type (MXF_TYPE_DMS1_CLIP_FRAMEWORK);
  _add_dm_type (MXF_TYPE_DMS1_SCENE_FRAMEWORK);
  _add_dm_type (MXF_TYPE_DMS1_TITLES);
  _add_dm_type (MXF_TYPE_DMS1_IDENTIFICATION);
  _add_dm_type (MXF_TYPE_DMS1_GROUP_RELATIONSHIP);
  _add_dm_type (MXF_TYPE_DMS1_BRANDING);
  _add_dm_type (MXF_TYPE_DMS1_EVENT);
  _add_dm_type (MXF_TYPE_DMS1_PUBLICATION);
  _add_dm_type (MXF_TYPE_DMS1_AWARD);
  _add_dm_type (MXF_TYPE_DMS1_CAPTIONS_DESCRIPTION);
  _add_dm_type (MXF_TYPE_DMS1_ANNOTATION);
  _add_dm_type (MXF_TYPE_DMS1_SETTING_PERIOD);
  _add_dm_type (MXF_TYPE_DMS1_SCRIPTING);
  _add_dm_type (MXF_TYPE_DMS1_CLASSIFICATION);
  _add_dm_type (MXF_TYPE_DMS1_SHOT);
  _add_dm_type (MXF_TYPE_DMS1_KEY_POINT);
  _add_dm_type (MXF_TYPE_DMS1_PARTICIPANT);
  _add_dm_type (MXF_TYPE_DMS1_PERSON);
  _add_dm_type (MXF_TYPE_DMS1_ORGANISATION);
  _add_dm_type (MXF_TYPE_DMS1_LOCATION);
  _add_dm_type (MXF_TYPE_DMS1_ADDRESS);
  _add_dm_type (MXF_TYPE_DMS1_COMMUNICATIONS);
  _add_dm_type (MXF_TYPE_DMS1_CONTRACT);
  _add_dm_type (MXF_TYPE_DMS1_RIGHTS);
  _add_dm_type (MXF_TYPE_DMS1_PICTURE_FORMAT);
  _add_dm_type (MXF_TYPE_DMS1_DEVICE_PARAMETERS);
  _add_dm_type (MXF_TYPE_DMS1_NAME_VALUE);
  _add_dm_type (MXF_TYPE_DMS1_PROCESSING);
  _add_dm_type (MXF_TYPE_DMS1_PROJECT);
  _add_dm_type (MXF_TYPE_DMS1_CONTACTS_LIST);
  _add_dm_type (MXF_TYPE_DMS1_CUE_WORDS);

  mxf_descriptive_metadata_register (0x01, (GType *) g_array_free (dms1_sets,
          FALSE));
}

#undef _add_dm_type

#undef ADD_SET
