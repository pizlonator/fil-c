/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "guide-editor.h"

struct _GuideEditor
{
  GtkWidget parent_instance;

  GtkWidget *grid;
  GtkWidget *name;
  GtkWidget *min_width;
  GtkWidget *min_height;
  GtkWidget *nat_width;
  GtkWidget *nat_height;
  GtkWidget *max_width;
  GtkWidget *max_height;
  GtkWidget *strength;
  GtkWidget *button;

  GtkConstraintGuide *guide;

  gboolean constructed;
};

enum {
  PROP_GUIDE = 1,
  LAST_PROP
};

static GParamSpec *pspecs[LAST_PROP];

enum {
  DONE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(GuideEditor, guide_editor, GTK_TYPE_WIDGET);

static GtkConstraintStrength
get_strength (unsigned int id)
{
  switch (id)
    {
    case 0: return GTK_CONSTRAINT_STRENGTH_WEAK;
    case 1: return GTK_CONSTRAINT_STRENGTH_MEDIUM;
    case 2: return GTK_CONSTRAINT_STRENGTH_STRONG;
    case 3: return GTK_CONSTRAINT_STRENGTH_REQUIRED;
    default: g_assert_not_reached ();
    }
}

static unsigned int
get_strength_id (GtkConstraintStrength strength)
{
  switch (strength)
    {
    case GTK_CONSTRAINT_STRENGTH_WEAK: return 0;
    case GTK_CONSTRAINT_STRENGTH_MEDIUM: return 1;
    case GTK_CONSTRAINT_STRENGTH_STRONG: return 2;
    case GTK_CONSTRAINT_STRENGTH_REQUIRED: return 3;
    default: g_assert_not_reached ();
    }
}

static const char *
get_strength_nick (GtkConstraintStrength strength)
{
  GEnumClass *class = g_type_class_ref (GTK_TYPE_CONSTRAINT_STRENGTH);
  GEnumValue *value = g_enum_get_value (class, strength);
  const char *nick = value->value_nick;
  g_type_class_unref (class);

  return nick;
}

void
guide_editor_serialize_guide (GString *str,
                              int      indent,
                              GtkConstraintGuide *guide)
{
  int min_width, min_height;
  int nat_width, nat_height;
  int max_width, max_height;
  const char *name;
  const char *strength;

  gtk_constraint_guide_get_min_size (guide, &min_width, &min_height);
  gtk_constraint_guide_get_nat_size (guide, &nat_width, &nat_height);
  gtk_constraint_guide_get_max_size (guide, &max_width, &max_height);
  name = gtk_constraint_guide_get_name (guide);
  strength = get_strength_nick (gtk_constraint_guide_get_strength (guide));

  g_string_append_printf (str, "%*s<guide min-width=\"%d\" min-height=\"%d\"\n", indent, "", min_width, min_height);
  g_string_append_printf (str, "%*s       nat-width=\"%d\" nat-height=\"%d\"\n", indent, "", nat_width, nat_height);
  g_string_append_printf (str, "%*s       max-width=\"%d\" max-height=\"%d\"\n", indent, "", max_width, max_height);
  g_string_append_printf (str, "%*s       name=\"%s\" strength=\"%s\" />\n", indent, "", name, strength);
}

static void
create_guide (GtkButton   *button,
              GuideEditor *editor)
{
  int strength;
  const char *name;
  int w, h;
  GtkConstraintGuide *guide;
  unsigned int id;

  if (editor->guide)
    guide = g_object_ref (editor->guide);
  else
    guide = gtk_constraint_guide_new ();

  name = gtk_editable_get_text (GTK_EDITABLE (editor->name));
  gtk_constraint_guide_set_name (guide, name);

  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->min_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->min_height));
  gtk_constraint_guide_set_min_size (guide, w, h);

  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->nat_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->nat_height));
  gtk_constraint_guide_set_nat_size (guide, w, h);

  w = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->max_width));
  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (editor->max_height));
  gtk_constraint_guide_set_max_size (guide, w, h);

  id = gtk_drop_down_get_selected (GTK_DROP_DOWN (editor->strength));
  strength = get_strength (id);
  gtk_constraint_guide_set_strength (guide, strength);

  g_signal_emit (editor, signals[DONE], 0, guide);
  g_object_unref (guide);
}

static void
guide_editor_init (GuideEditor *editor)
{
  gtk_widget_init_template (GTK_WIDGET (editor));
}

static int guide_counter;

static int
min_input (GtkSpinButton *spin_button,
           double        *new_val)
{
  if (strcmp (gtk_editable_get_text (GTK_EDITABLE (spin_button)), "") == 0)
    {
      *new_val = 0.0;
      return TRUE;
    }

  return FALSE;
}

static int
max_input (GtkSpinButton *spin_button,
           double        *new_val)
{
  if (strcmp (gtk_editable_get_text (GTK_EDITABLE (spin_button)), "") == 0)
    {
      *new_val = G_MAXINT;
      return TRUE;
    }

  return FALSE;
}

static void
guide_editor_constructed (GObject *object)
{
  GuideEditor *editor = GUIDE_EDITOR (object);

  g_signal_connect (editor->min_width, "input", G_CALLBACK (min_input), NULL);
  g_signal_connect (editor->min_height, "input", G_CALLBACK (min_input), NULL);
  g_signal_connect (editor->max_width, "input", G_CALLBACK (max_input), NULL);
  g_signal_connect (editor->max_height, "input", G_CALLBACK (max_input), NULL);

  if (editor->guide)
    {
      GtkConstraintStrength strength;
      const char *nick;
      int w, h;

      nick = gtk_constraint_guide_get_name (editor->guide);
      if (nick)
        gtk_editable_set_text (GTK_EDITABLE (editor->name), nick);

      gtk_constraint_guide_get_min_size (editor->guide, &w, &h);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_width), w);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_height), h);

      gtk_constraint_guide_get_nat_size (editor->guide, &w, &h);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_width), w);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_height), h);

      gtk_constraint_guide_get_max_size (editor->guide, &w, &h);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_width), w);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_height), h);

      strength = gtk_constraint_guide_get_strength (editor->guide);
      gtk_drop_down_set_selected (GTK_DROP_DOWN (editor->strength), get_strength_id (strength));

      gtk_button_set_label (GTK_BUTTON (editor->button), "Apply");
    }
  else
    {
      char *name;

      guide_counter++;
      name = g_strdup_printf ("Guide %d", guide_counter);
      gtk_editable_set_text (GTK_EDITABLE (editor->name), name);
      g_free (name);

      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_width), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->min_height), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_width), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->nat_height), 0.0);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_width), G_MAXINT);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (editor->max_height), G_MAXINT);

      gtk_drop_down_set_selected (GTK_DROP_DOWN (editor->strength), get_strength_id (GTK_CONSTRAINT_STRENGTH_MEDIUM));

      gtk_button_set_label (GTK_BUTTON (editor->button), "Create");
    }

  editor->constructed = TRUE;
}

static void
guide_editor_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GuideEditor *self = GUIDE_EDITOR (object);

  switch (property_id)
    {
    case PROP_GUIDE:
      self->guide = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
guide_editor_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GuideEditor *self = GUIDE_EDITOR (object);

  switch (property_id)
    {
    case PROP_GUIDE:
      g_value_set_object (value, self->guide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
guide_editor_dispose (GObject *object)
{
  GuideEditor *self = (GuideEditor *)object;

  g_clear_object (&self->guide);

  gtk_widget_dispose_template (GTK_WIDGET (self), GUIDE_EDITOR_TYPE);

  G_OBJECT_CLASS (guide_editor_parent_class)->dispose (object);
}

static void
guide_editor_class_init (GuideEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructed = guide_editor_constructed;
  object_class->dispose = guide_editor_dispose;
  object_class->set_property = guide_editor_set_property;
  object_class->get_property = guide_editor_get_property;

  pspecs[PROP_GUIDE] =
    g_param_spec_object ("guide", "guide", "guide",
                         GTK_TYPE_CONSTRAINT_GUIDE,
                         G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, pspecs);

  signals[DONE] =
    g_signal_new ("done",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, GTK_TYPE_CONSTRAINT_GUIDE);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/constraint-editor/guide-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, GuideEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, name);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, min_width);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, min_height);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, nat_width);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, nat_height);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, max_width);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, max_height);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, strength);
  gtk_widget_class_bind_template_child (widget_class, GuideEditor, button);

  gtk_widget_class_bind_template_callback (widget_class, create_guide);
}

GuideEditor *
guide_editor_new (GtkConstraintGuide *guide)
{
  return g_object_new (GUIDE_EDITOR_TYPE,
                       "guide", guide,
                       NULL);
}
