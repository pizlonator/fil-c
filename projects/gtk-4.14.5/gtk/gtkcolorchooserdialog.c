/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "deprecated/gtkdialog.h"
#include "deprecated/gtkdialogprivate.h"
#include "gtkbutton.h"
#include "gtkbox.h"
#include "gtkprivate.h"
#include "gtksettings.h"

#include "deprecated/gtkcolorchooserprivate.h"
#include "deprecated/gtkcolorchooserdialog.h"
#include "deprecated/gtkcolorchooserwidget.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkColorChooserDialog:
 *
 * A dialog for choosing a color.
 *
 * ![An example GtkColorChooserDialog](colorchooser.png)
 *
 * `GtkColorChooserDialog` implements the [iface@Gtk.ColorChooser] interface
 * and does not provide much API of its own.
 *
 * To create a `GtkColorChooserDialog`, use [ctor@Gtk.ColorChooserDialog.new].
 *
 * To change the initially selected color, use
 * [method@Gtk.ColorChooser.set_rgba]. To get the selected color use
 * [method@Gtk.ColorChooser.get_rgba].
 *
 * `GtkColorChooserDialog` has been deprecated in favor of [class@Gtk.ColorDialog].
 *
 * ## CSS nodes
 *
 * `GtkColorChooserDialog` has a single CSS node with the name `window` and style
 * class `.colorchooser`.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */

typedef struct _GtkColorChooserDialogClass   GtkColorChooserDialogClass;

struct _GtkColorChooserDialog
{
  GtkDialog parent_instance;

  GtkWidget *chooser;
};

struct _GtkColorChooserDialogClass
{
  GtkDialogClass parent_class;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_USE_ALPHA,
  PROP_SHOW_EDITOR
};

static void gtk_color_chooser_dialog_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorChooserDialog, gtk_color_chooser_dialog, GTK_TYPE_DIALOG,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_chooser_dialog_iface_init))

static void
propagate_notify (GObject               *o,
                  GParamSpec            *pspec,
                  GtkColorChooserDialog *cc)
{
  g_object_notify (G_OBJECT (cc), pspec->name);
}

static void
save_color (GtkColorChooserDialog *dialog)
{
  GdkRGBA color;

  /* This causes the color chooser widget to save the
   * selected and custom colors to GSettings.
   */
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &color);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &color);
}

static void
color_activated_cb (GtkColorChooser *chooser,
                    GdkRGBA         *color,
                    GtkDialog       *dialog)
{
  save_color (GTK_COLOR_CHOOSER_DIALOG (dialog));
  gtk_dialog_response (dialog, GTK_RESPONSE_OK);
}

static void
gtk_color_chooser_dialog_response (GtkDialog *dialog,
                                   int        response_id,
                                   gpointer   user_data)
{
  if (response_id == GTK_RESPONSE_OK)
    save_color (GTK_COLOR_CHOOSER_DIALOG (dialog));
}

static void
gtk_color_chooser_dialog_init (GtkColorChooserDialog *cc)
{
  gtk_widget_init_template (GTK_WIDGET (cc));
  gtk_dialog_set_use_header_bar_from_setting (GTK_DIALOG (cc));

  g_signal_connect (cc, "response",
                    G_CALLBACK (gtk_color_chooser_dialog_response), NULL);
}

static void
gtk_color_chooser_dialog_unmap (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_color_chooser_dialog_parent_class)->unmap (widget);

  /* We never want the dialog to come up with the editor,
   * even if it was showing the editor the last time it was used.
   */
  g_object_set (widget, "show-editor", FALSE, NULL);
}

static void
gtk_color_chooser_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc), &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_color_chooser_get_use_alpha (GTK_COLOR_CHOOSER (cc->chooser)));
      break;
    case PROP_SHOW_EDITOR:
      {
        gboolean show_editor;
        g_object_get (cc->chooser, "show-editor", &show_editor, NULL);
        g_value_set_boolean (value, show_editor);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc), g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      if (gtk_color_chooser_get_use_alpha (GTK_COLOR_CHOOSER (cc->chooser)) != g_value_get_boolean (value))
        {
          gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (cc->chooser), g_value_get_boolean (value));
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_SHOW_EDITOR:
      g_object_set (cc->chooser,
                    "show-editor", g_value_get_boolean (value),
                    NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_dialog_dispose (GObject *object)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (object);

  g_clear_pointer (&cc->chooser, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_color_chooser_dialog_parent_class)->dispose (object);
}

static void
gtk_color_chooser_dialog_class_init (GtkColorChooserDialogClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_color_chooser_dialog_dispose;
  object_class->get_property = gtk_color_chooser_dialog_get_property;
  object_class->set_property = gtk_color_chooser_dialog_set_property;

  widget_class->unmap = gtk_color_chooser_dialog_unmap;

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");
  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", NULL, NULL,
                            FALSE, GTK_PARAM_READWRITE));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkcolorchooserdialog.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkColorChooserDialog, chooser);
  gtk_widget_class_bind_template_callback (widget_class, propagate_notify);
  gtk_widget_class_bind_template_callback (widget_class, color_activated_cb);
}

static void
gtk_color_chooser_dialog_get_rgba (GtkColorChooser *chooser,
                                   GdkRGBA         *color)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (chooser);

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (cc->chooser), color);
}

static void
gtk_color_chooser_dialog_set_rgba (GtkColorChooser *chooser,
                                   const GdkRGBA   *color)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (chooser);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc->chooser), color);
}

static void
gtk_color_chooser_dialog_add_palette (GtkColorChooser *chooser,
                                      GtkOrientation   orientation,
                                      int              colors_per_line,
                                      int              n_colors,
                                      GdkRGBA         *colors)
{
  GtkColorChooserDialog *cc = GTK_COLOR_CHOOSER_DIALOG (chooser);

  gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (cc->chooser),
                                 orientation, colors_per_line, n_colors, colors);
}

static void
gtk_color_chooser_dialog_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_chooser_dialog_get_rgba;
  iface->set_rgba = gtk_color_chooser_dialog_set_rgba;
  iface->add_palette = gtk_color_chooser_dialog_add_palette;
}

/**
 * gtk_color_chooser_dialog_new:
 * @title: (nullable): Title of the dialog
 * @parent: (nullable): Transient parent of the dialog
 *
 * Creates a new `GtkColorChooserDialog`.
 *
 * Returns: a new `GtkColorChooserDialog`
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialog] instead
 */
GtkWidget *
gtk_color_chooser_dialog_new (const char *title,
                              GtkWindow   *parent)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_DIALOG,
                       "title", title,
                       "transient-for", parent,
                       NULL);
}
