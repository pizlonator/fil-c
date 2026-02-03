/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_PSPELL__H
#define ASPELL_PSPELL__H

#include "../aspell.h"

#define PspellCanHaveError   AspellCanHaveError 
#define PspellErrorExtraInfo AspellErrorExtraInfo

#define pspell_error_number     aspell_error_number
#define pspell_error_message    aspell_error_message
#define delete_pspell_can_have_error delete_aspell_can_have_error

#define PspellStringEmulation AspellStringEnumeration

#define pspell_string_emulation_clone   aspell_string_enumeration_clone
#define pspell_string_emulation_assign  aspell_string_enumeration_assign
#define pspell_string_emulation_at_end  aspell_string_enumeration_at_end
#define pspell_string_emulation_next    aspell_string_enumeration_next
#define delete_pspell_string_emulation  delete_aspell_string_enumeration

#define PspellStringPair          AspellStringPair
#define PspellStringPairEmulation AspellStringPairEnumeration

#define pspell_string_pair_emulation_clone  aspell_string_pair_enumeration_clone
#define pspell_string_pair_emulation_assign aspell_string_pair_enumeration_assign
#define pspell_string_pair_emulation_at_end aspell_string_pair_enumeration_at_end
#define delete_pspell_string_pair_emulation delete_aspell_string_pair_enumeration

#define PspellKeyInfoType   AspellKeyInfoType
#define PspellKeyInfoString AspellKeyInfoString
#define PspellKeyInfoInt    AspellKeyInfoInt
#define PspellKeyInfoBool   AspellKeyInfoBool
#define PspellKeyInfoList   AspellKeyInfoList

#define PspellKeyInfo          AspellKeyInfo
#define PspellKeyInfoEmulation AspellKeyInfoEnumeration

#define PspellConfig AspellConfig

#define new_pspell_config new_aspell_config
#define delete_pspell_config delete_aspell_config
#define pspell_config_error_number aspell_config_error_number
#define pspell_config_error_message aspell_config_error_message
#define pspell_config_clone aspell_config_clone
#define pspell_config_assign aspell_config_assign
#define pspell_config_set_extra aspell_config_set_extra
#define pspell_config_keyinfo aspell_config_keyinfo
#define pspell_config_possible_elements aspell_config_possible_elements
#define pspell_config_get_default aspell_config_get_default
#define pspell_config_elements aspell_config_elements
#define pspell_config_replace aspell_config_replace
#define pspell_config_remove aspell_config_remove
#define pspell_config_have aspell_config_have
#define pspell_config_retrieve aspell_config_retrieve
#define pspell_config_retrieve_list aspell_config_retrieve_list
#define pspell_config_retrieve_bool aspell_config_retrieve_bool
#define pspell_config_retrieve_int aspell_config_retrieve_int

#define PspellStringMap AspellStringMap;

#define new_pspell_string_map new_aspell_string_map
#define delete_pspell_string_map delete_aspell_string_map
#define pspell_string_map_clone aspell_string_map_clone
#define pspell_string_map_assign aspell_string_map_assign
#define pspell_string_map_elements aspell_string_map_elements
#define pspell_string_map_insert aspell_string_map_insert
#define pspell_string_map_replace aspell_string_map_replace
#define pspell_string_map_remove aspell_string_map_remove
#define pspell_string_map_clear aspell_string_map_clear
#define pspell_string_map_lookup aspell_string_map_lookup
#define pspell_string_map_size aspell_string_map_size
#define pspell_string_map_empty aspell_string_map_empty

#define PspellWordList AspellWordList
#define pspell_word_list_encoding aspell_word_list_encoding
#define pspell_word_list_empty aspell_word_list_empty
#define pspell_word_list_size aspell_word_list_size
#define pspell_word_list_elements aspell_word_list_elements

#define PspellManager AspellSpeller
#define new_pspell_manager new_aspell_speller
#define to_pspell_manager to_aspell_speller
#define delete_pspell_manager delete_aspell_speller
#define pspell_manager_error_number aspell_speller_error_number
#define pspell_manager_error_message aspell_speller_error_message
#define pspell_manager_config aspell_speller_config
#define pspell_manager_master_word_list aspell_speller_main_word_list
#define pspell_manager_personal_word_list aspell_speller_personal_word_list
#define pspell_manager_session_word_list aspell_speller_session_word_list
#define pspell_manager_save_all_word_lists aspell_speller_save_all_word_lists
#define pspell_manager_clear_session aspell_speller_clear_session

#if defined(USE_ORIGINAL_MANAGER_FUNCS)

#  define pspell_manager_check(m,str)\
    aspell_speller_check(m,str,-1)
#  define pspell_manager_add_to_personal(m,str)\
    aspell_speller_add_to_personal(m,str,-1)
#  define pspell_manager_add_to_session(m,str)\
    aspell_speller_add_to_session(m,str,-1)
#  define pspell_manager_suggest(m,str)\
    aspell_speller_suggest(m,str,-1)
#  define pspell_manager_store_replacement(m,mis,cor)\
    aspell_speller_store_replacement(m,mis,-1,cor,-1)

#else

#  define pspell_manager_check aspell_speller_check
#  define pspell_manager_add_to_personal aspell_speller_add_to_personal
#  define pspell_manager_add_to_session aspell_speller_add_to_session
#  define pspell_manager_suggest aspell_speller_suggest
#  define pspell_manager_store_replacement aspell_speller_store_replacement

#endif

#endif /* ASPELL_PSPELL__H */
