/*
 * Copyright © 2019 Benjamin Otte
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

#include "config.h"

#include "gtkbuilderscopeprivate.h"

#include "gtkbuilder.h"
#include "gtktestutils.h"

/**
 * GtkBuilderScope:
 *
 * `GtkBuilderScope` is an interface to provide language binding support
 * to `GtkBuilder`.
 *
 * The goal of `GtkBuilderScope` is to look up programming-language-specific
 * values for strings that are given in a `GtkBuilder` UI file.
 *
 * The primary intended audience is bindings that want to provide deeper
 * integration of `GtkBuilder` into the language.
 *
 * A `GtkBuilderScope` instance may be used with multiple `GtkBuilder` objects,
 * even at once.
 *
 * By default, GTK will use its own implementation of `GtkBuilderScope`
 * for the C language which can be created via [ctor@Gtk.BuilderCScope.new].
 *
 * If you implement `GtkBuilderScope` for a language binding, you
 * may want to (partially) derive from or fall back to a [class@Gtk.BuilderCScope],
 * as that class implements support for automatic lookups from C symbols.
 */

/**
 * GtkBuilderCScope:
 *
 * A `GtkBuilderScope` implementation for the C language.
 *
 * `GtkBuilderCScope` instances use symbols explicitly added to @builder
 * with prior calls to [method@Gtk.BuilderCScope.add_callback_symbol].
 * If developers want to do that, they are encouraged to create their
 * own scopes for that purpose.
 *
 * In the case that symbols are not explicitly added; GTK will uses
 * `GModule`’s introspective features (by opening the module %NULL) to
 * look at the application’s symbol table. From here it tries to match
 * the signal function names given in the interface description with
 * symbols in the application.
 *
 * Note that unless [method@Gtk.BuilderCScope.add_callback_symbol] is
 * called for all signal callbacks which are referenced by the loaded XML,
 * this functionality will require that `GModule` be supported on the platform.
 */


G_DEFINE_INTERFACE (GtkBuilderScope, gtk_builder_scope, G_TYPE_OBJECT)

static GType
gtk_builder_scope_default_get_type_from_name (GtkBuilderScope *self,
                                              GtkBuilder      *builder,
                                              const char      *type_name)
{
  GType type;

  type = g_type_from_name (type_name);
  if (type != G_TYPE_INVALID)
    return type;

  gtk_test_register_all_types ();
  return g_type_from_name (type_name);
}

static GType
gtk_builder_scope_default_get_type_from_function (GtkBuilderScope *self,
                                                  GtkBuilder      *builder,
                                                  const char      *type_name)
{
  return G_TYPE_INVALID;
}

static GClosure *
gtk_builder_scope_default_create_closure (GtkBuilderScope        *self,
                                          GtkBuilder             *builder,
                                          const char             *function_name,
                                          GtkBuilderClosureFlags  flags,
                                          GObject                *object,
                                          GError                **error)
{
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_INVALID_FUNCTION,
               "Creating closures is not supported by %s",
               G_OBJECT_TYPE_NAME (self));
  return NULL;
}

static void
gtk_builder_scope_default_init (GtkBuilderScopeInterface *iface)
{
  iface->get_type_from_name = gtk_builder_scope_default_get_type_from_name;
  iface->get_type_from_function = gtk_builder_scope_default_get_type_from_function;
  iface->create_closure = gtk_builder_scope_default_create_closure;
}

GType
gtk_builder_scope_get_type_from_name (GtkBuilderScope *self,
                                      GtkBuilder      *builder,
                                      const char      *type_name)
{
  g_return_val_if_fail (GTK_IS_BUILDER_SCOPE (self), G_TYPE_INVALID);
  g_return_val_if_fail (GTK_IS_BUILDER (builder), G_TYPE_INVALID);
  g_return_val_if_fail (type_name != NULL, G_TYPE_INVALID);

  return GTK_BUILDER_SCOPE_GET_IFACE (self)->get_type_from_name (self, builder, type_name);
}

GType
gtk_builder_scope_get_type_from_function (GtkBuilderScope *self,
                                          GtkBuilder      *builder,
                                          const char      *function_name)
{
  g_return_val_if_fail (GTK_IS_BUILDER_SCOPE (self), G_TYPE_INVALID);
  g_return_val_if_fail (GTK_IS_BUILDER (builder), G_TYPE_INVALID);
  g_return_val_if_fail (function_name != NULL, G_TYPE_INVALID);

  return GTK_BUILDER_SCOPE_GET_IFACE (self)->get_type_from_function (self, builder, function_name);
}

GClosure *
gtk_builder_scope_create_closure (GtkBuilderScope        *self,
                                  GtkBuilder             *builder,
                                  const char             *function_name,
                                  GtkBuilderClosureFlags  flags,
                                  GObject                *object,
                                  GError                **error)
{
  g_return_val_if_fail (GTK_IS_BUILDER_SCOPE (self), NULL);
  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (function_name != NULL, NULL);
  g_return_val_if_fail (object == NULL || G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GTK_BUILDER_SCOPE_GET_IFACE (self)->create_closure (self, builder, function_name, flags, object, error);
}

/*** GTK_BUILDER_CSCOPE ***/

typedef struct _GtkBuilderCScopePrivate GtkBuilderCScopePrivate;

struct _GtkBuilderCScopePrivate
{
  GModule *module;
  GHashTable *callbacks;
};

static void gtk_builder_cscope_scope_init (GtkBuilderScopeInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkBuilderCScope, gtk_builder_cscope, G_TYPE_OBJECT,
                         G_ADD_PRIVATE(GtkBuilderCScope)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDER_SCOPE,
                                                gtk_builder_cscope_scope_init))

static GModule *
gtk_builder_cscope_get_module (GtkBuilderCScope *self)
{
  GtkBuilderCScopePrivate *priv = gtk_builder_cscope_get_instance_private (self);

  if (priv->module == NULL)
    {
      if (!g_module_supported ())
        return NULL;

      priv->module = g_module_open (NULL, G_MODULE_BIND_LAZY);
    }

  return priv->module;
}

/*
 * Try to map a type name to a _get_type function
 * and call it, eg:
 *
 * GtkWindow -> gtk_window_get_type
 * GtkHBox -> gtk_hbox_get_type
 * GtkUIManager -> gtk_ui_manager_get_type
 * GWeatherLocation -> gweather_location_get_type (split_first_cap == FALSE)
 * GThemedIcon -> g_themed_icon_get_type (slit_first_cap == TRUE)
 *
 * Keep in sync with testsuite/gtk/typename.c !
 */
static char *
type_name_mangle (const char *name,
                  gboolean     split_first_cap)
{
  GString *symbol_name = g_string_new ("");
  int i;

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
             ((i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
              (i == 1 && name[0] == g_ascii_toupper (name[0]) && split_first_cap))) ||
           (i > 2 && name[i]  == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }
  g_string_append (symbol_name, "_get_type");

  return g_string_free (symbol_name, FALSE);
}

static GType
gtk_builder_cscope_resolve_type_lazily (GtkBuilderCScope *self,
                                        const char       *name)
{
  GModule *module;
  GType (*func) (void);
  char *symbol;
  GType gtype = G_TYPE_INVALID;

  module = gtk_builder_cscope_get_module (self);
  if (!module)
    return G_TYPE_INVALID;

  symbol = type_name_mangle (name, TRUE);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();

  g_free (symbol);

  symbol = type_name_mangle (name, FALSE);

  if (g_module_symbol (module, symbol, (gpointer)&func))
    gtype = func ();

  g_free (symbol);

  return gtype;
}

static GType
gtk_builder_cscope_get_type_from_name (GtkBuilderScope *scope,
                                       GtkBuilder      *builder,
                                       const char      *type_name)
{
  GtkBuilderCScope *self = GTK_BUILDER_CSCOPE (scope);
  GType type;

  type = g_type_from_name (type_name);
  if (type != G_TYPE_INVALID)
    return type;

  type = gtk_builder_cscope_resolve_type_lazily (self, type_name);
  if (type != G_TYPE_INVALID)
    return type;

  gtk_test_register_all_types ();
  type = g_type_from_name (type_name);

  return type;
}

static GCallback
gtk_builder_cscope_get_callback (GtkBuilderCScope  *self,
                                 const char        *function_name,
                                 GError           **error)
{
  GModule *module;
  GCallback func;

  func = gtk_builder_cscope_lookup_callback_symbol (self, function_name);
  if (func)
    return func;

  module = gtk_builder_cscope_get_module (self);
  if (module == NULL)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_FUNCTION,
                   "Could not look up function `%s`: GModule is not supported.",
                   function_name);
      return NULL;
    }

  if (!g_module_symbol (module, function_name, (gpointer)&func))
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_FUNCTION,
                   "No function named `%s`.",
                   function_name);
      return NULL;
    }

  return func;
}

static GType
gtk_builder_cscope_get_type_from_function (GtkBuilderScope *scope,
                                           GtkBuilder      *builder,
                                           const char      *function_name)
{
  GtkBuilderCScope *self = GTK_BUILDER_CSCOPE (scope);
  GType (* type_func) (void); 
  
  type_func = (GType (*) (void)) gtk_builder_cscope_get_callback (self, function_name, NULL);
  if (!type_func)
    return G_TYPE_INVALID;

  return type_func();
}

static GClosure *
gtk_builder_cscope_create_closure_for_funcptr (GtkBuilderCScope *self,
                                               GtkBuilder       *builder,
                                               GCallback         callback,
                                               gboolean          swapped,
                                               GObject          *object)
{
  GClosure *closure;

  if (object == NULL)
    object = gtk_builder_get_current_object (builder);

  if (object)
    {
      if (swapped)
        closure = g_cclosure_new_object_swap (callback, object);
      else
        closure = g_cclosure_new_object (callback, object);
    }
  else
    {
      if (swapped)
        closure = g_cclosure_new_swap (callback, NULL, NULL);
      else
        closure = g_cclosure_new (callback, NULL, NULL);
    }

  return closure;
}

static GClosure *
gtk_builder_cscope_create_closure (GtkBuilderScope        *scope,
                                   GtkBuilder             *builder,
                                   const char             *function_name,
                                   GtkBuilderClosureFlags  flags,
                                   GObject                *object,
                                   GError                **error)
{
  GtkBuilderCScope *self = GTK_BUILDER_CSCOPE (scope);
  GCallback func;
  gboolean swapped;

  swapped = flags & GTK_BUILDER_CLOSURE_SWAPPED;

  func = gtk_builder_cscope_get_callback (self, function_name, error);
  if (!func)
      return NULL;

  return gtk_builder_cscope_create_closure_for_funcptr (self, builder, func, swapped, object);
}

static void
gtk_builder_cscope_scope_init (GtkBuilderScopeInterface *iface)
{
  iface->get_type_from_name = gtk_builder_cscope_get_type_from_name;
  iface->get_type_from_function = gtk_builder_cscope_get_type_from_function;
  iface->create_closure = gtk_builder_cscope_create_closure;
}

static void
gtk_builder_cscope_finalize (GObject *object)
{
  GtkBuilderCScope *self = GTK_BUILDER_CSCOPE (object);
  GtkBuilderCScopePrivate *priv = gtk_builder_cscope_get_instance_private (self);

  g_clear_pointer (&priv->callbacks, g_hash_table_destroy);
  g_clear_pointer (&priv->module, g_module_close);

  G_OBJECT_CLASS (gtk_builder_cscope_parent_class)->finalize (object);
}
  
static void
gtk_builder_cscope_class_init (GtkBuilderCScopeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_builder_cscope_finalize;
}

static void
gtk_builder_cscope_init (GtkBuilderCScope *self)
{
}

/**
 * gtk_builder_cscope_new:
 *
 * Creates a new `GtkBuilderCScope` object to use with future
 * `GtkBuilder` instances.
 *
 * Calling this function is only necessary if you want to add
 * custom callbacks via [method@Gtk.BuilderCScope.add_callback_symbol].
 *
 * Returns: (transfer full) (type GtkBuilderCScope): a new `GtkBuilderCScope`
 */
GtkBuilderScope *
gtk_builder_cscope_new (void)
{
  return g_object_new (GTK_TYPE_BUILDER_CSCOPE, NULL);
}

/**
 * gtk_builder_cscope_add_callback:
 * @self: a `GtkBuilderCScope`
 * @callback_symbols: (scope async): The callback pointer
 *
 * Adds the @callback_symbol to the scope of @builder under its
 * own name.
 *
 * This is a convenience wrapper of [method@Gtk.BuilderCScope.add_callback_symbol].
 *
 * Since: 4.8
 */

/**
 * gtk_builder_cscope_add_callback_symbol:
 * @self: a `GtkBuilderCScope`
 * @callback_name: The name of the callback, as expected in the XML
 * @callback_symbol: (scope async): The callback pointer
 *
 * Adds the @callback_symbol to the scope of @builder under the
 * given @callback_name.
 *
 * Using this function overrides the behavior of
 * [method@Gtk.Builder.create_closure] for any callback symbols that
 * are added. Using this method allows for better encapsulation as it
 * does not require that callback symbols be declared in the global
 * namespace.
 */
void
gtk_builder_cscope_add_callback_symbol (GtkBuilderCScope *self,
                                        const char       *callback_name,
                                        GCallback         callback_symbol)
{
  GtkBuilderCScopePrivate *priv = gtk_builder_cscope_get_instance_private (self);

  g_return_if_fail (GTK_IS_BUILDER_CSCOPE (self));
  g_return_if_fail (callback_name && callback_name[0]);
  g_return_if_fail (callback_symbol != NULL);

  if (!priv->callbacks)
    priv->callbacks = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, NULL);

  g_hash_table_insert (priv->callbacks, g_strdup (callback_name), callback_symbol);
}

/**
 * gtk_builder_cscope_add_callback_symbols: (skip)
 * @self: a `GtkBuilderCScope`
 * @first_callback_name: The name of the callback, as expected in the XML
 * @first_callback_symbol: (scope async): The callback pointer
 * @...: A list of callback name and callback symbol pairs terminated with %NULL
 *
 * A convenience function to add many callbacks.
 *
 * This is equivalent to calling [method@Gtk.BuilderCScope.add_callback_symbol]
 * for each symbol.
 */
void
gtk_builder_cscope_add_callback_symbols (GtkBuilderCScope *self,
                                         const char       *first_callback_name,
                                         GCallback         first_callback_symbol,
                                         ...)
{
  va_list var_args;
  const char *callback_name;
  GCallback callback_symbol;

  g_return_if_fail (GTK_IS_BUILDER_CSCOPE (self));
  g_return_if_fail (first_callback_name && first_callback_name[0]);
  g_return_if_fail (first_callback_symbol != NULL);

  callback_name = first_callback_name;
  callback_symbol = first_callback_symbol;

  va_start (var_args, first_callback_symbol);

  do {

    gtk_builder_cscope_add_callback_symbol (self, callback_name, callback_symbol);

    callback_name = va_arg (var_args, const char *);

    if (callback_name)
      callback_symbol = va_arg (var_args, GCallback);

  } while (callback_name != NULL);

  va_end (var_args);
}

/**
 * gtk_builder_cscope_lookup_callback_symbol: (skip)
 * @self: a `GtkBuilderCScope`
 * @callback_name: The name of the callback
 *
 * Fetches a symbol previously added with
 * gtk_builder_cscope_add_callback_symbol().
 *
 * Returns: (nullable) (transfer none): The callback symbol
 *   in @builder for @callback_name
 */
GCallback
gtk_builder_cscope_lookup_callback_symbol (GtkBuilderCScope *self,
                                           const char       *callback_name)
{
  GtkBuilderCScopePrivate *priv = gtk_builder_cscope_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_BUILDER_CSCOPE (self), NULL);
  g_return_val_if_fail (callback_name && callback_name[0], NULL);

  if (priv->callbacks == NULL)
    return NULL;

  return g_hash_table_lookup (priv->callbacks, callback_name);
}

