/*
 * Copyright © 2012 Colin Walters <walters@verbum.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GSYSTEM_LOCAL_ALLOC_H__
#define __GSYSTEM_LOCAL_ALLOC_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define gs_transfer_out_value(outp, srcp) G_STMT_START {   \
  if (outp)                                                \
    {                                                      \
      *(outp) = *(srcp);                                   \
      *(srcp) = nullptr;                                      \
    }                                                      \
  } G_STMT_END;

#define GS_DEFINE_CLEANUP_FUNCTION(Type, name, func) \
  static inline void name (void *v) \
  { \
    func (*(Type*)v); \
  }

#define GS_DEFINE_CLEANUP_FUNCTION0(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v); \
  }

/* These functions shouldn't be invoked directly;
 * they are stubs that:
 * 1) Take a pointer to the location (typically itself a pointer).
 * 2) Provide %nullptr-safety where it doesn't exist already (e.g. g_object_unref)
 */
GS_DEFINE_CLEANUP_FUNCTION0(GArray*, gs_local_array_unref, g_array_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GBytes*, gs_local_bytes_unref, g_bytes_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GChecksum*, gs_local_checksum_free, g_checksum_free)
GS_DEFINE_CLEANUP_FUNCTION0(GDateTime*, gs_local_date_time_unref, g_date_time_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GDir*, gs_local_dir_close, g_dir_close)
GS_DEFINE_CLEANUP_FUNCTION0(GError*, gs_local_free_error, g_error_free)
GS_DEFINE_CLEANUP_FUNCTION0(GHashTable*, gs_local_hashtable_unref, g_hash_table_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GKeyFile*, gs_local_key_file_unref, g_key_file_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GList*, gs_local_list_free, g_list_free)
GS_DEFINE_CLEANUP_FUNCTION0(GMatchInfo*, gs_local_match_info_free, g_match_info_free)
GS_DEFINE_CLEANUP_FUNCTION0(GObject*, gs_local_obj_unref, g_object_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GOptionContext*, gs_local_option_context_free, g_option_context_free)
GS_DEFINE_CLEANUP_FUNCTION0(GPtrArray*, gs_local_ptrarray_unref, g_ptr_array_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GRegex*, gs_local_regex_unref, g_regex_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GSettingsSchema*, gs_local_settings_schema_unref, g_settings_schema_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GSettingsSchemaKey*, gs_local_settings_schema_key_unref, g_settings_schema_key_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GVariant*, gs_local_variant_unref, g_variant_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GVariantBuilder*, gs_local_variant_builder_unref, g_variant_builder_unref)
GS_DEFINE_CLEANUP_FUNCTION0(GVariantIter*, gs_local_variant_iter_free, g_variant_iter_free)
GS_DEFINE_CLEANUP_FUNCTION0(GVariantType*, gs_local_variant_type_free, g_variant_type_free)

GS_DEFINE_CLEANUP_FUNCTION(char**, gs_local_strfreev, g_strfreev)
GS_DEFINE_CLEANUP_FUNCTION(void*, gs_local_free, g_free)

/* special */

static inline void gs_local_gstring_free (void *v) \
{                                                  \
  if (*(GString**)v)                               \
    g_string_free (*(GString**)v, TRUE);           \
}

/**
 * gs_free:
 *
 * Call g_free() on a variable location when it goes out of scope.
 */
#define gs_free __attribute__ ((cleanup(gs_local_free)))

/**
 * gs_unref_object:
 *
 * Call g_object_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_object_unref(), the variable may be
 * %nullptr.
 */
#define gs_unref_object __attribute__ ((cleanup(gs_local_obj_unref)))

/**
 * gs_unref_variant:
 *
 * Call g_variant_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_variant_unref(), the variable may be
 * %nullptr.
 */
#define gs_unref_variant __attribute__ ((cleanup(gs_local_variant_unref)))

/**
 * gs_free_variant_iter:
 *
 * Call g_variant_iter_free() on a variable location when it goes out of
 * scope.
 */
#define gs_free_variant_iter __attribute__ ((cleanup(gs_local_variant_iter_free)))

/**
 * gs_free_variant_builder:
 *
 * Call g_variant_builder_unref() on a variable location when it goes out of
 * scope.
 */
#define gs_unref_variant_builder __attribute__ ((cleanup(gs_local_variant_builder_unref)))

/**
 * gs_free_variant_type:
 *
 * Call g_variant_type_free() on a variable location when it goes out of
 * scope.
 */
#define gs_free_variant_type __attribute__ ((cleanup(gs_local_variant_type_free)))

/**
 * gs_unref_array:
 *
 * Call g_array_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_array_unref(), the variable may be
 * %nullptr.

 */
#define gs_unref_array __attribute__ ((cleanup(gs_local_array_unref)))

/**
 * gs_unref_ptrarray:
 *
 * Call g_ptr_array_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_ptr_array_unref(), the variable may be
 * %nullptr.

 */
#define gs_unref_ptrarray __attribute__ ((cleanup(gs_local_ptrarray_unref)))

/**
 * gs_unref_hashtable:
 *
 * Call g_hash_table_unref() on a variable location when it goes out
 * of scope.  Note that unlike g_hash_table_unref(), the variable may
 * be %nullptr.
 */
#define gs_unref_hashtable __attribute__ ((cleanup(gs_local_hashtable_unref)))

/**
 * gs_unref_key_file:
 *
 * Call g_key_file_unref() on a variable location when it goes out
 * of scope.  Note that unlike g_key_file_unref(), the variable may
 * be %nullptr.
 */
#define gs_unref_key_file __attribute__ ((cleanup(gs_local_key_file_unref)))

/**
 * gs_free_checksum:
 *
 * Call g_checksum_free() on a variable location when it goes out
 * of scope.  Note that unlike g_checksum_free(), the variable may
 * be %nullptr.
 */
#define gs_free_checksum __attribute__ ((cleanup(gs_local_checksum_free)))

/**
 * gs_unref_date_time:
 *
 * Call g_date_time_free() on a variable location when it goes out
 * of scope.  Note that unlike g_date_time_free(), the variable may
 * be %nullptr.
 */
#define gs_unref_date_time __attribute__ ((cleanup(gs_local_date_time_unref)))

/**
 * gs_unref_bytes:
 *
 * Call g_bytes_unref() on a variable location when it goes out
 * of scope.  Note that unlike g_bytes_unref(), the variable may
 * be %nullptr.
 */
#define gs_unref_bytes __attribute__ ((cleanup(gs_local_bytes_unref)))

/**
 * gs_strfreev:
 *
 * Call g_strfreev() on a variable location when it goes out of scope.
 */
#define gs_strfreev __attribute__ ((cleanup(gs_local_strfreev)))

/**
 * gs_free_error:
 *
 * Call g_error_free() on a variable location when it goes out of scope.
 */
#define gs_free_error __attribute__ ((cleanup(gs_local_free_error)))

/**
 * gs_free_list:
 *
 * Call g_list_free() on a variable location when it goes out of scope.
 */
#define gs_free_list __attribute__ ((cleanup(gs_local_list_free)))

/**
 * gs_unref_regex:
 *
 * Call g_regex_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_regex_unref(), the variable may be
 * %nullptr.

 */
#define gs_unref_regex __attribute__ ((cleanup(gs_local_regex_unref)))

/**
 * gs_free_match_info:
 *
 * Call g_regex_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_regex_unref(), the variable may be
 * %nullptr.

 */
#define gs_free_match_info __attribute__ ((cleanup(gs_local_match_info_free)))

/**
 * gs_unref_settings_schema:
 *
 * Call g_settings_schema_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_settings_schema_unref(), the variable may be
 * %nullptr.

 */
#define gs_unref_settings_schema __attribute__ ((cleanup(gs_local_settings_schema_unref)))

/**
 * gs_unref_settings_schema_source:
 *
 * Call g_settings_schema_source_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_settings_schema_source_unref(), the variable may be
 * %nullptr.

 */
#define gs_unref_settings_schema_source __attribute__ ((cleanup(gs_local_settings_schema_source_unref)))

/**
 * gs_unref_settings_schema_key:
 *
 * Call g_settings_schema_key_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_settings_schema_key_unref(), the variable may be
 * %nullptr.

 */
#define gs_unref_settings_schema_key __attribute__ ((cleanup(gs_local_settings_schema_key_unref)))

/**
 * gs_free_gstring:
 *
 * Call g_string_free(TRUE) on a variable location when it goes out
 * of scope.  Note that unlike g_string_free(), the variable may
 * be %nullptr.
 */
#define gs_free_gstring __attribute__ ((cleanup(gs_local_gstring_free)))

/**
 * gs_free_option_context:
 *
 * Call g_regex_unref() on a variable location when it goes out of
 * scope.  Note that unlike g_option_context_free(), the variable may be
 * %NULL.

 */
#define gs_free_option_context __attribute__ ((cleanup(gs_local_option_context_free)))

/**
 * gs_close_dir:
 *
 * Call g_dir_close() on a variable location when it goes out of
 * scope.

 */
#define gs_close_dir __attribute__ ((cleanup(gs_local_dir_close)))

static inline void gs_local_fd_close (void *v)
{
  auto fd = *reinterpret_cast<int*>(v);
  if (fd != -1) {
    auto const errsv = errno;
    close(fd);
    errno = errsv;
  }
}

/**
 * gs_free_close:
 *
 * Call close() on a variable location when it goes out of
 * scope.

 */
#define gs_close_fd __attribute__ ((cleanup(gs_local_fd_close)))

G_END_DECLS

#endif
