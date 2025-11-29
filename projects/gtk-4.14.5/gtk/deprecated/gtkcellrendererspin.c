/* GtkCellRendererSpin
 * Copyright (C) 2004 Lorenzo Gil Sanchez
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
 *
 * Authors: Lorenzo Gil Sanchez    <lgs@sicem.biz>
 *          Carlos Garnacho Parro  <carlosg@gnome.org>
 */

#include "config.h"

#include "gtkcellrendererspin.h"

#include "gtkadjustment.h"
#include "gtkprivate.h"
#include "gtkspinbutton.h"
#include "gtkentry.h"
#include "gtkeventcontrollerkey.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellRendererSpin:
 *
 * Renders a spin button in a cell
 *
 * `GtkCellRendererSpin` renders text in a cell like `GtkCellRendererText` from
 * which it is derived. But while `GtkCellRendererText` offers a simple entry to
 * edit the text, `GtkCellRendererSpin` offers a `GtkSpinButton` widget. Of course,
 * that means that the text has to be parseable as a floating point number.
 *
 * The range of the spinbutton is taken from the adjustment property of the
 * cell renderer, which can be set explicitly or mapped to a column in the
 * tree model, like all properties of cell renders. `GtkCellRendererSpin`
 * also has properties for the `GtkCellRendererSpin:climb-rate` and the number
 * of `GtkCellRendererSpin:digits` to display. Other `GtkSpinButton` properties
 * can be set in a handler for the `GtkCellRenderer::editing-started` signal.
 *
 * Deprecated: 4.10: List views use widgets to display their contents.
 *   You should use [class@Gtk.SpinButton] instead
 */

typedef struct _GtkCellRendererSpinClass   GtkCellRendererSpinClass;
typedef struct _GtkCellRendererSpinPrivate GtkCellRendererSpinPrivate;

struct _GtkCellRendererSpin
{
  GtkCellRendererText parent;
};

struct _GtkCellRendererSpinClass
{
  GtkCellRendererTextClass parent;
};

struct _GtkCellRendererSpinPrivate
{
  GtkWidget *spin;
  GtkAdjustment *adjustment;
  double climb_rate;
  guint   digits;
};

static void gtk_cell_renderer_spin_finalize   (GObject                  *object);

static void gtk_cell_renderer_spin_get_property (GObject      *object,
						 guint         prop_id,
						 GValue       *value,
						 GParamSpec   *spec);
static void gtk_cell_renderer_spin_set_property (GObject      *object,
						 guint         prop_id,
						 const GValue *value,
						 GParamSpec   *spec);

static gboolean gtk_cell_renderer_spin_key_pressed (GtkEventControllerKey *controller,
                                                    guint                  keyval,
                                                    guint                  keycode,
                                                    GdkModifierType        state,
                                                    GtkWidget             *widget);

static GtkCellEditable * gtk_cell_renderer_spin_start_editing (GtkCellRenderer     *cell,
							       GdkEvent            *event,
							       GtkWidget           *widget,
							       const char          *path,
							       const GdkRectangle  *background_area,
							       const GdkRectangle  *cell_area,
							       GtkCellRendererState flags);
enum {
  PROP_0,
  PROP_ADJUSTMENT,
  PROP_CLIMB_RATE,
  PROP_DIGITS
};

#define GTK_CELL_RENDERER_SPIN_PATH "gtk-cell-renderer-spin-path"

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererSpin, gtk_cell_renderer_spin, GTK_TYPE_CELL_RENDERER_TEXT)


static void
gtk_cell_renderer_spin_class_init (GtkCellRendererSpinClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize     = gtk_cell_renderer_spin_finalize;
  object_class->get_property = gtk_cell_renderer_spin_get_property;
  object_class->set_property = gtk_cell_renderer_spin_set_property;

  cell_class->start_editing  = gtk_cell_renderer_spin_start_editing;

  /**
   * GtkCellRendererSpin:adjustment:
   *
   * The adjustment that holds the value of the spinbutton.
   * This must be non-%NULL for the cell renderer to be editable.
   */
  g_object_class_install_property (object_class,
				   PROP_ADJUSTMENT,
				   g_param_spec_object ("adjustment", NULL, NULL,
							GTK_TYPE_ADJUSTMENT,
							GTK_PARAM_READWRITE));


  /**
   * GtkCellRendererSpin:climb-rate:
   *
   * The acceleration rate when you hold down a button.
   */
  g_object_class_install_property (object_class,
				   PROP_CLIMB_RATE,
				   g_param_spec_double ("climb-rate", NULL, NULL,
							0.0, G_MAXDOUBLE, 0.0,
							GTK_PARAM_READWRITE));
  /**
   * GtkCellRendererSpin:digits:
   *
   * The number of decimal places to display.
   */
  g_object_class_install_property (object_class,
				   PROP_DIGITS,
				   g_param_spec_uint ("digits", NULL, NULL,
						      0, 20, 0,
						      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
gtk_cell_renderer_spin_init (GtkCellRendererSpin *self)
{
  GtkCellRendererSpinPrivate *priv = gtk_cell_renderer_spin_get_instance_private (self);

  priv->adjustment = NULL;
  priv->climb_rate = 0.0;
  priv->digits = 0;
}

static void
gtk_cell_renderer_spin_finalize (GObject *object)
{
  GtkCellRendererSpinPrivate *priv = gtk_cell_renderer_spin_get_instance_private (GTK_CELL_RENDERER_SPIN (object));

  g_clear_object (&priv->adjustment);
  g_clear_object (&priv->spin);

  G_OBJECT_CLASS (gtk_cell_renderer_spin_parent_class)->finalize (object);
}

static void
gtk_cell_renderer_spin_get_property (GObject      *object,
				     guint         prop_id,
				     GValue       *value,
				     GParamSpec   *pspec)
{
  GtkCellRendererSpinPrivate *priv = gtk_cell_renderer_spin_get_instance_private (GTK_CELL_RENDERER_SPIN (object));

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, priv->adjustment);
      break;
    case PROP_CLIMB_RATE:
      g_value_set_double (value, priv->climb_rate);
      break;
    case PROP_DIGITS:
      g_value_set_uint (value, priv->digits);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_spin_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  GtkCellRendererSpinPrivate *priv = gtk_cell_renderer_spin_get_instance_private (GTK_CELL_RENDERER_SPIN (object));
  GObject *obj;

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      obj = g_value_get_object (value);

      if (priv->adjustment)
	{
	  g_object_unref (priv->adjustment);
	  priv->adjustment = NULL;
	}

      if (obj)
	priv->adjustment = GTK_ADJUSTMENT (g_object_ref_sink (obj));

      break;
    case PROP_CLIMB_RATE:
      priv->climb_rate = g_value_get_double (value);
      break;
    case PROP_DIGITS:
      if (priv->digits != g_value_get_uint (value))
        {
          priv->digits = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_renderer_spin_focus_changed (GtkWidget  *widget,
                                      GParamSpec *pspec,
                                      gpointer    data)
{
  const char *path;
  const char *new_text;
  gboolean canceled;

  if (gtk_widget_has_focus (widget))
    return;

  g_object_get (widget, "editing-canceled", &canceled, NULL);

  g_signal_handlers_disconnect_by_func (widget,
					gtk_cell_renderer_spin_focus_changed,
					data);

  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (data), canceled);

  if (canceled)
    return;

  path = g_object_get_data (G_OBJECT (widget), GTK_CELL_RENDERER_SPIN_PATH);
  new_text = gtk_editable_get_text (GTK_EDITABLE (widget));
  g_signal_emit_by_name (data, "edited", path, new_text);
}

static gboolean
gtk_cell_renderer_spin_key_pressed (GtkEventControllerKey *controller,
                                    guint                  keyval,
                                    guint                  keycode,
                                    GdkModifierType        state,
                                    GtkWidget             *widget)
{
  if (state == 0)
    {
      if (keyval == GDK_KEY_Up)
	{
	  gtk_spin_button_spin (GTK_SPIN_BUTTON (widget), GTK_SPIN_STEP_FORWARD, 1);
	  return TRUE;
	}
      else if (keyval == GDK_KEY_Down)
	{
	  gtk_spin_button_spin (GTK_SPIN_BUTTON (widget), GTK_SPIN_STEP_BACKWARD, 1);
	  return TRUE;
	}
    }

  return FALSE;
}

static void
gtk_cell_renderer_spin_editing_done (GtkSpinButton       *spin,
                                     GtkCellRendererSpin *cell)
{
  GtkCellRendererSpinPrivate *priv = gtk_cell_renderer_spin_get_instance_private (GTK_CELL_RENDERER_SPIN (cell));
  gboolean canceled;
  const char *path;
  const char *new_text;

  g_clear_object (&priv->spin);

  g_object_get (spin, "editing-canceled", &canceled, NULL);
  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (cell), canceled);

  if (canceled)
    return;

  path = g_object_get_data (G_OBJECT (spin), GTK_CELL_RENDERER_SPIN_PATH);
  new_text = gtk_editable_get_text (GTK_EDITABLE (spin));
  g_signal_emit_by_name (cell, "edited", path, new_text);
}

static GtkCellEditable *
gtk_cell_renderer_spin_start_editing (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const char           *path,
				      const GdkRectangle   *background_area,
				      const GdkRectangle   *cell_area,
				      GtkCellRendererState  flags)
{
  GtkCellRendererSpinPrivate *priv = gtk_cell_renderer_spin_get_instance_private (GTK_CELL_RENDERER_SPIN (cell));
  GtkCellRendererText *cell_text = GTK_CELL_RENDERER_TEXT (cell);
  GtkEventController *key_controller;
  gboolean editable;
  char *text;

  g_object_get (cell_text, "editable", &editable, NULL);
  if (!editable)
    return NULL;

  if (!priv->adjustment)
    return NULL;

  priv->spin = gtk_spin_button_new (priv->adjustment, priv->climb_rate, priv->digits);
  g_object_ref_sink (priv->spin);

  g_object_get (cell_text, "text", &text, NULL);
  if (text)
    {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->spin), g_strtod (text, NULL));
      g_free (text);
    }

  key_controller = gtk_event_controller_key_new ();
  g_signal_connect (key_controller, "key-pressed",
                    G_CALLBACK (gtk_cell_renderer_spin_key_pressed), priv->spin);
  gtk_widget_add_controller (priv->spin, key_controller);

  g_object_set_data_full (G_OBJECT (priv->spin), GTK_CELL_RENDERER_SPIN_PATH,
			  g_strdup (path), g_free);

  g_signal_connect (priv->spin, "editing-done",
                    G_CALLBACK (gtk_cell_renderer_spin_editing_done), cell);

  g_signal_connect (priv->spin, "notify::has-focus",
		    G_CALLBACK (gtk_cell_renderer_spin_focus_changed), cell);

  return GTK_CELL_EDITABLE (priv->spin);
}

/**
 * gtk_cell_renderer_spin_new:
 *
 * Creates a new `GtkCellRendererSpin`.
 *
 * Returns: a new `GtkCellRendererSpin`
 *
 * Deprecated: 4.10
 */
GtkCellRenderer *
gtk_cell_renderer_spin_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_SPIN, NULL);
}
