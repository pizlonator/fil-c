/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_ASPELL__H
#define ASPELL_ASPELL__H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************* type id *******************************/


union AspellTypeId {

  unsigned int num;

  char str[4];

};


typedef union AspellTypeId AspellTypeId;


/************************** mutable container **************************/


typedef struct AspellMutableContainer AspellMutableContainer;


int aspell_mutable_container_add(struct AspellMutableContainer * ths, const char * to_add);

int aspell_mutable_container_remove(struct AspellMutableContainer * ths, const char * to_rem);

void aspell_mutable_container_clear(struct AspellMutableContainer * ths);

struct AspellMutableContainer * aspell_mutable_container_to_mutable_container(struct AspellMutableContainer * ths);



/******************************* key info *******************************/



enum AspellKeyInfoType {AspellKeyInfoString, AspellKeyInfoInt, AspellKeyInfoBool, AspellKeyInfoList};
typedef enum AspellKeyInfoType AspellKeyInfoType;


struct AspellKeyInfo {

  /* The name of the key. */
  const char * name;

  /* The key type. */
  enum AspellKeyInfoType type;

  /* The default value of the key. */
  const char * def;

  /* A brief description of the key or NULL if internal value. */
  const char * desc;

  int flags;

  int other_data;

};


typedef struct AspellKeyInfo AspellKeyInfo;


/******************************** config ********************************/


typedef struct AspellKeyInfoEnumeration AspellKeyInfoEnumeration;


int aspell_key_info_enumeration_at_end(const struct AspellKeyInfoEnumeration * ths);

const struct AspellKeyInfo * aspell_key_info_enumeration_next(struct AspellKeyInfoEnumeration * ths);

void delete_aspell_key_info_enumeration(struct AspellKeyInfoEnumeration * ths);

struct AspellKeyInfoEnumeration * aspell_key_info_enumeration_clone(const struct AspellKeyInfoEnumeration * ths);

void aspell_key_info_enumeration_assign(struct AspellKeyInfoEnumeration * ths, const struct AspellKeyInfoEnumeration * other);



typedef struct AspellConfig AspellConfig;


struct AspellConfig * new_aspell_config();

void delete_aspell_config(struct AspellConfig * ths);

struct AspellConfig * aspell_config_clone(const struct AspellConfig * ths);

void aspell_config_assign(struct AspellConfig * ths, const struct AspellConfig * other);

unsigned int aspell_config_error_number(const struct AspellConfig * ths);

const char * aspell_config_error_message(const struct AspellConfig * ths);

const struct AspellError * aspell_config_error(const struct AspellConfig * ths);

/* Sets extra keys which this config class should
 * accept. begin and end are expected to point to
 * the beginning and ending of an array of Aspell
 * Key Info. */
void aspell_config_set_extra(struct AspellConfig * ths, const struct AspellKeyInfo * begin, const struct AspellKeyInfo * end);

/* Returns the KeyInfo object for the
 * corresponding key or returns NULL and sets
 * error_num to PERROR_UNKNOWN_KEY if the key is
 * not valid. The pointer returned is valid for
 * the lifetime of the object. */
const struct AspellKeyInfo * aspell_config_keyinfo(struct AspellConfig * ths, const char * key);

/* Returns a newly allocated enumeration of all
 * the possible objects this config class uses. */
struct AspellKeyInfoEnumeration * aspell_config_possible_elements(struct AspellConfig * ths, int include_extra);

/* Returns the default value for given key which
 * may involve substituting variables, thus it is
 * not the same as keyinfo(key)->def returns NULL
 * and sets error_num to PERROR_UNKNOWN_KEY if
 * the key is not valid. Uses the temporary
 * string. */
const char * aspell_config_get_default(struct AspellConfig * ths, const char * key);

/* Returns a newly allocated enumeration of all
 * the key/value pairs. This DOES not include ones
 * which are set to their default values. */
struct AspellStringPairEnumeration * aspell_config_elements(struct AspellConfig * ths);

/* Inserts an item, if the item already exists it
 * will be replaced. Returns TRUE if it succeeded
 * or FALSE on error. If the key is not valid it
 * sets error_num to PERROR_UNKNOWN_KEY, if the
 * value is not valid it will set error_num to
 * PERROR_BAD_VALUE, if the value can not be
 * changed it sets error_num to
 * PERROR_CANT_CHANGE_VALUE, and if the value is
 * a list and you are trying to set its directory,
 * it sets error_num to PERROR_LIST_SET */
int aspell_config_replace(struct AspellConfig * ths, const char * key, const char * value);

/* Remove a key and returns TRUE if it exists
 * otherwise return FALSE. This effectively sets
 * the key to its default value. Calling replace
 * with a value of "<default>" will also call
 * remove. If the key does not exist then it sets
 * error_num to 0 or PERROR_NOT, if the key is
 * not valid then it sets error_num to
 * PERROR_UNKNOWN_KEY, if the value can not be
 * changed then it sets error_num to
 * PERROR_CANT_CHANGE_VALUE */
int aspell_config_remove(struct AspellConfig * ths, const char * key);

int aspell_config_have(const struct AspellConfig * ths, const char * key);

/* Returns NULL on error. */
const char * aspell_config_retrieve(struct AspellConfig * ths, const char * key);

int aspell_config_retrieve_list(struct AspellConfig * ths, const char * key, struct AspellMutableContainer * lst);

/* In "ths" Aspell configuration, search for a
 * character string matching "key" string.
 * If "key" is found then return 1 else return 0.
 * If error encountered, then return -1. */
int aspell_config_retrieve_bool(struct AspellConfig * ths, const char * key);

/* In "ths" Aspell configuration, search for an
 * integer value matching "key" string.
 * Return -1 on error. */
int aspell_config_retrieve_int(struct AspellConfig * ths, const char * key);



/******************************* version *******************************/


/* Returns a version string, which may include additional
 * information on how Aspell was compiled. */
const char * aspell_version_string();

/******************************** error ********************************/


struct AspellError {

  const char * mesg;

  const struct AspellErrorInfo * err;

};


typedef struct AspellError AspellError;

int aspell_error_is_a(const struct AspellError * ths, const struct AspellErrorInfo * e);


struct AspellErrorInfo {

  const struct AspellErrorInfo * isa;

  const char * mesg;

  unsigned int num_parms;

  const char * parms[3];

};


typedef struct AspellErrorInfo AspellErrorInfo;


/**************************** can have error ****************************/


typedef struct AspellCanHaveError AspellCanHaveError;


unsigned int aspell_error_number(const struct AspellCanHaveError * ths);

const char * aspell_error_message(const struct AspellCanHaveError * ths);

const struct AspellError * aspell_error(const struct AspellCanHaveError * ths);

void delete_aspell_can_have_error(struct AspellCanHaveError * ths);



/******************************** errors ********************************/


extern const struct AspellErrorInfo * const aerror_other;
extern const struct AspellErrorInfo * const aerror_operation_not_supported;
extern const struct AspellErrorInfo * const   aerror_cant_copy;
extern const struct AspellErrorInfo * const   aerror_unimplemented_method;
extern const struct AspellErrorInfo * const aerror_file;
extern const struct AspellErrorInfo * const   aerror_cant_open_file;
extern const struct AspellErrorInfo * const     aerror_cant_read_file;
extern const struct AspellErrorInfo * const     aerror_cant_write_file;
extern const struct AspellErrorInfo * const   aerror_invalid_name;
extern const struct AspellErrorInfo * const   aerror_bad_file_format;
extern const struct AspellErrorInfo * const aerror_dir;
extern const struct AspellErrorInfo * const   aerror_cant_read_dir;
extern const struct AspellErrorInfo * const aerror_config;
extern const struct AspellErrorInfo * const   aerror_unknown_key;
extern const struct AspellErrorInfo * const   aerror_cant_change_value;
extern const struct AspellErrorInfo * const   aerror_bad_key;
extern const struct AspellErrorInfo * const   aerror_bad_value;
extern const struct AspellErrorInfo * const   aerror_duplicate;
extern const struct AspellErrorInfo * const   aerror_key_not_string;
extern const struct AspellErrorInfo * const   aerror_key_not_int;
extern const struct AspellErrorInfo * const   aerror_key_not_bool;
extern const struct AspellErrorInfo * const   aerror_key_not_list;
extern const struct AspellErrorInfo * const   aerror_no_value_reset;
extern const struct AspellErrorInfo * const   aerror_no_value_enable;
extern const struct AspellErrorInfo * const   aerror_no_value_disable;
extern const struct AspellErrorInfo * const   aerror_no_value_clear;
extern const struct AspellErrorInfo * const aerror_language_related;
extern const struct AspellErrorInfo * const   aerror_unknown_language;
extern const struct AspellErrorInfo * const   aerror_unknown_soundslike;
extern const struct AspellErrorInfo * const   aerror_language_not_supported;
extern const struct AspellErrorInfo * const   aerror_no_wordlist_for_lang;
extern const struct AspellErrorInfo * const   aerror_mismatched_language;
extern const struct AspellErrorInfo * const aerror_affix;
extern const struct AspellErrorInfo * const   aerror_corrupt_affix;
extern const struct AspellErrorInfo * const   aerror_invalid_cond;
extern const struct AspellErrorInfo * const   aerror_invalid_cond_strip;
extern const struct AspellErrorInfo * const   aerror_incorrect_encoding;
extern const struct AspellErrorInfo * const aerror_encoding;
extern const struct AspellErrorInfo * const   aerror_unknown_encoding;
extern const struct AspellErrorInfo * const   aerror_encoding_not_supported;
extern const struct AspellErrorInfo * const   aerror_conversion_not_supported;
extern const struct AspellErrorInfo * const aerror_pipe;
extern const struct AspellErrorInfo * const   aerror_cant_create_pipe;
extern const struct AspellErrorInfo * const   aerror_process_died;
extern const struct AspellErrorInfo * const aerror_bad_input;
extern const struct AspellErrorInfo * const   aerror_invalid_string;
extern const struct AspellErrorInfo * const   aerror_invalid_word;
extern const struct AspellErrorInfo * const   aerror_invalid_affix;
extern const struct AspellErrorInfo * const   aerror_inapplicable_affix;
extern const struct AspellErrorInfo * const   aerror_unknown_unichar;
extern const struct AspellErrorInfo * const   aerror_word_list_flags;
extern const struct AspellErrorInfo * const     aerror_invalid_flag;
extern const struct AspellErrorInfo * const     aerror_conflicting_flags;
extern const struct AspellErrorInfo * const aerror_version_control;
extern const struct AspellErrorInfo * const   aerror_bad_version_string;
extern const struct AspellErrorInfo * const aerror_filter;
extern const struct AspellErrorInfo * const   aerror_cant_dlopen_file;
extern const struct AspellErrorInfo * const   aerror_empty_filter;
extern const struct AspellErrorInfo * const   aerror_no_such_filter;
extern const struct AspellErrorInfo * const   aerror_confusing_version;
extern const struct AspellErrorInfo * const   aerror_bad_version;
extern const struct AspellErrorInfo * const   aerror_identical_option;
extern const struct AspellErrorInfo * const   aerror_options_only;
extern const struct AspellErrorInfo * const   aerror_invalid_option_modifier;
extern const struct AspellErrorInfo * const   aerror_cant_describe_filter;
extern const struct AspellErrorInfo * const aerror_filter_mode_file;
extern const struct AspellErrorInfo * const   aerror_mode_option_name;
extern const struct AspellErrorInfo * const   aerror_no_filter_to_option;
extern const struct AspellErrorInfo * const   aerror_bad_mode_key;
extern const struct AspellErrorInfo * const   aerror_expect_mode_key;
extern const struct AspellErrorInfo * const   aerror_mode_version_requirement;
extern const struct AspellErrorInfo * const   aerror_confusing_mode_version;
extern const struct AspellErrorInfo * const   aerror_bad_mode_version;
extern const struct AspellErrorInfo * const   aerror_missing_magic_expression;
extern const struct AspellErrorInfo * const   aerror_empty_file_ext;
extern const struct AspellErrorInfo * const aerror_filter_mode_expand;
extern const struct AspellErrorInfo * const   aerror_unknown_mode;
extern const struct AspellErrorInfo * const   aerror_mode_extend_expand;
extern const struct AspellErrorInfo * const aerror_filter_mode_magic;
extern const struct AspellErrorInfo * const   aerror_file_magic_pos;
extern const struct AspellErrorInfo * const   aerror_file_magic_range;
extern const struct AspellErrorInfo * const   aerror_missing_magic;
extern const struct AspellErrorInfo * const   aerror_bad_magic;
extern const struct AspellErrorInfo * const aerror_expression;
extern const struct AspellErrorInfo * const   aerror_invalid_expression;


/******************************* speller *******************************/


typedef struct AspellSpeller AspellSpeller;


struct AspellCanHaveError * new_aspell_speller(struct AspellConfig * config);

struct AspellSpeller * to_aspell_speller(struct AspellCanHaveError * obj);

void delete_aspell_speller(struct AspellSpeller * ths);

unsigned int aspell_speller_error_number(const struct AspellSpeller * ths);

const char * aspell_speller_error_message(const struct AspellSpeller * ths);

const struct AspellError * aspell_speller_error(const struct AspellSpeller * ths);

struct AspellConfig * aspell_speller_config(struct AspellSpeller * ths);

/* Returns 0 if it is not in the dictionary,
 * 1 if it is, or -1 on error. */
int aspell_speller_check(struct AspellSpeller * ths, const char * word, int word_size);
int aspell_speller_check_wide(struct AspellSpeller * ths, const void * word, int word_size, int word_type_width);

/* version of aspell_speller_check that is safe to use with (null terminated) wide characters */
#define aspell_speller_check_w(ths, word, word_size)\
    aspell_speller_check_wide(ths, aspell_cast_from_wide_(word), word_size*aspell_cast_(int,sizeof(*(word))), sizeof(*(word)))

/* Add this word to your own personal word list. */
int aspell_speller_add_to_personal(struct AspellSpeller * ths, const char * word, int word_size);
int aspell_speller_add_to_personal_wide(struct AspellSpeller * ths, const void * word, int word_size, int word_type_width);

/* version of aspell_speller_add_to_personal that is safe to use with (null terminated) wide characters */
#define aspell_speller_add_to_personal_w(ths, word, word_size)\
    aspell_speller_add_to_personal_wide(ths, aspell_cast_from_wide_(word), word_size*aspell_cast_(int,sizeof(*(word))), sizeof(*(word)))

/* Add this word to the current spelling session. */
int aspell_speller_add_to_session(struct AspellSpeller * ths, const char * word, int word_size);
int aspell_speller_add_to_session_wide(struct AspellSpeller * ths, const void * word, int word_size, int word_type_width);

/* version of aspell_speller_add_to_session that is safe to use with (null terminated) wide characters */
#define aspell_speller_add_to_session_w(ths, word, word_size)\
    aspell_speller_add_to_session_wide(ths, aspell_cast_from_wide_(word), word_size*aspell_cast_(int,sizeof(*(word))), sizeof(*(word)))

/* This is your own personal word list file plus
 * any extra words added during this session to
 * your own personal word list. */
const struct AspellWordList * aspell_speller_personal_word_list(struct AspellSpeller * ths);

/* This is a list of words added to this session
 * that are not in the main word list or in your
 * own personal list but are considered valid for
 * this spelling session. */
const struct AspellWordList * aspell_speller_session_word_list(struct AspellSpeller * ths);

/* This is the main list of words used during this
 * spelling session. */
const struct AspellWordList * aspell_speller_main_word_list(struct AspellSpeller * ths);

int aspell_speller_save_all_word_lists(struct AspellSpeller * ths);

int aspell_speller_clear_session(struct AspellSpeller * ths);

/* Return NULL on error.
 * The word list returned by suggest is only
 * valid until the next call to suggest. */
const struct AspellWordList * aspell_speller_suggest(struct AspellSpeller * ths, const char * word, int word_size);
const struct AspellWordList * aspell_speller_suggest_wide(struct AspellSpeller * ths, const void * word, int word_size, int word_type_width);

/* version of aspell_speller_suggest that is safe to use with (null terminated) wide characters */
#define aspell_speller_suggest_w(ths, word, word_size)\
    aspell_speller_suggest_wide(ths, aspell_cast_from_wide_(word), word_size*aspell_cast_(int,sizeof(*(word))), sizeof(*(word)))

int aspell_speller_store_replacement(struct AspellSpeller * ths, const char * mis, int mis_size, const char * cor, int cor_size);
int aspell_speller_store_replacement_wide(struct AspellSpeller * ths, const void * mis, int mis_size, int mis_type_width, const void * cor, int cor_size, int cor_type_width);

/* version of aspell_speller_store_replacement that is safe to use with (null terminated) wide characters */
#define aspell_speller_store_replacement_w(ths, mis, mis_size, cor, cor_size)\
    aspell_speller_store_replacement_wide(ths, aspell_cast_from_wide_(mis), mis_size*aspell_cast_(int,sizeof(*(mis))), sizeof(*(mis)), aspell_cast_from_wide_(cor), cor_size*aspell_cast_(int,sizeof(*(cor))), sizeof(*(cor)))



/******************************** filter ********************************/


typedef struct AspellFilter AspellFilter;


void delete_aspell_filter(struct AspellFilter * ths);

unsigned int aspell_filter_error_number(const struct AspellFilter * ths);

const char * aspell_filter_error_message(const struct AspellFilter * ths);

const struct AspellError * aspell_filter_error(const struct AspellFilter * ths);

struct AspellFilter * to_aspell_filter(struct AspellCanHaveError * obj);



/*************************** document checker ***************************/


struct AspellToken {

  unsigned int offset;

  unsigned int len;

};


typedef struct AspellToken AspellToken;


typedef struct AspellDocumentChecker AspellDocumentChecker;


void delete_aspell_document_checker(struct AspellDocumentChecker * ths);

unsigned int aspell_document_checker_error_number(const struct AspellDocumentChecker * ths);

const char * aspell_document_checker_error_message(const struct AspellDocumentChecker * ths);

const struct AspellError * aspell_document_checker_error(const struct AspellDocumentChecker * ths);

/* Creates a new document checker.
 * The speller class is expected to last until
 * this class is destroyed.
 * If config is given it will be used to override
 * any relevant options set by this speller class.
 * The config class is not once this function is done.
 * If filter is given then it will take ownership of
 * the filter class and use it to do the filtering.
 * You are expected to free the checker when done. */
struct AspellCanHaveError * new_aspell_document_checker(struct AspellSpeller * speller);

struct AspellDocumentChecker * to_aspell_document_checker(struct AspellCanHaveError * obj);

/* Reset the internal state of the filter.
 * Should be called whenever a new document is
 * being filtered. */
void aspell_document_checker_reset(struct AspellDocumentChecker * ths);

/* Process a string.
 * The document is expected to be passed in one or more
 * lines at a time.  Splitting the document on
 * white space characters instead of new lines is
 * permissible but some filters which are line based
 * may give incorrect results.  Furthermore, between
 * calls to reset, each string should be passed
 * in exactly once and in the order they appeared
 * in the document.  Passing in strings out of
 * order, skipping strings or passing them in
 * more than once may lead to undefined results. */
void aspell_document_checker_process(struct AspellDocumentChecker * ths, const char * str, int str_size);
void aspell_document_checker_process_wide(struct AspellDocumentChecker * ths, const void * str, int str_size, int str_type_width);

/* version of aspell_document_checker_process that is safe to use with (null terminated) wide characters */
#define aspell_document_checker_process_w(ths, str, str_size)\
    aspell_document_checker_process_wide(ths, aspell_cast_from_wide_(str), str_size*aspell_cast_(int,sizeof(*(str))), sizeof(*(str)))

/* Returns the next misspelled word in the
 * processed string.  If there are no more
 * misspelled words, then token.word will be
 * NULL and token.size will be 0 */
struct AspellToken aspell_document_checker_next_misspelling(struct AspellDocumentChecker * ths);

#define aspell_document_checker_next_misspelling_w(type, ths) \
    aspell_document_checker_next_misspelling_adj(ths, sizeof(type))

/* internal: do not use */
struct AspellToken aspell_document_checker_next_misspelling_adj(struct AspellDocumentChecker * ths, int type_width);

/* Returns the underlying filter class. */
struct AspellFilter * aspell_document_checker_filter(struct AspellDocumentChecker * ths);



/****************************** word list ******************************/


typedef struct AspellWordList AspellWordList;


int aspell_word_list_empty(const struct AspellWordList * ths);

unsigned int aspell_word_list_size(const struct AspellWordList * ths);

struct AspellStringEnumeration * aspell_word_list_elements(const struct AspellWordList * ths);



/************************** string enumeration **************************/


typedef struct AspellStringEnumeration AspellStringEnumeration;


void delete_aspell_string_enumeration(struct AspellStringEnumeration * ths);

struct AspellStringEnumeration * aspell_string_enumeration_clone(const struct AspellStringEnumeration * ths);

void aspell_string_enumeration_assign(struct AspellStringEnumeration * ths, const struct AspellStringEnumeration * other);

int aspell_string_enumeration_at_end(const struct AspellStringEnumeration * ths);

const char * aspell_string_enumeration_next(struct AspellStringEnumeration * ths);

#define aspell_string_enumeration_next_w(type, ths) \
    aspell_cast_(const type *, aspell_string_enumeration_next_wide(ths, sizeof(type)))

const void * aspell_string_enumeration_next_wide(struct AspellStringEnumeration * ths, int type_width);



/********************************* info *********************************/


struct AspellModuleInfo {

  const char * name;

  double order_num;

  const char * lib_dir;

  struct AspellStringList * dict_dirs;

  struct AspellStringList * dict_exts;

};


typedef struct AspellModuleInfo AspellModuleInfo;


struct AspellDictInfo {

  /* The Name to identify this dictionary by. */
  const char * name;

  /* The language code to identify this dictionary.
   * A two letter UPPER-CASE ISO 639 language code
   * and an optional two letter ISO 3166 country
   * code after a dash or underscore. */
  const char * code;

  /* Any extra information to distinguish this
   * variety of dictionary from other dictionaries
   * which may have the same language and size. */
  const char * jargon;

  int size;

  /* A two char digit code describing the size of
   * the dictionary: 10=tiny, 20=really small,
   * 30=small, 40=med-small, 50=med, 60=med-large,
   * 70=large, 80=huge, 90=insane.  Please check
   * the README in aspell-lang-20??????.tar.bz2 or
   * see SCOWL (http://wordlist.sourceforge.net)
   * for an example of how these sizes are used. */
  const char * size_str;

  struct AspellModuleInfo * module;

};


typedef struct AspellDictInfo AspellDictInfo;


typedef struct AspellModuleInfoList AspellModuleInfoList;


struct AspellModuleInfoList * get_aspell_module_info_list(struct AspellConfig * config);

int aspell_module_info_list_empty(const struct AspellModuleInfoList * ths);

unsigned int aspell_module_info_list_size(const struct AspellModuleInfoList * ths);

struct AspellModuleInfoEnumeration * aspell_module_info_list_elements(const struct AspellModuleInfoList * ths);



typedef struct AspellDictInfoList AspellDictInfoList;


struct AspellDictInfoList * get_aspell_dict_info_list(struct AspellConfig * config);

int aspell_dict_info_list_empty(const struct AspellDictInfoList * ths);

unsigned int aspell_dict_info_list_size(const struct AspellDictInfoList * ths);

struct AspellDictInfoEnumeration * aspell_dict_info_list_elements(const struct AspellDictInfoList * ths);



typedef struct AspellModuleInfoEnumeration AspellModuleInfoEnumeration;


int aspell_module_info_enumeration_at_end(const struct AspellModuleInfoEnumeration * ths);

const struct AspellModuleInfo * aspell_module_info_enumeration_next(struct AspellModuleInfoEnumeration * ths);

void delete_aspell_module_info_enumeration(struct AspellModuleInfoEnumeration * ths);

struct AspellModuleInfoEnumeration * aspell_module_info_enumeration_clone(const struct AspellModuleInfoEnumeration * ths);

void aspell_module_info_enumeration_assign(struct AspellModuleInfoEnumeration * ths, const struct AspellModuleInfoEnumeration * other);



typedef struct AspellDictInfoEnumeration AspellDictInfoEnumeration;


int aspell_dict_info_enumeration_at_end(const struct AspellDictInfoEnumeration * ths);

const struct AspellDictInfo * aspell_dict_info_enumeration_next(struct AspellDictInfoEnumeration * ths);

void delete_aspell_dict_info_enumeration(struct AspellDictInfoEnumeration * ths);

struct AspellDictInfoEnumeration * aspell_dict_info_enumeration_clone(const struct AspellDictInfoEnumeration * ths);

void aspell_dict_info_enumeration_assign(struct AspellDictInfoEnumeration * ths, const struct AspellDictInfoEnumeration * other);



/***************************** string list *****************************/


typedef struct AspellStringList AspellStringList;


struct AspellStringList * new_aspell_string_list();

int aspell_string_list_empty(const struct AspellStringList * ths);

unsigned int aspell_string_list_size(const struct AspellStringList * ths);

struct AspellStringEnumeration * aspell_string_list_elements(const struct AspellStringList * ths);

int aspell_string_list_add(struct AspellStringList * ths, const char * to_add);

int aspell_string_list_remove(struct AspellStringList * ths, const char * to_rem);

void aspell_string_list_clear(struct AspellStringList * ths);

struct AspellMutableContainer * aspell_string_list_to_mutable_container(struct AspellStringList * ths);

void delete_aspell_string_list(struct AspellStringList * ths);

struct AspellStringList * aspell_string_list_clone(const struct AspellStringList * ths);

void aspell_string_list_assign(struct AspellStringList * ths, const struct AspellStringList * other);



/****************************** string map ******************************/


typedef struct AspellStringMap AspellStringMap;


struct AspellStringMap * new_aspell_string_map();

int aspell_string_map_add(struct AspellStringMap * ths, const char * to_add);

int aspell_string_map_remove(struct AspellStringMap * ths, const char * to_rem);

void aspell_string_map_clear(struct AspellStringMap * ths);

struct AspellMutableContainer * aspell_string_map_to_mutable_container(struct AspellStringMap * ths);

void delete_aspell_string_map(struct AspellStringMap * ths);

struct AspellStringMap * aspell_string_map_clone(const struct AspellStringMap * ths);

void aspell_string_map_assign(struct AspellStringMap * ths, const struct AspellStringMap * other);

int aspell_string_map_empty(const struct AspellStringMap * ths);

unsigned int aspell_string_map_size(const struct AspellStringMap * ths);

struct AspellStringPairEnumeration * aspell_string_map_elements(const struct AspellStringMap * ths);

/* Insert a new element.
 * Will NOT overwrite an existing entry.
 * Returns FALSE if the element already exists. */
int aspell_string_map_insert(struct AspellStringMap * ths, const char * key, const char * value);

/* Insert a new element.
 * Will overwrite an existing entry.
 * Always returns TRUE. */
int aspell_string_map_replace(struct AspellStringMap * ths, const char * key, const char * value);

/* Looks up an element and returns the value.
 * Returns NULL if the element does not exist.
 * Returns an empty string if the element exists
 * but has a NULL value. */
const char * aspell_string_map_lookup(const struct AspellStringMap * ths, const char * key);



/***************************** string pair *****************************/


struct AspellStringPair {

  const char * first;

  const char * second;

};


typedef struct AspellStringPair AspellStringPair;


/*********************** string pair enumeration ***********************/


typedef struct AspellStringPairEnumeration AspellStringPairEnumeration;


int aspell_string_pair_enumeration_at_end(const struct AspellStringPairEnumeration * ths);

struct AspellStringPair aspell_string_pair_enumeration_next(struct AspellStringPairEnumeration * ths);

void delete_aspell_string_pair_enumeration(struct AspellStringPairEnumeration * ths);

struct AspellStringPairEnumeration * aspell_string_pair_enumeration_clone(const struct AspellStringPairEnumeration * ths);

void aspell_string_pair_enumeration_assign(struct AspellStringPairEnumeration * ths, const struct AspellStringPairEnumeration * other);



/******************************** cache ********************************/


/* Reset the global cache(s) so that cache queries will
 * create a new object. If existing objects are still in
 * use they are not deleted. If which is NULL then all
 * caches will be reset. Current caches are "encode",
 * "decode", "dictionary", "language", and "keyboard". */
int aspell_reset_cache(const char * which);

/**********************************************************************/

#ifdef ASPELL_ENCODE_SETTING_SECURE
#define aspell_speller_check(ths, word, word_size)\
    aspell_speller_check_wide(ths, word, word_size, -1)
#define aspell_speller_add_to_personal(ths, word, word_size)\
    aspell_speller_add_to_personal_wide(ths, word, word_size, -1)
#define aspell_speller_add_to_session(ths, word, word_size)\
    aspell_speller_add_to_session_wide(ths, word, word_size, -1)
#define aspell_speller_suggest(ths, word, word_size)\
    aspell_speller_suggest_wide(ths, word, word_size, -1)
#define aspell_speller_store_replacement(ths, mis, mis_size, cor, cor_size)\
    aspell_speller_store_replacement_wide(ths, mis, mis_size, -1, cor, cor_size, -1)
#define aspell_document_checker_process(ths, str, str_size)\
    aspell_document_checker_process_wide(ths, str, str_size, -1)
#endif

/******************* private implemantion details *********************/

#ifdef __cplusplus
#  define aspell_cast_(type, expr) (static_cast<type>(expr))
#  define aspell_cast_from_wide_(str) (static_cast<const void *>(str))
#else
#  define aspell_cast_(type, expr) ((type)(expr))
#  define aspell_cast_from_wide_(str) ((const char *)(str))
#endif
#ifdef __cplusplus
}
#endif
#endif /* ASPELL_ASPELL__H */
