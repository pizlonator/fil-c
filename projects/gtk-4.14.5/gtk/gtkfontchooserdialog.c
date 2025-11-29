/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include "gtkfontchooserdialogprivate.h"
#include "deprecated/gtkfontchooser.h"
#include "deprecated/gtkfontchooserwidget.h"
#include "gtkfontchooserwidgetprivate.h"
#include "gtkfontchooserutils.h"
#include <glib/gi18n-lib.h>
#include "gtkbuildable.h"
#include "gtkprivate.h"
#include "gtkwidget.h"
#include "gtksettings.h"
#include "deprecated/gtkdialogprivate.h"
#include "gtktogglebutton.h"
#include "gtkheaderbar.h"
#include "gtkactionable.h"
#include "gtkeventcontrollerkey.h"
#include "gtkaccessible.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct _GtkFontChooserDialogClass GtkFontChooserDialogClass;

struct _GtkFontChooserDialog
{
  GtkDialog parent_instance;

  GtkWidget *fontchooser;
  GtkWidget *select_button;
  GtkWidget *cancel_button;
  GtkWidget *tweak_button;
};

struct _GtkFontChooserDialogClass
{
  GtkDialogClass parent_class;
};

/**
 * GtkFontChooserDialog:
 *
 * The `GtkFontChooserDialog` widget is a dialog for selecting a font.
 *
 * ![An example GtkFontChooserDialog](fontchooser.png)
 *
 * `GtkFontChooserDialog` implements the [iface@Gtk.FontChooser] interface
 * and does not provide much API of its own.
 *
 * To create a `GtkFontChooserDialog`, use [ctor@Gtk.FontChooserDialog.new].
 *
 * # GtkFontChooserDialog as GtkBuildable
 *
 * The `GtkFontChooserDialog` implementation of the `GtkBuildable`
 * interface exposes the buttons with the names “select_button”
 * and “cancel_button”.
 *
 * ## CSS nodes
 *
 * `GtkFontChooserDialog` has a single CSS node with the name `window` and style
 * class `.fontchooser`.
 *
 * Deprecated: 4.10: Use [class@Gtk.FontDialog] instead
 */

static void     gtk_font_chooser_dialog_buildable_interface_init     (GtkBuildableIface *iface);
static GObject *gtk_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                                      GtkBuilder   *builder,
                                                                      const char   *childname);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserDialog, gtk_font_chooser_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                _gtk_font_chooser_delegate_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_font_chooser_dialog_buildable_interface_init))

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_font_chooser_dialog_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    default:
      g_object_set_property (G_OBJECT (dialog->fontchooser), pspec->name, value);
      break;
    }
}

static void
gtk_font_chooser_dialog_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    default:
      g_object_get_property (G_OBJECT (dialog->fontchooser), pspec->name, value);
      break;
    }
}

static void
font_activated_cb (GtkFontChooser *fontchooser,
                   const char     *fontname,
                   gpointer        user_data)
{
  GtkDialog *dialog = user_data;

  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static gboolean
dialog_forward_key (GtkEventControllerKey *controller,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        modifiers,
                    GtkWidget             *widget)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (widget);

  return gtk_event_controller_key_forward (controller, dialog->fontchooser);
}

static void
update_tweak_button (GtkFontChooserDialog *dialog)
{
  GtkFontChooserLevel level;

  if (!dialog->tweak_button)
    return;

  g_object_get (dialog->fontchooser, "level", &level, NULL);
  gtk_widget_set_visible (dialog->tweak_button,
                          (level & (GTK_FONT_CHOOSER_LEVEL_FEATURES |
                                    GTK_FONT_CHOOSER_LEVEL_VARIATIONS)) != 0);
}

static void
setup_tweak_button (GtkFontChooserDialog *dialog)
{
  gboolean use_header;

  if (dialog->tweak_button)
    return;

  g_object_get (dialog, "use-header-bar", &use_header, NULL);
  if (use_header)
    {
      GtkWidget *button;
      GtkWidget *header;
      GActionGroup *actions;

      actions = G_ACTION_GROUP (g_simple_action_group_new ());
      g_action_map_add_action (G_ACTION_MAP (actions), gtk_font_chooser_widget_get_tweak_action (dialog->fontchooser));
      gtk_widget_insert_action_group (GTK_WIDGET (dialog), "font", actions);
      g_object_unref (actions);

      button = gtk_toggle_button_new ();
      gtk_actionable_set_action_name (GTK_ACTIONABLE (button), "font.tweak");
      gtk_widget_set_focus_on_click (button, FALSE);
      gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
      gtk_button_set_icon_name (GTK_BUTTON (button), "emblem-system-symbolic");
      gtk_widget_set_tooltip_text (button, _("Change Font Features"));
      gtk_accessible_update_property (GTK_ACCESSIBLE (button),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL,
                                      _("Change Font Features"),
                                      -1);

      header = gtk_dialog_get_header_bar (GTK_DIALOG (dialog));
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      dialog->tweak_button = button;

      update_tweak_button (dialog);
    }
}

static void
gtk_font_chooser_dialog_map (GtkWidget *widget)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (widget);

  setup_tweak_button (dialog);

  GTK_WIDGET_CLASS (gtk_font_chooser_dialog_parent_class)->map (widget);
}

static void
update_button (GtkFontChooserDialog *dialog)
{
  PangoFontDescription *desc;

  desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (dialog->fontchooser));

  gtk_widget_set_sensitive (dialog->select_button, desc != NULL);

  if (desc)
    pango_font_description_free (desc);
}

static void
gtk_font_chooser_dialog_dispose (GObject *object)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (object);

  if (dialog->fontchooser)
    {
      g_signal_handlers_disconnect_by_func (dialog->fontchooser,
                                            update_button,
                                            dialog);
      g_signal_handlers_disconnect_by_func (dialog->fontchooser,
                                            update_tweak_button,
                                            dialog);
    }

  /* tweak_button is not a template child */
  g_clear_pointer (&dialog->tweak_button, gtk_widget_unparent);

  gtk_widget_dispose_template (GTK_WIDGET (dialog), GTK_TYPE_FONT_CHOOSER_DIALOG);

  G_OBJECT_CLASS (gtk_font_chooser_dialog_parent_class)->dispose (object);
}

static void
gtk_font_chooser_dialog_class_init (GtkFontChooserDialogClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->dispose = gtk_font_chooser_dialog_dispose;
  gobject_class->get_property = gtk_font_chooser_dialog_get_property;
  gobject_class->set_property = gtk_font_chooser_dialog_set_property;

  widget_class->map = gtk_font_chooser_dialog_map;

  _gtk_font_chooser_install_properties (gobject_class);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/libgtk/ui/gtkfontchooserdialog.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserDialog, fontchooser);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserDialog, select_button);
  gtk_widget_class_bind_template_child (widget_class, GtkFontChooserDialog, cancel_button);
  gtk_widget_class_bind_template_callback (widget_class, font_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, dialog_forward_key);
}

static void
gtk_font_chooser_dialog_init (GtkFontChooserDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (dialog));

  _gtk_font_chooser_set_delegate (GTK_FONT_CHOOSER (dialog),
                                  GTK_FONT_CHOOSER (dialog->fontchooser));

  g_signal_connect_swapped (dialog->fontchooser, "notify::font-desc",
                            G_CALLBACK (update_button), dialog);
  update_button (dialog);
  g_signal_connect_swapped (dialog->fontchooser, "notify::level",
                            G_CALLBACK (update_tweak_button), dialog);
}

/**
 * gtk_font_chooser_dialog_new:
 * @title: (nullable): Title of the dialog
 * @parent: (nullable): Transient parent of the dialog
 *
 * Creates a new `GtkFontChooserDialog`.
 *
 * Returns: a new `GtkFontChooserDialog`
 *
 * Deprecated: 4.10: Use [class@Gtk.FontDialog] instead
 */
GtkWidget*
gtk_font_chooser_dialog_new (const char *title,
                             GtkWindow   *parent)
{
  GtkFontChooserDialog *dialog;

  dialog = g_object_new (GTK_TYPE_FONT_CHOOSER_DIALOG,
                         "title", title,
                         "transient-for", parent,
                         NULL);

  return GTK_WIDGET (dialog);
}

static void
gtk_font_chooser_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->get_internal_child = gtk_font_chooser_dialog_buildable_get_internal_child;
}

static GObject *
gtk_font_chooser_dialog_buildable_get_internal_child (GtkBuildable *buildable,
                                                      GtkBuilder   *builder,
                                                      const char   *childname)
{
  GtkFontChooserDialog *dialog = GTK_FONT_CHOOSER_DIALOG (buildable);

  if (g_strcmp0 (childname, "select_button") == 0)
    return G_OBJECT (dialog->select_button);
  else if (g_strcmp0 (childname, "cancel_button") == 0)
    return G_OBJECT (dialog->cancel_button);

  return parent_buildable_iface->get_internal_child (buildable, builder, childname);
}

void
gtk_font_chooser_dialog_set_filter (GtkFontChooserDialog *dialog,
                                    GtkFilter            *filter)
{
  gtk_font_chooser_widget_set_filter (GTK_FONT_CHOOSER_WIDGET (dialog->fontchooser), filter);
}
