/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "settings.h"
#include "gettext.h"
#include "error.hpp"
#include "errors.hpp"

namespace acommon {


static const ErrorInfo aerror_other_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_other = &aerror_other_obj;

static const ErrorInfo aerror_operation_not_supported_obj = {
  0, // isa
  N_("Operation Not Supported: %what:1"), // mesg
  1, // num_parms
  {"what"} // parms
};
extern "C" const ErrorInfo * const aerror_operation_not_supported = &aerror_operation_not_supported_obj;

static const ErrorInfo aerror_cant_copy_obj = {
  aerror_operation_not_supported, // isa
  0, // mesg
  1, // num_parms
  {"what"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_copy = &aerror_cant_copy_obj;

static const ErrorInfo aerror_unimplemented_method_obj = {
  aerror_operation_not_supported, // isa
  N_("The method \"%what:1\" is unimplemented in \"%where:2\"."), // mesg
  2, // num_parms
  {"what", "where"} // parms
};
extern "C" const ErrorInfo * const aerror_unimplemented_method = &aerror_unimplemented_method_obj;

static const ErrorInfo aerror_file_obj = {
  0, // isa
  N_("%file:1:"), // mesg
  1, // num_parms
  {"file"} // parms
};
extern "C" const ErrorInfo * const aerror_file = &aerror_file_obj;

static const ErrorInfo aerror_cant_open_file_obj = {
  aerror_file, // isa
  N_("The file \"%file:1\" can not be opened"), // mesg
  1, // num_parms
  {"file"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_open_file = &aerror_cant_open_file_obj;

static const ErrorInfo aerror_cant_read_file_obj = {
  aerror_cant_open_file, // isa
  N_("The file \"%file:1\" can not be opened for reading."), // mesg
  1, // num_parms
  {"file"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_read_file = &aerror_cant_read_file_obj;

static const ErrorInfo aerror_cant_write_file_obj = {
  aerror_cant_open_file, // isa
  N_("The file \"%file:1\" can not be opened for writing."), // mesg
  1, // num_parms
  {"file"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_write_file = &aerror_cant_write_file_obj;

static const ErrorInfo aerror_invalid_name_obj = {
  aerror_file, // isa
  N_("The file name \"%file:1\" is invalid."), // mesg
  1, // num_parms
  {"file"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_name = &aerror_invalid_name_obj;

static const ErrorInfo aerror_bad_file_format_obj = {
  aerror_file, // isa
  N_("The file \"%file:1\" is not in the proper format."), // mesg
  1, // num_parms
  {"file"} // parms
};
extern "C" const ErrorInfo * const aerror_bad_file_format = &aerror_bad_file_format_obj;

static const ErrorInfo aerror_dir_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"dir"} // parms
};
extern "C" const ErrorInfo * const aerror_dir = &aerror_dir_obj;

static const ErrorInfo aerror_cant_read_dir_obj = {
  aerror_dir, // isa
  N_("The directory \"%dir:1\" can not be opened for reading."), // mesg
  1, // num_parms
  {"dir"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_read_dir = &aerror_cant_read_dir_obj;

static const ErrorInfo aerror_config_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_config = &aerror_config_obj;

static const ErrorInfo aerror_unknown_key_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" is unknown."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_unknown_key = &aerror_unknown_key_obj;

static const ErrorInfo aerror_cant_change_value_obj = {
  aerror_config, // isa
  N_("The value for option \"%key:1\" can not be changed."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_change_value = &aerror_cant_change_value_obj;

static const ErrorInfo aerror_bad_key_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" is not %accepted:2 and is thus invalid."), // mesg
  2, // num_parms
  {"key", "accepted"} // parms
};
extern "C" const ErrorInfo * const aerror_bad_key = &aerror_bad_key_obj;

static const ErrorInfo aerror_bad_value_obj = {
  aerror_config, // isa
  N_("The value \"%value:2\" is not %accepted:3 and is thus invalid for the key \"%key:1\"."), // mesg
  3, // num_parms
  {"key", "value", "accepted"} // parms
};
extern "C" const ErrorInfo * const aerror_bad_value = &aerror_bad_value_obj;

static const ErrorInfo aerror_duplicate_obj = {
  aerror_config, // isa
  0, // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_duplicate = &aerror_duplicate_obj;

static const ErrorInfo aerror_key_not_string_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" is not a string."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_key_not_string = &aerror_key_not_string_obj;

static const ErrorInfo aerror_key_not_int_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" is not an integer."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_key_not_int = &aerror_key_not_int_obj;

static const ErrorInfo aerror_key_not_bool_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" is not a boolean."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_key_not_bool = &aerror_key_not_bool_obj;

static const ErrorInfo aerror_key_not_list_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" is not a list."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_key_not_list = &aerror_key_not_list_obj;

static const ErrorInfo aerror_no_value_reset_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" does not take any parameters when prefixed by a \"reset-\"."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_no_value_reset = &aerror_no_value_reset_obj;

static const ErrorInfo aerror_no_value_enable_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" does not take any parameters when prefixed by an \"enable-\"."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_no_value_enable = &aerror_no_value_enable_obj;

static const ErrorInfo aerror_no_value_disable_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" does not take any parameters when prefixed by a \"dont-\" or \"disable-\"."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_no_value_disable = &aerror_no_value_disable_obj;

static const ErrorInfo aerror_no_value_clear_obj = {
  aerror_config, // isa
  N_("The key \"%key:1\" does not take any parameters when prefixed by a \"clear-\"."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_no_value_clear = &aerror_no_value_clear_obj;

static const ErrorInfo aerror_language_related_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"lang"} // parms
};
extern "C" const ErrorInfo * const aerror_language_related = &aerror_language_related_obj;

static const ErrorInfo aerror_unknown_language_obj = {
  aerror_language_related, // isa
  N_("The language \"%lang:1\" is not known."), // mesg
  1, // num_parms
  {"lang"} // parms
};
extern "C" const ErrorInfo * const aerror_unknown_language = &aerror_unknown_language_obj;

static const ErrorInfo aerror_unknown_soundslike_obj = {
  aerror_language_related, // isa
  N_("The soundslike \"%sl:2\" is not known."), // mesg
  2, // num_parms
  {"lang", "sl"} // parms
};
extern "C" const ErrorInfo * const aerror_unknown_soundslike = &aerror_unknown_soundslike_obj;

static const ErrorInfo aerror_language_not_supported_obj = {
  aerror_language_related, // isa
  N_("The language \"%lang:1\" is not supported."), // mesg
  1, // num_parms
  {"lang"} // parms
};
extern "C" const ErrorInfo * const aerror_language_not_supported = &aerror_language_not_supported_obj;

static const ErrorInfo aerror_no_wordlist_for_lang_obj = {
  aerror_language_related, // isa
  N_("No word lists can be found for the language \"%lang:1\"."), // mesg
  1, // num_parms
  {"lang"} // parms
};
extern "C" const ErrorInfo * const aerror_no_wordlist_for_lang = &aerror_no_wordlist_for_lang_obj;

static const ErrorInfo aerror_mismatched_language_obj = {
  aerror_language_related, // isa
  N_("Expected language \"%lang:1\" but got \"%prev:2\"."), // mesg
  2, // num_parms
  {"lang", "prev"} // parms
};
extern "C" const ErrorInfo * const aerror_mismatched_language = &aerror_mismatched_language_obj;

static const ErrorInfo aerror_affix_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_affix = &aerror_affix_obj;

static const ErrorInfo aerror_corrupt_affix_obj = {
  aerror_affix, // isa
  N_("Affix '%aff:1' is corrupt."), // mesg
  1, // num_parms
  {"aff"} // parms
};
extern "C" const ErrorInfo * const aerror_corrupt_affix = &aerror_corrupt_affix_obj;

static const ErrorInfo aerror_invalid_cond_obj = {
  aerror_affix, // isa
  N_("The condition \"%cond:1\" is invalid."), // mesg
  1, // num_parms
  {"cond"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_cond = &aerror_invalid_cond_obj;

static const ErrorInfo aerror_invalid_cond_strip_obj = {
  aerror_affix, // isa
  N_("The condition \"%cond:1\" does not guarantee that \"%strip:2\" can always be stripped."), // mesg
  2, // num_parms
  {"cond", "strip"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_cond_strip = &aerror_invalid_cond_strip_obj;

static const ErrorInfo aerror_incorrect_encoding_obj = {
  aerror_affix, // isa
  N_("The file \"%file:1\" is not in the proper format. Expected the file to be in \"%exp:2\" not \"%got:3\"."), // mesg
  3, // num_parms
  {"file", "exp", "got"} // parms
};
extern "C" const ErrorInfo * const aerror_incorrect_encoding = &aerror_incorrect_encoding_obj;

static const ErrorInfo aerror_encoding_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"encod"} // parms
};
extern "C" const ErrorInfo * const aerror_encoding = &aerror_encoding_obj;

static const ErrorInfo aerror_unknown_encoding_obj = {
  aerror_encoding, // isa
  N_("The encoding \"%encod:1\" is not known."), // mesg
  1, // num_parms
  {"encod"} // parms
};
extern "C" const ErrorInfo * const aerror_unknown_encoding = &aerror_unknown_encoding_obj;

static const ErrorInfo aerror_encoding_not_supported_obj = {
  aerror_encoding, // isa
  N_("The encoding \"%encod:1\" is not supported."), // mesg
  1, // num_parms
  {"encod"} // parms
};
extern "C" const ErrorInfo * const aerror_encoding_not_supported = &aerror_encoding_not_supported_obj;

static const ErrorInfo aerror_conversion_not_supported_obj = {
  aerror_encoding, // isa
  N_("The conversion from \"%encod:1\" to \"%encod2:2\" is not supported."), // mesg
  2, // num_parms
  {"encod", "encod2"} // parms
};
extern "C" const ErrorInfo * const aerror_conversion_not_supported = &aerror_conversion_not_supported_obj;

static const ErrorInfo aerror_pipe_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_pipe = &aerror_pipe_obj;

static const ErrorInfo aerror_cant_create_pipe_obj = {
  aerror_pipe, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_cant_create_pipe = &aerror_cant_create_pipe_obj;

static const ErrorInfo aerror_process_died_obj = {
  aerror_pipe, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_process_died = &aerror_process_died_obj;

static const ErrorInfo aerror_bad_input_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_bad_input = &aerror_bad_input_obj;

static const ErrorInfo aerror_invalid_string_obj = {
  aerror_bad_input, // isa
  N_("The string \"%str:1\" is invalid."), // mesg
  1, // num_parms
  {"str"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_string = &aerror_invalid_string_obj;

static const ErrorInfo aerror_invalid_word_obj = {
  aerror_bad_input, // isa
  N_("The word \"%word:1\" is invalid."), // mesg
  1, // num_parms
  {"word"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_word = &aerror_invalid_word_obj;

static const ErrorInfo aerror_invalid_affix_obj = {
  aerror_bad_input, // isa
  N_("The affix flag '%aff:1' is invalid for word \"%word:2\"."), // mesg
  2, // num_parms
  {"aff", "word"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_affix = &aerror_invalid_affix_obj;

static const ErrorInfo aerror_inapplicable_affix_obj = {
  aerror_bad_input, // isa
  N_("The affix flag '%aff:1' can not be applied to word \"%word:2\"."), // mesg
  2, // num_parms
  {"aff", "word"} // parms
};
extern "C" const ErrorInfo * const aerror_inapplicable_affix = &aerror_inapplicable_affix_obj;

static const ErrorInfo aerror_unknown_unichar_obj = {
  aerror_bad_input, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_unknown_unichar = &aerror_unknown_unichar_obj;

static const ErrorInfo aerror_word_list_flags_obj = {
  aerror_bad_input, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_word_list_flags = &aerror_word_list_flags_obj;

static const ErrorInfo aerror_invalid_flag_obj = {
  aerror_word_list_flags, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_flag = &aerror_invalid_flag_obj;

static const ErrorInfo aerror_conflicting_flags_obj = {
  aerror_word_list_flags, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_conflicting_flags = &aerror_conflicting_flags_obj;

static const ErrorInfo aerror_version_control_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_version_control = &aerror_version_control_obj;

static const ErrorInfo aerror_bad_version_string_obj = {
  aerror_version_control, // isa
  N_("not a version number"), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_bad_version_string = &aerror_bad_version_string_obj;

static const ErrorInfo aerror_filter_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_filter = &aerror_filter_obj;

static const ErrorInfo aerror_cant_dlopen_file_obj = {
  aerror_filter, // isa
  N_("dlopen returned \"%return:1\"."), // mesg
  1, // num_parms
  {"return"} // parms
};
extern "C" const ErrorInfo * const aerror_cant_dlopen_file = &aerror_cant_dlopen_file_obj;

static const ErrorInfo aerror_empty_filter_obj = {
  aerror_filter, // isa
  N_("The file \"%filter:1\" does not contain any filters."), // mesg
  1, // num_parms
  {"filter"} // parms
};
extern "C" const ErrorInfo * const aerror_empty_filter = &aerror_empty_filter_obj;

static const ErrorInfo aerror_no_such_filter_obj = {
  aerror_filter, // isa
  N_("The filter \"%filter:1\" does not exist."), // mesg
  1, // num_parms
  {"filter"} // parms
};
extern "C" const ErrorInfo * const aerror_no_such_filter = &aerror_no_such_filter_obj;

static const ErrorInfo aerror_confusing_version_obj = {
  aerror_filter, // isa
  N_("Confused by version control."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_confusing_version = &aerror_confusing_version_obj;

static const ErrorInfo aerror_bad_version_obj = {
  aerror_filter, // isa
  N_("Aspell version does not match filter's requirement."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_bad_version = &aerror_bad_version_obj;

static const ErrorInfo aerror_identical_option_obj = {
  aerror_filter, // isa
  N_("Filter option already exists."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_identical_option = &aerror_identical_option_obj;

static const ErrorInfo aerror_options_only_obj = {
  aerror_filter, // isa
  N_("Use option modifiers only within named option."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_options_only = &aerror_options_only_obj;

static const ErrorInfo aerror_invalid_option_modifier_obj = {
  aerror_filter, // isa
  N_("Option modifier unknown."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_option_modifier = &aerror_invalid_option_modifier_obj;

static const ErrorInfo aerror_cant_describe_filter_obj = {
  aerror_filter, // isa
  N_("Error setting filter description."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_cant_describe_filter = &aerror_cant_describe_filter_obj;

static const ErrorInfo aerror_filter_mode_file_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_filter_mode_file = &aerror_filter_mode_file_obj;

static const ErrorInfo aerror_mode_option_name_obj = {
  aerror_filter_mode_file, // isa
  N_("Empty option specifier."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_mode_option_name = &aerror_mode_option_name_obj;

static const ErrorInfo aerror_no_filter_to_option_obj = {
  aerror_filter_mode_file, // isa
  N_("Option \"%option:1\" possibly specified prior to filter."), // mesg
  1, // num_parms
  {"option"} // parms
};
extern "C" const ErrorInfo * const aerror_no_filter_to_option = &aerror_no_filter_to_option_obj;

static const ErrorInfo aerror_bad_mode_key_obj = {
  aerror_filter_mode_file, // isa
  N_("Unknown mode description key \"%key:1\"."), // mesg
  1, // num_parms
  {"key"} // parms
};
extern "C" const ErrorInfo * const aerror_bad_mode_key = &aerror_bad_mode_key_obj;

static const ErrorInfo aerror_expect_mode_key_obj = {
  aerror_filter_mode_file, // isa
  N_("Expecting \"%modekey:1\" key."), // mesg
  1, // num_parms
  {"modekey"} // parms
};
extern "C" const ErrorInfo * const aerror_expect_mode_key = &aerror_expect_mode_key_obj;

static const ErrorInfo aerror_mode_version_requirement_obj = {
  aerror_filter_mode_file, // isa
  N_("Version specifier missing key: \"aspell\"."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_mode_version_requirement = &aerror_mode_version_requirement_obj;

static const ErrorInfo aerror_confusing_mode_version_obj = {
  aerror_filter_mode_file, // isa
  N_("Confused by version control."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_confusing_mode_version = &aerror_confusing_mode_version_obj;

static const ErrorInfo aerror_bad_mode_version_obj = {
  aerror_filter_mode_file, // isa
  N_("Aspell version does not match mode's requirement."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_bad_mode_version = &aerror_bad_mode_version_obj;

static const ErrorInfo aerror_missing_magic_expression_obj = {
  aerror_filter_mode_file, // isa
  N_("Missing magic mode expression."), // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_missing_magic_expression = &aerror_missing_magic_expression_obj;

static const ErrorInfo aerror_empty_file_ext_obj = {
  aerror_filter_mode_file, // isa
  N_("Empty extension at char %char:1."), // mesg
  1, // num_parms
  {"char"} // parms
};
extern "C" const ErrorInfo * const aerror_empty_file_ext = &aerror_empty_file_ext_obj;

static const ErrorInfo aerror_filter_mode_expand_obj = {
  0, // isa
  N_("\"%mode:1\" error"), // mesg
  1, // num_parms
  {"mode"} // parms
};
extern "C" const ErrorInfo * const aerror_filter_mode_expand = &aerror_filter_mode_expand_obj;

static const ErrorInfo aerror_unknown_mode_obj = {
  aerror_filter_mode_expand, // isa
  N_("Unknown mode: \"%mode:1\"."), // mesg
  1, // num_parms
  {"mode"} // parms
};
extern "C" const ErrorInfo * const aerror_unknown_mode = &aerror_unknown_mode_obj;

static const ErrorInfo aerror_mode_extend_expand_obj = {
  aerror_filter_mode_expand, // isa
  N_("\"%mode:1\" error while extend Aspell modes. (out of memory?)"), // mesg
  1, // num_parms
  {"mode"} // parms
};
extern "C" const ErrorInfo * const aerror_mode_extend_expand = &aerror_mode_extend_expand_obj;

static const ErrorInfo aerror_filter_mode_magic_obj = {
  0, // isa
  0, // mesg
  2, // num_parms
  {"mode", "magic"} // parms
};
extern "C" const ErrorInfo * const aerror_filter_mode_magic = &aerror_filter_mode_magic_obj;

static const ErrorInfo aerror_file_magic_pos_obj = {
  aerror_filter_mode_magic, // isa
  N_("\"%mode:1\": no start for magic search given for magic \"%magic:2\"."), // mesg
  2, // num_parms
  {"mode", "magic"} // parms
};
extern "C" const ErrorInfo * const aerror_file_magic_pos = &aerror_file_magic_pos_obj;

static const ErrorInfo aerror_file_magic_range_obj = {
  aerror_filter_mode_magic, // isa
  N_("\"%mode:1\": no range for magic search given for magic \"%magic:2\"."), // mesg
  2, // num_parms
  {"mode", "magic"} // parms
};
extern "C" const ErrorInfo * const aerror_file_magic_range = &aerror_file_magic_range_obj;

static const ErrorInfo aerror_missing_magic_obj = {
  aerror_filter_mode_magic, // isa
  N_("\"%mode:1\": no magic expression available for magic \"%magic:2\"."), // mesg
  2, // num_parms
  {"mode", "magic"} // parms
};
extern "C" const ErrorInfo * const aerror_missing_magic = &aerror_missing_magic_obj;

static const ErrorInfo aerror_bad_magic_obj = {
  aerror_filter_mode_magic, // isa
  N_("\"%mode:1\": Magic \"%magic:2\": bad regular expression after location specifier; regexp reports: \"%regerr:3\"."), // mesg
  3, // num_parms
  {"mode", "magic", "regerr"} // parms
};
extern "C" const ErrorInfo * const aerror_bad_magic = &aerror_bad_magic_obj;

static const ErrorInfo aerror_expression_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {""} // parms
};
extern "C" const ErrorInfo * const aerror_expression = &aerror_expression_obj;

static const ErrorInfo aerror_invalid_expression_obj = {
  aerror_expression, // isa
  N_("\"%expression:1\" is not a valid regular expression."), // mesg
  1, // num_parms
  {"expression"} // parms
};
extern "C" const ErrorInfo * const aerror_invalid_expression = &aerror_invalid_expression_obj;



}

