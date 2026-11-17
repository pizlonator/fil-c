/*
 * Copyright © 2018 Christian Persch
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

#include <glib/gi18n.h>

#include "terminal-headerbar.hh"
#include "terminal-app.hh"
#include "terminal-libgsystem.hh"

typedef struct _TerminalHeaderbarPrivate TerminalHeaderbarPrivate;

struct _TerminalHeaderbar
{
  GtkHeaderBar parent_instance;
};

struct _TerminalHeaderbarClass
{
  GtkHeaderBarClass parent_class;
};

struct _TerminalHeaderbarPrivate
{
  GtkWidget *profilebutton;
  GtkWidget *menubutton;
};

enum {
  PROP_0,
  LAST_PROP
};

enum {
  LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */
/* static GParamSpec *pspecs[LAST_PROP]; */

G_DEFINE_TYPE_WITH_PRIVATE (TerminalHeaderbar, terminal_headerbar, GTK_TYPE_HEADER_BAR)

#define PRIV(obj) ((TerminalHeaderbarPrivate *) terminal_headerbar_get_instance_private ((TerminalHeaderbar *)(obj)))

static void
profilemenu_items_changed_cb (GMenuModel *menu,
                              int position G_GNUC_UNUSED,
                              int removed G_GNUC_UNUSED,
                              int added G_GNUC_UNUSED,
                              TerminalHeaderbarPrivate *priv)
{
  if (g_menu_model_get_n_items (menu) > 0)
    gtk_widget_show (priv->profilebutton);
  else
    gtk_widget_hide (priv->profilebutton);
}

/* Class implementation */

static void
terminal_headerbar_init (TerminalHeaderbar *headerbar)
{

  TerminalHeaderbarPrivate *priv = PRIV (headerbar);
  GtkWidget *widget = GTK_WIDGET (headerbar);
  TerminalApp *app = terminal_app_get ();
  GMenuModel *profilemenu;

  gtk_widget_init_template (widget);

  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->menubutton),
                                  terminal_app_get_headermenu (app));

  profilemenu = terminal_app_get_profilemenu (app);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->profilebutton),
                                  profilemenu);

  g_signal_connect (profilemenu, "items-changed",
                    G_CALLBACK (profilemenu_items_changed_cb), priv);
  profilemenu_items_changed_cb (profilemenu, 0, 0, 0, priv);
}

static void
terminal_headerbar_dispose (GObject *object)
{
  TerminalHeaderbar *headerbar = TERMINAL_HEADERBAR (object);
  TerminalHeaderbarPrivate *priv = PRIV (headerbar);
  TerminalApp *app = terminal_app_get ();

  GMenuModel *profilemenu = terminal_app_get_profilemenu (app);
  g_signal_handlers_disconnect_by_func (profilemenu,
                                        (void*)profilemenu_items_changed_cb,
                                        priv);

  G_OBJECT_CLASS (terminal_headerbar_parent_class)->dispose (object);
}

static void
terminal_headerbar_get_property (GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  //  TerminalHeaderbar *headerbar = TERMINAL_HEADERBAR (object);

  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
terminal_headerbar_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  switch (prop_id) {
  default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_headerbar_class_init (TerminalHeaderbarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = terminal_headerbar_dispose;
  gobject_class->get_property = terminal_headerbar_get_property;
  gobject_class->set_property = terminal_headerbar_set_property;

  /* g_object_class_install_properties (gobject_class, G_N_ELEMENTS (pspecs), pspecs); */

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/terminal/ui/headerbar.ui");
  gtk_widget_class_bind_template_child_private (widget_class, TerminalHeaderbar, menubutton);
  gtk_widget_class_bind_template_child_private (widget_class, TerminalHeaderbar, profilebutton);
}

/* public API */

/**
 * terminal_headerbar_new:
 *
 * Returns: a new #TerminalHeaderbar
 */
GtkWidget *
terminal_headerbar_new (void)
{
  return reinterpret_cast<GtkWidget*>
    (g_object_new (TERMINAL_TYPE_HEADERBAR, nullptr));
}
