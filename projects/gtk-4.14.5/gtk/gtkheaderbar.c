/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "gtkheaderbarprivate.h"

#include "gtkbinlayout.h"
#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkcenterbox.h"
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowcontrols.h"
#include "gtkwindowhandle.h"

#include <string.h>

/**
 * GtkHeaderBar:
 *
 * `GtkHeaderBar` is a widget for creating custom title bars for windows.
 *
 * ![An example GtkHeaderBar](headerbar.png)
 *
 * `GtkHeaderBar` is similar to a horizontal `GtkCenterBox`. It allows
 * children to be placed at the start or the end. In addition, it allows
 * the window title to be displayed. The title will be centered with respect
 * to the width of the box, even if the children at either side take up
 * different amounts of space.
 *
 * `GtkHeaderBar` can add typical window frame controls, such as minimize,
 * maximize and close buttons, or the window icon.
 *
 * For these reasons, `GtkHeaderBar` is the natural choice for use as the
 * custom titlebar widget of a `GtkWindow` (see [method@Gtk.Window.set_titlebar]),
 * as it gives features typical of titlebars while allowing the addition of
 * child widgets.
 *
 * ## GtkHeaderBar as GtkBuildable
 *
 * The `GtkHeaderBar` implementation of the `GtkBuildable` interface supports
 * adding children at the start or end sides by specifying “start” or “end” as
 * the “type” attribute of a `<child>` element, or setting the title widget by
 * specifying “title” value.
 *
 * By default the `GtkHeaderBar` uses a `GtkLabel` displaying the title of the
 * window it is contained in as the title widget, equivalent to the following
 * UI definition:
 *
 * ```xml
 * <object class="GtkHeaderBar">
 *   <property name="title-widget">
 *     <object class="GtkLabel">
 *       <property name="label" translatable="yes">Label</property>
 *       <property name="single-line-mode">True</property>
 *       <property name="ellipsize">end</property>
 *       <property name="width-chars">5</property>
 *       <style>
 *         <class name="title"/>
 *       </style>
 *     </object>
 *   </property>
 * </object>
 * ```
 *
 * # CSS nodes
 *
 * ```
 * headerbar
 * ╰── windowhandle
 *     ╰── box
 *         ├── box.start
 *         │   ├── windowcontrols.start
 *         │   ╰── [other children]
 *         ├── [Title Widget]
 *         ╰── box.end
 *             ├── [other children]
 *             ╰── windowcontrols.end
 * ```
 *
 * A `GtkHeaderBar`'s CSS node is called `headerbar`. It contains a `windowhandle`
 * subnode, which contains a `box` subnode, which contains two `box` subnodes at
 * the start and end of the header bar, as well as a center node that represents
 * the title.
 *
 * Each of the boxes contains a `windowcontrols` subnode, see
 * [class@Gtk.WindowControls] for details, as well as other children.
 *
 * # Accessibility
 *
 * `GtkHeaderBar` uses the %GTK_ACCESSIBLE_ROLE_GROUP role.
 */

#define MIN_TITLE_CHARS 5

struct _GtkHeaderBar
{
  GtkWidget container;

  GtkWidget *handle;
  GtkWidget *center_box;
  GtkWidget *start_box;
  GtkWidget *end_box;

  GtkWidget *title_label;
  GtkWidget *title_widget;

  GtkWidget *start_window_controls;
  GtkWidget *end_window_controls;

  char *decoration_layout;

  guint show_title_buttons : 1;
  guint track_default_decoration : 1;
};

typedef struct _GtkHeaderBarClass GtkHeaderBarClass;

struct _GtkHeaderBarClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_TITLE_WIDGET,
  PROP_SHOW_TITLE_BUTTONS,
  PROP_DECORATION_LAYOUT,
  LAST_PROP
};

static GParamSpec *header_bar_props[LAST_PROP] = { NULL, };

static void gtk_header_bar_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkHeaderBar, gtk_header_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_header_bar_buildable_init));

static void
create_window_controls (GtkHeaderBar *bar)
{
  GtkWidget *controls;

  controls = gtk_window_controls_new (GTK_PACK_START);
  g_object_bind_property (bar, "decoration-layout",
                          controls, "decoration-layout",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (controls, "empty",
                          controls, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_box_prepend (GTK_BOX (bar->start_box), controls);
  bar->start_window_controls = controls;

  controls = gtk_window_controls_new (GTK_PACK_END);
  g_object_bind_property (bar, "decoration-layout",
                          controls, "decoration-layout",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (controls, "empty",
                          controls, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  gtk_box_append (GTK_BOX (bar->end_box), controls);
  bar->end_window_controls = controls;
}

static void
update_default_decoration (GtkHeaderBar *bar)
{
  gboolean have_children = FALSE;

  /* Check whether we have any child widgets that we didn't add ourselves */
  if (gtk_center_box_get_center_widget (GTK_CENTER_BOX (bar->center_box)) != NULL)
    {
      have_children = TRUE;
    }
  else
    {
      GtkWidget *w;

      for (w = _gtk_widget_get_first_child (bar->start_box);
           w != NULL;
           w = _gtk_widget_get_next_sibling (w))
        {
          if (w != bar->start_window_controls)
            {
              have_children = TRUE;
              break;
            }
        }

      if (!have_children)
        for (w = _gtk_widget_get_first_child (bar->end_box);
             w != NULL;
             w = _gtk_widget_get_next_sibling (w))
          {
            if (w != bar->end_window_controls)
              {
                have_children = TRUE;
                break;
              }
          }
    }

  if (have_children || bar->title_widget != NULL)
    gtk_widget_remove_css_class (GTK_WIDGET (bar), "default-decoration");
  else
    gtk_widget_add_css_class (GTK_WIDGET (bar), "default-decoration");
}

void
_gtk_header_bar_track_default_decoration (GtkHeaderBar *bar)
{
  g_assert (GTK_IS_HEADER_BAR (bar));

  bar->track_default_decoration = TRUE;

  update_default_decoration (bar);
}

static void
update_title (GtkHeaderBar *bar)
{
  GtkRoot *root;
  const char *title = NULL;

  if (!bar->title_label)
    return;

  root = gtk_widget_get_root (GTK_WIDGET (bar));

  if (GTK_IS_WINDOW (root))
    title = gtk_window_get_title (GTK_WINDOW (root));

  if (!title)
    title = g_get_application_name ();

  if (!title)
    title = g_get_prgname ();

  gtk_label_set_text (GTK_LABEL (bar->title_label), title);
}

static void
construct_title_label (GtkHeaderBar *bar)
{
  GtkWidget *label;

  g_assert (bar->title_label == NULL);

  label = gtk_label_new (NULL);
  gtk_widget_add_css_class (label, "title");
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_label_set_wrap (GTK_LABEL (label), FALSE);
  gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), MIN_TITLE_CHARS);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (bar->center_box), label);

  bar->title_label = label;

  update_title (bar);
}

/**
 * gtk_header_bar_set_title_widget:
 * @bar: a `GtkHeaderBar`
 * @title_widget: (nullable): a widget to use for a title
 *
 * Sets the title for the `GtkHeaderBar`.
 *
 * When set to %NULL, the headerbar will display the title of
 * the window it is contained in.
 *
 * The title should help a user identify the current view.
 * To achieve the same style as the builtin title, use the
 * “title” style class.
 *
 * You should set the title widget to %NULL, for the window
 * title label to be visible again.
 */
void
gtk_header_bar_set_title_widget (GtkHeaderBar *bar,
                                 GtkWidget    *title_widget)
{
  g_return_if_fail (GTK_IS_HEADER_BAR (bar));
  g_return_if_fail (title_widget == NULL || bar->title_widget == title_widget || gtk_widget_get_parent (title_widget) == NULL);

  /* No need to do anything if the title widget stays the same */
  if (bar->title_widget == title_widget)
    return;

  gtk_center_box_set_center_widget (GTK_CENTER_BOX (bar->center_box), NULL);
  bar->title_widget = NULL;

  if (title_widget != NULL)
    {
      bar->title_widget = title_widget;

      gtk_center_box_set_center_widget (GTK_CENTER_BOX (bar->center_box), title_widget);

      bar->title_label = NULL;
    }
  else
    {
      if (bar->title_label == NULL)
        construct_title_label (bar);
    }

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_TITLE_WIDGET]);
}

/**
 * gtk_header_bar_get_title_widget:
 * @bar: a `GtkHeaderBar`
 *
 * Retrieves the title widget of the header.
 *
 * See [method@Gtk.HeaderBar.set_title_widget].
 *
 * Returns: (nullable) (transfer none): the title widget of the header
 */
GtkWidget *
gtk_header_bar_get_title_widget (GtkHeaderBar *bar)
{
  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return bar->title_widget;
}

static void
gtk_header_bar_root (GtkWidget *widget)
{
  GtkWidget *root;

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->root (widget);

  root = GTK_WIDGET (gtk_widget_get_root (widget));

  if (GTK_IS_WINDOW (root))
    g_signal_connect_swapped (root, "notify::title",
                              G_CALLBACK (update_title), widget);

  update_title (GTK_HEADER_BAR (widget));
}

static void
gtk_header_bar_unroot (GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func (gtk_widget_get_root (widget),
                                        update_title, widget);

  GTK_WIDGET_CLASS (gtk_header_bar_parent_class)->unroot (widget);
}

static void
gtk_header_bar_dispose (GObject *object)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);

  bar->title_widget = NULL;
  bar->title_label = NULL;
  bar->start_box = NULL;
  bar->end_box = NULL;

  g_clear_pointer (&bar->handle, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_header_bar_parent_class)->dispose (object);
}

static void
gtk_header_bar_finalize (GObject *object)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);

  g_free (bar->decoration_layout);

  G_OBJECT_CLASS (gtk_header_bar_parent_class)->finalize (object);
}

static void
gtk_header_bar_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);

  switch (prop_id)
    {
    case PROP_TITLE_WIDGET:
      g_value_set_object (value, bar->title_widget);
      break;

    case PROP_SHOW_TITLE_BUTTONS:
      g_value_set_boolean (value, gtk_header_bar_get_show_title_buttons (bar));
      break;

    case PROP_DECORATION_LAYOUT:
      g_value_set_string (value, gtk_header_bar_get_decoration_layout (bar));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_header_bar_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkHeaderBar *bar = GTK_HEADER_BAR (object);

  switch (prop_id)
    {
    case PROP_TITLE_WIDGET:
      gtk_header_bar_set_title_widget (bar, g_value_get_object (value));
      break;

    case PROP_SHOW_TITLE_BUTTONS:
      gtk_header_bar_set_show_title_buttons (bar, g_value_get_boolean (value));
      break;

    case PROP_DECORATION_LAYOUT:
      gtk_header_bar_set_decoration_layout (bar, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_header_bar_pack (GtkHeaderBar *bar,
                     GtkWidget    *widget,
                     GtkPackType   pack_type)
{
  g_return_if_fail (gtk_widget_get_parent (widget) == NULL);

  if (pack_type == GTK_PACK_START)
    {
      gtk_box_append (GTK_BOX (bar->start_box), widget);
    }
  else if (pack_type == GTK_PACK_END)
    {
      gtk_box_prepend (GTK_BOX (bar->end_box), widget);
    }

  if (bar->track_default_decoration)
    update_default_decoration (bar);
}

/**
 * gtk_header_bar_remove:
 * @bar: a `GtkHeaderBar`
 * @child: the child to remove
 *
 * Removes a child from the `GtkHeaderBar`.
 *
 * The child must have been added with
 * [method@Gtk.HeaderBar.pack_start],
 * [method@Gtk.HeaderBar.pack_end] or
 * [method@Gtk.HeaderBar.set_title_widget].
 */
void
gtk_header_bar_remove (GtkHeaderBar *bar,
                       GtkWidget    *child)
{
  GtkWidget *parent;
  gboolean removed = FALSE;

  parent = gtk_widget_get_parent (child);

  if (parent == bar->start_box)
    {
      gtk_box_remove (GTK_BOX (bar->start_box), child);
      removed = TRUE;
    }
  else if (parent == bar->end_box)
    {
      gtk_box_remove (GTK_BOX (bar->end_box), child);
      removed = TRUE;
    }
  else if (parent == bar->center_box)
    {
      gtk_center_box_set_center_widget (GTK_CENTER_BOX (bar->center_box), NULL);
      removed = TRUE;
    }

  if (removed && bar->track_default_decoration)
    update_default_decoration (bar);
}

static GtkSizeRequestMode
gtk_header_bar_get_request_mode (GtkWidget *widget)
{
  GtkWidget *w;
  int wfh = 0, hfw = 0;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      GtkSizeRequestMode mode = gtk_widget_get_request_mode (w);

      switch (mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw ++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh ++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_header_bar_class_init (GtkHeaderBarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_header_bar_dispose;
  object_class->finalize = gtk_header_bar_finalize;
  object_class->get_property = gtk_header_bar_get_property;
  object_class->set_property = gtk_header_bar_set_property;

  widget_class->root = gtk_header_bar_root;
  widget_class->unroot = gtk_header_bar_unroot;
  widget_class->get_request_mode = gtk_header_bar_get_request_mode;

  header_bar_props[PROP_TITLE_WIDGET] =
      g_param_spec_object ("title-widget", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE|G_PARAM_STATIC_STRINGS|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkHeaderBar:show-title-buttons: (attributes org.gtk.Property.get=gtk_header_bar_get_show_title_buttons org.gtk.Property.set=gtk_header_bar_set_show_title_buttons)
   *
   * Whether to show title buttons like close, minimize, maximize.
   *
   * Which buttons are actually shown and where is determined
   * by the [property@Gtk.HeaderBar:decoration-layout] property,
   * and by the state of the window (e.g. a close button will not
   * be shown if the window can't be closed).
   */
  header_bar_props[PROP_SHOW_TITLE_BUTTONS] =
      g_param_spec_boolean ("show-title-buttons", NULL, NULL,
                            TRUE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkHeaderBar:decoration-layout: (attributes org.gtk.Property.get=gtk_header_bar_get_decoration_layout org.gtk.Property.set=gtk_header_bar_set_decoration_layout)
   *
   * The decoration layout for buttons.
   *
   * If this property is not set, the
   * [property@Gtk.Settings:gtk-decoration-layout] setting is used.
   */
  header_bar_props[PROP_DECORATION_LAYOUT] =
      g_param_spec_string ("decoration-layout", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, header_bar_props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("headerbar"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
gtk_header_bar_init (GtkHeaderBar *bar)
{
  bar->title_widget = NULL;
  bar->decoration_layout = NULL;
  bar->show_title_buttons = TRUE;

  bar->handle = gtk_window_handle_new ();
  gtk_widget_set_parent (bar->handle, GTK_WIDGET (bar));

  bar->center_box = gtk_center_box_new ();
  gtk_window_handle_set_child (GTK_WINDOW_HANDLE (bar->handle), bar->center_box);

  bar->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (bar->start_box, "start");
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (bar->center_box), bar->start_box);

  bar->end_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (bar->end_box, "end");
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (bar->center_box), bar->end_box);

  construct_title_label (bar);
  create_window_controls (bar);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_header_bar_buildable_add_child (GtkBuildable *buildable,
                                    GtkBuilder   *builder,
                                    GObject      *child,
                                    const char   *type)
{
  if (g_strcmp0 (type, "title") == 0)
    gtk_header_bar_set_title_widget (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "start") == 0)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "end") == 0)
    gtk_header_bar_pack_end (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else if (type == NULL && GTK_IS_WIDGET (child))
    gtk_header_bar_pack_start (GTK_HEADER_BAR (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_header_bar_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_header_bar_buildable_add_child;
}

/**
 * gtk_header_bar_pack_start:
 * @bar: A `GtkHeaderBar`
 * @child: the `GtkWidget` to be added to @bar
 *
 * Adds @child to @bar, packed with reference to the
 * start of the @bar.
 */
void
gtk_header_bar_pack_start (GtkHeaderBar *bar,
                           GtkWidget    *child)
{
  gtk_header_bar_pack (bar, child, GTK_PACK_START);
}

/**
 * gtk_header_bar_pack_end:
 * @bar: A `GtkHeaderBar`
 * @child: the `GtkWidget` to be added to @bar
 *
 * Adds @child to @bar, packed with reference to the
 * end of the @bar.
 */
void
gtk_header_bar_pack_end (GtkHeaderBar *bar,
                         GtkWidget    *child)
{
  gtk_header_bar_pack (bar, child, GTK_PACK_END);
}

/**
 * gtk_header_bar_new:
 *
 * Creates a new `GtkHeaderBar` widget.
 *
 * Returns: a new `GtkHeaderBar`
 */
GtkWidget *
gtk_header_bar_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_HEADER_BAR, NULL));
}

/**
 * gtk_header_bar_get_show_title_buttons: (attributes org.gtk.Method.get_property=show-title-buttons)
 * @bar: a `GtkHeaderBar`
 *
 * Returns whether this header bar shows the standard window
 * title buttons.
 *
 * Returns: %TRUE if title buttons are shown
 */
gboolean
gtk_header_bar_get_show_title_buttons (GtkHeaderBar *bar)
{
  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), FALSE);

  return bar->show_title_buttons;
}

/**
 * gtk_header_bar_set_show_title_buttons: (attributes org.gtk.Method.set_property=show-title-buttons)
 * @bar: a `GtkHeaderBar`
 * @setting: %TRUE to show standard title buttons
 *
 * Sets whether this header bar shows the standard window
 * title buttons.
 */
void
gtk_header_bar_set_show_title_buttons (GtkHeaderBar *bar,
                                       gboolean      setting)
{
  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  setting = setting != FALSE;

  if (bar->show_title_buttons == setting)
    return;

  bar->show_title_buttons = setting;

  if (setting)
    create_window_controls (bar);
  else
    {
      if (bar->start_box && bar->start_window_controls)
        {
          gtk_box_remove (GTK_BOX (bar->start_box), bar->start_window_controls);
          bar->start_window_controls = NULL;
        }

      if (bar->end_box && bar->end_window_controls)
        {
          gtk_box_remove (GTK_BOX (bar->end_box), bar->end_window_controls);
          bar->end_window_controls = NULL;
        }
    }

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_SHOW_TITLE_BUTTONS]);
}

/**
 * gtk_header_bar_set_decoration_layout: (attributes org.gtk.Method.set_property=decoration-layout)
 * @bar: a `GtkHeaderBar`
 * @layout: (nullable): a decoration layout, or %NULL to unset the layout
 *
 * Sets the decoration layout for this header bar.
 *
 * This property overrides the
 * [property@Gtk.Settings:gtk-decoration-layout] setting.
 *
 * There can be valid reasons for overriding the setting, such
 * as a header bar design that does not allow for buttons to take
 * room on the right, or only offers room for a single close button.
 * Split header bars are another example for overriding the setting.
 *
 * The format of the string is button names, separated by commas.
 * A colon separates the buttons that should appear on the left
 * from those on the right. Recognized button names are minimize,
 * maximize, close and icon (the window icon).
 *
 * For example, “icon:minimize,maximize,close” specifies an icon
 * on the left, and minimize, maximize and close buttons on the right.
 */
void
gtk_header_bar_set_decoration_layout (GtkHeaderBar *bar,
                                      const char   *layout)
{
  g_return_if_fail (GTK_IS_HEADER_BAR (bar));

  g_free (bar->decoration_layout);
  bar->decoration_layout = g_strdup (layout);

  g_object_notify_by_pspec (G_OBJECT (bar), header_bar_props[PROP_DECORATION_LAYOUT]);
}

/**
 * gtk_header_bar_get_decoration_layout: (attributes org.gtk.Method.get_property=decoration-layout)
 * @bar: a `GtkHeaderBar`
 *
 * Gets the decoration layout of the `GtkHeaderBar`.
 *
 * Returns: (nullable): the decoration layout
 */
const char *
gtk_header_bar_get_decoration_layout (GtkHeaderBar *bar)
{
  g_return_val_if_fail (GTK_IS_HEADER_BAR (bar), NULL);

  return bar->decoration_layout;
}
