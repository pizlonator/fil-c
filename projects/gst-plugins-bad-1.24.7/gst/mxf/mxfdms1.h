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

/* Implementation of SMPTE S380M - Descriptive  Scheme-1 */

#ifndef _MXF_DMS1_H_
#define _MXF_DMS1_H_

#include <gst/gst.h>
#include "mxfmetadata.h"

#define MXF_TYPE_DMS1 \
  (mxf_dms1_get_type())
#define MXF_DMS1(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1, MXFDMS1))
#define MXF_IS_DMS1(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1))
typedef struct _MXFDMS1 MXFDMS1;
typedef MXFDescriptiveMetadataClass MXFDMS1Class;
GType mxf_dms1_get_type (void);

#define MXF_TYPE_DMS1_TEXT_LANGUAGE \
  (mxf_dms1_text_language_get_type())
#define MXF_DMS1_TEXT_LANGUAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_TEXT_LANGUAGE, MXFDMS1TextLanguage))
#define MXF_IS_DMS1_TEXT_LANGUAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_TEXT_LANGUAGE))
typedef struct _MXFDMS1TextLanguage MXFDMS1TextLanguage;
typedef MXFDescriptiveMetadataClass MXFDMS1TextLanguageClass;
GType mxf_dms1_text_language_get_type (void);

#define MXF_TYPE_DMS1_THESAURUS \
  (mxf_dms1_thesaurus_get_type())
#define MXF_DMS1_THESAURUS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_THESAURUS, MXFDMS1Thesaurus))
#define MXF_IS_DMS1_THESAURUS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_THESAURUS))
typedef struct _MXFDMS1Thesaurus MXFDMS1Thesaurus;
typedef MXFDescriptiveMetadataClass MXFDMS1ThesaurusClass;
GType mxf_dms1_thesaurus_get_type (void);

#define MXF_TYPE_DMS1_FRAMEWORK \
  (mxf_dms1_framework_get_type())
#define MXF_DMS1_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_FRAMEWORK, MXFDMS1Framework))
#define MXF_IS_DMS1_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_FRAMEWORK))
typedef struct _MXFDMS1Framework MXFDMS1Framework;
typedef MXFDescriptiveMetadataClass MXFDMS1FrameworkClass;
GType mxf_dms1_framework_get_type (void);

#define MXF_TYPE_DMS1_PRODUCTION_CLIP_FRAMEWORK \
  (mxf_dms1_production_clip_framework_get_type())
#define MXF_DMS1_PRODUCTION_CLIP_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PRODUCTION_CLIP_FRAMEWORK, MXFDMS1ProductionClipFramework))
#define MXF_IS_DMS1_PRODUCTION_CLIP_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PRODUCTION_CLIP_FRAMEWORK))
typedef struct _MXFDMS1ProductionClipFramework MXFDMS1ProductionClipFramework;
typedef MXFDescriptiveMetadataClass MXFDMS1ProductionClipFrameworkClass;
GType mxf_dms1_production_clip_framework_get_type (void);

#define MXF_TYPE_DMS1_PRODUCTION_FRAMEWORK \
  (mxf_dms1_production_framework_get_type())
#define MXF_DMS1_PRODUCTION_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PRODUCTION_FRAMEWORK, MXFDMS1ProductionFramework))
#define MXF_IS_DMS1_PRODUCTION_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PRODUCTION_FRAMEWORK))
typedef struct _MXFDMS1ProductionFramework MXFDMS1ProductionFramework;
typedef MXFDescriptiveMetadataClass MXFDMS1ProductionFrameworkClass;
GType mxf_dms1_production_framework_get_type (void);

#define MXF_TYPE_DMS1_CLIP_FRAMEWORK \
  (mxf_dms1_clip_framework_get_type())
#define MXF_DMS1_CLIP_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CLIP_FRAMEWORK, MXFDMS1ClipFramework))
#define MXF_IS_DMS1_CLIP_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CLIP_FRAMEWORK))
typedef struct _MXFDMS1ClipFramework MXFDMS1ClipFramework;
typedef MXFDescriptiveMetadataClass MXFDMS1ClipFrameworkClass;
GType mxf_dms1_clip_framework_get_type (void);

#define MXF_TYPE_DMS1_SCENE_FRAMEWORK \
  (mxf_dms1_scene_framework_get_type())
#define MXF_DMS1_SCENE_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_SCENE_FRAMEWORK, MXFDMS1SceneFramework))
#define MXF_IS_DMS1_SCENE_FRAMEWORK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_SCENE_FRAMEWORK))
typedef struct _MXFDMS1SceneFramework MXFDMS1SceneFramework;
typedef MXFDescriptiveMetadataClass MXFDMS1SceneFrameworkClass;
GType mxf_dms1_scene_framework_get_type (void);

#define MXF_TYPE_DMS1_TITLES \
  (mxf_dms1_titles_get_type())
#define MXF_DMS1_TITLES(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_TITLES, MXFDMS1Titles))
#define MXF_IS_DMS1_TITLES(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_TITLES))
typedef struct _MXFDMS1Titles MXFDMS1Titles;
typedef MXFDescriptiveMetadataClass MXFDMS1TitlesClass;
GType mxf_dms1_titles_get_type (void);

#define MXF_TYPE_DMS1_IDENTIFICATION \
  (mxf_dms1_identification_get_type())
#define MXF_DMS1_IDENTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_IDENTIFICATION, MXFDMS1Identification))
#define MXF_IS_DMS1_IDENTIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_IDENTIFICATION))
typedef struct _MXFDMS1Identification MXFDMS1Identification;
typedef MXFDescriptiveMetadataClass MXFDMS1IdentificationClass;
GType mxf_dms1_identification_get_type (void);

#define MXF_TYPE_DMS1_GROUP_RELATIONSHIP \
  (mxf_dms1_group_relationship_get_type())
#define MXF_DMS1_GROUP_RELATIONSHIP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_GROUP_RELATIONSHIP, MXFDMS1GroupRelationship))
#define MXF_IS_DMS1_GROUP_RELATIONSHIP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_GROUP_RELATIONSHIP))
typedef struct _MXFDMS1GroupRelationship MXFDMS1GroupRelationship;
typedef MXFDescriptiveMetadataClass MXFDMS1GroupRelationshipClass;
GType mxf_dms1_group_relationship_get_type (void);

#define MXF_TYPE_DMS1_BRANDING \
  (mxf_dms1_branding_get_type())
#define MXF_DMS1_BRANDING(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_BRANDING, MXFDMS1Branding))
#define MXF_IS_DMS1_BRANDING(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_BRANDING))
typedef struct _MXFDMS1Branding MXFDMS1Branding;
typedef MXFDescriptiveMetadataClass MXFDMS1BrandingClass;
GType mxf_dms1_branding_get_type (void);

#define MXF_TYPE_DMS1_EVENT \
  (mxf_dms1_event_get_type())
#define MXF_DMS1_EVENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_EVENT, MXFDMS1Event))
#define MXF_IS_DMS1_EVENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_EVENT))
typedef struct _MXFDMS1Event MXFDMS1Event;
typedef MXFDescriptiveMetadataClass MXFDMS1EventClass;
GType mxf_dms1_event_get_type (void);

#define MXF_TYPE_DMS1_PUBLICATION \
  (mxf_dms1_publication_get_type())
#define MXF_DMS1_PUBLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PUBLICATION, MXFDMS1Publication))
#define MXF_IS_DMS1_PUBLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PUBLICATION))
typedef struct _MXFDMS1Publication MXFDMS1Publication;
typedef MXFDescriptiveMetadataClass MXFDMS1PublicationClass;
GType mxf_dms1_publication_get_type (void);

#define MXF_TYPE_DMS1_AWARD \
  (mxf_dms1_award_get_type())
#define MXF_DMS1_AWARD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_AWARD, MXFDMS1Award))
#define MXF_IS_DMS1_AWARD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_AWARD))
typedef struct _MXFDMS1Award MXFDMS1Award;
typedef MXFDescriptiveMetadataClass MXFDMS1AwardClass;
GType mxf_dms1_award_get_type (void);

#define MXF_TYPE_DMS1_CAPTIONS_DESCRIPTION \
  (mxf_dms1_captions_description_get_type())
#define MXF_DMS1_CAPTIONS_DESCRIPTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CAPTIONS_DESCRIPTION, MXFDMS1CaptionsDescription))
#define MXF_IS_DMS1_CAPTIONS_DESCRIPTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CAPTIONS_DESCRIPTION))
typedef struct _MXFDMS1CaptionsDescription MXFDMS1CaptionsDescription;
typedef MXFDescriptiveMetadataClass MXFDMS1CaptionsDescriptionClass;
GType mxf_dms1_captions_description_get_type (void);

#define MXF_TYPE_DMS1_ANNOTATION \
  (mxf_dms1_annotation_get_type())
#define MXF_DMS1_ANNOTATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_ANNOTATION, MXFDMS1Annotation))
#define MXF_IS_DMS1_ANNOTATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_ANNOTATION))
typedef struct _MXFDMS1Annotation MXFDMS1Annotation;
typedef MXFDescriptiveMetadataClass MXFDMS1AnnotationClass;
GType mxf_dms1_annotation_get_type (void);

#define MXF_TYPE_DMS1_SETTING_PERIOD \
  (mxf_dms1_setting_period_get_type())
#define MXF_DMS1_SETTING_PERIOD(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_SETTING_PERIOD, MXFDMS1SettingPeriod))
#define MXF_IS_DMS1_SETTING_PERIOD(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_SETTING_PERIOD))
typedef struct _MXFDMS1SettingPeriod MXFDMS1SettingPeriod;
typedef MXFDescriptiveMetadataClass MXFDMS1SettingPeriodClass;
GType mxf_dms1_setting_period_get_type (void);

#define MXF_TYPE_DMS1_SCRIPTING \
  (mxf_dms1_scripting_get_type())
#define MXF_DMS1_SCRIPTING(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_SCRIPTING, MXFDMS1Scripting))
#define MXF_IS_DMS1_SCRIPTING(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_SCRIPTING))
typedef struct _MXFDMS1Scripting MXFDMS1Scripting;
typedef MXFDescriptiveMetadataClass MXFDMS1ScriptingClass;
GType mxf_dms1_scripting_get_type (void);

#define MXF_TYPE_DMS1_CLASSIFICATION \
  (mxf_dms1_classification_get_type())
#define MXF_DMS1_CLASSIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CLASSIFICATION, MXFDMS1Classification))
#define MXF_IS_DMS1_CLASSIFICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CLASSIFICATION))
typedef struct _MXFDMS1Classification MXFDMS1Classification;
typedef MXFDescriptiveMetadataClass MXFDMS1ClassificationClass;
GType mxf_dms1_classification_get_type (void);

#define MXF_TYPE_DMS1_SHOT \
  (mxf_dms1_shot_get_type())
#define MXF_DMS1_SHOT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_SHOT, MXFDMS1Shot))
#define MXF_IS_DMS1_SHOT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_SHOT))
typedef struct _MXFDMS1Shot MXFDMS1Shot;
typedef MXFDescriptiveMetadataClass MXFDMS1ShotClass;
GType mxf_dms1_shot_get_type (void);

#define MXF_TYPE_DMS1_KEY_POINT \
  (mxf_dms1_key_point_get_type())
#define MXF_DMS1_KEY_POINT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_KEY_POINT, MXFDMS1KeyPoint))
#define MXF_IS_DMS1_KEY_POINT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_KEY_POINT))
typedef struct _MXFDMS1KeyPoint MXFDMS1KeyPoint;
typedef MXFDescriptiveMetadataClass MXFDMS1KeyPointClass;
GType mxf_dms1_key_point_get_type (void);

#define MXF_TYPE_DMS1_PARTICIPANT \
  (mxf_dms1_participant_get_type())
#define MXF_DMS1_PARTICIPANT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PARTICIPANT, MXFDMS1Participant))
#define MXF_IS_DMS1_PARTICIPANT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PARTICIPANT))
typedef struct _MXFDMS1Participant MXFDMS1Participant;
typedef MXFDescriptiveMetadataClass MXFDMS1ParticipantClass;
GType mxf_dms1_participant_get_type (void);

#define MXF_TYPE_DMS1_CONTACT \
  (mxf_dms1_contact_get_type())
#define MXF_DMS1_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CONTACT, MXFDMS1Contact))
#define MXF_IS_DMS1_CONTACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CONTACT))
typedef struct _MXFDMS1Contact MXFDMS1Contact;
typedef MXFDescriptiveMetadataClass MXFDMS1ContactClass;
GType mxf_dms1_contact_get_type (void);

#define MXF_TYPE_DMS1_PERSON \
  (mxf_dms1_person_get_type())
#define MXF_DMS1_PERSON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PERSON, MXFDMS1Person))
#define MXF_IS_DMS1_PERSON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PERSON))
typedef struct _MXFDMS1Person MXFDMS1Person;
typedef MXFDescriptiveMetadataClass MXFDMS1PersonClass;
GType mxf_dms1_person_get_type (void);

#define MXF_TYPE_DMS1_ORGANISATION \
  (mxf_dms1_organisation_get_type())
#define MXF_DMS1_ORGANISATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_ORGANISATION, MXFDMS1Organisation))
#define MXF_IS_DMS1_ORGANISATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_ORGANISATION))
typedef struct _MXFDMS1Organisation MXFDMS1Organisation;
typedef MXFDescriptiveMetadataClass MXFDMS1OrganisationClass;
GType mxf_dms1_organisation_get_type (void);

#define MXF_TYPE_DMS1_LOCATION \
  (mxf_dms1_location_get_type())
#define MXF_DMS1_LOCATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_LOCATION, MXFDMS1Location))
#define MXF_IS_DMS1_LOCATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_LOCATION))
typedef struct _MXFDMS1Location MXFDMS1Location;
typedef MXFDescriptiveMetadataClass MXFDMS1LocationClass;
GType mxf_dms1_location_get_type (void);

#define MXF_TYPE_DMS1_ADDRESS \
  (mxf_dms1_address_get_type())
#define MXF_DMS1_ADDRESS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_ADDRESS, MXFDMS1Address))
#define MXF_IS_DMS1_ADDRESS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_ADDRESS))
typedef struct _MXFDMS1Address MXFDMS1Address;
typedef MXFDescriptiveMetadataClass MXFDMS1AddressClass;
GType mxf_dms1_address_get_type (void);

#define MXF_TYPE_DMS1_COMMUNICATIONS \
  (mxf_dms1_communications_get_type())
#define MXF_DMS1_COMMUNICATIONS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_COMMUNICATIONS, MXFDMS1Communications))
#define MXF_IS_DMS1_COMMUNICATIONS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_COMMUNICATIONS))
typedef struct _MXFDMS1Communications MXFDMS1Communications;
typedef MXFDescriptiveMetadataClass MXFDMS1CommunicationsClass;
GType mxf_dms1_communications_get_type (void);

#define MXF_TYPE_DMS1_CONTRACT \
  (mxf_dms1_contract_get_type())
#define MXF_DMS1_CONTRACT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CONTRACT, MXFDMS1Contract))
#define MXF_IS_DMS1_CONTRACT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CONTRACT))
typedef struct _MXFDMS1Contract MXFDMS1Contract;
typedef MXFDescriptiveMetadataClass MXFDMS1ContractClass;
GType mxf_dms1_contract_get_type (void);

#define MXF_TYPE_DMS1_RIGHTS \
  (mxf_dms1_rights_get_type())
#define MXF_DMS1_RIGHTS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_RIGHTS, MXFDMS1Rights))
#define MXF_IS_DMS1_RIGHTS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_RIGHTS))
typedef struct _MXFDMS1Rights MXFDMS1Rights;
typedef MXFDescriptiveMetadataClass MXFDMS1RightsClass;
GType mxf_dms1_rights_get_type (void);

#define MXF_TYPE_DMS1_PICTURE_FORMAT \
  (mxf_dms1_picture_format_get_type())
#define MXF_DMS1_PICTURE_FORMAT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PICTURE_FORMAT, MXFDMS1PictureFormat))
#define MXF_IS_DMS1_PICTURE_FORMAT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PICTURE_FORMAT))
typedef struct _MXFDMS1PictureFormat MXFDMS1PictureFormat;
typedef MXFDescriptiveMetadataClass MXFDMS1PictureFormatClass;
GType mxf_dms1_picture_format_get_type (void);

#define MXF_TYPE_DMS1_DEVICE_PARAMETERS \
  (mxf_dms1_device_parameters_get_type())
#define MXF_DMS1_DEVICE_PARAMETERS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_DEVICE_PARAMETERS, MXFDMS1DeviceParameters))
#define MXF_IS_DMS1_DEVICE_PARAMETERS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_DEVICE_PARAMETERS))
typedef struct _MXFDMS1DeviceParameters MXFDMS1DeviceParameters;
typedef MXFDescriptiveMetadataClass MXFDMS1DeviceParametersClass;
GType mxf_dms1_device_parameters_get_type (void);

#define MXF_TYPE_DMS1_NAME_VALUE \
  (mxf_dms1_name_value_get_type())
#define MXF_DMS1_NAME_VALUE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_NAME_VALUE, MXFDMS1NameValue))
#define MXF_IS_DMS1_NAME_VALUE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_NAME_VALUE))
typedef struct _MXFDMS1NameValue MXFDMS1NameValue;
typedef MXFDescriptiveMetadataClass MXFDMS1NameValueClass;
GType mxf_dms1_name_value_get_type (void);

#define MXF_TYPE_DMS1_PROCESSING \
  (mxf_dms1_processing_get_type())
#define MXF_DMS1_PROCESSING(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PROCESSING, MXFDMS1Processing))
#define MXF_IS_DMS1_PROCESSING(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PROCESSING))
typedef struct _MXFDMS1Processing MXFDMS1Processing;
typedef MXFDescriptiveMetadataClass MXFDMS1ProcessingClass;
GType mxf_dms1_processing_get_type (void);

#define MXF_TYPE_DMS1_PROJECT \
  (mxf_dms1_project_get_type())
#define MXF_DMS1_PROJECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_PROJECT, MXFDMS1Project))
#define MXF_IS_DMS1_PROJECT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_PROJECT))
typedef struct _MXFDMS1Project MXFDMS1Project;
typedef MXFDescriptiveMetadataClass MXFDMS1ProjectClass;
GType mxf_dms1_project_get_type (void);

#define MXF_TYPE_DMS1_CONTACTS_LIST \
  (mxf_dms1_contacts_list_get_type())
#define MXF_DMS1_CONTACTS_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CONTACTS_LIST, MXFDMS1ContactsList))
#define MXF_IS_DMS1_CONTACTS_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CONTACTS_LIST))
typedef struct _MXFDMS1ContactsList MXFDMS1ContactsList;
typedef MXFDescriptiveMetadataClass MXFDMS1ContactsListClass;
GType mxf_dms1_contacts_list_get_type (void);

#define MXF_TYPE_DMS1_CUE_WORDS \
  (mxf_dms1_cue_words_get_type())
#define MXF_DMS1_CUE_WORDS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),MXF_TYPE_DMS1_CUE_WORDS, MXFDMS1CueWords))
#define MXF_IS_DMS1_CUE_WORDS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),MXF_TYPE_DMS1_CUE_WORDS))
typedef struct _MXFDMS1CueWords MXFDMS1CueWords;
typedef MXFDescriptiveMetadataClass MXFDMS1CueWordsClass;
GType mxf_dms1_cue_words_get_type (void);

struct _MXFDMS1 {
  MXFDescriptiveMetadata parent;
};

struct _MXFDMS1TextLanguage {
  MXFDMS1 parent;

  gchar extended_text_language_code[13];
};

struct _MXFDMS1Thesaurus {
  MXFDMS1TextLanguage parent;

  gchar *thesaurus_name;
};

struct _MXFDMS1Framework {
  MXFDMS1 parent;

  gchar framework_extended_text_language_code[13];
  gchar *framework_thesaurus_name;
  gchar *framework_title;
  gchar primary_extended_spoken_language_code[13];
  gchar secondary_extended_spoken_language_code[13];
  gchar original_extended_spoken_language_code[13];

  guint32 n_metadata_server_locators;
  MXFUUID *metadata_server_locators_uids;
  /* TODO */

  guint32 n_titles_sets;
  MXFUUID *titles_sets_uids;
  MXFDMS1Titles **titles_sets;

  guint32 n_annotation_sets;
  MXFUUID *annotation_sets_uids;
  MXFDMS1Annotation **annotation_sets;

  guint32 n_participant_sets;
  MXFUUID *participant_sets_uids;
  MXFDMS1Participant **participant_sets;

  MXFUUID contacts_list_set_uid;
  MXFDMS1ContactsList *contacts_list_set;

  guint32 n_location_sets;
  MXFUUID *location_sets_uids;
  MXFDMS1Location **location_sets;
};

struct _MXFDMS1ProductionClipFramework {
  MXFDMS1Framework parent;

  MXFUUID picture_format_set_uid;
  MXFDMS1PictureFormat *picture_format;
  
  guint32 n_captions_description_sets;
  MXFUUID *captions_description_sets_uids;
  MXFDMS1CaptionsDescription **captions_description_sets;

  guint32 n_contract_sets;
  MXFUUID *contract_sets_uids;
  MXFDMS1Contract **contract_sets;

  MXFUUID project_set_uid;
  MXFDMS1Project *project_set;
};

struct _MXFDMS1ProductionFramework {
  MXFDMS1ProductionClipFramework parent;

  gchar *integration_indication;
  
  guint32 n_identification_sets;
  MXFUUID *identification_sets_uids;
  MXFDMS1Identification **identification_sets;

  guint32 n_group_relationship_sets;
  MXFUUID *group_relationship_sets_uids;
  MXFDMS1GroupRelationship **group_relationship_sets;

  guint32 n_branding_sets;
  MXFUUID *branding_sets_uids;
  MXFDMS1Branding **branding_sets;

  guint32 n_event_sets;
  MXFUUID *event_sets_uids;
  MXFDMS1Event **event_sets;

  guint32 n_award_sets;
  MXFUUID *award_sets_uids;
  MXFDMS1Award **award_sets;

  guint32 n_setting_period_sets;
  MXFUUID *setting_period_sets_uids;
  MXFDMS1SettingPeriod **setting_period_sets;
};

struct _MXFDMS1ClipFramework {
  MXFDMS1ProductionClipFramework parent;

  gchar *clip_kind;
  gchar clip_number[33];
  
  guint8 extended_clip_id[64];
  gboolean extended_clip_id_full;

  MXFTimestamp clip_creation_date_and_time;
  
  guint16 take_number;

  gchar *slate_information;

  guint32 n_scripting_sets;
  MXFUUID *scripting_sets_uids;
  MXFDMS1Scripting **scripting_sets;

  guint32 n_shot_sets;
  MXFUUID *shot_sets_uids;
  MXFDMS1Shot **shot_sets;

  guint32 n_device_parameters_sets;
  MXFUUID *device_parameters_sets_uids;
  MXFDMS1DeviceParameters **device_parameters_sets;

  MXFUUID processing_set_uid;
  MXFDMS1Processing *processing_set;
};

struct _MXFDMS1SceneFramework {
  MXFDMS1Framework parent;

  gchar scene_number[33];

  guint32 n_setting_period_sets;
  MXFUUID *setting_period_sets_uids;
  MXFDMS1SettingPeriod **setting_period_sets;

  guint32 n_shot_scene_sets;
  MXFUUID *shot_scene_sets_uids;
  MXFDMS1Shot **shot_scene_sets;
};

struct _MXFDMS1Titles {
  MXFDMS1TextLanguage parent;

  gchar *main_title;
  gchar *secondary_title;
  gchar *working_title;
  gchar *original_title;
  gchar *version_title;
};

struct _MXFDMS1Identification {
  MXFDMS1Thesaurus parent;

  gchar identifier_kind[33];
  guint8 *identifier_value;
  guint16 identifier_value_length;
  MXFUUID identification_locator;
  gchar *identification_issuing_authority;
};

struct _MXFDMS1GroupRelationship {
  MXFDMS1Thesaurus parent;

  gchar *programming_group_kind;
  gchar* programming_group_title;
  gchar *group_synopsis;
  guint32 numerical_position_in_sequence;
  guint32 total_number_in_the_sequence;
  guint16 episodic_start_number;
  guint16 episodic_end_number;
};

struct _MXFDMS1Branding {
  MXFDMS1TextLanguage parent;

  gchar *brand_main_title;
  gchar *brand_original_title;
};

struct _MXFDMS1Event {
  MXFDMS1Thesaurus parent;

  gchar *event_indication;
  gchar event_start_date_and_time[33];
  gchar event_end_date_and_time[33];
  
  guint32 n_publication_sets;
  MXFUUID *publication_sets_uids;
  MXFDMS1Publication **publication_sets;

  guint32 n_annotation_sets;
  MXFUUID *annotation_sets_uids;
  MXFDMS1Annotation **annotation_sets;
};

struct _MXFDMS1Publication {
  MXFDMS1 parent;

  gchar *publication_organisation_name;
  gchar *publication_service_name;
  gchar *publication_medium;
  gchar *publication_region;
};

struct _MXFDMS1Award {
  MXFDMS1Thesaurus parent;

  gchar *festival;
  gchar festival_date_and_time[33];
  gchar *award_name;
  gchar *award_classification;
  gchar *nomination_category;

  guint32 n_participant_sets;
  MXFUUID *participant_sets_uids;
  MXFDMS1Participant **participant_sets;
};

struct _MXFDMS1CaptionsDescription {
  MXFDMS1Thesaurus parent;

  gchar extended_captions_language_code[13];
  gchar *caption_kind;
};

struct _MXFDMS1Annotation {
  MXFDMS1Thesaurus parent;

  gchar *annotation_kind;
  gchar *annotation_synopsis;
  gchar *annotation_description;
  gchar *related_material_description;

  guint32 n_classification_sets;
  MXFUUID *classification_sets_uids;
  MXFDMS1Classification **classification_sets;

  MXFUUID cue_words_set_uid;
  MXFDMS1CueWords *cue_words_set;

  guint32 n_related_material_locators;
  MXFUUID *related_material_locators;
  /* TODO */
  
  guint32 n_participant_sets;
  MXFUUID *participant_sets_uids;
  MXFDMS1Participant **participant_sets;
};

struct _MXFDMS1SettingPeriod {
  MXFDMS1Thesaurus parent;

  MXFTimestamp setting_date_and_time;
  gchar *time_period_keyword;
  gchar *setting_period_description;
};

struct _MXFDMS1Scripting {
  MXFDMS1Thesaurus parent;

  gchar *scripting_kind;
  gchar *scripting_text;

  guint32 n_scripting_locators;
  MXFUUID *scripting_locators;
  /* TODO */
};

struct _MXFDMS1Classification {
  MXFDMS1Thesaurus parent;

  gchar content_classification[128];

  guint32 n_name_value_sets;
  MXFUUID *name_value_sets_uids;
  MXFDMS1NameValue **name_value_sets;
};

struct _MXFDMS1Shot {
  MXFDMS1TextLanguage parent;

  gint64 shot_start_position;
  gint64 shot_duration;

  guint32 n_shot_track_ids;
  guint32 *shot_track_ids;
  
  gchar *shot_description;
  gchar *shot_comment_kind;
  gchar *shot_comment;

  MXFUUID cue_words_set_uid;
  MXFDMS1CueWords *cue_words_set;

  guint32 n_key_point_sets;
  MXFUUID *key_point_sets_uids;
  MXFDMS1KeyPoint **key_point_sets;
};

struct _MXFDMS1KeyPoint {
  MXFDMS1Thesaurus parent;

  gchar *keypoint_kind;
  gchar *keypoint_value;
  gint64 keypoint_position;
};

struct _MXFDMS1Participant {
  MXFDMS1Thesaurus parent;

  MXFUUID participant_uid;

  gchar *contribution_status;
  gchar *job_function;
  gchar job_function_code[33];
  gchar *role_or_identity_name;

  guint32 n_person_sets;
  MXFUUID *person_sets_uids;
  MXFDMS1Person **person_sets;

  guint32 n_organisation_sets;
  MXFUUID *organisation_sets_uids;
  MXFDMS1Organisation **organisation_sets;
};

struct _MXFDMS1Contact {
  MXFDMS1Thesaurus parent;

  MXFUUID contact_uid;

  guint32 n_name_value_sets;
  MXFUUID *name_value_sets_uids;
  MXFDMS1NameValue **name_value_sets;

  guint32 n_address_sets;
  MXFUUID *address_sets_uids;
  MXFDMS1Address **address_sets;
};

struct _MXFDMS1Person {
  MXFDMS1Contact parent;

  gchar *family_name;
  gchar *first_given_name;
  gchar *other_given_names;
  gchar *linking_name;
  gchar *salutation;
  gchar *name_suffix;
  gchar *honours_qualifications;
  gchar *former_family_name;
  gchar *person_description;
  gchar *alternate_name;
  gchar *nationality;
  gchar *citizenship;

  guint32 n_organisation_sets;
  MXFUUID *organisation_sets_uids;
  MXFDMS1Organisation **organisation_sets;
};

struct _MXFDMS1Organisation {
  MXFDMS1Contact parent;

  gchar *nature_of_organisation;
  gchar *organisation_main_name;
  gchar *organisation_code;
  gchar *contact_department;
};

struct _MXFDMS1Location {
  MXFDMS1Contact parent;

  gchar *location_kind;
  gchar *location_description;
};

struct _MXFDMS1Address {
  MXFDMS1 parent;
  
  gchar *room_or_suite_number;
  gchar *room_or_suite_name;
  gchar *building_name;
  gchar *place_name;
  gchar *street_number;
  gchar *street_name;
  gchar *postal_town;
  gchar *city;
  gchar *state_or_province_or_country;
  gchar *postal_code;
  gchar *country;
  guint8 geographical_coordinate[12];
  gchar *astronomical_body_name;

  guint32 n_communications_sets;
  MXFUUID *communications_sets_uids;
  MXFDMS1Communications **communications_sets;

  guint32 n_name_value_sets;
  MXFUUID *name_value_sets_uids;
  MXFDMS1NameValue **name_value_sets;
};

struct _MXFDMS1Communications {
  MXFDMS1 parent;

  gchar central_telephone_number[33];
  gchar telephone_number[33];
  gchar mobile_telephone_number[33];
  gchar fax_number[33];
  gchar *email_address;
  gchar *web_page;
};

struct _MXFDMS1Contract {
  MXFDMS1Thesaurus parent;

  gchar supply_contract_number[33];

  guint32 n_rights_sets;
  MXFUUID *rights_sets_uids;
  MXFDMS1Rights **rights_sets;

  guint32 n_participant_sets;
  MXFUUID *participant_sets_uids;
  MXFDMS1Participant **participant_sets;
};

struct _MXFDMS1Rights {
  MXFDMS1Thesaurus parent;

  gchar *copyright_owner;
  gchar *rights_holder;
  gchar *rights_managment_authority;
  gchar *region_or_area_of_ip_license;
  gchar *intellectual_property_type;
  gchar *right_condition;
  gchar *right_remarks;
  gchar *intellectual_property_right;
  MXFTimestamp rights_start_date_and_time;
  MXFTimestamp rights_stop_date_and_time;
  guint16 maximum_number_of_usages;
};

struct _MXFDMS1PictureFormat {
  MXFDMS1 parent;

  MXFFraction viewport_aspect_ratio;
  gchar perceived_display_format[33];
  gchar *colour_descriptor;
};

struct _MXFDMS1DeviceParameters {
  MXFDMS1Thesaurus parent;

  gchar *device_type;
  gchar device_designation[33];
  gchar device_asset_number[33];
  guint8 ieee_device_identifier[6];
  gchar *manufacturer;
  gchar *device_model;
  gchar *device_serial_number;
  gchar *device_usage_description;

  guint32 n_name_value_sets;
  MXFUUID *name_value_sets_uids;
  MXFDMS1NameValue **name_value_sets;
};

struct _MXFDMS1NameValue {
  MXFDMS1 parent;

  gchar *item_name;
  gchar *item_value;

  MXFUUID smpte_universal_label_locator;
};

struct _MXFDMS1Processing {
  MXFDMS1 parent;

  gboolean quality_flag;
  gchar *descriptive_comment;
  gboolean logo_flag;
  gchar *graphic_usage_type;
  guint16 process_steps;
  guint16 generation_copy_number;
  guint16 generation_clone_number;
};

struct _MXFDMS1Project {
  MXFDMS1 parent;

  gchar project_number[33];
  gchar *project_name_or_title;
};

struct _MXFDMS1ContactsList {
  MXFDMS1 parent;

  guint32 n_person_sets;
  MXFUUID *person_sets_uids;
  MXFDMS1Person **person_sets;

  guint32 n_organisation_sets;
  MXFUUID *organisation_sets_uids;
  MXFDMS1Organisation **organisation_sets;

  guint32 n_location_sets;
  MXFUUID *location_sets_uids;
  MXFDMS1Location **location_sets;
};

struct _MXFDMS1CueWords {
  MXFDMS1TextLanguage parent;

  gchar *in_cue_words;
  gchar *out_cue_words;
};

void mxf_dms1_initialize (void);

#endif /* _MXF_DMS1_H_ */
