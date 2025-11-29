/* gtkemojichooser.c: An Emoji chooser widget
 * Copyright 2017, Red Hat, Inc.
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

#include "gtkemojichooser.h"

#include "gtkadjustmentprivate.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtkflowboxprivate.h"
#include "gtkstack.h"
#include "gtklabel.h"
#include "gtkgesturelongpress.h"
#include "gtkpopover.h"
#include "gtkscrolledwindow.h"
#include "gtksearchentryprivate.h"
#include "gtktext.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"

/**
 * GtkEmojiChooser:
 *
 * The `GtkEmojiChooser` is used by text widgets such as `GtkEntry` or
 * `GtkTextView` to let users insert Emoji characters.
 *
 * ![An example GtkEmojiChooser](emojichooser.png)
 *
 * `GtkEmojiChooser` emits the [signal@Gtk.EmojiChooser::emoji-picked]
 * signal when an Emoji is selected.
 *
 * # CSS nodes
 *
 * ```
 * popover
 * ├── box.emoji-searchbar
 * │   ╰── entry.search
 * ╰── box.emoji-toolbar
 *     ├── button.image-button.emoji-section
 *     ├── ...
 *     ╰── button.image-button.emoji-section
 * ```
 *
 * Every `GtkEmojiChooser` consists of a main node called popover.
 * The contents of the popover are largely implementation defined
 * and supposed to inherit general styles.
 * The top searchbar used to search emoji and gets the .emoji-searchbar
 * style class itself.
 * The bottom toolbar used to switch between different emoji categories
 * consists of buttons with the .emoji-section style class and gets the
 * .emoji-toolbar style class itself.
 */

#define BOX_SPACE 6

GType gtk_emoji_chooser_child_get_type (void);

#define GTK_TYPE_EMOJI_CHOOSER_CHILD (gtk_emoji_chooser_child_get_type ())

typedef struct
{
  GtkFlowBoxChild parent;
  GtkWidget *variations;
} GtkEmojiChooserChild;

typedef struct
{
  GtkFlowBoxChildClass parent_class;
} GtkEmojiChooserChildClass;

G_DEFINE_TYPE (GtkEmojiChooserChild, gtk_emoji_chooser_child, GTK_TYPE_FLOW_BOX_CHILD)

static void
gtk_emoji_chooser_child_init (GtkEmojiChooserChild *child)
{
}

static void
gtk_emoji_chooser_child_dispose (GObject *object)
{
  GtkEmojiChooserChild *child = (GtkEmojiChooserChild *)object;

  g_clear_pointer (&child->variations, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_emoji_chooser_child_parent_class)->dispose (object);
}

static void
gtk_emoji_chooser_child_size_allocate (GtkWidget *widget,
                                       int        width,
                                       int        height,
                                       int        baseline)
{
  GtkEmojiChooserChild *child = (GtkEmojiChooserChild *)widget;

  GTK_WIDGET_CLASS (gtk_emoji_chooser_child_parent_class)->size_allocate (widget, width, height, baseline);
  if (child->variations)
    gtk_popover_present (GTK_POPOVER (child->variations));
}

static gboolean
gtk_emoji_chooser_child_focus (GtkWidget        *widget,
                               GtkDirectionType  direction)
{
  GtkEmojiChooserChild *child = (GtkEmojiChooserChild *)widget;

  if (child->variations && gtk_widget_is_visible (child->variations))
    {
      if (gtk_widget_child_focus (child->variations, direction))
        return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_emoji_chooser_child_parent_class)->focus (widget, direction);
}

static void scroll_to_child (GtkWidget *child);

static gboolean
gtk_emoji_chooser_child_grab_focus (GtkWidget *widget)
{
  gtk_widget_grab_focus_self (widget);
  scroll_to_child (widget);
  return TRUE;
}

static void show_variations (GtkEmojiChooser *chooser,
                             GtkWidget       *child);

static void
gtk_emoji_chooser_child_popup_menu (GtkWidget  *widget,
                                    const char *action_name,
                                    GVariant   *parameters)
{
  GtkWidget *chooser;

  chooser = gtk_widget_get_ancestor (widget, GTK_TYPE_EMOJI_CHOOSER);

  show_variations (GTK_EMOJI_CHOOSER (chooser), widget);
}

static void
gtk_emoji_chooser_child_class_init (GtkEmojiChooserChildClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_emoji_chooser_child_dispose;
  widget_class->size_allocate = gtk_emoji_chooser_child_size_allocate;
  widget_class->focus = gtk_emoji_chooser_child_focus;
  widget_class->grab_focus = gtk_emoji_chooser_child_grab_focus;

  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, gtk_emoji_chooser_child_popup_menu);

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);

  gtk_widget_class_set_css_name (widget_class, "emoji");
}

typedef struct {
  GtkWidget *box;
  GtkWidget *heading;
  GtkWidget *button;
  int group;
  gunichar label;
  gboolean empty;
} EmojiSection;

struct _GtkEmojiChooser
{
  GtkPopover parent_instance;

  GtkWidget *search_entry;
  GtkWidget *stack;
  GtkWidget *scrolled_window;

  int emoji_max_width;

  EmojiSection recent;
  EmojiSection people;
  EmojiSection body;
  EmojiSection nature;
  EmojiSection food;
  EmojiSection travel;
  EmojiSection activities;
  EmojiSection objects;
  EmojiSection symbols;
  EmojiSection flags;

  GVariant *data;
  GtkWidget *box;
  GVariantIter *iter;
  guint populate_idle;

  GSettings *settings;
};

struct _GtkEmojiChooserClass {
  GtkPopoverClass parent_class;
};

enum {
  EMOJI_PICKED,
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL];

G_DEFINE_TYPE (GtkEmojiChooser, gtk_emoji_chooser, GTK_TYPE_POPOVER)

static void
gtk_emoji_chooser_finalize (GObject *object)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (object);

  if (chooser->populate_idle)
    g_source_remove (chooser->populate_idle);

  g_clear_pointer (&chooser->data, g_variant_unref);
  g_clear_pointer (&chooser->iter, g_variant_iter_free);
  g_clear_object (&chooser->settings);

  G_OBJECT_CLASS (gtk_emoji_chooser_parent_class)->finalize (object);
}

static void
gtk_emoji_chooser_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), GTK_TYPE_EMOJI_CHOOSER);

  G_OBJECT_CLASS (gtk_emoji_chooser_parent_class)->dispose (object);
}

static void
scroll_to_section (EmojiSection *section)
{
  GtkEmojiChooser *chooser;
  GtkAdjustment *adj;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);

  chooser = GTK_EMOJI_CHOOSER (gtk_widget_get_ancestor (section->box, GTK_TYPE_EMOJI_CHOOSER));

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));
  if (section->heading)
    {
      if (!gtk_widget_compute_bounds (section->heading, gtk_widget_get_parent (section->heading), &bounds))
        graphene_rect_init (&bounds, 0, 0, 0, 0);
    }

  gtk_adjustment_animate_to_value (adj, bounds.origin.y - BOX_SPACE);
}

static void
scroll_to_child (GtkWidget *child)
{
  GtkEmojiChooser *chooser;
  GtkAdjustment *adj;
  graphene_point_t p;
  double value;
  double page_size;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);

  chooser = GTK_EMOJI_CHOOSER (gtk_widget_get_ancestor (child, GTK_TYPE_EMOJI_CHOOSER));

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));

  if (!gtk_widget_compute_bounds (child, gtk_widget_get_parent (child), &bounds))
    graphene_rect_init (&bounds, 0, 0, 0, 0);

  value = gtk_adjustment_get_value (adj);
  page_size = gtk_adjustment_get_page_size (adj);

  if (!gtk_widget_compute_point (child, gtk_widget_get_parent (chooser->recent.box),
                                 &GRAPHENE_POINT_INIT (0, 0), &p))
    return;

  if (p.y < value)
    gtk_adjustment_animate_to_value (adj, p.y);
  else if (p.y + bounds.size.height >= value + page_size)
    gtk_adjustment_animate_to_value (adj, value + ((p.y + bounds.size.height) - (value + page_size)));
}

static void
add_emoji (GtkWidget    *box,
           gboolean      prepend,
           GVariant     *item,
           gunichar      modifier,
           GtkEmojiChooser *chooser);

#define MAX_RECENT (7*3)

static void
populate_recent_section (GtkEmojiChooser *chooser)
{
  GVariant *variant;
  GVariant *item;
  GVariantIter iter;
  gboolean empty = TRUE;

  variant = g_settings_get_value (chooser->settings, "recently-used-emoji");
  g_variant_iter_init (&iter, variant);
  while ((item = g_variant_iter_next_value (&iter)))
    {
      GVariant *emoji_data;
      gunichar modifier;

      emoji_data = g_variant_get_child_value (item, 0);
      g_variant_get_child (item, 1, "u", &modifier);
      add_emoji (chooser->recent.box, FALSE, emoji_data, modifier, chooser);
      g_variant_unref (emoji_data);
      g_variant_unref (item);
      empty = FALSE;
    }

  gtk_widget_set_visible (chooser->recent.box, !empty);
  gtk_widget_set_sensitive (chooser->recent.button, !empty);

  g_variant_unref (variant);
}

static void
add_recent_item (GtkEmojiChooser *chooser,
                 GVariant        *item,
                 gunichar         modifier)
{
  GList *children, *l;
  int i;
  GVariantBuilder builder;
  GtkWidget *child;

  g_variant_ref (item);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a((aussasasu)u)"));
  g_variant_builder_add (&builder, "(@(aussasasu)u)", item, modifier);

  children = NULL;
  for (child = gtk_widget_get_last_child (chooser->recent.box);
       child != NULL;
       child = gtk_widget_get_prev_sibling (child))
    children = g_list_prepend (children, child);

  for (l = children, i = 1; l; l = l->next, i++)
    {
      GVariant *item2 = g_object_get_data (G_OBJECT (l->data), "emoji-data");
      gunichar modifier2 = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (l->data), "modifier"));

      if (modifier == modifier2 && g_variant_equal (item, item2))
        {
          gtk_flow_box_remove (GTK_FLOW_BOX (chooser->recent.box), l->data);
          i--;
          continue;
        }
      if (i >= MAX_RECENT)
        {
          gtk_flow_box_remove (GTK_FLOW_BOX (chooser->recent.box), l->data);
          continue;
        }

      g_variant_builder_add (&builder, "(@(aussasasu)u)", item2, modifier2);
    }
  g_list_free (children);

  add_emoji (chooser->recent.box, TRUE, item, modifier, chooser);

  /* Enable recent */
  gtk_widget_set_visible (chooser->recent.box, TRUE);
  gtk_widget_set_sensitive (chooser->recent.button, TRUE);

  g_settings_set_value (chooser->settings, "recently-used-emoji", g_variant_builder_end (&builder));

  g_variant_unref (item);
}

static gboolean
should_close (GtkEmojiChooser *chooser)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *device;
  GdkModifierType state;

  display = gtk_widget_get_display (GTK_WIDGET (chooser));
  seat = gdk_display_get_default_seat (display);
  device = gdk_seat_get_keyboard (seat);
  state = gdk_device_get_modifier_state (device);

  return (state & GDK_CONTROL_MASK) == 0;
}

static void
emoji_activated (GtkFlowBox      *box,
                 GtkFlowBoxChild *child,
                 gpointer         data)
{
  GtkEmojiChooser *chooser = data;
  char *text;
  GtkWidget *label;
  GVariant *item;
  gunichar modifier;

  if (should_close (chooser))
    gtk_popover_popdown (GTK_POPOVER (chooser));
  else
    {
      GtkWidget *popover;

      popover = gtk_widget_get_ancestor (GTK_WIDGET (box), GTK_TYPE_POPOVER);
      if (popover != GTK_WIDGET (chooser))
        gtk_popover_popdown (GTK_POPOVER (popover));
    }

  label = gtk_flow_box_child_get_child (child);
  text = g_strdup (gtk_label_get_label (GTK_LABEL (label)));

  item = (GVariant*) g_object_get_data (G_OBJECT (child), "emoji-data");
  modifier = (gunichar) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (child), "modifier"));
  add_recent_item (chooser, item, modifier);

  g_signal_emit (data, signals[EMOJI_PICKED], 0, text);
  g_free (text);
}

static gboolean
has_variations (GVariant *emoji_data)
{
  GVariant *codes;
  gsize i;
  gboolean has_variations;

  has_variations = FALSE;
  codes = g_variant_get_child_value (emoji_data, 0);
  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      gunichar code;
      g_variant_get_child (codes, i, "u", &code);
      if (code == 0 || code == 0x1f3fb)
        {
          has_variations = TRUE;
          break;
        }
    }
  g_variant_unref (codes);

  return has_variations;
}

static void
show_variations (GtkEmojiChooser *chooser,
                 GtkWidget       *child)
{
  GtkWidget *popover;
  GtkWidget *view;
  GtkWidget *box;
  GVariant *emoji_data;
  GtkWidget *parent_popover;
  gunichar modifier;
  GtkEmojiChooserChild *ch = (GtkEmojiChooserChild *)child;

  if (!child)
    return;

  emoji_data = (GVariant*) g_object_get_data (G_OBJECT (child), "emoji-data");
  if (!emoji_data)
    return;

  if (!has_variations (emoji_data))
    return;

  parent_popover = gtk_widget_get_ancestor (child, GTK_TYPE_POPOVER);
  g_clear_pointer (&ch->variations, gtk_widget_unparent);
  popover = ch->variations = gtk_popover_new ();
  gtk_popover_set_autohide (GTK_POPOVER (popover), TRUE);
  gtk_widget_set_parent (popover, child);
  view = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class (view, "view");
  box = gtk_flow_box_new ();
  gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (box), TRUE);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (box), 6);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (box), 6);
  gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (box), TRUE);
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (box), GTK_SELECTION_NONE);
  g_object_set (box, "accept-unpaired-release", TRUE, NULL);
  gtk_popover_set_child (GTK_POPOVER (popover), view);
  gtk_box_append (GTK_BOX (view), box);

  g_signal_connect (box, "child-activated", G_CALLBACK (emoji_activated), parent_popover);

  add_emoji (box, FALSE, emoji_data, 0, chooser);
  for (modifier = 0x1f3fb; modifier <= 0x1f3ff; modifier++)
    add_emoji (box, FALSE, emoji_data, modifier, chooser);

  gtk_popover_popup (GTK_POPOVER (popover));
}

static void
long_pressed_cb (GtkGesture *gesture,
                 double      x,
                 double      y,
                 gpointer    data)
{
  GtkEmojiChooser *chooser = data;
  GtkWidget *box;
  GtkWidget *child;

  box = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = GTK_WIDGET (gtk_flow_box_get_child_at_pos (GTK_FLOW_BOX (box), x, y));
  show_variations (chooser, child);
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  GtkEmojiChooser *chooser = data;
  GtkWidget *box;
  GtkWidget *child;

  box = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = GTK_WIDGET (gtk_flow_box_get_child_at_pos (GTK_FLOW_BOX (box), x, y));
  show_variations (chooser, child);
}

static void
add_emoji (GtkWidget    *box,
           gboolean      prepend,
           GVariant     *item,
           gunichar      modifier,
           GtkEmojiChooser *chooser)
{
  GtkWidget *child;
  GtkWidget *label;
  PangoAttrList *attrs;
  GVariant *codes;
  char text[64];
  char *p = text;
  int i;
  PangoLayout *layout;
  PangoRectangle rect;
  gunichar code = 0;

  codes = g_variant_get_child_value (item, 0);
  for (i = 0; i < g_variant_n_children (codes); i++)
    {
      g_variant_get_child (codes, i, "u", &code);
      if (code == 0)
        code = modifier != 0 ? modifier : 0xfe0f;
      if (code == 0x1f3fb)
        code = modifier;
      if (code != 0)
        p += g_unichar_to_utf8 (code, p);
    }
  g_variant_unref (codes);

  p[0] = 0;

  label = gtk_label_new (text);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);

  layout = gtk_label_get_layout (GTK_LABEL (label));
  pango_layout_get_extents (layout, &rect, NULL);

  /* Check for fallback rendering that generates too wide items */
  if (pango_layout_get_unknown_glyphs_count (layout) > 0 ||
      rect.width >= 1.5 * chooser->emoji_max_width)
    {
      g_object_ref_sink (label);
      g_object_unref (label);
      return;
    }

  child = g_object_new (GTK_TYPE_EMOJI_CHOOSER_CHILD, NULL);
  g_object_set_data_full (G_OBJECT (child), "emoji-data",
                          g_variant_ref (item),
                          (GDestroyNotify)g_variant_unref);
  if (modifier != 0)
    g_object_set_data (G_OBJECT (child), "modifier", GUINT_TO_POINTER (modifier));

  gtk_flow_box_child_set_child (GTK_FLOW_BOX_CHILD (child), label);
  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, prepend ? 0 : -1);
}

static GBytes *
get_emoji_data_by_language (const char *lang)
{
  GBytes *bytes;
  char *path;
  GError *error = NULL;

  path = g_strconcat ("/org/gtk/libgtk/emoji/", lang, ".data", NULL);
  bytes = g_resources_lookup_data (path, 0, &error);
  if (bytes)
    {
      g_debug ("Found emoji data for %s in resource %s", lang, path);
      g_free (path);
      return bytes;
    }

  if (g_error_matches (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND))
    {
      char *filename;
      char *gresource_name;
      GMappedFile *file;

      g_clear_error (&error);

      gresource_name = g_strconcat (lang, ".gresource", NULL);
      filename = g_build_filename (_gtk_get_data_prefix (), "share", "gtk-4.0",
                                   "emoji", gresource_name, NULL);
      g_clear_pointer (&gresource_name, g_free);
      file = g_mapped_file_new (filename, FALSE, NULL);

      if (file)
        {
          GBytes *data;
          GResource *resource;

          data = g_mapped_file_get_bytes (file);
          g_mapped_file_unref (file);

          resource = g_resource_new_from_data (data, NULL);
          g_bytes_unref (data);

          g_debug ("Registering resource for Emoji data for %s from file %s", lang, filename);
          g_resources_register (resource);
          g_resource_unref (resource);

          bytes = g_resources_lookup_data (path, 0, NULL);
          if (bytes)
            {
              g_debug ("Found emoji data for %s in resource %s", lang, path);
              g_free (path);
              g_free (filename);
              return bytes;
            }
        }

      g_free (filename);
    }

  g_clear_error (&error);
  g_free (path);

  return NULL;
}

GBytes *
get_emoji_data (void)
{
  GBytes *bytes;
  const char *lang;

  lang = pango_language_to_string (gtk_get_default_language ());
  bytes = get_emoji_data_by_language (lang);
  if (bytes)
    return bytes;

  if (strchr (lang, '-'))
    {
      char q[5];
      int i;

      for (i = 0; lang[i] != '-' && i < 4; i++)
        q[i] = lang[i];
      q[i] = '\0';

      bytes = get_emoji_data_by_language (q);
      if (bytes)
        return bytes;
    }

  bytes = get_emoji_data_by_language ("en");
  g_assert (bytes);

  return bytes;
}

static gboolean
populate_emoji_chooser (gpointer data)
{
  GtkEmojiChooser *chooser = data;
  GVariant *item;
  gint64 start, now;

  start = g_get_monotonic_time ();

  if (!chooser->data)
    {
      GBytes *bytes;

      bytes = get_emoji_data ();

      chooser->data = g_variant_ref_sink (g_variant_new_from_bytes (G_VARIANT_TYPE ("a(aussasasu)"), bytes, TRUE));
      g_bytes_unref (bytes);
    }

  if (!chooser->iter)
    {
      chooser->iter = g_variant_iter_new (chooser->data);
      chooser->box = chooser->people.box;
    }

  while ((item = g_variant_iter_next_value (chooser->iter)))
    {
      guint group;

      g_variant_get_child (item, 5, "u", &group);

      if (group == chooser->people.group)
        chooser->box = chooser->people.box;
      else if (group == chooser->body.group)
        chooser->box = chooser->body.box;
      else if (group == chooser->nature.group)
        chooser->box = chooser->nature.box;
      else if (group == chooser->food.group)
        chooser->box = chooser->food.box;
      else if (group == chooser->travel.group)
        chooser->box = chooser->travel.box;
      else if (group == chooser->activities.group)
        chooser->box = chooser->activities.box;
      else if (group == chooser->objects.group)
        chooser->box = chooser->objects.box;
      else if (group == chooser->symbols.group)
        chooser->box = chooser->symbols.box;
      else if (group == chooser->flags.group)
        chooser->box = chooser->flags.box;

      add_emoji (chooser->box, FALSE, item, 0, chooser);
      g_variant_unref (item);

      now = g_get_monotonic_time ();
      if (now > start + 200) /* 2 ms */
        {
          gdk_profiler_add_mark (start * 1000, (now - start) * 1000, "Emojichooser populate", NULL);
          return G_SOURCE_CONTINUE;
        }
    }

  g_variant_iter_free (chooser->iter);
  chooser->iter = NULL;
  chooser->box = NULL;
  chooser->populate_idle = 0;

  gdk_profiler_end_mark (start, "Emojichooser populate (finish)", NULL);

  return G_SOURCE_REMOVE;
}

static void
adj_value_changed (GtkAdjustment *adj,
                   gpointer       data)
{
  GtkEmojiChooser *chooser = data;
  double value = gtk_adjustment_get_value (adj);
  EmojiSection const *sections[] = {
    &chooser->recent,
    &chooser->people,
    &chooser->body,
    &chooser->nature,
    &chooser->food,
    &chooser->travel,
    &chooser->activities,
    &chooser->objects,
    &chooser->symbols,
    &chooser->flags,
  };
  EmojiSection const *select_section = sections[0];
  gsize i;

  /* Figure out which section the current scroll position is within */
  for (i = 0; i < G_N_ELEMENTS (sections); ++i)
    {
      EmojiSection const *section = sections[i];
      GtkWidget *child;
      graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);

      if (!gtk_widget_get_visible (section->box))
        continue;

      if (section->heading)
        child = section->heading;
      else
        child = section->box;

      if (!gtk_widget_compute_bounds (child, gtk_widget_get_parent (child), &bounds))
        graphene_rect_init (&bounds, 0, 0, 0, 0);

      if (value < bounds.origin.y - BOX_SPACE)
        break;

      select_section = section;
    }

  /* Un/Check the section buttons accordingly */
  for (i = 0; i < G_N_ELEMENTS (sections); ++i)
    {
      EmojiSection const *section = sections[i];

      if (section == select_section)
        gtk_widget_set_state_flags (section->button, GTK_STATE_FLAG_CHECKED, FALSE);
      else
        gtk_widget_unset_state_flags (section->button, GTK_STATE_FLAG_CHECKED);
    }
}

static gboolean
match_tokens (const char **term_tokens,
              const char **hit_tokens)
{
  int i, j;
  gboolean matched;

  matched = TRUE;

  for (i = 0; term_tokens[i]; i++)
    {
      for (j = 0; hit_tokens[j]; j++)
        if (g_str_has_prefix (hit_tokens[j], term_tokens[i]))
          goto one_matched;

      matched = FALSE;
      break;

one_matched:
      continue;
    }

  return matched;
}

static gboolean
filter_func (GtkFlowBoxChild *child,
             gpointer         data)
{
  EmojiSection *section = data;
  GtkEmojiChooser *chooser;
  GVariant *emoji_data;
  const char *text;
  const char *name_en;
  const char *name;
  const char **keywords_en;
  const char **keywords;
  char **term_tokens;
  char **name_tokens_en;
  char **name_tokens;
  gboolean res;

  res = TRUE;

  chooser = GTK_EMOJI_CHOOSER (gtk_widget_get_ancestor (GTK_WIDGET (child), GTK_TYPE_EMOJI_CHOOSER));
  text = gtk_editable_get_text (GTK_EDITABLE (chooser->search_entry));
  emoji_data = (GVariant *) g_object_get_data (G_OBJECT (child), "emoji-data");

  if (text[0] == 0)
    goto out;

  if (!emoji_data)
    goto out;

  term_tokens = g_str_tokenize_and_fold (text, "en", NULL);
  g_variant_get_child (emoji_data, 1, "&s", &name_en);
  name_tokens = g_str_tokenize_and_fold (name_en, "en", NULL);
  g_variant_get_child (emoji_data, 2, "&s", &name);
  name_tokens_en = g_str_tokenize_and_fold (name, "en", NULL);
  g_variant_get_child (emoji_data, 3, "^a&s", &keywords_en);
  g_variant_get_child (emoji_data, 4, "^a&s", &keywords);

  res = match_tokens ((const char **)term_tokens, (const char **)name_tokens) ||
        match_tokens ((const char **)term_tokens, (const char **)name_tokens_en) ||
        match_tokens ((const char **)term_tokens, keywords) ||
        match_tokens ((const char **)term_tokens, keywords_en);

  g_strfreev (term_tokens);
  g_strfreev (name_tokens);
  g_strfreev (name_tokens_en);

out:
  if (res)
    section->empty = FALSE;

  return res;
}

static void
invalidate_section (EmojiSection *section)
{
  section->empty = TRUE;
  gtk_flow_box_invalidate_filter (GTK_FLOW_BOX (section->box));
}

static void
update_headings (GtkEmojiChooser *chooser)
{
  gtk_widget_set_visible (chooser->people.heading, !chooser->people.empty);
  gtk_widget_set_visible (chooser->people.box, !chooser->people.empty);
  gtk_widget_set_visible (chooser->body.heading, !chooser->body.empty);
  gtk_widget_set_visible (chooser->body.box, !chooser->body.empty);
  gtk_widget_set_visible (chooser->nature.heading, !chooser->nature.empty);
  gtk_widget_set_visible (chooser->nature.box, !chooser->nature.empty);
  gtk_widget_set_visible (chooser->food.heading, !chooser->food.empty);
  gtk_widget_set_visible (chooser->food.box, !chooser->food.empty);
  gtk_widget_set_visible (chooser->travel.heading, !chooser->travel.empty);
  gtk_widget_set_visible (chooser->travel.box, !chooser->travel.empty);
  gtk_widget_set_visible (chooser->activities.heading, !chooser->activities.empty);
  gtk_widget_set_visible (chooser->activities.box, !chooser->activities.empty);
  gtk_widget_set_visible (chooser->objects.heading, !chooser->objects.empty);
  gtk_widget_set_visible (chooser->objects.box, !chooser->objects.empty);
  gtk_widget_set_visible (chooser->symbols.heading, !chooser->symbols.empty);
  gtk_widget_set_visible (chooser->symbols.box, !chooser->symbols.empty);
  gtk_widget_set_visible (chooser->flags.heading, !chooser->flags.empty);
  gtk_widget_set_visible (chooser->flags.box, !chooser->flags.empty);

  if (chooser->recent.empty && chooser->people.empty &&
      chooser->body.empty && chooser->nature.empty &&
      chooser->food.empty && chooser->travel.empty &&
      chooser->activities.empty && chooser->objects.empty &&
      chooser->symbols.empty && chooser->flags.empty)
    gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "empty");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "list");
}

static void
search_changed (GtkEntry *entry,
                gpointer  data)
{
  GtkEmojiChooser *chooser = data;

  invalidate_section (&chooser->recent);
  invalidate_section (&chooser->people);
  invalidate_section (&chooser->body);
  invalidate_section (&chooser->nature);
  invalidate_section (&chooser->food);
  invalidate_section (&chooser->travel);
  invalidate_section (&chooser->activities);
  invalidate_section (&chooser->objects);
  invalidate_section (&chooser->symbols);
  invalidate_section (&chooser->flags);

  update_headings (chooser);
}

static void
stop_search (GtkEntry *entry,
             gpointer  data)
{
  gtk_popover_popdown (GTK_POPOVER (data));
}

static void
setup_section (GtkEmojiChooser *chooser,
               EmojiSection    *section,
               int              group,
               const char      *icon)
{
  section->group = group;

  gtk_button_set_icon_name (GTK_BUTTON (section->button), icon);
  
  gtk_flow_box_disable_move_cursor (GTK_FLOW_BOX (section->box));
  gtk_flow_box_set_filter_func (GTK_FLOW_BOX (section->box), filter_func, section, NULL);
  g_signal_connect_swapped (section->button, "clicked", G_CALLBACK (scroll_to_section), section);
}

static void
gtk_emoji_chooser_init (GtkEmojiChooser *chooser)
{
  GtkAdjustment *adj;
  GtkText *text;

  chooser->settings = g_settings_new ("org.gtk.gtk4.Settings.EmojiChooser");

  gtk_widget_init_template (GTK_WIDGET (chooser));

  text = gtk_search_entry_get_text_widget (GTK_SEARCH_ENTRY (chooser->search_entry));
  gtk_text_set_input_hints (text, GTK_INPUT_HINT_NO_EMOJI);

  /* Get a reasonable maximum width for an emoji. We do this to
   * skip overly wide fallback rendering for certain emojis the
   * font does not contain and therefore end up being rendered
   * as multiply glyphs.
   */
  {
    PangoLayout *layout = gtk_widget_create_pango_layout (GTK_WIDGET (chooser), "🙂");
    PangoAttrList *attrs;
    PangoRectangle rect;

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_scale_new (PANGO_SCALE_X_LARGE));
    pango_layout_set_attributes (layout, attrs);
    pango_attr_list_unref (attrs);

    pango_layout_get_extents (layout, &rect, NULL);
    chooser->emoji_max_width = rect.width;

    g_object_unref (layout);
  }

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));
  g_signal_connect (adj, "value-changed", G_CALLBACK (adj_value_changed), chooser);

  setup_section (chooser, &chooser->recent, -1, "emoji-recent-symbolic");
  setup_section (chooser, &chooser->people, 0, "emoji-people-symbolic");
  setup_section (chooser, &chooser->body, 1, "emoji-body-symbolic");
  setup_section (chooser, &chooser->nature, 3, "emoji-nature-symbolic");
  setup_section (chooser, &chooser->food, 4, "emoji-food-symbolic");
  setup_section (chooser, &chooser->travel, 5, "emoji-travel-symbolic");
  setup_section (chooser, &chooser->activities, 6, "emoji-activities-symbolic");
  setup_section (chooser, &chooser->objects, 7, "emoji-objects-symbolic");
  setup_section (chooser, &chooser->symbols, 8, "emoji-symbols-symbolic");
  setup_section (chooser, &chooser->flags, 9, "emoji-flags-symbolic");

  populate_recent_section (chooser);

  chooser->populate_idle = g_idle_add (populate_emoji_chooser, chooser);
  gdk_source_set_static_name_by_id (chooser->populate_idle, "[gtk] populate_emoji_chooser");
}

static void
gtk_emoji_chooser_show (GtkWidget *widget)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);
  GtkAdjustment *adj;

  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->show (widget);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (chooser->scrolled_window));
  gtk_adjustment_set_value (adj, 0);
  adj_value_changed (adj, chooser);

  gtk_editable_set_text (GTK_EDITABLE (chooser->search_entry), "");
}

static EmojiSection *
find_section (GtkEmojiChooser *chooser,
              GtkWidget       *box)
{
  if (box == chooser->recent.box)
    return &chooser->recent;
  else if (box == chooser->people.box)
    return &chooser->people;
  else if (box == chooser->body.box)
    return &chooser->body;
  else if (box == chooser->nature.box)
    return &chooser->nature;
  else if (box == chooser->food.box)
    return &chooser->food;
  else if (box == chooser->travel.box)
    return &chooser->travel;
  else if (box == chooser->activities.box)
    return &chooser->activities;
  else if (box == chooser->objects.box)
    return &chooser->objects;
  else if (box == chooser->symbols.box)
    return &chooser->symbols;
  else if (box == chooser->flags.box)
    return &chooser->flags;
  else
    return NULL;
}

static EmojiSection *
find_next_section (GtkEmojiChooser *chooser,
                   GtkWidget       *box,
                   gboolean         down)
{
  EmojiSection *next;

  if (box == chooser->recent.box)
    next = down ? &chooser->people : NULL;
  else if (box == chooser->people.box)
    next = down ? &chooser->body : &chooser->recent;
  else if (box == chooser->body.box)
    next = down ? &chooser->nature : &chooser->people;
  else if (box == chooser->nature.box)
    next = down ? &chooser->food : &chooser->body;
  else if (box == chooser->food.box)
    next = down ? &chooser->travel : &chooser->nature;
  else if (box == chooser->travel.box)
    next = down ? &chooser->activities : &chooser->food;
  else if (box == chooser->activities.box)
    next = down ? &chooser->objects : &chooser->travel;
  else if (box == chooser->objects.box)
    next = down ? &chooser->symbols : &chooser->activities;
  else if (box == chooser->symbols.box)
    next = down ? &chooser->flags : &chooser->objects;
  else if (box == chooser->flags.box)
    next = down ? NULL : &chooser->symbols;
  else
    next = NULL;

  return next;
}

static void
gtk_emoji_chooser_scroll_section (GtkWidget  *widget,
                                  const char *action_name,
                                  GVariant   *parameter)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);
  int direction = g_variant_get_int32 (parameter);
  GtkWidget *focus;
  GtkWidget *box;
  EmojiSection *next;

  focus = gtk_root_get_focus (gtk_widget_get_root (widget));
  if (focus == NULL)
    return;

  if (gtk_widget_is_ancestor (focus, chooser->search_entry))
    box = chooser->recent.box;
  else
    box = gtk_widget_get_ancestor (focus, GTK_TYPE_FLOW_BOX);

  next = find_next_section (chooser, box, direction > 0);

  if (next)
    {
      gtk_widget_child_focus (next->box, GTK_DIR_TAB_FORWARD);
      scroll_to_section (next);
    }
}

static gboolean
keynav_failed (GtkWidget        *box,
               GtkDirectionType  direction,
               GtkEmojiChooser  *chooser)
{
  EmojiSection *next;
  GtkWidget *focus;
  GtkWidget *child;
  GtkWidget *sibling;
  int i;
  int column;
  int child_x;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);

  focus = gtk_root_get_focus (gtk_widget_get_root (box));
  if (focus == NULL)
    return FALSE;

  child = gtk_widget_get_ancestor (focus, GTK_TYPE_EMOJI_CHOOSER_CHILD);

  column = 0;
  child_x = G_MAXINT;
  for (sibling = gtk_widget_get_first_child (box);
       sibling;
       sibling = gtk_widget_get_next_sibling (sibling))
    {
      if (!gtk_widget_get_child_visible (sibling))
        continue;

      if (!gtk_widget_compute_bounds (sibling, box, &bounds))
        graphene_rect_init (&bounds, 0, 0, 0, 0);

      if (bounds.origin.x < child_x)
        column = 0;
      else
        column++;

      child_x = (int) bounds.origin.x;

      if (sibling == child)
        break;
    }

  if (direction == GTK_DIR_DOWN)
   {
      next = find_section (chooser, box);
      while (TRUE)
        {
          next = find_next_section (chooser, next->box, TRUE);
          if (next == NULL)
            return FALSE;

          i = 0;
          child_x = G_MAXINT;
          for (sibling = gtk_widget_get_first_child (next->box);
               sibling;
               sibling = gtk_widget_get_next_sibling (sibling))
            {
              if (!gtk_widget_get_child_visible (sibling))
                continue;

              if (!gtk_widget_compute_bounds (sibling, next->box, &bounds))
                graphene_rect_init (&bounds, 0, 0, 0, 0);

              if (bounds.origin.x < child_x)
                i = 0;
              else
                i++;

              child_x = (int) bounds.origin.x;

              if (i == column)
                {
                  gtk_widget_grab_focus (sibling);
                  return TRUE;
                }
            }
        }
    }
  else if (direction == GTK_DIR_UP)
    {
      next = find_section (chooser, box);
      while (TRUE)
        {
          next = find_next_section (chooser, next->box, FALSE);
          if (next == NULL)
            return FALSE;

          i = 0;
          child_x = G_MAXINT;
          child = NULL;
          for (sibling = gtk_widget_get_first_child (next->box);
               sibling;
               sibling = gtk_widget_get_next_sibling (sibling))
            {
              if (!gtk_widget_get_child_visible (sibling))
                continue;

              if (!gtk_widget_compute_bounds (sibling, next->box, &bounds))
                graphene_rect_init (&bounds, 0, 0, 0, 0);

              if (bounds.origin.x < child_x)
                i = 0;
              else
                i++;

              child_x = (int) bounds.origin.x;

              if (i == column)
                child = sibling;
            }

          if (child)
            {
              gtk_widget_grab_focus (child);
              return TRUE;
            }
        }
    }

  return FALSE;
}

static void
gtk_emoji_chooser_map (GtkWidget *widget)
{
  GtkEmojiChooser *chooser = GTK_EMOJI_CHOOSER (widget);

  GTK_WIDGET_CLASS (gtk_emoji_chooser_parent_class)->map (widget);

  gtk_widget_grab_focus (chooser->search_entry);
}

static void
gtk_emoji_chooser_class_init (GtkEmojiChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_emoji_chooser_finalize;
  object_class->dispose = gtk_emoji_chooser_dispose;
  widget_class->show = gtk_emoji_chooser_show;
  widget_class->map = gtk_emoji_chooser_map;

  /**
   * GtkEmojiChooser::emoji-picked:
   * @chooser: the `GtkEmojiChooser`
   * @text: the Unicode sequence for the picked Emoji, in UTF-8
   *
   * Emitted when the user selects an Emoji.
   */
  signals[EMOJI_PICKED] = g_signal_new ("emoji-picked",
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        NULL,
                                        G_TYPE_NONE, 1, G_TYPE_STRING|G_SIGNAL_TYPE_STATIC_SCOPE);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkemojichooser.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, stack);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, scrolled_window);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, recent.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, recent.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, people.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, people.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, people.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, body.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, body.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, body.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, nature.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, nature.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, nature.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, food.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, food.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, food.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, travel.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, travel.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, travel.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, activities.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, activities.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, activities.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, objects.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, objects.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, objects.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, symbols.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, symbols.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, symbols.button);

  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, flags.box);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, flags.heading);
  gtk_widget_class_bind_template_child (widget_class, GtkEmojiChooser, flags.button);

  gtk_widget_class_bind_template_callback (widget_class, emoji_activated);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, stop_search);
  gtk_widget_class_bind_template_callback (widget_class, pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, long_pressed_cb);
  gtk_widget_class_bind_template_callback (widget_class, keynav_failed);

  /**
   * GtkEmojiChooser|scroll.section:
   * @direction: 1 to scroll forward, -1 to scroll back
   *
   * Scrolls to the next or previous section.
   */
  gtk_widget_class_install_action (widget_class, "scroll.section", "i",
                                   gtk_emoji_chooser_scroll_section);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_n, GDK_CONTROL_MASK,
                                       "scroll.section", "i", 1);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_p, GDK_CONTROL_MASK,
                                       "scroll.section", "i", -1);
}

/**
 * gtk_emoji_chooser_new:
 *
 * Creates a new `GtkEmojiChooser`.
 *
 * Returns: a new `GtkEmojiChooser`
 */
GtkWidget *
gtk_emoji_chooser_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_EMOJI_CHOOSER, NULL));
}
