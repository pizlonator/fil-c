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

#include <cassert>

#include "terminal-debug.hh"
#include "terminal-libgsystem.hh"
#include "terminal-settings-bridge-backend.hh"
#include "terminal-settings-utils.hh"
#include "terminal-settings-bridge-generated.h"

#include <gio/gio.h>
#include <gio/gsettingsbackend.h>

struct _TerminalSettingsBridgeBackend {
  GSettingsBackend parent_instance;

  TerminalSettingsBridge* bridge;
  GCancellable* cancellable;

  GHashTable* cache;
};

struct _TerminalSettingsBridgeBackendClass {
  GSettingsBackendClass parent_class;
};

enum {
  PROP_SETTINGS_BRIDGE = 1,
};

#define PRIORITY (10000)

// _g_io_modules_ensure_extension_points_registered() is not public,
// so just ensure a type that does this call on registration, which
// all of the glib-internal settings backends do. However as an added
// complication, none of their get_type() functions are public. So
// instead we need to create an object using the public function and
// immediately delete it again.
// However, this *still* does not work; the
// g_null_settings_backend_new() call prints the warning about a non-
// registered extension point even though its get_type() function calls
// _g_io_modules_ensure_extension_points_registered() immediately before.
// Therefore we can only use this backend when creating a GSettings
// ourself, by explicitly passing it at that time.

G_DEFINE_TYPE_WITH_CODE(TerminalSettingsBridgeBackend,
                        terminal_settings_bridge_backend,
                        G_TYPE_SETTINGS_BACKEND,
                        // _g_io_modules_ensure_extension_points_registered();
                        // { gs_unref_object auto dummy = g_null_settings_backend_new(); }
                        //
                        // g_io_extension_point_implement(G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                        //                                g_define_type_id, "bridge", PRIORITY)
);

// Note that since D-Bus doesn't support maybe values, we use arrays
// with either zero or one item to send/receive a maybe.
// If we get more than one item, just use the first one.

/* helper functions */

template<typename T>
inline constexpr auto
IMPL(T* that) noexcept
{
  return reinterpret_cast<TerminalSettingsBridgeBackend*>(that);
}

typedef struct {
  GVariant* value;
  bool value_set;
  bool writable;
  bool writable_set;
} CacheEntry;

static auto
cache_entry_new(void)
{
  return g_new0(CacheEntry, 1);
}

static void
cache_entry_free(CacheEntry* e) noexcept
{
  if (e->value)
    g_variant_unref(e->value);
  g_free(e);
}

static auto
cache_lookup_entry(TerminalSettingsBridgeBackend* impl,
                   char const* key) noexcept
{
  return reinterpret_cast<CacheEntry*>(g_hash_table_lookup(impl->cache, key));
}

static auto
cache_ensure_entry(TerminalSettingsBridgeBackend* impl,
                   char const* key) noexcept
{
  g_hash_table_insert(impl->cache, g_strdup(key), cache_entry_new());
  return cache_lookup_entry(impl, key);
}

static void
cache_insert_value(TerminalSettingsBridgeBackend* impl,
                   char const* key,
                   GVariant* value) noexcept
{
  auto const ce = cache_ensure_entry(impl, key);
  g_clear_pointer(&ce->value, g_variant_unref);
  ce->value = value ? g_variant_ref(value) : nullptr;
  ce->value_set = true;
}

static void
cache_insert_writable(TerminalSettingsBridgeBackend* impl,
                      char const* key,
                      bool writable) noexcept
{
  auto const ce = cache_ensure_entry(impl, key);
  ce->writable = writable;
  ce->writable_set = true;
}

static void
cache_remove_path(TerminalSettingsBridgeBackend* impl,
                  char const* path) noexcept
{
  auto iter = GHashTableIter{};
  g_hash_table_iter_init(&iter, impl->cache);
  void* keyp = nullptr;
  void* valuep = nullptr;
  while (g_hash_table_iter_next(&iter, &keyp, &valuep)) {
    auto const key = reinterpret_cast<char const*>(keyp);
    if (g_str_has_prefix(key, path)) {
      auto ce = reinterpret_cast<CacheEntry*>(valuep);
      g_clear_pointer(&ce->value, g_variant_unref);
      ce->value_set = false;
    }
  }
}

static void
cache_remove_value(TerminalSettingsBridgeBackend* impl,
                   char const* key) noexcept
{
  auto const ce = cache_ensure_entry(impl, key);
  g_clear_pointer(&ce->value, g_variant_unref);
  ce->value_set = false;
}

static void
cache_remove_writable(TerminalSettingsBridgeBackend* impl,
                      char const* key) noexcept
{
  auto const ce = cache_ensure_entry(impl, key);
  ce->writable_set = false;
}

/* GSettingsBackend class implementation */

static GPermission*
terminal_settings_bridge_backend_get_permission(GSettingsBackend* backend,
                                                char const* path) noexcept
{
  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::get_permission\n");

  return g_simple_permission_new(true);
}

static gboolean
terminal_settings_bridge_backend_get_writable(GSettingsBackend* backend,
                                              char const* key) noexcept
{
  auto const impl = IMPL(backend);

  auto const ce = cache_lookup_entry(impl, key);
  if (ce && ce->writable_set)
    return ce->writable;

  auto writable = gboolean{false};
  auto const r =
    terminal_settings_bridge_call_get_writable_sync(impl->bridge,
                                                    key,
                                                    &writable,
                                                    impl->cancellable,
                                                    nullptr);

  if (r)
    cache_insert_writable(impl, key, writable);
  else
    cache_remove_writable(impl, key);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::get_writable key %s success %d writable %d\n",
                        key, r, writable);

  return writable;
}

static GVariant*
terminal_settings_bridge_backend_read(GSettingsBackend* backend,
                                      char const* key,
                                      GVariantType const* type,
                                      gboolean default_value) noexcept
{
  if (default_value)
    return nullptr;

  auto const impl = IMPL(backend);
  auto const ce = cache_lookup_entry(impl, key);
  if (ce && ce->value_set)
    return ce->value ? g_variant_ref(ce->value) : nullptr;

  gs_unref_variant GVariant* rv = nullptr;
  auto r =
    terminal_settings_bridge_call_read_sync(impl->bridge,
                                            key,
                                            g_variant_type_peek_string(type),
                                            default_value,
                                            &rv,
                                            impl->cancellable,
                                            nullptr);

  auto value = r ? terminal_g_variant_unwrap(rv) : nullptr;

  if (r && value && !g_variant_is_of_type(value, type)) {
    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                          "Bridge backend ::read key %s got type %s expected type %s\n",
                          key,
                          g_variant_get_type_string(value),
                          g_variant_type_peek_string(type));

    g_clear_pointer(&value, g_variant_unref);
    r = false;
  }

  if (r)
    cache_insert_value(impl, key, value);
  else
    cache_remove_value(impl, key);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::read key %s success %d value %s\n",
                        key, r, value ? g_variant_print(value, true) : "(null)");

  return value;
}

static GVariant*
terminal_settings_bridge_backend_read_user_value(GSettingsBackend* backend,
                                                 char const* key,
                                                 GVariantType const* type) noexcept
{
  auto const impl = IMPL(backend);

  auto const ce = cache_lookup_entry(impl, key);
  if (ce && ce->value_set)
    return ce->value ? g_variant_ref(ce->value) : nullptr;

  gs_unref_variant GVariant* rv = nullptr;
  auto r =
    terminal_settings_bridge_call_read_user_value_sync(impl->bridge,
                                                       key,
                                                       g_variant_type_peek_string(type),
                                                       &rv,
                                                       impl->cancellable,
                                                       nullptr);

  auto value = r ? terminal_g_variant_unwrap(rv) : nullptr;

  if (r && value && !g_variant_is_of_type(value, type)) {
    _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                          "Bridge backend ::read_user_value key %s got type %s expected type %s\n",
                          key,
                          g_variant_get_type_string(value),
                          g_variant_type_peek_string(type));

    g_clear_pointer(&value, g_variant_unref);
    r = false;
  }

  if (r)
    cache_insert_value(impl, key, value);
  else
    cache_remove_value(impl, key);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::read_user_value key %s success %d value %s\n",
                        key, r, value ? g_variant_print(value, true) : "(null)");

  return value;
}

static void
terminal_settings_bridge_backend_reset(GSettingsBackend* backend,
                                       char const* key,
                                       void* tag) noexcept
{
  auto const impl = IMPL(backend);
  auto const r =
    terminal_settings_bridge_call_reset_sync(impl->bridge,
                                             key,
                                             impl->cancellable,
                                             nullptr);

  cache_remove_value(impl, key);

  g_settings_backend_changed(backend, key, tag);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::reset key %s success %d\n",
                        key, r);
}

static void
terminal_settings_bridge_backend_sync(GSettingsBackend* backend) noexcept
{
  auto const impl = IMPL(backend);
  terminal_settings_bridge_call_sync_sync(impl->bridge,
                                          impl->cancellable,
                                          nullptr);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::sync\n");
}

static void
terminal_settings_bridge_backend_subscribe(GSettingsBackend* backend,
                                           char const* name) noexcept
{
  auto const impl = IMPL(backend);
  terminal_settings_bridge_call_subscribe_sync(impl->bridge,
                                               name,
                                               impl->cancellable,
                                               nullptr);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::subscribe name %s\n", name);
}

static void
terminal_settings_bridge_backend_unsubscribe(GSettingsBackend* backend,
                                             char const* name) noexcept
{
  auto const impl = IMPL(backend);
  terminal_settings_bridge_call_unsubscribe_sync(impl->bridge,
                                                 name,
                                                 impl->cancellable,
                                                 nullptr);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::unsubscribe name %s\n", name);
}

static gboolean
terminal_settings_bridge_backend_write(GSettingsBackend* backend,
                                       char const* key,
                                       GVariant* value,
                                       void* tag) noexcept
{
  auto const impl = IMPL(backend);

  gs_unref_variant auto holder = g_variant_ref_sink(value);

  auto success = gboolean{false};
  auto const r =
    terminal_settings_bridge_call_write_sync(impl->bridge,
                                             key,
                                             terminal_g_variant_wrap(value),
                                             &success,
                                             impl->cancellable,
                                             nullptr);

  cache_insert_value(impl, key, value);

  g_settings_backend_changed(backend, key, tag);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::write key %s value %s success %d\n",
                        key, value ? g_variant_print(value, true) : "(null)", r);

  return r && success;
}

static gboolean
terminal_settings_bridge_backend_write_tree(GSettingsBackend* backend,
                                            GTree* tree,
                                            void* tag) noexcept
{
  auto const impl = IMPL(backend);

  gs_free char* path_prefix = nullptr;
  gs_free char const** keys = nullptr;
  gs_free GVariant** values = nullptr;
  g_settings_backend_flatten_tree(tree,
                                  &path_prefix,
                                  &keys,
                                  &values);

  auto builder = GVariantBuilder{};
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a(smv)"));
  for (auto i = 0; keys[i]; ++i) {
    gs_unref_variant auto value = values[i] ? g_variant_ref_sink(values[i]) : nullptr;

    g_variant_builder_add(&builder,
                          "(smv)",
                          keys[i],
                          value ? g_variant_new_variant(value) : nullptr);

    gs_free auto wkey = g_strconcat(path_prefix, keys[i], nullptr);
    // Directory reset?
    if (g_str_has_suffix(wkey, "/")) {
      g_warn_if_fail(!value);
      cache_remove_path(impl, wkey);
    } else {
      cache_insert_value(impl, wkey, value);
    }
  }

  auto const tree_value = terminal_g_variant_wrap(g_variant_builder_end(&builder));

  auto success = gboolean{false};
  auto const r =
    terminal_settings_bridge_call_write_tree_sync(impl->bridge,
                                                  path_prefix,
                                                  tree_value, // consumed
                                                  &success,
                                                  impl->cancellable,
                                                  nullptr);

  g_settings_backend_changed_tree(backend, tree, tag);

  _terminal_debug_print(TERMINAL_DEBUG_BRIDGE,
                        "Bridge backend ::write_tree success %d\n",
                        r);

  return r && success;
}

/* GObject class implementation */

static void
terminal_settings_bridge_backend_init(TerminalSettingsBridgeBackend* backend) /* noexcept */
{
  auto const impl = IMPL(backend);

  // Note that unfortunately it appears to be impossible to receive all
  // change notifications from a GSettingsBackend directly, so we cannot
  // get forwarded change notifications from the bridge. Instead, we have
  // to cache written values (since the actual write happens delayed in
  // the remote backend and the next read may still return the old value
  // otherwise).
  impl->cache = g_hash_table_new_full(g_str_hash,
                                      g_str_equal,
                                      g_free,
                                      GDestroyNotify(cache_entry_free));
}

static void
terminal_settings_bridge_backend_constructed(GObject* object) noexcept
{
  G_OBJECT_CLASS(terminal_settings_bridge_backend_parent_class)->constructed(object);

  auto const impl = IMPL(object);
  assert(impl->bridge);
}

static void
terminal_settings_bridge_backend_finalize(GObject* object) noexcept
{
  auto const impl = IMPL(object);
  g_clear_pointer(&impl->cache, g_hash_table_unref);
  g_clear_object(&impl->cancellable);
  g_clear_object(&impl->bridge);

  G_OBJECT_CLASS(terminal_settings_bridge_backend_parent_class)->finalize(object);
}

static void
terminal_settings_bridge_backend_set_property(GObject* object,
                                              guint prop_id,
                                              GValue const* value,
                                              GParamSpec* pspec) noexcept
{
  auto const impl = IMPL(object);

  switch (prop_id) {
  case PROP_SETTINGS_BRIDGE:
    impl->bridge = TERMINAL_SETTINGS_BRIDGE(g_value_dup_object(value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static void
terminal_settings_bridge_backend_class_init(TerminalSettingsBridgeBackendClass* klass) /* noexcept */
{
  auto const gobject_class = G_OBJECT_CLASS(klass);
  gobject_class->constructed = terminal_settings_bridge_backend_constructed;
  gobject_class->finalize = terminal_settings_bridge_backend_finalize;
  gobject_class->set_property = terminal_settings_bridge_backend_set_property;

  g_object_class_install_property
    (gobject_class,
     PROP_SETTINGS_BRIDGE,
     g_param_spec_object("settings-bridge", nullptr, nullptr,
                         TERMINAL_TYPE_SETTINGS_BRIDGE,
                         GParamFlags(G_PARAM_WRITABLE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS)));

  auto const backend_class = G_SETTINGS_BACKEND_CLASS(klass);
  backend_class->get_permission = terminal_settings_bridge_backend_get_permission;
  backend_class->get_writable = terminal_settings_bridge_backend_get_writable;
  backend_class->read = terminal_settings_bridge_backend_read;
  backend_class->read_user_value = terminal_settings_bridge_backend_read_user_value;
  backend_class->reset = terminal_settings_bridge_backend_reset;
  backend_class->subscribe = terminal_settings_bridge_backend_subscribe;
  backend_class->sync = terminal_settings_bridge_backend_sync;
  backend_class->unsubscribe = terminal_settings_bridge_backend_unsubscribe;
  backend_class->write = terminal_settings_bridge_backend_write;
  backend_class->write_tree = terminal_settings_bridge_backend_write_tree;
}

/* public API */

/**
 *  terminal_settings_bridge_backend_new:
 *  @bridge: a #TerminalSettingsBridge
 *
 *  Returns: (transfer full): a new #TerminalSettingsBridgeBackend for @bridge
 */
GSettingsBackend*
terminal_settings_bridge_backend_new(TerminalSettingsBridge* bridge)
{
  return reinterpret_cast<GSettingsBackend*>
    (g_object_new (TERMINAL_TYPE_SETTINGS_BRIDGE_BACKEND,
		   "settings-bridge", bridge,
		   nullptr));
}
