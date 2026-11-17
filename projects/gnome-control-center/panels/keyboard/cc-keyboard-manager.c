/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 */

#include <glib/gi18n.h>

#include "cc-keyboard-manager.h"
#include "keyboard-shortcuts.h"

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#include <X11/Xatom.h>
#endif

#define BINDINGS_SCHEMA       "org.gnome.settings-daemon.plugins.media-keys"
#define CUSTOM_SHORTCUTS_ID   "custom"

struct _CcKeyboardManager
{
  GObject             parent;

  GtkListStore       *sections_store;

  GHashTable         *kb_system_sections;
  GHashTable         *kb_apps_sections;
  GHashTable         *kb_user_sections;

  GSettings          *binding_settings;
};

G_DEFINE_TYPE (CcKeyboardManager, cc_keyboard_manager, G_TYPE_OBJECT)

enum
{
  SHORTCUT_ADDED,
  SHORTCUT_CHANGED,
  SHORTCUT_REMOVED,
  SHORTCUTS_LOADED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

/*
 * Auxiliary methods
 */
static void
free_key_array (GPtrArray *keys)
{
  if (keys != NULL)
    {
      gint i;

      for (i = 0; i < keys->len; i++)
        {
          CcKeyboardItem *item;

          item = g_ptr_array_index (keys, i);

          g_object_unref (item);
        }

      g_ptr_array_free (keys, TRUE);
    }
}

static gboolean
find_conflict (CcUniquenessData *data,
               CcKeyboardItem   *item)
{
  GList *l;
  gboolean is_conflict = FALSE;

  if (data->orig_item && cc_keyboard_item_equal (data->orig_item, item))
    return FALSE;

  for (l = cc_keyboard_item_get_key_combos (item); l; l = l->next)
    {
      CcKeyCombo *combo = l->data;

      if (data->new_mask != combo->mask)
        continue;

      if (data->new_keyval != 0)
        is_conflict = data->new_keyval == combo->keyval;
      else
        is_conflict = combo->keyval == 0 && data->new_keycode == combo->keycode;

      if (is_conflict)
        break;
    }

  if (is_conflict)
    data->conflict_item = item;

  return is_conflict;
}

static gboolean
compare_keys_for_uniqueness (CcKeyboardItem   *current_item,
                             CcUniquenessData *data)
{
  CcKeyboardItem *reverse_item;

  /* No conflict for: blanks or ourselves */
  if (!current_item || data->orig_item == current_item)
    return FALSE;

  reverse_item = cc_keyboard_item_get_reverse_item (current_item);

  /* When the current item is the reversed shortcut of a main item, simply ignore it */
  if (reverse_item && cc_keyboard_item_is_hidden (current_item))
    return FALSE;

  if (find_conflict (data, current_item))
    return TRUE;

  /* Also check for the reverse item if any */
  if (reverse_item && find_conflict (data, reverse_item))
    return TRUE;

  return FALSE;
}

static gboolean
check_for_uniqueness (gpointer          key,
                      GPtrArray        *keys_array,
                      CcUniquenessData *data)
{
  guint i;

  for (i = 0; i < keys_array->len; i++)
    {
      CcKeyboardItem *item;

      item = keys_array->pdata[i];

      if (compare_keys_for_uniqueness (item, data))
        return TRUE;
    }

  return FALSE;
}


static GHashTable*
get_hash_for_group (CcKeyboardManager *self,
                    BindingGroupType   group)
{
  GHashTable *hash;

  switch (group)
    {
    case BINDING_GROUP_SYSTEM:
      hash = self->kb_system_sections;
      break;
    case BINDING_GROUP_APPS:
      hash = self->kb_apps_sections;
      break;
    case BINDING_GROUP_USER:
      hash = self->kb_user_sections;
      break;
    default:
      hash = NULL;
    }

  return hash;
}

static gboolean
have_key_for_group (CcKeyboardManager *self,
                    int                group,
                    const gchar       *name)
{
  GHashTableIter iter;
  GPtrArray *keys;
  gint i;

  g_hash_table_iter_init (&iter, get_hash_for_group (self, group));
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &keys))
    {
      for (i = 0; i < keys->len; i++)
        {
          CcKeyboardItem *item = g_ptr_array_index (keys, i);

          if (cc_keyboard_item_get_item_type (item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS &&
              g_strcmp0 (name, cc_keyboard_item_get_key (item)) == 0)
            {
              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
add_shortcuts (CcKeyboardManager *self)
{
  GtkTreeModel *sections_model;
  GtkTreeIter sections_iter;
  gboolean can_continue;

  sections_model = GTK_TREE_MODEL (self->sections_store);
  can_continue = gtk_tree_model_get_iter_first (sections_model, &sections_iter);

  while (can_continue)
    {
      BindingGroupType group;
      GPtrArray *keys;
      g_autofree gchar *id = NULL;
      g_autofree gchar *title = NULL;
      gint i;

      gtk_tree_model_get (sections_model,
                          &sections_iter,
                          SECTION_DESCRIPTION_COLUMN, &title,
                          SECTION_GROUP_COLUMN, &group,
                          SECTION_ID_COLUMN, &id,
                          -1);

      /* Ignore separators */
      if (group == BINDING_GROUP_SEPARATOR)
        {
          can_continue = gtk_tree_model_iter_next (sections_model, &sections_iter);
          continue;
        }

      keys = g_hash_table_lookup (get_hash_for_group (self, group), id);

      for (i = 0; i < keys->len; i++)
        {
          CcKeyboardItem *item = g_ptr_array_index (keys, i);

          if (!cc_keyboard_item_is_hidden (item))
            {
              g_signal_emit (self, signals[SHORTCUT_ADDED],
                             0,
                             item,
                             id,
                             title);
            }
        }

      can_continue = gtk_tree_model_iter_next (sections_model, &sections_iter);
    }

  g_signal_emit (self, signals[SHORTCUTS_LOADED], 0);
}

static void
append_section (CcKeyboardManager  *self,
                const gchar        *title,
                const gchar        *id,
                BindingGroupType    group,
                const KeyListEntry *keys_list)
{
  GtkTreeIter iter;
  GHashTable *reverse_items;
  GHashTable *hash;
  GPtrArray *keys_array;
  gboolean is_new;
  gint i;

  hash = get_hash_for_group (self, group);

  if (!hash)
    return;

  /* Add all CcKeyboardItems for this section */
  is_new = FALSE;
  keys_array = g_hash_table_lookup (hash, id);
  if (keys_array == NULL)
    {
      keys_array = g_ptr_array_new ();
      is_new = TRUE;
    }

  reverse_items = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; keys_list != NULL && keys_list[i].name != NULL; i++)
    {
      CcKeyboardItem *item;
      gboolean ret;

      if (have_key_for_group (self, group, keys_list[i].name))
        continue;

      item = cc_keyboard_item_new (keys_list[i].type);

      switch (keys_list[i].type)
        {
        case CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH:
          ret = cc_keyboard_item_load_from_gsettings_path (item, keys_list[i].name, FALSE);
          break;

        case CC_KEYBOARD_ITEM_TYPE_GSETTINGS:
          ret = cc_keyboard_item_load_from_gsettings (item,
                                                      keys_list[i].description,
                                                      keys_list[i].schema,
                                                      keys_list[i].name);
          if (ret && keys_list[i].reverse_entry != NULL)
            {
              CcKeyboardItem *reverse_item;
              reverse_item = g_hash_table_lookup (reverse_items,
                                                  keys_list[i].reverse_entry);
              if (reverse_item != NULL)
                {
                  cc_keyboard_item_add_reverse_item (item,
                                                     reverse_item,
                                                     keys_list[i].is_reversed);
                }
              else
                {
                  g_hash_table_insert (reverse_items,
                                       keys_list[i].name,
                                       item);
                }
            }
          break;

        default:
          g_assert_not_reached ();
        }

      if (ret == FALSE)
        {
          /* We don't actually want to popup a dialog - just skip this one */
          g_object_unref (item);
          continue;
        }

      cc_keyboard_item_set_hidden (item, keys_list[i].hidden);

      g_ptr_array_add (keys_array, item);
    }

  g_hash_table_destroy (reverse_items);

  /* Add the keys to the hash table */
  if (is_new)
    {
      g_hash_table_insert (hash, g_strdup (id), keys_array);

      /* Append the section to the left tree view */
      gtk_list_store_append (GTK_LIST_STORE (self->sections_store), &iter);
      gtk_list_store_set (GTK_LIST_STORE (self->sections_store),
                          &iter,
                          SECTION_DESCRIPTION_COLUMN, title,
                          SECTION_ID_COLUMN, id,
                          SECTION_GROUP_COLUMN, group,
                          -1);
    }
}

static void
append_sections_from_file (CcKeyboardManager  *self,
                           const gchar        *path,
                           const char         *datadir,
                           gchar             **wm_keybindings)
{
  KeyList *keylist;
  KeyListEntry *keys;
  KeyListEntry key = { 0, 0, 0, 0, 0, 0, 0 };
  const char *title;
  int group;
  guint i;

  keylist = parse_keylist_from_file (path);

  if (keylist == NULL)
    return;

#define const_strv(s) ((const gchar* const*) s)

  /* If there's no keys to add, or the settings apply to a window manager
   * that's not the one we're running */
  if (keylist->entries->len == 0 ||
      (keylist->wm_name != NULL && !g_strv_contains (const_strv (wm_keybindings), keylist->wm_name)) ||
      keylist->name == NULL)
    {
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return;
    }

#undef const_strv

  /* Empty KeyListEntry to end the array */
  key.name = NULL;
  g_array_append_val (keylist->entries, key);

  keys = (KeyListEntry *) g_array_free (keylist->entries, FALSE);
  if (keylist->package)
    {
      g_autofree gchar *localedir = NULL;

      localedir = g_build_filename (datadir, "locale", NULL);
      bindtextdomain (keylist->package, localedir);

      title = dgettext (keylist->package, keylist->name);
    } else {
      title = _(keylist->name);
    }

  if (keylist->group && strcmp (keylist->group, "system") == 0)
    group = BINDING_GROUP_SYSTEM;
  else
    group = BINDING_GROUP_APPS;

  append_section (self, title, keylist->name, group, keys);

  g_free (keylist->name);
  g_free (keylist->package);
  g_free (keylist->wm_name);
  g_free (keylist->schema);
  g_free (keylist->group);

  for (i = 0; keys[i].name != NULL; i++)
    {
      KeyListEntry *entry = &keys[i];
      g_free (entry->schema);
      g_free (entry->description);
      g_free (entry->name);
      g_free (entry->reverse_entry);
    }

  g_free (keylist);
  g_free (keys);
}

static void
append_sections_from_gsettings (CcKeyboardManager *self)
{
  g_auto(GStrv) custom_paths = NULL;
  GArray *entries;
  KeyListEntry key = { 0, 0, 0, 0, 0, 0, 0 };
  int i;

  /* load custom shortcuts from GSettings */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  custom_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");
  for (i = 0; custom_paths[i]; i++)
    {
      key.name = g_strdup (custom_paths[i]);
      if (!have_key_for_group (self, BINDING_GROUP_USER, key.name))
        {
          key.type = CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH;
          g_array_append_val (entries, key);
        }
      else
        g_free (key.name);
    }

  if (entries->len > 0)
    {
      KeyListEntry *keys;
      int i;

      /* Empty KeyListEntry to end the array */
      key.name = NULL;
      g_array_append_val (entries, key);

      keys = (KeyListEntry *) entries->data;
      append_section (self, _("Custom Shortcuts"), CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
        }
    }
  else
    {
      append_section (self, _("Custom Shortcuts"), CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, NULL);
    }

  g_array_free (entries, TRUE);
}

#ifdef GDK_WINDOWING_X11
static char *
get_window_manager_property (GdkDisplay *display,
                             Atom        atom,
                             Window      window)
{
  Display *xdisplay;
  Atom utf8_string;
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  char *value;

  if (window == None)
    return NULL;

  xdisplay = gdk_x11_display_get_xdisplay (display);
  utf8_string = XInternAtom (xdisplay, "UTF8_STRING", False);

  gdk_x11_display_error_trap_push (display);

  result = XGetWindowProperty (xdisplay,
                               window,
                               atom,
                               0,
                               G_MAXLONG,
                               False,
                               utf8_string,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (result != Success ||
      actual_type != utf8_string ||
      actual_format != 8 ||
      n_items == 0)
    {
      XFree (prop);
      return NULL;
    }

  value = g_strndup ((const char *) prop, n_items);
  XFree (prop);

  if (!g_utf8_validate (value, -1, NULL))
    {
      g_free (value);
      return NULL;
    }

  return value;
}

static Window
get_wm_window (GdkDisplay *display)
{
  Display *xdisplay;
  Atom wm_check;
  int result;
  Atom actual_type;
  int actual_format;
  unsigned long n_items;
  unsigned long bytes_after;
  unsigned char *prop;
  Window wm_window;

  xdisplay = gdk_x11_display_get_xdisplay (display);
  wm_check = XInternAtom (xdisplay, "_NET_SUPPORTING_WM_CHECK", False);

  gdk_x11_display_error_trap_push (display);

  result = XGetWindowProperty (xdisplay,
                               XDefaultRootWindow (xdisplay),
                               wm_check,
                               0,
                               G_MAXLONG,
                               False,
                               XA_WINDOW,
                               &actual_type,
                               &actual_format,
                               &n_items,
                               &bytes_after,
                               &prop);

  gdk_x11_display_error_trap_pop_ignored (display);

  if (result != Success ||
      actual_type != XA_WINDOW ||
      n_items == 0)
    {
      XFree (prop);
      return None;
    }

  wm_window = *(Window *) prop;
  XFree (prop);

  return wm_window;
}
#endif

static GStrv
get_current_keybindings (void)
{
#ifdef GDK_WINDOWING_X11
  GdkDisplay *display;
  Display *xdisplay;
  Atom keybindings_atom;
  Window wm_window;
  char *keybindings;
  GStrv results;

  display = gdk_display_get_default ();
  if (!GDK_IS_X11_DISPLAY (display))
    return NULL;

  xdisplay = gdk_x11_display_get_xdisplay (display);
  keybindings_atom = XInternAtom (xdisplay, "_GNOME_WM_KEYBINDINGS", False);

  wm_window = get_wm_window (display);
  keybindings = get_window_manager_property (display,
                                             keybindings_atom,
                                             wm_window);

  if (keybindings != NULL)
    {
      GStrv p;

      results = g_strsplit (keybindings, ",", -1);

      for (p = results; p && *p; p++)
        g_strstrip (*p);

      g_free (keybindings);
    }
  else
    {
      Atom wm_atom;
      char *wm_name;

      wm_atom = XInternAtom (xdisplay, "_NET_WM_NAME", False);
      wm_name = get_window_manager_property (display, wm_atom, wm_window);

      results = g_new0 (char *, 2);
      results[0] = wm_name ? wm_name : g_strdup ("Unknown");
    }

  return results;
#else
  return NULL;
#endif
}

static void
reload_sections (CcKeyboardManager *self)
{
  GHashTable *loaded_files;
  GDir *dir;
  gchar *default_wm_keybindings[] = { "Mutter", "GNOME Shell", NULL };
  g_auto(GStrv) wm_keybindings = NULL;
  const gchar * const * data_dirs;
  guint i;

  /* Clear previous models and hash tables */
  gtk_list_store_clear (GTK_LIST_STORE (self->sections_store));

  g_clear_pointer (&self->kb_system_sections, g_hash_table_destroy);
  self->kb_system_sections = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    (GDestroyNotify) free_key_array);

  g_clear_pointer (&self->kb_apps_sections, g_hash_table_destroy);
  self->kb_apps_sections = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) free_key_array);

  g_clear_pointer (&self->kb_user_sections, g_hash_table_destroy);
  self->kb_user_sections = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) free_key_array);

  /* Load WM keybindings */
  wm_keybindings = get_current_keybindings ();

  if (wm_keybindings == NULL)
    wm_keybindings = g_strdupv (default_wm_keybindings);

  loaded_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  data_dirs = g_get_system_data_dirs ();
  for (i = 0; data_dirs[i] != NULL; i++)
    {
      g_autofree gchar *dir_path = NULL;
      const gchar *name;

      dir_path = g_build_filename (data_dirs[i], "gnome-control-center", "keybindings", NULL);

      dir = g_dir_open (dir_path, 0, NULL);
      if (!dir)
        continue;

      for (name = g_dir_read_name (dir) ; name ; name = g_dir_read_name (dir))
        {
          g_autofree gchar *path = NULL;

          if (g_str_has_suffix (name, ".xml") == FALSE)
            continue;

          if (g_hash_table_lookup (loaded_files, name) != NULL)
            {
              g_debug ("Not loading %s, it was already loaded from another directory", name);
              continue;
            }

          g_hash_table_insert (loaded_files, g_strdup (name), GINT_TO_POINTER (1));
          path = g_build_filename (dir_path, name, NULL);
          append_sections_from_file (self, path, data_dirs[i], wm_keybindings);
        }

      g_dir_close (dir);
    }

  g_hash_table_destroy (loaded_files);

  /* Load custom keybindings */
  append_sections_from_gsettings (self);
}

/*
 * Callbacks
 */
static void
cc_keyboard_manager_finalize (GObject *object)
{
  CcKeyboardManager *self = (CcKeyboardManager *)object;

  g_clear_pointer (&self->kb_system_sections, g_hash_table_destroy);
  g_clear_pointer (&self->kb_apps_sections, g_hash_table_destroy);
  g_clear_pointer (&self->kb_user_sections, g_hash_table_destroy);
  g_clear_object (&self->binding_settings);
  g_clear_object (&self->sections_store);

  G_OBJECT_CLASS (cc_keyboard_manager_parent_class)->finalize (object);
}

static void
cc_keyboard_manager_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
cc_keyboard_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
cc_keyboard_manager_class_init (CcKeyboardManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_keyboard_manager_finalize;
  object_class->get_property = cc_keyboard_manager_get_property;
  object_class->set_property = cc_keyboard_manager_set_property;

  /**
   * CcKeyboardManager:shortcut-added:
   *
   * Emitted when a shortcut is added.
   */
  signals[SHORTCUT_ADDED] = g_signal_new ("shortcut-added",
                                          CC_TYPE_KEYBOARD_MANAGER,
                                          G_SIGNAL_RUN_FIRST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          3,
                                          CC_TYPE_KEYBOARD_ITEM,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING);

  /**
   * CcKeyboardManager:shortcut-changed:
   *
   * Emitted when a shortcut is added.
   */
  signals[SHORTCUT_CHANGED] = g_signal_new ("shortcut-changed",
                                            CC_TYPE_KEYBOARD_MANAGER,
                                            G_SIGNAL_RUN_FIRST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            1,
                                            CC_TYPE_KEYBOARD_ITEM);


  /**
   * CcKeyboardManager:shortcut-removed:
   *
   * Emitted when a shortcut is removed.
   */
  signals[SHORTCUT_REMOVED] = g_signal_new ("shortcut-removed",
                                            CC_TYPE_KEYBOARD_MANAGER,
                                            G_SIGNAL_RUN_FIRST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE,
                                            1,
                                            CC_TYPE_KEYBOARD_ITEM);

  /**
   * CcKeyboardManager:shortcuts-loaded:
   *
   * Emitted after all shortcuts are loaded.
   */
  signals[SHORTCUTS_LOADED] = g_signal_new ("shortcuts-loaded",
                                            CC_TYPE_KEYBOARD_MANAGER,
                                            G_SIGNAL_RUN_FIRST,
                                            0, NULL, NULL, NULL,
                                            G_TYPE_NONE, 0);
}

static void
cc_keyboard_manager_init (CcKeyboardManager *self)
{
  /* Bindings */
  self->binding_settings = g_settings_new (BINDINGS_SCHEMA);

  /* Setup the section models */
  self->sections_store = gtk_list_store_new (SECTION_N_COLUMNS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_INT);
}


CcKeyboardManager *
cc_keyboard_manager_new (void)
{
  return g_object_new (CC_TYPE_KEYBOARD_MANAGER, NULL);
}

void
cc_keyboard_manager_load_shortcuts (CcKeyboardManager *self)
{
  g_return_if_fail (CC_IS_KEYBOARD_MANAGER (self));

  reload_sections (self);
  add_shortcuts (self);
}

/**
 * cc_keyboard_manager_create_custom_shortcut:
 * @self: a #CcKeyboardPanel
 *
 * Creates a new temporary keyboard shortcut.
 *
 * Returns: (transfer full): a #CcKeyboardItem
 */
CcKeyboardItem*
cc_keyboard_manager_create_custom_shortcut (CcKeyboardManager *self)
{
  CcKeyboardItem *item;
  g_autofree gchar *settings_path = NULL;

  g_return_val_if_fail (CC_IS_KEYBOARD_MANAGER (self), NULL);

  item = cc_keyboard_item_new (CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  settings_path = find_free_settings_path (self->binding_settings);
  cc_keyboard_item_load_from_gsettings_path (item, settings_path, TRUE);

  return item;
}

/**
 * cc_keyboard_manager_add_custom_shortcut:
 * @self: a #CcKeyboardPanel
 * @item: the #CcKeyboardItem to be added
 *
 * Effectively adds the custom shortcut.
 */
void
cc_keyboard_manager_add_custom_shortcut (CcKeyboardManager *self,
                                         CcKeyboardItem    *item)
{
  GPtrArray *keys_array;
  GHashTable *hash;
  GVariantBuilder builder;
  char **settings_paths;
  int i;

  g_return_if_fail (CC_IS_KEYBOARD_MANAGER (self));

  hash = get_hash_for_group (self, BINDING_GROUP_USER);
  keys_array = g_hash_table_lookup (hash, CUSTOM_SHORTCUTS_ID);

  if (keys_array == NULL)
    {
      keys_array = g_ptr_array_new ();
      g_hash_table_insert (hash, g_strdup (CUSTOM_SHORTCUTS_ID), keys_array);
    }

  g_ptr_array_add (keys_array, item);

  settings_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  for (i = 0; settings_paths[i]; i++)
    g_variant_builder_add (&builder, "s", settings_paths[i]);

  g_variant_builder_add (&builder, "s", cc_keyboard_item_get_gsettings_path (item));

  g_settings_set_value (self->binding_settings, "custom-keybindings", g_variant_builder_end (&builder));

  g_signal_emit (self, signals[SHORTCUT_ADDED],
                 0,
                 item,
                 CUSTOM_SHORTCUTS_ID,
                 _("Custom Shortcuts"));
}

/**
 * cc_keyboard_manager_remove_custom_shortcut:
 * @self: a #CcKeyboardPanel
 * @item: the #CcKeyboardItem to be added
 *
 * Removed the custom shortcut.
 */
void
cc_keyboard_manager_remove_custom_shortcut  (CcKeyboardManager *self,
                                             CcKeyboardItem    *item)
{
  GPtrArray *keys_array;
  GVariantBuilder builder;
  GSettings *settings;
  char **settings_paths;
  int i;

  g_return_if_fail (CC_IS_KEYBOARD_MANAGER (self));

  /* Shortcut not a custom shortcut */
  g_assert (cc_keyboard_item_get_item_type (item) == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  settings = cc_keyboard_item_get_settings (item);
  g_settings_delay (settings);
  g_settings_reset (settings, "name");
  g_settings_reset (settings, "command");
  g_settings_reset (settings, "binding");
  g_settings_apply (settings);
  g_settings_sync ();

  settings_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  for (i = 0; settings_paths[i]; i++)
    if (strcmp (settings_paths[i], cc_keyboard_item_get_gsettings_path (item)) != 0)
      g_variant_builder_add (&builder, "s", settings_paths[i]);

  g_settings_set_value (self->binding_settings,
                        "custom-keybindings",
                        g_variant_builder_end (&builder));

  g_strfreev (settings_paths);

  keys_array = g_hash_table_lookup (get_hash_for_group (self, BINDING_GROUP_USER), CUSTOM_SHORTCUTS_ID);
  g_ptr_array_remove (keys_array, item);

  g_signal_emit (self, signals[SHORTCUT_REMOVED], 0, item);
}

/**
 * cc_keyboard_manager_get_collision:
 * @self: a #CcKeyboardManager
 * @item: (nullable): a keyboard shortcut
 * @combo: a #CcKeyCombo
 *
 * Retrieves the collision item for the given shortcut.
 *
 * Returns: (transfer none)(nullable): the collisioned shortcut
 */
CcKeyboardItem*
cc_keyboard_manager_get_collision (CcKeyboardManager *self,
                                   CcKeyboardItem    *item,
                                   CcKeyCombo        *combo)
{
  CcUniquenessData data;
  BindingGroupType i;

  g_return_val_if_fail (CC_IS_KEYBOARD_MANAGER (self), NULL);

  data.orig_item = item;
  data.new_keyval = combo->keyval;
  data.new_mask = combo->mask;
  data.new_keycode = combo->keycode;
  data.conflict_item = NULL;

  if (combo->keyval == 0 && combo->keycode == 0)
    return NULL;

  /* Any number of shortcuts can be disabled */
  for (i = BINDING_GROUP_SYSTEM; i <= BINDING_GROUP_USER && !data.conflict_item; i++)
    {
      GHashTable *table;

      table = get_hash_for_group (self, i);

      if (!table)
        continue;

      g_hash_table_find (table, (GHRFunc) check_for_uniqueness, &data);
    }

  return data.conflict_item;
}

/**
 * cc_keyboard_manager_reset_shortcut:
 * @self: a #CcKeyboardManager
 * @item: a #CcKeyboardItem
 *
 * Resets the keyboard shortcut managed by @item, and eventually
 * disables any shortcut that conflicts with the new shortcut's
 * value.
 */
void
cc_keyboard_manager_reset_shortcut (CcKeyboardManager *self,
                                    CcKeyboardItem    *item)
{
  GList *l;

  g_return_if_fail (CC_IS_KEYBOARD_MANAGER (self));
  g_return_if_fail (CC_IS_KEYBOARD_ITEM (item));

  /* Disables any shortcut that conflicts with the new shortcut's value */
  for (l = cc_keyboard_item_get_default_combos (item); l; l = l->next)
    {
      CcKeyCombo *combo = l->data;
      CcKeyboardItem *collision;

      collision = cc_keyboard_manager_get_collision (self, NULL, combo);
      if (collision)
        cc_keyboard_item_remove_key_combo (collision, combo);
    }

  /* Resets the current item */
  cc_keyboard_item_reset (item);
}
