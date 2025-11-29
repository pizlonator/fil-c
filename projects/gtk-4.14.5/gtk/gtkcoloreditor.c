/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "gtkcoloreditorprivate.h"

#include <glib/gi18n-lib.h>

#include "deprecated/gtkcolorchooserprivate.h"
#include "gtkcolorplaneprivate.h"
#include "gtkcolorscaleprivate.h"
#include "gtkcolorswatchprivate.h"
#include "gtkcolorchooserwidgetprivate.h"
#include "gtkcolorutils.h"
#include "gtkcolorpickerprivate.h"
#include "gtkgrid.h"
#include "gtkbutton.h"
#include "gtkorientable.h"
#include "gtkentry.h"
#include "gtkoverlay.h"
#include "gtkadjustment.h"
#include "gtklabel.h"
#include "gtkspinbutton.h"
#include "gtkeventcontrollerkey.h"
#include "gtkroot.h"

#include <math.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct _GtkColorEditorClass GtkColorEditorClass;

struct _GtkColorEditor
{
  GtkBox parent_instance;

  GtkWidget *overlay;
  GtkWidget *grid;
  GtkWidget *swatch;
  GtkWidget *entry;
  GtkWidget *h_slider;
  GtkWidget *h_popup;
  GtkWidget *h_entry;
  GtkWidget *a_slider;
  GtkWidget *a_popup;
  GtkWidget *a_entry;
  GtkWidget *sv_plane;
  GtkWidget *sv_popup;
  GtkWidget *s_entry;
  GtkWidget *v_entry;
  GtkWidget *current_popup;
  GtkWidget *popdown_focus;

  GtkAdjustment *h_adj;
  GtkAdjustment *s_adj;
  GtkAdjustment *v_adj;
  GtkAdjustment *a_adj;

  GtkWidget *picker_button;
  GtkColorPicker *picker;

  int popup_position;

  guint text_changed : 1;
  guint use_alpha    : 1;
};

struct _GtkColorEditorClass
{
  GtkBoxClass parent_class;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_USE_ALPHA
};

static void gtk_color_editor_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorEditor, gtk_color_editor, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_editor_iface_init))

static guint
scale_round (double value, double scale)
{
  value = floor (value * scale + 0.5);
  value = MAX (value, 0);
  value = MIN (value, scale);
  return (guint)value;
}

static void
entry_set_rgba (GtkColorEditor *editor,
                const GdkRGBA  *color)
{
  char *text;

  text = g_strdup_printf ("#%02X%02X%02X",
                          scale_round (color->red, 255),
                          scale_round (color->green, 255),
                          scale_round (color->blue, 255));
  gtk_editable_set_text (GTK_EDITABLE (editor->entry), text);
  editor->text_changed = FALSE;
  g_free (text);
}

static void
entry_apply (GtkWidget      *entry,
             GtkColorEditor *editor)
{
  GdkRGBA color;
  char *text;

  if (!editor->text_changed)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (editor->entry), 0, -1);
  if (gdk_rgba_parse (&color, text))
    {
      color.alpha = gtk_adjustment_get_value (editor->a_adj);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (editor), &color);
    }

  editor->text_changed = FALSE;

  g_free (text);
}

static void
entry_focus_changed (GtkWidget      *entry,
                     GParamSpec     *pspec,
                     GtkColorEditor *editor)
{
  if (!gtk_widget_has_focus (entry))
    entry_apply (entry, editor);
}

static void
entry_text_changed (GtkWidget      *entry,
                    GParamSpec     *pspec,
                    GtkColorEditor *editor)
{
  editor->text_changed = TRUE;
}

static void
update_color (GtkColorEditor *editor,
              const GdkRGBA  *color)
{
  char *name;
  char *text;
  name = accessible_color_name (color);
  text = g_strdup_printf (_("Color: %s"), name);
  gtk_accessible_update_property (GTK_ACCESSIBLE (editor->swatch),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, text,
                                  -1);
  g_free (name);
  g_free (text);
  gtk_color_swatch_set_rgba (GTK_COLOR_SWATCH (editor->swatch), color);
  gtk_color_scale_set_rgba (GTK_COLOR_SCALE (editor->a_slider), color);
  entry_set_rgba (editor, color);
}

static void
hsv_changed (GtkColorEditor *editor)
{
  GdkRGBA color;
  double h, s, v, a;

  h = gtk_adjustment_get_value (editor->h_adj);
  s = gtk_adjustment_get_value (editor->s_adj);
  v = gtk_adjustment_get_value (editor->v_adj);
  a = gtk_adjustment_get_value (editor->a_adj);

  gtk_hsv_to_rgb (h, s, v, &color.red, &color.green, &color.blue);
  color.alpha = a;

  update_color (editor, &color);

  g_object_notify (G_OBJECT (editor), "rgba");
}

static void
dismiss_current_popup (GtkColorEditor *editor)
{
  if (editor->current_popup)
    {
      gtk_widget_set_visible (editor->current_popup, FALSE);
      editor->current_popup = NULL;
      editor->popup_position = 0;
      if (editor->popdown_focus)
        {
          if (gtk_widget_is_visible (editor->popdown_focus))
            gtk_widget_grab_focus (editor->popdown_focus);
          g_clear_object (&editor->popdown_focus);
        }
    }
}

static void
popup_edit (GtkWidget  *widget,
            const char *action_name,
            GVariant   *parameters)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (widget);
  GtkWidget *popup = NULL;
  GtkRoot *root;
  GtkWidget *focus;
  int position;
  int s, e;
  const char *param;

  param = g_variant_get_string (parameters, NULL);

  if (strcmp (param, "sv") == 0)
    {
      popup = editor->sv_popup;
      focus = editor->s_entry;
      position = 0;
    }
  else if (strcmp (param, "h") == 0)
    {
      popup = editor->h_popup;
      focus = editor->h_entry;
      gtk_range_get_slider_range (GTK_RANGE (editor->h_slider), &s, &e);
      position = (s + e) / 2;
    }
  else if (strcmp (param, "a") == 0)
    {
      popup = editor->a_popup;
      focus = editor->a_entry;
      gtk_range_get_slider_range (GTK_RANGE (editor->a_slider), &s, &e);
      position = (s + e) / 2;
    }
  else
    {
      g_warning ("unsupported popup_edit parameter %s", param);
    }

  if (popup == editor->current_popup)
    dismiss_current_popup (editor);
  else if (popup)
    {
      dismiss_current_popup (editor);
      root = gtk_widget_get_root (GTK_WIDGET (editor));
      g_set_object (&editor->popdown_focus, gtk_root_get_focus (root));
      editor->current_popup = popup;
      editor->popup_position = position;
      gtk_widget_set_visible (popup, TRUE);
      gtk_widget_grab_focus (focus);
    }
}

static gboolean
popup_key_pressed (GtkEventController *controller,
                   guint               keyval,
                   guint               keycode,
                   GdkModifierType     state,
                   GtkColorEditor     *editor)
{
  if (keyval == GDK_KEY_Escape)
    {
      dismiss_current_popup (editor);
      return TRUE;
    }

  return FALSE;
}

static gboolean
get_child_position (GtkOverlay     *overlay,
                    GtkWidget      *widget,
                    GtkAllocation  *allocation,
                    GtkColorEditor *editor)
{
  GtkRequisition req;
  graphene_point_t p;

  gtk_widget_get_preferred_size (widget, &req, NULL);

  allocation->x = 0;
  allocation->y = 0;
  allocation->width = req.width;
  allocation->height = req.height;

  if (widget == editor->sv_popup)
    {
      if (!gtk_widget_compute_point (editor->sv_plane,
                                     gtk_widget_get_parent (editor->grid),
                                     &GRAPHENE_POINT_INIT (0, -6),
                                     &p))
        return FALSE;
      if (gtk_widget_get_direction (GTK_WIDGET (overlay)) == GTK_TEXT_DIR_RTL)
        p.x = 0;
      else
        p.x = gtk_widget_get_width (GTK_WIDGET (overlay)) - req.width;
    }
  else if (widget == editor->h_popup)
    {
      int slider_width;

      slider_width = gtk_widget_get_width (editor->h_slider);

      if (!gtk_widget_compute_point (editor->h_slider,
                                     gtk_widget_get_parent (editor->grid),
                                     gtk_widget_get_direction (GTK_WIDGET (overlay)) == GTK_TEXT_DIR_RTL
                                       ? &GRAPHENE_POINT_INIT (- req.width - 6, editor->popup_position - req.height / 2)
                                       : &GRAPHENE_POINT_INIT (slider_width + 6, editor->popup_position - req.height / 2),
                                     &p))
        return FALSE;
    }
  else if (widget == editor->a_popup)
    {
      if (!gtk_widget_compute_point (editor->a_slider,
                                     gtk_widget_get_parent (editor->grid),
                                     &GRAPHENE_POINT_INIT (editor->popup_position - req.width / 2, - req.height - 6),
                                     &p))
        return FALSE;
    }
  else
    return FALSE;

  allocation->x = CLAMP (p.x, 0, gtk_widget_get_width (GTK_WIDGET (overlay)) - req.width);
  allocation->y = CLAMP (p.y, 0, gtk_widget_get_height (GTK_WIDGET (overlay)) - req.height);

  return TRUE;
}

static void
value_changed (GtkAdjustment *a,
               GtkAdjustment *as)
{
  double scale;

  scale = gtk_adjustment_get_upper (as) / gtk_adjustment_get_upper (a);
  g_signal_handlers_block_by_func (as, value_changed, a);
  gtk_adjustment_set_value (as, gtk_adjustment_get_value (a) * scale);
  g_signal_handlers_unblock_by_func (as, value_changed, a);
}

static GtkAdjustment *
scaled_adjustment (GtkAdjustment *a,
                   double         scale)
{
  GtkAdjustment *as;

  as = gtk_adjustment_new (gtk_adjustment_get_value (a) * scale,
                           gtk_adjustment_get_lower (a) * scale,
                           gtk_adjustment_get_upper (a) * scale,
                           gtk_adjustment_get_step_increment (a) * scale,
                           gtk_adjustment_get_page_increment (a) * scale,
                           gtk_adjustment_get_page_size (a) * scale);

  g_signal_connect (a, "value-changed", G_CALLBACK (value_changed), as);
  g_signal_connect (as, "value-changed", G_CALLBACK (value_changed), a);

  return as;
}

static void
color_picked (GObject      *source,
              GAsyncResult *res,
              gpointer      data)
{
  GtkColorPicker *picker = GTK_COLOR_PICKER (source);
  GtkColorEditor *editor = data;
  GError *error = NULL;
  GdkRGBA *color;

  color = gtk_color_picker_pick_finish (picker, res, &error);
  if (color == NULL)
    {
      g_error_free (error);
    }
  else
    {
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (editor), color);
      gdk_rgba_free (color);
    }
}

static void
pick_color (GtkButton      *button,
            GtkColorEditor *editor)
{
  gtk_color_picker_pick (editor->picker, color_picked, editor);
}

static void
gtk_color_editor_init (GtkColorEditor *editor)
{
  GtkEventController *controller;

  editor->use_alpha = TRUE;

  g_type_ensure (GTK_TYPE_COLOR_SCALE);
  g_type_ensure (GTK_TYPE_COLOR_PLANE);
  g_type_ensure (GTK_TYPE_COLOR_SWATCH);
  gtk_widget_init_template (GTK_WIDGET (editor));

  /* Create the scaled popup adjustments manually here because connecting user data is not
   * supported by template GtkBuilder xml (it would be possible to set this up in the xml
   * but require 4 separate callbacks and would be rather ugly).
   */
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->h_entry), scaled_adjustment (editor->h_adj, 360));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->s_entry), scaled_adjustment (editor->s_adj, 100));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->v_entry), scaled_adjustment (editor->v_adj, 100));
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (editor->a_entry), scaled_adjustment (editor->a_adj, 100));

  /* This can be setup in the .ui file, but requires work in Glade otherwise it cannot be edited there */
  gtk_overlay_add_overlay (GTK_OVERLAY (editor->overlay), editor->sv_popup);
  gtk_overlay_add_overlay (GTK_OVERLAY (editor->overlay), editor->h_popup);
  gtk_overlay_add_overlay (GTK_OVERLAY (editor->overlay), editor->a_popup);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (popup_key_pressed), editor);
  gtk_widget_add_controller (editor->h_entry, controller);
  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (popup_key_pressed), editor);
  gtk_widget_add_controller (editor->s_entry, controller);
  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (popup_key_pressed), editor);
  gtk_widget_add_controller (editor->v_entry, controller);
  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed", G_CALLBACK (popup_key_pressed), editor);
  gtk_widget_add_controller (editor->a_entry, controller);

  gtk_widget_remove_css_class (editor->swatch, "activatable");

  editor->picker = gtk_color_picker_new ();
  gtk_widget_set_visible (editor->picker_button, editor->picker != NULL);
}

static void
gtk_color_editor_dispose (GObject *object)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (object);

  dismiss_current_popup (editor);
  g_clear_object (&editor->picker);

  G_OBJECT_CLASS (gtk_color_editor_parent_class)->dispose (object);
}

static void
gtk_color_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkColorEditor *ce = GTK_COLOR_EDITOR (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;
        gtk_color_chooser_get_rgba (cc, &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_widget_get_visible (ce->a_slider));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_set_use_alpha (GtkColorEditor *editor,
                                gboolean        use_alpha)
{
  if (editor->use_alpha != use_alpha)
    {
      editor->use_alpha = use_alpha;
      gtk_widget_set_visible (editor->a_slider, use_alpha);
      gtk_color_swatch_set_use_alpha (GTK_COLOR_SWATCH (editor->swatch), use_alpha);
    }
}

static void
gtk_color_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkColorEditor *ce = GTK_COLOR_EDITOR (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (cc, g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      gtk_color_editor_set_use_alpha (ce, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_editor_class_init (GtkColorEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = gtk_color_editor_dispose;
  object_class->get_property = gtk_color_editor_get_property;
  object_class->set_property = gtk_color_editor_set_property;

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkcoloreditor.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, overlay);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, grid);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, swatch);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, entry);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, h_slider);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, h_popup);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, h_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, a_slider);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, a_popup);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, a_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, sv_plane);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, sv_popup);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, s_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, v_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, h_adj);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, s_adj);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, v_adj);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, a_adj);
  gtk_widget_class_bind_template_child (widget_class, GtkColorEditor, picker_button);

  gtk_widget_class_bind_template_callback (widget_class, hsv_changed);
  gtk_widget_class_bind_template_callback (widget_class, dismiss_current_popup);
  gtk_widget_class_bind_template_callback (widget_class, get_child_position);
  gtk_widget_class_bind_template_callback (widget_class, entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, entry_apply);
  gtk_widget_class_bind_template_callback (widget_class, entry_focus_changed);
  gtk_widget_class_bind_template_callback (widget_class, pick_color);

  /**
   * GtkColorEditor|color.edit:
   * @component: the component to edit, "h", "sv" or "a"
   *
   * Opens the edit popup for one of the color components.
   */ 
  gtk_widget_class_install_action (widget_class, "color.edit", "s", popup_edit);
}

static void
gtk_color_editor_get_rgba (GtkColorChooser *chooser,
                           GdkRGBA         *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);
  float h, s, v;

  h = gtk_adjustment_get_value (editor->h_adj);
  s = gtk_adjustment_get_value (editor->s_adj);
  v = gtk_adjustment_get_value (editor->v_adj);
  gtk_hsv_to_rgb (h, s, v, &color->red, &color->green, &color->blue);
  color->alpha = gtk_adjustment_get_value (editor->a_adj);
}

static void
gtk_color_editor_set_rgba (GtkColorChooser *chooser,
                           const GdkRGBA   *color)
{
  GtkColorEditor *editor = GTK_COLOR_EDITOR (chooser);
  float h, s, v;

  gtk_rgb_to_hsv (color->red, color->green, color->blue, &h, &s, &v);

  gtk_adjustment_set_value (editor->h_adj, h);
  gtk_adjustment_set_value (editor->s_adj, s);
  gtk_adjustment_set_value (editor->v_adj, v);
  gtk_adjustment_set_value (editor->a_adj, color->alpha);

  update_color (editor, color);

  g_object_notify (G_OBJECT (editor), "rgba");
}

static void
gtk_color_editor_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_editor_get_rgba;
  iface->set_rgba = gtk_color_editor_set_rgba;
}

GtkWidget *
gtk_color_editor_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_COLOR_EDITOR, NULL);
}
