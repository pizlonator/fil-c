/* gtkshortcutssection.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutssection.h"

#include "gtkbox.h"
#include "gtkbuildable.h"
#include "gtkshortcutsgroup.h"
#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtkstackswitcher.h"
#include "gtkorientable.h"
#include "gtksizegroup.h"
#include "gtkwidget.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkgesturepan.h"
#include "gtkwidgetprivate.h"
#include "gtkcenterbox.h"
#include <glib/gi18n-lib.h>

/**
 * GtkShortcutsSection:
 *
 * A `GtkShortcutsSection` collects all the keyboard shortcuts and gestures
 * for a major application mode.
 *
 * If your application needs multiple sections, you should give each
 * section a unique [property@Gtk.ShortcutsSection:section-name] and
 * a [property@Gtk.ShortcutsSection:title] that can be shown in the
 * section selector of the [class@Gtk.ShortcutsWindow].
 *
 * The [property@Gtk.ShortcutsSection:max-height] property can be used
 * to influence how the groups in the section are distributed over pages
 * and columns.
 *
 * This widget is only meant to be used with [class@Gtk.ShortcutsWindow].
 *
 * The recommended way to construct a `GtkShortcutsSection` is with
 * [class@Gtk.Builder], by using the `<child>` tag to populate a
 * `GtkShortcutsSection` with one or more [class@Gtk.ShortcutsGroup]
 * instances, which in turn contain one or more [class@Gtk.ShortcutsShortcut]
 * objects.
 *
 * If you need to add a group programmatically, use
 * [method@Gtk.ShortcutsSection.add_group].
 */

struct _GtkShortcutsSection
{
  GtkBox            parent_instance;

  char             *name;
  char             *title;
  char             *view_name;
  guint             max_height;

  GtkStack         *stack;
  GtkStackSwitcher *switcher;
  GtkWidget        *show_all;
  GtkWidget        *footer;
  GList            *groups;

  gboolean          has_filtered_group;
};

struct _GtkShortcutsSectionClass
{
  GtkBoxClass parent_class;

  gboolean (* change_current_page) (GtkShortcutsSection *self,
                                    int                  offset);

};

static void gtk_shortcuts_section_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkShortcutsSection, gtk_shortcuts_section, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_shortcuts_section_buildable_iface_init))

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SECTION_NAME,
  PROP_VIEW_NAME,
  PROP_MAX_HEIGHT,
  LAST_PROP
};

enum {
  CHANGE_CURRENT_PAGE,
  LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP];
static guint signals[LAST_SIGNAL];

static void gtk_shortcuts_section_set_view_name    (GtkShortcutsSection *self,
                                                    const char          *view_name);
static void gtk_shortcuts_section_set_max_height   (GtkShortcutsSection *self,
                                                    guint                max_height);

static void gtk_shortcuts_section_show_all         (GtkShortcutsSection *self);
static void gtk_shortcuts_section_filter_groups    (GtkShortcutsSection *self);
static void gtk_shortcuts_section_reflow_groups    (GtkShortcutsSection *self);

static gboolean gtk_shortcuts_section_change_current_page (GtkShortcutsSection *self,
                                                           int                  offset);

static void gtk_shortcuts_section_pan_gesture_pan (GtkGesturePan       *gesture,
                                                   GtkPanDirection      direction,
                                                   double               offset,
                                                   GtkShortcutsSection *self);

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_shortcuts_section_buildable_add_child (GtkBuildable *buildable,
                                           GtkBuilder   *builder,
                                           GObject      *child,
                                           const char   *type)
{
  if (GTK_IS_SHORTCUTS_GROUP (child))
    gtk_shortcuts_section_add_group (GTK_SHORTCUTS_SECTION (buildable), GTK_SHORTCUTS_GROUP (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_shortcuts_section_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_shortcuts_section_buildable_add_child;
}

static void
map_child (GtkWidget *child)
{
  if (_gtk_widget_get_visible (child) &&
      _gtk_widget_get_child_visible (child) &&
      !_gtk_widget_get_mapped (child))
    gtk_widget_map (child);
}

static void
gtk_shortcuts_section_map (GtkWidget *widget)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (widget);

  GTK_WIDGET_CLASS (gtk_shortcuts_section_parent_class)->map (widget);

  map_child (GTK_WIDGET (self->stack));
  map_child (GTK_WIDGET (self->footer));
}

static void
gtk_shortcuts_section_unmap (GtkWidget *widget)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (widget);

  GTK_WIDGET_CLASS (gtk_shortcuts_section_parent_class)->unmap (widget);

  gtk_widget_unmap (GTK_WIDGET (self->footer));
  gtk_widget_unmap (GTK_WIDGET (self->stack));
}

static void
gtk_shortcuts_section_dispose (GObject *object)
{
  GtkShortcutsSection *self = GTK_SHORTCUTS_SECTION (object);

  g_clear_pointer ((GtkWidget **)&self->stack, gtk_widget_unparent);
  g_clear_pointer (&self->footer, gtk_widget_unparent);

  g_list_free (self->groups);
  self->groups = NULL;

  G_OBJECT_CLASS (gtk_shortcuts_section_parent_class)->dispose (object);
}

static void
gtk_shortcuts_section_finalize (GObject *object)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->view_name, g_free);

  G_OBJECT_CLASS (gtk_shortcuts_section_parent_class)->finalize (object);
}

static void
gtk_shortcuts_section_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_VIEW_NAME:
      g_value_set_string (value, self->view_name);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_MAX_HEIGHT:
      g_value_set_uint (value, self->max_height);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_section_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkShortcutsSection *self = (GtkShortcutsSection *)object;

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;

    case PROP_VIEW_NAME:
      gtk_shortcuts_section_set_view_name (self, g_value_get_string (value));
      break;

    case PROP_TITLE:
      g_free (self->title);
      self->title = g_value_dup_string (value);
      break;

    case PROP_MAX_HEIGHT:
      gtk_shortcuts_section_set_max_height (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_section_class_init (GtkShortcutsSectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shortcuts_section_finalize;
  object_class->dispose = gtk_shortcuts_section_dispose;
  object_class->get_property = gtk_shortcuts_section_get_property;
  object_class->set_property = gtk_shortcuts_section_set_property;

  widget_class->map = gtk_shortcuts_section_map;
  widget_class->unmap = gtk_shortcuts_section_unmap;

  klass->change_current_page = gtk_shortcuts_section_change_current_page;

  /**
   * GtkShortcutsSection:section-name:
   *
   * A unique name to identify this section among the sections
   * added to the `GtkShortcutsWindow`.
   *
   * Setting the [property@Gtk.ShortcutsWindow:section-name] property
   * to this string will make this section shown in the `GtkShortcutsWindow`.
   */
  properties[PROP_SECTION_NAME] =
    g_param_spec_string ("section-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsSection:view-name:
   *
   * A view name to filter the groups in this section by.
   *
   * See [property@Gtk.ShortcutsGroup:view].
   *
   * Applications are expected to use the
   * [property@Gtk.ShortcutsWindow:view-name] property
   * for this purpose.
   */
  properties[PROP_VIEW_NAME] =
    g_param_spec_string ("view-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkShortcutsSection:title:
   *
   * The string to show in the section selector of the `GtkShortcutsWindow`
   * for this section.
   *
   * If there is only one section, you don't need to set a title,
   * since the section selector will not be shown in this case.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsSection:max-height:
   *
   * The maximum number of lines to allow per column.
   *
   * This property can be used to influence how the groups in this
   * section are distributed across pages and columns. The default
   * value of 15 should work in most cases.
   */
  properties[PROP_MAX_HEIGHT] =
    g_param_spec_uint ("max-height", NULL, NULL,
                       0, G_MAXUINT, 15,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  signals[CHANGE_CURRENT_PAGE] =
    g_signal_new (I_("change-current-page"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkShortcutsSectionClass, change_current_page),
                  NULL, NULL,
                  _gtk_marshal_BOOLEAN__INT,
                  G_TYPE_BOOLEAN, 1,
                  G_TYPE_INT);
  g_signal_set_va_marshaller (signals[CHANGE_CURRENT_PAGE],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__INTv);

  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Page_Up, 0,
                                       "change-current-page",
                                       "(i)", -1);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Page_Down, 0,
                                       "change-current-page",
                                       "(i)", 1);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Page_Up, GDK_CONTROL_MASK,
                                       "change-current-page",
                                       "(i)", -1);
  gtk_widget_class_add_binding_signal (widget_class,
                                       GDK_KEY_Page_Down, GDK_CONTROL_MASK,
                                       "change-current-page",
                                       "(i)", 1);

  gtk_widget_class_set_css_name (widget_class, I_("shortcuts-section"));
}

static void
gtk_shortcuts_section_init (GtkShortcutsSection *self)
{
  GtkGesture *gesture;

  self->max_height = 15;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_homogeneous (GTK_BOX (self), FALSE);
  gtk_box_set_spacing (GTK_BOX (self), 22);

  self->stack = g_object_new (GTK_TYPE_STACK,
                              "hhomogeneous", TRUE,
                              "vhomogeneous", TRUE,
                              "transition-type", GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT,
                              "vexpand", TRUE,
                              "visible", TRUE,
                              NULL);
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->stack));

  self->switcher = g_object_new (GTK_TYPE_STACK_SWITCHER,
                                 "halign", GTK_ALIGN_CENTER,
                                 "stack", self->stack,
                                 "visible", FALSE,
                                 NULL);

  gtk_widget_remove_css_class (GTK_WIDGET (self->switcher), "linked");

  self->show_all = gtk_button_new_with_mnemonic (_("_Show All"));
  gtk_widget_set_visible (self->show_all, FALSE);
  g_signal_connect_swapped (self->show_all, "clicked",
                            G_CALLBACK (gtk_shortcuts_section_show_all), self);

  self->footer = gtk_center_box_new ();
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->footer));

  gtk_widget_set_hexpand (GTK_WIDGET (self->switcher), TRUE);
  gtk_widget_set_halign (GTK_WIDGET (self->switcher), GTK_ALIGN_CENTER);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (self->footer), GTK_WIDGET (self->switcher));
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (self->footer), self->show_all);
  gtk_widget_set_halign (self->show_all, GTK_ALIGN_END);

  gesture = gtk_gesture_pan_new (GTK_ORIENTATION_HORIZONTAL);
  g_signal_connect (gesture, "pan",
                    G_CALLBACK (gtk_shortcuts_section_pan_gesture_pan), self);
  gtk_widget_add_controller (GTK_WIDGET (self->stack), GTK_EVENT_CONTROLLER (gesture));
}

static void
gtk_shortcuts_section_set_view_name (GtkShortcutsSection *self,
                                     const char          *view_name)
{
  if (g_strcmp0 (self->view_name, view_name) == 0)
    return;

  g_free (self->view_name);
  self->view_name = g_strdup (view_name);

  gtk_shortcuts_section_filter_groups (self);
  gtk_shortcuts_section_reflow_groups (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VIEW_NAME]);
}

static void
gtk_shortcuts_section_set_max_height (GtkShortcutsSection *self,
                                      guint                max_height)
{
  if (self->max_height == max_height)
    return;

  self->max_height = max_height;

  gtk_shortcuts_section_reflow_groups (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_HEIGHT]);
}

/**
 * gtk_shortcuts_section_add_group:
 * @self: a `GtkShortcutsSection`
 * @group: the `GtkShortcutsGroup` to add
 *
 * Adds a group to the shortcuts section.
 *
 * This is the programmatic equivalent to using [class@Gtk.Builder] and a
 * `<child>` tag to add the child.
 * 
 * Adding children with the `GtkBox` API is not appropriate, as
 * `GtkShortcutsSection` manages its children internally.
 *
 * Since: 4.14
 */
void
gtk_shortcuts_section_add_group (GtkShortcutsSection *self,
                                 GtkShortcutsGroup   *group)
{
  g_return_if_fail (GTK_IS_SHORTCUTS_SECTION (self));
  g_return_if_fail (GTK_IS_SHORTCUTS_GROUP (group));
  g_return_if_fail (gtk_widget_get_parent (GTK_WIDGET (group)) == NULL);

  GtkWidget *page, *column;

  page = gtk_widget_get_last_child (GTK_WIDGET (self->stack));
  if (page == NULL)
    {
      page = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 22);
      gtk_stack_add_named (self->stack, page, "1");
    }

  column = gtk_widget_get_last_child (page);
  if (column == NULL)
    {
      column = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);
      gtk_box_append (GTK_BOX (page), column);
    }

  gtk_box_append (GTK_BOX (column), GTK_WIDGET (group));
  self->groups = g_list_append (self->groups, group);

  gtk_shortcuts_section_reflow_groups (self);
}

static void
gtk_shortcuts_section_show_all (GtkShortcutsSection *self)
{
  gtk_shortcuts_section_set_view_name (self, NULL);
}

static void
update_group_visibility (GtkWidget *child, gpointer data)
{
  GtkShortcutsSection *self = data;

  if (GTK_IS_SHORTCUTS_GROUP (child))
    {
      char *view;
      gboolean match;

      g_object_get (child, "view", &view, NULL);
      match = view == NULL ||
              self->view_name == NULL ||
              strcmp (view, self->view_name) == 0;

      gtk_widget_set_visible (child, match);
      self->has_filtered_group |= !match;

      g_free (view);
    }
  else
    {
      for (child = gtk_widget_get_first_child (GTK_WIDGET (child));
           child != NULL;
           child = gtk_widget_get_next_sibling (child))
        update_group_visibility (child, self);
    }
}

static void
gtk_shortcuts_section_filter_groups (GtkShortcutsSection *self)
{
  GtkWidget *child;

  self->has_filtered_group = FALSE;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    update_group_visibility (child, self);

  gtk_widget_set_visible (GTK_WIDGET (self->show_all), self->has_filtered_group);
  gtk_widget_set_visible (gtk_widget_get_parent (GTK_WIDGET (self->show_all)),
                          gtk_widget_get_visible (GTK_WIDGET (self->show_all)) ||
                          gtk_widget_get_visible (GTK_WIDGET (self->switcher)));
}

static void
gtk_shortcuts_section_reflow_groups (GtkShortcutsSection *self)
{
  GList *pages, *p;
  GtkWidget *page;
  GList *groups, *g;
  guint n_rows;
  guint n_columns;
  guint n_pages;
  GtkWidget *current_page, *current_column;

  /* collect all groups from the current pages */
  groups = NULL;
  for (page = gtk_widget_get_first_child (GTK_WIDGET (self->stack));
       page != NULL;
       page = gtk_widget_get_next_sibling (page))
    {
      GtkWidget *column;

      for (column = gtk_widget_get_last_child (page);
           column != NULL;
           column = gtk_widget_get_prev_sibling (column))
        {
          GtkWidget *group;

          for (group = gtk_widget_get_last_child (column);
               group != NULL;
               group = gtk_widget_get_prev_sibling (group))
            {
              groups = g_list_prepend (groups, group);
            }
        }
    }

  /* create new pages */
  current_page = NULL;
  current_column = NULL;

  pages = NULL;
  n_rows = 0;
  n_columns = 0;
  n_pages = 0;
  for (g = groups; g; g = g->next)
    {
      GtkShortcutsGroup *group = g->data;
      guint height;
      gboolean visible;

      g_object_get (group,
                    "visible", &visible,
                    "height", &height,
                    NULL);
      if (!visible)
        height = 0;

      if (current_column == NULL || n_rows + height > self->max_height)
        {
          GtkWidget *column_box;
          GtkSizeGroup *size_group;

          column_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);

          size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
          g_object_set_data_full (G_OBJECT (column_box), "accel-size-group", size_group, g_object_unref);
          size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
          g_object_set_data_full (G_OBJECT (column_box), "title-size-group", size_group, g_object_unref);

          if (n_columns % 2 == 0)
            {
              page = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 22);

              pages = g_list_append (pages, page);
              current_page = page;
            }

          gtk_box_append (GTK_BOX (current_page), column_box);
          current_column = column_box;
          n_columns += 1;
          n_rows = 0;
        }

      n_rows += height;

      g_object_set (group,
                    "accel-size-group", g_object_get_data (G_OBJECT (current_column), "accel-size-group"),
                    "title-size-group", g_object_get_data (G_OBJECT (current_column), "title-size-group"),
                    NULL);

      g_object_ref (group);
      gtk_box_remove (GTK_BOX (gtk_widget_get_parent (GTK_WIDGET (group))), GTK_WIDGET (group));
      gtk_box_append (GTK_BOX (current_column), GTK_WIDGET (group));
      g_object_unref (group);
    }

  /* balance the last page */
  if (n_columns % 2 == 1)
    {
      GtkWidget *column_box;
      GtkSizeGroup *size_group;
      GList *content;
      GtkWidget *child;
      guint n;

      column_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 22);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      g_object_set_data_full (G_OBJECT (column_box), "accel-size-group", size_group, g_object_unref);
      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      g_object_set_data_full (G_OBJECT (column_box), "title-size-group", size_group, g_object_unref);

      gtk_box_append (GTK_BOX (current_page), column_box);

      content = NULL;
      for (child = gtk_widget_get_last_child (current_column);
           child != NULL;
           child = gtk_widget_get_prev_sibling (child))
        content = g_list_prepend (content, child);
      n = 0;

      for (g = g_list_last (content); g; g = g->prev)
        {
          GtkShortcutsGroup *group = g->data;
          guint height;
          gboolean visible;

          g_object_get (group,
                        "visible", &visible,
                        "height", &height,
                        NULL);
          if (!visible)
            height = 0;

          if (n_rows - height == 0)
            break;
          if (ABS (n_rows - n) < ABS ((n_rows - height) - (n + height)))
            break;

          n_rows -= height;
          n += height;
        }

      g_assert (g);
      for (g = g->next; g; g = g->next)
        {
          GtkShortcutsGroup *group = g->data;

          g_object_set (group,
                        "accel-size-group", g_object_get_data (G_OBJECT (column_box), "accel-size-group"),
                        "title-size-group", g_object_get_data (G_OBJECT (column_box), "title-size-group"),
                        NULL);

          g_object_ref (group);
          gtk_box_remove (GTK_BOX (current_column), GTK_WIDGET (group));
          gtk_box_append (GTK_BOX (column_box), GTK_WIDGET (group));
          g_object_unref (group);
        }

      g_list_free (content);
    }

  /* replace the current pages with the new pages */
  while ((page = gtk_widget_get_first_child (GTK_WIDGET (self->stack))))
    gtk_stack_remove (self->stack, page);

  for (p = pages, n_pages = 0; p; p = p->next, n_pages++)
    {
      char *title;

      page = p->data;
      title = g_strdup_printf ("_%u", n_pages + 1);
      gtk_stack_add_titled (self->stack, page, title, title);
      g_free (title);
    }

  /* fix up stack switcher */
  {
    GtkWidget *w;

    gtk_widget_add_css_class (GTK_WIDGET (self->switcher), "circular");

    for (w = gtk_widget_get_first_child (GTK_WIDGET (self->switcher));
         w != NULL;
         w = gtk_widget_get_next_sibling (w))
      {
        GtkWidget *label;

        gtk_widget_add_css_class (w, "circular");

        label = gtk_button_get_child (GTK_BUTTON (w));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
      }

    gtk_widget_set_visible (GTK_WIDGET (self->switcher), (n_pages > 1));
    gtk_widget_set_visible (gtk_widget_get_parent (GTK_WIDGET (self->switcher)),
                            gtk_widget_get_visible (GTK_WIDGET (self->show_all)) ||
                            gtk_widget_get_visible (GTK_WIDGET (self->switcher)));
  }

  /* clean up */
  g_list_free (groups);
  g_list_free (pages);
}

static gboolean
gtk_shortcuts_section_change_current_page (GtkShortcutsSection *self,
                                           int                  offset)
{
  GtkWidget *child;

  child = gtk_stack_get_visible_child (self->stack);

  if (offset == 1)
    child = gtk_widget_get_next_sibling (child);
  else if (offset == -1)
    child = gtk_widget_get_prev_sibling (child);
  else
    g_assert_not_reached ();

  if (child)
    gtk_stack_set_visible_child (self->stack, child);
  else
    gtk_widget_error_bell (GTK_WIDGET (self));

  return TRUE;
}

static void
gtk_shortcuts_section_pan_gesture_pan (GtkGesturePan       *gesture,
                                       GtkPanDirection      direction,
                                       double               offset,
                                       GtkShortcutsSection *self)
{
  if (offset < 50)
    return;

  if (direction == GTK_PAN_DIRECTION_LEFT)
    gtk_shortcuts_section_change_current_page (self, 1);
  else if (direction == GTK_PAN_DIRECTION_RIGHT)
    gtk_shortcuts_section_change_current_page (self, -1);
  else
    g_assert_not_reached ();

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
}
