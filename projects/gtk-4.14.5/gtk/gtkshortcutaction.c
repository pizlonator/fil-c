/*
* Copyright © 2018 Benjamin Otte
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
* Authors: Benjamin Otte <otte@gnome.org>
*/

/**
* GtkShortcutAction:
*
* `GtkShortcutAction` encodes an action that can be triggered by a
* keyboard shortcut.
*
* `GtkShortcutActions` contain functions that allow easy presentation
* to end users as well as being printed for debugging.
*
* All `GtkShortcutActions` are immutable, you can only specify their
* properties during construction. If you want to change a action, you
* have to replace it with a new one. If you need to pass arguments to
* an action, these are specified by the higher-level `GtkShortcut` object.
*
* To activate a `GtkShortcutAction` manually, [method@Gtk.ShortcutAction.activate]
* can be called.
*
* GTK provides various actions:
*
*  - [class@Gtk.MnemonicAction]: a shortcut action that calls
*    gtk_widget_mnemonic_activate()
*  - [class@Gtk.CallbackAction]: a shortcut action that invokes
*    a given callback
*  - [class@Gtk.SignalAction]: a shortcut action that emits a
*    given signal
*  - [class@Gtk.ActivateAction]: a shortcut action that calls
*    gtk_widget_activate()
*  - [class@Gtk.NamedAction]: a shortcut action that calls
*    gtk_widget_activate_action()
*  - [class@Gtk.NothingAction]: a shortcut action that does nothing
*/

#include "config.h"

#include "gtkshortcutactionprivate.h"

#include "gtkbuilder.h"
#include "gtkwidgetprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"

/* {{{ GtkShortcutAction */

struct _GtkShortcutAction
{
GObject parent_instance;
};

struct _GtkShortcutActionClass
{
  GObjectClass parent_class;

  gboolean      (* activate)    (GtkShortcutAction            *action,
                                 GtkShortcutActionFlags        flags,
                                 GtkWidget                    *widget,
                                 GVariant                     *args);
  void          (* print)       (GtkShortcutAction            *action,
                                 GString                      *string);
};

G_DEFINE_ABSTRACT_TYPE (GtkShortcutAction, gtk_shortcut_action, G_TYPE_OBJECT)

static void
gtk_shortcut_action_class_init (GtkShortcutActionClass *klass)
{
}

static void
gtk_shortcut_action_init (GtkShortcutAction *self)
{
}

/**
 * gtk_shortcut_action_to_string:
 * @self: a `GtkShortcutAction`
 *
 * Prints the given action into a human-readable string.
 *
 * This is a small wrapper around [method@Gtk.ShortcutAction.print]
 * to help when debugging.
 *
 * Returns: (transfer full): a new string
 */
char *
gtk_shortcut_action_to_string (GtkShortcutAction *self)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION (self), NULL);

  string = g_string_new (NULL);
  gtk_shortcut_action_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_shortcut_action_print:
 * @self: a `GtkShortcutAction`
 * @string: a `GString` to print into
 *
 * Prints the given action into a string for the developer.
 *
 * This is meant for debugging and logging.
 *
 * The form of the representation may change at any time and is
 * not guaranteed to stay identical.
 */
void
gtk_shortcut_action_print (GtkShortcutAction *self,
                           GString           *string)
{
  g_return_if_fail (GTK_IS_SHORTCUT_ACTION (self));
  g_return_if_fail (string != NULL);

  GTK_SHORTCUT_ACTION_GET_CLASS (self)->print (self, string);
}

/**
 * gtk_shortcut_action_activate:
 * @self: a `GtkShortcutAction`
 * @flags: flags to activate with
 * @widget: Target of the activation
 * @args: (nullable): arguments to pass
 *
 * Activates the action on the @widget with the given @args.
 *
 * Note that some actions ignore the passed in @flags, @widget or @args.
 *
 * Activation of an action can fail for various reasons. If the action
 * is not supported by the @widget, if the @args don't match the action
 * or if the activation otherwise had no effect, %FALSE will be returned.
 *
 * Returns: %TRUE if this action was activated successfully
 */
gboolean
gtk_shortcut_action_activate (GtkShortcutAction      *self,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION (self), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (GTK_DEBUG_CHECK (KEYBINDINGS))
    {
      char *act = gtk_shortcut_action_to_string (self);
      gdk_debug_message ("Shortcut action activate on %s: %s", G_OBJECT_TYPE_NAME (widget), act);
      g_free (act);
    }

  return GTK_SHORTCUT_ACTION_GET_CLASS (self)->activate (self, flags, widget, args);
}

static char *
string_is_function (const char *string,
                    const char *function_name)
{
  gsize len;

  if (!g_str_has_prefix (string, function_name))
    return NULL;
  string += strlen (function_name);

  if (string[0] != '(')
    return NULL;
  string ++;

  len = strlen (string);
  if (len == 0 || string[len - 1] != ')')
    return NULL;

  return g_strndup (string, len - 1);
}

/**
 * gtk_shortcut_action_parse_string: (constructor)
 * @string: the string to parse
 *
 * Tries to parse the given string into an action.
 *
 * On success, the parsed action is returned. When parsing
 * failed, %NULL is returned.
 *
 * The accepted strings are:
 *
 * - `nothing`, for `GtkNothingAction`
 * - `activate`, for `GtkActivateAction`
 * - `mnemonic-activate`, for `GtkMnemonicAction`
 * - `action(NAME)`, for a `GtkNamedAction` for the action named `NAME`
 * - `signal(NAME)`, for a `GtkSignalAction` for the signal `NAME`
 *
 * Returns: (nullable) (transfer full): a new `GtkShortcutAction`
 */
GtkShortcutAction *
gtk_shortcut_action_parse_string (const char *string)
{
  GtkShortcutAction *result;
  char *arg;

  if (g_str_equal (string, "nothing"))
    return g_object_ref (gtk_nothing_action_get ());
  if (g_str_equal (string, "activate"))
    return g_object_ref (gtk_activate_action_get ());
  if (g_str_equal (string, "mnemonic-activate"))
    return g_object_ref (gtk_mnemonic_action_get ());

  if ((arg = string_is_function (string, "action")))
    {
      result = gtk_named_action_new (arg);
      g_free (arg);
    }
  else if ((arg = string_is_function (string, "signal")))
    {
      result = gtk_signal_action_new (arg);
      g_free (arg);
    }
  else
    return NULL;

  return result;
}

GtkShortcutAction *
gtk_shortcut_action_parse_builder (GtkBuilder  *builder,
                                   const char  *string,
                                   GError     **error)
{
  GtkShortcutAction *result;

  result = gtk_shortcut_action_parse_string (string);

  if (!result)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                   "String \"%s\" does not specify a GtkShortcutAction", string);
      return NULL;
    }

  return result;
}

/* }}} */

/* {{{ GtkNothingAction */

struct _GtkNothingAction
{
  GtkShortcutAction parent_instance;
};

struct _GtkNothingActionClass
{
  GtkShortcutActionClass parent_class;
};

G_DEFINE_TYPE (GtkNothingAction, gtk_nothing_action, GTK_TYPE_SHORTCUT_ACTION)

static void G_GNUC_NORETURN
gtk_nothing_action_finalize (GObject *gobject)
{
  g_assert_not_reached ();

  G_OBJECT_CLASS (gtk_nothing_action_parent_class)->finalize (gobject);
}

static gboolean
gtk_nothing_action_activate (GtkShortcutAction      *action,
                             GtkShortcutActionFlags  flags,
                             GtkWidget              *widget,
                             GVariant               *args)
{
  return FALSE;
}

static void
gtk_nothing_action_print (GtkShortcutAction *action,
                          GString           *string)
{
  g_string_append (string, "nothing");
}

static void
gtk_nothing_action_class_init (GtkNothingActionClass *klass)
{
  GtkShortcutActionClass *action_class = GTK_SHORTCUT_ACTION_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gtk_nothing_action_finalize;

  action_class->activate = gtk_nothing_action_activate;
  action_class->print = gtk_nothing_action_print;
}

static void
gtk_nothing_action_init (GtkNothingAction *self)
{
}

/**
 * gtk_nothing_action_get:
 *
 * Gets the nothing action.
 *
 * This is an action that does nothing and where
 * activating it always fails.
 *
 * Returns: (transfer none) (type GtkNothingAction): The nothing action
 */
GtkShortcutAction *
gtk_nothing_action_get (void)
{
  static GtkShortcutAction *nothing;

  if (nothing == NULL)
    nothing = g_object_new (GTK_TYPE_NOTHING_ACTION, NULL);

  return nothing;
}

/* }}} */

/* {{{ GtkCallbackAction */

struct _GtkCallbackAction
{
  GtkShortcutAction parent_instance;

  GtkShortcutFunc callback;
  gpointer user_data;
  GDestroyNotify destroy_notify;
};

struct _GtkCallbackActionClass
{
  GtkShortcutActionClass parent_class;
};

G_DEFINE_TYPE (GtkCallbackAction, gtk_callback_action, GTK_TYPE_SHORTCUT_ACTION)

static void
gtk_callback_action_finalize (GObject *gobject)
{
  GtkCallbackAction *self = GTK_CALLBACK_ACTION (gobject);

  if (self->destroy_notify != NULL)
    self->destroy_notify (self->user_data);

  G_OBJECT_CLASS (gtk_callback_action_parent_class)->finalize (gobject);
}

static gboolean
gtk_callback_action_activate (GtkShortcutAction      *action,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  GtkCallbackAction *self = GTK_CALLBACK_ACTION (action);

  return self->callback (widget, args, self->user_data);
}

static void
gtk_callback_action_print (GtkShortcutAction *action,
                           GString           *string)
{
  GtkCallbackAction *self = GTK_CALLBACK_ACTION (action);

  g_string_append_printf (string, "callback<%p>", self->callback);
}

static void
gtk_callback_action_class_init (GtkCallbackActionClass *klass)
{
  GtkShortcutActionClass *action_class = GTK_SHORTCUT_ACTION_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gtk_callback_action_finalize;

  action_class->activate = gtk_callback_action_activate;
  action_class->print = gtk_callback_action_print;
}

static void
gtk_callback_action_init (GtkCallbackAction *self)
{
}

/**
 * gtk_callback_action_new:
 * @callback: (scope notified): the callback to call
 * @data: (closure callback): the data to be passed to @callback
 * @destroy: (destroy data): the function to be called when the
 *   callback action is finalized
 *
 * Create a custom action that calls the given @callback when
 * activated.
 *
 * Returns: (transfer full) (type GtkCallbackAction): A new shortcut action
 */
GtkShortcutAction *
gtk_callback_action_new (GtkShortcutFunc callback,
                         gpointer        data,
                         GDestroyNotify  destroy)
{
  GtkCallbackAction *self;

  g_return_val_if_fail (callback != NULL, NULL);

  self = g_object_new (GTK_TYPE_CALLBACK_ACTION, NULL);

  self->callback = callback;
  self->user_data = data;
  self->destroy_notify = destroy;

  return GTK_SHORTCUT_ACTION (self);
}

/* }}} */

/* {{{ GtkActivateAction */

struct _GtkActivateAction
{
  GtkShortcutAction parent_instance;
};

struct _GtkActivateActionClass
{
  GtkShortcutActionClass parent_class;
};

G_DEFINE_TYPE (GtkActivateAction, gtk_activate_action, GTK_TYPE_SHORTCUT_ACTION)

static void G_GNUC_NORETURN
gtk_activate_action_finalize (GObject *gobject)
{
  g_assert_not_reached ();

  G_OBJECT_CLASS (gtk_activate_action_parent_class)->finalize (gobject);
}

static gboolean
gtk_activate_action_activate (GtkShortcutAction      *action,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  return gtk_widget_activate (widget);
}

static void
gtk_activate_action_print (GtkShortcutAction *action,
                           GString           *string)
{
  g_string_append (string, "activate");
}

static void
gtk_activate_action_class_init (GtkActivateActionClass *klass)
{
  GtkShortcutActionClass *action_class = GTK_SHORTCUT_ACTION_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gtk_activate_action_finalize;

  action_class->activate = gtk_activate_action_activate;
  action_class->print = gtk_activate_action_print;
}

static void
gtk_activate_action_init (GtkActivateAction *self)
{
}

/**
 * gtk_activate_action_get:
 *
 * Gets the activate action.
 *
 * This is an action that calls gtk_widget_activate()
 * on the given widget upon activation.
 *
 * Returns: (transfer none) (type GtkActivateAction): The activate action
 */
GtkShortcutAction *
gtk_activate_action_get (void)
{
  static GtkShortcutAction *action;

  if (action == NULL)
    action = g_object_new (GTK_TYPE_ACTIVATE_ACTION, NULL);

  return action;
}

/* }}} */

/* {{{ GtkMnemonicAction */

struct _GtkMnemonicAction
{
  GtkShortcutAction parent_instance;
};

struct _GtkMnemonicActionClass
{
  GtkShortcutActionClass parent_class;
};

G_DEFINE_TYPE (GtkMnemonicAction, gtk_mnemonic_action, GTK_TYPE_SHORTCUT_ACTION)

static void G_GNUC_NORETURN
gtk_mnemonic_action_finalize (GObject *gobject)
{
  g_assert_not_reached ();

  G_OBJECT_CLASS (gtk_mnemonic_action_parent_class)->finalize (gobject);
}

static gboolean
gtk_mnemonic_action_activate (GtkShortcutAction      *action,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  return gtk_widget_mnemonic_activate (widget, flags & GTK_SHORTCUT_ACTION_EXCLUSIVE ? FALSE : TRUE);
}

static void
gtk_mnemonic_action_print (GtkShortcutAction *action,
                           GString           *string)
{
  g_string_append (string, "mnemonic-activate");
}

static void
gtk_mnemonic_action_class_init (GtkMnemonicActionClass *klass)
{
  GtkShortcutActionClass *action_class = GTK_SHORTCUT_ACTION_CLASS (klass);

  G_OBJECT_CLASS (klass)->finalize = gtk_mnemonic_action_finalize;

  action_class->activate = gtk_mnemonic_action_activate;
  action_class->print = gtk_mnemonic_action_print;
}

static void
gtk_mnemonic_action_init (GtkMnemonicAction *self)
{
}

/**
 * gtk_mnemonic_action_get:
 *
 * Gets the mnemonic action.
 *
 * This is an action that calls gtk_widget_mnemonic_activate()
 * on the given widget upon activation.
 *
 * Returns: (transfer none) (type GtkMnemonicAction): The mnemonic action
 */
GtkShortcutAction *
gtk_mnemonic_action_get (void)
{
  static GtkShortcutAction *mnemonic;

  if (G_UNLIKELY (mnemonic == NULL))
    mnemonic = g_object_new (GTK_TYPE_MNEMONIC_ACTION, NULL);

  return mnemonic;
}

/* }}} */

/* {{{ GtkSignalAction */

struct _GtkSignalAction
{
  GtkShortcutAction parent_instance;

  const char *name; /* interned */
};

struct _GtkSignalActionClass
{
  GtkShortcutActionClass parent_class;
};

enum
{
  SIGNAL_PROP_SIGNAL_NAME = 1,
  SIGNAL_N_PROPS
};

static GParamSpec *signal_props[SIGNAL_N_PROPS];

G_DEFINE_TYPE (GtkSignalAction, gtk_signal_action, GTK_TYPE_SHORTCUT_ACTION)

static void
gtk_signal_action_finalize (GObject *gobject)
{
  //GtkSignalAction *self = GTK_SIGNAL_ACTION (gobject);

  G_OBJECT_CLASS (gtk_signal_action_parent_class)->finalize (gobject);
}

static gboolean
binding_compose_params (GtkWidget     *widget,
                        GVariantIter  *args,
                        GSignalQuery  *query,
                        GValue       **params_p)
{
  GValue *params;
  const GType *types;
  guint i;
  gboolean valid;

  params = g_new0 (GValue, query->n_params + 1);
  *params_p = params;

  /* The instance we emit on is the first object in the array
   */
  g_value_init (params, G_TYPE_OBJECT);
  g_value_set_object (params, G_OBJECT (widget));
  params++;

  types = query->param_types;
  valid = TRUE;
  for (i = 1; i < query->n_params + 1 && valid; i++)
    {
      GValue tmp_value = G_VALUE_INIT;
      GVariant *tmp_variant;

      g_value_init (params, *types);
      tmp_variant = g_variant_iter_next_value (args);

      switch ((guint) g_variant_classify (tmp_variant))
        {
        case G_VARIANT_CLASS_BOOLEAN:
          g_value_init (&tmp_value, G_TYPE_BOOLEAN);
          g_value_set_boolean (&tmp_value, g_variant_get_boolean (tmp_variant));
          break;
        case G_VARIANT_CLASS_DOUBLE:
          g_value_init (&tmp_value, G_TYPE_DOUBLE);
          g_value_set_double (&tmp_value, g_variant_get_double (tmp_variant));
          break;
        case G_VARIANT_CLASS_INT32:
          g_value_init (&tmp_value, G_TYPE_LONG);
          g_value_set_long (&tmp_value, g_variant_get_int32 (tmp_variant));
          break;
        case G_VARIANT_CLASS_UINT32:
          g_value_init (&tmp_value, G_TYPE_LONG);
          g_value_set_long (&tmp_value, g_variant_get_uint32 (tmp_variant));
          break;
        case G_VARIANT_CLASS_INT64:
          g_value_init (&tmp_value, G_TYPE_LONG);
          g_value_set_long (&tmp_value, g_variant_get_int64 (tmp_variant));
          break;
        case G_VARIANT_CLASS_STRING:
          /* gtk_rc_parse_flags/enum() has fancier parsing for this; we can't call
           * that since we don't have a GParamSpec, so just do something simple
           */
          if (G_TYPE_FUNDAMENTAL (*types) == G_TYPE_ENUM)
            {
              GEnumClass *class = G_ENUM_CLASS (g_type_class_ref (*types));
              GEnumValue *enum_value;
              const char *s = g_variant_get_string (tmp_variant, NULL);

              valid = FALSE;

              enum_value = g_enum_get_value_by_name (class, s);
              if (!enum_value)
                enum_value = g_enum_get_value_by_nick (class, s);

              if (enum_value)
                {
                  g_value_init (&tmp_value, *types);
                  g_value_set_enum (&tmp_value, enum_value->value);
                  valid = TRUE;
                }

              g_type_class_unref (class);
            }
          /* This is just a hack for compatibility with GTK 1.2 where a string
           * could be used for a single flag value / without the support for multiple
           * values in gtk_rc_parse_flags(), this isn't very useful.
           */
          else if (G_TYPE_FUNDAMENTAL (*types) == G_TYPE_FLAGS)
            {
              GFlagsClass *class = G_FLAGS_CLASS (g_type_class_ref (*types));
              GFlagsValue *flags_value;
              const char *s = g_variant_get_string (tmp_variant, NULL);

              valid = FALSE;

              flags_value = g_flags_get_value_by_name (class, s);
              if (!flags_value)
                flags_value = g_flags_get_value_by_nick (class, s);
              if (flags_value)
                {
                  g_value_init (&tmp_value, *types);
                  g_value_set_flags (&tmp_value, flags_value->value);
                  valid = TRUE;
                }

              g_type_class_unref (class);
            }
          else
            {
              g_value_init (&tmp_value, G_TYPE_STRING);
              g_value_set_static_string (&tmp_value, g_variant_get_string (tmp_variant, NULL));
            }
          break;
        default:
          valid = FALSE;
          break;
        }

      if (valid)
        {
          if (!g_value_transform (&tmp_value, params))
            valid = FALSE;

          g_value_unset (&tmp_value);
        }

      g_variant_unref (tmp_variant);
      types++;
      params++;
    }

  if (!valid)
    {
      guint j;

      for (j = 0; j < i; j++)
        g_value_unset (&(*params_p)[j]);

      g_free (*params_p);
      *params_p = NULL;
    }

  return valid;
}

static gboolean
gtk_signal_action_emit_signal (GtkWidget   *widget,
                               const char  *signal,
                               GVariant    *args,
                               gboolean    *handled,
                               GError     **error)
{
  GSignalQuery query;
  guint signal_id;
  GValue *params = NULL;
  GValue return_val = G_VALUE_INIT;
  GVariantIter args_iter;
  gsize n_args;
  guint i;

  *handled = FALSE;

  signal_id = g_signal_lookup (signal, G_OBJECT_TYPE (widget));
  if (!signal_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not find signal \"%s\" in the '%s' class ancestry",
                   signal,
                   g_type_name (G_OBJECT_TYPE (widget)));
      return FALSE;
    }

  g_signal_query (signal_id, &query);
  if (args == NULL)
    n_args = 0;
  else if (g_variant_is_of_type (args, G_VARIANT_TYPE_TUPLE))
    n_args = g_variant_iter_init (&args_iter, args);
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "argument GVariant is not a tuple");
      return FALSE;
    }
  if (query.n_params != n_args ||
      (query.return_type != G_TYPE_NONE && query.return_type != G_TYPE_BOOLEAN) ||
      !binding_compose_params (widget, &args_iter, &query, &params))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "signature mismatch for signal \"%s\" in the '%s' class ancestry",
                   signal,
                   g_type_name (G_OBJECT_TYPE (widget)));
      return FALSE;
    }
  else if (!(query.signal_flags & G_SIGNAL_ACTION))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "signal \"%s\" in the '%s' class ancestry cannot be used for action emissions",
                   signal,
                   g_type_name (G_OBJECT_TYPE (widget)));
      return FALSE;
    }

  if (query.return_type == G_TYPE_BOOLEAN)
    g_value_init (&return_val, G_TYPE_BOOLEAN);

  g_signal_emitv (params, signal_id, 0, &return_val);

  if (query.return_type == G_TYPE_BOOLEAN)
    {
      if (g_value_get_boolean (&return_val))
        *handled = TRUE;
      g_value_unset (&return_val);
    }
  else
    *handled = TRUE;

  if (params != NULL)
    {
      for (i = 0; i < query.n_params + 1; i++)
        g_value_unset (&params[i]);

      g_free (params);
    }

  return TRUE;
}

static gboolean
gtk_signal_action_activate (GtkShortcutAction      *action,
                            GtkShortcutActionFlags  flags,
                            GtkWidget              *widget,
                            GVariant               *args)
{
  GtkSignalAction *self = GTK_SIGNAL_ACTION (action);
  GError *error = NULL;
  gboolean handled;

  if (!gtk_signal_action_emit_signal (widget,
                                      self->name,
                                      args,
                                      &handled,
                                      &error))
    {
      g_warning ("gtk_signal_action_activate(): %s",
                 error->message);
      g_clear_error (&error);
      return FALSE;
    }

  return handled;
}

static void
gtk_signal_action_print (GtkShortcutAction *action,
                         GString           *string)
{
  GtkSignalAction *self = GTK_SIGNAL_ACTION (action);

  g_string_append_printf (string, "signal(%s)", self->name);
}

static void
gtk_signal_action_constructed (GObject *gobject)
{
  GtkSignalAction *self G_GNUC_UNUSED = GTK_SIGNAL_ACTION (gobject);

  g_assert (self->name != NULL && self->name[0] != '\0');

  G_OBJECT_CLASS (gtk_signal_action_parent_class)->constructed (gobject);
}

static void
gtk_signal_action_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkSignalAction *self = GTK_SIGNAL_ACTION (gobject);

  switch (prop_id)
    {
    case SIGNAL_PROP_SIGNAL_NAME:
      self->name = g_intern_string (g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_signal_action_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkSignalAction *self = GTK_SIGNAL_ACTION (gobject);

  switch (prop_id)
    {
    case SIGNAL_PROP_SIGNAL_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_signal_action_class_init (GtkSignalActionClass *klass)
{
  GtkShortcutActionClass *action_class = GTK_SHORTCUT_ACTION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_signal_action_constructed;
  gobject_class->set_property = gtk_signal_action_set_property;
  gobject_class->get_property = gtk_signal_action_get_property;
  gobject_class->finalize = gtk_signal_action_finalize;

  action_class->activate = gtk_signal_action_activate;
  action_class->print = gtk_signal_action_print;

  /**
   * GtkSignalAction:signal-name: (attributes org.gtk.Property.get=gtk_signal_action_get_signal_name)
   *
   * The name of the signal to emit.
   */
  signal_props[SIGNAL_PROP_SIGNAL_NAME] =
    g_param_spec_string (I_("signal-name"), NULL, NULL,
                         NULL,
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, SIGNAL_N_PROPS, signal_props);
}

static void
gtk_signal_action_init (GtkSignalAction *self)
{
}

/**
 * gtk_signal_action_new:
 * @signal_name: name of the signal to emit
 *
 * Creates an action that when activated, emits the given action signal
 * on the provided widget.
 *
 * It will also unpack the args into arguments passed to the signal.
 *
 * Returns: (transfer full) (type GtkSignalAction): a new `GtkShortcutAction`
 */
GtkShortcutAction *
gtk_signal_action_new (const char *signal_name)
{
  GtkShortcutAction *action;
  const char *name = "signal-name";
  GValue value = G_VALUE_INIT;

  g_return_val_if_fail (signal_name != NULL, NULL);

  g_value_init (&value, G_TYPE_STRING);
  g_value_set_static_string (&value, signal_name);

  action = GTK_SHORTCUT_ACTION (g_object_new_with_properties (GTK_TYPE_SIGNAL_ACTION,
                                                              1, &name, &value));

  g_value_unset (&value);

  return action;
}

/**
 * gtk_signal_action_get_signal_name: (attributes org.gtk.Method.get_property=signal-name)
 * @self: a signal action
 *
 * Returns the name of the signal that will be emitted.
 *
 * Returns: (transfer none): the name of the signal to emit
 */
const char *
gtk_signal_action_get_signal_name (GtkSignalAction *self)
{
  g_return_val_if_fail (GTK_IS_SIGNAL_ACTION (self), NULL);

  return self->name;
}

/* }}} */

/* {{{ GtkNamedAction */

struct _GtkNamedAction
{
  GtkShortcutAction parent_instance;

  char *name;
};

struct _GtkNamedActionClass
{
  GtkShortcutActionClass parent_class;
};

enum
{
  NAMED_PROP_ACTION_NAME = 1,
  NAMED_N_PROPS
};

static GParamSpec *named_props[NAMED_N_PROPS];

G_DEFINE_TYPE (GtkNamedAction, gtk_named_action, GTK_TYPE_SHORTCUT_ACTION)

static void
gtk_named_action_finalize (GObject *gobject)
{
  GtkNamedAction *self = GTK_NAMED_ACTION (gobject);

  g_free (self->name);

  G_OBJECT_CLASS (gtk_named_action_parent_class)->finalize (gobject);
}

static gboolean
check_parameter_type (GVariant           *args,
                      const GVariantType *parameter_type)
{
  if (args)
    {
      if (parameter_type == NULL)
        {
          g_warning ("Trying to invoke action with arguments, but action has no parameter");
          return FALSE;
        }

      if (!g_variant_is_of_type (args, parameter_type))
        {
          char *typestr = g_variant_type_dup_string (parameter_type);
          char *targetstr = g_variant_print (args, TRUE);
          g_warning ("Trying to invoke action with target '%s',"
                     " but action expects parameter with type '%s'", targetstr, typestr);
          g_free (targetstr);
          g_free (typestr);
          return FALSE;
        }
    }
  else
    {
      if (parameter_type != NULL)
        {
          char *typestr = g_variant_type_dup_string (parameter_type);
          g_warning ("Trying to invoke action without arguments,"
                     " but action expects parameter with type '%s'", typestr);
          g_free (typestr);
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
gtk_named_action_activate (GtkShortcutAction      *action,
                           GtkShortcutActionFlags  flags,
                           GtkWidget              *widget,
                           GVariant               *args)
{
  GtkNamedAction *self = GTK_NAMED_ACTION (action);
  const GVariantType *parameter_type;
  GtkActionMuxer *muxer;
  gboolean enabled;

  muxer = _gtk_widget_get_action_muxer (widget, FALSE);
  if (muxer == NULL)
    return FALSE;

  if (!gtk_action_muxer_query_action (muxer, self->name,
                                      &enabled, &parameter_type,
                                      NULL, NULL, NULL))
    return FALSE;

  if (!enabled)
    return FALSE;

  /* We found an action with the correct name and it's enabled.
   * This is the action that we are going to try to invoke.
   *
   * There is still the possibility that the args don't
   * match the expected parameter type.  In that case, we will print
   * a warning.
   */
  if (!check_parameter_type (args, parameter_type))
    return FALSE;

  gtk_action_muxer_activate_action (muxer, self->name, args);

  return TRUE;
}

static void
gtk_named_action_print (GtkShortcutAction *action,
                        GString           *string)
{
  GtkNamedAction *self = GTK_NAMED_ACTION (action);

  g_string_append_printf (string, "action(%s)", self->name);
}

static void
gtk_named_action_set_property (GObject      *gobject,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkNamedAction *self = GTK_NAMED_ACTION (gobject);

  switch (prop_id)
    {
    case NAMED_PROP_ACTION_NAME:
      self->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_named_action_get_property (GObject    *gobject,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkNamedAction *self = GTK_NAMED_ACTION (gobject);

  switch (prop_id)
    {
    case NAMED_PROP_ACTION_NAME:
      g_value_set_string (value, self->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_named_action_constructed (GObject *gobject)
{
  GtkNamedAction *self G_GNUC_UNUSED = GTK_NAMED_ACTION (gobject);

  g_assert (self->name != NULL && self->name[0] != '\0');

  G_OBJECT_CLASS (gtk_named_action_parent_class)->constructed (gobject);
}

static void
gtk_named_action_class_init (GtkNamedActionClass *klass)
{
  GtkShortcutActionClass *action_class = GTK_SHORTCUT_ACTION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = gtk_named_action_constructed;
  gobject_class->set_property = gtk_named_action_set_property;
  gobject_class->get_property = gtk_named_action_get_property;
  gobject_class->finalize = gtk_named_action_finalize;

  action_class->activate = gtk_named_action_activate;
  action_class->print = gtk_named_action_print;

  /**
   * GtkNamedAction:action-name: (attributes org.gtk.Property.get=gtk_named_action_get_action_name)
   *
   * The name of the action to activate.
   */
  named_props[NAMED_PROP_ACTION_NAME] =
    g_param_spec_string (I_("action-name"), NULL, NULL,
                         NULL,
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, NAMED_N_PROPS, named_props);
}

static void
gtk_named_action_init (GtkNamedAction *self)
{
}

/**
 * gtk_named_action_new:
 * @name: the detailed name of the action
 *
 * Creates an action that when activated, activates
 * the named action on the widget.
 *
 * It also passes the given arguments to it.
 *
 * See [method@Gtk.Widget.insert_action_group] for
 * how to add actions to widgets.
 *
 * Returns: (transfer full) (type GtkNamedAction): a new `GtkShortcutAction`
 */
GtkShortcutAction *
gtk_named_action_new (const char *name)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GTK_TYPE_NAMED_ACTION,
                       "action-name", name,
                       NULL);
}

/**
 * gtk_named_action_get_action_name: (attributes org.gtk.Method.get_property=action-name)
 * @self: a named action
 *
 * Returns the name of the action that will be activated.
 *
 * Returns: the name of the action to activate
 */
const char *
gtk_named_action_get_action_name (GtkNamedAction *self)
{
  g_return_val_if_fail (GTK_IS_NAMED_ACTION (self), NULL);

  return self->name;
}
