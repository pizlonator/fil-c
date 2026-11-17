/*
 * Copyright © 2013 Christian Persch
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

#include "config.h"

#include "terminal-settings-list.hh"
#include "terminal-client-utils.hh"

#include <string.h>
#include <uuid.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#include "terminal-type-builtins.hh"
#include "terminal-settings-utils.hh"
#include "terminal-schemas.hh"
#include "terminal-debug.hh"
#include "terminal-libgsystem.hh"

struct _TerminalSettingsList {
  GSettings parent;

  GSettingsBackend* settings_backend;
  GSettingsSchemaSource* schema_source;
  char *path;
  char *child_schema_id;

  char **uuids;
  char *default_uuid;

  GHashTable *children;

  TerminalSettingsListFlags flags;
};

struct _TerminalSettingsListClass {
  GSettingsClass parent;

  void (* children_changed) (TerminalSettingsList *list);
  void (* default_changed)  (TerminalSettingsList *list);
};

enum {
  PROP_SCHEMA_SOURCE = 1,
  PROP_CHILD_SCHEMA_ID,
  PROP_FLAGS
};

enum {
  SIGNAL_CHILDREN_CHANGED,
  SIGNAL_DEFAULT_CHANGED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static void
strv_printerr (char **strv)
{
  char **p;

  if (strv == nullptr) {
    g_printerr ("(null)");
    return;
  }

  for (p = strv; *p; p++)
    g_printerr ("%s'%s'", p != strv ? ", " : "", *p);
}

static char **
strv_sort (char **strv)
{
  // FIXMEchpe
  return strv;
}

static gboolean
strv_equal (char **a,
            char **b)
{
  char **e, **f;

  if (a == nullptr || b == nullptr)
    return a == b;

  for (e = a, f = b; *e && *f; e++, f++) {
    if (!g_str_equal (*e, *f))
      return FALSE;
  }

  return *e == *f;
}

static int
strv_find (char **strv,
           const char *str)
{
  int i;

  if (strv == nullptr || str == nullptr)
    return -1;

  for (i = 0; strv[i]; i++) {
    if (!g_str_equal (strv[i], str))
      continue;

    return i;
  }

  return -1;
}

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

static char **
strv_dupv_insert (char **strv,
                  const char *str)
{
  char **nstrv, **p, **q;

  if (strv == nullptr) {
    char *s[2] = { (char *) str, nullptr };
    return g_strdupv (s);
  }

  /* Is it already in the list? */
  for (p = strv; *p; p++)
    if (g_str_equal (*p, str))
      return g_strdupv (strv);

  /* Not found; append */
  nstrv = g_new (char *, (p - strv) + 2);
  for (p = strv, q = nstrv; *p; p++, q++)
    *q = g_strdup (*p);
  *q++ = g_strdup (str);
  *q = nullptr;

  return strv_sort (nstrv);
}

static char **
strv_dupv_remove (char **strv,
                  const char *str)
{
  char **nstrv, **p, **q;

  if (strv == nullptr)
    return nullptr;

  nstrv = g_strdupv (strv);
  for (p = q = nstrv; *p; p++) {
    if (!g_str_equal (*p, str))
      *q++ = *p;
    else
      g_free (*p);
  }
  *q = nullptr;

  return nstrv;
}

#endif /* TERMINAL_SERVER || TERMINAL_PREFERENCES */

gboolean
terminal_settings_list_valid_uuid (const char *str)
{
  uuid_t u;

  if (str == nullptr)
    return FALSE;

  return uuid_parse ((char *) str, u) == 0;
}

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

static char *
new_list_entry (void)
{
  uuid_t u;
  char name[37];

  uuid_generate (u);
  uuid_unparse (u, name);

  return g_strdup (name);
}

#endif /* TERMINAL_SERVER || TERMINAL_PREFERENCES */

static gboolean
validate_list (TerminalSettingsList *list,
               char **entries)
{
  gboolean allow_empty = (list->flags & TERMINAL_SETTINGS_LIST_FLAG_ALLOW_EMPTY) != 0;
  guint i;

  if (entries == nullptr)
    return allow_empty;

  for (i = 0; entries[i]; i++) {
    if (!terminal_settings_list_valid_uuid (entries[i]))
      return FALSE;
  }

  return (i > 0) || allow_empty;
}

static gboolean
list_map_func (GVariant *value,
               gpointer *result,
               gpointer user_data)
{
  TerminalSettingsList *list = (TerminalSettingsList*)user_data;
  gs_strfreev char **entries;

  entries = strv_sort (g_variant_dup_strv (value, nullptr));

  if (validate_list (list, entries)) {
    gs_transfer_out_value(result, &entries);
    return TRUE;
  }

  return FALSE;
}

static char*
path_new (TerminalSettingsList *list,
          char const* uuid)
{
  return g_strdup_printf ("%s:%s/", list->path, uuid);
}

static GSettings *
terminal_settings_list_ref_child_internal (TerminalSettingsList *list,
                                           const char *uuid)
{
  GSettings *child;
  gs_free char *path = nullptr;

  if (strv_find (list->uuids, uuid) == -1)
    return nullptr;

  _terminal_debug_print (TERMINAL_DEBUG_SETTINGS_LIST,
                         "%s UUID %s\n", G_STRFUNC, uuid);

  child = (GSettings*)g_hash_table_lookup (list->children, uuid);
  if (child)
    goto done;

  path = path_new (list, uuid);
  child = terminal_g_settings_new_with_path(list->settings_backend,
                                            list->schema_source,
                                            list->child_schema_id,
                                            path);
  g_hash_table_insert (list->children, g_strdup (uuid), child /* adopted */);

 done:
  return (GSettings*)g_object_ref(child);
}

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

static char *
terminal_settings_list_add_child_internal (TerminalSettingsList *list,
                                           const char *uuid,
                                           const char *name)
{

  auto const new_uuid = new_list_entry();
  _terminal_debug_print (TERMINAL_DEBUG_SETTINGS_LIST,
                         "%s NEW UUID %s\n", G_STRFUNC, new_uuid);

  gs_free auto path = path_new(list, uuid);
  gs_free auto new_path = path_new(list, new_uuid);

  auto tree = terminal_g_settings_backend_create_tree();
  terminal_g_settings_backend_clone_schema(list->settings_backend,
                                           list->schema_source,
                                           list->child_schema_id,
                                           path,
                                           new_path,
                                           tree);
  if (name) {
    g_tree_insert(tree,
                  g_strconcat(new_path, TERMINAL_PROFILE_VISIBLE_NAME_KEY, nullptr), // transfer
                  g_variant_take_ref(g_variant_new_string(name))); // transfer
  }

#ifdef ENABLE_DEBUG
  _TERMINAL_DEBUG_IF(TERMINAL_DEBUG_SETTINGS_LIST) {
    g_printerr("Cloning schema %s from %s -> %s\n", list->child_schema_id, path, new_path);
    terminal_g_settings_backend_print_tree(tree);
  }
#endif

  auto const tag = &list;
  (void)terminal_g_settings_backend_write_tree(list->settings_backend, tree, tag);
  g_tree_unref(tree);

  gs_strfreev auto new_uuids = strv_dupv_insert(list->uuids, new_uuid);
  g_settings_set_strv(&list->parent, TERMINAL_SETTINGS_LIST_LIST_KEY,
                      (char const* const*)new_uuids);

  return new_uuid;
}

static void
terminal_settings_list_remove_child_internal (TerminalSettingsList *list,
                                              const char *uuid)
{
  gs_strfreev char **new_uuids;

  _terminal_debug_print (TERMINAL_DEBUG_SETTINGS_LIST,
                         "%s UUID %s\n", G_STRFUNC, uuid);

  new_uuids = strv_dupv_remove (list->uuids, uuid);

  if ((new_uuids == nullptr || new_uuids[0] == nullptr) &&
      (list->flags & TERMINAL_SETTINGS_LIST_FLAG_ALLOW_EMPTY) == 0)
    return;

  g_settings_set_strv (&list->parent, TERMINAL_SETTINGS_LIST_LIST_KEY, (const char * const *) new_uuids);

  if (list->default_uuid != nullptr &&
      g_str_equal (list->default_uuid, uuid))
    g_settings_set_string (&list->parent, TERMINAL_SETTINGS_LIST_DEFAULT_KEY, "");

  /* Now we unset all keys under the child */
  gs_free auto path = path_new(list, uuid);
  terminal_g_settings_backend_erase_path(list->settings_backend,
                                         list->schema_source,
                                         list->child_schema_id,
                                         path);
}

#endif /* TERMINAL_SERVER || TERMINAL_PREFERENCES */

static void
terminal_settings_list_update_list (TerminalSettingsList *list)
{
  char **uuids, *uuid;
  GSettings *child;
  GHashTable *new_children;
  guint i;
  gboolean changed;

  uuids = (char**)g_settings_get_mapped (&list->parent,
					 TERMINAL_SETTINGS_LIST_LIST_KEY,
					 list_map_func, list);

  _TERMINAL_DEBUG_IF (TERMINAL_DEBUG_SETTINGS_LIST) {
    g_printerr ("%s: current UUIDs [", G_STRFUNC);
    strv_printerr (list->uuids);
    g_printerr ("]\n new UUIDs [");
    strv_printerr (uuids);
    g_printerr ("]\n");
  }

  if (strv_equal (uuids, list->uuids) &&
      ((list->flags & TERMINAL_SETTINGS_LIST_FLAG_HAS_DEFAULT) == 0 ||
       strv_find (list->uuids, list->default_uuid) != -1)) {
    g_strfreev (uuids);
    return;
  }

  new_children = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        (GDestroyNotify) g_free,
                                        (GDestroyNotify) g_object_unref);

  if (uuids) {
    for (i = 0; uuids[i] != nullptr; i++) {
      uuid = uuids[i];

      child = (GSettings*)g_hash_table_lookup (list->children, uuid);

      if (child) {
        g_object_ref (child);
        g_hash_table_remove (list->children, uuid);
        g_hash_table_insert (new_children, g_strdup (uuid), child /* adopted */);
      }
    }

    changed = !strv_equal (uuids, list->uuids);
  } else {
    changed = g_strv_length (list->uuids) != 0;
  }

  g_hash_table_unref (list->children);
  list->children = new_children;

  g_strfreev (list->uuids);
  list->uuids = uuids; /* adopts */

  if (changed)
    g_signal_emit (list, signals[SIGNAL_CHILDREN_CHANGED], 0);
}

static void
terminal_settings_list_update_default (TerminalSettingsList *list)
{
  if ((list->flags & TERMINAL_SETTINGS_LIST_FLAG_HAS_DEFAULT) == 0)
    return;

  g_free (list->default_uuid);
  list->default_uuid = g_settings_get_string (&list->parent,
                                              TERMINAL_SETTINGS_LIST_DEFAULT_KEY);

  _terminal_debug_print (TERMINAL_DEBUG_SETTINGS_LIST,
                         "%s new default UUID %s\n", G_STRFUNC, list->default_uuid);

  g_signal_emit (list, signals[SIGNAL_DEFAULT_CHANGED], 0);
}

G_DEFINE_TYPE (TerminalSettingsList, terminal_settings_list, G_TYPE_SETTINGS);

static void
terminal_settings_list_changed (GSettings *list_settings,
                                const char *key)
{
  TerminalSettingsList *list = TERMINAL_SETTINGS_LIST (list_settings);

  _terminal_debug_print (TERMINAL_DEBUG_SETTINGS_LIST,
                         "%s key %s", G_STRFUNC, key ? key : "(null)");

  if (key == nullptr ||
      g_str_equal (key, TERMINAL_SETTINGS_LIST_LIST_KEY)) {
    terminal_settings_list_update_list (list);
    terminal_settings_list_update_default (list);
  }

  if (key == nullptr)
    return;

  if (g_str_equal (key, TERMINAL_SETTINGS_LIST_DEFAULT_KEY)) {
    terminal_settings_list_update_default (list);
  }
}

static void
terminal_settings_list_init (TerminalSettingsList *list)
{
  list->flags = TERMINAL_SETTINGS_LIST_FLAG_NONE;
}

static void
terminal_settings_list_constructed (GObject *object)
{
  TerminalSettingsList *list = TERMINAL_SETTINGS_LIST (object);

  G_OBJECT_CLASS (terminal_settings_list_parent_class)->constructed (object);

  g_object_get(object, "backend", &list->settings_backend, nullptr);
  g_assert(list->settings_backend);
  g_assert(list->schema_source);

  g_assert (list->schema_source != nullptr);
  g_assert (list->child_schema_id != nullptr);

  g_object_get (object, "path", &list->path, nullptr);

  list->children = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          (GDestroyNotify) g_free,
                                          (GDestroyNotify) g_object_unref);

  terminal_settings_list_changed (&list->parent, nullptr);
}

static void
terminal_settings_list_finalize (GObject *object)
{
  TerminalSettingsList *list = TERMINAL_SETTINGS_LIST (object);

  g_free (list->path);
  g_free (list->child_schema_id);
  g_strfreev (list->uuids);
  g_free (list->default_uuid);
  g_hash_table_unref (list->children);
  g_settings_schema_source_unref(list->schema_source);
  g_clear_object(&list->settings_backend);

  G_OBJECT_CLASS (terminal_settings_list_parent_class)->finalize (object);
}

static void
terminal_settings_list_set_property (GObject *object,
                                     guint prop_id,
                                     const GValue *value,
                                     GParamSpec *pspec)
{
  TerminalSettingsList *list = TERMINAL_SETTINGS_LIST (object);

  switch (prop_id) {
  case PROP_SCHEMA_SOURCE: {
    auto const schema_source = reinterpret_cast<GSettingsSchemaSource*>(g_value_get_boxed(value));
    list->schema_source = g_settings_schema_source_ref(schema_source);
    break;
  }
    case PROP_CHILD_SCHEMA_ID:
      list->child_schema_id = g_value_dup_string (value);
      break;
    case PROP_FLAGS:
      list->flags = TerminalSettingsListFlags(g_value_get_flags (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_settings_list_class_init (TerminalSettingsListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GSettingsClass *settings_class = G_SETTINGS_CLASS (klass);

  object_class->set_property = terminal_settings_list_set_property;
  object_class->constructed = terminal_settings_list_constructed;
  object_class->finalize = terminal_settings_list_finalize;

  /**
   * TerminalSettingsList:child-schema-id:
   *
   * The name of the schema of the children of this list.
   */
  g_object_class_install_property (object_class, PROP_SCHEMA_SOURCE,
                                   g_param_spec_boxed("schema-source", nullptr, nullptr,
                                                      G_TYPE_SETTINGS_SCHEMA_SOURCE,
                                                      GParamFlags(G_PARAM_CONSTRUCT_ONLY |
                                                                  G_PARAM_WRITABLE |
                                                                  G_PARAM_STATIC_STRINGS)));

  /**
   * TerminalSettingsList:child-schema-id:
   *
   * The name of the schema of the children of this list.
   */
  g_object_class_install_property (object_class, PROP_CHILD_SCHEMA_ID,
                                   g_param_spec_string ("child-schema-id", nullptr, nullptr,
                                                        nullptr,
                                                        GParamFlags(G_PARAM_CONSTRUCT_ONLY |
								    G_PARAM_WRITABLE |
								    G_PARAM_STATIC_STRINGS)));

  /**
   * TerminalSettingsList:flags:
   *
   * Flags from #TerminalSettingsListFlags.
   */
  g_object_class_install_property (object_class, PROP_FLAGS,
                                   g_param_spec_flags ("flags", nullptr,nullptr,
                                                       TERMINAL_TYPE_SETTINGS_LIST_FLAGS,
                                                       TERMINAL_SETTINGS_LIST_FLAG_NONE,
                                                       GParamFlags(G_PARAM_CONSTRUCT_ONLY |
								   G_PARAM_WRITABLE |
								   G_PARAM_STATIC_STRINGS)));

  /**
   * TerminalSettingsList::children-changed:
   * @list: the object on which the signal was emitted
   *
   * The "children-changed" signal is emitted when the list of children
   * has potentially changed.
   */
  signals[SIGNAL_CHILDREN_CHANGED] =
    g_signal_new ("children-changed", TERMINAL_TYPE_SETTINGS_LIST,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalSettingsListClass, children_changed),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  /**
   * TerminalSettingsList::default-changed:
   * @list: the object on which the signal was emitted
   *
   * The "default-changed" signal is emitted when the default child
   * has potentially changed.
   */
  signals[SIGNAL_DEFAULT_CHANGED] =
    g_signal_new ("default-changed", TERMINAL_TYPE_SETTINGS_LIST,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalSettingsListClass, default_changed),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  settings_class->changed = terminal_settings_list_changed;
}


/**
 * terminal_settings_list_new:
 * @backend: (nullable): a #GSettingsBackend, or %NULL
 * @schema_source: a #GSettingsSchemaSource
 * @path: the settings path for the list
 * @schema_id: the schema of the list, equal to or derived from "org.gnome.Terminal.SettingsList"
 * @child_schema_id: the schema of the list children
 * @flags: list flags
 *
 * Returns: (transfer full): the newly created #TerminalSettingsList
 */
TerminalSettingsList *
terminal_settings_list_new (GSettingsBackend* backend,
                            GSettingsSchemaSource* schema_source,
                            const char *path,
                            const char *schema_id,
                            const char *child_schema_id,
                            TerminalSettingsListFlags flags)
{
  g_return_val_if_fail (backend == nullptr || G_IS_SETTINGS_BACKEND (backend), nullptr);
  g_return_val_if_fail (schema_source != nullptr, nullptr);
  g_return_val_if_fail (path != nullptr, nullptr);
  g_return_val_if_fail (schema_id != nullptr, nullptr);
  g_return_val_if_fail (child_schema_id != nullptr, nullptr);
  g_return_val_if_fail (g_str_has_suffix (path, ":/"), nullptr);

  gs_unref_settings_schema auto schema =
    g_settings_schema_source_lookup(schema_source, schema_id, true);
  g_assert_nonnull(schema);

  return reinterpret_cast<TerminalSettingsList*>(g_object_new (TERMINAL_TYPE_SETTINGS_LIST,
                                                               "backend", backend,
                                                               "schema-source", schema_source,
							       "settings-schema", schema,
							       "child-schema-id", child_schema_id,
							       "path", path,
							       "flags", flags,
							       nullptr));
}

/**
 * terminal_settings_list_dupv_children:
 * @list: a #TerminalSettingsList
 *
 * Returns: (transfer full): the UUIDs of the children in the settings list, or %nullptr
 *  if the list is empty
 */
char **
terminal_settings_list_dupv_children (TerminalSettingsList *list)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);

  return g_strdupv (list->uuids);
}

/**
 * terminal_settings_list_dup_default_child:
 * @list: a #TerminalSettingsList
 *
 * Returns: (transfer full): the UUID of the default child in the settings list
 */
char *
terminal_settings_list_dup_default_child (TerminalSettingsList *list)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);

  if ((list->flags & TERMINAL_SETTINGS_LIST_FLAG_HAS_DEFAULT) == 0)
    return nullptr;

  if ((strv_find (list->uuids, list->default_uuid)) != -1)
    return g_strdup (list->default_uuid);

  /* Just randomly designate the first child as default, but don't write that
   * to the settings.
   */
  if (list->uuids == nullptr || list->uuids[0] == nullptr) {
    g_warn_if_fail ((list->flags & TERMINAL_SETTINGS_LIST_FLAG_ALLOW_EMPTY));
    return nullptr;
  }

  return g_strdup (list->uuids[0]);
}

/**
 * terminal_settings_list_has_child:
 * @list: a #TerminalSettingsList
 * @uuid: the UUID of a list child
 *
 * Returns: %TRUE iff the child with @uuid exists
 */
gboolean
terminal_settings_list_has_child (TerminalSettingsList *list,
                                  const char *uuid)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), FALSE);
  g_return_val_if_fail (terminal_settings_list_valid_uuid (uuid), FALSE);

  return strv_find (list->uuids, uuid) != -1;
}

/**
 * terminal_settings_list_ref_child:
 * @list: a #TerminalSettingsList
 * @uuid: the UUID of a list child
 *
 * Returns the child #GSettings for the list child with UUID @uuid, or %nullptr
 *   if @list has no such child.
 *
 * Returns: (transfer full): a reference to the #GSettings for the child, or %nullptr
 */
GSettings *
terminal_settings_list_ref_child (TerminalSettingsList *list,
                                  const char *uuid)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);
  g_return_val_if_fail (terminal_settings_list_valid_uuid (uuid), nullptr);

  return terminal_settings_list_ref_child_internal (list, uuid);
}

/**
 * terminal_settings_list_ref_children:
 * @list: a #TerminalSettingsList
 *
 * Returns the list of children #GSettings or @list.
 *
 * Returns: (transfer full): a list of child #GSettings of @list
 */
GList *
terminal_settings_list_ref_children (TerminalSettingsList *list)
{
  GList *l;
  guint i;

  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);

  if (list->uuids == nullptr)
    return nullptr;

  l = nullptr;
  for (i = 0; list->uuids[i]; i++)
    l = g_list_prepend (l, terminal_settings_list_ref_child (list, list->uuids[i]));

  return g_list_reverse (l);
}

/**
 * terminal_settings_list_ref_default_child:
 * @list: a #TerminalSettingsList
 *
 * Returns the default child #GSettings for the list, or %nullptr if @list has no
 *   children.
 *
 * Returns: (transfer full): a reference to the #GSettings for the default child, or %nullptr
 */
GSettings *
terminal_settings_list_ref_default_child (TerminalSettingsList *list)
{
  gs_free char *uuid = nullptr;

  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);

  uuid = terminal_settings_list_dup_default_child (list);
  if (uuid == nullptr)
    return nullptr;

  return terminal_settings_list_ref_child_internal (list, uuid);
}

#if defined(TERMINAL_SERVER) || defined(TERMINAL_PREFERENCES)

/**
 * terminal_settings_list_add_child:
 * @list: a #TerminalSettingsList
 * @name: the name of the new profile
 *
 * Adds a new child to the list, and returns a reference to its #GSettings.
 *
 * Returns: (transfer full): the UUID of new child
 */
char *
terminal_settings_list_add_child (TerminalSettingsList *list,
                                  const char *name)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);

  return terminal_settings_list_add_child_internal (list, nullptr, name);
}

/**
 * terminal_settings_list_clone_child:
 * @list: a #TerminalSettingsList
 * @uuid: the UUID of the child to clone
 * @name: the name of the new child
 *
 * Adds a new child to the list, and returns a reference to its #GSettings.
 * All keys of the new child will have the same value as @uuid's.
 *
 * Returns: (transfer full): the UUID of new child
 */
char *
terminal_settings_list_clone_child (TerminalSettingsList *list,
                                    const char *uuid,
                                    const char *name)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);
  g_return_val_if_fail (terminal_settings_list_valid_uuid (uuid), nullptr);

  return terminal_settings_list_add_child_internal (list, uuid, name);
}

/**
 * terminal_settings_list_remove_child:
 * @list: a #TerminalSettingsList
 * @uuid: the UUID of a list child
 *
 * Removes the child with UUID @uuid from the list.
 */
void
terminal_settings_list_remove_child (TerminalSettingsList *list,
                                     const char *uuid)
{
  g_return_if_fail (TERMINAL_IS_SETTINGS_LIST (list));
  g_return_if_fail (terminal_settings_list_valid_uuid (uuid));

  terminal_settings_list_remove_child_internal (list, uuid);
}

#endif /* TERMINAL_SERVER || TERMINAL_PREFERENCES */

/**
 * terminal_settings_list_dup_uuid_from_child:
 * @list: a #TerminalSettingsList
 * @child: a #GSettings of a child in the list
 *
 * Returns the UUID of @child in the list, or %nullptr if @child is not in the list.
 *
 * Returns: (transfer full): the UUID of the child in the settings list, or %nullptr
 */
char *
terminal_settings_list_dup_uuid_from_child (TerminalSettingsList *list,
                                            GSettings *child)
{
  gs_free char *path;
  char *p;

  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), nullptr);

  g_object_get (child, "path", &path, nullptr);
  g_return_val_if_fail (g_str_has_prefix (path, list->path), nullptr);

  p = path + strlen (list->path);
  g_return_val_if_fail (p[0] == ':', nullptr);
  p++;
  g_return_val_if_fail (strlen (p) == 37, nullptr);
  g_return_val_if_fail (p[36] == '/', nullptr);
  p[36] = '\0';
  g_assert (terminal_settings_list_valid_uuid (p));

  return g_strdup (p);
}

/**
 * terminal_settings_list_get_set_default_child:
 * @list: a #TerminalSettingsList
 * @uuid: the UUID of a child in the list
 *
 * Sets @uuid as the default child.
 */
void
terminal_settings_list_set_default_child (TerminalSettingsList *list,
                                          const char *uuid)
{
  g_return_if_fail (TERMINAL_IS_SETTINGS_LIST (list));
  g_return_if_fail (terminal_settings_list_valid_uuid (uuid));

  if (!terminal_settings_list_has_child (list, uuid))
    return;

  g_settings_set_string (&list->parent, TERMINAL_SETTINGS_LIST_DEFAULT_KEY, uuid);
}

/**
 * terminal_settings_list_foreach_child:
 * @list: a #TerminalSettingsList
 * @callback: a #TerminalSettingsListForeachFunc
 * @user_data: user data for @callback
 *
 * Calls @callback for each child of @list.
 *
 * NOTE: No changes to @list must be made from @callback.
 */
void
terminal_settings_list_foreach_child (TerminalSettingsList *list,
                                      TerminalSettingsListForeachFunc callback,
                                      gpointer user_data)
{
  g_return_if_fail (TERMINAL_IS_SETTINGS_LIST (list));
  g_return_if_fail (callback);

  for (char **p = list->uuids; *p; p++) {
    const char *uuid = *p;
    gs_unref_object GSettings *child = terminal_settings_list_ref_child_internal (list, uuid);
    if (child != nullptr)
      callback (list, uuid, child, user_data);
  }
}

/**
 * terminal_settings_list_foreach_child:
 * @list: a #TerminalSettingsList
 *
 * Returns: the number of children of @list.
 */
guint
terminal_settings_list_get_n_children (TerminalSettingsList *list)
{
  g_return_val_if_fail (TERMINAL_IS_SETTINGS_LIST (list), 0);

  return g_hash_table_size (list->children);
}
