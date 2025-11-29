/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include <stdlib.h>

#include "init.h"
#include "window.h"
#include "prop-list.h"
#include "clipboard.h"
#include "controllers.h"
#include "css-editor.h"
#include "css-node-tree.h"
#include "object-tree.h"
#include "size-groups.h"
#include "a11y.h"
#include "actions.h"
#include "shortcuts.h"
#include "list-data.h"
#include "menu.h"
#include "misc-info.h"
#include "magnifier.h"
#include "recorder.h"
#include "tree-data.h"
#include "visual.h"
#include "general.h"
#include "logs.h"

#include "gdkdebugprivate.h"
#include "gdkmarshalers.h"
#include "gskrendererprivate.h"
#include "gtkbutton.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtklabel.h"
#include "gtkmodulesprivate.h"
#include "gtkprivate.h"
#include "gtknative.h"
#include "gtkstack.h"
#include "gtkwindowgroup.h"
#include "gtkrevealer.h"
#include "gtklayoutmanager.h"
#include "gtkcssprovider.h"
#include "gtkwidgetprivate.h"


enum {
  PROP_INSPECTED_DISPLAY = 1,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  EVENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


G_DEFINE_TYPE (GtkInspectorWindow, gtk_inspector_window, GTK_TYPE_WINDOW)


/* Fast way of knowing that further checks are necessary because at least
 * one inspector window has been constructed. */
static gboolean any_inspector_window_constructed = FALSE;


static gboolean
set_selected_object (GtkInspectorWindow *iw,
                     GObject            *selected)
{
  GList *l;
  char *title;

  if (!gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->prop_list), selected))
    return FALSE;

  title = gtk_inspector_get_object_title (selected);
  gtk_label_set_label (GTK_LABEL (iw->object_title), title);
  g_free (title);

  gtk_inspector_prop_list_set_layout_child (GTK_INSPECTOR_PROP_LIST (iw->layout_prop_list), selected);
  gtk_inspector_misc_info_set_object (GTK_INSPECTOR_MISC_INFO (iw->misc_info), selected);
  gtk_inspector_css_node_tree_set_object (GTK_INSPECTOR_CSS_NODE_TREE (iw->widget_css_node_tree), selected);
  gtk_inspector_size_groups_set_object (GTK_INSPECTOR_SIZE_GROUPS (iw->size_groups), selected);
  gtk_inspector_tree_data_set_object (GTK_INSPECTOR_TREE_DATA (iw->tree_data), selected);
  gtk_inspector_list_data_set_object (GTK_INSPECTOR_LIST_DATA (iw->list_data), selected);
  gtk_inspector_actions_set_object (GTK_INSPECTOR_ACTIONS (iw->actions), selected);
  gtk_inspector_shortcuts_set_object (GTK_INSPECTOR_SHORTCUTS (iw->shortcuts), selected);
  gtk_inspector_menu_set_object (GTK_INSPECTOR_MENU (iw->menu), selected);
  gtk_inspector_controllers_set_object (GTK_INSPECTOR_CONTROLLERS (iw->controllers), selected);
  gtk_inspector_magnifier_set_object (GTK_INSPECTOR_MAGNIFIER (iw->magnifier), selected);
  gtk_inspector_a11y_set_object (GTK_INSPECTOR_A11Y (iw->a11y), selected);

  for (l = iw->extra_pages; l != NULL; l = l->next)
    g_object_set (l->data, "object", selected, NULL);

  return TRUE;
}

static void
on_object_activated (GtkInspectorObjectTree *wt,
                     GObject                *selected,
                     GtkInspectorWindow     *iw)
{
  if (GTK_IS_WIDGET (selected))
    gtk_inspector_window_set_object (iw, selected, CHILD_KIND_WIDGET, 0);
  else
    gtk_inspector_window_set_object (iw, selected, CHILD_KIND_OTHER, 0);

  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-details");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "details");
}

static void
on_object_selected (GtkInspectorObjectTree *wt,
                    GObject                *selected,
                    GtkInspectorWindow     *iw)
{
  gtk_widget_set_sensitive (iw->object_details_button, selected != NULL);
  if (GTK_IS_WIDGET (selected))
    gtk_inspector_flash_widget (iw, GTK_WIDGET (selected));
}

static void
notify_node (GtkInspectorCssNodeTree *cnt,
             GParamSpec              *pspec,
             GtkInspectorWindow      *iw)
{
  GtkCssNode *node;
  GtkWidget *widget = NULL;

  for (node = gtk_inspector_css_node_tree_get_node (cnt);
       node != NULL;
       node = gtk_css_node_get_parent (node))
    {
      if (!GTK_IS_CSS_WIDGET_NODE (node))
        continue;

      widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (node));
      if (widget != NULL)
        break;
    }
  if (widget)
    gtk_inspector_flash_widget (iw, widget);
}

static void
close_object_details (GtkWidget *button, GtkInspectorWindow *iw)
{
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-tree");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "list");
}

static void
open_object_details (GtkWidget *button, GtkInspectorWindow *iw)
{
  GObject *selected;

  selected = gtk_inspector_object_tree_get_selected (GTK_INSPECTOR_OBJECT_TREE (iw->object_tree));

  gtk_inspector_window_set_object (iw, selected, CHILD_KIND_WIDGET, 0);

  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-details");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "details");
}

static gboolean
translate_visible_child_name (GBinding     *binding,
                              const GValue *from,
                              GValue       *to,
                              gpointer      user_data)
{
  GtkInspectorWindow *iw = user_data;
  const char *name;

  name = g_value_get_string (from);

  if (gtk_stack_get_child_by_name (GTK_STACK (iw->object_start_stack), name))
    g_value_set_string (to, name);
  else
    g_value_set_string (to, "empty");

  return TRUE;
}

typedef struct
{
  GObject *object;
  ChildKind kind;
  guint position;
} ChildData;

static void
gtk_inspector_window_init (GtkInspectorWindow *iw)
{
  GIOExtensionPoint *extension_point;
  GList *l, *extensions;

  iw->objects = g_array_new (FALSE, FALSE, sizeof (ChildData));

  gtk_widget_init_template (GTK_WIDGET (iw));

  g_object_bind_property_full (iw->object_details, "visible-child-name",
                               iw->object_start_stack, "visible-child-name",
                               G_BINDING_SYNC_CREATE,
                               translate_visible_child_name,
                               NULL,
                               iw,
                               NULL);

  gtk_window_group_add_window (gtk_window_group_new (), GTK_WINDOW (iw));

  extension_point = g_io_extension_point_lookup ("gtk-inspector-page");
  extensions = g_io_extension_point_get_extensions (extension_point);

  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = l->data;
      GType type;
      GtkWidget *widget;
      const char *name;
      char *title;
      GtkWidget *button;
      gboolean use_picker;

      type = g_io_extension_get_type (extension);

      widget = g_object_new (type, NULL);

      iw->extra_pages = g_list_prepend (iw->extra_pages, widget);

      name = g_io_extension_get_name (extension);
      g_object_get (widget, "title", &title, NULL);

      if (g_object_class_find_property (G_OBJECT_GET_CLASS (widget), "use-picker"))
        g_object_get (widget, "use-picker", &use_picker, NULL);
      else
        use_picker = FALSE;

      if (use_picker)
        {
          button = gtk_button_new_from_icon_name ("find-location-symbolic");
          gtk_widget_set_focus_on_click (button, FALSE);
          gtk_widget_set_halign (button, GTK_ALIGN_START);
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          g_signal_connect (button, "clicked",
                            G_CALLBACK (gtk_inspector_on_inspect), iw);
        }
      else
        button = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      gtk_stack_add_titled (GTK_STACK (iw->top_stack), widget, name, title);
      gtk_stack_add_named (GTK_STACK (iw->button_stack), button, name);

      g_free (title);
    }
}

static void
gtk_inspector_window_constructed (GObject *object)
{
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (object);

  G_OBJECT_CLASS (gtk_inspector_window_parent_class)->constructed (object);

  g_object_set_data (G_OBJECT (iw->inspected_display), "-gtk-inspector", iw);
  any_inspector_window_constructed = TRUE;

  gtk_inspector_object_tree_set_display (GTK_INSPECTOR_OBJECT_TREE (iw->object_tree), iw->inspected_display);
  gtk_inspector_css_editor_set_display (GTK_INSPECTOR_CSS_EDITOR (iw->css_editor), iw->inspected_display);
  gtk_inspector_visual_set_display (GTK_INSPECTOR_VISUAL (iw->visual), iw->inspected_display);
  gtk_inspector_general_set_display (GTK_INSPECTOR_GENERAL (iw->general), iw->inspected_display);
  gtk_inspector_clipboard_set_display (GTK_INSPECTOR_CLIPBOARD (iw->clipboard), iw->inspected_display);
  gtk_inspector_logs_set_display (GTK_INSPECTOR_LOGS (iw->logs), iw->inspected_display);
  gtk_inspector_css_node_tree_set_display (GTK_INSPECTOR_CSS_NODE_TREE (iw->widget_css_node_tree), iw->inspected_display);
}

static void
gtk_inspector_window_dispose (GObject *object)
{
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (object);

  g_object_set_data (G_OBJECT (iw->inspected_display), "-gtk-inspector", NULL);

  g_clear_object (&iw->flash_overlay);
  g_clear_pointer (&iw->objects, g_array_unref);

  gtk_widget_dispose_template (GTK_WIDGET (iw), GTK_TYPE_INSPECTOR_WINDOW);

  G_OBJECT_CLASS (gtk_inspector_window_parent_class)->dispose (object);
}

static void
object_details_changed (GtkWidget          *combo,
                        GParamSpec         *pspec,
                        GtkInspectorWindow *iw)
{
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_center_stack), "title");
}

static void
go_up_cb (GtkWidget          *button,
          GtkInspectorWindow *iw)
{
  if (iw->objects->len > 1)
    {
      gtk_inspector_window_pop_object (iw);
      return;
    }
  else if (iw->objects->len > 0)
    {
      ChildData *data = &g_array_index (iw->objects, ChildData, 0);
      GtkWidget *widget = (GtkWidget *)data->object;
      if (GTK_IS_WIDGET (widget) && gtk_widget_get_parent (widget))
        {
          GObject *obj = G_OBJECT (gtk_widget_get_parent (widget));
          gtk_inspector_window_replace_object (iw, obj, CHILD_KIND_WIDGET, 0);
          return;
        }
    }

  gtk_widget_error_bell (GTK_WIDGET (iw));
}

static void
go_down_cb (GtkWidget          *button,
            GtkInspectorWindow *iw)
{
  ChildData *data;
  GObject *object;

  if (iw->objects->len < 1)
    {
      gtk_widget_error_bell (GTK_WIDGET (iw));
      return;
    }

  data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
  object = data->object;

  if (GTK_IS_WIDGET (object))
    {
      GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (object));

      if (child)
        {
          gtk_inspector_window_push_object (iw, G_OBJECT (child), CHILD_KIND_WIDGET, 0);
          return;
        }
    }
  else if (G_IS_LIST_MODEL (object))
    {
      GObject *item = g_list_model_get_item (G_LIST_MODEL (object), 0);
      if (item)
        {
          gtk_inspector_window_push_object (iw, item, CHILD_KIND_LISTITEM, 0);
          g_object_unref (item);
          return;
        }
    }

  gtk_widget_error_bell (GTK_WIDGET (iw));
}

static void
go_previous_cb (GtkWidget          *button,
                GtkInspectorWindow *iw)
{
  ChildData *data;
  GObject *object;
  GObject *parent;

  if (iw->objects->len < 1)
    {
      gtk_widget_error_bell (GTK_WIDGET (iw));
      return;
    }

  if (iw->objects->len > 1)
    {
      data = &g_array_index (iw->objects, ChildData, iw->objects->len - 2);
      parent = data->object;
    }
  else
    parent = NULL;

  data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
  object = data->object;

  switch (data->kind)
    {
    case CHILD_KIND_WIDGET:
      {
        GtkWidget *sibling = gtk_widget_get_prev_sibling (GTK_WIDGET (object));
        if (sibling)
          {
            gtk_inspector_window_replace_object (iw, (GObject*)sibling, CHILD_KIND_WIDGET, 0);
            return;
          }
      }
      break;

    case CHILD_KIND_LISTITEM:
      {
        GObject *item;

        if (parent && data->position > 0)
          item = g_list_model_get_item (G_LIST_MODEL (parent), data->position - 1);
        else
          item = NULL;

        if (item)
          {
            gtk_inspector_window_replace_object (iw, item, CHILD_KIND_LISTITEM, data->position - 1);
            g_object_unref (item);
            return;
          }
      }
      break;

    case CHILD_KIND_CONTROLLER:
    case CHILD_KIND_PROPERTY:
    case CHILD_KIND_OTHER:
    default: ;
    }

  gtk_widget_error_bell (GTK_WIDGET (iw));
}

static void
go_next_cb (GtkWidget          *button,
            GtkInspectorWindow *iw)
{
  ChildData *data;
  GObject *object;
  GObject *parent;

  if (iw->objects->len < 1)
    {
      gtk_widget_error_bell (GTK_WIDGET (iw));
      return;
    }

  if (iw->objects->len > 1)
    {
      data = &g_array_index (iw->objects, ChildData, iw->objects->len - 2);
      parent = data->object;
    }
  else
    parent = NULL;

  data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
  object = data->object;

  switch (data->kind)
    {
    case CHILD_KIND_WIDGET:
      {
        GtkWidget *sibling = gtk_widget_get_next_sibling (GTK_WIDGET (object));
        if (sibling)
          {
            gtk_inspector_window_replace_object (iw, (GObject*)sibling, CHILD_KIND_WIDGET, 0);
            return;
          }
      }
      break;

    case CHILD_KIND_LISTITEM:
      {
        GObject *item;

        if (parent &&
            data->position + 1 < g_list_model_get_n_items (G_LIST_MODEL (parent)))
          item = g_list_model_get_item (G_LIST_MODEL (parent), data->position + 1);
        else
          item = NULL;

        if (item)
          {
            gtk_inspector_window_replace_object (iw, item, CHILD_KIND_LISTITEM, data->position + 1);
            g_object_unref (item);
            return;
          }
      }
      break;

    case CHILD_KIND_CONTROLLER:
    case CHILD_KIND_PROPERTY:
    case CHILD_KIND_OTHER:
    default: ;
    }

  gtk_widget_error_bell (GTK_WIDGET (iw));
}

static void
gtk_inspector_window_realize (GtkWidget *widget)
{
  GskRenderer *renderer;
  GtkCssProvider *provider;

  GTK_WIDGET_CLASS (gtk_inspector_window_parent_class)->realize (widget);

  renderer = gtk_native_get_renderer (GTK_NATIVE (widget));
  gsk_renderer_set_debug_flags (renderer, 0);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gtk/libgtk/inspector/inspector.css");
  gtk_style_context_add_provider_for_display (gtk_widget_get_display (widget),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);
  g_object_unref (provider);
}

static void
gtk_inspector_window_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_INSPECTED_DISPLAY:
      iw->inspected_display = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_inspector_window_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (object);

  switch (prop_id)
    {
    case PROP_INSPECTED_DISPLAY:
      g_value_set_object (value, iw->inspected_display);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
gtk_inspector_window_enable_debugging (GtkWindow *window,
                                       gboolean   toggle)
{
  return FALSE;
}

static void
force_one_full_redraw (GtkWidget *w)
{
  gtk_widget_queue_draw (w);
  for (w = gtk_widget_get_first_child (w); w; w = gtk_widget_get_next_sibling (w))
    force_one_full_redraw (w);
}

static void
force_full_redraw (GtkInspectorWindow *window)
{
  GListModel *toplevels;

  toplevels = gtk_window_get_toplevels ();
  for (unsigned int i = 0; i < g_list_model_get_n_items (toplevels); i++)
    {
      GtkWidget *w = GTK_WIDGET (g_list_model_get_item (toplevels, i));

      if (gtk_widget_get_display (w) == window->inspected_display)
        force_one_full_redraw (w);

      g_object_unref (w);
    }
}

static void
gtk_inspector_window_class_init (GtkInspectorWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);

  object_class->constructed = gtk_inspector_window_constructed;
  object_class->dispose = gtk_inspector_window_dispose;
  object_class->set_property = gtk_inspector_window_set_property;
  object_class->get_property = gtk_inspector_window_get_property;

  widget_class->realize = gtk_inspector_window_realize;

  window_class->enable_debugging = gtk_inspector_window_enable_debugging;

  properties[PROP_INSPECTED_DISPLAY] =
      g_param_spec_object ("inspected-display", NULL, NULL,
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  signals[EVENT] = g_signal_new (g_intern_static_string ("event"),
                                 G_OBJECT_CLASS_TYPE (object_class),
                                 G_SIGNAL_RUN_LAST,
                                 0,
                                 g_signal_accumulator_true_handled,
                                 NULL,
                                 _gdk_marshal_BOOLEAN__POINTER,
                                 G_TYPE_BOOLEAN,
                                 1,
                                 GDK_TYPE_EVENT);
  g_signal_set_va_marshaller (signals[EVENT],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_BOOLEAN__POINTERv);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/window.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, top_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, button_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_details);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_start_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_center_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_buttons);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_details_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, select_object);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, layout_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_css_node_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_recorder);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_title);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, size_groups);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, tree_data);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, list_data);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, actions);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, shortcuts);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, menu);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, misc_info);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, controllers);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, magnifier);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, a11y);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, sidebar_revealer);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, css_editor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, visual);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, general);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, clipboard);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, logs);

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, go_up_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, go_down_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, go_previous_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, list_position_label);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, go_next_button);

  gtk_widget_class_bind_template_callback (widget_class, gtk_inspector_on_inspect);
  gtk_widget_class_bind_template_callback (widget_class, on_object_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_object_selected);
  gtk_widget_class_bind_template_callback (widget_class, open_object_details);
  gtk_widget_class_bind_template_callback (widget_class, close_object_details);
  gtk_widget_class_bind_template_callback (widget_class, object_details_changed);
  gtk_widget_class_bind_template_callback (widget_class, notify_node);
  gtk_widget_class_bind_template_callback (widget_class, go_previous_cb);
  gtk_widget_class_bind_template_callback (widget_class, go_up_cb);
  gtk_widget_class_bind_template_callback (widget_class, go_down_cb);
  gtk_widget_class_bind_template_callback (widget_class, go_next_cb);
  gtk_widget_class_bind_template_callback (widget_class, force_full_redraw);
}

static GdkDisplay *
get_inspector_display (void)
{
  GdkDisplay *display;
  const char *name;

  name = g_getenv ("GTK_INSPECTOR_DISPLAY");
  display = gdk_display_open (name);

  if (display)
    g_debug ("Using display %s for GtkInspector", name);
  else
    g_message ("Failed to open display %s", name);

  if (!display)
    {
      display = gdk_display_open (NULL);
      if (display)
        g_debug ("Using default display for GtkInspector");
      else
        g_message ("Failed to open separate connection to default display");
    }


  if (display)
    {
      GdkDebugFlags flags;

      name = g_getenv ("GTK_INSPECTOR_RENDERER");

      g_object_set_data_full (G_OBJECT (display), "gsk-renderer",
                              g_strdup (name), g_free);

      flags = gdk_display_get_debug_flags (gdk_display_get_default ());

      gdk_display_set_debug_flags (display, flags & (GDK_DEBUG_GL_DISABLE_GL |
                                                     GDK_DEBUG_GL_DISABLE_GLES |
                                                     GDK_DEBUG_GL_GLX));
      gtk_set_display_debug_flags (display, 0);
    }

  if (!display)
    display = gdk_display_get_default ();

  if (display == gdk_display_get_default ())
    g_message ("Using default display for GtkInspector; expect some spillover");

  return display;
}

static GtkInspectorWindow *
gtk_inspector_window_new (GdkDisplay *display)
{
  GtkInspectorWindow *iw;

  iw = g_object_new (GTK_TYPE_INSPECTOR_WINDOW,
                     "display", get_inspector_display (),
                     "inspected-display", display,
                     NULL);

  return iw;
}

GtkWidget *
gtk_inspector_window_get (GdkDisplay *display)
{
  GtkWidget *iw;

  gtk_inspector_init ();

  iw = GTK_WIDGET (g_object_get_data (G_OBJECT (display), "-gtk-inspector"));

  if (!iw)
    iw = GTK_WIDGET (gtk_inspector_window_new (display));

  return iw;
}

void
gtk_inspector_window_add_overlay (GtkInspectorWindow  *iw,
                                  GtkInspectorOverlay *overlay)
{
  iw->overlays = g_list_prepend (iw->overlays, g_object_ref (overlay));

  gtk_inspector_overlay_queue_draw (overlay);
}

void
gtk_inspector_window_remove_overlay (GtkInspectorWindow  *iw,
                                     GtkInspectorOverlay *overlay)
{
  GList *item;

  item = g_list_find (iw->overlays, overlay);
  if (item == NULL)
    return;

  gtk_inspector_overlay_queue_draw (overlay);

  iw->overlays = g_list_delete_link (iw->overlays, item);
  g_object_unref (overlay);
}

static GtkInspectorWindow *
gtk_inspector_window_get_for_display (GdkDisplay *display)
{
  return g_object_get_data (G_OBJECT (display), "-gtk-inspector");
}

GskRenderNode *
gtk_inspector_prepare_render (GtkWidget            *widget,
                              GskRenderer          *renderer,
                              GdkSurface           *surface,
                              const cairo_region_t *region,
                              GskRenderNode        *root,
                              GskRenderNode        *widget_node)
{
  GtkInspectorWindow *iw;

  iw = gtk_inspector_window_get_for_display (gtk_widget_get_display (widget));
  if (iw == NULL)
    return root;

  /* sanity check for single-display GDK backends */
  if (GTK_WIDGET (iw) == widget)
    return root;

  gtk_inspector_recorder_record_render (GTK_INSPECTOR_RECORDER (iw->widget_recorder),
                                        widget,
                                        renderer,
                                        surface,
                                        region,
                                        root);

  if (iw->overlays)
    {
      GtkSnapshot *snapshot;
      GList *l;
      double native_x, native_y;

      snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_node (snapshot, root);

      gtk_native_get_surface_transform (GTK_NATIVE (widget), &native_x, &native_y);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &(graphene_point_t) { native_x, native_y });

      for (l = iw->overlays; l; l = l->next)
        {
          gtk_inspector_overlay_snapshot (l->data, snapshot, widget_node, widget);
        }

      gtk_snapshot_restore (snapshot);

      gsk_render_node_unref (root);
      root = gtk_snapshot_free_to_node (snapshot);
    }

  return root;
}

gboolean
gtk_inspector_is_recording (GtkWidget *widget)
{
  GtkInspectorWindow *iw;

  if (!any_inspector_window_constructed)
    return FALSE;

  iw = gtk_inspector_window_get_for_display (gtk_widget_get_display (widget));
  if (iw == NULL)
    return FALSE;

  /* sanity check for single-display GDK backends */
  if (GTK_WIDGET (iw) == widget)
    return FALSE;

  return gtk_inspector_recorder_is_recording (GTK_INSPECTOR_RECORDER (iw->widget_recorder));
}

gboolean
gtk_inspector_handle_event (GdkEvent *event)
{
  GtkInspectorWindow *iw;
  gboolean handled = FALSE;

  if (!any_inspector_window_constructed)
    return FALSE;

  iw = gtk_inspector_window_get_for_display (gdk_event_get_display (event));
  if (iw == NULL)
    return FALSE;

  gtk_inspector_recorder_record_event (GTK_INSPECTOR_RECORDER (iw->widget_recorder),
                                       gtk_get_event_widget (event),
                                       event);

  g_signal_emit (iw, signals[EVENT], 0, event, &handled);

  return handled;
}

GdkDisplay *
gtk_inspector_window_get_inspected_display (GtkInspectorWindow *iw)
{
  return iw->inspected_display;
}

static void
update_go_button (GtkWidget  *button,
                  gboolean    enabled,
                  const char *tooltip)
{
  gtk_widget_set_sensitive (button, enabled);
  gtk_widget_set_tooltip_text (button, tooltip);
}

static void
update_go_buttons (GtkInspectorWindow *iw)
{
  GObject *parent;
  GObject *object;
  ChildKind kind;
  guint position;

  if (iw->objects->len > 1)
    {
      ChildData *data = &g_array_index (iw->objects, ChildData, iw->objects->len - 2);
      parent = data->object;
    }
  else
    {
      parent = NULL;
    }

  if (iw->objects->len > 0)
    {
      ChildData *data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
      object = data->object;
      kind = data->kind;
      position = data->position;
    }
  else
    {
      object = NULL;
      kind = CHILD_KIND_OTHER;
      position = 0;
    }

  if (parent)
    {
      char *text;
      text = g_strdup_printf ("Go to %s", G_OBJECT_TYPE_NAME (parent));
      update_go_button (iw->go_up_button, TRUE, text);
      g_free (text);
    }
  else
    {
      update_go_button (iw->go_up_button, GTK_IS_WIDGET (object) && !GTK_IS_ROOT (object), "Parent widget");
    }

  switch (kind)
    {
    case CHILD_KIND_WIDGET:
      update_go_button (iw->go_down_button,
                        GTK_IS_WIDGET (object) &&gtk_widget_get_first_child (GTK_WIDGET (object)) != NULL,
                        "First child");
      update_go_button (iw->go_previous_button,
                        GTK_IS_WIDGET (object) && gtk_widget_get_prev_sibling (GTK_WIDGET (object)) != NULL,
                        "Previous sibling");
      update_go_button (iw->go_next_button,
                        GTK_IS_WIDGET (object) && gtk_widget_get_next_sibling (GTK_WIDGET (object)) != NULL,
                        "Next sibling");
      gtk_widget_set_visible (iw->list_position_label, FALSE);
      break;
    case CHILD_KIND_LISTITEM:
      update_go_button (iw->go_down_button, FALSE, NULL);
      update_go_button (iw->go_previous_button, position > 0, "Previous list item");
      update_go_button (iw->go_next_button, position + 1 < g_list_model_get_n_items (G_LIST_MODEL (parent)), "Next list item");
      {
        char *text = g_strdup_printf ("%u", position);
        gtk_label_set_label (GTK_LABEL (iw->list_position_label), text);
        g_free (text);
        gtk_widget_set_visible (iw->list_position_label, TRUE);
      }
      break;
    case CHILD_KIND_PROPERTY:
    case CHILD_KIND_CONTROLLER:
    case CHILD_KIND_OTHER:
      update_go_button (iw->go_down_button, FALSE, NULL);
      update_go_button (iw->go_previous_button, FALSE, NULL);
      update_go_button (iw->go_next_button, FALSE, NULL);
      gtk_widget_set_visible (iw->list_position_label, FALSE);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
show_object_details (GtkInspectorWindow *iw,
                     GObject            *object,
                     const char         *tab)
{
  set_selected_object (iw, object);

  if (tab)
    gtk_stack_set_visible_child_name (GTK_STACK (iw->object_details), tab);
  if (!gtk_stack_get_visible_child_name (GTK_STACK (iw->object_details)))
    gtk_stack_set_visible_child_name (GTK_STACK (iw->object_details), "properties");

  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-details");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "details");
}

void
gtk_inspector_window_push_object (GtkInspectorWindow *iw,
                                  GObject            *object,
                                  ChildKind           kind,
                                  guint               position)
{
  ChildData data;

  data.kind = kind;
  data.object = object;
  data.position = position;
  g_array_append_val (iw->objects, data);
  show_object_details (iw, object, "properties");
  update_go_buttons (iw);
}

void
gtk_inspector_window_pop_object (GtkInspectorWindow *iw)
{
  ChildData *data;
  const char *tabs[] = {
    "properties",
    "controllers",
    "properties",
    "list-data",
    "misc",
  };
  const char *tab;

  if (iw->objects->len < 2)
    {
      gtk_widget_error_bell (GTK_WIDGET (iw));
      return;
    }

  data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
  tab = tabs[data->kind];
  g_array_remove_index (iw->objects, iw->objects->len - 1);
  data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
  show_object_details (iw, data->object, tab);
  update_go_buttons (iw);
}

void
gtk_inspector_window_replace_object (GtkInspectorWindow *iw,
                                     GObject            *object,
                                     ChildKind           kind,
                                     guint               position)
{
  ChildData *data;

  data = &g_array_index (iw->objects, ChildData, iw->objects->len - 1);
  g_assert (data->kind == kind);
  data->object = object;
  data->position = position;
  show_object_details (iw, object, NULL);
  update_go_buttons (iw);
}

void
gtk_inspector_window_set_object (GtkInspectorWindow *iw,
                                 GObject            *object,
                                 ChildKind           kind,
                                 guint               position)
{
  g_array_set_size (iw->objects, 0);
  gtk_inspector_window_push_object (iw, object, kind, position);
  update_go_buttons (iw);
}

// vim: set et sw=2 ts=2:

