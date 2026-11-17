/* cc-split-row.c
 *
 * Copyright 2018 Purism SPC
 *           2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *           2023 Red Hat, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cc-split-row.h"

struct _CcSplitRow
{
  CcVerticalRow      parent;

  GtkWidget         *box;
  GtkSizeGroup      *size_group;

  GtkPicture        *default_option_picture;
  GtkPicture        *alternative_option_picture;

  GtkCheckButton    *alternative_option_checkbutton;
  GtkCheckButton    *default_option_checkbutton;

  gchar             *alternative_resource_path;
  gchar             *default_resource_path;

  gchar             *alternative_option_title;
  gchar             *alternative_option_subtitle;
  gchar             *default_option_title;
  gchar             *default_option_subtitle;

  gboolean           use_default;
  gboolean           compact;
};

G_DEFINE_FINAL_TYPE (CcSplitRow, cc_split_row, CC_TYPE_VERTICAL_ROW);

enum
{
  PROP_0,
  PROP_USE_DEFAULT,
  PROP_COMPACT,
  PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE,
  PROP_ALTERNATIVE_OPTION_TITLE,
  PROP_ALTERNATIVE_OPTION_SUBTITLE,
  PROP_DEFAULT_ILLUSTRATION_RESOURCE,
  PROP_DEFAULT_OPTION_TITLE,
  PROP_DEFAULT_OPTION_SUBTITLE,
  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { NULL, };

static void
on_default_option_click_released_cb (CcSplitRow *self)
{
  gtk_widget_activate (GTK_WIDGET (self->default_option_checkbutton));
}

static void
on_alternative_option_click_released_cb (CcSplitRow *self)
{
  gtk_widget_activate (GTK_WIDGET (self->alternative_option_checkbutton));
}

static void
focus_leave (GtkPicture *picture)
{
  GtkMediaStream *stream;
  GdkPaintable *paintable;

  paintable = gtk_picture_get_paintable (picture);

  if (!GTK_IS_MEDIA_STREAM (paintable))
    return;

  stream = GTK_MEDIA_STREAM (paintable);
  gtk_media_stream_set_loop (stream, FALSE);
  gtk_media_stream_pause (stream);
}

static void
on_default_option_focus_leave_cb (CcSplitRow *self)
{
  focus_leave (self->default_option_picture);
}

static void
on_alternative_option_focus_leave_cb (CcSplitRow *self)
{
  focus_leave (self->alternative_option_picture);
}

static void
focus_enter (GtkPicture *picture)
{
  GtkMediaStream *stream;
  GdkPaintable *paintable;

  paintable = gtk_picture_get_paintable (picture);

  if (!GTK_IS_MEDIA_STREAM (paintable))
    return;

  stream = GTK_MEDIA_STREAM (paintable);
  gtk_media_stream_set_loop (stream, TRUE);
  gtk_media_stream_play (stream);
}

static void
on_default_option_focus_enter_cb (CcSplitRow *self)
{
  focus_enter (self->default_option_picture);
}

static void
on_alternative_option_focus_enter_cb (CcSplitRow *self)
{
  focus_enter (self->alternative_option_picture);
}

static void
on_checkbutton_toggled_cb (CcSplitRow *self)
{
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USE_DEFAULT]);
}

static void
cc_split_row_dispose (GObject *object)
{
  CcSplitRow *self = CC_SPLIT_ROW (object);

  g_clear_pointer (&self->default_resource_path, g_free);
  g_clear_pointer (&self->alternative_resource_path, g_free);
  g_clear_pointer (&self->alternative_option_title, g_free);
  g_clear_pointer (&self->alternative_option_subtitle, g_free);
  g_clear_pointer (&self->default_option_title, g_free);
  g_clear_pointer (&self->default_option_subtitle, g_free);

  G_OBJECT_CLASS (cc_split_row_parent_class)->dispose (object);
}

static void
cc_split_row_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  CcSplitRow *self = CC_SPLIT_ROW (object);

  switch (prop_id)
    {
    case PROP_USE_DEFAULT:
      g_value_set_boolean (value, gtk_check_button_get_active (self->default_option_checkbutton));
      break;
    case PROP_COMPACT:
      g_value_set_boolean (value, cc_split_row_get_compact (self));
      break;
    case PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE:
      g_value_set_string (value, cc_split_row_get_alternative_illustration_resource (self));
      break;
    case PROP_ALTERNATIVE_OPTION_TITLE:
      g_value_set_string (value, cc_split_row_get_alternative_option_title (self));
      break;
    case PROP_ALTERNATIVE_OPTION_SUBTITLE:
      g_value_set_string (value, cc_split_row_get_alternative_option_subtitle (self));
      break;
    case PROP_DEFAULT_ILLUSTRATION_RESOURCE:
      g_value_set_string (value, cc_split_row_get_default_illustration_resource (self));
      break;
    case PROP_DEFAULT_OPTION_TITLE:
      g_value_set_string (value, cc_split_row_get_default_option_title (self));
      break;
    case PROP_DEFAULT_OPTION_SUBTITLE:
      g_value_set_string (value, cc_split_row_get_default_option_subtitle (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_split_row_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  CcSplitRow *self = CC_SPLIT_ROW (object);

  switch (prop_id)
    {
    case PROP_USE_DEFAULT:
      cc_split_row_set_use_default (self, g_value_get_boolean (value));
      break;
    case PROP_COMPACT:
      cc_split_row_set_compact (self, g_value_get_boolean (value));
      break;
    case PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE:
      cc_split_row_set_alternative_illustration_resource (self, g_value_get_string (value));
      break;
    case PROP_ALTERNATIVE_OPTION_TITLE:
      cc_split_row_set_alternative_option_title (self, g_value_get_string (value));
      break;
    case PROP_ALTERNATIVE_OPTION_SUBTITLE:
      cc_split_row_set_alternative_option_subtitle (self, g_value_get_string (value));
      break;
    case PROP_DEFAULT_ILLUSTRATION_RESOURCE:
      cc_split_row_set_default_illustration_resource (self, g_value_get_string (value));
      break;
    case PROP_DEFAULT_OPTION_TITLE:
      cc_split_row_set_default_option_title (self, g_value_get_string (value));
      break;
    case PROP_DEFAULT_OPTION_SUBTITLE:
      cc_split_row_set_default_option_subtitle (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_split_row_class_init (CcSplitRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_split_row_dispose;
  object_class->get_property = cc_split_row_get_property;
  object_class->set_property = cc_split_row_set_property;

  props[PROP_USE_DEFAULT] =
    g_param_spec_boolean ("use-default",
                          "Use Default",
                          "Use Default",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_COMPACT] =
    g_param_spec_boolean ("compact",
                          "Compact",
                          "Compact",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE] =
    g_param_spec_string ("alternative-illustration-resource",
                         "Alternative illustration resource",
                         "Alternative illustration resource",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_ALTERNATIVE_OPTION_TITLE] =
    g_param_spec_string ("alternative-option-title",
                         "Alternative option title",
                         "Alternative option title",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_ALTERNATIVE_OPTION_SUBTITLE] =
    g_param_spec_string ("alternative-option-subtitle",
                         "Alternative option subtitle",
                         "Alternative option subtitle",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_DEFAULT_ILLUSTRATION_RESOURCE] =
    g_param_spec_string ("default-illustration-resource",
                         "Default illustration resource",
                         "Default illustration resource",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_DEFAULT_OPTION_TITLE] =
    g_param_spec_string ("default-option-title",
                         "Default option title",
                         "Default option title",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  props[PROP_DEFAULT_OPTION_SUBTITLE] =
    g_param_spec_string ("default-option-subtitle",
                         "Default option subtitle",
                         "Default option subtitle",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-split-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, box);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, size_group);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, alternative_option_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, alternative_option_picture);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, default_option_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, CcSplitRow, default_option_picture);

  gtk_widget_class_bind_template_callback (widget_class, on_checkbutton_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_default_option_click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_alternative_option_click_released_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_default_option_focus_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_alternative_option_focus_enter_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_default_option_focus_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_alternative_option_focus_leave_cb);
}

static void
cc_split_row_init (CcSplitRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_widget_set_name (GTK_WIDGET (self), "split-row");
  gtk_widget_add_css_class (GTK_WIDGET (self), "split-row");
}

const gchar *
cc_split_row_get_default_illustration_resource (CcSplitRow *self)
{
  return self->default_resource_path;
}

void
cc_split_row_set_default_illustration_resource (CcSplitRow  *self,
                                                const gchar *resource_path)
{
  g_autoptr(GtkMediaStream) media_file = NULL;

  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  g_set_str (&self->default_resource_path, resource_path);
  media_file = gtk_media_file_new_for_resource (resource_path);

  gtk_picture_set_paintable (self->default_option_picture, GDK_PAINTABLE (media_file));
  gtk_widget_set_visible (GTK_WIDGET (self->default_option_picture),
                          resource_path != NULL && g_strcmp0 (resource_path, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_ILLUSTRATION_RESOURCE]);
}

const gchar *
cc_split_row_get_alternative_illustration_resource (CcSplitRow *self)
{
  return self->alternative_resource_path;
}

void
cc_split_row_set_alternative_illustration_resource (CcSplitRow  *self,
                                                    const gchar *resource_path)
{
  g_autoptr(GtkMediaStream) media_file = NULL;

  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  g_set_str (&self->alternative_resource_path, resource_path);
  media_file = gtk_media_file_new_for_resource (resource_path);
  gtk_media_stream_set_loop (media_file, TRUE);

  gtk_picture_set_paintable (self->alternative_option_picture, GDK_PAINTABLE (media_file));
  gtk_widget_set_visible (GTK_WIDGET (self->alternative_option_picture),
                          resource_path != NULL && g_strcmp0 (resource_path, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALTERNATIVE_ILLUSTRATION_RESOURCE]);
}

void
cc_split_row_set_use_default (CcSplitRow *self,
                              gboolean    use_default)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  gtk_check_button_set_active (use_default ? self->default_option_checkbutton : self->alternative_option_checkbutton, TRUE);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USE_DEFAULT]);
}

gboolean
cc_split_row_get_use_default (CcSplitRow *self)
{
  return gtk_check_button_get_active (self->default_option_checkbutton);
}

void
cc_split_row_set_compact (CcSplitRow *self,
                          gboolean    compact)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  self->compact = !!compact;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self->box),
                                  compact ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
  gtk_size_group_set_mode (self->size_group,
                           compact ? GTK_SIZE_GROUP_NONE : GTK_SIZE_GROUP_VERTICAL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COMPACT]);
}

gboolean
cc_split_row_get_compact (CcSplitRow *self)
{
  return self->compact;
}

const gchar *
cc_split_row_get_default_option_title (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->default_option_title;
}

void
cc_split_row_set_default_option_title (CcSplitRow  *self,
                                       const gchar *title)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->default_option_title, title))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_OPTION_TITLE]);
}

const gchar *
cc_split_row_get_default_option_subtitle (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->default_option_subtitle;
}

void
cc_split_row_set_default_option_subtitle (CcSplitRow  *self,
                                          const gchar *subtitle)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->default_option_subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DEFAULT_OPTION_SUBTITLE]);
}

const gchar *
cc_split_row_get_alternative_option_title (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->alternative_option_title;
}

void
cc_split_row_set_alternative_option_title (CcSplitRow  *self,
                                           const gchar *title)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->alternative_option_title, title))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALTERNATIVE_OPTION_TITLE]);
}

const gchar *
cc_split_row_get_alternative_option_subtitle (CcSplitRow *self)
{
  g_return_val_if_fail (CC_IS_SPLIT_ROW (self), NULL);

  return self->alternative_option_subtitle;
}

void
cc_split_row_set_alternative_option_subtitle (CcSplitRow  *self,
                                              const gchar *subtitle)
{
  g_return_if_fail (CC_IS_SPLIT_ROW (self));

  if (g_set_str (&self->alternative_option_subtitle, subtitle))
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ALTERNATIVE_OPTION_SUBTITLE]);
}
