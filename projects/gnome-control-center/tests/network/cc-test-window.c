/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010, 2018 Red Hat, Inc.
 * Copyright (c) 2016 Endless, Inc.
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
 * Author: Benjamin Berg <bberg@redhat.com>
 */

#define G_LOG_DOMAIN "cc-test-window"

#include <config.h>

#include "cc-test-window.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#include "shell/cc-panel.h"
#include "shell/cc-shell.h"
#include "cc-util.h"


struct _CcTestWindow
{
  GtkWindow    parent;

  GtkWidget   *main_box;

  GtkWidget  *header;
  CcPanel    *active_panel;
};

static void     cc_shell_iface_init         (CcShellInterface      *iface);

G_DEFINE_TYPE_WITH_CODE (CcTestWindow, cc_test_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE (CC_TYPE_SHELL, cc_shell_iface_init))

enum
{
  PROP_0,
  PROP_ACTIVE_PANEL
};



static void
set_active_panel (CcTestWindow *shell,
                  CcPanel      *panel)
{
  g_assert (CC_IS_SHELL (shell));
  g_assert (CC_IS_PANEL (panel));

  /* Only allow setting to a non NULL value once. */
  g_assert (shell->active_panel == NULL);

  if (panel)
    {
      shell->active_panel = g_object_ref (panel);
      gtk_box_append (GTK_BOX (shell->main_box), GTK_WIDGET (panel));
    }
}

/* CcShell implementation */
static gboolean
cc_test_window_set_active_panel_from_id (CcShell      *shell,
                                         const gchar  *start_id,
                                         GVariant     *parameters,
                                         GError      **error)
{
  /* Not implemented */
  g_assert_not_reached ();
}

static GtkWidget *
cc_test_window_get_toplevel (CcShell *shell)
{
  return GTK_WIDGET (shell);
}

static void
cc_shell_iface_init (CcShellInterface *iface)
{
  iface->set_active_panel_from_id = cc_test_window_set_active_panel_from_id;
  iface->get_toplevel = cc_test_window_get_toplevel;
}

/* GObject Implementation */
static void
cc_test_window_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CcTestWindow *self = CC_TEST_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      g_value_set_object (value, self->active_panel);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_test_window_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CcTestWindow *shell = CC_TEST_WINDOW (object);

  switch (property_id)
    {
    case PROP_ACTIVE_PANEL:
      set_active_panel (shell, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_test_window_dispose (GObject *object)
{
  CcTestWindow *self = CC_TEST_WINDOW (object);

  g_clear_object (&self->active_panel);

  G_OBJECT_CLASS (cc_test_window_parent_class)->dispose (object);
}

static void
cc_test_window_class_init (CcTestWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_test_window_get_property;
  object_class->set_property = cc_test_window_set_property;
  object_class->dispose = cc_test_window_dispose;

  g_object_class_override_property (object_class, PROP_ACTIVE_PANEL, "active-panel");
}

static void
cc_test_window_init (CcTestWindow *self)
{
  gtk_widget_set_size_request (GTK_WIDGET (self), 500, 800);

  self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);

  gtk_window_set_child (GTK_WINDOW (self), self->main_box);
}

CcTestWindow *
cc_test_window_new (void)
{
  return g_object_new (CC_TYPE_TEST_WINDOW,
                       "resizable", TRUE,
                       "title", "Test Settings",
                       NULL);
}
