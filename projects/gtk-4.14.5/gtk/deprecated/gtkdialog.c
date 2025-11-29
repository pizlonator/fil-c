/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkdialogprivate.h"
#include "gtkheaderbar.h"
#include "gtkheaderbarprivate.h"
#include "gtklabel.h"
#include "gtkmarshalers.h"
#include "gtkbox.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"
#include "gtksizegroup.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkDialog:
 *
 * Dialogs are a convenient way to prompt the user for a small amount
 * of input.
 *
 * ![An example GtkDialog](dialog.png)
 *
 * Typical uses are to display a message, ask a question, or anything else
 * that does not require extensive effort on the user’s part.
 *
 * The main area of a `GtkDialog` is called the "content area", and is yours
 * to populate with widgets such a `GtkLabel` or `GtkEntry`, to present
 * your information, questions, or tasks to the user.
 *
 * In addition, dialogs allow you to add "action widgets". Most commonly,
 * action widgets are buttons. Depending on the platform, action widgets may
 * be presented in the header bar at the top of the window, or at the bottom
 * of the window. To add action widgets, create your `GtkDialog` using
 * [ctor@Gtk.Dialog.new_with_buttons], or use
 * [method@Gtk.Dialog.add_button], [method@Gtk.Dialog.add_buttons],
 * or [method@Gtk.Dialog.add_action_widget].
 *
 * `GtkDialogs` uses some heuristics to decide whether to add a close
 * button to the window decorations. If any of the action buttons use
 * the response ID %GTK_RESPONSE_CLOSE or %GTK_RESPONSE_CANCEL, the
 * close button is omitted.
 *
 * Clicking a button that was added as an action widget will emit the
 * [signal@Gtk.Dialog::response] signal with a response ID that you specified.
 * GTK will never assign a meaning to positive response IDs; these are
 * entirely user-defined. But for convenience, you can use the response
 * IDs in the [enum@Gtk.ResponseType] enumeration (these all have values
 * less than zero). If a dialog receives a delete event, the
 * [signal@Gtk.Dialog::response] signal will be emitted with the
 * %GTK_RESPONSE_DELETE_EVENT response ID.
 *
 * Dialogs are created with a call to [ctor@Gtk.Dialog.new] or
 * [ctor@Gtk.Dialog.new_with_buttons]. The latter is recommended; it allows
 * you to set the dialog title, some convenient flags, and add buttons.
 *
 * A “modal” dialog (that is, one which freezes the rest of the application
 * from user input), can be created by calling [method@Gtk.Window.set_modal]
 * on the dialog. When using [ctor@Gtk.Dialog.new_with_buttons], you can also
 * pass the %GTK_DIALOG_MODAL flag to make a dialog modal.
 *
 * For the simple dialog in the following example, a [class@Gtk.MessageDialog]
 * would save some effort. But you’d need to create the dialog contents manually
 * if you had more than a simple message in the dialog.
 *
 * An example for simple `GtkDialog` usage:
 *
 * ```c
 * // Function to open a dialog box with a message
 * void
 * quick_message (GtkWindow *parent, char *message)
 * {
 *  GtkWidget *dialog, *label, *content_area;
 *  GtkDialogFlags flags;
 *
 *  // Create the widgets
 *  flags = GTK_DIALOG_DESTROY_WITH_PARENT;
 *  dialog = gtk_dialog_new_with_buttons ("Message",
 *                                        parent,
 *                                        flags,
 *                                        _("_OK"),
 *                                        GTK_RESPONSE_NONE,
 *                                        NULL);
 *  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
 *  label = gtk_label_new (message);
 *
 *  // Ensure that the dialog box is destroyed when the user responds
 *
 *  g_signal_connect_swapped (dialog,
 *                            "response",
 *                            G_CALLBACK (gtk_window_destroy),
 *                            dialog);
 *
 *  // Add the label, and show everything we’ve added
 *
 *  gtk_box_append (GTK_BOX (content_area), label);
 *  gtk_widget_show (dialog);
 * }
 * ```
 *
 * # GtkDialog as GtkBuildable
 *
 * The `GtkDialog` implementation of the `GtkBuildable` interface exposes the
 * @content_area as an internal child with the name “content_area”.
 *
 * `GtkDialog` supports a custom `<action-widgets>` element, which can contain
 * multiple `<action-widget>` elements. The “response” attribute specifies a
 * numeric response, and the content of the element is the id of widget
 * (which should be a child of the dialogs @action_area). To mark a response
 * as default, set the “default” attribute of the `<action-widget>` element
 * to true.
 *
 * `GtkDialog` supports adding action widgets by specifying “action” as
 * the “type” attribute of a `<child>` element. The widget will be added
 * either to the action area or the headerbar of the dialog, depending
 * on the “use-header-bar” property. The response id has to be associated
 * with the action widget using the `<action-widgets>` element.
 *
 * An example of a `GtkDialog` UI definition fragment:
 *
 * ```xml
 * <object class="GtkDialog" id="dialog1">
 *   <child type="action">
 *     <object class="GtkButton" id="button_cancel"/>
 *   </child>
 *   <child type="action">
 *     <object class="GtkButton" id="button_ok">
 *     </object>
 *   </child>
 *   <action-widgets>
 *     <action-widget response="cancel">button_cancel</action-widget>
 *     <action-widget response="ok" default="true">button_ok</action-widget>
 *   </action-widgets>
 * </object>
 * ```
 *
 * # Accessibility
 *
 * `GtkDialog` uses the %GTK_ACCESSIBLE_ROLE_DIALOG role.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */

typedef struct _ResponseData ResponseData;

typedef struct
{
  GtkWidget *headerbar;
  GtkWidget *action_area;
  GtkWidget *content_area;
  GtkWidget *action_box;
  GtkSizeGroup *size_group;

  int use_header_bar;
  gboolean constructed;
  ResponseData *action_widgets;
} GtkDialogPrivate;

struct _ResponseData
{
  ResponseData *next;
  GtkDialog *dialog;
  GtkWidget *widget;
  int response_id;
};

static void      gtk_dialog_add_buttons_valist   (GtkDialog    *dialog,
                                                  const char   *first_button_text,
                                                  va_list       args);

static gboolean  gtk_dialog_close_request        (GtkWindow    *window);
static void      gtk_dialog_map                  (GtkWidget    *widget);

static void      gtk_dialog_close                (GtkDialog    *dialog);

static ResponseData * get_response_data          (GtkDialog    *dialog,
                                                  GtkWidget    *widget,
                                                  gboolean      create);

static void     gtk_dialog_buildable_interface_init   (GtkBuildableIface  *iface);
static gboolean gtk_dialog_buildable_custom_tag_start (GtkBuildable       *buildable,
                                                       GtkBuilder         *builder,
                                                       GObject            *child,
                                                       const char         *tagname,
                                                       GtkBuildableParser *parser,
                                                       gpointer           *data);
static void     gtk_dialog_buildable_custom_finished  (GtkBuildable       *buildable,
                                                       GtkBuilder         *builder,
                                                       GObject            *child,
                                                       const char         *tagname,
                                                       gpointer            user_data);
static void     gtk_dialog_buildable_add_child        (GtkBuildable       *buildable,
                                                       GtkBuilder         *builder,
                                                       GObject            *child,
                                                       const char         *type);


enum {
  PROP_0,
  PROP_USE_HEADER_BAR
};

enum {
  RESPONSE,
  CLOSE,
  LAST_SIGNAL
};

static guint dialog_signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (GtkDialog, gtk_dialog, GTK_TYPE_WINDOW,
                         G_ADD_PRIVATE (GtkDialog)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_dialog_buildable_interface_init))

static void
set_use_header_bar (GtkDialog *dialog,
                    int        use_header_bar)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (use_header_bar == -1)
    return;

  priv->use_header_bar = use_header_bar;
}

/* A convenience helper for built-in dialogs */
void
gtk_dialog_set_use_header_bar_from_setting (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_assert (!priv->constructed);

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (dialog)),
                "gtk-dialogs-use-header", &priv->use_header_bar,
                NULL);
}

static void
gtk_dialog_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkDialog *dialog = GTK_DIALOG (object);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      set_use_header_bar (dialog, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_dialog_get_property (GObject      *object,
                         guint         prop_id,
                         GValue       *value,
                         GParamSpec   *pspec)
{
  GtkDialog *dialog = GTK_DIALOG (object);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  switch (prop_id)
    {
    case PROP_USE_HEADER_BAR:
      g_value_set_int (value, priv->use_header_bar);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
action_widget_activated (GtkWidget *widget, GtkDialog *dialog)
{
  int response_id;

  response_id = gtk_dialog_get_response_for_widget (dialog, widget);

  gtk_dialog_response (dialog, response_id);
}

static void
add_response_data (GtkDialog *dialog,
                   GtkWidget *child,
                   int        response_id)
{
  ResponseData *ad;
  guint signal_id;

  ad = get_response_data (dialog, child, TRUE);
  ad->response_id = response_id;

  if (GTK_IS_BUTTON (child))
    signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
  else
    signal_id = gtk_widget_class_get_activate_signal (GTK_WIDGET_GET_CLASS (child));

  if (signal_id)
    {
      GClosure *closure;

      closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
                                       G_OBJECT (dialog));
      g_signal_connect_closure_by_id (child, signal_id, 0, closure, FALSE);
    }
  else
    g_warning ("Only 'activatable' widgets can be packed into the action area of a GtkDialog");
}

static void
add_to_header_bar (GtkDialog *dialog,
                   GtkWidget *child,
                   int        response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_HELP)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->headerbar), child);
  else
    gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->headerbar), child);

  gtk_size_group_add_widget (priv->size_group, child);

  if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_CLOSE)
    gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (priv->headerbar), FALSE);
}

static void
apply_response_for_action_area (GtkDialog *dialog,
                                GtkWidget *child,
                                int        response_id)
{
  GtkDialogPrivate *priv G_GNUC_UNUSED = gtk_dialog_get_instance_private (dialog);

  g_assert (gtk_widget_get_parent (child) == priv->action_area);
}

static void
add_to_action_area (GtkDialog *dialog,
                    GtkWidget *child,
                    int        response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  gtk_widget_set_valign (child, GTK_ALIGN_BASELINE);
  gtk_box_append (GTK_BOX (priv->action_area), child);
  apply_response_for_action_area (dialog, child, response_id);
}

static void
update_suggested_action (GtkDialog *dialog,
                         GtkWidget *child)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (priv->use_header_bar)
    {
      if (gtk_widget_has_css_class (child, "default"))
        gtk_widget_add_css_class (child, "suggested-action");
      else
        gtk_widget_remove_css_class (child, "suggested-action");
    }
}

static void
gtk_dialog_constructed (GObject *object)
{
  GtkDialog *dialog = GTK_DIALOG (object);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  G_OBJECT_CLASS (gtk_dialog_parent_class)->constructed (object);

  priv->constructed = TRUE;
  if (priv->use_header_bar == -1)
    priv->use_header_bar = FALSE;

  if (priv->use_header_bar)
    {
      GList *children, *l;
      GtkWidget *child;

      children = NULL;
      for (child = gtk_widget_get_first_child (priv->action_area);
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        children = g_list_append (children, child);

      for (l = children; l != NULL; l = l->next)
        {
          gboolean has_default;
          ResponseData *rd;
          int response_id;

          child = l->data;

          has_default = gtk_widget_has_default (child);
          rd = get_response_data (dialog, child, FALSE);
          response_id = rd ? rd->response_id : GTK_RESPONSE_NONE;

          g_object_ref (child);
          gtk_box_remove (GTK_BOX (priv->action_area), child);
          add_to_header_bar (dialog, child, response_id);
          g_object_unref (child);

          if (has_default)
            {
              gtk_window_set_default_widget (GTK_WINDOW (dialog), child);
              update_suggested_action (dialog, child);
            }
        }
      g_list_free (children);

      if (GTK_IS_HEADER_BAR (priv->headerbar))
        _gtk_header_bar_track_default_decoration (GTK_HEADER_BAR (priv->headerbar));
    }
  else
    {
      gtk_window_set_titlebar (GTK_WINDOW (dialog), NULL);
      priv->headerbar = NULL;
    }

  gtk_widget_set_visible (priv->action_box, !priv->use_header_bar);
}

static void
gtk_dialog_finalize (GObject *obj)
{
  GtkDialog *dialog = GTK_DIALOG (obj);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  while (priv->action_widgets)
    g_object_set_data (G_OBJECT (priv->action_widgets->widget),
                       "gtk-dialog-response-data", NULL);

  g_object_unref (priv->size_group);

  G_OBJECT_CLASS (gtk_dialog_parent_class)->finalize (obj);
}

static void
gtk_dialog_class_init (GtkDialogClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkWindowClass *window_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = GTK_WIDGET_CLASS (class);
  window_class = GTK_WINDOW_CLASS (class);

  gobject_class->constructed  = gtk_dialog_constructed;
  gobject_class->set_property = gtk_dialog_set_property;
  gobject_class->get_property = gtk_dialog_get_property;
  gobject_class->finalize = gtk_dialog_finalize;

  widget_class->map = gtk_dialog_map;

  window_class->close_request = gtk_dialog_close_request;

  class->close = gtk_dialog_close;

  /**
   * GtkDialog::response:
   * @dialog: the object on which the signal is emitted
   * @response_id: the response ID
   *
   * Emitted when an action widget is clicked.
   *
   * The signal is also emitted when the dialog receives a
   * delete event, and when [method@Gtk.Dialog.response] is called.
   * On a delete event, the response ID is %GTK_RESPONSE_DELETE_EVENT.
   * Otherwise, it depends on which action widget was clicked.
   *
   * Deprecated: 4.10: Use [class@Gtk.Window] instead
   */
  dialog_signals[RESPONSE] =
    g_signal_new (I_("response"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkDialogClass, response),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 1,
		  G_TYPE_INT);

  /**
   * GtkDialog::close:
   *
   * Emitted when the user uses a keybinding to close the dialog.
   *
   * This is a [keybinding signal](class.SignalAction.html).
   *
   * The default binding for this signal is the Escape key.
   *
   * Deprecated: 4.10: Use [class@Gtk.Window] instead
   */
  dialog_signals[CLOSE] =
    g_signal_new (I_("close"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkDialogClass, close),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  /**
   * GtkDialog:use-header-bar:
   *
   * %TRUE if the dialog uses a headerbar for action buttons
   * instead of the action-area.
   *
   * For technical reasons, this property is declared as an integer
   * property, but you should only set it to %TRUE or %FALSE.
   *
   * ## Creating a dialog with headerbar
   *
   * Builtin `GtkDialog` subclasses such as [class@Gtk.ColorChooserDialog]
   * set this property according to platform conventions (using the
   * [property@Gtk.Settings:gtk-dialogs-use-header] setting).
   *
   * Here is how you can achieve the same:
   *
   * ```c
   * g_object_get (settings, "gtk-dialogs-use-header", &header, NULL);
   * dialog = g_object_new (GTK_TYPE_DIALOG, header, TRUE, NULL);
   * ```
   *
   * Deprecated: 4.10: Use [class@Gtk.Window] instead
   */
  g_object_class_install_property (gobject_class,
                                   PROP_USE_HEADER_BAR,
                                   g_param_spec_int ("use-header-bar", NULL, NULL,
                                                     -1, 1, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Escape, 0, "close", NULL);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkdialog.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, headerbar);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, action_area);
  gtk_widget_class_bind_template_child_internal_private (widget_class, GtkDialog, content_area);
  gtk_widget_class_bind_template_child_private (widget_class, GtkDialog, action_box);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_DIALOG);
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  gtk_widget_add_css_class (GTK_WIDGET (dialog), "dialog");

  priv->use_header_bar = -1;
  priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  gtk_widget_init_template (GTK_WIDGET (dialog));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_dialog_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = gtk_dialog_buildable_custom_tag_start;
  iface->custom_finished = gtk_dialog_buildable_custom_finished;
  iface->add_child = gtk_dialog_buildable_add_child;
}

static gboolean
gtk_dialog_close_request (GtkWindow *window)
{
  gtk_dialog_response (GTK_DIALOG (window), GTK_RESPONSE_DELETE_EVENT);

  return GTK_WINDOW_CLASS (gtk_dialog_parent_class)->close_request (window);
}

/* A far too tricky heuristic for getting the right initial
 * focus widget if none was set. What we do is we focus the first
 * widget in the tab chain, but if this results in the focus
 * ending up on one of the response widgets _other_ than the
 * default response, we focus the default response instead.
 *
 * Additionally, skip selectable labels when looking for the
 * right initial focus widget.
 */
static void
gtk_dialog_map (GtkWidget *widget)
{
  GtkWidget *default_widget, *focus;
  GtkWindow *window = GTK_WINDOW (widget);
  GtkDialog *dialog = GTK_DIALOG (widget);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  ResponseData *rd;

  if (gtk_window_get_transient_for (window) == NULL)
    g_message ("GtkDialog mapped without a transient parent. This is discouraged.");

  GTK_WIDGET_CLASS (gtk_dialog_parent_class)->map (widget);

  focus = gtk_window_get_focus (window);
  if (!focus)
    {
      GtkWidget *first_focus = NULL;

      do
        {
          g_signal_emit_by_name (window, "move_focus", GTK_DIR_TAB_FORWARD);

          focus = gtk_window_get_focus (window);
          if (GTK_IS_LABEL (focus) &&
              !gtk_label_get_current_uri (GTK_LABEL (focus)))
            gtk_label_select_region (GTK_LABEL (focus), 0, 0);

          if (first_focus == NULL)
            first_focus = focus;
          else if (first_focus == focus)
            break;

          if (!GTK_IS_LABEL (focus))
            break;
        }
      while (TRUE);

      default_widget = gtk_window_get_default_widget (window);
      for (rd = priv->action_widgets; rd != NULL; rd = rd->next)
        {
          if ((focus == NULL || rd->widget == focus) &&
               rd->widget != default_widget &&
               default_widget)
            {
              gtk_widget_grab_focus (default_widget);
              break;
            }
        }
    }
}

static void
gtk_dialog_close (GtkDialog *dialog)
{
  gtk_window_close (GTK_WINDOW (dialog));
}

/**
 * gtk_dialog_new:
 *
 * Creates a new dialog box.
 *
 * Widgets should not be packed into the `GtkWindow`
 * directly, but into the @content_area and @action_area,
 * as described above.
 *
 * Returns: the new dialog as a `GtkWidget`
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
GtkWidget*
gtk_dialog_new (void)
{
  return g_object_new (GTK_TYPE_DIALOG, NULL);
}

static GtkWidget*
gtk_dialog_new_empty (const char      *title,
                      GtkWindow       *parent,
                      GtkDialogFlags   flags)
{
  GtkDialog *dialog;

  dialog = g_object_new (GTK_TYPE_DIALOG,
                         "use-header-bar", (flags & GTK_DIALOG_USE_HEADER_BAR) != 0,
                         NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (dialog), title);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  if (flags & GTK_DIALOG_MODAL)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (flags & GTK_DIALOG_DESTROY_WITH_PARENT)
    gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

  return GTK_WIDGET (dialog);
}

/**
 * gtk_dialog_new_with_buttons:
 * @title: (nullable): Title of the dialog
 * @parent: (nullable): Transient parent of the dialog
 * @flags: from `GtkDialogFlags`
 * @first_button_text: (nullable): text to go in first button
 * @...: response ID for first button, then additional buttons, ending with %NULL
 *
 * Creates a new `GtkDialog` with the given title and transient parent.
 *
 * The @flags argument can be used to make the dialog modal, have it
 * destroyed along with its transient parent, or make it use a headerbar.
 *
 * Button text/response ID pairs should be listed in pairs, with a %NULL
 * pointer ending the list. Button text can be arbitrary text. A response
 * ID can be any positive number, or one of the values in the
 * [enum@Gtk.ResponseType] enumeration. If the user clicks one of these
 * buttons, `GtkDialog` will emit the [signal@Gtk.Dialog::response] signal
 * with the corresponding response ID.
 *
 * If a `GtkDialog` receives a delete event, it will emit ::response with a
 * response ID of %GTK_RESPONSE_DELETE_EVENT.
 *
 * However, destroying a dialog does not emit the ::response signal;
 * so be careful relying on ::response when using the
 * %GTK_DIALOG_DESTROY_WITH_PARENT flag.
 *
 * Here’s a simple example:
 * ```c
 * GtkWindow *main_app_window; // Window the dialog should show up on
 * GtkWidget *dialog;
 * GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
 * dialog = gtk_dialog_new_with_buttons ("My dialog",
 *                                       main_app_window,
 *                                       flags,
 *                                       _("_OK"),
 *                                       GTK_RESPONSE_ACCEPT,
 *                                       _("_Cancel"),
 *                                       GTK_RESPONSE_REJECT,
 *                                       NULL);
 * ```
 *
 * Returns: a new `GtkDialog`
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
GtkWidget*
gtk_dialog_new_with_buttons (const char     *title,
                             GtkWindow      *parent,
                             GtkDialogFlags  flags,
                             const char     *first_button_text,
                             ...)
{
  GtkDialog *dialog;
  va_list args;

  dialog = GTK_DIALOG (gtk_dialog_new_empty (title, parent, flags));

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);

  va_end (args);

  return GTK_WIDGET (dialog);
}

static void
response_data_free (gpointer data)
{
  ResponseData *ad = data;
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (ad->dialog);

  if (priv->action_widgets == ad)
    {
      priv->action_widgets = ad->next;
    }
  else
    {
      ResponseData *prev = priv->action_widgets;
      while (prev)
        {
          if (prev->next == ad)
            {
              prev->next = ad->next;
              break;
            }
          prev = prev->next;
        }
    }
  g_slice_free (ResponseData, data);
}

static ResponseData *
get_response_data (GtkDialog *dialog,
                   GtkWidget *widget,
                   gboolean   create)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  ResponseData *ad = g_object_get_data (G_OBJECT (widget),
                                        "gtk-dialog-response-data");

  if (ad == NULL && create)
    {
      ad = g_slice_new (ResponseData);
      ad->dialog = dialog;
      ad->widget = widget;
      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-dialog-response-data"),
                              ad,
                              response_data_free);
      ad->next = priv->action_widgets;
      priv->action_widgets = ad;
    }

  return ad;
}

/**
 * gtk_dialog_add_action_widget:
 * @dialog: a `GtkDialog`
 * @child: an activatable widget
 * @response_id: response ID for @child
 *
 * Adds an activatable widget to the action area of a `GtkDialog`.
 *
 * GTK connects a signal handler that will emit the
 * [signal@Gtk.Dialog::response] signal on the dialog when the widget
 * is activated. The widget is appended to the end of the dialog’s action
 * area.
 *
 * If you want to add a non-activatable widget, simply pack it into
 * the @action_area field of the `GtkDialog` struct.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
void
gtk_dialog_add_action_widget (GtkDialog *dialog,
                              GtkWidget *child,
                              int        response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_if_fail (GTK_IS_DIALOG (dialog));
  g_return_if_fail (GTK_IS_WIDGET (child));

  add_response_data (dialog, child, response_id);

  if (priv->constructed && priv->use_header_bar)
    {
      add_to_header_bar (dialog, child, response_id);

      if (gtk_widget_has_default (child))
        {
          gtk_window_set_default_widget (GTK_WINDOW (dialog), child);
          update_suggested_action (dialog, child);
        }
    }
  else
    add_to_action_area (dialog, child, response_id);
}

/**
 * gtk_dialog_add_button:
 * @dialog: a `GtkDialog`
 * @button_text: text of button
 * @response_id: response ID for the button
 *
 * Adds a button with the given text.
 *
 * GTK arranges things so that clicking the button will emit the
 * [signal@Gtk.Dialog::response] signal with the given @response_id.
 * The button is appended to the end of the dialog’s action area.
 * The button widget is returned, but usually you don’t need it.
 *
 * Returns: (transfer none): the `GtkButton` widget that was added
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
GtkWidget*
gtk_dialog_add_button (GtkDialog   *dialog,
                       const char *button_text,
                       int          response_id)
{
  GtkWidget *button;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);
  g_return_val_if_fail (button_text != NULL, NULL);

  button = gtk_button_new_with_label (button_text);
  gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);

  gtk_dialog_add_action_widget (dialog, button, response_id);

  return button;
}

static void
gtk_dialog_add_buttons_valist (GtkDialog      *dialog,
                               const char     *first_button_text,
                               va_list         args)
{
  const char * text;
  int response_id;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  if (first_button_text == NULL)
    return;

  text = first_button_text;
  response_id = va_arg (args, int);

  while (text != NULL)
    {
      gtk_dialog_add_button (dialog, text, response_id);

      text = va_arg (args, char *);
      if (text == NULL)
        break;
      response_id = va_arg (args, int);
    }
}

/**
 * gtk_dialog_add_buttons:
 * @dialog: a `GtkDialog`
 * @first_button_text: button text
 * @...: response ID for first button, then more text-response_id pairs
 *
 * Adds multiple buttons.
 *
 * This is the same as calling [method@Gtk.Dialog.add_button]
 * repeatedly. The variable argument list should be %NULL-terminated
 * as with [ctor@Gtk.Dialog.new_with_buttons]. Each button must have both
 * text and response ID.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
void
gtk_dialog_add_buttons (GtkDialog   *dialog,
                        const char *first_button_text,
                        ...)
{
  va_list args;

  va_start (args, first_button_text);

  gtk_dialog_add_buttons_valist (dialog,
                                 first_button_text,
                                 args);

  va_end (args);
}

/**
 * gtk_dialog_set_response_sensitive:
 * @dialog: a `GtkDialog`
 * @response_id: a response ID
 * @setting: %TRUE for sensitive
 *
 * A convenient way to sensitize/desensitize dialog buttons.
 *
 * Calls `gtk_widget_set_sensitive (widget, @setting)`
 * for each widget in the dialog’s action area with the given @response_id.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
void
gtk_dialog_set_response_sensitive (GtkDialog *dialog,
                                   int        response_id,
                                   gboolean   setting)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  ResponseData *rd;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  for (rd = priv->action_widgets; rd != NULL; rd = rd->next)
    {
      if (rd->response_id == response_id)
        gtk_widget_set_sensitive (rd->widget, setting);
    }
}

/**
 * gtk_dialog_set_default_response:
 * @dialog: a `GtkDialog`
 * @response_id: a response ID
 *
 * Sets the default widget for the dialog based on the response ID.
 *
 * Pressing “Enter” normally activates the default widget.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
void
gtk_dialog_set_default_response (GtkDialog *dialog,
                                 int        response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  ResponseData *rd;

  g_return_if_fail (GTK_IS_DIALOG (dialog));

  for (rd = priv->action_widgets; rd != NULL; rd = rd->next)
    {
      if (rd->response_id == response_id)
        {
          gtk_window_set_default_widget (GTK_WINDOW (dialog), rd->widget);
          update_suggested_action (dialog, rd->widget);
        }
    }
}

/**
 * gtk_dialog_response: (attributes org.gtk.Method.signal=response)
 * @dialog: a `GtkDialog`
 * @response_id: response ID
 *
 * Emits the ::response signal with the given response ID.
 *
 * Used to indicate that the user has responded to the dialog in some way.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
void
gtk_dialog_response (GtkDialog *dialog,
                     int        response_id)
{
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  g_signal_emit (dialog,
		 dialog_signals[RESPONSE],
		 0,
		 response_id);
}

/**
 * gtk_dialog_get_widget_for_response:
 * @dialog: a `GtkDialog`
 * @response_id: the response ID used by the @dialog widget
 *
 * Gets the widget button that uses the given response ID in the action area
 * of a dialog.
 *
 * Returns: (nullable) (transfer none): the @widget button that uses the given
 *   @response_id
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
GtkWidget*
gtk_dialog_get_widget_for_response (GtkDialog *dialog,
                                    int        response_id)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  ResponseData *rd;

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  for (rd = priv->action_widgets; rd != NULL; rd = rd->next)
    {
      if (rd->response_id == response_id)
        return rd->widget;
    }

  return NULL;
}

/**
 * gtk_dialog_get_response_for_widget:
 * @dialog: a `GtkDialog`
 * @widget: a widget in the action area of @dialog
 *
 * Gets the response id of a widget in the action area
 * of a dialog.
 *
 * Returns: the response id of @widget, or %GTK_RESPONSE_NONE
 *  if @widget doesn’t have a response id set.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
int
gtk_dialog_get_response_for_widget (GtkDialog *dialog,
				    GtkWidget *widget)
{
  ResponseData *rd;

  rd = get_response_data (dialog, widget, FALSE);
  if (!rd)
    return GTK_RESPONSE_NONE;
  else
    return rd->response_id;
}

typedef struct {
  char *widget_name;
  int response_id;
  gboolean is_default;
  int line;
  int col;
} ActionWidgetInfo;

typedef struct {
  GtkDialog *dialog;
  GtkBuilder *builder;
  GSList *items;
  int response_id;
  gboolean is_default;
  gboolean is_text;
  GString *string;
  gboolean in_action_widgets;
  int line;
  int col;
} SubParserData;

static void
free_action_widget_info (gpointer data)
{
  ActionWidgetInfo *item = data;

  g_free (item->widget_name);
  g_free (item);
}

static void
parser_start_element (GtkBuildableParseContext *context,
                      const char               *element_name,
                      const char              **names,
                      const char              **values,
                      gpointer                  user_data,
                      GError                  **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (strcmp (element_name, "action-widget") == 0)
    {
      const char *response;
      gboolean is_default = FALSE;
      GValue gvalue = G_VALUE_INIT;

      if (!_gtk_builder_check_parent (data->builder, context, "action-widgets", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_STRING, "response", &response,
                                        G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "default", &is_default,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      if (!gtk_builder_value_from_string_type (data->builder, GTK_TYPE_RESPONSE_TYPE, response, &gvalue, error))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      data->response_id = g_value_get_enum (&gvalue);
      data->is_default = is_default;
      data->is_text = TRUE;
      g_string_set_size (data->string, 0);
      gtk_buildable_parse_context_get_position (context, &data->line, &data->col);
    }
  else if (strcmp (element_name, "action-widgets") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, names, values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);

      data->in_action_widgets = TRUE;
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkDialog", element_name,
                                        error);
    }
}

static void
parser_text_element (GtkBuildableParseContext *context,
                     const char               *text,
                     gsize                     text_len,
                     gpointer                  user_data,
                     GError                  **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->is_text)
    g_string_append_len (data->string, text, text_len);
}

static void
parser_end_element (GtkBuildableParseContext  *context,
                    const char                *element_name,
                    gpointer                   user_data,
                    GError                   **error)
{
  SubParserData *data = (SubParserData*)user_data;

  if (data->is_text)
    {
      ActionWidgetInfo *item;

      item = g_new (ActionWidgetInfo, 1);
      item->widget_name = g_strdup (data->string->str);
      item->response_id = data->response_id;
      item->is_default = data->is_default;
      item->line = data->line;
      item->col = data->col;

      data->items = g_slist_prepend (data->items, item);
      data->is_default = FALSE;
      data->is_text = FALSE;
    }
}

static const GtkBuildableParser sub_parser =
  {
    parser_start_element,
    parser_end_element,
    parser_text_element,
  };

static gboolean
gtk_dialog_buildable_custom_tag_start (GtkBuildable       *buildable,
                                       GtkBuilder         *builder,
                                       GObject            *child,
                                       const char         *tagname,
                                       GtkBuildableParser *parser,
                                       gpointer           *parser_data)
{
  SubParserData *data;

  if (child)
    return FALSE;

  if (strcmp (tagname, "action-widgets") == 0)
    {
      data = g_slice_new0 (SubParserData);
      data->dialog = GTK_DIALOG (buildable);
      data->builder = builder;
      data->string = g_string_new ("");
      data->items = NULL;
      data->in_action_widgets = FALSE;

      *parser = sub_parser;
      *parser_data = data;
      return TRUE;
    }

  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
						   tagname, parser, parser_data);
}

static void
gtk_dialog_buildable_custom_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder,
				      GObject      *child,
				      const char   *tagname,
				      gpointer      user_data)
{
  GtkDialog *dialog = GTK_DIALOG (buildable);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);
  GSList *l;
  SubParserData *data;
  GObject *object;
  ResponseData *ad;
  guint signal_id;

  if (strcmp (tagname, "action-widgets") != 0)
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  data = (SubParserData*)user_data;
  data->items = g_slist_reverse (data->items);

  for (l = data->items; l; l = l->next)
    {
      ActionWidgetInfo *item = l->data;
      gboolean is_action;

      object = _gtk_builder_lookup_object (builder, item->widget_name, item->line, item->col);
      if (!object)
        continue;

      /* If the widget already has response data at this point, it
       * was either added by gtk_dialog_add_action_widget(), or via
       * <child type="action"> or by moving an action area child
       * to the header bar. In these cases, apply placement heuristics
       * based on the response id.
       */
      is_action = get_response_data (dialog, GTK_WIDGET (object), FALSE) != NULL;

      ad = get_response_data (dialog, GTK_WIDGET (object), TRUE);
      ad->response_id = item->response_id;

      if (GTK_IS_BUTTON (object))
	signal_id = g_signal_lookup ("clicked", GTK_TYPE_BUTTON);
      else
	signal_id = gtk_widget_class_get_activate_signal (GTK_WIDGET_GET_CLASS (object));

      if (signal_id && !is_action)
	{
	  GClosure *closure;

	  closure = g_cclosure_new_object (G_CALLBACK (action_widget_activated),
					   G_OBJECT (dialog));
	  g_signal_connect_closure_by_id (object, signal_id, 0, closure, FALSE);
	}

      if (gtk_widget_get_parent (GTK_WIDGET (object)) == priv->action_area)
        {
          apply_response_for_action_area (dialog, GTK_WIDGET (object), ad->response_id);
        }
      else if (gtk_widget_get_ancestor (GTK_WIDGET (object), GTK_TYPE_HEADER_BAR) == priv->headerbar)
        {
          if (is_action)
            {
              g_object_ref (object);
              gtk_header_bar_remove (GTK_HEADER_BAR (priv->headerbar), GTK_WIDGET (object));
              add_to_header_bar (dialog, GTK_WIDGET (object), ad->response_id);
              g_object_unref (object);
            }
        }

      if (item->is_default)
        {
          gtk_window_set_default_widget (GTK_WINDOW (dialog), GTK_WIDGET (object));
          update_suggested_action (dialog, GTK_WIDGET (object));
        }
    }

  g_slist_free_full (data->items, free_action_widget_info);
  g_string_free (data->string, TRUE);
  g_slice_free (SubParserData, data);
}

static void
gtk_dialog_buildable_add_child (GtkBuildable  *buildable,
                                GtkBuilder    *builder,
                                GObject       *child,
                                const char    *type)
{
  GtkDialog *dialog = GTK_DIALOG (buildable);
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  if (type == NULL)
    parent_buildable_iface->add_child (buildable, builder, child, type);
  else if (g_str_equal (type, "titlebar"))
    {
      priv->headerbar = GTK_WIDGET (child);
      if (GTK_IS_HEADER_BAR (priv->headerbar))
        _gtk_header_bar_track_default_decoration (GTK_HEADER_BAR (priv->headerbar));
      gtk_window_set_titlebar (GTK_WINDOW (buildable), priv->headerbar);
    }
  else if (g_str_equal (type, "action"))
    gtk_dialog_add_action_widget (GTK_DIALOG (buildable), GTK_WIDGET (child), GTK_RESPONSE_NONE);
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

GtkWidget *
gtk_dialog_get_action_area (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return priv->action_area;
}

/**
 * gtk_dialog_get_header_bar:
 * @dialog: a `GtkDialog`
 *
 * Returns the header bar of @dialog.
 *
 * Note that the headerbar is only used by the dialog if the
 * [property@Gtk.Dialog:use-header-bar] property is %TRUE.
 *
 * Returns: (type Gtk.HeaderBar) (transfer none): the header bar
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
GtkWidget *
gtk_dialog_get_header_bar (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return priv->headerbar;
}

/**
 * gtk_dialog_get_content_area:
 * @dialog: a `GtkDialog`
 *
 * Returns the content area of @dialog.
 *
 * Returns: (type Gtk.Box) (transfer none): the content area `GtkBox`.
 *
 * Deprecated: 4.10: Use [class@Gtk.Window] instead
 */
GtkWidget *
gtk_dialog_get_content_area (GtkDialog *dialog)
{
  GtkDialogPrivate *priv = gtk_dialog_get_instance_private (dialog);

  g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

  return priv->content_area;
}
