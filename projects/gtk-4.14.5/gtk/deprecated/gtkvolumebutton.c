/* GTK - The GIMP Toolkit
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
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

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkvolumebutton.h"

#include "gtkadjustment.h"
#include <glib/gi18n-lib.h>
#include "gtktooltip.h"
#include "gtkprivate.h"


G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkVolumeButton:
 *
 * `GtkVolumeButton` is a `GtkScaleButton` subclass tailored for
 * volume control.
 *
 * ![An example GtkVolumeButton](volumebutton.png)
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */

typedef struct _GtkVolumeButtonClass  GtkVolumeButtonClass;

struct _GtkVolumeButtonClass
{
  GtkScaleButtonClass parent_class;
};

#define EPSILON (1e-10)

static const char * const icons[] =
{
  "audio-volume-muted",
  "audio-volume-high",
  "audio-volume-low",
  "audio-volume-medium",
  NULL
};

static const char * const icons_symbolic[] =
{
  "audio-volume-muted-symbolic",
  "audio-volume-high-symbolic",
  "audio-volume-low-symbolic",
  "audio-volume-medium-symbolic",
  NULL
};

enum
{
  PROP_0,
  PROP_SYMBOLIC
};

static gboolean cb_query_tooltip (GtkWidget       *button,
                                  int              x,
                                  int              y,
                                  gboolean         keyboard_mode,
                                  GtkTooltip      *tooltip,
                                  gpointer         user_data);
static void     cb_value_changed (GtkVolumeButton *button,
                                  double           value,
                                  gpointer         user_data);

G_DEFINE_TYPE (GtkVolumeButton, gtk_volume_button, GTK_TYPE_SCALE_BUTTON)

static gboolean
get_symbolic (GtkScaleButton *button)
{
  char **icon_list;
  gboolean ret;

  g_object_get (button, "icons", &icon_list, NULL);
  if (icon_list != NULL &&
      icon_list[0] != NULL &&
      g_str_equal (icon_list[0], icons_symbolic[0]))
    ret = TRUE;
  else
    ret = FALSE;
  g_strfreev (icon_list);

  return ret;
}

static void
gtk_volume_button_set_property (GObject       *object,
                                guint          prop_id,
                                const GValue  *value,
                                GParamSpec    *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_SYMBOLIC:
      if (get_symbolic (button) != g_value_get_boolean (value))
        {
          if (g_value_get_boolean (value))
            gtk_scale_button_set_icons (button, (const char **) icons_symbolic);
          else
            gtk_scale_button_set_icons (button, (const char **) icons);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_volume_button_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  switch (prop_id)
    {
    case PROP_SYMBOLIC:
      g_value_set_boolean (value, get_symbolic (GTK_SCALE_BUTTON (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_volume_button_class_init (GtkVolumeButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->set_property = gtk_volume_button_set_property;
  gobject_class->get_property = gtk_volume_button_get_property;

  /**
   * GtkVolumeButton:use-symbolic:
   *
   * Whether to use symbolic icons as the icons.
   *
   * Note that if the symbolic icons are not available in your installed
   * theme, then the normal (potentially colorful) icons will be used.
   *
   * Deprecated: 4.10: This widget will be removed in GTK 5
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SYMBOLIC,
                                   g_param_spec_boolean ("use-symbolic", NULL, NULL,
                                                         TRUE,
                                                         GTK_PARAM_READWRITE|G_PARAM_CONSTRUCT|G_PARAM_EXPLICIT_NOTIFY));

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkvolumebutton.ui");
  gtk_widget_class_bind_template_callback (widget_class, cb_query_tooltip);
  gtk_widget_class_bind_template_callback (widget_class, cb_value_changed);
}

static void
gtk_volume_button_init (GtkVolumeButton *button)
{
  GtkWidget *widget = GTK_WIDGET (button);

  gtk_widget_init_template (widget);
}

/**
 * gtk_volume_button_new:
 *
 * Creates a `GtkVolumeButton`.
 *
 * The button has a range between 0.0 and 1.0, with a stepping of 0.02.
 * Volume values can be obtained and modified using the functions from
 * [class@Gtk.ScaleButton].
 *
 * Returns: a new `GtkVolumeButton`
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkWidget *
gtk_volume_button_new (void)
{
  GObject *button;
  button = g_object_new (GTK_TYPE_VOLUME_BUTTON, NULL);
  return GTK_WIDGET (button);
}

static gboolean
cb_query_tooltip (GtkWidget  *button,
                  int         x,
                  int         y,
                  gboolean    keyboard_mode,
                  GtkTooltip *tooltip,
                  gpointer    user_data)
{
  GtkScaleButton *scale_button = GTK_SCALE_BUTTON (button);
  GtkAdjustment *adjustment;
  double val;
  char *str;

  adjustment = gtk_scale_button_get_adjustment (scale_button);
  val = gtk_scale_button_get_value (scale_button);

  if (val < (gtk_adjustment_get_lower (adjustment) + EPSILON))
    {
      str = g_strdup (_("Muted"));
    }
  else if (val >= (gtk_adjustment_get_upper (adjustment) - EPSILON))
    {
      str = g_strdup (_("Full Volume"));
    }
  else
    {
      int percent;

      percent = (int) (100. * val / (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) + .5);

      /* Translators: this is the percentage of the current volume,
       * as used in the tooltip, eg. "49 %".
       * Translate the "%d" to "%Id" if you want to use localised digits,
       * or otherwise translate the "%d" to "%d".
       */
      str = g_strdup_printf (C_("volume percentage", "%d %%"), percent);
    }

  gtk_tooltip_set_text (tooltip, str);
  g_free (str);

  return TRUE;
}

static void
cb_value_changed (GtkVolumeButton *button, double value, gpointer user_data)
{
  gtk_widget_trigger_tooltip_query (GTK_WIDGET (button));
}
