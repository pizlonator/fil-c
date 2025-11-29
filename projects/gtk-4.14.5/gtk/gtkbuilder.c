/* GTK - The GIMP Toolkit
 * Copyright (C) 1998-2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>,
 *                         Henrique Romano <henrique@async.com.br>
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

/**
 * GtkBuilder:
 *
 * A `GtkBuilder` reads XML descriptions of a user interface and
 * instantiates the described objects.
 *
 * To create a `GtkBuilder` from a user interface description, call
 * [ctor@Gtk.Builder.new_from_file], [ctor@Gtk.Builder.new_from_resource]
 * or [ctor@Gtk.Builder.new_from_string].
 *
 * In the (unusual) case that you want to add user interface
 * descriptions from multiple sources to the same `GtkBuilder` you can
 * call [ctor@Gtk.Builder.new] to get an empty builder and populate it by
 * (multiple) calls to [method@Gtk.Builder.add_from_file],
 * [method@Gtk.Builder.add_from_resource] or
 * [method@Gtk.Builder.add_from_string].
 *
 * A `GtkBuilder` holds a reference to all objects that it has constructed
 * and drops these references when it is finalized. This finalization can
 * cause the destruction of non-widget objects or widgets which are not
 * contained in a toplevel window. For toplevel windows constructed by a
 * builder, it is the responsibility of the user to call
 * [method@Gtk.Window.destroy] to get rid of them and all the widgets
 * they contain.
 *
 * The functions [method@Gtk.Builder.get_object] and
 * [method@Gtk.Builder.get_objects] can be used to access the widgets in
 * the interface by the names assigned to them inside the UI description.
 * Toplevel windows returned by these functions will stay around until the
 * user explicitly destroys them with [method@Gtk.Window.destroy]. Other
 * widgets will either be part of a larger hierarchy constructed by the
 * builder (in which case you should not have to worry about their lifecycle),
 * or without a parent, in which case they have to be added to some container
 * to make use of them. Non-widget objects need to be reffed with
 * g_object_ref() to keep them beyond the lifespan of the builder.
 *
 * ## GtkBuilder UI Definitions
 *
 * `GtkBuilder` parses textual descriptions of user interfaces which are
 * specified in XML format. We refer to these descriptions as “GtkBuilder
 * UI definitions” or just “UI definitions” if the context is clear.
 *
 * ### Structure of UI definitions
 *
 * UI definition files are always encoded in UTF-8.
 *
 * The toplevel element is `<interface>`. It optionally takes a “domain”
 * attribute, which will make the builder look for translated strings
 * using `dgettext()` in the domain specified. This can also be done by
 * calling [method@Gtk.Builder.set_translation_domain] on the builder.
 * For example:
 *
 * ```xml
 * <?xml version="1.0" encoding="UTF-8">
 * <interface domain="your-app">
 *   ...
 * </interface>
 * ```
 *
 * ### Requirements
 *
 * The target toolkit version(s) are described by `<requires>` elements,
 * the “lib” attribute specifies the widget library in question (currently
 * the only supported value is “gtk”) and the “version” attribute specifies
 * the target version in the form “`<major>`.`<minor>`”. `GtkBuilder` will
 * error out if the version requirements are not met. For example:
 *
 * ```xml
 * <?xml version="1.0" encoding="UTF-8">
 * <interface domain="your-app">
 *   <requires lib="gtk" version="4.0" />
 * </interface>
 * ```
 *
 * ### Objects
 *
 * Objects are defined as children of the `<interface>` element.
 *
 * Objects are described by `<object>` elements, which can contain
 * `<property>` elements to set properties, `<signal>` elements which
 * connect signals to handlers, and `<child>` elements, which describe
 * child objects.
 *
 * Typically, the specific kind of object represented by an `<object>`
 * element is specified by the “class” attribute. If the type has not
 * been loaded yet, GTK tries to find the `get_type()` function from the
 * class name by applying heuristics. This works in most cases, but if
 * necessary, it is possible to specify the name of the `get_type()`
 * function explicitly with the "type-func" attribute. If your UI definition
 * is referencing internal types, you should make sure to call
 * `g_type_ensure()` for each object type before parsing the UI definition.
 *
 * Objects may be given a name with the “id” attribute, which allows the
 * application to retrieve them from the builder with
 * [method@Gtk.Builder.get_object]. An id is also necessary to use the
 * object as property value in other parts of the UI definition. GTK
 * reserves ids starting and ending with `___` (three consecutive
 * underscores) for its own purposes.
 *
 * ### Properties
 *
 * Setting properties of objects is pretty straightforward with the
 * `<property>` element: the “name” attribute specifies the name of the
 * property, and the content of the element specifies the value:
 *
 * ```xml
 * <object class="GtkButton">
 *   <property name="label">Hello, world</property>
 * </object>
 * ```
 *
 * If the “translatable” attribute is set to a true value, GTK uses
 * `gettext()` (or `dgettext()` if the builder has a translation domain set)
 * to find a translation for the value. This happens before the value
 * is parsed, so it can be used for properties of any type, but it is
 * probably most useful for string properties. It is also possible to
 * specify a context to disambiguate short strings, and comments which
 * may help the translators:
 *
 * ```xml
 * <object class="GtkButton">
 *   <property name="label" translatable="yes" context="button">Hello, world</property>
 * </object>
 * ```
 *
 * `GtkBuilder` can parse textual representations for the most common
 * property types:
 *
 * - characters
 * - strings
 * - integers
 * - floating-point numbers
 * - booleans (strings like “TRUE”, “t”, “yes”, “y”, “1” are interpreted
 *   as true values, strings like “FALSE”, “f”, “no”, “n”, “0” are interpreted
 *   as false values)
 * - enumeration types (can be specified by their full C identifier their short
 *   name used when registering the enumeration type, or their integer value)
 * - flag types (can be specified by their C identifier, short name, integer
 *   value, and optionally combined with “|” for bitwise OR, e.g.
 *   “GTK_INPUT_HINT_EMOJI|GTK_INPUT_HINT_LOWERCASE”, or “emoji|lowercase”)
 * - colors (in a format understood by [method@Gdk.RGBA.parse])
 * - `GVariant` (can be specified in the format understood by
 *    [func@GLib.Variant.parse])
 * - pixbufs (can be specified as a filename of an image file to load)
 *
 * Objects can be referred to by their name and by default refer to
 * objects declared in the local XML fragment and objects exposed via
 * [method@Gtk.Builder.expose_object]. In general, `GtkBuilder` allows
 * forward references to objects declared in the local XML; an object
 * doesn’t have to be constructed before it can be referred to. The
 * exception to this rule is that an object has to be constructed before
 * it can be used as the value of a construct-only property.
 *
 * ### Child objects
 *
 * Many widgets have properties for child widgets, such as
 * [property@Gtk.Expander:child]. In this case, the preferred way to
 * specify the child widget in a ui file is to simply set the property:
 *
 * ```xml
 * <object class="GtkExpander">
 *   <property name="child">
 *     <object class="GtkLabel">
 *     ...
 *     </object>
 *   </property>
 * </object>
 * ```
 *
 * Generic containers that can contain an arbitrary number of children,
 * such as [class@Gtk.Box] instead use the `<child>` element. A `<child>`
 * element contains an `<object>` element which describes the child object.
 * Most often, child objects are widgets inside a container, but they can
 * also be, e.g., actions in an action group, or columns in a tree model.
 *
 * Any object type that implements the [iface@Gtk.Buildable] interface can
 * specify how children may be added to it. Since many objects and widgets that
 * are included with GTK already implement the `GtkBuildable` interface,
 * typically child objects can be added using the `<child>` element without
 * having to be concerned about the underlying implementation.
 *
 * See the [`GtkWidget` documentation](class.Widget.html#gtkwidget-as-gtkbuildable)
 * for many examples of using `GtkBuilder` with widgets, including setting
 * child objects using the `<child>` element.
 *
 * A noteworthy special case to the general rule that only objects implementing
 * `GtkBuildable` may specify how to handle the `<child>` element is that
 * `GtkBuilder` provides special support for adding objects to a
 * [class@Gio.ListStore] by using the `<child>` element. For instance:
 *
 * ```xml
 * <object class="GListStore">
 *   <property name="item-type">MyObject</property>
 *   <child>
 *     <object class="MyObject" />
 *   </child>
 *   ...
 * </object>
 * ```
 *
 * ### Property bindings
 *
 * It is also possible to bind a property value to another object's
 * property value using the attributes "bind-source" to specify the
 * source object of the binding, and optionally, "bind-property" and
 * "bind-flags" to specify the source property and source binding flags
 * respectively. Internally, `GtkBuilder` implements this using
 * [class@GObject.Binding] objects.
 *
 * For instance, in the example below the “label” property of the
 * `bottom_label` widget is bound to the “label” property of the
 * `top_button` widget:
 *
 * ```xml
 * <object class="GtkBox">
 *   <property name="orientation">vertical</property>
 *   <child>
 *     <object class="GtkButton" id="top_button">
 *       <property name="label">Hello, world</property>
 *     </object>
 *   </child>
 *   <child>
 *     <object class="GtkLabel" id="bottom_label">
 *       <property name="label"
 *                 bind-source="top_button"
 *                 bind-property="label"
 *                 bind-flags="sync-create" />
 *     </object>
 *   </child>
 * </object>
 * ```
 *
 * For more information, see the documentation of the
 * [method@GObject.Object.bind_property] method.
 *
 * Please note that another way to set up bindings between objects in .ui files
 * is to use the `GtkExpression` methodology. See the
 * [`GtkExpression` documentation](class.Expression.html#gtkexpression-in-ui-files)
 * for more information.
 *
 * ### Internal children
 *
 * Sometimes it is necessary to refer to widgets which have implicitly
 * been constructed by GTK as part of a composite widget, to set
 * properties on them or to add further children (e.g. the content area
 * of a `GtkDialog`). This can be achieved by setting the “internal-child”
 * property of the `<child>` element to a true value. Note that `GtkBuilder`
 * still requires an `<object>` element for the internal child, even if it
 * has already been constructed.
 *
 * ### Specialized children
 *
 * A number of widgets have different places where a child can be added
 * (e.g. tabs vs. page content in notebooks). This can be reflected in
 * a UI definition by specifying the “type” attribute on a `<child>`
 * The possible values for the “type” attribute are described in the
 * sections describing the widget-specific portions of UI definitions.
 *
 * ### Signal handlers and function pointers
 *
 * Signal handlers are set up with the `<signal>` element. The “name”
 * attribute specifies the name of the signal, and the “handler” attribute
 * specifies the function to connect to the signal.
 *
 * ```xml
 * <object class="GtkButton" id="hello_button">
 *   <signal name="clicked" handler="hello_button__clicked" />
 * </object>
 * ```
 *
 * The remaining attributes, “after”, “swapped” and “object”, have the
 * same meaning as the corresponding parameters of the
 * [func@GObject.signal_connect_object] or [func@GObject.signal_connect_data]
 * functions:
 *
 * - “after” matches the `G_CONNECT_AFTER` flag, and will ensure that the
 *   handler is called after the default class closure for the signal
 * - “swapped” matches the `G_CONNECT_SWAPPED` flag, and will swap the
 *   instance and closure arguments when invoking the signal handler
 * - “object” will bind the signal handler to the lifetime of the object
 *   referenced by the attribute
 *
 * By default "swapped" will be set to "yes" if not specified otherwise, in
 * the case where "object" is set, for convenience. A “last_modification_time”
 * attribute is also allowed, but it does not have a meaning to the builder.
 *
 * When compiling applications for Windows, you must declare signal callbacks
 * with the `G_MODULE_EXPORT` decorator, or they will not be put in the symbol
 * table:
 *
 * ```c
 * G_MODULE_EXPORT void
 * hello_button__clicked (GtkButton *button,
 *                        gpointer data)
 * {
 *   // ...
 * }
 * ```
 *
 * On Linux and Unix, this is not necessary; applications should instead
 * be compiled with the `-Wl,--export-dynamic` argument inside their compiler
 * flags, and linked against `gmodule-export-2.0`.
 *
 * ## Example UI Definition
 *
 * ```xml
 * <interface>
 *   <object class="GtkDialog" id="dialog1">
 *     <child internal-child="content_area">
 *       <object class="GtkBox">
 *         <child internal-child="action_area">
 *           <object class="GtkBox">
 *             <child>
 *               <object class="GtkButton" id="ok_button">
 *                 <property name="label" translatable="yes">_Ok</property>
 *                 <property name="use-underline">True</property>
 *                 <signal name="clicked" handler="ok_button_clicked"/>
 *               </object>
 *             </child>
 *           </object>
 *         </child>
 *       </object>
 *     </child>
 *   </object>
 * </interface>
 * ```
 *
 * ## Using GtkBuildable for extending UI definitions
 *
 * Objects can implement the [iface@Gtk.Buildable] interface to add custom
 * elements and attributes to the XML. Typically, any extension will be
 * documented in each type that implements the interface.
 *
 * ## Templates
 *
 * When describing a [class@Gtk.Widget], you can use the `<template>` tag to
 * describe a UI bound to a specific widget type. GTK will automatically load
 * the UI definition when instantiating the type, and bind children and
 * signal handlers to instance fields and function symbols.
 *
 * For more information, see the [`GtkWidget` documentation](class.Widget.html#building-composite-widgets-from-template-xml)
 * for details.
 */

#include "config.h"
#include <errno.h> /* errno */
#include <stdlib.h>
#include <string.h> /* strlen */

#include "gtkbuilderprivate.h"

#include "gtkbuildableprivate.h"
#include "gtkbuilderlistitemfactory.h"
#include "gtkbuilderscopeprivate.h"
#include "gtkdebug.h"
#include "gtkexpression.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkshortcutactionprivate.h"
#include "gtkshortcuttrigger.h"
#include "gtktypebuiltins.h"
#include "gtkiconthemeprivate.h"
#include "gtkdebug.h"


static void gtk_builder_finalize       (GObject         *object);
static void gtk_builder_set_property   (GObject         *object,
                                        guint            prop_id,
                                        const GValue    *value,
                                        GParamSpec      *pspec);
static void gtk_builder_get_property   (GObject         *object,
                                        guint            prop_id,
                                        GValue          *value,
                                        GParamSpec      *pspec);

enum {
  PROP_0,
  PROP_CURRENT_OBJECT,
  PROP_SCOPE,
  PROP_TRANSLATION_DOMAIN,
  LAST_PROP
};

static GParamSpec *builder_props[LAST_PROP];

struct _GtkBuilder
{
  GObject parent_instance;
};

struct _GtkBuilderClass
{
  GObjectClass parent_class;
};

typedef struct
{
  char *domain;
  GHashTable *objects;
  GSList *delayed_properties;
  GPtrArray *signals;
  GSList *bindings;
  char *filename;
  char *resource_prefix;
  GType template_type;
  gboolean allow_template_parents;
  GObject *current_object;
  GtkBuilderScope *scope;
} GtkBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkBuilder, gtk_builder, G_TYPE_OBJECT)

static void
gtk_builder_dispose (GObject *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (GTK_BUILDER (object));

  g_clear_object (&priv->current_object);
  g_clear_object (&priv->scope);

  G_OBJECT_CLASS (gtk_builder_parent_class)->dispose (object);
}

static void
gtk_builder_class_init (GtkBuilderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_builder_dispose;
  gobject_class->finalize = gtk_builder_finalize;
  gobject_class->set_property = gtk_builder_set_property;
  gobject_class->get_property = gtk_builder_get_property;

 /**
  * GtkBuilder:translation-domain: (attributes org.gtk.Property.get=gtk_builder_get_translation_domain org.gtk.Property.set=gtk_builder_set_translation_domain)
  *
  * The translation domain used when translating property values that
  * have been marked as translatable.
  *
  * If the translation domain is %NULL, `GtkBuilder` uses gettext(),
  * otherwise g_dgettext().
  */
  builder_props[PROP_TRANSLATION_DOMAIN] =
      g_param_spec_string ("translation-domain", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

 /**
  * GtkBuilder:current-object: (attributes org.gtk.Property.get=gtk_builder_get_current_object org.gtk.Property.set=gtk_builder_set_current_object)
  *
  * The object the builder is evaluating for.
  */
  builder_props[PROP_CURRENT_OBJECT] =
      g_param_spec_object ("current-object", NULL, NULL,
                           G_TYPE_OBJECT,
                           GTK_PARAM_READWRITE);

 /**
  * GtkBuilder:scope: (attributes org.gtk.Property.get=gtk_builder_get_scope org.gtk.Property.set=gtk_builder_set_scope)
  *
  * The scope the builder is operating in
  */
  builder_props[PROP_SCOPE] =
      g_param_spec_object ("scope", NULL, NULL,
                           GTK_TYPE_BUILDER_SCOPE,
                           GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (gobject_class, LAST_PROP, builder_props);
}

static void
gtk_builder_init (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  priv->domain = NULL;
  priv->objects = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}


/*
 * GObject virtual methods
 */

static void
gtk_builder_finalize (GObject *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (GTK_BUILDER (object));

  g_free (priv->domain);
  g_free (priv->filename);
  g_free (priv->resource_prefix);

  if (GTK_DEBUG_CHECK (BUILDER_OBJECTS))
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, priv->objects);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          if (G_OBJECT (value)->ref_count == 1)
            g_message ("builder: %s with id %s unused",
                       G_OBJECT_TYPE_NAME (value), (const char *)key);
        }
    }

  g_hash_table_destroy (priv->objects);
  if (priv->signals)
    g_ptr_array_free (priv->signals, TRUE);

  G_OBJECT_CLASS (gtk_builder_parent_class)->finalize (object);
}

static void
gtk_builder_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GtkBuilder *builder = GTK_BUILDER (object);

  switch (prop_id)
    {
    case PROP_CURRENT_OBJECT:
      gtk_builder_set_current_object (builder, g_value_get_object (value));
      break;

    case PROP_SCOPE:
      gtk_builder_set_scope (builder, g_value_get_object (value));
      break;

    case PROP_TRANSLATION_DOMAIN:
      gtk_builder_set_translation_domain (builder, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_builder_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkBuilder *builder = GTK_BUILDER (object);
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  switch (prop_id)
    {
    case PROP_CURRENT_OBJECT:
      g_value_set_object (value, priv->current_object);
      break;

    case PROP_SCOPE:
      g_value_set_object (value, priv->scope);
      break;

    case PROP_TRANSLATION_DOMAIN:
      g_value_set_string (value, priv->domain);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*
 * GtkBuilder virtual methods
 */

typedef struct
{
  char *object;
  GParamSpec *pspec;
  char *value;
  int line;
  int col;
} DelayedProperty;

typedef struct
{
  GPtrArray *names;
  GArray *values;
} ObjectProperties;


static void
object_properties_init (ObjectProperties *self)
{
  self->names = NULL;
  self->values = NULL;
}

static void
object_properties_destroy (ObjectProperties *self)
{
  if (self == NULL)
    return;

  if (self->names)
    g_ptr_array_unref (self->names);

  if (self->values)
    g_array_unref (self->values);
}

static void
object_properties_add (ObjectProperties *self,
                       const char       *name,
                       const GValue     *value)
{
  if (!self->names)
    {
      self->names = g_ptr_array_sized_new (8);
      self->values = g_array_sized_new (FALSE, FALSE, sizeof (GValue), 8);
      g_array_set_clear_func (self->values, (GDestroyNotify) g_value_unset);
    }

  g_ptr_array_add (self->names, (char *) name);
  g_array_append_vals (self->values, value, 1);

  g_assert (self->names->len == self->values->len);
}

static const char *
object_properties_get_name (const ObjectProperties *self,
                            guint                   idx)
{
  g_assert (self->names);

  return g_ptr_array_index (self->names, idx);
}

static GValue *
object_properties_get_value (const ObjectProperties *self,
                             guint                   idx)
{
  g_assert (self->values);

  return &g_array_index (self->values, GValue, idx);
}

static void
gtk_builder_get_parameters (GtkBuilder         *builder,
                            GType               object_type,
                            const char         *object_name,
                            GPtrArray          *properties,
                            GParamFlags         filter_flags,
                            ObjectProperties   *parameters,
                            ObjectProperties   *filtered_parameters)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  DelayedProperty *property;
  GError *error = NULL;

  if (!properties)
    return;

  for (guint i = 0; i < properties->len; i++)
    {
      PropertyInfo *prop = g_ptr_array_index (properties, i);
      const char *property_name = prop->pspec->name;
      GValue property_value = G_VALUE_INIT;
      ObjectProperties *params;

      if (prop->applied)
        continue;

      if ((prop->pspec->flags & filter_flags) != 0)
        params = filtered_parameters;
      else
        params = parameters;

      if (!params)
        continue;

      if (prop->value)
        {
          g_value_init (&property_value, G_PARAM_SPEC_VALUE_TYPE (prop->pspec));

          if (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) == GTK_TYPE_EXPRESSION)
            gtk_value_set_expression (&property_value, prop->value);
          else
            g_assert_not_reached ();
        }
      else if (prop->bound && (!prop->text || prop->text->len == 0))
        {
          /* Ignore properties with a binding and no value since they are
           * only there for to express the binding.
           */
          continue;
        }
      else if (G_IS_PARAM_SPEC_OBJECT (prop->pspec) &&
               (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GDK_TYPE_PIXBUF) &&
               (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GDK_TYPE_TEXTURE) &&
               (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GDK_TYPE_PAINTABLE) &&
               (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GTK_TYPE_SHORTCUT_TRIGGER) &&
               (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != GTK_TYPE_SHORTCUT_ACTION) &&
               (G_PARAM_SPEC_VALUE_TYPE (prop->pspec) != G_TYPE_FILE))
        {
          GObject *object = g_hash_table_lookup (priv->objects,
                                                 g_strstrip (prop->text->str));

          if (object)
            {
              g_value_init (&property_value, G_OBJECT_TYPE (object));
              g_value_set_object (&property_value, object);
            }
          else
            {
              if (prop->pspec->flags & G_PARAM_CONSTRUCT_ONLY)
                {
                  g_warning ("Failed to get construct only property "
                             "%s of %s with value '%s'",
                             prop->pspec->name, object_name, prop->text->str);
                  continue;
                }
              /* Delay setting property */

              prop->applied = TRUE;

              property = g_new (DelayedProperty, 1);
              property->pspec = prop->pspec;
              property->object = g_strdup (object_name);
              property->value = g_strdup (prop->text->str);
              property->line = prop->line;
              property->col = prop->col;
              priv->delayed_properties = g_slist_prepend (priv->delayed_properties,
                                                          property);
              continue;
            }
        }
      else if (!gtk_builder_value_from_string (builder, prop->pspec,
                                               prop->text->str,
                                               &property_value,
                                               &error))
        {
          g_warning ("Failed to set property %s.%s to %s: %s",
                     g_type_name (object_type), prop->pspec->name, prop->text->str,
                     error->message);
          g_error_free (error);
          error = NULL;
          continue;
        }

      /* At this point, property_value has been set, and we need to either
       * copy it to one of the two arrays, or unset it.
       */
      g_assert (G_IS_VALUE (&property_value));
      object_properties_add (params, property_name, &property_value);
      prop->applied = TRUE;
    }
}

static const char *
object_get_id (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_buildable_id (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-id");
}

static GObject *
gtk_builder_get_internal_child (GtkBuilder   *builder,
                                ObjectInfo   *info,
                                const char   *childname,
                                GError      **error)
{
  GObject *obj = NULL;

  while (!obj)
    {
      if (!info->parent)
        break;

      info = (ObjectInfo*)((ChildInfo*)info->parent)->parent;
      if (!info)
        break;

      GTK_DEBUG (BUILDER, "Trying to get internal child %s from %s",
                          childname, object_get_id (info->object));

      if (GTK_IS_BUILDABLE (info->object))
          obj = gtk_buildable_get_internal_child (GTK_BUILDABLE (info->object),
                                                  builder,
                                                  childname);
    };

  if (!obj)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_VALUE,
                   "Unknown internal child: %s", childname);
    }
  return obj;
}

static inline void
object_set_id (GObject     *object,
                 const char *id)
{
  if (GTK_IS_BUILDABLE (object))
    gtk_buildable_set_buildable_id (GTK_BUILDABLE (object), id);
  else
    g_object_set_data_full (object, "gtk-builder-id", g_strdup (id), g_free);
}

void
_gtk_builder_add_object (GtkBuilder  *builder,
                         const char *id,
                         GObject     *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  object_set_id (object, id);
  g_hash_table_insert (priv->objects, g_strdup (id), g_object_ref (object));
}

void
gtk_builder_take_bindings (GtkBuilder *builder,
                           GObject    *target,
                           GSList     *bindings)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l;

  for (l = bindings; l; l = l->next)
    {
      CommonInfo *common_info = l->data;

      if (common_info->tag_type == TAG_BINDING)
        {
          BindingInfo *info = l->data;
          info->target = target;
        }
      else if (common_info->tag_type == TAG_BINDING_EXPRESSION)
        {
          BindingExpressionInfo *info = l->data;
          info->target = target;
        }
      else
        {
          g_assert_not_reached ();
        }
    }

  priv->bindings = g_slist_concat (priv->bindings, bindings);
}

static void
ensure_special_construct_parameters (GtkBuilder       *builder,
                                     GType             object_type,
                                     ObjectProperties *construct_parameters)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GValue value = G_VALUE_INIT;

  if (g_type_is_a (object_type, GTK_TYPE_BUILDER_LIST_ITEM_FACTORY))
    {
      g_value_init (&value, GTK_TYPE_BUILDER_SCOPE);
      g_value_set_object (&value, priv->scope);
      object_properties_add (construct_parameters, "scope", &value);
    }
}

GObject *
_gtk_builder_construct (GtkBuilder  *builder,
                        ObjectInfo  *info,
                        GError     **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  ObjectProperties parameters, construct_parameters;
  GObject *obj;
  int i;
  GParamFlags param_filter_flags;

  g_assert (info->type != G_TYPE_INVALID);

  if (priv->template_type != 0 &&
      g_type_is_a (info->type, priv->template_type))
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_OBJECT_TYPE_REFUSED,
                   "Refused to build object of type '%s' because it "
                   "conforms to the template type '%s', avoiding infinite recursion.",
                   g_type_name (info->type), g_type_name (priv->template_type));
      return NULL;
    }

  /* If there is a manual constructor (like UIManager), or if this is a
   * reference to an internal child, then we filter out construct-only
   * and warn that they cannot be set.
   *
   * Otherwise if we are calling g_object_new_with_properties(), we want
   * to pass both G_PARAM_CONSTRUCT and G_PARAM_CONSTRUCT_ONLY to the
   * object's constructor.
   *
   * Passing all construct properties to g_object_new_with_properties()
   * slightly improves performance as the construct properties will only
   * be set once.
   */
  if (info->constructor ||
      (info->parent &&
       info->parent->tag_type == TAG_CHILD &&
       ((ChildInfo*)info->parent)->internal_child != NULL))
    param_filter_flags = G_PARAM_CONSTRUCT_ONLY;
  else
    param_filter_flags = G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY;

  object_properties_init (&parameters);
  object_properties_init (&construct_parameters);

  gtk_builder_get_parameters (builder, info->type,
                              info->id,
                              info->properties,
                              param_filter_flags,
                              &parameters,
                              &construct_parameters);

  if (info->constructor)
    {
      GObject *constructor;

      constructor = g_hash_table_lookup (priv->objects, info->constructor);
      if (constructor == NULL)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Unknown object constructor for %s: %s",
                       info->id,
                       info->constructor);
          object_properties_destroy (&parameters);
          object_properties_destroy (&construct_parameters);
          return NULL;
        }
      obj = gtk_buildable_construct_child (GTK_BUILDABLE (constructor),
                                           builder,
                                           info->id);
      g_assert (obj != NULL);
      if (construct_parameters.names->len > 0)
        g_warning ("Can't pass in construct-only parameters to %s", info->id);
    }
  else if (info->parent &&
           info->parent->tag_type == TAG_CHILD &&
           ((ChildInfo*)info->parent)->internal_child != NULL)
    {
      char *childname = ((ChildInfo*)info->parent)->internal_child;
      obj = gtk_builder_get_internal_child (builder, info, childname, error);
      if (!obj)
        {
          object_properties_destroy (&parameters);
          object_properties_destroy (&construct_parameters);
          return NULL;
        }
      if (construct_parameters.names)
        g_warning ("Can't pass in construct-only parameters to %s", childname);
      g_object_ref (obj);
    }
  else
    {
      ensure_special_construct_parameters (builder, info->type, &construct_parameters);

      if (construct_parameters.names)
        obj = g_object_new_with_properties (info->type,
                                            construct_parameters.names->len,
                                            (const char **) construct_parameters.names->pdata,
                                            (GValue *) construct_parameters.values->data);
      else
        obj = g_object_new (info->type, NULL);

      /* No matter what, make sure we have a reference.
       *
       * If it's an initially unowned object, sink it.
       * If it's not initially unowned then we have the reference already.
       *
       * In the case that this is a window it will be sunk already and
       * this is effectively a call to g_object_ref().  That's what
       * we want.
       */
      if (G_IS_INITIALLY_UNOWNED (obj))
        g_object_ref_sink (obj);

      GTK_DEBUG (BUILDER, "created %s of type %s", info->id, g_type_name (info->type));
    }
  object_properties_destroy (&construct_parameters);

  if (parameters.names)
    {
      GtkBuildableIface *iface = NULL;
      gboolean custom_set_property = FALSE;
      GtkBuildable *buildable = NULL;

      if (GTK_IS_BUILDABLE (obj))
        {
          buildable = GTK_BUILDABLE (obj);
          iface = GTK_BUILDABLE_GET_IFACE (obj);
          if (iface->set_buildable_property)
            custom_set_property = TRUE;
        }

      if (custom_set_property)
        {
          for (i = 0; i < parameters.names->len; i++)
            {
              const char *name = object_properties_get_name (&parameters, i);
              const GValue *value = object_properties_get_value (&parameters, i);

              iface->set_buildable_property (buildable, builder, name, value);
              if (GTK_DEBUG_CHECK (BUILDER))
                {
                  char *str = g_strdup_value_contents (value);
                  g_message ("set %s: %s = %s", info->id, name, str);
                  g_free (str);
                }
            }
        }
      else
        {
          g_object_setv (obj,
                         parameters.names->len,
                         (const char **) parameters.names->pdata,
                         (GValue *) parameters.values->data);
          if (GTK_DEBUG_CHECK (BUILDER))
            {
              for (i = 0; i < parameters.names->len; i++)
                {
                  const char *name = object_properties_get_name (&parameters, i);
                  const GValue *value = object_properties_get_value (&parameters, i);
                  char *str = g_strdup_value_contents (value);
                  g_message ("set %s: %s = %s", info->id, name, str);
                  g_free (str);
                }
            }
        }
    }

  object_properties_destroy (&parameters);

  /* put it in the hash table. */
  _gtk_builder_add_object (builder, info->id, obj);

  /* we already own a reference to obj. */
  g_object_unref (obj);

  return obj;
}

void
_gtk_builder_apply_properties (GtkBuilder  *builder,
                               ObjectInfo  *info,
                               GError     **error)
{
  ObjectProperties parameters;

  g_assert (info->object != NULL);
  g_assert (info->type != G_TYPE_INVALID);

  object_properties_init (&parameters);

  /* Fetch all properties that are not construct-only */
  gtk_builder_get_parameters (builder, info->type,
                              info->id,
                              info->properties,
                              G_PARAM_CONSTRUCT_ONLY,
                              &parameters, NULL);

  if (parameters.names)
    {
      GtkBuildableIface *iface = NULL;
      GtkBuildable *buildable = NULL;
      gboolean custom_set_property = FALSE;
      int i;

      if (GTK_IS_BUILDABLE (info->object))
        {
          buildable = GTK_BUILDABLE (info->object);
          iface = GTK_BUILDABLE_GET_IFACE (info->object);
          if (iface->set_buildable_property)
            custom_set_property = TRUE;
        }

      if (custom_set_property)
        {
          for (i = 0; i < parameters.names->len; i++)
            {
              const char *name = object_properties_get_name (&parameters, i);
              const GValue *value = object_properties_get_value (&parameters, i);
              iface->set_buildable_property (buildable, builder, name, value);
              if (GTK_DEBUG_CHECK (BUILDER))
                {
                  char *str = g_strdup_value_contents (value);
                  g_message ("set %s: %s = %s", info->id, name, str);
                  g_free (str);
                }
            }
        }
      else
        {
          g_object_setv (info->object,
                         parameters.names->len,
                         (const char **) parameters.names->pdata,
                         (GValue *) parameters.values->data);
          if (GTK_DEBUG_CHECK (BUILDER))
            {
              for (i = 0; i < parameters.names->len; i++)
                {
                  const char *name = object_properties_get_name (&parameters, i);
                  const GValue *value = object_properties_get_value (&parameters, i);
                  char *str = g_strdup_value_contents (value);
                  g_message ("set %s: %s = %s", info->id, name, str);
                  g_free (str);
                }
            }
        }
    }

  object_properties_destroy (&parameters);
}

void
_gtk_builder_add (GtkBuilder *builder,
                  ChildInfo  *child_info)
{
  GObject *object;
  GObject *parent;

  /* Internal children are already added
   * Also prevent us from being called twice.
   */
  if (!child_info ||
      child_info->internal_child ||
      child_info->added)
    return;

  object = child_info->object;
  if (!object)
    return;

  if (!child_info->parent)
    {
      g_warning ("%s: Not adding, No parent", object_get_id (object));
      return;
    }

  g_assert (object != NULL);

  parent = ((ObjectInfo*)child_info->parent)->object;

  GTK_DEBUG (BUILDER, "adding %s to %s", object_get_id (object), object_get_id (parent));

  if (G_IS_LIST_STORE (parent))
    {
      if (child_info->type != NULL)
        {
          GTK_BUILDER_WARN_INVALID_CHILD_TYPE (parent, child_info->type);
        }
      else
        {
          g_list_store_append (G_LIST_STORE (parent), object);
        }
    }
  else
    {
      g_assert (GTK_IS_BUILDABLE (parent));
      gtk_buildable_add_child (GTK_BUILDABLE (parent), builder, object,
                               child_info->type);
    }

  child_info->added = TRUE;
}

void
_gtk_builder_add_signals (GtkBuilder *builder,
                          GPtrArray  *signals)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  if (G_UNLIKELY (!priv->signals))
    priv->signals = g_ptr_array_new_with_free_func ((GDestroyNotify)_free_signal_info);

  g_ptr_array_extend_and_steal (priv->signals, signals);
}

static gboolean
gtk_builder_apply_delayed_properties (GtkBuilder  *builder,
                                      GError     **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l, *props;
  gboolean result = TRUE;

  /* take the list over from the builder->priv.
   *
   * g_slist_reverse does not copy the list, so the list now
   * belongs to us (and we free it at the end of this function).
   */
  props = g_slist_reverse (priv->delayed_properties);
  priv->delayed_properties = NULL;

  for (l = props; l; l = l->next)
    {
      DelayedProperty *property = l->data;
      GObject *object, *obj;

      if (result)
        {
          object = g_hash_table_lookup (priv->objects, property->object);
          g_assert (object != NULL);

          obj = gtk_builder_lookup_object (builder, property->value, property->line, property->col, error);
          if (obj)
            g_object_set (object, property->pspec->name, obj, NULL);
          else
            result = FALSE;
        }

      g_free (property->value);
      g_free (property->object);
      g_free (property);
    }
  g_slist_free (props);

  return result;
}

static inline gboolean
gtk_builder_create_bindings (GtkBuilder  *builder,
                             GError     **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *l;
  gboolean result = TRUE;

  for (l = priv->bindings; l; l = l->next)
    {
      CommonInfo *common_info = l->data;

      if (common_info->tag_type == TAG_BINDING)
        {
          BindingInfo *info = l->data;
          GObject *source;

          source = gtk_builder_lookup_object (builder, info->source, info->line, info->col, error);
          if (source)
            g_object_bind_property (source, info->source_property,
                                    info->target, info->target_pspec->name,
                                    info->flags);
          else
            error = NULL;

          _free_binding_info (info, NULL);
        }
      else if (common_info->tag_type == TAG_BINDING_EXPRESSION)
        {
          BindingExpressionInfo *info = l->data;
          GtkExpression *expression;
          GObject *object;

          if (info->object_name)
            {
              object = gtk_builder_lookup_object (builder, info->object_name, info->line, info->col, error);
              if (object == NULL)
                {
                  error = NULL;
                  result = FALSE;
                }
            }
          else if (priv->current_object)
            {
              object = priv->current_object;
            }
          else
            {
              object = info->target;
            }

          if (object)
            {
              expression = expression_info_construct (builder, info->expr, error);
              if (expression == NULL)
                {
                  g_prefix_error (error, "%s:%d:%d: ", priv->filename, info->line, info->col);
                  error = NULL;
                  result = FALSE;
                }
              else
                {
                  gtk_expression_bind (expression, info->target, info->target_pspec->name, object);
                }
            }

          free_binding_expression_info (info);
        }
      else
        g_assert_not_reached ();
    }

  g_slist_free (priv->bindings);
  priv->bindings = NULL;
  return result;
}

/**
 * gtk_builder_new:
 *
 * Creates a new empty builder object.
 *
 * This function is only useful if you intend to make multiple calls
 * to [method@Gtk.Builder.add_from_file], [method@Gtk.Builder.add_from_resource]
 * or [method@Gtk.Builder.add_from_string] in order to merge multiple UI
 * descriptions into a single builder.
 *
 * Returns: a new (empty) `GtkBuilder` object
 */
GtkBuilder *
gtk_builder_new (void)
{
  return g_object_new (GTK_TYPE_BUILDER, NULL);
}

/**
 * gtk_builder_add_from_file:
 * @builder: a `GtkBuilder`
 * @filename: (type filename): the name of the file to parse
 * @error: (nullable): return location for an error
 *
 * Parses a file containing a UI definition and merges it with
 * the current contents of @builder.
 *
 * This function is useful if you need to call
 * [method@Gtk.Builder.set_current_object]) to add user data to
 * callbacks before loading GtkBuilder UI. Otherwise, you probably
 * want [ctor@Gtk.Builder.new_from_file] instead.
 *
 * If an error occurs, 0 will be returned and @error will be assigned a
 * `GError` from the `GTK_BUILDER_ERROR`, `G_MARKUP_ERROR` or `G_FILE_ERROR`
 * domains.
 *
 * It’s not really reasonable to attempt to handle failures of this
 * call. You should not use this function with untrusted files (ie:
 * files that are not part of your application). Broken `GtkBuilder`
 * files can easily crash your program, and it’s possible that memory
 * was leaked leading up to the reported failure. The only reasonable
 * thing to do when an error is detected is to call `g_error()`.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gtk_builder_add_from_file (GtkBuilder   *builder,
                           const char   *filename,
                           GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  char *buffer;
  gsize length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (filename);
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, (gssize)length,
                                    NULL,
                                    &tmp_error);

  g_free (buffer);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_objects_from_file:
 * @builder: a `GtkBuilder`
 * @filename: (type filename): the name of the file to parse
 * @object_ids: (array zero-terminated=1) (element-type utf8): nul-terminated array of objects to build
 * @error: (nullable): return location for an error
 *
 * Parses a file containing a UI definition building only the
 * requested objects and merges them with the current contents
 * of @builder.
 *
 * Upon errors, 0 will be returned and @error will be assigned a
 * `GError` from the %GTK_BUILDER_ERROR, %G_MARKUP_ERROR or %G_FILE_ERROR
 * domain.
 *
 * If you are adding an object that depends on an object that is not
 * its child (for instance a `GtkTreeView` that depends on its
 * `GtkTreeModel`), you have to explicitly list all of them in @object_ids.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gtk_builder_add_objects_from_file (GtkBuilder   *builder,
                                   const char   *filename,
                                   const char  **object_ids,
                                   GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  char *buffer;
  gsize length;
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (filename != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  if (!g_file_get_contents (filename, &buffer, &length, &tmp_error))
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (filename);
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, (gssize)length,
                                    object_ids,
                                    &tmp_error);

  g_free (buffer);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

void
gtk_builder_set_allow_template_parents (GtkBuilder *builder,
                                        gboolean    allow_parents)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  priv->allow_template_parents = allow_parents;
}

/**
 * gtk_builder_extend_with_template:
 * @builder: a `GtkBuilder`
 * @object: the object that is being extended
 * @template_type: the type that the template is for
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: (nullable): return location for an error
 *
 * Main private entry point for building composite components
 * from template XML.
 *
 * Most likely you do not need to call this function in applications as
 * templates are handled by `GtkWidget`.
 *
 * Returns: A positive value on success, 0 if an error occurred
 */
gboolean
gtk_builder_extend_with_template (GtkBuilder   *builder,
                                  GObject      *object,
                                  GType         template_type,
                                  const char   *buffer,
                                  gssize        length,
                                  GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  const char *name;
  GError *tmp_error;
  char *filename;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (G_IS_OBJECT (object), 0);
  g_return_val_if_fail (g_type_name (template_type) != NULL, 0);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), template_type), 0);
  g_return_val_if_fail (buffer && buffer[0], 0);

  tmp_error = NULL;

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");
  priv->resource_prefix = NULL;
  priv->template_type = template_type;

  /* We specifically allow this function to be called multiple times with
   * the same @template_type as that is used in applications like Builder
   * to implement UI merging.
   */
  name = g_type_name (template_type);
  if (gtk_builder_get_object (builder, name) != object)
    gtk_builder_expose_object (builder, name, object);
  if (priv->allow_template_parents)
    {
      GType subtype;
      for (subtype = g_type_parent (template_type);
           subtype != G_TYPE_OBJECT;
           subtype = g_type_parent (subtype))
        {
          name = g_type_name (subtype);
          if (gtk_builder_get_object (builder, name) != object)
            gtk_builder_expose_object (builder, name, object);
        }
    }

  filename = g_strconcat ("<", name, " template>", NULL);
  _gtk_builder_parser_parse_buffer (builder, filename,
                                    buffer, length,
                                    NULL,
                                    &tmp_error);
  g_free (filename);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_from_resource:
 * @builder: a `GtkBuilder`
 * @resource_path: the path of the resource file to parse
 * @error: (nullable): return location for an error
 *
 * Parses a resource file containing a UI definition
 * and merges it with the current contents of @builder.
 *
 * This function is useful if you need to call
 * [method@Gtk.Builder.set_current_object] to add user data to
 * callbacks before loading GtkBuilder UI. Otherwise, you probably
 * want [ctor@Gtk.Builder.new_from_resource] instead.
 *
 * If an error occurs, 0 will be returned and @error will be assigned a
 * `GError` from the %GTK_BUILDER_ERROR, %G_MARKUP_ERROR or %G_RESOURCE_ERROR
 * domain.
 *
 * It’s not really reasonable to attempt to handle failures of this
 * call.  The only reasonable thing to do when an error is detected is
 * to call g_error().
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gtk_builder_add_from_resource (GtkBuilder   *builder,
                               const char   *resource_path,
                               GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;
  GBytes *data;
  char *filename_for_errors;
  char *slash;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (resource_path != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  data = g_resources_lookup_data (resource_path, 0, &tmp_error);
  if (data == NULL)
    {
      g_propagate_error (error, tmp_error);
      return 0;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");

  slash = strrchr (resource_path, '/');
  if (slash != NULL)
    priv->resource_prefix = g_strndup (resource_path, slash - resource_path + 1);
  else
    priv->resource_prefix = g_strdup ("/");

  filename_for_errors = g_strconcat ("<resource>", resource_path, NULL);

  _gtk_builder_parser_parse_buffer (builder, filename_for_errors,
                                    g_bytes_get_data (data, NULL), g_bytes_get_size (data),
                                    NULL,
                                    &tmp_error);

  g_free (filename_for_errors);
  g_bytes_unref (data);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_objects_from_resource:
 * @builder: a `GtkBuilder`
 * @resource_path: the path of the resource file to parse
 * @object_ids: (array zero-terminated=1) (element-type utf8): nul-terminated array of objects to build
 * @error: (nullable): return location for an error
 *
 * Parses a resource file containing a UI definition, building
 * only the requested objects and merges them with the current
 * contents of @builder.
 *
 * Upon errors, 0 will be returned and @error will be assigned a
 * `GError` from the %GTK_BUILDER_ERROR, %G_MARKUP_ERROR or %G_RESOURCE_ERROR
 * domain.
 *
 * If you are adding an object that depends on an object that is not
 * its child (for instance a `GtkTreeView` that depends on its
 * `GtkTreeModel`), you have to explicitly list all of them in @object_ids.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gtk_builder_add_objects_from_resource (GtkBuilder   *builder,
                                       const char   *resource_path,
                                       const char  **object_ids,
                                       GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;
  GBytes *data;
  char *filename_for_errors;
  char *slash;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (resource_path != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  data = g_resources_lookup_data (resource_path, 0, &tmp_error);
  if (data == NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");

  slash = strrchr (resource_path, '/');
  if (slash != NULL)
    priv->resource_prefix = g_strndup (resource_path, slash - resource_path + 1);
  else
    priv->resource_prefix = g_strdup ("/");

  filename_for_errors = g_strconcat ("<resource>", resource_path, NULL);

  _gtk_builder_parser_parse_buffer (builder, filename_for_errors,
                                    g_bytes_get_data (data, NULL), g_bytes_get_size (data),
                                    object_ids,
                                    &tmp_error);
  g_free (filename_for_errors);
  g_bytes_unref (data);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_from_string:
 * @builder: a `GtkBuilder`
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @error: (nullable): return location for an error
 *
 * Parses a string containing a UI definition and merges it
 * with the current contents of @builder.
 *
 * This function is useful if you need to call
 * [method@Gtk.Builder.set_current_object] to add user data to
 * callbacks before loading `GtkBuilder` UI. Otherwise, you probably
 * want [ctor@Gtk.Builder.new_from_string] instead.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * `GError` from the %GTK_BUILDER_ERROR, %G_MARKUP_ERROR or
 * %G_VARIANT_PARSE_ERROR domain.
 *
 * It’s not really reasonable to attempt to handle failures of this
 * call.  The only reasonable thing to do when an error is detected is
 * to call g_error().
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gtk_builder_add_from_string (GtkBuilder   *builder,
                             const char   *buffer,
                             gssize        length,
                             GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    NULL,
                                    &tmp_error);
  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_add_objects_from_string:
 * @builder: a `GtkBuilder`
 * @buffer: the string to parse
 * @length: the length of @buffer (may be -1 if @buffer is nul-terminated)
 * @object_ids: (array zero-terminated=1) (element-type utf8): nul-terminated array of objects to build
 * @error: (nullable): return location for an error
 *
 * Parses a string containing a UI definition, building only the
 * requested objects and merges them with the current contents of
 * @builder.
 *
 * Upon errors %FALSE will be returned and @error will be assigned a
 * `GError` from the %GTK_BUILDER_ERROR or %G_MARKUP_ERROR domain.
 *
 * If you are adding an object that depends on an object that is not
 * its child (for instance a `GtkTreeView` that depends on its
 * `GtkTreeModel`), you have to explicitly list all of them in @object_ids.
 *
 * Returns: %TRUE on success, %FALSE if an error occurred
 */
gboolean
gtk_builder_add_objects_from_string (GtkBuilder   *builder,
                                     const char   *buffer,
                                     gssize        length,
                                     const char  **object_ids,
                                     GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GError *tmp_error;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), 0);
  g_return_val_if_fail (buffer != NULL, 0);
  g_return_val_if_fail (object_ids != NULL && object_ids[0] != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  tmp_error = NULL;

  g_free (priv->filename);
  g_free (priv->resource_prefix);
  priv->filename = g_strdup (".");
  priv->resource_prefix = NULL;

  _gtk_builder_parser_parse_buffer (builder, "<input>",
                                    buffer, length,
                                    object_ids,
                                    &tmp_error);

  if (tmp_error != NULL)
    {
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  return TRUE;
}

/**
 * gtk_builder_get_object:
 * @builder: a `GtkBuilder`
 * @name: name of object to get
 *
 * Gets the object named @name.
 *
 * Note that this function does not increment the reference count
 * of the returned object.
 *
 * Returns: (nullable) (transfer none): the object named @name
 */
GObject *
gtk_builder_get_object (GtkBuilder  *builder,
                        const char *name)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_hash_table_lookup (priv->objects, name);
}

/**
 * gtk_builder_get_objects:
 * @builder: a `GtkBuilder`
 *
 * Gets all objects that have been constructed by @builder.
 *
 * Note that this function does not increment the reference
 * counts of the returned objects.
 *
 * Returns: (element-type GObject) (transfer container): a
 *   newly-allocated `GSList` containing all the objects
 *   constructed by the `GtkBuilder instance`. It should be
 *   freed by g_slist_free()
 */
GSList *
gtk_builder_get_objects (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GSList *objects = NULL;
  GObject *object;
  GHashTableIter iter;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  g_hash_table_iter_init (&iter, priv->objects);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&object))
    objects = g_slist_prepend (objects, object);

  return g_slist_reverse (objects);
}

/**
 * gtk_builder_set_translation_domain: (attributes org.gtk.Method.set_property=translation-domain)
 * @builder: a `GtkBuilder`
 * @domain: (nullable): the translation domain
 *
 * Sets the translation domain of @builder.
 */
void
gtk_builder_set_translation_domain (GtkBuilder  *builder,
                                    const char *domain)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  char *new_domain;

  g_return_if_fail (GTK_IS_BUILDER (builder));

  new_domain = g_strdup (domain);
  g_free (priv->domain);
  priv->domain = new_domain;

  g_object_notify_by_pspec (G_OBJECT (builder), builder_props[PROP_TRANSLATION_DOMAIN]);
}

/**
 * gtk_builder_get_translation_domain: (attributes org.gtk.Method.get_property=translation-domain)
 * @builder: a `GtkBuilder`
 *
 * Gets the translation domain of @builder.
 *
 * Returns: (transfer none) (nullable): the translation domain
 */
const char *
gtk_builder_get_translation_domain (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  return priv->domain;
}

/**
 * gtk_builder_expose_object:
 * @builder: a `GtkBuilder`
 * @name: the name of the object exposed to the builder
 * @object: the object to expose
 *
 * Add @object to the @builder object pool so it can be
 * referenced just like any other object built by builder.
 *
 * Only a single object may be added using @name. However,
 * it is not an error to expose the same object under multiple
 * names. `gtk_builder_get_object()` may be used to determine
 * if an object has already been added with @name.
 */
void
gtk_builder_expose_object (GtkBuilder    *builder,
                           const char    *name,
                           GObject       *object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (name && name[0]);
  g_return_if_fail (!g_hash_table_contains (priv->objects, name));

  object_set_id (object, name);
  g_hash_table_insert (priv->objects,
                       g_strdup (name),
                       g_object_ref (object));
}

/**
 * gtk_builder_get_current_object: (attributes org.gtk.Method.get_property=current-object)
 * @builder: a `GtkBuilder`
 *
 * Gets the current object set via gtk_builder_set_current_object().
 *
 * Returns: (nullable) (transfer none): the current object
 */
GObject *
gtk_builder_get_current_object (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  return priv->current_object;
}

/**
 * gtk_builder_set_current_object: (attributes org.gtk.Method.set_property=current-object)
 * @builder: a `GtkBuilder`
 * @current_object: (nullable) (transfer none): the new current object
 *
 * Sets the current object for the @builder.
 *
 * The current object can be thought of as the `this` object that the
 * builder is working for and will often be used as the default object
 * when an object is optional.
 *
 * [method@Gtk.Widget.init_template] for example will set the current
 * object to the widget the template is inited for. For functions like
 * [ctor@Gtk.Builder.new_from_resource], the current object will be %NULL.
 */
void
gtk_builder_set_current_object (GtkBuilder *builder,
                                GObject    *current_object)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (current_object || G_IS_OBJECT (current_object));

  if (!g_set_object (&priv->current_object, current_object))
    return;

  g_object_notify_by_pspec (G_OBJECT (builder), builder_props[PROP_CURRENT_OBJECT]);
}

/**
 * gtk_builder_get_scope: (attributes org.gtk.Method.get_property=scope)
 * @builder: a `GtkBuilder`
 *
 * Gets the scope in use that was set via gtk_builder_set_scope().
 *
 * Returns: (transfer none): the current scope
 */
GtkBuilderScope *
gtk_builder_get_scope (GtkBuilder *builder)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);

  return priv->scope;
}

/**
 * gtk_builder_set_scope: (attributes org.gtk.Method.set_property=scope)
 * @builder: a `GtkBuilder`
 * @scope: (nullable) (transfer none): the scope to use
 *
 * Sets the scope the builder should operate in.
 *
 * If @scope is %NULL, a new [class@Gtk.BuilderCScope] will be created.
 */
void
gtk_builder_set_scope (GtkBuilder      *builder,
                       GtkBuilderScope *scope)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_if_fail (GTK_IS_BUILDER (builder));
  g_return_if_fail (scope == NULL || GTK_IS_BUILDER_SCOPE (scope));

  if (scope && priv->scope == scope)
    return;

  g_clear_object (&priv->scope);

  if (scope)
    priv->scope = g_object_ref (scope);
  else
    priv->scope = gtk_builder_cscope_new ();

  g_object_notify_by_pspec (G_OBJECT (builder), builder_props[PROP_SCOPE]);
}

static gboolean
gtk_builder_connect_signals (GtkBuilder  *builder,
                             GError     **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GObject *object;
  GObject *connect_object;
  gboolean result = TRUE;

  if (!priv->signals ||
      priv->signals->len == 0)
    return TRUE;

  for (guint i = 0; i < priv->signals->len; i++)
    {
      SignalInfo *signal = g_ptr_array_index (priv->signals, i);
      GClosure *closure;

      g_assert (signal != NULL);
      g_assert (signal->id != 0);

      object = g_hash_table_lookup (priv->objects, signal->object_name);
      g_assert (object != NULL);

      connect_object = NULL;

      if (signal->connect_object_name)
        {
          connect_object = g_hash_table_lookup (priv->objects,
                                                signal->connect_object_name);
          if (!connect_object)
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ID,
                           "Could not lookup object %s on signal %s of object %s",
                           signal->connect_object_name, g_signal_name (signal->id),
                           signal->object_name);
              break;
            }
        }

      closure = gtk_builder_create_closure (builder,
                                            signal->handler,
                                            signal->flags & G_CONNECT_SWAPPED ? TRUE : FALSE,
                                            connect_object,
                                            error);

      if (closure == NULL)
        {
          result = false;
          break;
        }

      g_signal_connect_closure_by_id (object,
                                      signal->id,
                                      signal->detail,
                                      closure,
                                      signal->flags & G_CONNECT_AFTER ? TRUE : FALSE);
    }

  g_ptr_array_free (priv->signals, TRUE);
  priv->signals = NULL;

  return result;
}

gboolean
_gtk_builder_finish (GtkBuilder  *builder,
                     GError     **error)
{
  return gtk_builder_apply_delayed_properties (builder, error) &&
         gtk_builder_create_bindings (builder, error) &&
         gtk_builder_connect_signals (builder, error);
}

/**
 * gtk_builder_value_from_string:
 * @builder: a `GtkBuilder`
 * @pspec: the `GParamSpec` for the property
 * @string: the string representation of the value
 * @value: (out): the `GValue` to store the result in
 * @error: (nullable): return location for an error
 *
 * Demarshals a value from a string.
 *
 * This function calls g_value_init() on the @value argument,
 * so it need not be initialised beforehand.
 *
 * Can handle char, uchar, boolean, int, uint, long,
 * ulong, enum, flags, float, double, string, `GdkRGBA` and
 * `GtkAdjustment` type values.
 *
 * Upon errors %FALSE will be returned and @error will be
 * assigned a `GError` from the %GTK_BUILDER_ERROR domain.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_builder_value_from_string (GtkBuilder   *builder,
                               GParamSpec   *pspec,
                               const char   *string,
                               GValue       *value,
                               GError      **error)
{
  g_return_val_if_fail (GTK_IS_BUILDER (builder), FALSE);
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /*
   * GParamSpecUnichar has the internal type G_TYPE_UINT,
   * so we cannot handle this in the switch, do it separately
   */
  if (G_IS_PARAM_SPEC_UNICHAR (pspec))
    {
      gunichar c;
      g_value_init (value, G_TYPE_UINT);
      c = g_utf8_get_char_validated (string, -1);
      if (c != 0 && c != (gunichar)-1 && c != (gunichar)-2)
        g_value_set_uint (value, c);
      return TRUE;
    }

  /*
   * GParamSpecVariant can specify a GVariantType which can help with
   * parsing, so we need to take care of that here.
   */
  if (G_IS_PARAM_SPEC_VARIANT (pspec))
    {
      GParamSpecVariant *variant_pspec = G_PARAM_SPEC_VARIANT (pspec);
      const GVariantType *type;
      GVariant *variant;

      g_value_init (value, G_TYPE_VARIANT);

      /* The GVariant parser doesn't deal with indefinite types */
      if (g_variant_type_is_definite (variant_pspec->type))
        type = variant_pspec->type;
      else
        type = NULL;

      variant = g_variant_parse (type, string, NULL, NULL, error);
      if (variant == NULL)
        return FALSE;
      g_value_take_variant (value, variant);
      return TRUE;
    }

  return gtk_builder_value_from_string_type (builder,
                                             G_PARAM_SPEC_VALUE_TYPE (pspec),
                                             string, value, error);
}

gboolean
_gtk_builder_boolean_from_string (const char   *string,
                                  gboolean     *value,
                                  GError      **error)
{
  if (string[0] == '\0')
    goto error;
  else if (string[1] == '\0')
    {
      char c;

      c = string[0];
      if (c == '1' ||
          c == 'y' || c == 't' ||
          c == 'Y' || c == 'T')
        *value = TRUE;
      else if (c == '0' ||
               c == 'n' || c == 'f' ||
               c == 'N' || c == 'F')
        *value = FALSE;
      else
        goto error;
    }
  else
    {
      if (g_ascii_strcasecmp (string, "true") == 0 ||
          g_ascii_strcasecmp (string, "yes") == 0)
        *value = TRUE;
      else if (g_ascii_strcasecmp (string, "false") == 0 ||
               g_ascii_strcasecmp (string, "no") == 0)
        *value = FALSE;
      else
        goto error;
    }

  return TRUE;

error:
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_INVALID_VALUE,
               "Could not parse boolean '%s'",
               string);
  return FALSE;
}


/**
 * gtk_builder_value_from_string_type:
 * @builder: a `GtkBuilder`
 * @type: the `GType` of the value
 * @string: the string representation of the value
 * @value: (out): the `GValue` to store the result in
 * @error: (nullable): return location for an error
 *
 * Demarshals a value from a string.
 *
 * Unlike [method@Gtk.Builder.value_from_string], this function
 * takes a `GType` instead of `GParamSpec`.
 *
 * Calls g_value_init() on the @value argument, so it
 * need not be initialised beforehand.
 *
 * Upon errors %FALSE will be returned and @error will be
 * assigned a `GError` from the %GTK_BUILDER_ERROR domain.
 *
 * Returns: %TRUE on success
 */
gboolean
gtk_builder_value_from_string_type (GtkBuilder   *builder,
                                    GType         type,
                                    const char   *string,
                                    GValue       *value,
                                    GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  gboolean ret = TRUE;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_value_init (value, type);

  switch (G_TYPE_FUNDAMENTAL (type))
    {
    case G_TYPE_CHAR:
      g_value_set_schar (value, string[0]);
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (value, (guchar)string[0]);
      break;
    case G_TYPE_BOOLEAN:
      {
        gboolean b;

        if (!_gtk_builder_boolean_from_string (string, &b, error))
          {
            ret = FALSE;
            break;
          }
        g_value_set_boolean (value, b);
        break;
      }
    case G_TYPE_INT:
    case G_TYPE_LONG:
    case G_TYPE_INT64:
      {
        gint64 l;
        char *endptr = NULL;

        errno = 0;
        l = g_ascii_strtoll (string, &endptr, 0);
        if (errno || endptr == string)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_VALUE,
                         "Could not parse integer '%s'",
                         string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_INT (value))
          g_value_set_int (value, l);
        else if (G_VALUE_HOLDS_LONG (value))
          g_value_set_long (value, l);
        else
          g_value_set_int64 (value, l);
        break;
      }
    case G_TYPE_UINT:
    case G_TYPE_ULONG:
    case G_TYPE_UINT64:
      {
        guint64 ul;
        char *endptr = NULL;
        errno = 0;
        ul = g_ascii_strtoull (string, &endptr, 0);
        if (errno || endptr == string)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_VALUE,
                         "Could not parse unsigned integer '%s'",
                         string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_UINT (value))
          g_value_set_uint (value, ul);
        else if (G_VALUE_HOLDS_ULONG (value))
          g_value_set_ulong (value, ul);
        else
          g_value_set_uint64 (value, ul);
        break;
      }
    case G_TYPE_ENUM:
      {
        int enum_value;
        if (!_gtk_builder_enum_from_string (type, string, &enum_value, error))
          {
            ret = FALSE;
            break;
          }
        g_value_set_enum (value, enum_value);
        break;
      }
    case G_TYPE_FLAGS:
      {
        guint flags_value;

        if (!_gtk_builder_flags_from_string (type, string, &flags_value, error))
          {
            ret = FALSE;
            break;
          }
        g_value_set_flags (value, flags_value);
        break;
      }
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      {
        double d;
        char *endptr = NULL;
        errno = 0;
        d = g_ascii_strtod (string, &endptr);
        if (errno || endptr == string)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_VALUE,
                         "Could not parse double '%s'",
                         string);
            ret = FALSE;
            break;
          }
        if (G_VALUE_HOLDS_FLOAT (value))
          g_value_set_float (value, d);
        else
          g_value_set_double (value, d);
        break;
      }
    case G_TYPE_STRING:
      g_value_set_string (value, string);
      break;
    case G_TYPE_VARIANT:
      {
        GVariant *variant;

        variant = g_variant_parse (NULL, string, NULL, NULL, error);
        if (value != NULL)
          g_value_take_variant (value, variant);
        else
          ret = FALSE;
      }
      break;
    case G_TYPE_BOXED:
      if (G_VALUE_HOLDS (value, GDK_TYPE_RGBA))
        {
          GdkRGBA rgba = { 0 };

          if (gdk_rgba_parse (&rgba, string))
            g_value_set_boxed (value, &rgba);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse RGBA color '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, GDK_TYPE_CONTENT_FORMATS))
        {
          GdkContentFormats *formats;

          formats = gdk_content_formats_parse (string);
          if (formats)
            g_value_take_boxed (value, formats);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse GdkContentFormats '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, GSK_TYPE_TRANSFORM))
        {
          GskTransform *transform;

          if (gsk_transform_parse (string, &transform))
            g_value_take_boxed (value, transform);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse transform '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          char **vector = g_strsplit (string, "\n", 0);
          g_value_take_boxed (value, vector);
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_BYTES))
        {
          g_value_take_boxed (value, g_bytes_new (string, strlen (string)));
        }
      else if (G_VALUE_HOLDS (value, PANGO_TYPE_ATTR_LIST))
        {
          PangoAttrList *attrs;

          attrs = pango_attr_list_from_string (string);
          if (attrs)
            g_value_take_boxed (value, attrs);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse PangoAttrList '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME))
        {
          GDateTime *date_time;

          date_time = g_date_time_new_from_iso8601 (string, NULL);

          if (date_time)
            g_value_take_boxed (value, date_time);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse GDateTime '%s'",
                           string);
              ret = FALSE;
            }
        }
      else
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Could not parse '%s' as a %s",
                       string, G_VALUE_TYPE_NAME (value));
          ret = FALSE;
        }
      break;
    case G_TYPE_OBJECT:
    case G_TYPE_INTERFACE:
      if (G_VALUE_HOLDS (value, GDK_TYPE_PAINTABLE) ||
          G_VALUE_HOLDS (value, GDK_TYPE_TEXTURE))
        {
          GObject *object = g_hash_table_lookup (priv->objects, string);
          char *filename;
          GError *tmp_error = NULL;
          GdkTexture *texture = NULL;

          if (object)
            {
              if (g_type_is_a (G_OBJECT_TYPE (object), G_VALUE_TYPE (value)))
                {
                  g_value_set_object (value, object);
                  return TRUE;
                }
              else
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_VALUE,
                               "Could not load image '%s': "
                               " '%s' is already used as object id for a %s",
                               string, string, G_OBJECT_TYPE_NAME (object));
                  return FALSE;
                }
            }

          filename = _gtk_builder_get_resource_path (builder, string);
          if (filename != NULL)
            {
              texture = gdk_texture_new_from_resource (filename);
            }
          else
            {
              GFile *file;

              filename = _gtk_builder_get_absolute_filename (builder, string);
              file = g_file_new_for_path (filename);
              texture = gdk_texture_new_from_file (file, &tmp_error);
              g_object_unref (file);
            }

          g_free (filename);

          if (!texture)
            {
              g_warning ("Could not load image '%s': %s", string, tmp_error->message);
              g_error_free (tmp_error);

              texture = gdk_texture_new_from_resource (IMAGE_MISSING_RESOURCE_PATH);
            }

          g_value_take_object (value, texture);

          ret = TRUE;
        }
      else if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
        {
          char *filename;
          GError *tmp_error = NULL;
          GdkPixbuf *pixbuf = NULL;
          GObject *object;

          object = g_hash_table_lookup (priv->objects, string);

          if (object)
            {
              if (g_type_is_a (G_OBJECT_TYPE (object), G_VALUE_TYPE (value)))
                {
                  g_value_set_object (value, object);
                  return TRUE;
                }
              else
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_VALUE,
                               "Could not load image '%s': "
                               " '%s' is already used as object id for a %s",
                               string, string, G_OBJECT_TYPE_NAME (object));
                  return FALSE;
                }
            }

          filename = _gtk_builder_get_resource_path (builder, string);
          if (filename != NULL)
            {
              GInputStream *stream = g_resources_open_stream (filename, 0, &tmp_error);
              if (stream != NULL)
                {
                  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &tmp_error);
                  g_object_unref (stream);
                }
            }
          else
            {
              filename = _gtk_builder_get_absolute_filename (builder, string);
              pixbuf = gdk_pixbuf_new_from_file (filename, &tmp_error);
            }

          if (pixbuf == NULL)
            {
              g_warning ("Could not load image '%s': %s", string, tmp_error->message);
              g_error_free (tmp_error);

              pixbuf = gdk_pixbuf_new_from_resource (IMAGE_MISSING_RESOURCE_PATH, NULL);
            }

          g_value_take_object (value, pixbuf);

          g_free (filename);

          ret = TRUE;
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_FILE))
        {
          GFile *file;

          if (g_hash_table_contains (priv->objects, string))
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not create file '%s': "
                           " '%s' is already used as object id",
                           string, string);
              return FALSE;
            }

          file = g_file_new_for_uri (string);
          g_value_set_object (value, file);
          g_object_unref (G_OBJECT (file));

          ret = TRUE;
        }
      else if (G_VALUE_HOLDS (value, GTK_TYPE_SHORTCUT_TRIGGER))
        {
          GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string (string);

          if (trigger)
            g_value_take_object (value, trigger);
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Could not parse shortcut trigger '%s'",
                           string);
              ret = FALSE;
            }
        }
      else if (G_VALUE_HOLDS (value, GTK_TYPE_SHORTCUT_ACTION))
        {
          GtkShortcutAction *action = gtk_shortcut_action_parse_builder (builder, string, error);

          if (action)
            g_value_take_object (value, action);
          else
            ret = FALSE;
        }
      else
        {
          GObject *object = g_hash_table_lookup (priv->objects, string);

          if (object && g_value_type_compatible (G_OBJECT_TYPE (object), type))
            {
              g_value_set_object (value, object);
            }
          else if (object)
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Object named \"%s\" is of type \"%s\" which is not compatible with expected type \"%s\"",
                           string, G_OBJECT_TYPE_NAME (object), g_type_name (type));
              ret = FALSE;
            }
          else
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "No object named \"%s\"", string);
              ret = FALSE;
            }
        }
      break;
    case G_TYPE_POINTER:
      if (G_VALUE_HOLDS (value, G_TYPE_GTYPE))
        {
          GType resolved_type;

          resolved_type = gtk_builder_get_type_from_name (builder, string);
          if (resolved_type == G_TYPE_INVALID)
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_INVALID_VALUE,
                           "Unsupported GType '%s' for value of type 'GType'", string);
              return FALSE;
            }
          g_value_set_gtype (value, resolved_type);

          ret = TRUE;
        }
      else
        ret = FALSE;
      break;
    default:
      ret = FALSE;
      break;
    }

  /* Catch unassigned error for object types as well as any unsupported types.
   * While parsing GtkBuilder; object types are deserialized
   * without calling gtk_builder_value_from_string_type().
   */
  if (!ret && error && *error == NULL)
    g_set_error (error,
                 GTK_BUILDER_ERROR,
                 GTK_BUILDER_ERROR_INVALID_VALUE,
                 "Unsupported GType '%s'", g_type_name (type));

  return ret;
}

gboolean
_gtk_builder_enum_from_string (GType         type,
                               const char   *string,
                               int          *enum_value,
                               GError      **error)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  char *endptr;
  int value;
  gboolean ret;

  g_return_val_if_fail (G_TYPE_IS_ENUM (type), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  ret = TRUE;

  endptr = NULL;
  errno = 0;
  value = g_ascii_strtoull (string, &endptr, 0);
  if (errno == 0 && endptr != string) /* parsed a number */
    *enum_value = value;
  else
    {
      eclass = g_type_class_ref (type);
      ev = g_enum_get_value_by_nick (eclass, string);
      if (!ev)
        ev = g_enum_get_value_by_name (eclass, string);

      if (ev)
        *enum_value = ev->value;
      else
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Could not parse enum: '%s'",
                       string);
          ret = FALSE;
        }

      g_type_class_unref (eclass);
    }

  return ret;
}

gboolean
_gtk_builder_flags_from_string (GType         type,
                                const char   *string,
                                guint        *flags_value,
                                GError      **error)
{
  GFlagsClass *fclass;
  char *endptr, *prevptr;
  guint i, j, value;
  char *flagstr;
  GFlagsValue *fv;
  const char *flag;
  gunichar ch;
  gboolean eos, ret;

  g_return_val_if_fail (G_TYPE_IS_FLAGS (type), FALSE);
  g_return_val_if_fail (string != 0, FALSE);

  ret = TRUE;

  endptr = NULL;
  errno = 0;
  value = g_ascii_strtoull (string, &endptr, 0);
  if (errno == 0 && endptr != string) /* parsed a number */
    *flags_value = value;
  else
    {
      fclass = g_type_class_ref (type);

      flagstr = g_strdup (string);
      for (value = i = j = 0; ; i++)
        {

          eos = flagstr[i] == '\0';

          if (!eos && flagstr[i] != '|')
            continue;

          flag = &flagstr[j];
          endptr = &flagstr[i];

          if (!eos)
            {
              flagstr[i++] = '\0';
              j = i;
            }

          /* trim spaces */
          for (;;)
            {
              ch = g_utf8_get_char (flag);
              if (!g_unichar_isspace (ch))
                break;
              flag = g_utf8_next_char (flag);
            }

          while (endptr > flag)
            {
              prevptr = g_utf8_prev_char (endptr);
              ch = g_utf8_get_char (prevptr);
              if (!g_unichar_isspace (ch))
                break;
              endptr = prevptr;
            }

          if (endptr > flag)
            {
              *endptr = '\0';

              fv = NULL;

              if (!fv)
                fv = g_flags_get_value_by_name (fclass, flag);

              if (!fv)
                fv = g_flags_get_value_by_nick (fclass, flag);

              if (fv)
                value |= fv->value;
              else
                {
                  g_set_error (error,
                               GTK_BUILDER_ERROR,
                               GTK_BUILDER_ERROR_INVALID_VALUE,
                               "Unknown flag: '%s'",
                               flag);
                  ret = FALSE;
                  break;
                }
            }

          if (eos)
            {
              *flags_value = value;
              break;
            }
        }

      g_free (flagstr);

      g_type_class_unref (fclass);
    }

  return ret;
}

/**
 * gtk_builder_get_type_from_name:
 * @builder: a `GtkBuilder`
 * @type_name: type name to lookup
 *
 * Looks up a type by name.
 *
 * This is using the virtual function that `GtkBuilder` has
 * for that purpose. This is mainly used when implementing
 * the `GtkBuildable` interface on a type.
 *
 * Returns: the `GType` found for @type_name or %G_TYPE_INVALID
 *   if no type was found
 */
GType
gtk_builder_get_type_from_name (GtkBuilder  *builder,
                                const char *type_name)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GType type;

  g_return_val_if_fail (GTK_IS_BUILDER (builder), G_TYPE_INVALID);
  g_return_val_if_fail (type_name != NULL, G_TYPE_INVALID);

  type = gtk_builder_scope_get_type_from_name (priv->scope, builder, type_name);
  if (type == G_TYPE_INVALID)
    return G_TYPE_INVALID;

  if (G_TYPE_IS_CLASSED (type))
    g_type_class_unref (g_type_class_ref (type));

  return type;
}

GQuark
gtk_builder_error_quark (void)
{
  return g_quark_from_static_string ("gtk-builder-error-quark");
}

char *
_gtk_builder_get_resource_path (GtkBuilder *builder, const char *string)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  if (g_str_has_prefix (string, "resource:///"))
    return g_uri_unescape_string (string + 11, "/");

  if (g_path_is_absolute (string) ||
      priv->resource_prefix == NULL)
    return NULL;

  return g_build_path ("/", priv->resource_prefix, string, NULL);
}

char *
_gtk_builder_get_absolute_filename (GtkBuilder  *builder,
                                    const char *string)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  char *filename;
  char *dirname = NULL;

  if (g_path_is_absolute (string))
    return g_strdup (string);

  if (priv->filename &&
      strcmp (priv->filename, ".") != 0)
    {
      dirname = g_path_get_dirname (priv->filename);

      if (strcmp (dirname, ".") == 0)
        {
          g_free (dirname);
          dirname = g_get_current_dir ();
        }
    }
  else
    dirname = g_get_current_dir ();

  filename = g_build_filename (dirname, string, NULL);
  g_free (dirname);

  return filename;
}

GType
gtk_builder_get_template_type (GtkBuilder *builder,
                               gboolean   *out_allow_parents)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  *out_allow_parents = priv->allow_template_parents;

  return priv->template_type;
}

/**
 * gtk_builder_create_closure:
 * @builder: a `GtkBuilder`
 * @function_name: name of the function to look up
 * @flags: closure creation flags
 * @object: (nullable): Object to create the closure with
 * @error: (nullable): return location for an error
 *
 * Creates a closure to invoke the function called @function_name.
 *
 * This is using the create_closure() implementation of @builder's
 * [iface@Gtk.BuilderScope].
 *
 * If no closure could be created, %NULL will be returned and @error
 * will be set.
 *
 * Returns: (nullable): A new closure for invoking @function_name
 */
GClosure *
gtk_builder_create_closure (GtkBuilder             *builder,
                            const char             *function_name,
                            GtkBuilderClosureFlags  flags,
                            GObject                *object,
                            GError                **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);

  g_return_val_if_fail (GTK_IS_BUILDER (builder), NULL);
  g_return_val_if_fail (function_name, NULL);
  g_return_val_if_fail (object == NULL || G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return gtk_builder_scope_create_closure (priv->scope, builder, function_name, flags, object, error);
}

/**
 * gtk_builder_new_from_file: (constructor)
 * @filename: (type filename): filename of user interface description file
 *
 * Parses the UI definition in the file @filename.
 *
 * If there is an error opening the file or parsing the description then
 * the program will be aborted. You should only ever attempt to parse
 * user interface descriptions that are shipped as part of your program.
 *
 * Returns: (transfer full): a `GtkBuilder` containing the described interface
 */
GtkBuilder *
gtk_builder_new_from_file (const char *filename)
{
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, filename, &error))
    g_error ("failed to add UI from file %s: %s", filename, error->message);

  return builder;
}

/**
 * gtk_builder_new_from_resource: (constructor)
 * @resource_path: a `GResource` resource path
 *
 * Parses the UI definition at @resource_path.
 *
 * If there is an error locating the resource or parsing the
 * description, then the program will be aborted.
 *
 * Returns: (transfer full): a `GtkBuilder` containing the described interface
 */
GtkBuilder *
gtk_builder_new_from_resource (const char *resource_path)
{
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_resource (builder, resource_path, &error))
    g_error ("failed to add UI from resource %s: %s", resource_path, error->message);

  return builder;
}

/**
 * gtk_builder_new_from_string: (constructor)
 * @string: a user interface (XML) description
 * @length: the length of @string, or -1
 *
 * Parses the UI definition in @string.
 *
 * If @string is %NULL-terminated, then @length should be -1.
 * If @length is not -1, then it is the length of @string.
 *
 * If there is an error parsing @string then the program will be
 * aborted. You should not attempt to parse user interface description
 * from untrusted sources.
 *
 * Returns: (transfer full): a `GtkBuilder` containing the interface described by @string
 */
GtkBuilder *
gtk_builder_new_from_string (const char *string,
                             gssize       length)
{
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, string, length, &error))
    g_error ("failed to add UI: %s", error->message);

  return builder;
}

/*< private >
 * _gtk_builder_prefix_error:
 * @builder: a `GtkBuilder`
 * @context: the `GtkBuildableParseContext`
 * @error: an error
 *
 * Calls g_prefix_error() to prepend a filename:line:column marker
 * to the given error. The filename is taken from @builder, and
 * the line and column are obtained by calling
 * g_markup_parse_context_get_position().
 *
 * This is intended to be called on errors returned by
 * g_markup_collect_attributes() in a start_element vfunc.
 */
void
_gtk_builder_prefix_error (GtkBuilder                *builder,
                           GtkBuildableParseContext  *context,
                           GError                   **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  int line, col;

  gtk_buildable_parse_context_get_position (context, &line, &col);
  g_prefix_error (error, "%s:%d:%d ", priv->filename, line, col);
}

/*< private >
 * _gtk_builder_error_unhandled_tag:
 * @builder: a `GtkBuilder`
 * @context: the `GtkBuildableParseContext`
 * @object: name of the object that is being handled
 * @element_name: name of the element whose start tag is being handled
 * @error: return location for the error
 *
 * Sets @error to a suitable error indicating that an @element_name
 * tag is not expected in the custom markup for @object.
 *
 * This is intended to be called in a start_element vfunc.
 */
void
_gtk_builder_error_unhandled_tag (GtkBuilder                *builder,
                                  GtkBuildableParseContext  *context,
                                  const char                *object,
                                  const char                *element_name,
                                  GError                   **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  int line, col;

  gtk_buildable_parse_context_get_position (context, &line, &col);
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_UNHANDLED_TAG,
               "%s:%d:%d Unsupported tag for %s: <%s>",
               priv->filename, line, col,
               object, element_name);
}

/*< private >
 * @builder: a `GtkBuilder`
 * @context: the `GtkBuildableParseContext`
 * @parent_name: the name of the expected parent element
 * @error: return location for an error
 *
 * Checks that the parent element of the currently handled
 * start tag is @parent_name and set @error if it isn't.
 *
 * This is intended to be called in start_element vfuncs to
 * ensure that element nesting is as intended.
 *
 * Returns: %TRUE if @parent_name is the parent element
 */
gboolean
_gtk_builder_check_parent (GtkBuilder                *builder,
                           GtkBuildableParseContext  *context,
                           const char                *parent_name,
                           GError                   **error)
{
  return _gtk_builder_check_parents (builder, context, error, parent_name, NULL);
}

gboolean
_gtk_builder_check_parents (GtkBuilder                *builder,
                            GtkBuildableParseContext  *context,
                            GError                   **error,
                            ...)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GPtrArray *stack;
  int line, col;
  const char *parent;
  const char *element;
  va_list args;
  gboolean in_template;

  stack = gtk_buildable_parse_context_get_element_stack (context);

  element = g_ptr_array_index (stack, stack->len - 1);
  parent = stack->len > 1 ? g_ptr_array_index (stack, stack->len - 2) : "";

  in_template = g_str_equal (parent, "template");

  va_start (args, error);

  while (1) {
    char *parent_name = va_arg (args, char *);

    if (parent_name == NULL)
      break;

    if (g_str_equal (parent_name, parent) || (in_template && g_str_equal (parent_name, "object")))
      {
        va_end (args);
        return TRUE;
      }
  }

  va_end (args);

  gtk_buildable_parse_context_get_position (context, &line, &col);
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_INVALID_TAG,
               "%s:%d:%d Can't use <%s> here",
               priv->filename, line, col, element);

  return FALSE;
}

GObject *
gtk_builder_lookup_object (GtkBuilder   *builder,
                           const char   *name,
                           int           line,
                           int           col,
                           GError      **error)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GObject *obj;

  obj = g_hash_table_lookup (priv->objects, name);
  if (obj == NULL)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ID,
                   "%s:%d:%d Object with ID %s not found",
                   priv->filename, line, col, name);
    }

  return obj;
}

/*< private >
 * @builder: a `GtkBuilder`
 * @name: object name to look up
 * @line: line number where @name was encountered
 * @col: column number where @name was encountered
 *
 * Looks up an object by name. Similar to gtk_builder_get_object(),
 * but sets an error if lookup fails during custom_tag_end,
 * custom_finished or parser_finished vfuncs.
 *
 * The reason for doing things this way is that these vfuncs don't
 * take a GError** parameter to return an error.
 *
 * Returns: the found object
 */
GObject *
_gtk_builder_lookup_object (GtkBuilder  *builder,
                            const char *name,
                            int          line,
                            int          col)
{
  GtkBuilderPrivate *priv = gtk_builder_get_instance_private (builder);
  GObject *obj;
  GError *error = NULL;

  obj = g_hash_table_lookup (priv->objects, name);
  error = (GError *) g_object_get_data (G_OBJECT (builder), "lookup-error");

  if (!obj && !error)
    {
      g_set_error (&error,
                   GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ID,
                   "%s:%d:%d Object with ID %s not found",
                   priv->filename, line, col, name);
      g_object_set_data_full (G_OBJECT (builder), "lookup-error",
                              error, (GDestroyNotify)g_error_free);
    }

  return obj;
}

/*< private >
 * _gtk_builder_lookup_failed:
 * @GtkBuilder: a `GtkBuilder`
 * @error: return location for error
 *
 * Finds whether any object lookups have failed.
 *
 * Returns: %TRUE if @error has been set
 */
gboolean
_gtk_builder_lookup_failed (GtkBuilder  *builder,
                            GError     **error)
{
  GError *lookup_error;

  lookup_error = (GError*) g_object_steal_data (G_OBJECT (builder), "lookup-error");
  if (lookup_error)
    {
      g_propagate_error (error, lookup_error);
      return TRUE;
    }

  return FALSE;
}
