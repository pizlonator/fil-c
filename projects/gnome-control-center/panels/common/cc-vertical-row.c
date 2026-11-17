/* cc-vertical-row.c
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

#include "cc-vertical-row.h"

typedef struct
{
  AdwPreferencesRow  parent;

  GtkBox            *content_box;
  GtkBox            *header;
  GtkImage          *image;
  GtkBox            *prefixes;
  GtkLabel          *subtitle;
  GtkBox            *suffixes;
  GtkLabel          *title;
  GtkBox            *title_box;

  GtkWidget         *previous_parent;

  gboolean           use_underline;
  gint               title_lines;
  gint               subtitle_lines;
  GtkWidget         *activatable_widget;
} CcVerticalRowPrivate;

static GtkBuildableIface *parent_buildable_iface;

static void cc_vertical_row_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcVerticalRow, cc_vertical_row, ADW_TYPE_PREFERENCES_ROW,
                         G_ADD_PRIVATE (CcVerticalRow) G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, cc_vertical_row_buildable_init))

enum
{
  PROP_0,
  PROP_ICON_NAME,
  PROP_ACTIVATABLE_WIDGET,
  PROP_SUBTITLE,
  PROP_USE_UNDERLINE,
  PROP_TITLE_LINES,
  PROP_SUBTITLE_LINES,
  N_PROPS,
};

enum
{
  SIGNAL_ACTIVATED,
  SIGNAL_LAST_SIGNAL,
};

static GParamSpec *props[N_PROPS] = { NULL, };
static guint signals[SIGNAL_LAST_SIGNAL] = { 0, };

static void
row_activated_cb (CcVerticalRow *self,
                  GtkListBoxRow *row)
{
  /* No need to use GTK_LIST_BOX_ROW() for a pointer comparison. */
  if ((GtkListBoxRow *) self == row)
    cc_vertical_row_activate (self);
}

static void
parent_cb (CcVerticalRow *self)
{
  GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (self));
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  if (priv->previous_parent != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->previous_parent,
                                            G_CALLBACK (row_activated_cb),
                                            self);
      priv->previous_parent = NULL;
    }

  if (parent == NULL || !GTK_IS_LIST_BOX (parent))
    return;

  priv->previous_parent = parent;
  g_signal_connect_swapped (parent, "row-activated", G_CALLBACK (row_activated_cb), self);
}

static void
update_subtitle_visibility (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  gtk_widget_set_visible (GTK_WIDGET (priv->subtitle),
                          gtk_label_get_text (priv->subtitle) != NULL &&
                          g_strcmp0 (gtk_label_get_text (priv->subtitle), "") != 0);
}

static void
cc_vertical_row_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcVerticalRow *self = CC_VERTICAL_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      g_value_set_string (value, cc_vertical_row_get_icon_name (self));
      break;
    case PROP_ACTIVATABLE_WIDGET:
      g_value_set_object (value, (GObject *) cc_vertical_row_get_activatable_widget (self));
      break;
    case PROP_SUBTITLE:
      g_value_set_string (value, cc_vertical_row_get_subtitle (self));
      break;
    case PROP_SUBTITLE_LINES:
      g_value_set_int (value, cc_vertical_row_get_subtitle_lines (self));
      break;
    case PROP_TITLE_LINES:
      g_value_set_int (value, cc_vertical_row_get_title_lines (self));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, cc_vertical_row_get_use_underline (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_vertical_row_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcVerticalRow *self = CC_VERTICAL_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON_NAME:
      cc_vertical_row_set_icon_name (self, g_value_get_string (value));
      break;
    case PROP_ACTIVATABLE_WIDGET:
      cc_vertical_row_set_activatable_widget (self, (GtkWidget*) g_value_get_object (value));
      break;
    case PROP_SUBTITLE:
      cc_vertical_row_set_subtitle (self, g_value_get_string (value));
      break;
    case PROP_SUBTITLE_LINES:
      cc_vertical_row_set_subtitle_lines (self, g_value_get_int (value));
      break;
    case PROP_TITLE_LINES:
      cc_vertical_row_set_title_lines (self, g_value_get_int (value));
      break;
    case PROP_USE_UNDERLINE:
      cc_vertical_row_set_use_underline (self, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_vertical_row_dispose (GObject *object)
{
  CcVerticalRow *self = CC_VERTICAL_ROW (object);
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  if (priv->previous_parent != NULL) {
    g_signal_handlers_disconnect_by_func (priv->previous_parent, G_CALLBACK (row_activated_cb), self);
    priv->previous_parent = NULL;
  }

  cc_vertical_row_set_activatable_widget (self, NULL);
  g_clear_pointer ((GtkWidget**)&priv->header, gtk_widget_unparent);

  G_OBJECT_CLASS (cc_vertical_row_parent_class)->dispose (object);
}

static void
cc_vertical_row_class_init (CcVerticalRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_vertical_row_get_property;
  object_class->set_property = cc_vertical_row_set_property;
  object_class->dispose = cc_vertical_row_dispose;

  props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "Icon name",
                         "Icon name",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_ACTIVATABLE_WIDGET] =
      g_param_spec_object ("activatable-widget",
                           "Activatable widget",
                           "The widget to be activated when the row is activated",
                           GTK_TYPE_WIDGET,
                           G_PARAM_READWRITE);

  props[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "Subtitle",
                         "Subtitle",
                         "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_USE_UNDERLINE] =
    g_param_spec_boolean ("use-underline",
                          "Use underline",
                          "If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_TITLE_LINES] =
    g_param_spec_int ("title-lines",
                      "Number of title lines",
                      "The desired number of title lines",
                      0, G_MAXINT,
                      1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_SUBTITLE_LINES] =
    g_param_spec_int ("subtitle-lines",
                      "Number of subtitle lines",
                      "The desired number of subtitle lines",
                      0, G_MAXINT,
                      1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, props);

  signals[SIGNAL_ACTIVATED] =
    g_signal_new ("activated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE,
                  0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-vertical-row.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, content_box);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, header);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, image);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, prefixes);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, subtitle);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, suffixes);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, title);
  gtk_widget_class_bind_template_child_private (widget_class, CcVerticalRow, title_box);
}

static gboolean
string_is_not_empty (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  const gchar *string = g_value_get_string (from_value);

  g_value_set_boolean (to_value, string != NULL && g_strcmp0 (string, "") != 0);

  return TRUE;
}

static void
cc_vertical_row_init (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  priv->title_lines = 1;
  priv->subtitle_lines = 1;

  gtk_widget_init_template (GTK_WIDGET (self));

  g_object_bind_property_full (self, "title",
                               priv->title, "visible",
                               G_BINDING_SYNC_CREATE,
                               string_is_not_empty,
                               NULL, NULL, NULL);

  update_subtitle_visibility (self);

  g_signal_connect (self, "notify::parent", G_CALLBACK (parent_cb), NULL);
}

static void
cc_vertical_row_buildable_add_child (GtkBuildable *buildable,
                                     GtkBuilder   *builder,
                                     GObject      *child,
                                     const gchar  *type)
{
  CcVerticalRow *self = CC_VERTICAL_ROW (buildable);
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  if (!priv->header)
    parent_buildable_iface->add_child (buildable, builder, child, type);
  else if (type && strcmp (type, "prefix") == 0)
    cc_vertical_row_add_prefix (self, GTK_WIDGET (child));
  else if (type && strcmp (type, "content") == 0)
    cc_vertical_row_add_content (self, GTK_WIDGET (child));
  else if (!type && GTK_IS_WIDGET (child))
    {
      gtk_box_append (priv->suffixes, GTK_WIDGET (child));
      gtk_widget_set_visible (GTK_WIDGET (priv->suffixes), TRUE);
    }
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
cc_vertical_row_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = cc_vertical_row_buildable_add_child;
}

const gchar *
cc_vertical_row_get_subtitle (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  g_return_val_if_fail (CC_IS_VERTICAL_ROW (self), NULL);

  return gtk_label_get_text (priv->subtitle);
}

void
cc_vertical_row_set_subtitle (CcVerticalRow *self,
                              const gchar  *subtitle)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  g_return_if_fail (CC_IS_VERTICAL_ROW (self));

  if (g_strcmp0 (gtk_label_get_text (priv->subtitle), subtitle) == 0)
    return;

  gtk_label_set_text (priv->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (priv->subtitle),
                          subtitle != NULL && g_strcmp0 (subtitle, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE]);
}

const gchar *
cc_vertical_row_get_icon_name (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  g_return_val_if_fail (CC_IS_VERTICAL_ROW (self), NULL);

  return gtk_image_get_icon_name (priv->image);
}

void
cc_vertical_row_set_icon_name (CcVerticalRow *self,
                               const gchar   *icon_name)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  const gchar *old_icon_name;

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));

  old_icon_name = gtk_image_get_icon_name (priv->image);
  if (g_strcmp0 (old_icon_name, icon_name) == 0)
    return;

  gtk_image_set_from_icon_name (priv->image, icon_name);
  gtk_widget_set_visible (GTK_WIDGET (priv->image),
                          icon_name != NULL && g_strcmp0 (icon_name, "") != 0);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ICON_NAME]);
}

GtkWidget *
cc_vertical_row_get_activatable_widget (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  g_return_val_if_fail (CC_IS_VERTICAL_ROW (self), NULL);

  return priv->activatable_widget;
}

static void
activatable_widget_weak_notify (gpointer  data,
                                GObject  *where_the_object_was)
{
  CcVerticalRow *self = CC_VERTICAL_ROW (data);
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  priv->activatable_widget = NULL;

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVATABLE_WIDGET]);
}

void
cc_vertical_row_set_activatable_widget (CcVerticalRow *self,
                                        GtkWidget     *widget)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  if (priv->activatable_widget == widget)
    return;

  if (priv->activatable_widget)
    g_object_weak_unref (G_OBJECT (priv->activatable_widget),
                         activatable_widget_weak_notify,
                         self);

  priv->activatable_widget = widget;

  if (priv->activatable_widget != NULL) {
    g_object_weak_ref (G_OBJECT (priv->activatable_widget),
                       activatable_widget_weak_notify,
                       self);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), TRUE);
    gtk_accessible_update_relation (GTK_ACCESSIBLE (priv->activatable_widget),
                                GTK_ACCESSIBLE_RELATION_LABELLED_BY, priv->title, NULL,
                                GTK_ACCESSIBLE_RELATION_DESCRIBED_BY, priv->subtitle, NULL,
                                -1);

  }

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVATABLE_WIDGET]);
}

gboolean
cc_vertical_row_get_use_underline (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_val_if_fail (CC_IS_VERTICAL_ROW (self), FALSE);

  return priv->use_underline;
}

void
cc_vertical_row_set_use_underline (CcVerticalRow *self,
                                   gboolean       use_underline)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));

  use_underline = !!use_underline;

  if (priv->use_underline == use_underline)
    return;

  priv->use_underline = use_underline;
  adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (self), priv->use_underline);
  gtk_label_set_use_underline (priv->title, priv->use_underline);
  gtk_label_set_use_underline (priv->subtitle, priv->use_underline);
  gtk_label_set_mnemonic_widget (priv->title, GTK_WIDGET (self));
  gtk_label_set_mnemonic_widget (priv->subtitle, GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USE_UNDERLINE]);
}

gint
cc_vertical_row_get_title_lines (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_val_if_fail (CC_IS_VERTICAL_ROW (self), 0);

  return priv->title_lines;
}

void
cc_vertical_row_set_title_lines (CcVerticalRow *self,
                                 gint           title_lines)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));
  g_return_if_fail (title_lines >= 0);

  if (priv->title_lines == title_lines)
    return;

  priv->title_lines = title_lines;

  gtk_label_set_lines (priv->title, title_lines);
  gtk_label_set_ellipsize (priv->title, title_lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_TITLE_LINES]);
}

gint
cc_vertical_row_get_subtitle_lines (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_val_if_fail (CC_IS_VERTICAL_ROW (self), 0);

  return priv->subtitle_lines;
}

void
cc_vertical_row_set_subtitle_lines (CcVerticalRow *self,
                                    gint           subtitle_lines)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));
  g_return_if_fail (subtitle_lines >= 0);

  if (priv->subtitle_lines == subtitle_lines)
    return;

  priv->subtitle_lines = subtitle_lines;

  gtk_label_set_lines (priv->subtitle, subtitle_lines);
  gtk_label_set_ellipsize (priv->subtitle, subtitle_lines == 0 ? PANGO_ELLIPSIZE_NONE : PANGO_ELLIPSIZE_END);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUBTITLE_LINES]);
}

void
cc_vertical_row_add_prefix (CcVerticalRow *self,
                            GtkWidget     *widget)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (self));

  gtk_box_append (priv->prefixes, widget);
  gtk_widget_set_visible (GTK_WIDGET (priv->prefixes), TRUE);
}

void
cc_vertical_row_add_content (CcVerticalRow *self,
                             GtkWidget     *widget)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (self));

  /* HACK: the content box pushes the title too much to the top, so we
   * need to compensate this here.
   */
  gtk_widget_set_margin_top (GTK_WIDGET (priv->header), 12);

  gtk_box_append (priv->content_box, widget);
  gtk_widget_set_visible (GTK_WIDGET (priv->content_box), TRUE);
}

void
cc_vertical_row_activate (CcVerticalRow *self)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  g_return_if_fail (CC_IS_VERTICAL_ROW (self));

  if (priv->activatable_widget)
    gtk_widget_mnemonic_activate (priv->activatable_widget, FALSE);

  g_signal_emit (self, signals[SIGNAL_ACTIVATED], 0);
}

void
cc_vertical_row_remove (CcVerticalRow *self,
                        GtkWidget     *child)
{
  CcVerticalRowPrivate *priv = cc_vertical_row_get_instance_private (self);
  GtkWidget *parent;

  g_return_if_fail (CC_IS_VERTICAL_ROW (self));
  g_return_if_fail (GTK_IS_WIDGET (child));

  parent = gtk_widget_get_parent (child);

  if (parent == GTK_WIDGET (priv->prefixes))
    gtk_box_remove (priv->prefixes, child);
  else if (parent == GTK_WIDGET (priv->suffixes))
    gtk_box_remove (priv->suffixes, child);
  else if (parent == GTK_WIDGET (priv->content_box))
    gtk_box_remove (priv->content_box, child);
  else
    g_warning ("%p is not a child of %p", child, self);
}

