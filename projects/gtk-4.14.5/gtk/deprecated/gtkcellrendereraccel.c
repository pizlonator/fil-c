/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcellrendereraccel.h"

#include <glib/gi18n-lib.h>
#include "gtkaccelgroup.h"
#include "gtkmarshalers.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtknative.h"
#include "gtkbinlayout.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkCellRendererAccel:
 *
 * Renders a keyboard accelerator in a cell
 *
 * `GtkCellRendererAccel` displays a keyboard accelerator (i.e. a key
 * combination like `Control + a`). If the cell renderer is editable,
 * the accelerator can be changed by simply typing the new combination.
 *
 * Deprecated: 4.10: Applications editing keyboard accelerators should
 *   provide their own implementation according to platform design
 *   guidelines
 */


static void gtk_cell_renderer_accel_get_property (GObject         *object,
                                                  guint            param_id,
                                                  GValue          *value,
                                                  GParamSpec      *pspec);
static void gtk_cell_renderer_accel_set_property (GObject         *object,
                                                  guint            param_id,
                                                  const GValue    *value,
                                                  GParamSpec      *pspec);
static void gtk_cell_renderer_accel_get_preferred_width
                                                 (GtkCellRenderer *cell,
                                                  GtkWidget       *widget,
                                                  int             *minimum_size,
                                                  int             *natural_size);
static GtkCellEditable *
           gtk_cell_renderer_accel_start_editing (GtkCellRenderer      *cell,
                                                  GdkEvent             *event,
                                                  GtkWidget            *widget,
                                                  const char           *path,
                                                  const GdkRectangle   *background_area,
                                                  const GdkRectangle   *cell_area,
                                                  GtkCellRendererState  flags);
static char *convert_keysym_state_to_string     (GtkCellRendererAccel *accel,
                                                  guint                 keysym,
                                                  GdkModifierType       mask,
                                                  guint                 keycode);
static GtkWidget *gtk_cell_editable_widget_new (GtkCellRenderer          *cell,
                                                   GtkCellRendererAccelMode  mode,
                                                   const char               *path);

enum {
  ACCEL_EDITED,
  ACCEL_CLEARED,
  LAST_SIGNAL
};

enum {
  PROP_ACCEL_KEY = 1,
  PROP_ACCEL_MODS,
  PROP_KEYCODE,
  PROP_ACCEL_MODE
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct _GtkCellRendererAccelPrivate       GtkCellRendererAccelPrivate;
typedef struct _GtkCellRendererAccelClass         GtkCellRendererAccelClass;

struct _GtkCellRendererAccel
{
  GtkCellRendererText parent;
};

struct _GtkCellRendererAccelClass
{
  GtkCellRendererTextClass parent_class;

  void (* accel_edited)  (GtkCellRendererAccel *accel,
                          const char           *path_string,
                          guint                 accel_key,
                          GdkModifierType       accel_mods,
                          guint                 hardware_keycode);

  void (* accel_cleared) (GtkCellRendererAccel *accel,
                          const char           *path_string);

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

struct _GtkCellRendererAccelPrivate
{
  GtkWidget *sizing_label;

  GtkCellRendererAccelMode accel_mode;
  GdkModifierType accel_mods;
  guint accel_key;
  guint keycode;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererAccel, gtk_cell_renderer_accel, GTK_TYPE_CELL_RENDERER_TEXT)

static void
gtk_cell_renderer_accel_init (GtkCellRendererAccel *cell_accel)
{
  char *text;

  text = convert_keysym_state_to_string (cell_accel, 0, 0, 0);
  g_object_set (cell_accel, "text", text, NULL);
  g_free (text);
}

static void
gtk_cell_renderer_accel_dispose (GObject *object)
{
  GtkCellRendererAccelPrivate *priv = gtk_cell_renderer_accel_get_instance_private (GTK_CELL_RENDERER_ACCEL (object));

  g_clear_object (&priv->sizing_label);

  G_OBJECT_CLASS (gtk_cell_renderer_accel_parent_class)->dispose (object);
}

static void
gtk_cell_renderer_accel_class_init (GtkCellRendererAccelClass *cell_accel_class)
{
  GObjectClass *object_class;
  GtkCellRendererClass *cell_renderer_class;

  object_class = G_OBJECT_CLASS (cell_accel_class);
  cell_renderer_class = GTK_CELL_RENDERER_CLASS (cell_accel_class);

  object_class->set_property = gtk_cell_renderer_accel_set_property;
  object_class->get_property = gtk_cell_renderer_accel_get_property;
  object_class->dispose = gtk_cell_renderer_accel_dispose;

  cell_renderer_class->get_preferred_width = gtk_cell_renderer_accel_get_preferred_width;
  cell_renderer_class->start_editing = gtk_cell_renderer_accel_start_editing;

  /**
   * GtkCellRendererAccel:accel-key:
   *
   * The keyval of the accelerator.
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_KEY,
                                   g_param_spec_uint ("accel-key", NULL, NULL,
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererAccel:accel-mods:
   *
   * The modifier mask of the accelerator.
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODS,
                                   g_param_spec_flags ("accel-mods", NULL, NULL,
                                                       GDK_TYPE_MODIFIER_TYPE,
                                                       GDK_NO_MODIFIER_MASK,
                                                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererAccel:keycode:
   *
   * The hardware keycode of the accelerator. Note that the hardware keycode is
   * only relevant if the key does not have a keyval. Normally, the keyboard
   * configuration should assign keyvals to all keys.
   */
  g_object_class_install_property (object_class,
                                   PROP_KEYCODE,
                                   g_param_spec_uint ("keycode", NULL, NULL,
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererAccel:accel-mode:
   *
   * Determines if the edited accelerators are GTK accelerators. If
   * they are, consumed modifiers are suppressed, only accelerators
   * accepted by GTK are allowed, and the accelerators are rendered
   * in the same way as they are in menus.
   */
  g_object_class_install_property (object_class,
                                   PROP_ACCEL_MODE,
                                   g_param_spec_enum ("accel-mode", NULL, NULL,
                                                      GTK_TYPE_CELL_RENDERER_ACCEL_MODE,
                                                      GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                                                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererAccel::accel-edited:
   * @accel: the object receiving the signal
   * @path_string: the path identifying the row of the edited cell
   * @accel_key: the new accelerator keyval
   * @accel_mods: the new accelerator modifier mask
   * @hardware_keycode: the keycode of the new accelerator
   *
   * Gets emitted when the user has selected a new accelerator.
   */
  signals[ACCEL_EDITED] = g_signal_new (I_("accel-edited"),
                                        GTK_TYPE_CELL_RENDERER_ACCEL,
                                        G_SIGNAL_RUN_LAST,
                                        G_STRUCT_OFFSET (GtkCellRendererAccelClass, accel_edited),
                                        NULL, NULL,
                                        _gtk_marshal_VOID__STRING_UINT_FLAGS_UINT,
                                        G_TYPE_NONE, 4,
                                        G_TYPE_STRING,
                                        G_TYPE_UINT,
                                        GDK_TYPE_MODIFIER_TYPE,
                                        G_TYPE_UINT);

  /**
   * GtkCellRendererAccel::accel-cleared:
   * @accel: the object receiving the signal
   * @path_string: the path identifying the row of the edited cell
   *
   * Gets emitted when the user has removed the accelerator.
   */
  signals[ACCEL_CLEARED] = g_signal_new (I_("accel-cleared"),
                                         GTK_TYPE_CELL_RENDERER_ACCEL,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GtkCellRendererAccelClass, accel_cleared),
                                         NULL, NULL,
                                         NULL,
                                         G_TYPE_NONE, 1,
                                         G_TYPE_STRING);
}


/**
 * gtk_cell_renderer_accel_new:
 *
 * Creates a new `GtkCellRendererAccel`.
 *
 * Returns: the new cell renderer
 *
 * Deprecated: 4.10
 */
GtkCellRenderer *
gtk_cell_renderer_accel_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_ACCEL, NULL);
}

static char *
convert_keysym_state_to_string (GtkCellRendererAccel *accel,
                                guint                 keysym,
                                GdkModifierType       mask,
                                guint                 keycode)
{
  GtkCellRendererAccelPrivate *priv = gtk_cell_renderer_accel_get_instance_private (accel);

  if (keysym == 0 && keycode == 0)
    /* This label is displayed in a treeview cell displaying
     * a disabled accelerator key combination.
     */
    return g_strdup (C_("Accelerator", "Disabled"));
  else
    {
      if (priv->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK)
        {
          if (!gtk_accelerator_valid (keysym, mask))
            /* This label is displayed in a treeview cell displaying
             * an accelerator key combination that is not valid according
             * to gtk_accelerator_valid().
             */
            return g_strdup (C_("Accelerator", "Invalid"));

          return gtk_accelerator_get_label (keysym, mask);
        }
      else
        {
          char *name;

          name = gtk_accelerator_get_label_with_keycode (NULL, keysym, keycode, mask);
          if (name == NULL)
            name = gtk_accelerator_name_with_keycode (NULL, keysym, keycode, mask);

          return name;
        }
    }
}

static void
gtk_cell_renderer_accel_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkCellRendererAccelPrivate *priv = gtk_cell_renderer_accel_get_instance_private (GTK_CELL_RENDERER_ACCEL (object));

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      g_value_set_uint (value, priv->accel_key);
      break;

    case PROP_ACCEL_MODS:
      g_value_set_flags (value, priv->accel_mods);
      break;

    case PROP_KEYCODE:
      g_value_set_uint (value, priv->keycode);
      break;

    case PROP_ACCEL_MODE:
      g_value_set_enum (value, priv->accel_mode);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_accel_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkCellRendererAccel *accel = GTK_CELL_RENDERER_ACCEL (object);
  GtkCellRendererAccelPrivate *priv = gtk_cell_renderer_accel_get_instance_private (GTK_CELL_RENDERER_ACCEL (object));
  gboolean changed = FALSE;

  switch (param_id)
    {
    case PROP_ACCEL_KEY:
      {
        guint accel_key = g_value_get_uint (value);

        if (priv->accel_key != accel_key)
          {
            priv->accel_key = accel_key;
            changed = TRUE;
            g_object_notify (object, "accel-key");
          }
      }
      break;

    case PROP_ACCEL_MODS:
      {
        guint accel_mods = g_value_get_flags (value);

        if (priv->accel_mods != accel_mods)
          {
            priv->accel_mods = accel_mods;
            changed = TRUE;
            g_object_notify (object, "accel-mods");
          }
      }
      break;
    case PROP_KEYCODE:
      {
        guint keycode = g_value_get_uint (value);

        if (priv->keycode != keycode)
          {
            priv->keycode = keycode;
            changed = TRUE;
            g_object_notify (object, "keycode");
          }
      }
      break;

    case PROP_ACCEL_MODE:
      if (priv->accel_mode != g_value_get_enum (value))
        {
          priv->accel_mode = g_value_get_enum (value);
          g_object_notify (object, "accel-mode");
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }

  if (changed)
    {
      char *text;

      text = convert_keysym_state_to_string (accel, priv->accel_key, priv->accel_mods, priv->keycode);
      g_object_set (accel, "text", text, NULL);
      g_free (text);
    }
}

static void
gtk_cell_renderer_accel_get_preferred_width (GtkCellRenderer *cell,
                                             GtkWidget       *widget,
                                             int             *minimum_size,
                                             int             *natural_size)

{
  GtkCellRendererAccelPrivate *priv = gtk_cell_renderer_accel_get_instance_private (GTK_CELL_RENDERER_ACCEL (cell));
  GtkRequisition min_req, nat_req;

  if (priv->sizing_label == NULL)
    {
      priv->sizing_label = gtk_label_new (_("New accelerator…"));
      g_object_ref_sink (priv->sizing_label);
    }

  gtk_widget_get_preferred_size (priv->sizing_label, &min_req, &nat_req);

  GTK_CELL_RENDERER_CLASS (gtk_cell_renderer_accel_parent_class)->get_preferred_width (cell, widget,
                                                                                       minimum_size, natural_size);

  /* FIXME: need to take the cell_area et al. into account */
  if (minimum_size)
    *minimum_size = MAX (*minimum_size, min_req.width);
  if (natural_size)
    *natural_size = MAX (*natural_size, nat_req.width);
}

static GtkCellEditable *
gtk_cell_renderer_accel_start_editing (GtkCellRenderer      *cell,
                                       GdkEvent             *event,
                                       GtkWidget            *widget,
                                       const char           *path,
                                       const GdkRectangle   *background_area,
                                       const GdkRectangle   *cell_area,
                                       GtkCellRendererState  flags)
{
  GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
  GtkCellRendererAccel *accel = GTK_CELL_RENDERER_ACCEL (cell);
  GtkCellRendererAccelPrivate *priv = gtk_cell_renderer_accel_get_instance_private (accel);
  GtkWidget *editable;
  gboolean is_editable;

  /* If the cell isn't editable we return NULL. */
  g_object_get (celltext, "editable", &is_editable, NULL);
  if (!is_editable)
    return NULL;

  editable = gtk_cell_editable_widget_new (cell, priv->accel_mode, path);

  return GTK_CELL_EDITABLE (editable);
}

/* --------------------------------- */

typedef struct _GtkCellEditableWidget GtkCellEditableWidget;
typedef         GtkWidgetClass        GtkCellEditableWidgetClass;

struct _GtkCellEditableWidget
{
  GtkWidget parent;

  gboolean editing_canceled;
  GtkCellRendererAccelMode accel_mode;
  char *path;
  GtkCellRenderer *cell;
  GtkWidget *label;
};

enum {
  PROP_EDITING_CANCELED = 1,
  PROP_MODE,
  PROP_PATH
};

GType       gtk_cell_editable_widget_get_type (void);
static void gtk_cell_editable_widget_cell_editable_init (GtkCellEditableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkCellEditableWidget, gtk_cell_editable_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE, gtk_cell_editable_widget_cell_editable_init))

static void
gtk_cell_editable_widget_start_editing (GtkCellEditable *cell_editable,
                                        GdkEvent        *event)
{
  /* do nothing, because we are pointless */
}

static void
gtk_cell_editable_widget_cell_editable_init (GtkCellEditableIface *iface)
{
  iface->start_editing = gtk_cell_editable_widget_start_editing;
}

static gboolean
key_controller_modifiers (GtkEventControllerKey *key,
                          GdkModifierType        state,
                          GtkWidget             *widget)
{
  /* Ignore modifiers */
  return TRUE;
}

static gboolean
key_controller_key_pressed (GtkEventControllerKey *key,
                            guint                  keyval,
                            guint                  keycode,
                            GdkModifierType        state,
                            GtkWidget             *widget)
{
  GtkCellEditableWidget *box = (GtkCellEditableWidget*)widget;
  gboolean edited = FALSE;
  gboolean cleared = FALSE;
  GdkModifierType accel_mods = 0;
  guint accel_key;
  GdkEvent *event;

  event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (key));
  if (!gdk_key_event_get_match (event, &accel_key, &accel_mods))
    return FALSE;

  if (accel_mods == 0)
    {
      switch (keyval)
	{
	case GDK_KEY_BackSpace:
	  cleared = TRUE;
          G_GNUC_FALLTHROUGH;
	case GDK_KEY_Escape:
	  goto out;
	default:
	  break;
	}
    }

  if (box->accel_mode == GTK_CELL_RENDERER_ACCEL_MODE_GTK &&
      !gtk_accelerator_valid (accel_key, accel_mods))
    {
      gtk_widget_error_bell (widget);
      return TRUE;
    }

  edited = TRUE;

 out:
  gtk_grab_remove (widget);
  gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (widget));
  gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (widget));

  if (edited)
    g_signal_emit (box->cell, signals[ACCEL_EDITED], 0, box->path,
                   accel_key, accel_mods, keycode);
  else if (cleared)
    g_signal_emit (box->cell, signals[ACCEL_CLEARED], 0, box->path);

  return TRUE;
}

static void
gtk_cell_editable_widget_unrealize (GtkWidget *widget)
{
  gtk_grab_remove (widget);

  GTK_WIDGET_CLASS (gtk_cell_editable_widget_parent_class)->unrealize (widget);
}

static void
gtk_cell_editable_widget_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkCellEditableWidget *box = (GtkCellEditableWidget*)object;

  switch (prop_id)
    {
    case PROP_EDITING_CANCELED:
      box->editing_canceled = g_value_get_boolean (value);
      break;
    case PROP_MODE:
      box->accel_mode = g_value_get_enum (value);
      break;
    case PROP_PATH:
      box->path = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_editable_widget_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkCellEditableWidget *box = (GtkCellEditableWidget*)object;

  switch (prop_id)
    {
    case PROP_EDITING_CANCELED:
      g_value_set_boolean (value, box->editing_canceled);
      break;
    case PROP_MODE:
      g_value_set_enum (value, box->accel_mode);
      break;
    case PROP_PATH:
      g_value_set_string (value, box->path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_editable_widget_dispose (GObject *object)
{
  GtkCellEditableWidget *box = (GtkCellEditableWidget*)object;

  g_clear_pointer (&box->label, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_cell_editable_widget_parent_class)->dispose (object);
}

static void
gtk_cell_editable_widget_finalize (GObject *object)
{
  GtkCellEditableWidget *box = (GtkCellEditableWidget*)object;

  g_free (box->path);

  G_OBJECT_CLASS (gtk_cell_editable_widget_parent_class)->finalize (object);
}

static void
gtk_cell_editable_widget_class_init (GtkCellEditableWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = gtk_cell_editable_widget_finalize;
  object_class->dispose = gtk_cell_editable_widget_dispose;
  object_class->set_property = gtk_cell_editable_widget_set_property;
  object_class->get_property = gtk_cell_editable_widget_get_property;

  widget_class->unrealize = gtk_cell_editable_widget_unrealize;

  g_object_class_override_property (object_class,
                                    PROP_EDITING_CANCELED,
                                    "editing-canceled");

  g_object_class_install_property (object_class, PROP_MODE,
      g_param_spec_enum ("accel-mode", NULL, NULL,
                         GTK_TYPE_CELL_RENDERER_ACCEL_MODE,
                         GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                         GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_PATH,
      g_param_spec_string ("path", NULL, NULL,
                           NULL, GTK_PARAM_READWRITE));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("acceleditor"));
}

static void
gtk_cell_editable_widget_init (GtkCellEditableWidget *box)
{
  GtkWidget *widget = GTK_WIDGET (box);
  GtkEventController *controller;

  gtk_widget_set_focusable (widget, TRUE);

  controller = gtk_event_controller_key_new ();
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (key_controller_key_pressed), box);
  g_signal_connect (controller, "modifiers",
                    G_CALLBACK (key_controller_modifiers), box);
  gtk_widget_add_controller (widget, controller);
}

static GtkWidget *
gtk_cell_editable_widget_new (GtkCellRenderer          *cell,
                              GtkCellRendererAccelMode  mode,
                              const char               *path)
{
  GtkCellEditableWidget *box;

  box = g_object_new (gtk_cell_editable_widget_get_type (),
                      "accel-mode", mode,
                      "path", path,
                      NULL);
  box->cell = cell;

  box->label = gtk_label_new (NULL);
  gtk_widget_set_halign (box->label, GTK_ALIGN_START);
  gtk_widget_set_valign (box->label, GTK_ALIGN_CENTER);

  gtk_widget_set_state_flags (box->label, GTK_STATE_FLAG_SELECTED, TRUE);

  /* This label is displayed in a treeview cell displaying an accelerator
   * when the cell is clicked to change the accelerator.
   */
  gtk_label_set_text (GTK_LABEL (box->label), _("New accelerator…"));

  gtk_widget_set_parent (box->label, GTK_WIDGET (box));

  gtk_grab_add (GTK_WIDGET (box));

  return GTK_WIDGET (box);
}
