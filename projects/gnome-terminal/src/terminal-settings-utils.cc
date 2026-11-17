/*
 *  Copyright © 2008, 2010, 2011, 2022 Christian Persch
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#define G_SETTINGS_ENABLE_BACKEND

#include <gio/gsettingsbackend.h>

#include "terminal-settings-utils.hh"
#include "terminal-client-utils.hh"
#include "terminal-debug.hh"
#include "terminal-libgsystem.hh"

#ifdef ENABLE_DEBUG

static gboolean
settings_change_event_cb(GSettings* settings,
                         void* keys,
                         int n_keys,
                         void* data)
{
  gs_free char* schema_id = nullptr;
  gs_free char* path = nullptr;
  g_object_get(settings,
               "schema-id", &schema_id,
               "path", &path,
               nullptr);

  auto const qkeys = reinterpret_cast<GQuark*>(keys);
  for (auto i = 0; i < n_keys; ++i) {
    auto key = g_quark_to_string(qkeys[i]);
    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                          "Bridge backend ::change-event schema %s path %s key %s\n",
                          schema_id, path, key);
  }

  return false; // propagate
}

static gboolean
settings_writable_change_event_cb(GSettings* settings,
                                  char const* key,
                                  void* data)
{
  gs_free char* schema_id = nullptr;
  gs_free char* path = nullptr;
  g_object_get(settings,
               "schema-id", &schema_id,
               "path", &path,
               nullptr);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::writeable-change-event schema %s path %s key %s\n",
                        schema_id, path, key);

  return false; // propagate
}

#endif /* ENABLE_DEBUG */

GSettings*
terminal_g_settings_new_with_path (GSettingsBackend* backend,
                                   GSettingsSchemaSource* source,
                                   char const* schema_id,
                                   char const* path)
{
  gs_unref_settings_schema GSettingsSchema* schema =
    g_settings_schema_source_lookup(source,
                                    schema_id,
                                    TRUE /* recursive */);
  g_assert_nonnull(schema);

  auto const settings = g_settings_new_full(schema,
                                            backend,
                                            path);

#ifdef ENABLE_DEBUG
  _TERMINAL_DEBUG_IF(TERMINAL_DEBUG_BRIDGE) {

    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                          "Creating GSettings for schema %s at %s with backend %s\n",
                          schema_id, path,
                          backend ? G_OBJECT_TYPE_NAME(backend) : "(default)");

    if (backend != nullptr &&
        g_str_equal(G_OBJECT_TYPE_NAME(backend), "TerminalSettingsBridgeBackend")) {
      g_signal_connect(settings,
                       "change-event",
                       G_CALLBACK(settings_change_event_cb),
                       nullptr);
      g_signal_connect(settings,
                       "writable-change-event",
                       G_CALLBACK(settings_writable_change_event_cb),
                       nullptr);
    }
  }
#endif /* ENABLE_DEBUG */

  return settings;
}

GSettings*
terminal_g_settings_new(GSettingsBackend* backend,
                        GSettingsSchemaSource* source,
                        char const* schema_id)
{
  return terminal_g_settings_new_with_path(backend, source, schema_id, nullptr);
}

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

void
terminal_g_settings_backend_clone_schema(GSettingsBackend* backend,
                                         GSettingsSchemaSource* schema_source,
                                         char const* schema_id,
                                         char const* path,
                                         char const* new_path,
                                         GTree* tree)
{
  gs_unref_settings_schema auto schema =
    g_settings_schema_source_lookup(schema_source, schema_id, true);
  if (schema == nullptr) [[unlikely]] // This shouldn't really happen ever
    return;

  gs_strfreev auto keys = g_settings_schema_list_keys(schema);

  for (auto i = 0; keys[i]; ++i) {
    gs_unref_settings_schema_key auto schema_key =
      g_settings_schema_get_key(schema, keys[i]);

    gs_free auto rkey = g_strconcat(path, keys[i], nullptr);
    auto const value =
      terminal_g_settings_backend_read(backend,
                                       rkey,
                                       g_settings_schema_key_get_value_type(schema_key),
                                       false);

    if (value) {
      g_tree_insert(tree,
                    g_strconcat(new_path, keys[i], nullptr), // transfer
                    value); // transfer
    }
  }
}

gboolean
terminal_g_settings_backend_erase_path(GSettingsBackend* backend,
                                       GSettingsSchemaSource* schema_source,
                                       char const* schema_id,
                                       char const* path)

{
  // We want to erase all keys below @path, not just keys we wrote ourself
  // or that are (currently) in a known schema.  DConf supports this kind of
  // 'directory reset' by writing a NULL value for the non-key @path (i.e.
  // which ends in a slash). However, neither g_settings_backend_reset() nor
  // g_settings_backend_write() accept a non-key path, and the latter
  // doesn't accept NULL values anyway. g_settings_backend_write_tree()
  // does allow NULL values, and the DConf backend works fine with this and
  // performs the directory reset, however it also (as is a documented
  // requirement) calls g_settings_backend_changed_tree() which chokes on
  // such a tree containing a non-key path.
  //
  // We could:
  // 1. Just do nothing, i.e. leave the deleted settings lying around.
  // 2. Fix glib. However, getting any improvements to gsettings into glib
  //    seems almost impossible at this point.
  // 3. Interpose a fixed g_settings_backend_changed_tree() that works
  //    with these non-key paths. This will work with out-of-tree
  //    settings backends like DConf. However, this will *not* work with
  //    the settings backends inside libgio, like the memory and keyfile
  //    backends, due to -Bsymbolic_functions.
  // 4. At least reset those keys we know might exists, i.e. those in
  //    the schema.
  //
  // Since I don't like 1, 2 is impossible, and 3 is too hacky, let's at least
  // do 4.

#if 0
  // This is how this function would ideally work if glib was fixed (option 2 above)
  auto tree = terminal_g_settings_backend_create_tree();
  g_tree_insert(tree, g_strdup(path), nullptr);
  auto const tag = &backend;
  auto const r = terminal_g_settings_backend_write_tree(backend, tree, tag);
  g_tree_unref(tree);
#endif

  gs_unref_settings_schema auto schema =
    g_settings_schema_source_lookup(schema_source, schema_id, true);
  if (schema == nullptr) [[unlikely]] // This shouldn't really happen ever
    return false;

  auto tree = terminal_g_settings_backend_create_tree();
  gs_strfreev auto keys = g_settings_schema_list_keys(schema);

  for (auto i = 0; keys[i]; ++i) {
    g_tree_insert(tree,
                  g_strconcat(path, keys[i], nullptr), // transfer
                  nullptr); // reset key
  }

  auto const tag = &backend;
  auto const r = terminal_g_settings_backend_write_tree(backend, tree, tag);
  g_tree_unref(tree);
  return r;
}

#endif /* TERMINAL_SERVER || TERMINAL_PREFERENCES */

#define TERMINAL_SCHEMA_VERIFIER_ERROR (g_quark_from_static_string("TerminalSchemaVerifier"))

typedef enum {
  TERMINAL_SCHEMA_VERIFIER_SCHEMA_MISSING,
  TERMINAL_SCHEMA_VERIFIER_SCHEMA_PATH,
  TERMINAL_SCHEMA_VERIFIER_KEY_MISSING,
  TERMINAL_SCHEMA_VERIFIER_KEY_TYPE,
  TERMINAL_SCHEMA_VERIFIER_KEY_DEFAULT,
  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_TYPE,
  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE,
  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_TYPE_UNKNOWN,
  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_TYPE_MISMATCH,
  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_ENUM_VALUE,
  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_INTERVAL,
  TERMINAL_SCHEMA_VERIFIER_CHILD_MISSING,
} TerminalSchemaVerifierError;

static gboolean
strv_contains(char const* const* strv,
              char const* str)
{
  if (strv == nullptr)
    return FALSE;

  for (size_t i = 0; strv[i]; i++) {
    if (g_str_equal (strv[i], str))
      return TRUE;
  }

  return FALSE;
}

static gboolean
strv_set_equal(char const* const* strv1,
               gsize len1,
               char const* const* strv2,
               gsize len2)
{
  if (len1 != len2)
    return false;

  for (auto i = gsize{0}; i < len2; ++i) {
    if (!strv_contains(strv1, strv2[i]))
        return false;
  }

  return true;
}

static gboolean
schema_key_range_compatible(GSettingsSchema* source_schema,
                            GSettingsSchemaKey* source_key,
                            char const* key,
                            GSettingsSchemaKey* reference_key,
                            GError** error)
{
  gs_unref_variant GVariant* source_range =
    g_settings_schema_key_get_range(source_key);
  gs_unref_variant GVariant* reference_range =
    g_settings_schema_key_get_range(reference_key);

  char const* source_type = nullptr;
  gs_unref_variant GVariant* source_data = nullptr;
  g_variant_get(source_range, "(&sv)", &source_type, &source_data);

  char const* reference_type = nullptr;
  gs_unref_variant GVariant* reference_data = nullptr;
  g_variant_get(reference_range, "(&sv)", &reference_type, &reference_data);

  if (!g_str_equal(source_type, reference_type)) {
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_TYPE,
                "Schema \"%s\" key \"%s\" has range type \"%s\" but reference range type is \"%s\"",
                g_settings_schema_get_id(source_schema),
                key, source_type, reference_type);
    return FALSE;
  }

  if (g_str_equal(reference_type, "type"))
    ; /* no constraints; this is fine */
  else if (g_str_equal(reference_type, "enum")) {
    size_t source_values_len = 0;
    gs_free char const** source_values = g_variant_get_strv(source_data, &source_values_len);

    size_t reference_values_len = 0;
    gs_free char const** reference_values = g_variant_get_strv(reference_data, &reference_values_len);

    // The sets of enum values in source and reference must be equal
    if (!strv_set_equal(source_values, source_values_len,
                        reference_values, reference_values_len)) {
      gs_free auto source_values_set_str = g_strjoinv(", ", (char**)source_values);
      gs_free auto reference_values_set_str = g_strjoinv(", ", (char**)reference_values);
      g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_ENUM_VALUE,
                  "Schema \"%s\" key \"%s\" enum values set {%s} not equal to reference schema set {%s}",
                  g_settings_schema_get_id(source_schema),
                  key,
                  source_values_set_str,
                  reference_values_set_str);
        return FALSE;
    }
  } else if (g_str_equal(reference_type, "flags")) {
    /* Our schemas don't use flags. If that changes, need to implement this! */
    g_assert_not_reached();
  } else if (g_str_equal(reference_type, "range")) {
    if (!g_variant_is_of_type(source_data,
                              g_variant_get_type(reference_data))) {
      char const* source_type_str = g_variant_get_type_string(source_data);
      char const* reference_type_str = g_variant_get_type_string(reference_data);
      g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_TYPE_MISMATCH,
                  "Schema \"%s\" key \"%s\" has range type \"%s\" but reference range type is \"%s\"",
                  g_settings_schema_get_id(source_schema),
                  key, source_type_str, reference_type_str);
      return FALSE;
    }

    gs_unref_variant GVariant* reference_min = nullptr;
    gs_unref_variant GVariant* reference_max = nullptr;
    g_variant_get(reference_data, "(**)", &reference_min, &reference_max);

    gs_unref_variant GVariant* source_min = nullptr;
    gs_unref_variant GVariant* source_max = nullptr;
    g_variant_get(source_data, "(**)", &source_min, &source_max);

    // The source interval must be equal to the reference interval
    if (g_variant_compare(source_min, reference_min) != 0 ||
        g_variant_compare(source_max, reference_max) != 0) {
      gs_free auto reference_min_str = g_variant_print(reference_min, true);
      gs_free auto reference_max_str = g_variant_print(reference_max, true);
      gs_free auto source_min_str = g_variant_print(source_min, true);
      gs_free auto source_max_str = g_variant_print(source_max, true);
      g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                  TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_INTERVAL,
                  "Schema \"%s\" key \"%s\" has range interval [%s, %s] not equal to the reference range interval [%s, %s]",
                  g_settings_schema_get_id(source_schema), key,
                  source_min_str, source_max_str,
                  reference_min_str, reference_max_str);
        return FALSE;
    }
  } else {
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_KEY_RANGE_TYPE_UNKNOWN,
                "Schema \"%s\" key \"%s\" has unknown range type \"%s\"",
                g_settings_schema_get_id(source_schema),
                key, reference_type);
    return FALSE;
  }

  return TRUE;
}

static gboolean
schema_verify_key(GSettingsSchema* source_schema,
                  char const* key,
                  GSettingsSchema* reference_schema,
                  GError** error)
{
  if (!g_settings_schema_has_key(source_schema, key)) {
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_KEY_MISSING,
                "Schema \"%s\" has missing key \"%s\"",
                g_settings_schema_get_id(source_schema), key);
    return FALSE;
  }

  gs_unref_settings_schema_key GSettingsSchemaKey* source_key =
    g_settings_schema_get_key(source_schema, key);
  g_assert_nonnull(source_key);

  gs_unref_settings_schema_key GSettingsSchemaKey* reference_key =
    g_settings_schema_get_key(reference_schema, key);
  g_assert_nonnull(reference_key);

  GVariantType const* source_type = g_settings_schema_key_get_value_type(source_key);
  GVariantType const* reference_type = g_settings_schema_key_get_value_type(reference_key);
  if (!g_variant_type_equal(source_type, reference_type)) {
    gs_free char* source_type_str = g_variant_type_dup_string(source_type);
    gs_free char* reference_type_str = g_variant_type_dup_string(reference_type);
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_KEY_TYPE,
                "Schema \"%s\" has type \"%s\" but reference type is \"%s\"",
                g_settings_schema_get_id(source_schema),
                source_type_str, reference_type_str);
    return FALSE;
  }

  gs_unref_variant GVariant* source_default = g_settings_schema_key_get_default_value(source_key);
  if (!g_settings_schema_key_range_check(reference_key, source_default)) {
    gs_free char* source_value_str = g_variant_print(source_default, TRUE);
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_KEY_DEFAULT,
                "Schema \"%s\" default value \"%s\" does not conform to reference schema",
                g_settings_schema_get_id(source_schema), source_value_str);
    return FALSE;
  }

  if (!schema_key_range_compatible(source_schema,
                                   source_key,
                                   key,
                                   reference_key,
                                   error))
    return FALSE;

  return TRUE;
}

static gboolean
schema_verify_child(GSettingsSchema* source_schema,
                    char const* child_name,
                    GSettingsSchema* reference_schema,
                    GError** error)
{
  /* Should verify the child's schema ID is as expected and exists in
   * the source, but there appears to be no API to get the schema ID of
   * the child.
   *
   * We work around this missing verification by never calling
   * g_settings_get_child() and instead always constructing the child
   * GSettings directly; and the existence and correctness of that
   * schema is verified by the per-schema checks.
   */

  return TRUE;
}

static gboolean
schema_verify(GSettingsSchema* source_schema,
              GSettingsSchema* reference_schema,
              GError** error)
{
  /* Verify path */
  char const* source_path = g_settings_schema_get_path(source_schema);
  char const* reference_path = g_settings_schema_get_path(reference_schema);
  if (g_strcmp0(source_path, reference_path) != 0) {
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_SCHEMA_PATH,
                "Schema \"%s\" has path \"%s\" but reference path is \"%s\"",
                g_settings_schema_get_id(source_schema),
                source_path ? source_path : "(null)",
                reference_path ? reference_path : "(null)");
    return FALSE;
  }

  /* Verify keys */
  gs_strfreev char** keys = g_settings_schema_list_keys(reference_schema);
  if (keys) {
    for (int i = 0; keys[i]; ++i) {
      if (!schema_verify_key(source_schema,
                             keys[i],
                             reference_schema,
                             error))
        return FALSE;
    }
  }

  /* Verify child schemas */
  gs_strfreev char** source_children = g_settings_schema_list_children(source_schema);
  gs_strfreev char** reference_children = g_settings_schema_list_children(reference_schema);
  if (reference_children) {
    for (size_t i = 0; reference_children[i]; ++i) {
      if (!strv_contains((char const* const*)source_children, reference_children[i])) {
        g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                    TERMINAL_SCHEMA_VERIFIER_CHILD_MISSING,
                    "Schema \"%s\" has missing child \"%s\"",
                    g_settings_schema_get_id(source_schema),
                    reference_children[i]);
        return FALSE;
      }

      if (!schema_verify_child(source_schema,
                               reference_children[i],
                               reference_schema,
                               error))
          return FALSE;
    }
  }

  return TRUE;
}

static gboolean
schemas_source_verify_schema_by_name(GSettingsSchemaSource* source,
                                     char const* schema_name,
                                     GSettingsSchemaSource* reference_source,
                                     GError** error)
{
  gs_unref_settings_schema GSettingsSchema* source_schema =
    g_settings_schema_source_lookup(source, schema_name, TRUE /* recursive */);

  if (!source_schema) {
    g_set_error(error, TERMINAL_SCHEMA_VERIFIER_ERROR,
                TERMINAL_SCHEMA_VERIFIER_SCHEMA_MISSING,
                "Schema \"%s\" is missing", schema_name);
    return FALSE;
  }

  gs_unref_settings_schema GSettingsSchema* reference_schema =
    g_settings_schema_source_lookup(reference_source,
                                    schema_name,
                                    FALSE /* recursive */);
  g_assert_nonnull(reference_schema);

  return schema_verify(source_schema,
                       reference_schema,
                       error);
}

static gboolean
schemas_source_verify_schemas(GSettingsSchemaSource* source,
                              char const* const* schemas,
                              GSettingsSchemaSource* reference_source,
                              GError** error)
{
  if (!schemas)
    return TRUE;

  for (int i = 0; schemas[i]; ++i) {
    if (!schemas_source_verify_schema_by_name(source,
                                              schemas[i],
                                              reference_source,
                                              error))
      return FALSE;
  }

  return TRUE;
}

static gboolean
schemas_source_verify(GSettingsSchemaSource* source,
                      GSettingsSchemaSource* reference_source,
                      GError** error)
{
  gs_strfreev char** reloc_schemas = nullptr;
  gs_strfreev char** nonreloc_schemas = nullptr;

  g_settings_schema_source_list_schemas(reference_source,
                                        FALSE /* recursive */,
                                        &reloc_schemas,
                                        &nonreloc_schemas);

  if (!schemas_source_verify_schemas(source,
                                     (char const* const*)reloc_schemas,
                                     reference_source,
                                     error))
    return FALSE;

  if (!schemas_source_verify_schemas(source,
                                     (char const* const*)nonreloc_schemas,
                                     reference_source,
                                     error))
    return FALSE;

  return TRUE;
}

GSettingsSchemaSource*
terminal_g_settings_schema_source_get_default(void)
{
  GSettingsSchemaSource* default_source = g_settings_schema_source_get_default();

  gs_free auto schema_dir =
    terminal_client_get_directory_uninstalled(
#if defined(TERMINAL_SERVER)
                                              TERM_LIBEXECDIR,
#elif defined(TERMINAL_PREFERENCES)
                                              TERM_LIBEXECDIR,
#elif defined(TERMINAL_CLIENT)
                                              TERM_BINDIR,
#else
#error Need to define installed location
#endif
                                              TERM_PKGLIBDIR,
                                              "gschemas.compiled",
                                              GFileTest(0));

  gs_free_error GError* error = nullptr;
  GSettingsSchemaSource* reference_source =
    g_settings_schema_source_new_from_directory(schema_dir,
                                                nullptr /* parent source */,
                                                FALSE /* trusted */,
                                                &error);
  if (!reference_source)  {
    /* Can only use the installed schemas, or abort here. */
    g_printerr("Failed to load reference schemas: %s\n"
               "Using unverified installed schemas.\n",
               error->message);

    return g_settings_schema_source_ref(default_source);
  }

  if (!schemas_source_verify(default_source, reference_source, &error)) {
    g_printerr("Installed schemas failed verification: %s\n"
               "Falling back to built-in reference schemas.\n",
               error->message);

    return reference_source; /* transfer */
  }

  /* Installed schemas verified; use them. */
  g_settings_schema_source_unref(reference_source);
  return g_settings_schema_source_ref(default_source);
}

// BEGIN copied from glib/gio/gsettingsbackend.c

/*
 * Copyright © 2009, 2010 Codethink Limited
 * Copyright © 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Matthias Clasen <mclasen@redhat.com>
 */

static void
variant_unref0(void* data)
{
  if (data)
    g_variant_unref(reinterpret_cast<GVariant*>(data));
}

static int
compare_string(void const* a,
               void const* b,
               void* closure)
{
  return strcmp(reinterpret_cast<char const*>(a),
                reinterpret_cast<char const*>(b));
}

/*
 * terminal_g_settings_backend_create_tree:
 *
 * This is a convenience function for creating a tree that is compatible
 * with terminal_g_settings_backend_write().  It merely calls g_tree_new_full()
 * with strcmp(), g_free() and g_variant_unref().
 *
 * Returns: (transfer full): a new #GTree
 */
GTree*
terminal_g_settings_backend_create_tree(void)
{
  return g_tree_new_full(compare_string, nullptr,
                         g_free,
                         variant_unref0);
}

#ifdef ENABLE_DEBUG

static gboolean
print_tree(void* key,
           void* value,
           void* closure)
{
  g_printerr("  %s => %s\n",
             reinterpret_cast<char const*>(key),
             value ? g_variant_print(reinterpret_cast<GVariant*>(value), true): "(null)");

  return false; // continue
}

void
terminal_g_settings_backend_print_tree(GTree* tree)
{
  g_printerr("Settings tree: [\n");
  g_tree_foreach(tree, print_tree, nullptr);
  g_printerr("]\n");
}

#endif /* ENABLE_DEBUG */

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

/*
 * g_settings_backend_read:
 * @backend: a #GSettingsBackend implementation
 * @key: the key to read
 * @expected_type: a #GVariantType
 * @default_value: if the default value should be returned
 *
 * Reads a key. This call will never block.
 *
 * If the key exists, the value associated with it will be returned.
 * If the key does not exist, %nullptr will be returned.
 *
 * The returned value will be of the type given in @expected_type.  If
 * the backend stored a value of a different type then %nullptr will be
 * returned.
 *
 * If @default_value is %TRUE then this gets the default value from the
 * backend (ie: the one that the backend would contain if
 * g_settings_reset() were called).
 *
 * Returns: (nullable) (transfer full): the value that was read, or %nullptr
 */
GVariant*
terminal_g_settings_backend_read(GSettingsBackend* backend,
                                 char const* key,
                                 GVariantType const* expected_type,
                                 gboolean default_value)
{
  auto value = G_SETTINGS_BACKEND_GET_CLASS(backend)->read (backend,
                                                            key,
                                                            expected_type,
                                                            default_value);

  if (value)
    value = g_variant_take_ref(value);

  if (value && !g_variant_is_of_type(value, expected_type)) [[unlikely]] {
    g_clear_pointer(&value, g_variant_unref);
  }

  return value;
}

/*
 * terminal_g_settings_backend_read_user_value:
 * @backend: a #GSettingsBackend implementation
 * @key: the key to read
 * @expected_type: a #GVariantType
 *
 * Reads the 'user value' of a key.
 *
 * This is the value of the key that the user has control over and has
 * set for themselves.  Put another way: if the user did not set the
 * value for themselves, then this will return %nullptr(even if the
 * sysadmin has provided a default value).
 *
 * Returns:(nullable)(transfer full): the value that was read, or %nullptr
 */
GVariant*
terminal_g_settings_backend_read_user_value(GSettingsBackend* backend,
                                            char const*key,
                                            GVariantType const* expected_type)
{
  auto value = G_SETTINGS_BACKEND_GET_CLASS(backend)->read_user_value(backend,
                                                                      key,
                                                                      expected_type);

  if (value)
    value = g_variant_take_ref(value);

  if (value && !g_variant_is_of_type(value, expected_type)) [[unlikely]] {
    g_clear_pointer(&value, g_variant_unref);
  }

  return value;
}

/*
 * terminal_g_settings_backend_write:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of the key
 * @value: a #GVariant value to write to this key
 * @origin_tag: the origin tag
 *
 * Writes exactly one key.
 *
 * This call does not fail.  During this call a
 * #GSettingsBackend::changed signal will be emitted if the value of the
 * key has changed.  The updated key value will be visible to any signal
 * callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal(either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 *
 * If @value has a floating reference, it will be sunk.
 *
 * Returns: %TRUE if the write succeeded, %FALSE if the key was not writable
 */
gboolean
terminal_g_settings_backend_write(GSettingsBackend* backend,
                                  char const* key,
                                  GVariant* value,
                                  void* origin_tag)
{
  g_variant_ref_sink(value);
  auto const success = G_SETTINGS_BACKEND_GET_CLASS(backend)->write(backend,
                                                                    key,
                                                                    value,
                                                                    origin_tag);
  g_variant_unref(value);

  return success;
}

/*
 * terminal_g_settings_backend_write_tree:
 * @backend: a #GSettingsBackend implementation
 * @tree: a #GTree containing key-value pairs to write
 * @origin_tag: the origin tag
 *
 * Writes one or more keys.  This call will never block.
 *
 * The key of each item in the tree is the key name to write to and the
 * value is a #GVariant to write.  The proper type of #GTree for this
 * call can be created with terminal_g_settings_backend_create_tree().  This call
 * might take a reference to the tree; you must not modified the #GTree
 * after passing it to this call.
 *
 * This call does not fail.  During this call a #GSettingsBackend::changed
 * signal will be emitted if any keys have been changed.  The new values of
 * all updated keys will be visible to any signal callbacks.
 *
 * One possible method that an implementation might deal with failures is
 * to emit a second "changed" signal(either during this call, or later)
 * to indicate that the affected keys have suddenly "changed back" to their
 * old values.
 */
gboolean
terminal_g_settings_backend_write_tree(GSettingsBackend* backend,
                                       GTree* tree,
                                       void* origin_tag)
{
  return G_SETTINGS_BACKEND_GET_CLASS(backend)->write_tree(backend,
                                                           tree,
                                                           origin_tag);
}

/*
 * terminal_g_settings_backend_reset:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of a key
 * @origin_tag: the origin tag
 *
 * "Resets" the named key to its "default" value(ie: after system-wide
 * defaults, mandatory keys, etc. have been taken into account) or possibly
 * unsets it.
 */
void
terminal_g_settings_backend_reset(GSettingsBackend*backend,
                                  char const* key,
                                  void* origin_tag)
{
  G_SETTINGS_BACKEND_GET_CLASS(backend)->reset(backend, key, origin_tag);
}

/*
 * terminal_g_settings_backend_get_writable:
 * @backend: a #GSettingsBackend implementation
 * @key: the name of a key
 *
 * Finds out if a key is available for writing to.  This is the
 * interface through which 'lockdown' is implemented.  Locked down
 * keys will have %FALSE returned by this call.
 *
 * You should not write to locked-down keys, but if you do, the
 * implementation will deal with it.
 *
 * Returns: %TRUE if the key is writable
 */
gboolean
terminal_g_settings_backend_get_writable(GSettingsBackend* backend,
                                         char const* key)
{
  return G_SETTINGS_BACKEND_GET_CLASS(backend)->get_writable(backend, key);
}

/*
 * terminal_g_settings_backend_unsubscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Reverses the effect of a previous call to
 * terminal_g_settings_backend_subscribe().
 */
void
terminal_g_settings_backend_unsubscribe(GSettingsBackend* backend,
                                        const char* name)
{
  G_SETTINGS_BACKEND_GET_CLASS(backend)->unsubscribe(backend, name);
}

/*
 * terminal_g_settings_backend_subscribe:
 * @backend: a #GSettingsBackend
 * @name: a key or path to subscribe to
 *
 * Requests that change signals be emitted for events on @name.
 */
void
terminal_g_settings_backend_subscribe(GSettingsBackend* backend,
                                      char const* name)
{
  G_SETTINGS_BACKEND_GET_CLASS(backend)->subscribe(backend, name);
}

/*
 * terminal_g_settings_backend_get_permission:
 * @backend: a #GSettingsBackend
 * @path: a path
 *
 * Gets the permission object associated with writing to keys below
 * @path on @backend.
 *
 * If this is not implemented in the backend, then a %TRUE
 * #GSimplePermission is returned.
 *
 * Returns:(not nullable)(transfer full): a non-%nullptr #GPermission.
 *     Free with g_object_unref()
 */
GPermission*
terminal_g_settings_backend_get_permission(GSettingsBackend* backend,
                                           char const* path)
{
  auto const klass = G_SETTINGS_BACKEND_GET_CLASS(backend);
  if (klass->get_permission)
    return klass->get_permission(backend, path);

  return g_simple_permission_new(TRUE);
}

/*
 * terminal_g_settings_backend_sync_default:
 *
 * Syncs.
 */
void
terminal_g_settings_backend_sync(GSettingsBackend* backend)
{
  auto const klass = G_SETTINGS_BACKEND_GET_CLASS(backend);
  if (klass->sync)
    klass->sync(backend);
}

// END copied from glib

// Note: Since D-Bus/GDBus does not support GVariant maybe types (not even
// on private peer-to-peer connections), we need to wrap the variants
// for transport over the bus.  The format is a "mv" variant whose inner
// value is the variant to transport, or Nothing for a nullptr GVariant.
// We then transport that variant in serialised form as a byte array over
// the bus.

/*
 * terminal_g_variant_wrap:
 * @variant: (nullable): a #GVariant, or %NULL
 *
 * Wraps @variant for transport over D-Bus.
 * if @variant is floating, it is consumed.
 *
 * Returns: (transfer full): a floating variant wrapping @variant
 */
GVariant*
terminal_g_variant_wrap(GVariant* variant)
{
  auto const value = variant ? g_variant_new_variant(variant) : nullptr;
  auto const maybe = g_variant_new_maybe(G_VARIANT_TYPE_VARIANT, value);

  return g_variant_new_from_data(G_VARIANT_TYPE("ay"),
                                 g_variant_get_data(maybe),
                                 g_variant_get_size(maybe),
                                 false, // trusted
                                 GDestroyNotify(g_variant_unref),
                                 g_variant_ref_sink(maybe)); // adopts
}

/*
 * terminal_g_variant_unwrap:
 * @variant: a "ay" #GVariant
 *
 * Unwraps a variant transported over D-Bus.
 * If @variant is floating, it is NOT consumed.
 *
 * Returns: (transfer full): a non-floating variant unwrapping @variant
 */
GVariant*
terminal_g_variant_unwrap(GVariant* variant)
{
  g_return_val_if_fail(g_variant_is_of_type(variant, G_VARIANT_TYPE("ay")), nullptr);

  gs_unref_bytes auto bytes = g_variant_get_data_as_bytes(variant);
  gs_unref_variant auto maybe = g_variant_take_ref(g_variant_new_from_bytes(G_VARIANT_TYPE("mv"), bytes, false));
  gs_unref_variant auto value = g_variant_get_maybe(maybe);
  return value ? g_variant_get_variant(value) : nullptr;
}


#endif /* TERMINAL_SERVER || TERMINAL_PREFERENCES */
