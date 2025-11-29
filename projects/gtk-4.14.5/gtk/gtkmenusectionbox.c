/*
 * Copyright © 2014 Canonical Limited
 * Copyright © 2013 Carlos Garnacho
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmenusectionboxprivate.h"

#include "gtkwidgetprivate.h"
#include "gtklabel.h"
#include "gtkmenutrackerprivate.h"
#include "gtkmodelbuttonprivate.h"
#include "gtkseparator.h"
#include "gtksizegroup.h"
#include "gtkstack.h"
#include "gtkpopovermenuprivate.h"
#include "gtkorientable.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkgizmoprivate.h"
#include "gtkbinlayout.h"
#include "gtkprivate.h"

typedef GtkBoxClass GtkMenuSectionBoxClass;

struct _GtkMenuSectionBox
{
  GtkBox               parent_instance;

  GtkMenuSectionBox   *toplevel;
  GtkMenuTracker      *tracker;
  GtkBox              *item_box;
  GtkWidget           *separator;
  guint                separator_sync_idle;
  gboolean             iconic;
  gboolean             inline_buttons;
  gboolean             circular;
  int                  depth;
  GtkPopoverMenuFlags  flags;
  GtkSizeGroup        *indicators;
  GHashTable          *custom_slots;
};

typedef struct
{
  int      n_items;
  gboolean previous_is_iconic;
} MenuData;

G_DEFINE_TYPE (GtkMenuSectionBox, gtk_menu_section_box, GTK_TYPE_BOX)

static void        gtk_menu_section_box_sync_separators (GtkMenuSectionBox  *box,
                                                         MenuData           *data);
static void        gtk_menu_section_box_new_submenu     (GtkMenuTrackerItem *item,
                                                         GtkMenuSectionBox  *toplevel,
                                                         GtkWidget          *focus,
                                                         const char         *name);
static GtkWidget * gtk_menu_section_box_new_section     (GtkMenuTrackerItem *item,
                                                         GtkMenuSectionBox  *parent);

/* We are trying to implement the following rules here:
 *
 * rule 1: never ever show separators for empty sections
 * rule 2: always show a separator if there is a label
 * rule 3: don't show a separator for the first section
 * rule 4: don't show a separator for the following sections if there are
 *         no items before it
 * rule 5: never show separators directly above or below an iconic box
 * (rule 6: these rules don't apply exactly the same way for subsections)
 */
static void
gtk_menu_section_box_sync_separators (GtkMenuSectionBox *box,
                                      MenuData          *data)
{
  gboolean previous_section_is_iconic;
  gboolean should_have_separator;
  gboolean should_have_top_margin = FALSE;
  gboolean is_not_empty_item;
  gboolean has_separator;
  gboolean has_label;
  gboolean separator_condition;
  int n_items_before;
  GtkWidget *child;

  n_items_before =  data->n_items;
  previous_section_is_iconic = data->previous_is_iconic;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (box->item_box));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_MENU_SECTION_BOX (child))
        gtk_menu_section_box_sync_separators (GTK_MENU_SECTION_BOX (child), data);
      else
        data->n_items++;
    }

  is_not_empty_item = (data->n_items > n_items_before);

  if (is_not_empty_item)
    data->previous_is_iconic = box->iconic;

  if (box->separator == NULL)
    return;

  has_separator = gtk_widget_get_parent (box->separator) != NULL;
  has_label = !GTK_IS_SEPARATOR (box->separator);

  separator_condition = has_label ? TRUE : n_items_before > 0 &&
                                           box->depth <= 1 &&
                                           !previous_section_is_iconic &&
                                           !box->iconic;

  should_have_separator = separator_condition && is_not_empty_item;

  should_have_top_margin = !should_have_separator &&
                           (box->depth <= 1 || box->iconic || box->circular) &&
                           n_items_before > 0 &&
                           is_not_empty_item;

  gtk_widget_set_margin_top (GTK_WIDGET (box->item_box), should_have_top_margin ? 10 : 0);

  if (has_label)
    {
      GtkWidget *separator = gtk_widget_get_first_child (box->separator);

      gtk_widget_set_visible (separator, n_items_before > 0);
    }

  if (should_have_separator == has_separator)
    return;

  if (should_have_separator)
    gtk_box_insert_child_after (GTK_BOX (box), box->separator, NULL);
  else
    gtk_box_remove (GTK_BOX (box), box->separator);
}

static gboolean
gtk_menu_section_box_handle_sync_separators (gpointer user_data)
{
  GtkMenuSectionBox *box = user_data;
  MenuData data;

  data.n_items = 0;
  data.previous_is_iconic = FALSE;
  gtk_menu_section_box_sync_separators (box, &data);

  box->separator_sync_idle = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_menu_section_box_schedule_separator_sync (GtkMenuSectionBox *box)
{
  box = box->toplevel;

  if (!box->separator_sync_idle)
    {
      box->separator_sync_idle = g_idle_add_full (G_PRIORITY_HIGH_IDLE, /* before resize... */
                                                  gtk_menu_section_box_handle_sync_separators,
                                                  box, NULL);
      gdk_source_set_static_name_by_id (box->separator_sync_idle, "[gtk] menu section box handle sync separators");
    }
}

static void
gtk_popover_item_activate (GtkWidget *button,
                           gpointer   user_data)
{
  GtkMenuTrackerItem *item = user_data;
  GtkWidget *popover = NULL;

  if (gtk_menu_tracker_item_get_role (item) == GTK_MENU_TRACKER_ITEM_ROLE_NORMAL)
    {
      /* Activating the item could cause the popover
       * to be free'd, for example if it is a Quit item
       */
      popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
      if (popover)
        g_object_ref (popover);
    }

  gtk_menu_tracker_item_activated (item);

  if (popover != NULL)
    {
      gtk_widget_set_visible (popover, FALSE);
      g_object_unref (popover);
    }
}

static void
gtk_menu_section_box_remove_func (int      position,
                                  gpointer user_data)
{
  GtkMenuSectionBox *box = user_data;
  GtkMenuTrackerItem *item;
  GtkWidget *widget;
  int pos;

  for (widget = gtk_widget_get_first_child (GTK_WIDGET (box->item_box)), pos = 0;
       widget != NULL;
       widget = gtk_widget_get_next_sibling (widget), pos++)
    {
      if (pos == position)
        break;
    }

  item = g_object_get_data (G_OBJECT (widget), "GtkMenuTrackerItem");
  if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      GtkWidget *stack, *subbox;

      stack = gtk_widget_get_ancestor (GTK_WIDGET (box->toplevel), GTK_TYPE_STACK);
      subbox = gtk_stack_get_child_by_name (GTK_STACK (stack), gtk_menu_tracker_item_get_label (item));
      if (subbox != NULL)
        gtk_stack_remove (GTK_STACK (stack), subbox);
    }

  gtk_box_remove (GTK_BOX (box->item_box), widget);

  gtk_menu_section_box_schedule_separator_sync (box);
}

static gboolean
get_ancestors (GtkWidget  *widget,
               GType       widget_type,
               GtkWidget **ancestor,
               GtkWidget **below)
{
  GtkWidget *a, *b;

  a = NULL;
  b = widget;
  while (b != NULL)
    {
      a = gtk_widget_get_parent (b);
      if (!a)
        return FALSE;
      if (g_type_is_a (G_OBJECT_TYPE (a), widget_type))
        break;
      b = a;
    }

  *below = b;
  *ancestor = a;

  return TRUE;
}

static void
close_submenu (GtkWidget *button,
               gpointer   data)
{
  GtkMenuTrackerItem *item = data;
  GtkWidget *focus;

  if (gtk_menu_tracker_item_get_should_request_show (item))
    gtk_menu_tracker_item_request_submenu_shown (item, FALSE);

  focus = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "focus"));
  gtk_widget_grab_focus (focus);
}

static void
open_submenu (GtkWidget *button,
              gpointer   data)
{
  GtkMenuTrackerItem *item = data;
  GtkWidget *focus;

  if (gtk_menu_tracker_item_get_should_request_show (item))
    gtk_menu_tracker_item_request_submenu_shown (item, TRUE);

  focus = GTK_WIDGET (g_object_get_data (G_OBJECT (button), "focus"));
  gtk_widget_grab_focus (focus);
}

static void
submenu_shown (GtkPopoverMenu     *popover,
               GtkMenuTrackerItem *item)
{
  if (gtk_menu_tracker_item_get_should_request_show (item))
    gtk_menu_tracker_item_request_submenu_shown (item, TRUE);
}

static void
submenu_hidden (GtkPopoverMenu     *popover,
                GtkMenuTrackerItem *item)
{
  if (gtk_menu_tracker_item_get_should_request_show (item))
    gtk_menu_tracker_item_request_submenu_shown (item, FALSE);
}

/* We're using the gizmo as an easy single child container, not as
 * a custom widget to draw some visual indicators (like markers).
 * As such its default focus functions blocks focus through the children,
 * so we need to handle it correctly here so that custom widgets inside
 * menus can be focused.
 */
static gboolean
custom_widget_focus (GtkGizmo        *gizmo,
                     GtkDirectionType direction)
{
  return gtk_widget_focus_child (GTK_WIDGET (gizmo), direction);
}

static gboolean
custom_widget_grab_focus (GtkGizmo *gizmo)
{
  return gtk_widget_grab_focus_child (GTK_WIDGET (gizmo));
}

static void
gtk_menu_section_box_insert_func (GtkMenuTrackerItem *item,
                                  int                 position,
                                  gpointer            user_data)
{
  GtkMenuSectionBox *box = user_data;
  GtkWidget *widget;

  if (gtk_menu_tracker_item_get_is_separator (item))
    {
      widget = gtk_menu_section_box_new_section (item, box);
    }
  else if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      if (box->flags & GTK_POPOVER_MENU_NESTED)
        {
          GMenuModel *model;
          GtkWidget *submenu;

          model = _gtk_menu_tracker_item_get_link (item, G_MENU_LINK_SUBMENU);

          submenu = gtk_popover_menu_new_from_model_full (model, box->flags);
          gtk_popover_set_has_arrow (GTK_POPOVER (submenu), FALSE);
          gtk_widget_set_valign (submenu, GTK_ALIGN_START);

          widget = g_object_new (GTK_TYPE_MODEL_BUTTON,
                                 "popover", submenu,
                                 "indicator-size-group", box->indicators,
                                 NULL);
          g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);
          g_object_bind_property (item, "icon", widget, "icon", G_BINDING_SYNC_CREATE);
          g_object_bind_property (item, "use-markup", widget, "use-markup", G_BINDING_SYNC_CREATE);
          g_object_bind_property (item, "sensitive", widget, "sensitive", G_BINDING_SYNC_CREATE);

          g_signal_connect (submenu, "show", G_CALLBACK (submenu_shown), item);
          g_signal_connect (submenu, "hide", G_CALLBACK (submenu_hidden), item);
        }
      else
        {
          GtkWidget *stack = NULL;
          GtkWidget *parent = NULL;
          char *name;

          widget = g_object_new (GTK_TYPE_MODEL_BUTTON,
                                 "menu-name", gtk_menu_tracker_item_get_label (item),
                                 "indicator-size-group", box->indicators,
                                 NULL);
          g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);
          g_object_bind_property (item, "icon", widget, "icon", G_BINDING_SYNC_CREATE);
          g_object_bind_property (item, "use-markup", widget, "use-markup", G_BINDING_SYNC_CREATE);
          g_object_bind_property (item, "sensitive", widget, "sensitive", G_BINDING_SYNC_CREATE);

          get_ancestors (GTK_WIDGET (box->toplevel), GTK_TYPE_STACK, &stack, &parent);
          g_object_get (gtk_stack_get_page (GTK_STACK (stack), parent), "name", &name, NULL);
          gtk_menu_section_box_new_submenu (item, box->toplevel, widget, name);
          g_free (name);
        }
    }
  else if (gtk_menu_tracker_item_get_custom (item))
    {
      const char *id = gtk_menu_tracker_item_get_custom (item);

      widget = gtk_gizmo_new ("widget", NULL, NULL, NULL, NULL, custom_widget_focus, custom_widget_grab_focus);
      gtk_widget_set_layout_manager (widget, gtk_bin_layout_new ());

      if (g_hash_table_lookup (box->custom_slots, id))
        g_warning ("Duplicate custom ID: %s", id);
      else
        {
          char *slot_id = g_strdup (id);
          g_object_set_data_full (G_OBJECT (widget), "slot-id", slot_id, g_free);
          g_hash_table_insert (box->custom_slots, slot_id, widget);
        }
    }
  else
    {
      widget = g_object_new (GTK_TYPE_MODEL_BUTTON,
                             "indicator-size-group", box->indicators,
                             NULL);
      g_object_bind_property (item, "label", widget, "text", G_BINDING_SYNC_CREATE);

      if (box->iconic)
        {
          g_object_bind_property (item, "verb-icon", widget, "icon", G_BINDING_SYNC_CREATE);
          g_object_set (widget, "iconic", TRUE, NULL);
        }
      else if (box->inline_buttons)
        {
          g_object_bind_property (item, "verb-icon", widget, "icon", G_BINDING_SYNC_CREATE);
          g_object_set (widget, "iconic", TRUE, NULL);
          gtk_widget_add_css_class (widget, "flat");
        }
      else if (box->circular)
        {
          g_object_bind_property (item, "verb-icon", widget, "icon", G_BINDING_SYNC_CREATE);
          g_object_set (widget, "iconic", TRUE, NULL);
          gtk_widget_add_css_class (widget, "circular");
        }
      else
        g_object_bind_property (item, "icon", widget, "icon", G_BINDING_SYNC_CREATE);

      g_object_bind_property (item, "use-markup", widget, "use-markup", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "sensitive", widget, "sensitive", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "role", widget, "role", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "toggled", widget, "active", G_BINDING_SYNC_CREATE);
      g_object_bind_property (item, "accel", widget, "accel", G_BINDING_SYNC_CREATE);
      g_signal_connect (widget, "clicked", G_CALLBACK (gtk_popover_item_activate), item);
    }

  g_object_set_data_full (G_OBJECT (widget), "GtkMenuTrackerItem", g_object_ref (item), g_object_unref);

  if (box->circular)
    {
      gtk_widget_set_hexpand (widget, TRUE);
      gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
    }
  else
    {
      gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
    }
  gtk_box_append (GTK_BOX (box->item_box), widget);

  if (position == 0)
    gtk_box_reorder_child_after (GTK_BOX (box->item_box), widget, NULL);
  else
    {
      GtkWidget *sibling = gtk_widget_get_first_child (GTK_WIDGET (box->item_box));
      int i;
      for (i = 1; i < position; i++)
        sibling = gtk_widget_get_next_sibling (sibling);
      gtk_box_reorder_child_after (GTK_BOX (box->item_box), widget, sibling);
    }

  if (box->circular)
    {
      GtkWidget *c1, *c2, *c3;

      /* special-case the n > 2 case */
      c1 = gtk_widget_get_first_child (GTK_WIDGET (box->item_box));
      if ((c2 = gtk_widget_get_next_sibling (c1)) != NULL &&
          (c3 = gtk_widget_get_next_sibling (c2)) != NULL)
        {
          gtk_widget_set_halign (c1, GTK_ALIGN_START);
          while (c3 != NULL)
            {
              gtk_widget_set_halign (c2, GTK_ALIGN_CENTER);
              c2 = c3;
              c3 = gtk_widget_get_next_sibling (c3);
            }
          gtk_widget_set_halign (c2, GTK_ALIGN_END);
        }
    }

  gtk_menu_section_box_schedule_separator_sync (box);
}

static void
gtk_menu_section_box_init (GtkMenuSectionBox *box)
{
  GtkWidget *item_box;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);

  box->toplevel = box;

  item_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  box->item_box = GTK_BOX (item_box);
  gtk_box_append (GTK_BOX (box), item_box);
  gtk_widget_set_halign (GTK_WIDGET (item_box), GTK_ALIGN_FILL);
  gtk_widget_set_halign (GTK_WIDGET (box), GTK_ALIGN_FILL);
}

static void
gtk_menu_section_box_dispose (GObject *object)
{
  GtkMenuSectionBox *box = GTK_MENU_SECTION_BOX (object);

  if (box->separator_sync_idle)
    {
      g_source_remove (box->separator_sync_idle);
      box->separator_sync_idle = 0;
    }

  g_clear_object (&box->separator);

  if (box->tracker)
    {
      gtk_menu_tracker_free (box->tracker);
      box->tracker = NULL;
    }

  g_clear_object (&box->indicators);
  g_clear_pointer (&box->custom_slots, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_menu_section_box_parent_class)->dispose (object);
}

static void
gtk_menu_section_box_class_init (GtkMenuSectionBoxClass *class)
{
  G_OBJECT_CLASS (class)->dispose = gtk_menu_section_box_dispose;
}

static void
update_popover_position_cb (GObject    *source,
                            GParamSpec *spec,
                            gpointer   *user_data)
{
  GtkPopover *popover = GTK_POPOVER (source);
  GtkMenuSectionBox *box = GTK_MENU_SECTION_BOX (user_data);
  GtkWidget *w;

  GtkPositionType new_pos = gtk_popover_get_position (popover);

  for (w = gtk_widget_get_first_child (gtk_widget_get_parent (GTK_WIDGET (box)));
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      if (new_pos == GTK_POS_BOTTOM)
        gtk_widget_set_valign (w, GTK_ALIGN_START);
      else if (new_pos == GTK_POS_TOP)
        gtk_widget_set_valign (w, GTK_ALIGN_END);
      else
        gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
    }
}

void
gtk_menu_section_box_new_toplevel (GtkPopoverMenu      *popover,
                                   GMenuModel          *model,
                                   GtkPopoverMenuFlags  flags)
{
  GtkMenuSectionBox *box;

  box = g_object_new (GTK_TYPE_MENU_SECTION_BOX, NULL);
  box->indicators = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  box->custom_slots = g_hash_table_new (g_str_hash, g_str_equal);
  box->flags = flags;

  gtk_popover_menu_add_submenu (popover, GTK_WIDGET (box), "main");

  box->tracker = gtk_menu_tracker_new (GTK_ACTION_OBSERVABLE (_gtk_widget_get_action_muxer (GTK_WIDGET (box), TRUE)),
                                       model, TRUE, FALSE, FALSE, NULL,
                                       gtk_menu_section_box_insert_func,
                                       gtk_menu_section_box_remove_func, box);

  g_signal_connect (G_OBJECT (popover), "notify::position", G_CALLBACK (update_popover_position_cb), box);
}

static void
gtk_menu_section_box_new_submenu (GtkMenuTrackerItem *item,
                                  GtkMenuSectionBox  *toplevel,
                                  GtkWidget          *focus,
                                  const char         *name)
{
  GtkMenuSectionBox *box;
  GtkWidget *button;

  box = g_object_new (GTK_TYPE_MENU_SECTION_BOX, NULL);
  box->indicators = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  box->custom_slots = g_hash_table_ref (toplevel->custom_slots);
  box->flags = toplevel->flags;

  button = g_object_new (GTK_TYPE_MODEL_BUTTON,
                         "menu-name", name,
                         "role", GTK_BUTTON_ROLE_TITLE,
                         NULL);

  g_object_bind_property (item, "label", button, "text", G_BINDING_SYNC_CREATE);
  g_object_bind_property (item, "icon", button, "icon", G_BINDING_SYNC_CREATE);

  g_object_set_data (G_OBJECT (button), "focus", focus);
  g_object_set_data (G_OBJECT (focus), "focus", button);

  gtk_box_insert_child_after (GTK_BOX (box), button, NULL);

  g_signal_connect (focus, "clicked", G_CALLBACK (open_submenu), item);
  g_signal_connect (button, "clicked", G_CALLBACK (close_submenu), item);

  gtk_stack_add_named (GTK_STACK (gtk_widget_get_ancestor (GTK_WIDGET (toplevel), GTK_TYPE_STACK)),
                       GTK_WIDGET (box), gtk_menu_tracker_item_get_label (item));

  box->tracker = gtk_menu_tracker_new_for_item_link (item, G_MENU_LINK_SUBMENU, FALSE, FALSE,
                                                     gtk_menu_section_box_insert_func,
                                                     gtk_menu_section_box_remove_func,
                                                     box);
}

static GtkWidget *
gtk_menu_section_box_new_section (GtkMenuTrackerItem *item,
                                  GtkMenuSectionBox  *parent)
{
  GtkMenuSectionBox *box;
  const char *label;
  const char *hint;
  const char *text_direction;

  box = g_object_new (GTK_TYPE_MENU_SECTION_BOX, NULL);
  box->indicators = g_object_ref (parent->indicators);
  box->custom_slots = g_hash_table_ref (parent->toplevel->custom_slots);
  box->toplevel = parent->toplevel;
  box->depth = parent->depth + 1;
  box->flags = parent->flags;

  label = gtk_menu_tracker_item_get_label (item);
  hint = gtk_menu_tracker_item_get_display_hint (item);
  text_direction = gtk_menu_tracker_item_get_text_direction (item);

  if (hint && g_str_equal (hint, "horizontal-buttons"))
    {
      gtk_box_set_homogeneous (box->item_box, TRUE);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (box->item_box), GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_add_css_class (GTK_WIDGET (box->item_box), "linked");
      gtk_widget_add_css_class (GTK_WIDGET (box->item_box), "horizontal-buttons");
      box->iconic = TRUE;

      if (text_direction)
        {
          GtkTextDirection dir = GTK_TEXT_DIR_NONE;

          if (g_str_equal (text_direction, "rtl"))
            dir = GTK_TEXT_DIR_RTL;
          else if (g_str_equal (text_direction, "ltr"))
            dir = GTK_TEXT_DIR_LTR;

          gtk_widget_set_direction (GTK_WIDGET (box->item_box), dir);
        }
    }
  else if (hint && g_str_equal (hint, "inline-buttons"))
    {
      GtkWidget *item_box;
      GtkWidget *spacer;

      box->inline_buttons = TRUE;

      gtk_orientable_set_orientation (GTK_ORIENTABLE (box->item_box), GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_add_css_class (GTK_WIDGET (box->item_box), "inline-buttons");

      spacer = gtk_gizmo_new ("none", NULL, NULL, NULL,NULL, NULL, NULL);
      gtk_box_append (GTK_BOX (box->item_box), spacer);
      gtk_size_group_add_widget (box->indicators, spacer);

      if (label != NULL)
        {
          GtkWidget *title;

          title = gtk_label_new (label);
          gtk_widget_set_hexpand (title, TRUE);
          gtk_widget_set_halign (title, GTK_ALIGN_START);
          g_object_bind_property (item, "label", title, "label", G_BINDING_SYNC_CREATE);
          gtk_box_append (GTK_BOX (box->item_box), title);
        }

      item_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box->item_box), item_box);
      box->item_box = GTK_BOX (item_box);
    }
  else if (hint && g_str_equal (hint, "circular-buttons"))
    {
      gtk_box_set_homogeneous (box->item_box, TRUE);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (box->item_box), GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_add_css_class (GTK_WIDGET (box->item_box), "circular-buttons");
      box->circular = TRUE;
    }

  if (label != NULL && !box->inline_buttons)
    {
      GtkWidget *separator;
      GtkWidget *title;

      box->separator = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      g_object_ref_sink (box->separator);

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_set_valign (separator, GTK_ALIGN_CENTER);
      gtk_widget_set_hexpand (separator, TRUE);
      gtk_box_append (GTK_BOX (box->separator), separator);

      title = gtk_label_new (label);
      g_object_bind_property (item, "label", title, "label", G_BINDING_SYNC_CREATE);
      gtk_widget_add_css_class (title, "separator");
      gtk_widget_set_halign (title, GTK_ALIGN_START);
      gtk_label_set_xalign (GTK_LABEL (title), 0.0);
      gtk_widget_add_css_class (title, "title");
      gtk_box_append (GTK_BOX (box->separator), title);
    }
  else
    {
      box->separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      g_object_ref_sink (box->separator);
    }

  box->tracker = gtk_menu_tracker_new_for_item_link (item, G_MENU_LINK_SECTION, FALSE, FALSE,
                                                     gtk_menu_section_box_insert_func,
                                                     gtk_menu_section_box_remove_func,
                                                     box);

  return GTK_WIDGET (box);
}

gboolean
gtk_menu_section_box_add_custom (GtkPopoverMenu *popover,
                                 GtkWidget      *child,
                                 const char     *id)
{
  GtkWidget *stack;
  GtkMenuSectionBox *box;
  GtkWidget *slot;

  stack = gtk_popover_menu_get_stack (popover);
  box = GTK_MENU_SECTION_BOX (gtk_stack_get_child_by_name (GTK_STACK (stack), "main"));
  if (box == NULL)
    return FALSE;

  slot = (GtkWidget *)g_hash_table_lookup (box->custom_slots, id);

  if (slot == NULL)
    return FALSE;

  if (gtk_widget_get_first_child (slot))
    return FALSE;

  gtk_widget_insert_before (child, slot, NULL);
  return TRUE;
}

gboolean
gtk_menu_section_box_remove_custom (GtkPopoverMenu *popover,
                                    GtkWidget      *child)
{
  GtkWidget *stack;
  GtkMenuSectionBox *box;
  GtkWidget *parent;
  const char *id;
  GtkWidget *slot;

  stack = gtk_popover_menu_get_stack (popover);
  box = GTK_MENU_SECTION_BOX (gtk_stack_get_child_by_name (GTK_STACK (stack), "main"));
  if (box == NULL)
    return FALSE;

  parent = gtk_widget_get_parent (child);

  id = (const char *) g_object_get_data (G_OBJECT (parent), "slot-id");
  g_return_val_if_fail (id != NULL, FALSE);

  slot = (GtkWidget *)g_hash_table_lookup (box->custom_slots, id);

  if (slot != parent)
    return FALSE;

  gtk_widget_unparent (child);

  return TRUE;
}
