/* gtkshortcutsshortcut.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutsshortcutprivate.h"

#include "gtkimage.h"
#include "gtkbox.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkshortcutlabel.h"
#include "gtkshortcutswindowprivate.h"
#include "gtksizegroup.h"
#include "gtktypebuiltins.h"

/**
 * GtkShortcutsShortcut:
 *
 * A `GtkShortcutsShortcut` represents a single keyboard shortcut or gesture
 * with a short text.
 *
 * This widget is only meant to be used with `GtkShortcutsWindow`.
 */

struct _GtkShortcutsShortcut
{
  GtkWidget         parent_instance;

  GtkBox           *box;
  GtkImage         *image;
  GtkShortcutLabel *accelerator;
  GtkLabel         *title;
  GtkLabel         *subtitle;
  GtkLabel         *title_box;

  GtkSizeGroup *accel_size_group;
  GtkSizeGroup *title_size_group;

  gboolean subtitle_set;
  gboolean icon_set;
  GtkTextDirection direction;
  char *action_name;
  GtkShortcutType  shortcut_type;
};

struct _GtkShortcutsShortcutClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsShortcut, gtk_shortcuts_shortcut, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_ACCELERATOR,
  PROP_ICON,
  PROP_ICON_SET,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_SUBTITLE_SET,
  PROP_ACCEL_SIZE_GROUP,
  PROP_TITLE_SIZE_GROUP,
  PROP_DIRECTION,
  PROP_SHORTCUT_TYPE,
  PROP_ACTION_NAME,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP];

static void
gtk_shortcuts_shortcut_set_accelerator (GtkShortcutsShortcut *self,
                                        const char           *accelerator)
{
  gtk_shortcut_label_set_accelerator (self->accelerator, accelerator);
}

static void
gtk_shortcuts_shortcut_set_accel_size_group (GtkShortcutsShortcut *self,
                                             GtkSizeGroup         *group)
{
  if (self->accel_size_group)
    {
      gtk_size_group_remove_widget (self->accel_size_group, GTK_WIDGET (self->accelerator));
      gtk_size_group_remove_widget (self->accel_size_group, GTK_WIDGET (self->image));
    }

  if (group)
    {
      gtk_size_group_add_widget (group, GTK_WIDGET (self->accelerator));
      gtk_size_group_add_widget (group, GTK_WIDGET (self->image));
    }

  g_set_object (&self->accel_size_group, group);
}

static void
gtk_shortcuts_shortcut_set_title_size_group (GtkShortcutsShortcut *self,
                                             GtkSizeGroup         *group)
{
  if (self->title_size_group)
    gtk_size_group_remove_widget (self->title_size_group, GTK_WIDGET (self->title_box));
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (self->title_box));

  g_set_object (&self->title_size_group, group);
}

static void
update_subtitle_from_type (GtkShortcutsShortcut *self)
{
  const char *subtitle;

  if (self->subtitle_set)
    return;

  switch (self->shortcut_type)
    {
    case GTK_SHORTCUT_ACCELERATOR:
    case GTK_SHORTCUT_GESTURE:
      subtitle = NULL;
      break;

    case GTK_SHORTCUT_GESTURE_PINCH:
      subtitle = _("Two finger pinch");
      break;

    case GTK_SHORTCUT_GESTURE_STRETCH:
      subtitle = _("Two finger stretch");
      break;

    case GTK_SHORTCUT_GESTURE_ROTATE_CLOCKWISE:
      subtitle = _("Rotate clockwise");
      break;

    case GTK_SHORTCUT_GESTURE_ROTATE_COUNTERCLOCKWISE:
      subtitle = _("Rotate counterclockwise");
      break;

    case GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_LEFT:
      subtitle = _("Two finger swipe left");
      break;

    case GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_RIGHT:
      subtitle = _("Two finger swipe right");
      break;

    case GTK_SHORTCUT_GESTURE_SWIPE_LEFT:
      subtitle = _("Swipe left");
      break;

    case GTK_SHORTCUT_GESTURE_SWIPE_RIGHT:
      subtitle = _("Swipe right");
      break;

    default:
      subtitle = NULL;
      break;
    }

  gtk_label_set_label (self->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), subtitle != NULL);
  g_object_notify (G_OBJECT (self), "subtitle");
}

static void
gtk_shortcuts_shortcut_set_subtitle_set (GtkShortcutsShortcut *self,
                                         gboolean              subtitle_set)
{
  if (self->subtitle_set != subtitle_set)
    {
      self->subtitle_set = subtitle_set;
      g_object_notify (G_OBJECT (self), "subtitle-set");
    }
  update_subtitle_from_type (self);
}

static void
gtk_shortcuts_shortcut_set_subtitle (GtkShortcutsShortcut *self,
                                     const char           *subtitle)
{
  gtk_label_set_label (self->subtitle, subtitle);
  gtk_widget_set_visible (GTK_WIDGET (self->subtitle), subtitle && subtitle[0]);
  gtk_shortcuts_shortcut_set_subtitle_set (self, subtitle && subtitle[0]);

  g_object_notify (G_OBJECT (self), "subtitle");
}

static void
update_icon_from_type (GtkShortcutsShortcut *self)
{
  GIcon *icon;

  if (self->icon_set)
    return;

  switch (self->shortcut_type)
    {
    case GTK_SHORTCUT_GESTURE_PINCH:
      icon = g_themed_icon_new ("gesture-pinch-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_STRETCH:
      icon = g_themed_icon_new ("gesture-stretch-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_ROTATE_CLOCKWISE:
      icon = g_themed_icon_new ("gesture-rotate-clockwise-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_ROTATE_COUNTERCLOCKWISE:
      icon = g_themed_icon_new ("gesture-rotate-anticlockwise-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_LEFT:
      icon = g_themed_icon_new ("gesture-two-finger-swipe-left-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_TWO_FINGER_SWIPE_RIGHT:
      icon = g_themed_icon_new ("gesture-two-finger-swipe-right-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_SWIPE_LEFT:
      icon = g_themed_icon_new ("gesture-swipe-left-symbolic");
      break;

    case GTK_SHORTCUT_GESTURE_SWIPE_RIGHT:
      icon = g_themed_icon_new ("gesture-swipe-right-symbolic");
      break;

    case GTK_SHORTCUT_ACCELERATOR:
    case GTK_SHORTCUT_GESTURE:
    default:
      icon = NULL;
      break;
    }

  if (icon)
    {
      gtk_image_set_from_gicon (self->image, icon);
      gtk_image_set_pixel_size (self->image, 64);
      g_object_unref (icon);
    }
}

static void
gtk_shortcuts_shortcut_set_icon_set (GtkShortcutsShortcut *self,
                                     gboolean              icon_set)
{
  if (self->icon_set != icon_set)
    {
      self->icon_set = icon_set;
      g_object_notify (G_OBJECT (self), "icon-set");
    }
  update_icon_from_type (self);
}

static void
gtk_shortcuts_shortcut_set_icon (GtkShortcutsShortcut *self,
                                 GIcon                *gicon)
{
  gtk_image_set_from_gicon (self->image, gicon);
  gtk_shortcuts_shortcut_set_icon_set (self, gicon != NULL);
  g_object_notify (G_OBJECT (self), "icon");
}

static void
update_visible_from_direction (GtkShortcutsShortcut *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self),
                          self->direction == GTK_TEXT_DIR_NONE ||
                          self->direction == gtk_widget_get_direction (GTK_WIDGET (self)));
}

static void
gtk_shortcuts_shortcut_set_direction (GtkShortcutsShortcut *self,
                                      GtkTextDirection      direction)
{
  if (self->direction == direction)
    return;

  self->direction = direction;

  update_visible_from_direction (self);

  g_object_notify (G_OBJECT (self), "direction");
}

static void
gtk_shortcuts_shortcut_direction_changed (GtkWidget        *widget,
                                          GtkTextDirection  previous_dir)
{
  update_visible_from_direction (GTK_SHORTCUTS_SHORTCUT (widget));

  GTK_WIDGET_CLASS (gtk_shortcuts_shortcut_parent_class)->direction_changed (widget, previous_dir);
}

static void
gtk_shortcuts_shortcut_set_type (GtkShortcutsShortcut *self,
                                 GtkShortcutType       type)
{
  if (self->shortcut_type == type)
    return;

  self->shortcut_type = type;

  update_subtitle_from_type (self);
  update_icon_from_type (self);

  gtk_widget_set_visible (GTK_WIDGET (self->accelerator), type == GTK_SHORTCUT_ACCELERATOR);
  gtk_widget_set_visible (GTK_WIDGET (self->image), type != GTK_SHORTCUT_ACCELERATOR);


  g_object_notify (G_OBJECT (self), "shortcut-type");
}

static void
gtk_shortcuts_shortcut_set_action_name (GtkShortcutsShortcut *self,
                                        const char           *action_name)
{
  g_free (self->action_name);
  self->action_name = g_strdup (action_name);

  g_object_notify (G_OBJECT (self), "action-name");
}

static void
gtk_shortcuts_shortcut_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkShortcutsShortcut *self = GTK_SHORTCUTS_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (self->title));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (self->subtitle));
      break;

    case PROP_SUBTITLE_SET:
      g_value_set_boolean (value, self->subtitle_set);
      break;

    case PROP_ACCELERATOR:
      g_value_set_string (value, gtk_shortcut_label_get_accelerator (self->accelerator));
      break;

    case PROP_ICON:
      g_value_set_object (value, gtk_image_get_gicon (self->image));
      break;

    case PROP_ICON_SET:
      g_value_set_boolean (value, self->icon_set);
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, self->direction);
      break;

    case PROP_SHORTCUT_TYPE:
      g_value_set_enum (value, self->shortcut_type);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, self->action_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcuts_shortcut_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkShortcutsShortcut *self = GTK_SHORTCUTS_SHORTCUT (object);

  switch (prop_id)
    {
    case PROP_ACCELERATOR:
      gtk_shortcuts_shortcut_set_accelerator (self, g_value_get_string (value));
      break;

    case PROP_ICON:
      gtk_shortcuts_shortcut_set_icon (self, g_value_get_object (value));
      break;

    case PROP_ICON_SET:
      gtk_shortcuts_shortcut_set_icon_set (self, g_value_get_boolean (value));
      break;

    case PROP_ACCEL_SIZE_GROUP:
      gtk_shortcuts_shortcut_set_accel_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_TITLE:
      gtk_label_set_label (self->title, g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_shortcuts_shortcut_set_subtitle (self, g_value_get_string (value));
      break;

    case PROP_SUBTITLE_SET:
      gtk_shortcuts_shortcut_set_subtitle_set (self, g_value_get_boolean (value));
      break;

    case PROP_TITLE_SIZE_GROUP:
      gtk_shortcuts_shortcut_set_title_size_group (self, GTK_SIZE_GROUP (g_value_get_object (value)));
      break;

    case PROP_DIRECTION:
      gtk_shortcuts_shortcut_set_direction (self, g_value_get_enum (value));
      break;

    case PROP_SHORTCUT_TYPE:
      gtk_shortcuts_shortcut_set_type (self, g_value_get_enum (value));
      break;

    case PROP_ACTION_NAME:
      gtk_shortcuts_shortcut_set_action_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_shortcuts_shortcut_finalize (GObject *object)
{
  GtkShortcutsShortcut *self = GTK_SHORTCUTS_SHORTCUT (object);

  g_clear_object (&self->accel_size_group);
  g_clear_object (&self->title_size_group);
  g_free (self->action_name);
  gtk_widget_unparent (GTK_WIDGET (self->box));

  G_OBJECT_CLASS (gtk_shortcuts_shortcut_parent_class)->finalize (object);
}

void
gtk_shortcuts_shortcut_update_accel (GtkShortcutsShortcut *self,
                                     GtkWindow            *window)
{
  GtkApplication *app;
  char **accels;
  char *str;

  if (self->action_name == NULL)
    return;

  app = gtk_window_get_application (window);
  if (app == NULL)
    return;

  accels = gtk_application_get_accels_for_action (app, self->action_name);
  str = g_strjoinv (" ", accels);

  gtk_shortcuts_shortcut_set_accelerator (self, str);

  g_free (str);
  g_strfreev (accels);
}

static void
gtk_shortcuts_shortcut_measure (GtkWidget      *widget,
                                GtkOrientation  orientation,
                                int            for_size,
                                int           *minimum,
                                int           *natural,
                                int           *minimum_baseline,
                                int           *natural_baseline)
{
  gtk_widget_measure (GTK_WIDGET (GTK_SHORTCUTS_SHORTCUT (widget)->box),
                      orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_shortcuts_shortcut_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  gtk_widget_snapshot_child (widget, GTK_WIDGET (GTK_SHORTCUTS_SHORTCUT (widget)->box), snapshot);
}

static void
gtk_shortcuts_shortcut_size_allocate (GtkWidget *widget,
                                      int        width,
                                      int        height,
                                      int        baseline)
{
  GTK_WIDGET_CLASS (gtk_shortcuts_shortcut_parent_class)->size_allocate (widget, width, height, baseline);

  gtk_widget_size_allocate (GTK_WIDGET (GTK_SHORTCUTS_SHORTCUT (widget)->box),
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            }, -1);
}

static void
gtk_shortcuts_shortcut_class_init (GtkShortcutsShortcutClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_shortcuts_shortcut_finalize;
  object_class->get_property = gtk_shortcuts_shortcut_get_property;
  object_class->set_property = gtk_shortcuts_shortcut_set_property;

  widget_class->direction_changed = gtk_shortcuts_shortcut_direction_changed;
  widget_class->measure = gtk_shortcuts_shortcut_measure;
  widget_class->snapshot = gtk_shortcuts_shortcut_snapshot;
  widget_class->size_allocate = gtk_shortcuts_shortcut_size_allocate;

  /**
   * GtkShortcutsShortcut:accelerator:
   *
   * The accelerator(s) represented by this object.
   *
   * This property is used if [property@Gtk.ShortcutsShortcut:shortcut-type]
   * is set to %GTK_SHORTCUT_ACCELERATOR.
   *
   * The syntax of this property is (an extension of) the syntax understood
   * by [func@Gtk.accelerator_parse]. Multiple accelerators can be specified
   * by separating them with a space, but keep in mind that the available width
   * is limited.
   *
   * It is also possible to specify ranges of shortcuts, using `...` between
   * the keys. Sequences of keys can be specified using a `+` or `&` between
   * the keys.
   *
   * Examples:
   *
   * - A single shortcut: `<ctl><alt>delete`
   * - Two alternative shortcuts: `<shift>a Home`
   * - A range of shortcuts: `<alt>1...<alt>9`
   * - Several keys pressed together: `Control_L&Control_R`
   * - A sequence of shortcuts or keys: `<ctl>c+<ctl>x`
   *
   * Use "+" instead of "&" when the keys may (or have to be) pressed
   * sequentially (e.g use "t+t" for 'press the t key twice').
   *
   * Note that `<`, `>` and `&` need to be escaped as `&lt;`, `&gt`; and `&amp`; when used
   * in .ui files.
   */
  properties[PROP_ACCELERATOR] =
    g_param_spec_string ("accelerator", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:icon:
   *
   * An icon to represent the shortcut or gesture.
   *
   * This property is used if [property@Gtk.ShortcutsShortcut:shortcut-type]
   * is set to %GTK_SHORTCUT_GESTURE.
   *
   * For the other predefined gesture types, GTK provides an icon on its own.
   */
  properties[PROP_ICON] =
    g_param_spec_object ("icon", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:icon-set:
   *
   * %TRUE if an icon has been set.
   */
  properties[PROP_ICON_SET] =
    g_param_spec_boolean ("icon-set", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:title:
   *
   * The textual description for the shortcut or gesture represented by
   * this object.
   *
   * This should be a short string that can fit in a single line.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:subtitle:
   *
   * The subtitle for the shortcut or gesture.
   *
   * This is typically used for gestures and should be a short, one-line
   * text that describes the gesture itself. For the predefined gesture
   * types, GTK provides a subtitle on its own.
   */
  properties[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:subtitle-set:
   *
   * %TRUE if a subtitle has been set.
   */
  properties[PROP_SUBTITLE_SET] =
    g_param_spec_boolean ("subtitle-set", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:accel-size-group:
   *
   * The size group for the accelerator portion of this shortcut.
   *
   * This is used internally by GTK, and must not be modified by applications.
   */
  properties[PROP_ACCEL_SIZE_GROUP] =
    g_param_spec_object ("accel-size-group", NULL, NULL,
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:title-size-group:
   *
   * The size group for the textual portion of this shortcut.
   *
   * This is used internally by GTK, and must not be modified by applications.
   */
  properties[PROP_TITLE_SIZE_GROUP] =
    g_param_spec_object ("title-size-group", NULL, NULL,
                         GTK_TYPE_SIZE_GROUP,
                         (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GtkShortcutsShortcut:direction:
   *
   * The text direction for which this shortcut is active.
   *
   * If the shortcut is used regardless of the text direction,
   * set this property to %GTK_TEXT_DIR_NONE.
   */
  properties[PROP_DIRECTION] =
    g_param_spec_enum ("direction", NULL, NULL,
                       GTK_TYPE_TEXT_DIRECTION,
                       GTK_TEXT_DIR_NONE,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkShortcutsShortcut:shortcut-type:
   *
   * The type of shortcut that is represented.
   */
  properties[PROP_SHORTCUT_TYPE] =
    g_param_spec_enum ("shortcut-type", NULL, NULL,
                       GTK_TYPE_SHORTCUT_TYPE,
                       GTK_SHORTCUT_ACCELERATOR,
                       (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkShortcutsShortcut:action-name:
   *
   * A detailed action name.
   *
   * If this is set for a shortcut of type %GTK_SHORTCUT_ACCELERATOR,
   * then GTK will use the accelerators that are associated with the
   * action via [method@Gtk.Application.set_accels_for_action], and
   * setting [property@Gtk.ShortcutsShortcut:accelerator] is not necessary.
   */
  properties[PROP_ACTION_NAME] =
    g_param_spec_string ("action-name", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
  gtk_widget_class_set_css_name (widget_class, I_("shortcut"));
  /* It is semantically a label, but the label role has such specific meaning in Orca
   * as to be unusable in this context.
   */
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LABEL);
}

static void
gtk_shortcuts_shortcut_init (GtkShortcutsShortcut *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);


  self->box = g_object_new (GTK_TYPE_BOX,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            "spacing", 12,
                            NULL);
  gtk_widget_set_parent (GTK_WIDGET (self->box), GTK_WIDGET (self));

  self->direction = GTK_TEXT_DIR_NONE;
  self->shortcut_type = GTK_SHORTCUT_ACCELERATOR;

  self->image = g_object_new (GTK_TYPE_IMAGE,
                              "visible", FALSE,
                              "valign", GTK_ALIGN_CENTER,
                              "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                              NULL);
  gtk_box_append (GTK_BOX (self->box), GTK_WIDGET (self->image));

  self->accelerator = g_object_new (GTK_TYPE_SHORTCUT_LABEL,
                                    "visible", TRUE,
                                    "valign", GTK_ALIGN_CENTER,
                                    NULL);
  gtk_box_append (GTK_BOX (self->box), GTK_WIDGET (self->accelerator));

  self->title_box = g_object_new (GTK_TYPE_BOX,
                                  "visible", TRUE,
                                  "valign", GTK_ALIGN_CENTER,
                                  "hexpand", TRUE,
                                  "orientation", GTK_ORIENTATION_VERTICAL,
                                  NULL);
  gtk_box_append (GTK_BOX (self->box), GTK_WIDGET (self->title_box));

  self->title = g_object_new (GTK_TYPE_LABEL,
                              "visible", TRUE,
                              "xalign", 0.0f,
                              NULL);
  gtk_box_append (GTK_BOX (self->title_box), GTK_WIDGET (self->title));

  self->subtitle = g_object_new (GTK_TYPE_LABEL,
                                 "visible", FALSE,
                                 "xalign", 0.0f,
                                 NULL);
  gtk_widget_add_css_class (GTK_WIDGET (self->subtitle), "dim-label");
  gtk_box_append (GTK_BOX (self->title_box), GTK_WIDGET (self->subtitle));

#if 0
  gtk_accessible_update_relation (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->accelerator, self->title, NULL,
                                  -1);
#endif

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, "",
                                  -1);
}
