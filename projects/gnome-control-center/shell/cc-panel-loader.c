/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#include "cc-panel.h"
#include "cc-panel-loader.h"

#ifndef CC_PANEL_LOADER_NO_GTYPES

/* Extension points */
extern GType cc_applications_panel_get_type (void);
extern GType cc_background_panel_get_type (void);
#ifdef BUILD_BLUETOOTH
extern GType cc_bluetooth_panel_get_type (void);
#endif /* BUILD_BLUETOOTH */
extern GType cc_color_panel_get_type (void);
extern GType cc_date_time_panel_get_type (void);
extern GType cc_display_panel_get_type (void);
extern GType cc_keyboard_panel_get_type (void);
extern GType cc_mouse_panel_get_type (void);
extern GType cc_multitasking_panel_get_type (void);
#ifdef BUILD_NETWORK
extern GType cc_network_panel_get_type (void);
extern GType cc_wifi_panel_get_type (void);
#endif /* BUILD_NETWORK */
extern GType cc_notifications_panel_get_type (void);
extern GType cc_online_accounts_panel_get_type (void);
extern GType cc_power_panel_get_type (void);
extern GType cc_printers_panel_get_type (void);
extern GType cc_privacy_panel_get_type (void);
extern GType cc_search_panel_get_type (void);
extern GType cc_sharing_panel_get_type (void);
extern GType cc_sound_panel_get_type (void);
extern GType cc_system_panel_get_type (void);
extern GType cc_ua_panel_get_type (void);
#ifdef BUILD_WACOM
extern GType cc_wacom_panel_get_type (void);
#endif /* BUILD_WACOM */
#ifdef BUILD_WWAN
extern GType cc_wwan_panel_get_type (void);
#endif /* BUILD_WWAN */

/* Static init functions */
#ifdef BUILD_NETWORK
extern void cc_wifi_panel_static_init_func (void);
#endif /* BUILD_NETWORK */
extern void cc_sharing_panel_static_init_func (void);
#ifdef BUILD_WACOM
extern void cc_wacom_panel_static_init_func (void);
#endif /* BUILD_WACOM */
#ifdef BUILD_WWAN
extern void cc_wwan_panel_static_init_func (void);
#endif /* BUILD_WWAN */

#define PANEL_TYPE(name, get_type, init_func) { name, get_type, init_func }

#else /* CC_PANEL_LOADER_NO_GTYPES */

#define PANEL_TYPE(name, get_type, init_func) { name }

#endif

static CcPanelLoaderVtable default_panels[] =
{
  PANEL_TYPE("applications",     cc_applications_panel_get_type,         NULL),
  PANEL_TYPE("background",       cc_background_panel_get_type,           NULL),
#ifdef BUILD_BLUETOOTH
  PANEL_TYPE("bluetooth",        cc_bluetooth_panel_get_type,            NULL),
#endif
#ifdef HAVE_COLORD_GTK
  PANEL_TYPE("color",            cc_color_panel_get_type,                NULL),
#endif /* HAVE_COLORD_GTK */
  PANEL_TYPE("display",          cc_display_panel_get_type,              NULL),
  PANEL_TYPE("keyboard",         cc_keyboard_panel_get_type,             NULL),
  PANEL_TYPE("mouse",            cc_mouse_panel_get_type,                NULL),
  PANEL_TYPE("multitasking",     cc_multitasking_panel_get_type,         NULL),
#ifdef BUILD_NETWORK
  PANEL_TYPE("network",          cc_network_panel_get_type,              NULL),
  PANEL_TYPE("wifi",             cc_wifi_panel_get_type,                 cc_wifi_panel_static_init_func),
#endif
  PANEL_TYPE("notifications",    cc_notifications_panel_get_type,        NULL),
#ifdef HAVE_GOA
  PANEL_TYPE("online-accounts",  cc_online_accounts_panel_get_type,      NULL),
#endif /* HAVE_GOA */
  PANEL_TYPE("power",            cc_power_panel_get_type,                NULL),
#ifdef HAVE_CUPS
  PANEL_TYPE("printers",         cc_printers_panel_get_type,             NULL),
#endif /* HAVE_CUPS */
#ifdef HAVE_LIBGTOP
  PANEL_TYPE("privacy",          cc_privacy_panel_get_type,              NULL),
#endif /* HAVE_LIBGTOP */
  PANEL_TYPE("search",           cc_search_panel_get_type,               NULL),
  PANEL_TYPE("sharing",          cc_sharing_panel_get_type,              cc_sharing_panel_static_init_func),
#ifdef HAVE_GSOUND
  PANEL_TYPE("sound",            cc_sound_panel_get_type,                NULL),
#endif /* HAVE_GSOUND */
#if defined(HAVE_LIBGTOP) && defined(HAVE_UDISKS2)
  PANEL_TYPE("system",           cc_system_panel_get_type,               NULL),
#endif /* HAVE_LIBGTOP && HAVE_UDISKS2 */
  PANEL_TYPE("universal-access", cc_ua_panel_get_type,                   NULL),
#ifdef BUILD_WACOM
  PANEL_TYPE("wacom",            cc_wacom_panel_get_type,                cc_wacom_panel_static_init_func),
#endif
#ifdef BUILD_WWAN
  PANEL_TYPE("wwan",             cc_wwan_panel_get_type,                 cc_wwan_panel_static_init_func),
#endif
};

/* Override for the panel vtable. When NULL, the default_panels will
 * be used.
 */
static CcPanelLoaderVtable *panels_vtable = default_panels;
static gsize panels_vtable_len = G_N_ELEMENTS (default_panels);

typedef struct
{
  CcPanelCategory category;
  const gchar *name;
} CcSubpageLoaderVtable;

static CcSubpageLoaderVtable default_subpages[] =
{
  {CC_CATEGORY_SYSTEM, "about"},
  {CC_CATEGORY_SYSTEM, "datetime"},
  {CC_CATEGORY_SYSTEM, "region"},
  {CC_CATEGORY_SYSTEM, "users"},
};
static CcSubpageLoaderVtable *subpages_vtable = default_subpages;
static gsize supages_vtable_len = G_N_ELEMENTS (default_subpages);

static int
parse_categories (GDesktopAppInfo *app)
{
  g_auto(GStrv) split = NULL;
  const gchar *categories;
  gint retval;

  categories = g_desktop_app_info_get_categories (app);
  split = g_strsplit (categories, ";", -1);

  retval = -1;

#define const_strv(s) ((const gchar* const*) s)

  if (g_strv_contains (const_strv (split), "X-GNOME-ConnectivitySettings"))
    retval = CC_CATEGORY_CONNECTIVITY;
  else if (g_strv_contains (const_strv (split), "X-GNOME-PersonalizationSettings"))
    retval = CC_CATEGORY_PERSONALIZATION;
  else if (g_strv_contains (const_strv (split), "X-GNOME-AccountSettings"))
    retval = CC_CATEGORY_ACCOUNT;
  else if (g_strv_contains (const_strv (split), "X-GNOME-DevicesSettings"))
    retval = CC_CATEGORY_DEVICES;
  else if (g_strv_contains (const_strv (split), "X-GNOME-DetailsSettings"))
    retval = CC_CATEGORY_DETAILS;
  else if (g_strv_contains (const_strv (split), "X-GNOME-SystemSettings"))
    retval = CC_CATEGORY_SYSTEM;
  else if (g_strv_contains (const_strv (split), "X-GNOME-PrivacySettings"))
    retval = CC_CATEGORY_PRIVACY;
  else if (g_strv_contains (const_strv (split), "HardwareSettings"))
    retval = CC_CATEGORY_HARDWARE;

#undef const_strv

  if (retval < 0)
    {
      g_warning ("Invalid categories %s for panel %s",
                 categories, g_app_info_get_id (G_APP_INFO (app)));
    }

  return retval;
}

#ifndef CC_PANEL_LOADER_NO_GTYPES

static GHashTable *panel_types;

static void
ensure_panel_types (void)
{
  int i;

  if (G_LIKELY (panel_types != NULL))
    return;

  panel_types = g_hash_table_new (g_str_hash, g_str_equal);
  for (i = 0; i < panels_vtable_len; i++)
    g_hash_table_insert (panel_types, (char*)panels_vtable[i].name, panels_vtable[i].get_type);
}

/**
 * cc_panel_loader_load_by_name:
 * @shell: a #CcShell implementation
 * @name: name of the panel
 * @parameters: parameters passed to the new panel
 *
 * Creates a new instance of a #CcPanel from @name, and sets the
 * @shell and @parameters properties at construction time.
 */
CcPanel *
cc_panel_loader_load_by_name (CcShell     *shell,
                              const gchar *name,
                              const gchar *title,
                              GVariant    *parameters)
{
  GType (*get_type) (void);

  ensure_panel_types ();

  get_type = g_hash_table_lookup (panel_types, name);
  g_assert (get_type != NULL);

  return g_object_new (get_type (),
                       "shell", shell,
                       "parameters", parameters,
                       "title", title,
                       NULL);
}

#endif /* CC_PANEL_LOADER_NO_GTYPES */

/**
 * cc_panel_loader_fill_model:
 * @model: a #CcShellModel
 *
 * Fills @model with information from the available panels. It
 * iterates over the panel vtable, gathering the panel names,
 * build the desktop filename from it, and retrieves additional
 * information from it.
 */
void
cc_panel_loader_fill_model (CcShellModel *model)
{
  guint i;

  for (i = 0; i < panels_vtable_len; i++)
    {
      g_autoptr(GDesktopAppInfo) app = NULL;
      g_autofree gchar *desktop_name = NULL;
      gint category;

      desktop_name = g_strconcat ("gnome-", panels_vtable[i].name, "-panel.desktop", NULL);
      app = g_desktop_app_info_new (desktop_name);

      if (!app)
        {
          g_warning ("Ignoring broken panel %s (missing desktop file)", panels_vtable[i].name);
          continue;
        }

      category = parse_categories (app);
      if (G_UNLIKELY (category < 0))
        continue;

      cc_shell_model_add_item (model, category, G_APP_INFO (app), panels_vtable[i].name);
    }

  for (i = 0; i < supages_vtable_len; i++)
    {
      g_autoptr(GDesktopAppInfo) app = NULL;
      g_autofree gchar *desktop_name = NULL;

      desktop_name = g_strconcat ("gnome-", subpages_vtable[i].name, "-panel.desktop", NULL);
      app = g_desktop_app_info_new (desktop_name);

      if (!app)
        {
          g_warning ("Ignoring broken panel %s (missing desktop file)", subpages_vtable[i].name);
          continue;
        }

      cc_shell_model_add_item (model, subpages_vtable[i].category, G_APP_INFO (app), subpages_vtable[i].name);
      cc_shell_model_set_panel_visibility (model, subpages_vtable[i].name, CC_PANEL_VISIBLE_IN_SEARCH);
    }

  /* If there's an static init function, execute it after adding all panels to
   * the model. This will allow the panels to show or hide themselves without
   * having an instance running.
   */
#ifndef CC_PANEL_LOADER_NO_GTYPES
  for (i = 0; i < panels_vtable_len; i++)
    {
      if (panels_vtable[i].static_init_func)
        panels_vtable[i].static_init_func ();
    }
#endif
}

/**
 * cc_panel_loader_list_panels:
 *
 * Prints the list of panels from the current panel vtable,
 * usually as response to running GNOME Settings with the
 * '--list' command line argument.
 */
void
cc_panel_loader_list_panels (void)
{
  guint i;

  g_print ("%s\n", _("Available panels:"));

  for (i = 0; i < panels_vtable_len; i++)
    g_print ("\t%s\n", panels_vtable[i].name);

}

/**
 * cc_panel_loader_override_vtable:
 * @override_vtable: the new panel vtable
 * @n_elements: number of items of @override_vtable
 *
 * Override the default panel vtable so that GNOME Settings loads
 * a custom set of panels. Intended to be used by tests to inject
 * panels that exercise specific interactions with CcWindow (e.g.
 * header widgets, permissions, etc).
 */
void
cc_panel_loader_override_vtable (CcPanelLoaderVtable *override_vtable,
                                 gsize                n_elements)
{
  g_assert (override_vtable != NULL);
  g_assert (n_elements > 0);

  g_debug ("Overriding default panel vtable");

  panels_vtable = override_vtable;
  panels_vtable_len = n_elements;
}
