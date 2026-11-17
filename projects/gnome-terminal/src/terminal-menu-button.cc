/*
 * Copyright © 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANMENUILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "terminal-menu-button.hh"
#include "terminal-intl.hh"
#include "terminal-libgsystem.hh"

/* All this just because GtkToggleButton:toggled is RUN_FIRST (and the
 * notify::active comes after the toggled signal). :-(
 */

enum
{
  UPDATE_MENU,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void popup_menu_selection_done_cb (GtkMenu *menu,
                                          GtkMenuButton *button);

/* The menu button sets itself insensitive when it has no menu.
 * Work around this by using an empty menu.
 */
static void
set_empty_menu (GtkMenuButton *button)
{
  gs_unref_object GMenu *menu = g_menu_new ();
  gtk_menu_button_set_menu_model (button, G_MENU_MODEL (menu));
}

static void
disconnect_popup_menu (GtkMenuButton *button)
{
  GtkMenu *popup_menu = gtk_menu_button_get_popup (button);

  if (popup_menu)
    g_signal_handlers_disconnect_by_func(popup_menu,
					 (void*)popup_menu_selection_done_cb,
					 button);
}

static void
popup_menu_selection_done_cb (GtkMenu *menu,
                              GtkMenuButton *button)
{
  disconnect_popup_menu (button);
  set_empty_menu (button);
}

/* Class implementation */

G_DEFINE_TYPE (TerminalMenuButton, terminal_menu_button, GTK_TYPE_MENU_BUTTON);

static void
terminal_menu_button_init (TerminalMenuButton *button_)
{
  GtkButton *button = GTK_BUTTON (button_);
  GtkMenuButton *menu_button = GTK_MENU_BUTTON (button_);

  gtk_button_set_relief (button, GTK_RELIEF_NONE);
  gtk_button_set_focus_on_click (button, FALSE);
  gtk_menu_button_set_use_popover (menu_button, FALSE);
  set_empty_menu (menu_button);
}

static void
terminal_menu_button_toggled (GtkToggleButton *button)
{
  gboolean active = gtk_toggle_button_get_active (button); /* this is already the new state */

  /* On activate, update the menu */
  if (active)
    g_signal_emit (button, signals[UPDATE_MENU], 0);

  GTK_TOGGLE_BUTTON_CLASS (terminal_menu_button_parent_class)->toggled (button);
}

static void
terminal_menu_button_update_menu (TerminalMenuButton *button)
{
  GtkMenuButton *gtk_button = GTK_MENU_BUTTON (button);
  GtkMenu *popup_menu = gtk_menu_button_get_popup (gtk_button);

  if (popup_menu)
    g_signal_connect (popup_menu, "selection-done",
                      G_CALLBACK (popup_menu_selection_done_cb), button);
}

static void
terminal_menu_button_dispose (GObject *object)
{
  disconnect_popup_menu (GTK_MENU_BUTTON (object));

  G_OBJECT_CLASS (terminal_menu_button_parent_class)->dispose (object);
}

static void
terminal_menu_button_class_init (TerminalMenuButtonClass *klass)
{
  klass->update_menu = terminal_menu_button_update_menu;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = terminal_menu_button_dispose;

  GtkToggleButtonClass *toggle_button_class = GTK_TOGGLE_BUTTON_CLASS (klass);
  toggle_button_class->toggled = terminal_menu_button_toggled;

  signals[UPDATE_MENU] =
    g_signal_new (I_("update-menu"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalMenuButtonClass, update_menu),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

/* public API */

/**
 * terminal_menu_button_new:
 *
 * Returns: a new #TerminalMenuButton
 */
GtkWidget *
terminal_menu_button_new (void)
{
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_MENU_BUTTON, nullptr));
}
