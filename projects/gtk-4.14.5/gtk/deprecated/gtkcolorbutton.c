/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
/* Color picker button for GNOME
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkcolorbutton.h"

#include "gtkbinlayout.h"
#include "gtkbutton.h"
#include "gtkcolorchooser.h"
#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserdialog.h"
#include "gtkcolorswatchprivate.h"
#include "gtkdragsource.h"
#include "gtkdroptarget.h"
#include <glib/gi18n-lib.h>
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtksnapshot.h"
#include "gtkwidgetprivate.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkColorButton:
 *
 * The `GtkColorButton` allows to open a color chooser dialog to change
 * the color.
 *
 * ![An example GtkColorButton](color-button.png)
 *
 * It is suitable widget for selecting a color in a preference dialog.
 *
 * # CSS nodes
 *
 * ```
 * colorbutton
 * ╰── button.color
 *     ╰── [content]
 * ```
 *
 * `GtkColorButton` has a single CSS node with name colorbutton which
 * contains a button node. To differentiate it from a plain `GtkButton`,
 * it gets the .color style class.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialogButton] instead
 */

typedef struct _GtkColorButtonClass     GtkColorButtonClass;

struct _GtkColorButton {
  GtkWidget parent_instance;

  GtkWidget *button;

  GtkWidget *swatch;    /* Widget where we draw the color sample */
  GtkWidget *cs_dialog; /* Color selection dialog */

  char *title;         /* Title for the color chooser dialog */
  GdkRGBA rgba;

  guint use_alpha   : 1;  /* Use alpha or not */
  guint show_editor : 1;
  guint modal       : 1;
};

struct _GtkColorButtonClass {
  GtkWidgetClass parent_class;

  void (* color_set) (GtkColorButton *cp);
  void (* activate)  (GtkColorButton *self);
};

/* Properties */
enum
{
  PROP_0,
  PROP_USE_ALPHA,
  PROP_TITLE,
  PROP_RGBA,
  PROP_SHOW_EDITOR,
  PROP_MODAL
};

/* Signals */
enum
{
  COLOR_SET,
  ACTIVATE,
  LAST_SIGNAL
};

/* gobject signals */
static void gtk_color_button_finalize      (GObject          *object);
static void gtk_color_button_set_property  (GObject          *object,
                                            guint             param_id,
                                            const GValue     *value,
                                            GParamSpec       *pspec);
static void gtk_color_button_get_property  (GObject          *object,
                                            guint             param_id,
                                            GValue           *value,
                                            GParamSpec       *pspec);

static void gtk_color_button_root          (GtkWidget        *widget);

/* gtkbutton signals */
static void gtk_color_button_clicked       (GtkButton        *button,
                                            gpointer          user_data);


static guint color_button_signals[LAST_SIGNAL] = { 0 };

static void gtk_color_button_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorButton, gtk_color_button, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_button_iface_init))

static void
gtk_color_button_activate (GtkColorButton *self)
{
  gtk_widget_activate (self->button);
}

static void
gtk_color_button_class_init (GtkColorButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->get_property = gtk_color_button_get_property;
  gobject_class->set_property = gtk_color_button_set_property;
  gobject_class->finalize = gtk_color_button_finalize;

  widget_class->grab_focus = gtk_widget_grab_focus_child;
  widget_class->focus = gtk_widget_focus_child;
  widget_class->root = gtk_color_button_root;

  klass->color_set = NULL;
  klass->activate = gtk_color_button_activate;

  g_object_class_override_property (gobject_class, PROP_RGBA, "rgba");
  g_object_class_override_property (gobject_class, PROP_USE_ALPHA, "use-alpha");

  /**
   * GtkColorButton:title: (attributes org.gtk.Property.get=gtk_color_button_get_title org.gtk.Property.set=gtk_color_button_set_title)
   *
   * The title of the color chooser dialog
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", NULL, NULL,
                                                        _("Pick a Color"),
                                                        GTK_PARAM_READWRITE));


  /**
   * GtkColorButton::color-set:
   * @widget: the object which received the signal.
   *
   * Emitted when the user selects a color.
   *
   * When handling this signal, use [method@Gtk.ColorChooser.get_rgba]
   * to find out which color was just selected.
   *
   * Note that this signal is only emitted when the user changes the color.
   * If you need to react to programmatic color changes as well, use
   * the notify::rgba signal.
   */
  color_button_signals[COLOR_SET] = g_signal_new (I_("color-set"),
                                                  G_TYPE_FROM_CLASS (gobject_class),
                                                  G_SIGNAL_RUN_FIRST,
                                                  G_STRUCT_OFFSET (GtkColorButtonClass, color_set),
                                                  NULL, NULL,
                                                  NULL,
                                                  G_TYPE_NONE, 0);

  /**
   * GtkColorButton::activate:
   * @widget: the object which received the signal.
   *
   * Emitted to when the color button is activated.
   *
   * The `::activate` signal on `GtkMenuButton` is an action signal and
   * emitting it causes the button to pop up its dialog.
   *
   * Since: 4.4
   */
  color_button_signals[ACTIVATE] =
      g_signal_new (I_ ("activate"),
                    G_OBJECT_CLASS_TYPE (gobject_class),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                    G_STRUCT_OFFSET (GtkColorButtonClass, activate),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, color_button_signals[ACTIVATE]);

  /**
   * GtkColorButton:show-editor:
   *
   * Whether the color chooser should open in editor mode.
   *
   * This property should be used in cases where the palette
   * in the editor would be redundant, such as when the color
   * button is already part of a palette.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_EDITOR,
                                   g_param_spec_boolean ("show-editor", NULL, NULL,
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkColorButton:modal: (attributes org.gtk.Property.get=gtk_color_button_get_modal org.gtk.Property.set=gtk_color_button_set_modal)
   *
   * Whether the color chooser dialog should be modal.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MODAL,
                                   g_param_spec_boolean ("modal", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "colorbutton");
}

static gboolean
gtk_color_button_drop (GtkDropTarget  *dest,
                       const GValue   *value,
                       double          x,
                       double          y,
                       GtkColorButton *button)
{
  GdkRGBA *color = g_value_get_boxed (value);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), color);
  return TRUE;
}

static GdkContentProvider *
gtk_color_button_drag_prepare (GtkDragSource  *source,
                               double          x,
                               double          y,
                               GtkColorButton *button)
{
  return gdk_content_provider_new_typed (GDK_TYPE_RGBA, &button->rgba);
}

static void
gtk_color_button_init (GtkColorButton *button)
{
  PangoLayout *layout;
  PangoRectangle rect;
  GtkDragSource *source;
  GtkDropTarget *dest;

  button->button = gtk_button_new ();
  g_signal_connect (button->button, "clicked", G_CALLBACK (gtk_color_button_clicked), button);
  g_object_bind_property (button, "focus-on-click",
                          button->button, "focus-on-click",
                          0);
  gtk_widget_set_parent (button->button, GTK_WIDGET (button));

  button->swatch = g_object_new (GTK_TYPE_COLOR_SWATCH,
                                 "accessible-role", GTK_ACCESSIBLE_ROLE_IMG,
                                 "selectable", FALSE,
                                 "has-menu", FALSE,
                                 "can-drag", FALSE,
                                 NULL);
  gtk_widget_set_can_focus (button->swatch, FALSE);
  gtk_widget_remove_css_class (button->swatch, "activatable");
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (button), "Black");
  pango_layout_get_pixel_extents (layout, NULL, &rect);
  g_object_unref (layout);

  gtk_widget_set_size_request (button->swatch, rect.width, rect.height);

  gtk_button_set_child (GTK_BUTTON (button->button), button->swatch);

  button->title = g_strdup (_("Pick a Color")); /* default title */

  /* Start with opaque black, alpha disabled */
  button->rgba.red = 0;
  button->rgba.green = 0;
  button->rgba.blue = 0;
  button->rgba.alpha = 1;
  button->use_alpha = FALSE;
  button->modal = TRUE;

  dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (gtk_color_button_drop), button);
  gtk_widget_add_controller (GTK_WIDGET (button), GTK_EVENT_CONTROLLER (dest));

  source = gtk_drag_source_new ();
  g_signal_connect (source, "prepare", G_CALLBACK (gtk_color_button_drag_prepare), button);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (source), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (button->button, GTK_EVENT_CONTROLLER (source));

  gtk_widget_add_css_class (button->button, "color");
}

static void
gtk_color_button_finalize (GObject *object)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (object);

  if (button->cs_dialog != NULL)
    gtk_window_destroy (GTK_WINDOW (button->cs_dialog));

  g_free (button->title);
  gtk_widget_unparent (button->button);

  G_OBJECT_CLASS (gtk_color_button_parent_class)->finalize (object);
}


/**
 * gtk_color_button_new:
 *
 * Creates a new color button.
 *
 * This returns a widget in the form of a small button containing
 * a swatch representing the current selected color. When the button
 * is clicked, a color chooser dialog will open, allowing the user
 * to select a color. The swatch will be updated to reflect the new
 * color when the user finishes.
 *
 * Returns: a new color button
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialogButton] instead
 */
GtkWidget *
gtk_color_button_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_BUTTON, NULL);
}

/**
 * gtk_color_button_new_with_rgba:
 * @rgba: A `GdkRGBA` to set the current color with
 *
 * Creates a new color button showing the given color.
 *
 * Returns: a new color button
 */
GtkWidget *
gtk_color_button_new_with_rgba (const GdkRGBA *rgba)
{
  return g_object_new (GTK_TYPE_COLOR_BUTTON, "rgba", rgba, NULL);
}

static gboolean
dialog_destroy (GtkWidget *widget,
                gpointer   data)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (data);

  button->cs_dialog = NULL;

  return FALSE;
}

static void
dialog_response (GtkDialog *dialog,
                 int        response,
                 gpointer   data)
{
  if (response == GTK_RESPONSE_CANCEL)
    gtk_widget_hide (GTK_WIDGET (dialog));
  else if (response == GTK_RESPONSE_OK)
    {
      GtkColorButton *button = GTK_COLOR_BUTTON (data);

      gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &button->rgba);
      gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (button->swatch), &button->rgba);

      gtk_widget_hide (GTK_WIDGET (dialog));
      g_object_ref (button);
      g_signal_emit (button, color_button_signals[COLOR_SET], 0);

      g_object_freeze_notify (G_OBJECT (button));
      g_object_notify (G_OBJECT (button), "rgba");
      g_object_thaw_notify (G_OBJECT (button));
      g_object_unref (button);
    }
}

/* Create the dialog and connects its buttons */
static void
ensure_dialog (GtkColorButton *button)
{
  GtkWidget *parent, *dialog;

  if (button->cs_dialog != NULL)
    return;

  parent = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));

  button->cs_dialog = dialog = gtk_color_chooser_dialog_new (button->title, NULL);
  gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW (dialog), button->modal);

  if (GTK_IS_WINDOW (parent))
    {
      if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (dialog)))
        gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));

      if (gtk_window_get_modal (GTK_WINDOW (parent)))
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
    }

  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_response), button);
  g_signal_connect (dialog, "destroy",
                    G_CALLBACK (dialog_destroy), button);
}

static void
gtk_color_button_root (GtkWidget *widget)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (widget);
  GtkWidget *parent;

  GTK_WIDGET_CLASS (gtk_color_button_parent_class)->root (widget);

  if (!button->cs_dialog)
    return;

  parent = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (button)));
  if (GTK_IS_WINDOW (parent))
    {
      if (GTK_WINDOW (parent) != gtk_window_get_transient_for (GTK_WINDOW (button->cs_dialog)))
        gtk_window_set_transient_for (GTK_WINDOW (button->cs_dialog), GTK_WINDOW (parent));

      if (gtk_window_get_modal (GTK_WINDOW (parent)))
        gtk_window_set_modal (GTK_WINDOW (button->cs_dialog), TRUE);
    }
}

static void
gtk_color_button_clicked (GtkButton *b,
                          gpointer   user_data)
{
  GtkColorButton *button = user_data;

  /* if dialog already exists, make sure it's shown and raised */
  ensure_dialog (button);

  g_object_set (button->cs_dialog, "show-editor", button->show_editor, NULL);

  gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (button->cs_dialog), button->use_alpha);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button->cs_dialog), &button->rgba);

  gtk_window_present (GTK_WINDOW (button->cs_dialog));
}

static guint
scale_round (double value,
             double scale)
{
  value = floor (value * scale + 0.5);
  value = CLAMP (value, 0, scale);
  return (guint)value;
}

static char *
accessible_color_name (const GdkRGBA *color)
{
  if (color->alpha < 1.0)
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%, Alpha %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100),
                            scale_round (color->alpha, 100));
  else
    return g_strdup_printf (_("Red %d%%, Green %d%%, Blue %d%%"),
                            scale_round (color->red, 100),
                            scale_round (color->green, 100),
                            scale_round (color->blue, 100));
}

static void
gtk_color_button_set_rgba (GtkColorChooser *chooser,
                           const GdkRGBA   *rgba)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (chooser);
  char *text;

  g_return_if_fail (GTK_IS_COLOR_BUTTON (chooser));
  g_return_if_fail (rgba != NULL);

  button->rgba = *rgba;
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (button->swatch), &button->rgba);

  text = accessible_color_name (rgba);
  gtk_accessible_update_property (GTK_ACCESSIBLE (button->swatch),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, text,
                                  -1);
  g_free (text);

  g_object_notify (G_OBJECT (chooser), "rgba");
}

static void
gtk_color_button_get_rgba (GtkColorChooser *chooser,
                           GdkRGBA        *rgba)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (chooser);

  g_return_if_fail (GTK_IS_COLOR_BUTTON (chooser));
  g_return_if_fail (rgba != NULL);

  *rgba = button->rgba;
}

static void
set_use_alpha (GtkColorButton *button,
               gboolean        use_alpha)
{
  use_alpha = (use_alpha != FALSE);

  if (button->use_alpha != use_alpha)
    {
      button->use_alpha = use_alpha;

      gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (button->swatch), use_alpha);

      g_object_notify (G_OBJECT (button), "use-alpha");
    }
}

/**
 * gtk_color_button_set_title: (attributes org.gtk.Method.set_property=title)
 * @button: a `GtkColorButton`
 * @title: String containing new window title
 *
 * Sets the title for the color chooser dialog.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialogButton] instead
 */
void
gtk_color_button_set_title (GtkColorButton *button,
                            const char     *title)
{
  char *old_title;

  g_return_if_fail (GTK_IS_COLOR_BUTTON (button));

  old_title = button->title;
  button->title = g_strdup (title);
  g_free (old_title);

  if (button->cs_dialog)
    gtk_window_set_title (GTK_WINDOW (button->cs_dialog), button->title);

  g_object_notify (G_OBJECT (button), "title");
}

/**
 * gtk_color_button_get_title: (attributes org.gtk.Method.get_property=title)
 * @button: a `GtkColorButton`
 *
 * Gets the title of the color chooser dialog.
 *
 * Returns: An internal string, do not free the return value
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialogButton] instead
 */
const char *
gtk_color_button_get_title (GtkColorButton *button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (button), NULL);

  return button->title;
}

/**
 * gtk_color_button_set_modal: (attributes org.gtk.Method.set_property=modal)
 * @button: a `GtkColorButton`
 * @modal: %TRUE to make the dialog modal
 *
 * Sets whether the dialog should be modal.
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialogButton] instead
 */
void
gtk_color_button_set_modal (GtkColorButton *button,
                            gboolean        modal)
{
  g_return_if_fail (GTK_IS_COLOR_BUTTON (button));

  if (button->modal == modal)
    return;

  button->modal = modal;

  if (button->cs_dialog)
    gtk_window_set_modal (GTK_WINDOW (button->cs_dialog), button->modal);

  g_object_notify (G_OBJECT (button), "modal");
}

/**
 * gtk_color_button_get_modal: (attributes org.gtk.Method.get_property=modal)
 * @button: a `GtkColorButton`
 *
 * Gets whether the dialog is modal.
 *
 * Returns: %TRUE if the dialog is modal
 *
 * Deprecated: 4.10: Use [class@Gtk.ColorDialogButton] instead
 */
gboolean
gtk_color_button_get_modal (GtkColorButton *button)
{
  g_return_val_if_fail (GTK_IS_COLOR_BUTTON (button), FALSE);

  return button->modal;
}

static void
gtk_color_button_set_property (GObject      *object,
                               guint         param_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (object);

  switch (param_id)
    {
    case PROP_USE_ALPHA:
      set_use_alpha (button, g_value_get_boolean (value));
      break;
    case PROP_TITLE:
      gtk_color_button_set_title (button, g_value_get_string (value));
      break;
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), g_value_get_boxed (value));
      break;
    case PROP_SHOW_EDITOR:
      {
        gboolean show_editor = g_value_get_boolean (value);
        if (button->show_editor != show_editor)
          {
            button->show_editor = show_editor;
            g_object_notify (object, "show-editor");
          }
      }
      break;
    case PROP_MODAL:
      gtk_color_button_set_modal (button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_color_button_get_property (GObject    *object,
                               guint       param_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (object);

  switch (param_id)
    {
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, button->use_alpha);
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtk_color_button_get_title (button));
      break;
    case PROP_RGBA:
      {
        GdkRGBA rgba;

        gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);
        g_value_set_boxed (value, &rgba);
      }
      break;
    case PROP_SHOW_EDITOR:
      g_value_set_boolean (value, button->show_editor);
      break;
    case PROP_MODAL:
      g_value_set_boolean (value, button->modal);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_color_button_add_palette (GtkColorChooser *chooser,
                              GtkOrientation   orientation,
                              int              colors_per_line,
                              int              n_colors,
                              GdkRGBA         *colors)
{
  GtkColorButton *button = GTK_COLOR_BUTTON (chooser);

  ensure_dialog (button);

  gtk_color_chooser_add_palette (GTK_COLOR_CHOOSER (button->cs_dialog),
                                 orientation, colors_per_line, n_colors, colors);
}

static void
gtk_color_button_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_button_get_rgba;
  iface->set_rgba = gtk_color_button_set_rgba;
  iface->add_palette = gtk_color_button_add_palette;
}
