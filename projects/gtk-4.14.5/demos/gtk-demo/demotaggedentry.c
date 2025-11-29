/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
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

#include "demotaggedentry.h"

#include <gtk/gtk.h>

struct _DemoTaggedEntry
{
  GtkWidget parent_instance;

  GtkWidget *text;
};

struct _DemoTaggedEntryClass
{
  GtkWidgetClass parent_class;
};

static void demo_tagged_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (DemoTaggedEntry, demo_tagged_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, demo_tagged_entry_editable_init))

static void
demo_tagged_entry_init (DemoTaggedEntry *entry)
{
  GtkCssProvider *provider;

  entry->text = gtk_text_new ();
  gtk_widget_set_hexpand (entry->text, TRUE);
  gtk_widget_set_vexpand (entry->text, TRUE);
  gtk_widget_set_parent (entry->text, GTK_WIDGET (entry));
  gtk_editable_init_delegate (GTK_EDITABLE (entry));
  gtk_editable_set_width_chars (GTK_EDITABLE (entry->text), 6);
  gtk_editable_set_max_width_chars (GTK_EDITABLE (entry->text), 6);
  gtk_widget_add_css_class (GTK_WIDGET (entry), "tagged");

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/tagged_entry/tagstyle.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);
  g_object_unref (provider);
}

static void
demo_tagged_entry_dispose (GObject *object)
{
  DemoTaggedEntry *entry = DEMO_TAGGED_ENTRY (object);
  GtkWidget *child;

  if (entry->text)
    gtk_editable_finish_delegate (GTK_EDITABLE (entry));

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (entry))))
    gtk_widget_unparent (child);

  entry->text = NULL;

  G_OBJECT_CLASS (demo_tagged_entry_parent_class)->dispose (object);
}

static void
demo_tagged_entry_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  if (gtk_editable_delegate_set_property (object, prop_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
demo_tagged_entry_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  if (gtk_editable_delegate_get_property (object, prop_id, value, pspec))
    return;

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static gboolean
demo_tagged_entry_grab_focus (GtkWidget *widget)
{
  DemoTaggedEntry *entry = DEMO_TAGGED_ENTRY (widget);

  return gtk_widget_grab_focus (entry->text);
}

static void
demo_tagged_entry_class_init (DemoTaggedEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = demo_tagged_entry_dispose;
  object_class->get_property = demo_tagged_entry_get_property;
  object_class->set_property = demo_tagged_entry_set_property;

  widget_class->grab_focus = demo_tagged_entry_grab_focus;

  gtk_editable_install_properties (object_class, 1);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "entry");
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);
}

static GtkEditable *
demo_tagged_entry_get_delegate (GtkEditable *editable)
{
  return GTK_EDITABLE (DEMO_TAGGED_ENTRY (editable)->text);
}

static void
demo_tagged_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = demo_tagged_entry_get_delegate;
}

GtkWidget *
demo_tagged_entry_new (void)
{
  return GTK_WIDGET (g_object_new (DEMO_TYPE_TAGGED_ENTRY, NULL));
}

void
demo_tagged_entry_add_tag (DemoTaggedEntry *entry,
                           GtkWidget       *tag)
{
  g_return_if_fail (DEMO_IS_TAGGED_ENTRY (entry));

  gtk_widget_set_parent (tag, GTK_WIDGET (entry));
}

void
demo_tagged_entry_insert_tag_after (DemoTaggedEntry *entry,
                                    GtkWidget       *tag,
                                    GtkWidget       *sibling)
{
  g_return_if_fail (DEMO_IS_TAGGED_ENTRY (entry));

  gtk_widget_insert_after (tag, GTK_WIDGET (entry), sibling);
}

void
demo_tagged_entry_remove_tag (DemoTaggedEntry *entry,
                              GtkWidget       *tag)
{
  g_return_if_fail (DEMO_IS_TAGGED_ENTRY (entry));

  gtk_widget_unparent (tag);
}

struct _DemoTaggedEntryTag
{
  GtkWidget parent;

  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;

  gboolean has_close_button;
  char *style;
};

struct _DemoTaggedEntryTagClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_HAS_CLOSE_BUTTON,
};

enum {
  SIGNAL_CLICKED,
  SIGNAL_BUTTON_CLICKED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (DemoTaggedEntryTag, demo_tagged_entry_tag, GTK_TYPE_WIDGET)

static void
on_released (GtkGestureClick    *gesture,
             int                 n_press,
             double              x,
             double              y,
             DemoTaggedEntryTag *tag)
{
  g_signal_emit (tag, signals[SIGNAL_CLICKED], 0);
}

static void
demo_tagged_entry_tag_init (DemoTaggedEntryTag *tag)
{
  GtkGesture *gesture;

  tag->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (tag->box, GTK_WIDGET (tag));
  tag->label = gtk_label_new ("");
  gtk_box_append (GTK_BOX (tag->box), tag->label);

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "released", G_CALLBACK (on_released), tag);
  gtk_widget_add_controller (GTK_WIDGET (tag), GTK_EVENT_CONTROLLER (gesture));
}

static void
demo_tagged_entry_tag_dispose (GObject *object)
{
  DemoTaggedEntryTag *tag = DEMO_TAGGED_ENTRY_TAG (object);

  g_clear_pointer (&tag->box, gtk_widget_unparent);

  G_OBJECT_CLASS (demo_tagged_entry_tag_parent_class)->dispose (object);
}

static void
demo_tagged_entry_tag_finalize (GObject *object)
{
  DemoTaggedEntryTag *tag = DEMO_TAGGED_ENTRY_TAG (object);

  g_clear_pointer (&tag->box, gtk_widget_unparent);
  g_clear_pointer (&tag->style, g_free);

  G_OBJECT_CLASS (demo_tagged_entry_tag_parent_class)->finalize (object);
}

static void
demo_tagged_entry_tag_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  DemoTaggedEntryTag *tag = DEMO_TAGGED_ENTRY_TAG (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      demo_tagged_entry_tag_set_label (tag, g_value_get_string (value));
      break;

    case PROP_HAS_CLOSE_BUTTON:
      demo_tagged_entry_tag_set_has_close_button (tag, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo_tagged_entry_tag_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  DemoTaggedEntryTag *tag = DEMO_TAGGED_ENTRY_TAG (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, demo_tagged_entry_tag_get_label (tag));
      break;

    case PROP_HAS_CLOSE_BUTTON:
      g_value_set_boolean (value, demo_tagged_entry_tag_get_has_close_button (tag));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo_tagged_entry_tag_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline)
{
  DemoTaggedEntryTag *tag = DEMO_TAGGED_ENTRY_TAG (widget);

  gtk_widget_measure (tag->box, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
demo_tagged_entry_tag_size_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  DemoTaggedEntryTag *tag = DEMO_TAGGED_ENTRY_TAG (widget);

  gtk_widget_size_allocate (tag->box,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}

static void
demo_tagged_entry_tag_class_init (DemoTaggedEntryTagClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_tagged_entry_tag_dispose;
  object_class->finalize = demo_tagged_entry_tag_finalize;
  object_class->set_property = demo_tagged_entry_tag_set_property;
  object_class->get_property = demo_tagged_entry_tag_get_property;

  widget_class->measure = demo_tagged_entry_tag_measure;
  widget_class->size_allocate = demo_tagged_entry_tag_size_allocate;

  signals[SIGNAL_CLICKED] =
      g_signal_new ("clicked",
                    DEMO_TYPE_TAGGED_ENTRY_TAG,
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL, NULL,
                    G_TYPE_NONE, 0);
  signals[SIGNAL_BUTTON_CLICKED] =
      g_signal_new ("button-clicked",
                    DEMO_TYPE_TAGGED_ENTRY_TAG,
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL, NULL,
                    G_TYPE_NONE, 0);

  g_object_class_install_property (object_class, PROP_LABEL,
      g_param_spec_string ("label", "Label", "Label",
                           NULL, G_PARAM_READWRITE));
  g_object_class_install_property (object_class, PROP_HAS_CLOSE_BUTTON,
      g_param_spec_boolean ("has-close-button", "Has close button", "Whether this tag has a close button",
                            FALSE, G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  gtk_widget_class_set_css_name (widget_class, "tag");
}

DemoTaggedEntryTag *
demo_tagged_entry_tag_new (const char *label)
{
  return DEMO_TAGGED_ENTRY_TAG (g_object_new (DEMO_TYPE_TAGGED_ENTRY_TAG,
                                              "label", label,
                                              NULL));
}

const char *
demo_tagged_entry_tag_get_label (DemoTaggedEntryTag *tag)
{
  g_return_val_if_fail (DEMO_IS_TAGGED_ENTRY_TAG (tag), NULL);

  return gtk_label_get_label (GTK_LABEL (tag->label));
}

void
demo_tagged_entry_tag_set_label (DemoTaggedEntryTag *tag,
                                 const char         *label)
{
  g_return_if_fail (DEMO_IS_TAGGED_ENTRY_TAG (tag));

  gtk_label_set_label (GTK_LABEL (tag->label), label);
}

static void
on_button_clicked (GtkButton          *button,
                   DemoTaggedEntryTag *tag)
{
  g_signal_emit (tag, signals[SIGNAL_BUTTON_CLICKED], 0);
}

void
demo_tagged_entry_tag_set_has_close_button (DemoTaggedEntryTag *tag,
                                            gboolean            has_close_button)
{
  g_return_if_fail (DEMO_IS_TAGGED_ENTRY_TAG (tag));

  if ((tag->button != NULL) == has_close_button)
    return;

  if (!has_close_button && tag->button)
    {
      gtk_box_remove (GTK_BOX (tag->box), tag->button);
      tag->button = NULL;
    }
  else if (has_close_button && tag->button == NULL)
    {
      GtkWidget *image;

      image = gtk_image_new_from_icon_name ("window-close-symbolic");
      tag->button = gtk_button_new ();
      gtk_button_set_child (GTK_BUTTON (tag->button), image);
      gtk_widget_set_halign (tag->button, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (tag->button, GTK_ALIGN_CENTER);
      gtk_button_set_has_frame (GTK_BUTTON (tag->button), FALSE);
      gtk_box_append (GTK_BOX (tag->box), tag->button);
      g_signal_connect (tag->button, "clicked", G_CALLBACK (on_button_clicked), tag);
    }

  g_object_notify (G_OBJECT (tag), "has-close-button");
}

gboolean
demo_tagged_entry_tag_get_has_close_button (DemoTaggedEntryTag *tag)
{
  g_return_val_if_fail (DEMO_IS_TAGGED_ENTRY_TAG (tag), FALSE);

  return tag->button != NULL;
}
