/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "prop-editor.h"

#include "strv-editor.h"
#include "prop-list.h"

#include "gtkactionable.h"
#include "gtkadjustment.h"
#include "gtkapplicationwindow.h"
#include "deprecated/gtkcelllayout.h"
#include "deprecated/gtkcombobox.h"
#include "deprecated/gtkiconview.h"
#include "deprecated/gtktreeview.h"
#include "gtkcolordialogbutton.h"
#include "gtkfontdialogbutton.h"
#include "gtklabel.h"
#include "gtkpopover.h"
#include "gtkscrolledwindow.h"
#include "gtkspinbutton.h"
#include "gtksettingsprivate.h"
#include "gtktogglebutton.h"
#include "gtkviewport.h"
#include "gtkwidgetprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtklistbox.h"
#include "gtkmenubutton.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

struct _GtkInspectorPropEditor
{
  GtkBox parent_instance;

  GObject *object;
  char *name;
  GtkWidget *self;
  GtkSizeGroup *size_group;
};

enum
{
  PROP_0,
  PROP_OBJECT,
  PROP_NAME,
  PROP_SIZE_GROUP
};

enum
{
  SHOW_OBJECT,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkInspectorPropEditor, gtk_inspector_prop_editor, GTK_TYPE_BOX);

static GParamSpec *
find_property (GtkInspectorPropEditor *self)
{
  return g_object_class_find_property (G_OBJECT_GET_CLASS (self->object), self->name);
}

typedef struct
{
  gpointer instance;
  GObject *alive_object;
  gulong id;
} DisconnectData;

static void
disconnect_func (gpointer data)
{
  DisconnectData *dd = data;

  g_signal_handler_disconnect (dd->instance, dd->id);
}

static void
signal_removed (gpointer  data,
                GClosure *closure)
{
  DisconnectData *dd = data;

  g_object_steal_data (dd->alive_object, "alive-object-data");
  g_free (dd);
}

static void
g_object_connect_property (GObject    *object,
                           GParamSpec *spec,
                           GCallback   func,
                           gpointer    data,
                           GObject    *alive_object)
{
  GClosure *closure;
  char *with_detail;
  DisconnectData *dd;

  with_detail = g_strconcat ("notify::", spec->name, NULL);

  dd = g_new (DisconnectData, 1);

  closure = g_cclosure_new (func, data, NULL);
  g_closure_add_invalidate_notifier (closure, dd, signal_removed);
  dd->id = g_signal_connect_closure (object, with_detail, closure, FALSE);
  dd->instance = object;
  dd->alive_object = alive_object;

  g_object_set_data_full (G_OBJECT (alive_object), "alive-object-data",
                          dd, disconnect_func);

  g_free (with_detail);
}

static void
block_notify (GObject *self)
{
  DisconnectData *dd = (DisconnectData *)g_object_get_data (self, "alive-object-data");

  if (dd)
    g_signal_handler_block (dd->instance, dd->id);
}

static void
unblock_notify (GObject *self)
{
  DisconnectData *dd = (DisconnectData *)g_object_get_data (self, "alive-object-data");

  if (dd)
    g_signal_handler_unblock (dd->instance, dd->id);
}

typedef struct
{
  GObject *obj;
  GParamSpec *spec;
  gulong modified_id;
} ObjectProperty;

static void
free_object_property (ObjectProperty *p)
{
  g_free (p);
}

static void
connect_controller (GObject     *controller,
                    const char *signal,
                    GObject     *model,
                    GParamSpec  *spec,
                    GCallback    func)
{
  ObjectProperty *p;

  p = g_new (ObjectProperty, 1);
  p->obj = model;
  p->spec = spec;

  p->modified_id = g_signal_connect_data (controller, signal, func, p,
                                          (GClosureNotify)free_object_property, 0);
  g_object_set_data (controller, "object-property", p);
}

static void
block_controller (GObject *controller)
{
  ObjectProperty *p = g_object_get_data (controller, "object-property");

  if (p)
    g_signal_handler_block (controller, p->modified_id);
}

static void
unblock_controller (GObject *controller)
{
  ObjectProperty *p = g_object_get_data (controller, "object-property");

  if (p)
    g_signal_handler_unblock (controller, p->modified_id);
}

static void
get_property_value (GObject *object, GParamSpec *pspec, GValue *value)
{
  g_object_get_property (object, pspec->name, value);
}

static void
set_property_value (GObject *object, GParamSpec *pspec, GValue *value)
{
  g_object_set_property (object, pspec->name, value);
}

static void
notify_property (GObject *object, GParamSpec *pspec)
{
  g_object_notify (object, pspec->name);
}

static void
int_modified (GtkAdjustment *adj, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, (int) gtk_adjustment_get_value (adj));
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
int_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_INT);
  get_property_value (object, pspec, &val);

  if (g_value_get_int (&val) != (int)gtk_adjustment_get_value (adj))
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_int (&val));
      unblock_controller (G_OBJECT (adj));
    }

  g_value_unset (&val);
}
static void
uint_modified (GtkAdjustment *adj, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_UINT);
  g_value_set_uint (&val, (guint) gtk_adjustment_get_value (adj));
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
uint_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_UINT);
  get_property_value (object, pspec, &val);

  if (g_value_get_uint (&val) != (guint)gtk_adjustment_get_value (adj))
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_uint (&val));
      unblock_controller (G_OBJECT (adj));
    }

  g_value_unset (&val);
}

static void
float_modified (GtkAdjustment *adj, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_FLOAT);
  g_value_set_float (&val, (float) gtk_adjustment_get_value (adj));
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
float_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_FLOAT);
  get_property_value (object, pspec, &val);

  if (g_value_get_float (&val) != (float) gtk_adjustment_get_value (adj))
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_float (&val));
      unblock_controller (G_OBJECT (adj));
    }

  g_value_unset (&val);
}

static void
double_modified (GtkAdjustment *adj, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_DOUBLE);
  g_value_set_double (&val, gtk_adjustment_get_value (adj));
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
double_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkAdjustment *adj = GTK_ADJUSTMENT (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_DOUBLE);
  get_property_value (object, pspec, &val);

  if (g_value_get_double (&val) != gtk_adjustment_get_value (adj))
    {
      block_controller (G_OBJECT (adj));
      gtk_adjustment_set_value (adj, g_value_get_double (&val));
      unblock_controller (G_OBJECT (adj));
    }

  g_value_unset (&val);
}

static void
string_modified (GtkEntry *entry, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_STRING);
  g_value_set_static_string (&val, gtk_editable_get_text (GTK_EDITABLE (entry)));
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
intern_string_modified (GtkEntry *entry, ObjectProperty *p)
{
  const char *s;

  s = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (g_str_equal (p->spec->name, "id"))
    gtk_css_node_set_id (GTK_CSS_NODE (p->obj), g_quark_from_string (s));
  else if (g_str_equal (p->spec->name, "name"))
    gtk_css_node_set_name (GTK_CSS_NODE (p->obj), g_quark_from_string (s));
}

static void
attr_list_modified (GtkEntry *entry, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;
  PangoAttrList *attrs;

  attrs = pango_attr_list_from_string (gtk_editable_get_text (GTK_EDITABLE (entry)));
  if (!attrs)
    return;

  g_value_init (&val, PANGO_TYPE_ATTR_LIST);
  g_value_take_boxed (&val, attrs);
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
string_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GValue val = G_VALUE_INIT;
  const char *str;
  const char *text;

  g_value_init (&val, G_TYPE_STRING);
  get_property_value (object, pspec, &val);

  str = g_value_get_string (&val);
  if (str == NULL)
    str = "";
  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (g_strcmp0 (str, text) != 0)
    {
      block_controller (G_OBJECT (entry));
      gtk_editable_set_text (GTK_EDITABLE (entry), str);
      unblock_controller (G_OBJECT (entry));
    }

  g_value_unset (&val);
}

static void
attr_list_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  GValue val = G_VALUE_INIT;
  char *str = NULL;
  const char *text;
  PangoAttrList *attrs;

  g_value_init (&val, PANGO_TYPE_ATTR_LIST);
  get_property_value (object, pspec, &val);

  attrs = g_value_get_boxed (&val);
  if (attrs)
    str = pango_attr_list_to_string (attrs);
  if (str == NULL)
    str = g_strdup ("");
  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (g_strcmp0 (str, text) != 0)
    {
      block_controller (G_OBJECT (entry));
      gtk_editable_set_text (GTK_EDITABLE (entry), str);
      unblock_controller (G_OBJECT (entry));
    }

  g_free (str);

  g_value_unset (&val);
}

static void
strv_modified (GtkInspectorStrvEditor *self, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;
  char **strv;

  g_value_init (&val, G_TYPE_STRV);
  strv = gtk_inspector_strv_editor_get_strv (self);
  g_value_take_boxed (&val, strv);
  block_notify (G_OBJECT (self));
  set_property_value (p->obj, p->spec, &val);
  unblock_notify (G_OBJECT (self));
  g_value_unset (&val);
}

static void
strv_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkInspectorStrvEditor *self = data;
  GValue val = G_VALUE_INIT;
  char **strv;

  g_value_init (&val, G_TYPE_STRV);
  get_property_value (object, pspec, &val);

  strv = g_value_get_boxed (&val);
  block_controller (G_OBJECT (self));
  gtk_inspector_strv_editor_set_strv (self, strv);
  unblock_controller (G_OBJECT (self));

  g_value_unset (&val);
}

static void
bool_modified (GtkCheckButton *cb,
               ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&val, gtk_check_button_get_active (cb));
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
bool_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkCheckButton *cb = GTK_CHECK_BUTTON (data);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, G_TYPE_BOOLEAN);
  get_property_value (object, pspec, &val);

  if (g_value_get_boolean (&val) != gtk_check_button_get_active (cb))
    {
      block_controller (G_OBJECT (cb));
      gtk_check_button_set_active (cb, g_value_get_boolean (&val));
      unblock_controller (G_OBJECT (cb));
    }

  g_value_unset (&val);
}

static void
enum_modified (GtkDropDown *dropdown, GParamSpec *pspec, ObjectProperty *p)
{
  int i = gtk_drop_down_get_selected (dropdown);
  GEnumClass *eclass;
  GValue val = G_VALUE_INIT;

  eclass = G_ENUM_CLASS (g_type_class_peek (p->spec->value_type));

  g_value_init (&val, p->spec->value_type);
  g_value_set_enum (&val, eclass->values[i].value);
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
enum_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GValue val = G_VALUE_INIT;
  GEnumClass *eclass;
  int i;

  eclass = G_ENUM_CLASS (g_type_class_peek (pspec->value_type));

  g_value_init (&val, pspec->value_type);
  get_property_value (object, pspec, &val);

  i = 0;
  while (i < eclass->n_values)
    {
      if (eclass->values[i].value == g_value_get_enum (&val))
        break;
      ++i;
    }
  g_value_unset (&val);

  block_controller (G_OBJECT (data));
  gtk_drop_down_set_selected (GTK_DROP_DOWN (data), i);
  unblock_controller (G_OBJECT (data));
}

static void
flags_modified (GtkCheckButton *button, ObjectProperty *p)
{
  gboolean active;
  GFlagsClass *fclass;
  guint flags;
  int i;
  GValue val = G_VALUE_INIT;

  active = gtk_check_button_get_active (button);
  i = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "index"));
  fclass = G_FLAGS_CLASS (g_type_class_peek (p->spec->value_type));

  g_value_init (&val, p->spec->value_type);
  get_property_value (p->obj, p->spec, &val);
  flags = g_value_get_flags (&val);
  if (active)
    flags |= fclass->values[i].value;
  else
    flags &= ~fclass->values[i].value;
  g_value_set_flags (&val, flags);
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static char *
flags_to_string (GFlagsClass *flags_class,
                 guint        value)
{
  GString *str;
  GFlagsValue *flags_value;

  str = g_string_new (NULL);

  while ((str->len == 0 || value != 0) &&
         (flags_value = g_flags_get_first_value (flags_class, value)) != NULL)
    {
      if (str->len > 0)
        g_string_append (str, " | ");

      g_string_append (str, flags_value->value_nick);

      value &= ~flags_value->value;
    }

  /* Show the extra bits */
  if (value != 0 || str->len == 0)
    {
      if (str->len > 0)
        g_string_append (str, " | ");

      g_string_append_printf (str, "0x%x", value);
    }

  return g_string_free (str, FALSE);
}

static void
flags_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GValue val = G_VALUE_INIT;
  GFlagsClass *fclass;
  guint flags;
  int i;
  GtkPopover *popover;
  GtkWidget *sw;
  GtkWidget *viewport;
  GtkWidget *box;
  char *str;
  GtkWidget *child;

  fclass = G_FLAGS_CLASS (g_type_class_peek (pspec->value_type));

  g_value_init (&val, pspec->value_type);
  get_property_value (object, pspec, &val);
  flags = g_value_get_flags (&val);
  g_value_unset (&val);

  str = flags_to_string (fclass, flags);
  gtk_menu_button_set_label (GTK_MENU_BUTTON (data), str);
  g_free (str);

  popover = gtk_menu_button_get_popover (GTK_MENU_BUTTON (data));
  sw =  gtk_popover_get_child (GTK_POPOVER (popover));
  viewport = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (sw));
  box = gtk_viewport_get_child (GTK_VIEWPORT (viewport));
  for (child = gtk_widget_get_first_child (box);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    block_controller (G_OBJECT (child));

  for (child = gtk_widget_get_first_child (box), i = 0;
       child != NULL;
       child = gtk_widget_get_next_sibling (child), i++)
    gtk_check_button_set_active (GTK_CHECK_BUTTON (child),
                                 (fclass->values[i].value & flags) != 0);

  for (child = gtk_widget_get_first_child (box);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    unblock_controller (G_OBJECT (child));
}

static gunichar
unichar_get_value (GtkEntry *entry)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (text[0])
    return g_utf8_get_char (text);
  else
    return 0;
}

static void
unichar_modified (GtkEntry *entry, ObjectProperty *p)
{
  gunichar u = unichar_get_value (entry);
  GValue val = G_VALUE_INIT;

  g_value_init (&val, p->spec->value_type);
  g_value_set_uint (&val, u);
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}
static void
unichar_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (data);
  gunichar new_val;
  gunichar old_val = unichar_get_value (entry);
  GValue val = G_VALUE_INIT;
  char buf[7];
  int len;

  g_value_init (&val, pspec->value_type);
  get_property_value (object, pspec, &val);
  new_val = (gunichar)g_value_get_uint (&val);
  g_value_unset (&val);

  if (new_val != old_val)
    {
      if (!new_val)
        len = 0;
      else
        len = g_unichar_to_utf8 (new_val, buf);

      buf[len] = '\0';

      block_controller (G_OBJECT (entry));
      gtk_editable_set_text (GTK_EDITABLE (entry), buf);
      unblock_controller (G_OBJECT (entry));
    }
}

static void
pointer_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkLabel *label = GTK_LABEL (data);
  char *str;
  gpointer ptr;

  g_object_get (object, pspec->name, &ptr, NULL);

  str = g_strdup_printf (_("Pointer: %p"), ptr);
  gtk_label_set_text (label, str);
  g_free (str);
}

static char *
object_label (GObject *obj, GParamSpec *pspec)
{
  return g_strdup_printf ("%p", obj);
}

static void
object_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkWidget *label, *button;
  char *str;
  GObject *obj;

  label = gtk_widget_get_first_child (GTK_WIDGET (data));
  button = gtk_widget_get_next_sibling (label);
  g_object_get (object, pspec->name, &obj, NULL);

  str = object_label (obj, pspec);

  gtk_label_set_text (GTK_LABEL (label), str);
  gtk_widget_set_sensitive (button, G_IS_OBJECT (obj));

  if (obj)
    g_object_unref (obj);

  g_free (str);
}

static void
object_properties (GtkInspectorPropEditor *self)
{
  GObject *obj;

  g_object_get (self->object, self->name, &obj, NULL);
  if (G_IS_OBJECT (obj))
    g_signal_emit (self, signals[SHOW_OBJECT], 0, obj, self->name, "properties");
}

static void
rgba_modified (GtkColorDialogButton *cb, GParamSpec *ignored, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, p->spec->value_type);
  g_object_get_property (G_OBJECT (cb), "rgba", &val);
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
rgba_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkColorChooser *cb = GTK_COLOR_CHOOSER (data);
  GValue val = G_VALUE_INIT;
  GdkRGBA *color;

  g_value_init (&val, GDK_TYPE_RGBA);
  get_property_value (object, pspec, &val);

  color = g_value_get_boxed (&val);

  if (color != NULL)
    {
      block_controller (G_OBJECT (cb));
      gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (cb), color);
      unblock_controller (G_OBJECT (cb));
    }
 g_value_unset (&val);
}

static void
font_modified (GtkFontDialogButton *fb, GParamSpec *pspec, ObjectProperty *p)
{
  GValue val = G_VALUE_INIT;

  g_value_init (&val, PANGO_TYPE_FONT_DESCRIPTION);
  g_object_get_property (G_OBJECT (fb), "font-desc", &val);
  set_property_value (p->obj, p->spec, &val);
  g_value_unset (&val);
}

static void
font_changed (GObject *object, GParamSpec *pspec, gpointer data)
{
  GtkFontDialogButton *fb = GTK_FONT_DIALOG_BUTTON (data);
  GValue val = G_VALUE_INIT;
  const PangoFontDescription *font_desc;

  g_value_init (&val, PANGO_TYPE_FONT_DESCRIPTION);
  get_property_value (object, pspec, &val);

  font_desc = g_value_get_boxed (&val);

  if (font_desc != NULL)
    {
      block_controller (G_OBJECT (fb));
      gtk_font_dialog_button_set_font_desc (fb, font_desc);
      unblock_controller (G_OBJECT (fb));
    }

  g_value_unset (&val);
}

static char *
describe_expression (GtkExpression *expression)
{
  if (expression == NULL)
    return NULL;

  if (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_CONSTANT_EXPRESSION))
    {
      const GValue *value = gtk_constant_expression_get_value (expression);
      GValue dest = G_VALUE_INIT;

      g_value_init (&dest, G_TYPE_STRING);
      if (g_value_transform (value, &dest))
        {
          /* Translators: %s is a type name, for example
           * GtkPropertyExpression with value \"2.5\"
           */
          char *res = g_strdup_printf (_("%s with value \"%s\""),
                                       g_type_name (G_TYPE_FROM_INSTANCE (expression)),
                                       g_value_get_string (&dest));
          g_value_unset (&dest);
          return res;
        }
      else
        {
          /* Translators: Both %s are type names, for example
           * GtkPropertyExpression with type GObject
           */
          return g_strdup_printf (_("%s with type %s"),
                                  g_type_name (G_TYPE_FROM_INSTANCE (expression)),
                                  g_type_name (G_VALUE_TYPE (value)));
        }
    }
  else if (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_OBJECT_EXPRESSION))
    {
      gpointer obj = gtk_object_expression_get_object (expression);

      if (obj)
        /* Translators: Both %s are type names, for example
         * GtkObjectExpression for GtkStringObject 0x23456789
         */
        return g_strdup_printf (_("%s for %s %p"),
                                g_type_name (G_TYPE_FROM_INSTANCE (expression)),
                                G_OBJECT_TYPE_NAME (obj), obj);
      else
        return g_strdup (g_type_name (G_TYPE_FROM_INSTANCE (expression)));
    }
  else if (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_PROPERTY_EXPRESSION))
    {
      GParamSpec *pspec = gtk_property_expression_get_pspec (expression);
      GtkExpression *expr = gtk_property_expression_get_expression (expression);
      char *str;
      char *res;

      str = describe_expression (expr);
      /* Translators: The first %s is a type name, %s:%s is a qualified
       * property name, and is a value, for example
       * GtkPropertyExpression for property GtkLabellabel on: GObjectExpression ...
       */
      res = g_strdup_printf ("%s for property %s:%s on: %s",
                             g_type_name (G_TYPE_FROM_INSTANCE (expression)),
                             g_type_name (pspec->owner_type),
                             pspec->name,
                             str);
      g_free (str);
      return res;
    }
  else
    /* Translators: Both %s are type names, for example
     * GtkPropertyExpression with value type: gchararray
     */
    return g_strdup_printf (_("%s with value type %s"),
                            g_type_name (G_TYPE_FROM_INSTANCE (expression)),
                            g_type_name (gtk_expression_get_value_type (expression)));
}

static void
toggle_unicode (GtkToggleButton *button,
                GParamSpec      *pspec,
                GtkWidget       *stack)
{
  GtkWidget *entry;
  GtkWidget *unicode;

  entry = gtk_stack_get_child_by_name (GTK_STACK (stack), "entry");
  unicode = gtk_stack_get_child_by_name (GTK_STACK (stack), "unicode");
  if (gtk_toggle_button_get_active (button))
    {
      const char *text;
      const char *p;
      GString *s;

      text = gtk_editable_get_text (GTK_EDITABLE (entry));
      s = g_string_sized_new (6 * strlen (text));
      for (p = text; *p; p = g_utf8_next_char (p))
        {
          gunichar ch = g_utf8_get_char (p);
          if (s->len > 0)
            g_string_append_c (s, ' ');
          g_string_append_printf (s, "U+%04X", ch);
        }
      gtk_editable_set_text (GTK_EDITABLE (unicode), s->str);
      g_string_free (s, TRUE);

      gtk_stack_set_visible_child_name (GTK_STACK (stack), "unicode");
    }
  else
    {
      gtk_editable_set_text (GTK_EDITABLE (unicode), "");
      gtk_stack_set_visible_child_name (GTK_STACK (stack), "entry");
    }
}

static GtkWidget *
property_editor (GObject                *object,
                 GParamSpec             *spec,
                 GtkInspectorPropEditor *self)
{
  GtkWidget *prop_edit;
  GtkAdjustment *adj;
  char *msg;
  GType type = G_PARAM_SPEC_TYPE (spec);

  if (type == G_TYPE_PARAM_INT)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_INT (spec)->default_value,
                                G_PARAM_SPEC_INT (spec)->minimum,
                                G_PARAM_SPEC_INT (spec)->maximum,
                                1,
                                MAX ((((double) G_PARAM_SPEC_INT (spec)->maximum - (double)G_PARAM_SPEC_INT (spec)->minimum)) / 10, 1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 1.0, 0);

      g_object_connect_property (object, spec, G_CALLBACK (int_changed), adj, G_OBJECT (adj));

      connect_controller (G_OBJECT (adj), "value_changed",
                          object, spec, G_CALLBACK (int_modified));
    }
  else if (type == G_TYPE_PARAM_UINT)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_UINT (spec)->default_value,
                                G_PARAM_SPEC_UINT (spec)->minimum,
                                G_PARAM_SPEC_UINT (spec)->maximum,
                                1,
                                MAX ((G_PARAM_SPEC_UINT (spec)->maximum - G_PARAM_SPEC_UINT (spec)->minimum) / 10, 1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 1.0, 0);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (uint_changed),
                                 adj, G_OBJECT (adj));

      connect_controller (G_OBJECT (adj), "value_changed",
                          object, spec, G_CALLBACK (uint_modified));
    }
  else if (type == G_TYPE_PARAM_FLOAT)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_FLOAT (spec)->default_value,
                                G_PARAM_SPEC_FLOAT (spec)->minimum,
                                G_PARAM_SPEC_FLOAT (spec)->maximum,
                                0.1,
                                MAX ((G_PARAM_SPEC_FLOAT (spec)->maximum - G_PARAM_SPEC_FLOAT (spec)->minimum) / 10, 0.1),
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 0.1, 2);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (float_changed),
                                 adj, G_OBJECT (adj));

      connect_controller (G_OBJECT (adj), "value_changed",
                          object, spec, G_CALLBACK (float_modified));
    }
  else if (type == G_TYPE_PARAM_DOUBLE)
    {
      adj = gtk_adjustment_new (G_PARAM_SPEC_DOUBLE (spec)->default_value,
                                G_PARAM_SPEC_DOUBLE (spec)->minimum,
                                G_PARAM_SPEC_DOUBLE (spec)->maximum,
                                0.1,
                                1.0,
                                0.0);

      prop_edit = gtk_spin_button_new (adj, 0.1, 2);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (double_changed),
                                 adj, G_OBJECT (adj));

      connect_controller (G_OBJECT (adj), "value_changed",
                          object, spec, G_CALLBACK (double_modified));
    }
  else if (type == G_TYPE_PARAM_STRING)
    {
      GtkWidget *entry;
      GtkWidget *button;
      GtkWidget *stack;
      GtkWidget *unicode;

      entry = gtk_entry_new ();

      g_object_connect_property (object, spec,
                                 G_CALLBACK (string_changed),
                                 entry, G_OBJECT (entry));

      if (GTK_IS_CSS_NODE (object))
        connect_controller (G_OBJECT (entry), "changed",
                            object, spec, G_CALLBACK (intern_string_modified));
      else
        connect_controller (G_OBJECT (entry), "changed",
                            object, spec, G_CALLBACK (string_modified));

      unicode = gtk_entry_new ();
      gtk_editable_set_editable (GTK_EDITABLE (unicode), FALSE);

      stack = gtk_stack_new ();
      gtk_stack_add_named (GTK_STACK (stack), entry, "entry");
      gtk_stack_add_named (GTK_STACK (stack), unicode, "unicode");

      prop_edit = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_append (GTK_BOX (prop_edit), stack);

      button = gtk_toggle_button_new_with_label ("Unicode");
      gtk_box_append (GTK_BOX (prop_edit), button);

      g_signal_connect (button, "notify::active", G_CALLBACK (toggle_unicode), stack);
    }
  else if (type == G_TYPE_PARAM_BOOLEAN)
    {
      prop_edit = gtk_check_button_new_with_label ("");

      g_object_connect_property (object, spec,
                                 G_CALLBACK (bool_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      connect_controller (G_OBJECT (prop_edit), "toggled",
                          object, spec, G_CALLBACK (bool_modified));
    }
  else if (type == G_TYPE_PARAM_ENUM)
    {
      {
        GEnumClass *eclass;
        GtkStringList *names;
        int j;

        eclass = G_ENUM_CLASS (g_type_class_ref (spec->value_type));

        names = gtk_string_list_new (NULL);
        for (j = 0; j < eclass->n_values; j++)
          gtk_string_list_append (names, eclass->values[j].value_name);

        prop_edit = gtk_drop_down_new (G_LIST_MODEL (names), NULL);

        connect_controller (G_OBJECT (prop_edit), "notify::selected",
                            object, spec, G_CALLBACK (enum_modified));

        g_type_class_unref (eclass);

        g_object_connect_property (object, spec,
                                   G_CALLBACK (enum_changed),
                                   prop_edit, G_OBJECT (prop_edit));
      }
    }
  else if (type == G_TYPE_PARAM_FLAGS)
    {
      {
        GtkWidget *box;
        GtkWidget *sw;
        GtkWidget *popover;
        GFlagsClass *fclass;
        int j;

        popover = gtk_popover_new ();
        prop_edit = gtk_menu_button_new ();
        gtk_menu_button_set_popover (GTK_MENU_BUTTON (prop_edit), popover);

        sw = gtk_scrolled_window_new ();
        gtk_popover_set_child (GTK_POPOVER (popover), sw);
        g_object_set (sw,
                      "hexpand", TRUE,
                      "vexpand", TRUE,
                      "hscrollbar-policy", GTK_POLICY_NEVER,
                      "vscrollbar-policy", GTK_POLICY_NEVER,
                      NULL);
        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);

        fclass = G_FLAGS_CLASS (g_type_class_ref (spec->value_type));

        for (j = 0; j < fclass->n_values; j++)
          {
            GtkWidget *b;

            b = gtk_check_button_new_with_label (fclass->values[j].value_nick);
            g_object_set_data (G_OBJECT (b), "index", GINT_TO_POINTER (j));
            gtk_box_append (GTK_BOX (box), b);
            connect_controller (G_OBJECT (b), "toggled",
                                object, spec, G_CALLBACK (flags_modified));
          }

        if (j >= 10)
          {
            g_object_set (sw,
                          "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
                          "min-content-height", 250,
                          NULL);
          }

        g_type_class_unref (fclass);

        g_object_connect_property (object, spec,
                                   G_CALLBACK (flags_changed),
                                   prop_edit, G_OBJECT (prop_edit));
      }
    }
  else if (type == G_TYPE_PARAM_UNICHAR)
    {
      prop_edit = gtk_entry_new ();
      gtk_entry_set_max_length (GTK_ENTRY (prop_edit), 1);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (unichar_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      connect_controller (G_OBJECT (prop_edit), "changed",
                          object, spec, G_CALLBACK (unichar_modified));
    }
  else if (type == G_TYPE_PARAM_POINTER)
    {
      prop_edit = gtk_label_new ("");

      g_object_connect_property (object, spec,
                                 G_CALLBACK (pointer_changed),
                                 prop_edit, G_OBJECT (prop_edit));
    }
  else if (type == G_TYPE_PARAM_OBJECT)
    {
      GtkWidget *label, *button;

      prop_edit = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

      label = gtk_label_new ("");
      button = gtk_button_new_with_label (_("Properties"));
      g_signal_connect_swapped (button, "clicked",
                                G_CALLBACK (object_properties),
                                self);
      gtk_box_append (GTK_BOX (prop_edit), label);
      gtk_box_append (GTK_BOX (prop_edit), button);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (object_changed),
                                 prop_edit, G_OBJECT (label));
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == GDK_TYPE_RGBA)
    {
      prop_edit = gtk_color_dialog_button_new (gtk_color_dialog_new ());

      g_object_connect_property (object, spec,
                                 G_CALLBACK (rgba_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      connect_controller (G_OBJECT (prop_edit), "notify::rgba",
                          object, spec, G_CALLBACK (rgba_modified));
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == PANGO_TYPE_FONT_DESCRIPTION)
    {
      prop_edit = gtk_font_dialog_button_new (gtk_font_dialog_new ());

      g_object_connect_property (object, spec,
                                 G_CALLBACK (font_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      connect_controller (G_OBJECT (prop_edit), "notify::font-desc",
                          object, spec, G_CALLBACK (font_modified));
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == G_TYPE_STRV)
    {
      prop_edit = g_object_new (gtk_inspector_strv_editor_get_type (),
                                "visible", TRUE,
                                NULL);

      g_object_connect_property (object, spec,
                                 G_CALLBACK (strv_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      connect_controller (G_OBJECT (prop_edit), "changed",
                          object, spec, G_CALLBACK (strv_modified));

      gtk_widget_set_halign (prop_edit, GTK_ALIGN_START);
      gtk_widget_set_valign (prop_edit, GTK_ALIGN_CENTER);
    }
  else if (type == G_TYPE_PARAM_BOXED &&
           G_PARAM_SPEC_VALUE_TYPE (spec) == PANGO_TYPE_ATTR_LIST)
    {
      prop_edit = gtk_entry_new ();

      g_object_connect_property (object, spec,
                                 G_CALLBACK (attr_list_changed),
                                 prop_edit, G_OBJECT (prop_edit));

      connect_controller (G_OBJECT (prop_edit), "changed",
                          object, spec, G_CALLBACK (attr_list_modified));
    }
  else if (type == GTK_TYPE_PARAM_SPEC_EXPRESSION)
    {
      GtkExpression *expression;
      g_object_get (object, spec->name, &expression, NULL);
      msg = describe_expression (expression);
      prop_edit = gtk_label_new (msg);
      g_free (msg);
      g_clear_pointer (&expression, gtk_expression_unref);
      gtk_widget_set_halign (prop_edit, GTK_ALIGN_START);
      gtk_widget_set_valign (prop_edit, GTK_ALIGN_CENTER);
    }
  else
    {
      msg = g_strdup_printf (_("Uneditable property type: %s"),
                             g_type_name (G_PARAM_SPEC_TYPE (spec)));
      prop_edit = gtk_label_new (msg);
      g_free (msg);
      gtk_widget_set_halign (prop_edit, GTK_ALIGN_START);
      gtk_widget_set_valign (prop_edit, GTK_ALIGN_CENTER);
    }

  notify_property (object, spec);

  if (GTK_IS_LABEL (prop_edit))
    {
      gtk_widget_set_can_focus (prop_edit, TRUE);
      gtk_accessible_update_property (GTK_ACCESSIBLE (prop_edit),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL,
                                      g_strdup_printf ("%s: %s",
                                                       self->name,
                                                       gtk_label_get_text (GTK_LABEL (prop_edit))),
                                      -1);
    }
  else
    {
      gtk_accessible_update_property (GTK_ACCESSIBLE (prop_edit),
                                      GTK_ACCESSIBLE_PROPERTY_LABEL, self->name,
                                      -1);
    }

  return prop_edit;
}

static void
gtk_inspector_prop_editor_init (GtkInspectorPropEditor *self)
{
  g_object_set (self,
                "orientation", GTK_ORIENTATION_HORIZONTAL,
                "spacing", 10,
                NULL);
}

static GtkTreeModel *
gtk_cell_layout_get_model (GtkCellLayout *layout)
{
  if (GTK_IS_TREE_VIEW_COLUMN (layout))
    return gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_tree_view_column_get_tree_view (GTK_TREE_VIEW_COLUMN (layout))));
  else if (GTK_IS_ICON_VIEW (layout))
    return gtk_icon_view_get_model (GTK_ICON_VIEW (layout));
  else if (GTK_IS_COMBO_BOX (layout))
    return gtk_combo_box_get_model (GTK_COMBO_BOX (layout));
  else
    return NULL;
}

static GtkWidget *
gtk_cell_layout_get_widget (GtkCellLayout *layout)
{
  if (GTK_IS_TREE_VIEW_COLUMN (layout))
    return gtk_tree_view_column_get_tree_view (GTK_TREE_VIEW_COLUMN (layout));
  else if (GTK_IS_WIDGET (layout))
    return GTK_WIDGET (layout);
  else
    return NULL;
}

static void
model_properties (GtkButton              *button,
                  GtkInspectorPropEditor *self)
{
  GObject *model;

  model = g_object_get_data (G_OBJECT (button), "model");
  g_signal_emit (self, signals[SHOW_OBJECT], 0, model, "model", "data");
}

static void
attribute_mapping_changed (GtkDropDown            *dropdown,
                           GParamSpec             *pspec,
                           GtkInspectorPropEditor *self)
{
  int col;
  gpointer layout;
  GtkCellRenderer *cell;
  GtkCellArea *area;

  col = gtk_drop_down_get_selected (dropdown) - 1;
  layout = g_object_get_data (self->object, "gtk-inspector-cell-layout");
  if (GTK_IS_CELL_LAYOUT (layout))
    {
      cell = GTK_CELL_RENDERER (self->object);
      area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (layout));
      gtk_cell_area_attribute_disconnect (area, cell, self->name);
      if (col != -1)
        gtk_cell_area_attribute_connect (area, cell, self->name, col);
      gtk_widget_set_sensitive (self->self, col == -1);
      notify_property (self->object, find_property (self));
      gtk_widget_queue_draw (gtk_cell_layout_get_widget (GTK_CELL_LAYOUT (layout)));
    }
}

#define ATTRIBUTE_TYPE_HOLDER (attribute_holder_get_type ())
G_DECLARE_FINAL_TYPE (AttributeHolder, attribute_holder, ATTRIBUTE, HOLDER, GObject)

struct _AttributeHolder {
  GObject parent_instance;
  int column;
  gboolean sensitive;
};

G_DEFINE_TYPE (AttributeHolder, attribute_holder, G_TYPE_OBJECT);

static void
attribute_holder_init (AttributeHolder *holder)
{
}

static void
attribute_holder_class_init (AttributeHolderClass *class)
{
}

static AttributeHolder *
attribute_holder_new (int      column,
                      gboolean sensitive)
{
  AttributeHolder *holder = g_object_new (ATTRIBUTE_TYPE_HOLDER, NULL);
  holder->column = column;
  holder->sensitive = sensitive;
  return holder;
}

static void
attribute_setup_item (GtkSignalListItemFactory *factory,
                      GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  gtk_list_item_set_child (item, label);
}

static void
attribute_bind_item (GtkSignalListItemFactory *factory,
                     GtkListItem              *item)
{
  GtkWidget *label;
  AttributeHolder *holder;

  holder = gtk_list_item_get_item (item);
  label = gtk_list_item_get_child (item);

  if (holder->column >= 0)
    {
      char *text = g_strdup_printf ("%d", holder->column);
      gtk_label_set_label (GTK_LABEL (label), text);
      g_free (text);
    }
  else
    gtk_label_set_label (GTK_LABEL (label), C_("column number", "None"));

  gtk_list_item_set_selectable (item, holder->sensitive);
  gtk_widget_set_sensitive (label, holder->sensitive);
}

static GtkWidget *
attribute_editor (GObject                *object,
                  GParamSpec             *spec,
                  GtkInspectorPropEditor *self)
{
  gpointer layout;
  GtkCellArea *area;
  GtkTreeModel *model = NULL;
  int col = -1;
  GtkWidget *label;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *dropdown;
  GListStore *store;
  GtkListItemFactory *factory;
  int i;
  AttributeHolder *holder;
  gboolean sensitive;

  layout = g_object_get_data (object, "gtk-inspector-cell-layout");
  if (GTK_IS_CELL_LAYOUT (layout))
    {
      area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (layout));
      col = gtk_cell_area_attribute_get_column (area,
                                                GTK_CELL_RENDERER (object),
                                                self->name);
      model = gtk_cell_layout_get_model (GTK_CELL_LAYOUT (layout));
    }

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  label = gtk_label_new (_("Attribute:"));
  gtk_box_append (GTK_BOX (box), label);

  button = gtk_button_new_with_label (_("Model"));
  g_object_set_data (G_OBJECT (button), "model", model);
  g_signal_connect (button, "clicked", G_CALLBACK (model_properties), self);
  gtk_box_append (GTK_BOX (box), button);

  gtk_box_append (GTK_BOX (box), gtk_label_new (_("Column:")));
  dropdown = gtk_drop_down_new (NULL, NULL);

  store = g_list_store_new (ATTRIBUTE_TYPE_HOLDER);
  holder = attribute_holder_new (-1, TRUE);
  g_list_store_append (store, holder);
  g_object_unref (holder);

  for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
    {
      sensitive = g_value_type_transformable (gtk_tree_model_get_column_type (model, i),
                                              spec->value_type);
      holder = attribute_holder_new (i, sensitive);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }
  gtk_drop_down_set_model (GTK_DROP_DOWN (dropdown), G_LIST_MODEL (store));
  g_object_unref (store);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (attribute_setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (attribute_bind_item), NULL);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (dropdown), factory);
  g_object_unref (factory);

  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), col + 1);
  attribute_mapping_changed (GTK_DROP_DOWN (dropdown), NULL, self);
  g_signal_connect (dropdown, "notify::selected",
                    G_CALLBACK (attribute_mapping_changed), self);

  gtk_box_append (GTK_BOX (box), dropdown);

  return box;
}

static GObject *
find_action_owner (GtkActionable *actionable)
{
  GtkWidget *widget = GTK_WIDGET (actionable);
  const char *full_name;
  GtkWidget *win;

  full_name = gtk_actionable_get_action_name (actionable);
  if (!full_name)
    return NULL;

  win = gtk_widget_get_ancestor (widget, GTK_TYPE_APPLICATION_WINDOW);
  if (g_str_has_prefix (full_name, "win.") == 0)
    {
      if (G_IS_OBJECT (win))
        return (GObject *)win;
    }
  else if (g_str_has_prefix (full_name, "app.") == 0)
    {
      if (GTK_IS_WINDOW (win))
        return (GObject *)gtk_window_get_application (GTK_WINDOW (win));
    }

  while (widget != NULL)
    {
      GtkActionMuxer *muxer;

      muxer = _gtk_widget_get_action_muxer (widget, FALSE);
      if (muxer && gtk_action_muxer_find (muxer, full_name, NULL))
        return (GObject *)widget;

      widget = gtk_widget_get_parent (widget);
    }

  return NULL;
}

static void
show_action_owner (GtkButton              *button,
                   GtkInspectorPropEditor *self)
{
  GObject *owner;

  owner = g_object_get_data (G_OBJECT (button), "owner");
  g_signal_emit (self, signals[SHOW_OBJECT], 0, owner, NULL, "actions");
}

static GtkWidget *
action_editor (GObject                *object,
               GtkInspectorPropEditor *self)
{
  GtkWidget *box;
  GtkWidget *button;
  GObject *owner;
  char *text;

  owner = find_action_owner (GTK_ACTIONABLE (object));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  if (owner)
    {
      /* Translators: %s is a type name, for example
       * Action from 0x2345678 (GtkApplicationWindow)
       */
      text = g_strdup_printf (_("Action from: %p (%s)"),
                              owner, g_type_name_from_instance ((GTypeInstance *)owner));
      gtk_box_append (GTK_BOX (box), gtk_label_new (text));
      g_free (text);
      button = gtk_button_new_with_label (_("Properties"));
      g_object_set_data (G_OBJECT (button), "owner", owner);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (show_action_owner), self);
      gtk_box_append (GTK_BOX (box), button);
    }

  return box;
}

static void
add_attribute_info (GtkInspectorPropEditor *self,
                    GParamSpec             *spec)
{
  if (GTK_IS_CELL_RENDERER (self->object))
    gtk_box_append (GTK_BOX (self),
                    attribute_editor (self->object, spec, self));
}

static void
add_actionable_info (GtkInspectorPropEditor *self)
{
  if (GTK_IS_ACTIONABLE (self->object) &&
      g_strcmp0 (self->name, "action-name") == 0)
    gtk_box_append (GTK_BOX (self),
                    action_editor (self->object, self));
}

static void
reset_setting (GtkInspectorPropEditor *self)
{
  gtk_settings_reset_property (GTK_SETTINGS (self->object), self->name);
}

static void
add_gtk_settings_info (GtkInspectorPropEditor *self)
{
  GObject *object;
  const char *name;
  GtkWidget *row;
  const char *source;
  GtkWidget *button;

  object = self->object;
  name = self->name;

  if (!GTK_IS_SETTINGS (object))
    return;

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  button = gtk_button_new_with_label (_("Reset"));
  gtk_box_append (GTK_BOX (row), button);
  gtk_widget_set_sensitive (button, FALSE);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (reset_setting), self);

  switch (_gtk_settings_get_setting_source (GTK_SETTINGS (object), name))
    {
    case GTK_SETTINGS_SOURCE_DEFAULT:
      source = C_("GtkSettings source", "Default");
      break;
    case GTK_SETTINGS_SOURCE_THEME:
      source = C_("GtkSettings source", "Theme");
      break;
    case GTK_SETTINGS_SOURCE_XSETTING:
      source = C_("GtkSettings source", "XSettings");
      break;
    case GTK_SETTINGS_SOURCE_APPLICATION:
      gtk_widget_set_sensitive (button, TRUE);
      source = C_("GtkSettings source", "Application");
      break;
    default:
      source = C_("GtkSettings source", "Unknown");
      break;
    }
  gtk_box_append (GTK_BOX (row), gtk_label_new (_("Source:")));
  gtk_box_append (GTK_BOX (row), gtk_label_new (source));

  gtk_box_append (GTK_BOX (self), row);
}

static void
readonly_changed (GObject    *object,
                  GParamSpec *spec,
                  gpointer    data)
{
  GValue gvalue = {0};
  char *value;
  char *type;

  g_value_init (&gvalue, spec->value_type);
  g_object_get_property (object, spec->name, &gvalue);
  strdup_value_contents (&gvalue, &value, &type);

  gtk_label_set_label (GTK_LABEL (data), value);

  g_value_unset (&gvalue);
  g_free (value);
  g_free (type);
}

static void
constructed (GObject *object)
{
  GtkInspectorPropEditor *self = GTK_INSPECTOR_PROP_EDITOR (object);
  GParamSpec *spec;
  GtkWidget *label;
  gboolean can_modify;
  GtkWidget *box;

  spec = find_property (self);

  can_modify = ((spec->flags & G_PARAM_WRITABLE) != 0 &&
                (spec->flags & G_PARAM_CONSTRUCT_ONLY) == 0);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  if ((spec->flags & G_PARAM_CONSTRUCT_ONLY) != 0)
    label = gtk_label_new ("(construct-only)");
  else if ((spec->flags & G_PARAM_WRITABLE) == 0)
    label = gtk_label_new ("(not writable)");
  else
    label = NULL;

  if (label)
    {
      gtk_widget_add_css_class (label, "dim-label");
      gtk_box_append (GTK_BOX (box), label);
    }

  /* By reaching this, we already know the property is readable.
   * Since all we can do for a GObject is dive down into it's properties
   * and inspect bindings and such, pretend to be mutable.
   */
  if (g_type_is_a (spec->value_type, G_TYPE_OBJECT))
    can_modify = TRUE;

  if (!can_modify)
    {
      label = gtk_label_new ("");
      gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
      gtk_label_set_max_width_chars (GTK_LABEL (label), 20);
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_hexpand (label, TRUE);
      gtk_widget_set_halign (label, GTK_ALIGN_FILL);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_box_append (GTK_BOX (box), label);

      readonly_changed (self->object, spec, label);
      g_object_connect_property (self->object, spec,
                                 G_CALLBACK (readonly_changed),
                                 label, G_OBJECT (label));

      if (self->size_group)
        gtk_size_group_add_widget (self->size_group, box);
      gtk_box_append (GTK_BOX (self), box);
      return;
    }

  self->self = property_editor (self->object, spec, self);
  gtk_box_append (GTK_BOX (box), self->self);
  if (self->size_group)
    gtk_size_group_add_widget (self->size_group, box);
  gtk_box_append (GTK_BOX (self), box);

  add_attribute_info (self, spec);
  add_actionable_info (self);
  add_gtk_settings_info (self);
}

static void
finalize (GObject *object)
{
  GtkInspectorPropEditor *self = GTK_INSPECTOR_PROP_EDITOR (object);

  g_free (self->name);

  G_OBJECT_CLASS (gtk_inspector_prop_editor_parent_class)->finalize (object);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorPropEditor *self = GTK_INSPECTOR_PROP_EDITOR (object);

  switch (param_id)
    {
    case PROP_OBJECT:
      g_value_set_object (value, self->object);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case PROP_SIZE_GROUP:
      g_value_set_object (value, self->size_group);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorPropEditor *self = GTK_INSPECTOR_PROP_EDITOR (object);

  switch (param_id)
    {
    case PROP_OBJECT:
      self->object = g_value_get_object (value);
      break;

    case PROP_NAME:
      g_free (self->name);
      self->name = g_value_dup_string (value);
      break;

    case PROP_SIZE_GROUP:
      self->size_group = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_prop_editor_class_init (GtkInspectorPropEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  widget_class->focus = gtk_widget_focus_child;
  widget_class->grab_focus = gtk_widget_grab_focus_child;

  signals[SHOW_OBJECT] =
    g_signal_new ("show-object",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 3, G_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_STRING);

  g_object_class_install_property (object_class, PROP_OBJECT,
      g_param_spec_object ("object", NULL, NULL,
                           G_TYPE_OBJECT, G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_NAME,
      g_param_spec_string ("name", NULL, NULL,
                           NULL, G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_SIZE_GROUP,
      g_param_spec_object ("size-group", NULL, NULL,
                           GTK_TYPE_SIZE_GROUP, G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}

GtkWidget *
gtk_inspector_prop_editor_new (GObject      *object,
                               const char   *name,
                               GtkSizeGroup *values)
{
  return g_object_new (GTK_TYPE_INSPECTOR_PROP_EDITOR,
                       "object", object,
                       "name", name,
                       "size-group", values,
                       NULL);
}

gboolean
gtk_inspector_prop_editor_should_expand (GtkInspectorPropEditor *self)
{
  if (GTK_IS_SCROLLED_WINDOW (self->self))
    {
      GtkPolicyType policy;

      g_object_get (self->self, "vscrollbar-policy", &policy, NULL);
      if (policy != GTK_POLICY_NEVER)
        return TRUE;
    }

  return FALSE;
}


// vim: set et:
