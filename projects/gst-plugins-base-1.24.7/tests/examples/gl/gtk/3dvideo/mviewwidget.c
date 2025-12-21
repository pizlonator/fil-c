/*
 * GStreamer
 * Copyright (C) 2014-2015 Jan Schmidt <jan@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mviewwidget.h"

G_DEFINE_TYPE (GstMViewWidget, gst_mview_widget, GTK_TYPE_GRID);

static void gst_mview_widget_constructed (GObject * o);
static void gst_mview_widget_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_mview_widget_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

#define DEFAULT_DOWNMIX GST_GL_STEREO_DOWNMIX_ANAGLYPH_GREEN_MAGENTA_DUBOIS

enum
{
  PROP_0,
  PROP_IS_OUTPUT,
  PROP_MODE_SELECTOR,
  PROP_FLAGS,
  PROP_DOWNMIX_MODE
};

typedef struct _ToggleClosure
{
  GstMViewWidget *mv;
  GstVideoMultiviewFlags flag;
} ToggleClosure;

static GtkWidget *combo_box_from_enum (GType enum_type);

static void
gst_mview_widget_class_init (GstMViewWidgetClass * klass)
{
  GObjectClass *object_klass = (GObjectClass *) (klass);

  object_klass->constructed = gst_mview_widget_constructed;
  object_klass->set_property = gst_mview_widget_set_property;
  object_klass->get_property = gst_mview_widget_get_property;

  g_object_class_install_property (object_klass, PROP_IS_OUTPUT,
      g_param_spec_boolean ("is-output", "Is an Output widget",
          "TRUE if the widget should have downmix mode", FALSE,
          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_klass, PROP_MODE_SELECTOR,
      g_param_spec_object ("mode-selector", "Multiview Mode selector",
          "Multiview Mode selector widget",
          GTK_TYPE_WIDGET, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_klass, PROP_FLAGS,
      g_param_spec_flags ("flags", "Multiview Flags",
          "multiview flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGS,
          GST_VIDEO_MULTIVIEW_FLAGS_NONE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_klass, PROP_DOWNMIX_MODE,
      g_param_spec_enum ("downmix-mode",
          "Mode for mono downmixed output",
          "Output anaglyph type to generate when downmixing to mono",
          GST_TYPE_GL_STEREO_DOWNMIX, DEFAULT_DOWNMIX,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_mview_widget_init (GstMViewWidget * mv)
{
}

static void
flag_changed (GObject * w, ToggleClosure * c)
{
  GstMViewWidget *mv = GST_MVIEW_WIDGET (c->mv);
  gboolean flag_set;

  g_object_get (w, "active", &flag_set, NULL);

  if (flag_set)
    mv->flags |= c->flag;
  else
    mv->flags &= ~(c->flag);
  if (!mv->synching)
    g_object_notify (G_OBJECT (mv), "flags");
}

static void
link_button_to_flag (GstMViewWidget * mv, GtkWidget * w,
    GstVideoMultiviewFlags flag)
{
  ToggleClosure *c = g_new0 (ToggleClosure, 1);

  c->mv = mv;
  c->flag = flag;

  g_signal_connect_data (G_OBJECT (w), "toggled", G_CALLBACK (flag_changed),
      c, (GClosureNotify) g_free, 0);
}

static void
sync_flags (GstMViewWidget * mv)
{
  mv->synching = TRUE;
  g_object_set (G_OBJECT (mv->lflip), "active",
      !!(mv->flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED), NULL);
  g_object_set (G_OBJECT (mv->lflop), "active",
      !!(mv->flags & GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED), NULL);
  g_object_set (G_OBJECT (mv->rflip), "active",
      !!(mv->flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED), NULL);
  g_object_set (G_OBJECT (mv->rflop), "active",
      !!(mv->flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED), NULL);
  g_object_set (G_OBJECT (mv->right_first), "active",
      !!(mv->flags & GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST), NULL);
  g_object_set (G_OBJECT (mv->half_aspect), "active",
      !!(mv->flags & GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT), NULL);
  mv->synching = FALSE;
}

static const gchar *
enum_value_to_nick (GType enum_type, guint value)
{
  GEnumClass *enum_info;
  GEnumValue *v;
  const gchar *nick;

  enum_info = (GEnumClass *) (g_type_class_ref (enum_type));
  g_return_val_if_fail (enum_info != NULL, NULL);

  v = g_enum_get_value (enum_info, value);
  g_return_val_if_fail (v != NULL, NULL);

  nick = v->value_nick;

  g_type_class_unref (enum_info);

  return nick;
}

static void
sync_downmix (GstMViewWidget * mv)
{
  mv->synching = TRUE;
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (mv->downmix_combo),
      enum_value_to_nick (GST_TYPE_GL_STEREO_DOWNMIX, mv->downmix_mode));
  mv->synching = FALSE;
}

static gboolean
set_downmix_mode (GtkWidget * widget, gpointer data)
{
  GstMViewWidget *mv = GST_MVIEW_WIDGET (data);
  gchar *downmix_mode = NULL;
  GEnumClass *p_class;
  GEnumValue *v;
  GParamSpec *p =
      g_object_class_find_property (G_OBJECT_GET_CLASS (mv), "downmix-mode");

  g_return_val_if_fail (p != NULL, FALSE);

  p_class = G_PARAM_SPEC_ENUM (p)->enum_class;
  g_return_val_if_fail (p_class != NULL, FALSE);

  g_object_get (G_OBJECT (widget), "active-id", &downmix_mode, NULL);
  g_return_val_if_fail (downmix_mode != NULL, FALSE);

  v = g_enum_get_value_by_nick (p_class, downmix_mode);
  g_return_val_if_fail (v != NULL, FALSE);

  mv->downmix_mode = v->value;
  if (!mv->synching)
    g_object_notify (G_OBJECT (mv), "downmix-mode");

  return FALSE;
}

static void
gst_mview_widget_constructed (GObject * o)
{
  GstMViewWidget *mv = GST_MVIEW_WIDGET (o);
  GtkGrid *g = GTK_GRID (mv);
  GtkWidget *w;

  gtk_widget_set_has_window (GTK_WIDGET (mv), FALSE);

  if (mv->is_output) {
    mv->mode_selector = w = combo_box_from_enum (GST_TYPE_VIDEO_MULTIVIEW_MODE);
    gtk_grid_attach (g, gtk_label_new ("Output:"), 0, 0, 1, 1);
  } else {
    mv->mode_selector = w =
        combo_box_from_enum (GST_TYPE_VIDEO_MULTIVIEW_FRAME_PACKING);
    gtk_grid_attach (g, gtk_label_new ("Input:"), 0, 0, 1, 1);
  }
  gtk_grid_attach (g, mv->mode_selector, 1, 0, 3, 1);

  gtk_grid_attach (g, gtk_label_new (" Left "), 4, 0, 1, 1);
  mv->lflip = w = gtk_toggle_button_new_with_label ("Flip");
  link_button_to_flag (mv, w, GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLIPPED);
  gtk_grid_attach (g, w, 5, 0, 1, 1);
  mv->lflop = w = gtk_toggle_button_new_with_label ("Flop");
  link_button_to_flag (mv, w, GST_VIDEO_MULTIVIEW_FLAGS_LEFT_FLOPPED);
  gtk_grid_attach (g, w, 6, 0, 1, 1);

  gtk_grid_attach (g, gtk_label_new (" Right "), 4, 1, 1, 1);
  mv->rflip = w = gtk_toggle_button_new_with_label ("Flip");
  link_button_to_flag (mv, w, GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLIPPED);
  gtk_grid_attach (g, w, 5, 1, 1, 1);
  mv->rflop = w = gtk_toggle_button_new_with_label ("Flop");
  link_button_to_flag (mv, w, GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_FLOPPED);
  gtk_grid_attach (g, w, 6, 1, 1, 1);

  mv->right_first = w = gtk_toggle_button_new_with_label ("Left/Right swap");
  link_button_to_flag (mv, w, GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST);
  gtk_grid_attach (g, w, 1, 1, 1, 1);
  mv->half_aspect = w = gtk_toggle_button_new_with_label ("Half-Aspect");
  link_button_to_flag (mv, w, GST_VIDEO_MULTIVIEW_FLAGS_HALF_ASPECT);
  gtk_grid_attach (g, w, 2, 1, 1, 1);

  if (mv->is_output) {
    mv->downmix_combo = w = combo_box_from_enum (GST_TYPE_GL_STEREO_DOWNMIX);
    gtk_grid_attach (g, w, 1, 2, 3, 1);
    sync_downmix (mv);
    g_signal_connect (G_OBJECT (w), "changed",
        G_CALLBACK (set_downmix_mode), mv);
  }
}

static void
gst_mview_widget_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstMViewWidget *mv = GST_MVIEW_WIDGET (object);
  switch (prop_id) {
    case PROP_IS_OUTPUT:
      mv->is_output = g_value_get_boolean (value);
      break;
    case PROP_FLAGS:
      mv->flags = (GstVideoMultiviewFlags) g_value_get_flags (value);
      sync_flags (mv);
      break;
    case PROP_DOWNMIX_MODE:
      mv->downmix_mode = g_value_get_enum (value);
      sync_downmix (mv);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mview_widget_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstMViewWidget *mv = GST_MVIEW_WIDGET (object);
  switch (prop_id) {
    case PROP_IS_OUTPUT:
      g_value_set_boolean (value, mv->is_output);
      break;
    case PROP_MODE_SELECTOR:
      g_value_set_object (value, mv->mode_selector);
      break;
    case PROP_FLAGS:
      g_value_set_flags (value, mv->flags);
      break;
    case PROP_DOWNMIX_MODE:
      g_value_set_enum (value, mv->downmix_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GtkWidget *
combo_box_from_enum (GType enum_type)
{
  GEnumClass *enum_info;
  GtkWidget *combo;
  guint i;

  enum_info = (GEnumClass *) (g_type_class_ref (enum_type));
  g_return_val_if_fail (enum_info != NULL, NULL);

  combo = gtk_combo_box_text_new ();
  for (i = 0; i < enum_info->n_values; i++) {
    GEnumValue *v = enum_info->values + i;
    gtk_combo_box_text_insert (GTK_COMBO_BOX_TEXT (combo),
        i, v->value_nick, v->value_name);
  }

  g_type_class_unref (enum_info);

  return combo;
}

GtkWidget *
gst_mview_widget_new (gboolean is_output)
{
  GtkWidget *ret;

  ret = g_object_new (GST_TYPE_MVIEW_WIDGET, "is-output", is_output, NULL);

  return ret;
}
