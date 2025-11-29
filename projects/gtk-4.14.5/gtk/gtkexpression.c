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

#include "gtkexpression.h"

#include "gtkprivate.h"

#include <gobject/gvaluecollector.h>

/**
 * GtkExpression: (ref-func gtk_expression_ref) (unref-func gtk_expression_unref) (set-value-func gtk_value_set_expression) (get-value-func gtk_value_get_expression)
 *
 * `GtkExpression` provides a way to describe references to values.
 *
 * An important aspect of expressions is that the value can be obtained
 * from a source that is several steps away. For example, an expression
 * may describe ‘the value of property A of `object1`, which is itself the
 * value of a property of `object2`’. And `object1` may not even exist yet
 * at the time that the expression is created. This is contrast to `GObject`
 * property bindings, which can only create direct connections between
 * the properties of two objects that must both exist for the duration
 * of the binding.
 *
 * An expression needs to be "evaluated" to obtain the value that it currently
 * refers to. An evaluation always happens in the context of a current object
 * called `this` (it mirrors the behavior of object-oriented languages),
 * which may or may not influence the result of the evaluation. Use
 * [method@Gtk.Expression.evaluate] for evaluating an expression.
 *
 * Various methods for defining expressions exist, from simple constants via
 * [ctor@Gtk.ConstantExpression.new] to looking up properties in a `GObject`
 * (even recursively) via [ctor@Gtk.PropertyExpression.new] or providing
 * custom functions to transform and combine expressions via
 * [ctor@Gtk.ClosureExpression.new].
 *
 * Here is an example of a complex expression:
 *
 * ```c
 *   color_expr = gtk_property_expression_new (GTK_TYPE_LIST_ITEM,
 *                                             NULL, "item");
 *   expression = gtk_property_expression_new (GTK_TYPE_COLOR,
 *                                             color_expr, "name");
 * ```
 *
 * when evaluated with `this` being a `GtkListItem`, it will obtain the
 * "item" property from the `GtkListItem`, and then obtain the "name" property
 * from the resulting object (which is assumed to be of type `GTK_TYPE_COLOR`).
 *
 * A more concise way to describe this would be
 *
 * ```
 *   this->item->name
 * ```
 *
 * The most likely place where you will encounter expressions is in the context
 * of list models and list widgets using them. For example, `GtkDropDown` is
 * evaluating a `GtkExpression` to obtain strings from the items in its model
 * that it can then use to match against the contents of its search entry.
 * `GtkStringFilter` is using a `GtkExpression` for similar reasons.
 *
 * By default, expressions are not paying attention to changes and evaluation is
 * just a snapshot of the current state at a given time. To get informed about
 * changes, an expression needs to be "watched" via a [struct@Gtk.ExpressionWatch],
 * which will cause a callback to be called whenever the value of the expression may
 * have changed; [method@Gtk.Expression.watch] starts watching an expression, and
 * [method@Gtk.ExpressionWatch.unwatch] stops.
 *
 * Watches can be created for automatically updating the property of an object,
 * similar to GObject's `GBinding` mechanism, by using [method@Gtk.Expression.bind].
 *
 * ## GtkExpression in GObject properties
 *
 * In order to use a `GtkExpression` as a `GObject` property, you must use the
 * [func@Gtk.param_spec_expression] when creating a `GParamSpec` to install in the
 * `GObject` class being defined; for instance:
 *
 * ```c
 * obj_props[PROP_EXPRESSION] =
 *   gtk_param_spec_expression ("expression",
 *                              "Expression",
 *                              "The expression used by the widget",
 *                              G_PARAM_READWRITE |
 *                              G_PARAM_STATIC_STRINGS |
 *                              G_PARAM_EXPLICIT_NOTIFY);
 * ```
 *
 * When implementing the `GObjectClass.set_property` and `GObjectClass.get_property`
 * virtual functions, you must use [func@Gtk.value_get_expression], to retrieve the
 * stored `GtkExpression` from the `GValue` container, and [func@Gtk.value_set_expression],
 * to store the `GtkExpression` into the `GValue`; for instance:
 *
 * ```c
 *   // in set_property()...
 *   case PROP_EXPRESSION:
 *     foo_widget_set_expression (foo, gtk_value_get_expression (value));
 *     break;
 *
 *   // in get_property()...
 *   case PROP_EXPRESSION:
 *     gtk_value_set_expression (value, foo->expression);
 *     break;
 * ```
 *
 * ## GtkExpression in .ui files
 *
 * `GtkBuilder` has support for creating expressions. The syntax here can be used where
 * a `GtkExpression` object is needed like in a `<property>` tag for an expression
 * property, or in a `<binding name="property">` tag to bind a property to an expression.
 *
 * To create a property expression, use the `<lookup>` element. It can have a `type`
 * attribute to specify the object type, and a `name` attribute to specify the property
 * to look up. The content of `<lookup>` can either be an element specifying the expression
 * to use the object, or a string that specifies the name of the object to use.
 *
 * Example:
 *
 * ```xml
 *   <lookup name='search'>string_filter</lookup>
 * ```
 *
 * Since the `<lookup>` element creates an expression and its element content can
 * itself be an expression, this means that `<lookup>` tags can also be nested.
 * This is a common idiom when dealing with `GtkListItem`s. See
 * [class@Gtk.BuilderListItemFactory] for an example of this technique.
 *
 * To create a constant expression, use the `<constant>` element. If the type attribute
 * is specified, the element content is interpreted as a value of that type. Otherwise,
 * it is assumed to be an object. For instance:
 *
 * ```xml
 *   <constant>string_filter</constant>
 *   <constant type='gchararray'>Hello, world</constant>
 * ```
 *
 * To create a closure expression, use the `<closure>` element. The `function`
 * attribute specifies what function to use for the closure, and the `type`
 * attribute specifies its return type. The content of the element contains the
 * expressions for the parameters. For instance:
 *
 * ```xml
 *   <closure type='gchararray' function='combine_args_somehow'>
 *     <constant type='gchararray'>File size:</constant>
 *     <lookup type='GFile' name='size'>myfile</lookup>
 *   </closure>
 * ```
 *
 * To create a property binding, use the `<binding>` element in place of where a
 * `<property>` tag would ordinarily be used. The `name` and `object` attributes are
 * supported. The `name` attribute is required, and pertains to the applicable property
 * name. The `object` attribute is optional. If provided, it will use the specified object
 * as the `this` object when the expression is evaluated. Here is an example in which the
 * `label` property of a `GtkLabel` is bound to the `string` property of another arbitrary
 * object:
 *
 * ```xml
 *   <object class='GtkLabel'>
 *     <binding name='label'>
 *       <lookup name='string'>some_other_object</lookup>
 *     </binding>
 *   </object>
 * ```
 */

typedef struct _WeakRefGuard WeakRefGuard;

struct _WeakRefGuard
{
  gatomicrefcount ref_count;
  gpointer data;
};

static WeakRefGuard *
weak_ref_guard_new (gpointer data)
{
  WeakRefGuard *guard;

  guard = g_new0 (WeakRefGuard, 1);
  g_atomic_ref_count_init (&guard->ref_count);
  guard->data = data;

  return guard;
}

static WeakRefGuard *
weak_ref_guard_ref (WeakRefGuard *guard)
{
  g_atomic_ref_count_inc (&guard->ref_count);
  return guard;
}

static void
weak_ref_guard_unref (WeakRefGuard *guard)
{
  /* Always clear data pointer after first unref so that it
   * cannot be accessed unless both the expression/watch is
   * valid _and_ the weak ref is still active.
   */
  guard->data = NULL;

  if (g_atomic_ref_count_dec (&guard->ref_count))
    g_free (guard);
}

typedef struct _GtkExpressionClass GtkExpressionClass;
typedef struct _GtkExpressionSubWatch GtkExpressionSubWatch;
typedef struct _GtkExpressionTypeInfo GtkExpressionTypeInfo;

struct _GtkExpression
{
  GTypeInstance parent_instance;

  gatomicrefcount ref_count;

  GType value_type;

  GtkExpression *owner;
};

struct _GtkExpressionClass
{
  GTypeClass parent_class;

  void                  (* finalize)            (GtkExpression          *expr);
  gboolean              (* is_static)           (GtkExpression          *expr);
  gboolean              (* evaluate)            (GtkExpression          *expr,
                                                 gpointer                this,
                                                 GValue                 *value);

  gsize                 (* watch_size)          (GtkExpression          *expr);
  void                  (* watch)               (GtkExpression          *self,
                                                 GtkExpressionSubWatch  *watch,
                                                 gpointer                this_,
                                                 GtkExpressionNotify     notify,
                                                 gpointer                user_data);
  void                  (* unwatch)             (GtkExpression          *self,
                                                 GtkExpressionSubWatch  *watch);
};

struct _GtkExpressionTypeInfo
{
  gsize instance_size;

  void                  (* instance_init)       (GtkExpression          *expr);
  void                  (* finalize)            (GtkExpression          *expr);
  gboolean              (* is_static)           (GtkExpression          *expr);
  gboolean              (* evaluate)            (GtkExpression          *expr,
                                                 gpointer                this,
                                                 GValue                 *value);

  gsize                 (* watch_size)          (GtkExpression          *expr);
  void                  (* watch)               (GtkExpression          *self,
                                                 GtkExpressionSubWatch  *watch,
                                                 gpointer                this_,
                                                 GtkExpressionNotify     notify,
                                                 gpointer                user_data);
  void                  (* unwatch)             (GtkExpression          *self,
                                                 GtkExpressionSubWatch  *watch);
};

/**
 * GtkExpressionWatch:
 *
 * An opaque structure representing a watched `GtkExpression`.
 *
 * The contents of `GtkExpressionWatch` should only be accessed through the
 * provided API.
 */
struct _GtkExpressionWatch
{
  GtkExpression         *expression;
  WeakRefGuard          *guard;
  GWeakRef               this_wr;
  GDestroyNotify         user_destroy;
  GtkExpressionNotify    notify;
  gpointer               user_data;
  guchar                 sub[0];
};

G_DEFINE_BOXED_TYPE (GtkExpressionWatch, gtk_expression_watch,
                     gtk_expression_watch_ref,
                     gtk_expression_watch_unref)

#define GTK_EXPRESSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_EXPRESSION, GtkExpressionClass))

/*< private >
 * GTK_DEFINE_EXPRESSION_TYPE:
 * @TypeName: the type name, in camel case
 * @type_name: the type name, in snake case
 * @type_info: the address of the `GtkExpressionTypeInfo` for the expression type
 *
 * Registers a new `GtkExpression` subclass with the given @TypeName and @type_info.
 *
 * Similarly to %G_DEFINE_TYPE, this macro will generate a `get_type()`
 * function that registers the event type.
 *
 * You can specify code to be run after the type registration; the `GType` of
 * the event is available in the `gtk_define_expression_type_id` variable.
 */
#define GTK_DEFINE_EXPRESSION_TYPE(TypeName, type_name, type_info) \
GType \
type_name ## _get_type (void) \
{ \
  static gsize gtk_define_expression_type_id__volatile; \
  if (g_once_init_enter (&gtk_define_expression_type_id__volatile)) \
    { \
      GType gtk_define_expression_type_id = \
        gtk_expression_type_register_static (g_intern_static_string (#TypeName), type_info); \
      g_once_init_leave (&gtk_define_expression_type_id__volatile, gtk_define_expression_type_id); \
    } \
  return gtk_define_expression_type_id__volatile; \
}

#define GTK_EXPRESSION_SUPER(expr) \
  ((GtkExpressionClass *) g_type_class_peek (g_type_parent (G_TYPE_FROM_INSTANCE (expr))))

/* {{{ GtkExpression internals */

static void
value_expression_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_expression_free_value (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    gtk_expression_unref (value->data[0].v_pointer);
}

static void
value_expression_copy_value (const GValue *src,
                             GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = gtk_expression_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}

static gpointer
value_expression_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static char *
value_expression_collect_value (GValue      *value,
                                guint        n_collect_values,
                                GTypeCValue *collect_values,
                                guint        collect_flags)
{
  GtkExpression *expression = collect_values[0].v_pointer;

  if (expression == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (expression->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed GtkExpression pointer for "
                        "value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = gtk_expression_ref (expression);

  return NULL;
}

static char *
value_expression_lcopy_value (const GValue *value,
                              guint         n_collect_values,
                              GTypeCValue  *collect_values,
                              guint         collect_flags)
{
  GtkExpression **expression_p = collect_values[0].v_pointer;

  if (expression_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *expression_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *expression_p = value->data[0].v_pointer;
  else
    *expression_p = gtk_expression_ref (value->data[0].v_pointer);

  return NULL;
}

/**
 * gtk_value_set_expression:
 * @value: a `GValue` initialized with type `GTK_TYPE_EXPRESSION`
 * @expression: a `GtkExpression`
 *
 * Stores the given `GtkExpression` inside `value`.
 *
 * The `GValue` will acquire a reference to the `expression`.
 */
void
gtk_value_set_expression (GValue        *value,
                          GtkExpression *expression)
{
  g_return_if_fail (G_VALUE_HOLDS (value, GTK_TYPE_EXPRESSION));

  GtkExpression *old_expression = value->data[0].v_pointer;

  if (expression != NULL)
    {
      g_return_if_fail (GTK_IS_EXPRESSION (expression));

      value->data[0].v_pointer = gtk_expression_ref (expression);
    }
  else
    {
      value->data[0].v_pointer = NULL;
    }

  if (old_expression != NULL)
    gtk_expression_unref (old_expression);
}

/**
 * gtk_value_take_expression:
 * @value: a `GValue` initialized with type `GTK_TYPE_EXPRESSION`
 * @expression: (transfer full) (nullable): a `GtkExpression`
 *
 * Stores the given `GtkExpression` inside `value`.
 *
 * This function transfers the ownership of the `expression` to the `GValue`.
 */
void
gtk_value_take_expression (GValue        *value,
                           GtkExpression *expression)
{
  g_return_if_fail (G_VALUE_HOLDS (value, GTK_TYPE_EXPRESSION));

  GtkExpression *old_expression = value->data[0].v_pointer;

  if (expression != NULL)
    {
      g_return_if_fail (GTK_IS_EXPRESSION (expression));

      value->data[0].v_pointer = expression;
    }
  else
    {
      value->data[0].v_pointer = NULL;
    }

  if (old_expression != NULL)
    gtk_expression_unref (old_expression);
}

/**
 * gtk_value_get_expression:
 * @value: a `GValue` initialized with type `GTK_TYPE_EXPRESSION`
 *
 * Retrieves the `GtkExpression` stored inside the given `value`.
 *
 * Returns: (transfer none) (nullable): a `GtkExpression`
 */
GtkExpression *
gtk_value_get_expression (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, GTK_TYPE_EXPRESSION), NULL);

  return value->data[0].v_pointer;
}

/**
 * gtk_value_dup_expression:
 * @value: a `GValue` initialized with type `GTK_TYPE_EXPRESSION`
 *
 * Retrieves the `GtkExpression` stored inside the given `value`, and acquires
 * a reference to it.
 *
 * Returns: (transfer full) (nullable): a `GtkExpression`
 */
GtkExpression *
gtk_value_dup_expression (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, GTK_TYPE_EXPRESSION), NULL);

  if (value->data[0].v_pointer == NULL)
    return NULL;

  GtkExpression *expression = value->data[0].v_pointer;

  return gtk_expression_ref (expression);
}

static void
param_expression_init (GParamSpec *pspec)
{
}

static void
param_expression_set_default (GParamSpec *pspec,
                              GValue     *value)
{
  value->data[0].v_pointer = NULL;
}

static gboolean
param_expression_validate (GParamSpec *pspec,
                           GValue     *value)
{
  GtkParamSpecExpression *espec = GTK_PARAM_SPEC_EXPRESSION (pspec);
  GtkExpression *expr = value->data[0].v_pointer;
  guint changed = 0;

  if (expr != NULL &&
      !g_value_type_compatible (G_TYPE_FROM_INSTANCE (expr), G_PARAM_SPEC_VALUE_TYPE (espec)))
    {
      gtk_expression_unref (expr);
      value->data[0].v_pointer = NULL;
      changed++;
    }

  return changed;
}

static int
param_expression_values_cmp (GParamSpec   *pspec,
                             const GValue *value1,
                             const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  return p1 < p2 ? -1 : p1 > p2;
}

GType
gtk_param_expression_get_type (void)
{
  static gsize param_expression_type__volatile;

  if (g_once_init_enter (&param_expression_type__volatile))
    {
      const GParamSpecTypeInfo pspec_info = {
        sizeof (GtkParamSpecExpression),
        16,
        param_expression_init,
        GTK_TYPE_EXPRESSION,
        NULL,
        param_expression_set_default,
        param_expression_validate,
        param_expression_values_cmp,
      };

      GType param_expression_type =
        g_param_type_register_static (g_intern_static_string ("GtkParamSpecExpression"),
                                      &pspec_info);

      g_once_init_leave (&param_expression_type__volatile, param_expression_type);
    }

  return param_expression_type__volatile;
}

/**
 * gtk_param_spec_expression:
 * @name: canonical name of the property
 * @nick: a user-readable name for the property
 * @blurb: a user-readable description of the property
 * @flags: flags for the property
 *
 * Creates a new `GParamSpec` instance for a property holding a `GtkExpression`.
 *
 * See `g_param_spec_internal()` for details on the property strings.
 *
 * Returns: (transfer full): a newly created property specification
 */
GParamSpec *
gtk_param_spec_expression (const char  *name,
                           const char  *nick,
                           const char  *blurb,
                           GParamFlags  flags)
{
  GParamSpec *pspec = g_param_spec_internal (GTK_TYPE_PARAM_SPEC_EXPRESSION,
                                             name, nick, blurb,
                                             flags);

  pspec->value_type = GTK_TYPE_EXPRESSION;

  return pspec;
}

/* }}} */

/* {{{ GtkExpression internals */

static void
gtk_expression_real_finalize (GtkExpression *self)
{
  g_type_free_instance ((GTypeInstance *) self);
}

static gsize
gtk_expression_real_watch_size (GtkExpression *self)
{
  return 0;
}

static void
gtk_expression_real_watch (GtkExpression         *self,
                           GtkExpressionSubWatch *watch,
                           gpointer               this_,
                           GtkExpressionNotify    notify,
                           gpointer               user_data)
{
}

static void
gtk_expression_real_unwatch (GtkExpression         *self,
                             GtkExpressionSubWatch *watch)
{
}

static gsize
gtk_expression_watch_size (GtkExpression *self)
{
  return GTK_EXPRESSION_GET_CLASS (self)->watch_size (self);
}

static void
gtk_expression_class_init (GtkExpressionClass *klass)
{
  klass->finalize = gtk_expression_real_finalize;
  klass->watch_size = gtk_expression_real_watch_size;
  klass->watch = gtk_expression_real_watch;
  klass->unwatch = gtk_expression_real_unwatch;
}

static void
gtk_expression_init (GtkExpression *self)
{
  g_atomic_ref_count_init (&self->ref_count);
}

GType
gtk_expression_get_type (void)
{
  static gsize expression_type__volatile;

  if (g_once_init_enter (&expression_type__volatile))
    {
      static const GTypeFundamentalInfo finfo = {
        (G_TYPE_FLAG_CLASSED |
         G_TYPE_FLAG_INSTANTIATABLE |
         G_TYPE_FLAG_DERIVABLE |
         G_TYPE_FLAG_DEEP_DERIVABLE),
      };

      static const GTypeValueTable value_table = {
        value_expression_init,
        value_expression_free_value,
        value_expression_copy_value,
        value_expression_peek_pointer,
        "p",
        value_expression_collect_value,
        "p",
        value_expression_lcopy_value,
      };

      const GTypeInfo event_info = {
        /* Class */
        sizeof (GtkExpressionClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gtk_expression_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,

        /* Instance */
        sizeof (GtkExpression),
        0,
        (GInstanceInitFunc) gtk_expression_init,

        /* GValue */
        &value_table,
      };

      GType expression_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("GtkExpression"),
                                     &event_info, &finfo,
                                     G_TYPE_FLAG_ABSTRACT);

      g_once_init_leave (&expression_type__volatile, expression_type);
    }

  return expression_type__volatile;
}

static void
gtk_expression_generic_class_init (gpointer g_class,
                                   gpointer class_data)
{
  GtkExpressionTypeInfo *info = class_data;
  GtkExpressionClass *expression_class = g_class;

  /* Mandatory */
  expression_class->is_static = info->is_static;
  expression_class->evaluate = info->evaluate;

  /* Optional */
  if (info->finalize != NULL)
    expression_class->finalize = info->finalize;
  if (info->watch_size != NULL)
    expression_class->watch_size = info->watch_size;
  if (info->watch != NULL)
    expression_class->watch = info->watch;
  if (info->unwatch != NULL)
    expression_class->unwatch = info->unwatch;

  g_free (info);
}

static GType
gtk_expression_type_register_static (const char                  *type_name,
                                     const GtkExpressionTypeInfo *type_info)
{
  GTypeInfo info;

  info.class_size = sizeof (GtkExpressionClass);
  info.base_init = NULL;
  info.base_finalize = NULL;
  info.class_init = gtk_expression_generic_class_init;
  info.class_finalize = NULL;
  info.class_data = g_memdup2 (type_info, sizeof (GtkExpressionTypeInfo));

  info.instance_size = type_info->instance_size;
  info.n_preallocs = 0;
  info.instance_init = (GInstanceInitFunc) type_info->instance_init;
  info.value_table = NULL;

  return g_type_register_static (GTK_TYPE_EXPRESSION, type_name, &info, 0);
}

/*< private >
 * gtk_expression_alloc:
 * @expression_type: the type of expression to create
 * @value_type: the type of the value returned by this expression
 *
 * Returns: (transfer full): the newly created `GtkExpression`
 */
static gpointer
gtk_expression_alloc (GType expression_type,
                      GType value_type)
{
  GtkExpression *self;

  self = (GtkExpression *) g_type_create_instance (expression_type);

  self->value_type = value_type;

  return self;
}

static void
gtk_expression_subwatch_init (GtkExpression         *self,
                              GtkExpressionSubWatch *watch,
                              gpointer               this,
                              GtkExpressionNotify    notify,
                              gpointer               user_data)
{
  GTK_EXPRESSION_GET_CLASS (self)->watch (self, watch, this, notify, user_data);
}

static void
gtk_expression_subwatch_finish (GtkExpression         *self,
                                GtkExpressionSubWatch *watch)
{
  GTK_EXPRESSION_GET_CLASS (self)->unwatch (self, watch);
}

/* }}} */

/* {{{ GtkConstantExpression */

/**
 * GtkConstantExpression:
 *
 * A constant value in a `GtkExpression`.
 */
struct _GtkConstantExpression
{
  GtkExpression parent;

  GValue value;
};

static void
gtk_constant_expression_finalize (GtkExpression *expr)
{
  GtkConstantExpression *self = (GtkConstantExpression *) expr;

  g_value_unset (&self->value);

  GTK_EXPRESSION_SUPER (expr)->finalize (expr);
}

static gboolean
gtk_constant_expression_is_static (GtkExpression *expr)
{
  return TRUE;
}

static gboolean
gtk_constant_expression_evaluate (GtkExpression *expr,
                                  gpointer       this,
                                  GValue        *value)
{
  GtkConstantExpression *self = (GtkConstantExpression *) expr;

  g_value_init (value, G_VALUE_TYPE (&self->value));
  g_value_copy (&self->value, value);
  return TRUE;
}

static const GtkExpressionTypeInfo gtk_constant_expression_info =
{
  sizeof (GtkConstantExpression),
  NULL,
  gtk_constant_expression_finalize,
  gtk_constant_expression_is_static,
  gtk_constant_expression_evaluate,
  NULL,
  NULL,
  NULL,
};

GTK_DEFINE_EXPRESSION_TYPE (GtkConstantExpression,
                            gtk_constant_expression,
                            &gtk_constant_expression_info)

/**
 * gtk_constant_expression_new:
 * @value_type: The type of the object
 * @...: arguments to create the object from
 *
 * Creates a `GtkExpression` that evaluates to the
 * object given by the arguments.
 *
 * Returns: (transfer full) (type GtkConstantExpression): a new `GtkExpression`
 */
GtkExpression *
gtk_constant_expression_new (GType value_type,
                             ...)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *result;
  va_list args;
  char *error;

  va_start (args, value_type);
  G_VALUE_COLLECT_INIT (&value, value_type,
                        args, G_VALUE_NOCOPY_CONTENTS,
                        &error);
  if (error)
    {
      g_critical ("%s: %s", G_STRLOC, error);
      g_free (error);
      /* we purposely leak the value here, it might not be
       * in a sane state if an error condition occurred
       */
      return NULL;
    }

  result = gtk_constant_expression_new_for_value (&value);

  g_value_unset (&value);
  va_end (args);

  return result;
}

/**
 * gtk_constant_expression_new_for_value: (constructor)
 * @value: a `GValue`
 *
 * Creates an expression that always evaluates to the given `value`.
 *
 * Returns: (transfer full) (type GtkConstantExpression):  a new `GtkExpression`
 **/
GtkExpression *
gtk_constant_expression_new_for_value (const GValue *value)
{
  GtkExpression *result;
  GtkConstantExpression *self;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  result = gtk_expression_alloc (GTK_TYPE_CONSTANT_EXPRESSION, G_VALUE_TYPE (value));
  self = (GtkConstantExpression *) result;

  g_value_init (&self->value, G_VALUE_TYPE (value));
  g_value_copy (value, &self->value);

  return result;
}

/**
 * gtk_constant_expression_get_value:
 * @expression: (type GtkConstantExpression): a constant `GtkExpression`
 *
 * Gets the value that a constant expression evaluates to.
 *
 * Returns: (transfer none): the value
 */
const GValue *
gtk_constant_expression_get_value (GtkExpression *expression)
{
  GtkConstantExpression *self = (GtkConstantExpression *) expression;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_CONSTANT_EXPRESSION), NULL);

  return &self->value;
}

/* }}} */

/* {{{ GtkObjectExpression */

typedef struct _GtkObjectExpressionWatch GtkObjectExpressionWatch;

/**
 * GtkObjectExpression:
 *
 * A `GObject` value in a `GtkExpression`.
 */
struct _GtkObjectExpression
{
  GtkExpression parent;

  WeakRefGuard *guard;
  GWeakRef object_wr;
  GSList *watches;
};

struct _GtkObjectExpressionWatch
{
  GtkExpressionNotify    notify;
  gpointer               user_data;
};

static void
gtk_object_expression_weak_ref_cb (gpointer  data,
                                   GObject  *object)
{
  WeakRefGuard *guard = data;
  GtkObjectExpression *self = guard->data;

  if (self != NULL)
    {
      GSList *iter = self->watches;

      while (iter)
        {
          GtkObjectExpressionWatch *owatch = iter->data;
          iter = iter->next;
          owatch->notify (owatch->user_data);
        }
    }

  weak_ref_guard_unref (guard);
}

static void
gtk_object_expression_finalize (GtkExpression *expr)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;
  GObject *object;

  object = g_weak_ref_get (&self->object_wr);

  if (object != NULL)
    {
      g_object_weak_unref (object, gtk_object_expression_weak_ref_cb, self->guard);
      weak_ref_guard_unref (self->guard);
      g_object_unref (object);
    }
  else
    {
      /* @object has been disposed. Which means that either our
       * gtk_object_expression_weak_ref_cb has been called or we
       * can expect it to be called shortly after this. No need to
       * call g_object_weak_unref() or unref the handle which will
       * be unref'ed by that callback.
       */
    }

  g_clear_pointer (&self->guard, weak_ref_guard_unref);
  g_weak_ref_clear (&self->object_wr);

  g_assert (self->watches == NULL);

  GTK_EXPRESSION_SUPER (expr)->finalize (expr);
}

static gboolean
gtk_object_expression_is_static (GtkExpression *expr)
{
  return FALSE;
}

static gboolean
gtk_object_expression_evaluate (GtkExpression *expr,
                                gpointer       this,
                                GValue        *value)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;
  GObject *object;

  object = g_weak_ref_get (&self->object_wr);
  if (object == NULL)
    return FALSE;

  g_value_init (value, gtk_expression_get_value_type (expr));
  g_value_take_object (value, object);
  return TRUE;
}

static gsize
gtk_object_expression_watch_size (GtkExpression *expr)
{
  return sizeof (GtkObjectExpressionWatch);
}

static void
gtk_object_expression_watch (GtkExpression         *expr,
                             GtkExpressionSubWatch *watch,
                             gpointer               this_,
                             GtkExpressionNotify    notify,
                             gpointer               user_data)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;
  GtkObjectExpressionWatch *owatch = (GtkObjectExpressionWatch *) watch;

  owatch->notify = notify;
  owatch->user_data = user_data;
  self->watches = g_slist_prepend (self->watches, owatch);
}

static void
gtk_object_expression_unwatch (GtkExpression         *expr,
                               GtkExpressionSubWatch *watch)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  self->watches = g_slist_remove (self->watches, watch);
}

static const GtkExpressionTypeInfo gtk_object_expression_info =
{
  sizeof (GtkObjectExpression),
  NULL,
  gtk_object_expression_finalize,
  gtk_object_expression_is_static,
  gtk_object_expression_evaluate,
  gtk_object_expression_watch_size,
  gtk_object_expression_watch,
  gtk_object_expression_unwatch
};

GTK_DEFINE_EXPRESSION_TYPE (GtkObjectExpression,
                            gtk_object_expression,
                            &gtk_object_expression_info)

/**
 * gtk_object_expression_new: (constructor)
 * @object: (transfer none): object to watch
 *
 * Creates an expression evaluating to the given `object` with a weak reference.
 *
 * Once the `object` is disposed, it will fail to evaluate.
 *
 * This expression is meant to break reference cycles.
 *
 * If you want to keep a reference to `object`, use [ctor@Gtk.ConstantExpression.new].
 *
 * Returns: (type GtkObjectExpression) (transfer full): a new `GtkExpression`
 **/
GtkExpression *
gtk_object_expression_new (GObject *object)
{
  GtkExpression *result;
  GtkObjectExpression *self;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  result = gtk_expression_alloc (GTK_TYPE_OBJECT_EXPRESSION, G_OBJECT_TYPE (object));

  self = (GtkObjectExpression *) result;
  g_weak_ref_init (&self->object_wr, object);
  self->guard = weak_ref_guard_new (self);

  g_object_weak_ref (object,
                     gtk_object_expression_weak_ref_cb,
                     weak_ref_guard_ref (self->guard));

  return result;
}

/**
 * gtk_object_expression_get_object:
 * @expression: (type GtkObjectExpression): an object `GtkExpression`
 *
 * Gets the object that the expression evaluates to.
 *
 * Returns: (transfer none) (nullable): the object, or `NULL`
 */
GObject *
gtk_object_expression_get_object (GtkExpression *expression)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expression;
  GObject *object;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_OBJECT_EXPRESSION), NULL);

  object = g_weak_ref_get (&self->object_wr);

  /* Return a borrowed instance */
  if (object != NULL)
    g_object_unref (object);

  return object;
}

/* }}} */

/* {{{ GtkPropertyExpression */

/**
 * GtkPropertyExpression:
 *
 * A `GObject` property value in a `GtkExpression`.
 */
struct _GtkPropertyExpression
{
  GtkExpression parent;

  GtkExpression *expr;

  GParamSpec *pspec;
};

static void
gtk_property_expression_finalize (GtkExpression *expr)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  g_clear_pointer (&self->expr, gtk_expression_unref);

  GTK_EXPRESSION_SUPER (expr)->finalize (expr);
}

static gboolean
gtk_property_expression_is_static (GtkExpression *expr)
{
  return FALSE;
}

static GObject *
gtk_property_expression_get_object (GtkPropertyExpression *self,
                                    gpointer               this)
{
  GValue expr_value = G_VALUE_INIT;
  GObject *object;

  if (self->expr == NULL)
    {
      if (this)
        return g_object_ref (this);
      else
        return NULL;
    }

  if (!gtk_expression_evaluate (self->expr, this, &expr_value))
    return NULL;

  if (!G_VALUE_HOLDS_OBJECT (&expr_value))
    {
      g_value_unset (&expr_value);
      return NULL;
    }

  object = g_value_dup_object (&expr_value);
  g_value_unset (&expr_value);
  if (object == NULL)
    return NULL;

  if (!G_TYPE_CHECK_INSTANCE_TYPE (object, self->pspec->owner_type))
    {
      g_object_unref (object);
      return NULL;
    }

  return object;
}

static gboolean
gtk_property_expression_evaluate (GtkExpression *expr,
                                  gpointer       this,
                                  GValue        *value)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;
  GObject *object;

  object = gtk_property_expression_get_object (self, this);
  if (object == NULL)
    return FALSE;

  g_object_get_property (object, self->pspec->name, value);
  g_object_unref (object);
  return TRUE;
}

typedef struct _GtkPropertyExpressionWatch GtkPropertyExpressionWatch;

struct _GtkPropertyExpressionWatch
{
  GtkExpressionNotify    notify;
  gpointer               user_data;

  GtkPropertyExpression *expr;
  gpointer               this;
  GClosure              *closure;
  guchar                 sub[0];
};

static void
gtk_property_expression_watch_destroy_closure (GtkPropertyExpressionWatch *pwatch)
{
  if (pwatch->closure == NULL)
    return;

  g_closure_invalidate (pwatch->closure);
  g_closure_unref (pwatch->closure);
  pwatch->closure = NULL;
}

static void
gtk_property_expression_watch_notify_cb (GObject                    *object,
                                         GParamSpec                 *pspec,
                                         GtkPropertyExpressionWatch *pwatch)
{
  pwatch->notify (pwatch->user_data);
}

static void
gtk_property_expression_watch_create_closure (GtkPropertyExpressionWatch *pwatch)
{
  GObject *object;

  object = gtk_property_expression_get_object (pwatch->expr, pwatch->this);
  if (object == NULL)
    return;

  pwatch->closure = g_cclosure_new (G_CALLBACK (gtk_property_expression_watch_notify_cb), pwatch, NULL);
  if (!g_signal_connect_closure_by_id (object,
                                       g_signal_lookup ("notify", G_OBJECT_TYPE (object)),
                                       g_quark_from_string (pwatch->expr->pspec->name),
                                       g_closure_ref (pwatch->closure),
                                       FALSE))
    {
      g_assert_not_reached ();
    }

  g_object_unref (object);
}

static void
gtk_property_expression_watch_expr_notify_cb (gpointer data)
{
  GtkPropertyExpressionWatch *pwatch = data;

  gtk_property_expression_watch_destroy_closure (pwatch);
  gtk_property_expression_watch_create_closure (pwatch);
  pwatch->notify (pwatch->user_data);
}

static gsize
gtk_property_expression_watch_size (GtkExpression *expr)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;
  gsize result;

  result = sizeof (GtkPropertyExpressionWatch);
  if (self->expr)
    result += gtk_expression_watch_size (self->expr);

  return result;
}

static void
gtk_property_expression_watch (GtkExpression         *expr,
                               GtkExpressionSubWatch *watch,
                               gpointer               this_,
                               GtkExpressionNotify    notify,
                               gpointer               user_data)
{
  GtkPropertyExpressionWatch *pwatch = (GtkPropertyExpressionWatch *) watch;
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  pwatch->notify = notify;
  pwatch->user_data = user_data;
  pwatch->expr = self;
  pwatch->this = this_;
  if (self->expr && !gtk_expression_is_static (self->expr))
    {
      gtk_expression_subwatch_init (self->expr,
                                    (GtkExpressionSubWatch *) pwatch->sub,
                                    this_,
                                    gtk_property_expression_watch_expr_notify_cb,
                                    pwatch);
    }

  gtk_property_expression_watch_create_closure (pwatch);
}

static void
gtk_property_expression_unwatch (GtkExpression         *expr,
                                 GtkExpressionSubWatch *watch)
{
  GtkPropertyExpressionWatch *pwatch = (GtkPropertyExpressionWatch *) watch;
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  gtk_property_expression_watch_destroy_closure (pwatch);

  if (self->expr && !gtk_expression_is_static (self->expr))
    gtk_expression_subwatch_finish (self->expr, (GtkExpressionSubWatch *) pwatch->sub);
}

static const GtkExpressionTypeInfo gtk_property_expression_info =
{
  sizeof (GtkPropertyExpression),
  NULL,
  gtk_property_expression_finalize,
  gtk_property_expression_is_static,
  gtk_property_expression_evaluate,
  gtk_property_expression_watch_size,
  gtk_property_expression_watch,
  gtk_property_expression_unwatch
};

GTK_DEFINE_EXPRESSION_TYPE (GtkPropertyExpression,
                            gtk_property_expression,
                            &gtk_property_expression_info)

/**
 * gtk_property_expression_new: (constructor)
 * @this_type: The type to expect for the this type
 * @expression: (nullable) (transfer full): Expression to
 *   evaluate to get the object to query or `NULL` to
 *   query the `this` object
 * @property_name: name of the property
 *
 * Creates an expression that looks up a property.
 *
 * The object to use is found by evaluating the `expression`,
 * or using the `this` argument when `expression` is `NULL`.
 *
 * If the resulting object conforms to `this_type`, its property named
 * `property_name` will be queried. Otherwise, this expression's
 * evaluation will fail.
 *
 * The given `this_type` must have a property with `property_name`.
 *
 * Returns: (type GtkPropertyExpression) (transfer full): a new `GtkExpression`
 **/
GtkExpression *
gtk_property_expression_new (GType          this_type,
                             GtkExpression *expression,
                             const char    *property_name)
{
  GParamSpec *pspec;

  if (g_type_fundamental (this_type) == G_TYPE_OBJECT)
    {
      GObjectClass *class = g_type_class_ref (this_type);
      pspec = g_object_class_find_property (class, property_name);
      g_type_class_unref (class);
    }
  else if (g_type_fundamental (this_type) == G_TYPE_INTERFACE)
    {
      GTypeInterface *iface = g_type_default_interface_ref (this_type);
      pspec = g_object_interface_find_property (iface, property_name);
      g_type_default_interface_unref (iface);
    }
  else
    {
      g_critical ("Type `%s` does not support properties", g_type_name (this_type));
      return NULL;
    }

  if (pspec == NULL)
    {
      g_critical ("Type `%s` does not have a property named `%s`", g_type_name (this_type), property_name);
      return NULL;
    }

  return gtk_property_expression_new_for_pspec (expression, pspec);
}

/**
 * gtk_property_expression_new_for_pspec: (constructor)
 * @expression: (nullable) (transfer full): Expression to
 *   evaluate to get the object to query or `NULL` to
 *   query the `this` object
 * @pspec: the `GParamSpec` for the property to query
 *
 * Creates an expression that looks up a property.
 *
 * The object to use is found by evaluating the `expression`,
 * or using the `this` argument when `expression` is `NULL`.
 *
 * If the resulting object conforms to `this_type`, its
 * property specified by `pspec` will be queried.
 * Otherwise, this expression's evaluation will fail.
 *
 * Returns: (type GtkPropertyExpression) (transfer full): a new `GtkExpression`
 **/
GtkExpression *
gtk_property_expression_new_for_pspec (GtkExpression *expression,
                                       GParamSpec    *pspec)
{
  GtkExpression *result;
  GtkPropertyExpression *self;

  result = gtk_expression_alloc (GTK_TYPE_PROPERTY_EXPRESSION, pspec->value_type);
  self = (GtkPropertyExpression *) result;

  self->pspec = pspec;
  self->expr = expression;

  return result;
}

/**
 * gtk_property_expression_get_expression:
 * @expression: (type GtkPropertyExpression): a property `GtkExpression`
 *
 * Gets the expression specifying the object of
 * a property expression.
 *
 * Returns: (transfer none) (nullable): the object expression
 */
GtkExpression *
gtk_property_expression_get_expression (GtkExpression *expression)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expression;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_PROPERTY_EXPRESSION), NULL);

  return self->expr;
}

/**
 * gtk_property_expression_get_pspec:
 * @expression: (type GtkPropertyExpression): a property `GtkExpression`
 *
 * Gets the `GParamSpec` specifying the property of
 * a property expression.
 *
 * Returns: (transfer none): the `GParamSpec` for the property
 */
GParamSpec *
gtk_property_expression_get_pspec (GtkExpression *expression)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expression;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (expression, GTK_TYPE_PROPERTY_EXPRESSION), NULL);

  return self->pspec;
}

/* }}} */

/* {{{ GtkClosureExpression */

/**
 * GtkClosureExpression:
 *
 * An expression using a custom `GClosure` to compute the value from
 * its parameters.
 */
struct _GtkClosureExpression
{
  GtkExpression parent;

  GClosure *closure;
  guint n_params;
  GtkExpression **params;
};

static void
gtk_closure_expression_finalize (GtkExpression *expr)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guint i;

  for (i = 0; i < self->n_params; i++)
    {
      gtk_expression_unref (self->params[i]);
    }
  g_free (self->params);

  g_closure_unref (self->closure);

  GTK_EXPRESSION_SUPER (expr)->finalize (expr);
}

static gboolean
gtk_closure_expression_is_static (GtkExpression *expr)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guint i;

  for (i = 0; i < self->n_params; i++)
    {
      if (!gtk_expression_is_static (self->params[i]))
        return FALSE;
    }

  return TRUE;
}

static gboolean
gtk_closure_expression_evaluate (GtkExpression *expr,
                                 gpointer       this,
                                 GValue        *value)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  GValue *instance_and_params;
  gboolean result = TRUE;
  guint i;

  instance_and_params = g_alloca (sizeof (GValue) * (self->n_params + 1));
  memset (instance_and_params, 0, sizeof (GValue) * (self->n_params + 1));

  for (i = 0; i < self->n_params; i++)
    {
      if (!gtk_expression_evaluate (self->params[i], this, &instance_and_params[i + 1]))
        {
          result = FALSE;
          goto out;
        }
    }
  if (this)
    g_value_init_from_instance (instance_and_params, this);
  else
    g_value_init (instance_and_params, G_TYPE_OBJECT);

  g_value_init (value, expr->value_type);
  g_closure_invoke (self->closure,
                    value,
                    self->n_params + 1,
                    instance_and_params,
                    NULL);

out:
  for (i = 0; i < self->n_params + 1; i++)
    g_value_unset (&instance_and_params[i]);

  return result;
}

typedef struct _GtkClosureExpressionWatch GtkClosureExpressionWatch;
struct _GtkClosureExpressionWatch
{
  GtkExpressionNotify    notify;
  gpointer               user_data;

  guchar                 sub[0];
};

static void
gtk_closure_expression_watch_notify_cb (gpointer data)
{
  GtkClosureExpressionWatch *cwatch = data;

  cwatch->notify (cwatch->user_data);
}

static gsize
gtk_closure_expression_watch_size (GtkExpression *expr)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  gsize size;
  guint i;

  size = sizeof (GtkClosureExpressionWatch);

  for (i = 0; i < self->n_params; i++)
    {
      if (gtk_expression_is_static (self->params[i]))
        continue;

      size += gtk_expression_watch_size (self->params[i]);
    }

  return size;
}

static void
gtk_closure_expression_watch (GtkExpression         *expr,
                              GtkExpressionSubWatch *watch,
                              gpointer               this_,
                              GtkExpressionNotify    notify,
                              gpointer               user_data)
{
  GtkClosureExpressionWatch *cwatch = (GtkClosureExpressionWatch *) watch;
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guchar *sub;
  guint i;

  cwatch->notify = notify;
  cwatch->user_data = user_data;

  sub = cwatch->sub;
  for (i = 0; i < self->n_params; i++)
    {
      if (gtk_expression_is_static (self->params[i]))
        continue;

      gtk_expression_subwatch_init (self->params[i],
                                    (GtkExpressionSubWatch *) sub,
                                    this_,
                                    gtk_closure_expression_watch_notify_cb,
                                    watch);
      sub += gtk_expression_watch_size (self->params[i]);
    }
}

static void
gtk_closure_expression_unwatch (GtkExpression         *expr,
                                GtkExpressionSubWatch *watch)
{
  GtkClosureExpressionWatch *cwatch = (GtkClosureExpressionWatch *) watch;
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guchar *sub;
  guint i;

  sub = cwatch->sub;
  for (i = 0; i < self->n_params; i++)
    {
      if (gtk_expression_is_static (self->params[i]))
        continue;

      gtk_expression_subwatch_finish (self->params[i],
                                      (GtkExpressionSubWatch *) sub);
      sub += gtk_expression_watch_size (self->params[i]);
    }
}

static const GtkExpressionTypeInfo gtk_closure_expression_info =
{
  sizeof (GtkClosureExpression),
  NULL,
  gtk_closure_expression_finalize,
  gtk_closure_expression_is_static,
  gtk_closure_expression_evaluate,
  gtk_closure_expression_watch_size,
  gtk_closure_expression_watch,
  gtk_closure_expression_unwatch
};

GTK_DEFINE_EXPRESSION_TYPE (GtkClosureExpression,
                            gtk_closure_expression,
                            &gtk_closure_expression_info)

/**
 * gtk_closure_expression_new: (constructor)
 * @value_type: the type of the value that this expression evaluates to
 * @closure: closure to call when evaluating this expression. If closure is floating, it is adopted
 * @n_params: the number of params needed for evaluating `closure`
 * @params: (nullable) (array length=n_params) (transfer full): expressions for each parameter
 *
 * Creates a `GtkExpression` that calls `closure` when it is evaluated.
 *
 * `closure` is called with the `this` object and the results of evaluating
 * the `params` expressions.
 *
 * Returns: (transfer full) (type GtkClosureExpression): a new `GtkExpression`
 */
GtkExpression *
gtk_closure_expression_new (GType                value_type,
                            GClosure            *closure,
                            guint                n_params,
                            GtkExpression      **params)
{
  GtkExpression *result;
  GtkClosureExpression *self;
  guint i;

  g_return_val_if_fail (closure != NULL, NULL);
  g_return_val_if_fail (n_params == 0 || params != NULL, NULL);

  result = gtk_expression_alloc (GTK_TYPE_CLOSURE_EXPRESSION, value_type);
  self = (GtkClosureExpression *) result;

  self->closure = g_closure_ref (closure);
  g_closure_sink (closure);
  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    g_closure_set_marshal (closure, g_cclosure_marshal_generic);

  self->n_params = n_params;
  self->params = g_new (GtkExpression *, n_params);
  for (i = 0; i < n_params; i++)
    self->params[i] = params[i];

  return result;
}

/* }}} */

/* {{{ GtkCClosureExpression */

/**
 * GtkCClosureExpression:
 *
 * A variant of `GtkClosureExpression` using a C closure.
 */
struct _GtkCClosureExpression
{
  GtkClosureExpression parent;
};

static const GtkExpressionTypeInfo gtk_cclosure_expression_info =
{
  sizeof (GtkClosureExpression),
  NULL,
  gtk_closure_expression_finalize,
  gtk_closure_expression_is_static,
  gtk_closure_expression_evaluate,
  gtk_closure_expression_watch_size,
  gtk_closure_expression_watch,
  gtk_closure_expression_unwatch
};

GTK_DEFINE_EXPRESSION_TYPE (GtkCClosureExpression,
                            gtk_cclosure_expression,
                            &gtk_cclosure_expression_info)

/**
 * gtk_cclosure_expression_new: (constructor)
 * @value_type: the type of the value that this expression evaluates to
 * @marshal: (scope call) (nullable): marshaller used for creating a closure
 * @n_params: the number of params needed for evaluating @closure
 * @params: (array length=n_params) (transfer full): expressions for each parameter
 * @callback_func: (scope notified) (closure user_data) (destroy user_destroy): callback used for creating a closure
 * @user_data: (nullable): user data used for creating a closure
 * @user_destroy: (nullable): destroy notify for @user_data
 *
 * Creates a `GtkExpression` that calls `callback_func` when it is evaluated.
 *
 * This function is a variant of [ctor@Gtk.ClosureExpression.new] that
 * creates a `GClosure` by calling g_cclosure_new() with the given
 * `callback_func`, `user_data` and `user_destroy`.
 *
 * Returns: (transfer full) (type GtkCClosureExpression): a new `GtkExpression`
 */
GtkExpression *
gtk_cclosure_expression_new (GType                value_type,
                             GClosureMarshal      marshal,
                             guint                n_params,
                             GtkExpression      **params,
                             GCallback            callback_func,
                             gpointer             user_data,
                             GClosureNotify       user_destroy)
{
  GtkExpression *result;
  GtkClosureExpression *self;
  GClosure *closure;
  guint i;

  g_return_val_if_fail (callback_func != NULL, NULL);
  g_return_val_if_fail (n_params == 0 || params != NULL, NULL);

  result = gtk_expression_alloc (GTK_TYPE_CCLOSURE_EXPRESSION, value_type);
  self = (GtkClosureExpression *) result;

  closure = g_cclosure_new (callback_func, user_data, user_destroy);
  if (marshal)
    g_closure_set_marshal (closure, marshal);

  self->closure = g_closure_ref (closure);
  g_closure_sink (closure);
  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    g_closure_set_marshal (closure, g_cclosure_marshal_generic);

  self->n_params = n_params;
  self->params = g_new (GtkExpression *, n_params);
  for (i = 0; i < n_params; i++)
    self->params[i] = params[i];

  return result;
}

/* }}} */

/* {{{ GtkExpression public API */

/**
 * gtk_expression_ref:
 * @self: a `GtkExpression`
 *
 * Acquires a reference on the given `GtkExpression`.
 *
 * Returns: (transfer full): the `GtkExpression` with an additional reference
 */
GtkExpression *
gtk_expression_ref (GtkExpression *self)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

/**
 * gtk_expression_unref:
 * @self: (transfer full): a `GtkExpression`
 *
 * Releases a reference on the given `GtkExpression`.
 *
 * If the reference was the last, the resources associated to the `self` are
 * freed.
 */
void
gtk_expression_unref (GtkExpression *self)
{
  g_return_if_fail (GTK_IS_EXPRESSION (self));

  if (g_atomic_ref_count_dec (&self->ref_count))
    GTK_EXPRESSION_GET_CLASS (self)->finalize (self);
}

/**
 * gtk_expression_get_value_type:
 * @self: a `GtkExpression`
 *
 * Gets the `GType` that this expression evaluates to.
 *
 * This type is constant and will not change over the lifetime
 * of this expression.
 *
 * Returns: The type returned from [method@Gtk.Expression.evaluate]
 */
GType
gtk_expression_get_value_type (GtkExpression *self)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), G_TYPE_INVALID);

  return self->value_type;
}

/**
 * gtk_expression_evaluate:
 * @self: a `GtkExpression`
 * @this_: (transfer none) (type GObject) (nullable): the this argument for the evaluation
 * @value: an empty `GValue`
 *
 * Evaluates the given expression and on success stores the result
 * in @value.
 *
 * The `GType` of `value` will be the type given by
 * [method@Gtk.Expression.get_value_type].
 *
 * It is possible that expressions cannot be evaluated - for example
 * when the expression references objects that have been destroyed or
 * set to `NULL`. In that case `value` will remain empty and `FALSE`
 * will be returned.
 *
 * Returns: `TRUE` if the expression could be evaluated
 **/
gboolean
gtk_expression_evaluate (GtkExpression *self,
                         gpointer       this_,
                         GValue        *value)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), FALSE);
  g_return_val_if_fail (this_ == NULL || G_IS_OBJECT (this_), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return GTK_EXPRESSION_GET_CLASS (self)->evaluate (self, this_, value);
}

/**
 * gtk_expression_is_static:
 * @self: a `GtkExpression`
 *
 * Checks if the expression is static.
 *
 * A static expression will never change its result when
 * [method@Gtk.Expression.evaluate] is called on it with the same arguments.
 *
 * That means a call to [method@Gtk.Expression.watch] is not necessary because
 * it will never trigger a notify.
 *
 * Returns: `TRUE` if the expression is static
 **/
gboolean
gtk_expression_is_static (GtkExpression *self)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), FALSE);

  return GTK_EXPRESSION_GET_CLASS (self)->is_static (self);
}

static gboolean
gtk_expression_watch_is_watching (GtkExpressionWatch *watch)
{
  return watch->expression != NULL;
}

static void
gtk_expression_watch_this_cb (gpointer data,
                              GObject *this)
{
  WeakRefGuard *guard = data;
  GtkExpressionWatch *watch = guard->data;

  if (watch != NULL)
    {
      g_weak_ref_set (&watch->this_wr, NULL);

      watch->notify (watch->user_data);

      gtk_expression_watch_unwatch (watch);
    }

  weak_ref_guard_unref (guard);
}

static void
gtk_expression_watch_cb (gpointer data)
{
  GtkExpressionWatch *watch = data;

  if (!gtk_expression_watch_is_watching (watch))
    return;

  watch->notify (watch->user_data);
}

/**
 * gtk_expression_watch:
 * @self: a `GtkExpression`
 * @this_: (transfer none) (type GObject) (nullable): the `this` argument to
 *   watch
 * @notify: (closure user_data): callback to invoke when the expression changes
 * @user_data: user data to pass to the `notify` callback
 * @user_destroy: destroy notify for `user_data`
 *
 * Watch the given `expression` for changes.
 *
 * The @notify function will be called whenever the evaluation of `self`
 * may have changed.
 *
 * GTK cannot guarantee that the evaluation did indeed change when the @notify
 * gets invoked, but it guarantees the opposite: When it did in fact change,
 * the @notify will be invoked.
 *
 * Returns: (transfer none): The newly installed watch. Note that the only
 *   reference held to the watch will be released when the watch is unwatched
 *   which can happen automatically, and not just via
 *   [method@Gtk.ExpressionWatch.unwatch]. You should call [method@Gtk.ExpressionWatch.ref]
 *   if you want to keep the watch around.
 **/
GtkExpressionWatch *
gtk_expression_watch (GtkExpression       *self,
                      gpointer             this_,
                      GtkExpressionNotify  notify,
                      gpointer             user_data,
                      GDestroyNotify       user_destroy)
{
  GtkExpressionWatch *watch;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (this_ == NULL || G_IS_OBJECT (this_), NULL);
  g_return_val_if_fail (notify != NULL, NULL);

  watch = g_atomic_rc_box_alloc0 (sizeof (GtkExpressionWatch) + gtk_expression_watch_size (self));

  watch->expression = gtk_expression_ref (self);
  watch->guard = weak_ref_guard_new (watch);
  g_weak_ref_init (&watch->this_wr, this_);
  if (this_)
    g_object_weak_ref (this_,
                       gtk_expression_watch_this_cb,
                       weak_ref_guard_ref (watch->guard));
  watch->notify = notify;
  watch->user_data = user_data;
  watch->user_destroy = user_destroy;

  gtk_expression_subwatch_init (self,
                                (GtkExpressionSubWatch *) watch->sub,
                                this_,
                                gtk_expression_watch_cb,
                                watch);

  return watch;
}

/**
 * gtk_expression_watch_ref:
 * @watch: a `GtkExpressionWatch`
 *
 * Acquires a reference on the given `GtkExpressionWatch`.
 *
 * Returns: (transfer full): the `GtkExpressionWatch` with an additional reference
 */
GtkExpressionWatch *
gtk_expression_watch_ref (GtkExpressionWatch *watch)
{
  return g_atomic_rc_box_acquire (watch);
}

static void
gtk_expression_watch_finalize (gpointer data)
{
  GtkExpressionWatch *watch G_GNUC_UNUSED = data;

  g_assert (!gtk_expression_watch_is_watching (data));

  weak_ref_guard_unref (watch->guard);

  g_weak_ref_clear (&watch->this_wr);
}

/**
 * gtk_expression_watch_unref:
 * @watch: (transfer full): a `GtkExpressionWatch`
 *
 * Releases a reference on the given `GtkExpressionWatch`.
 *
 * If the reference was the last, the resources associated to `self` are
 * freed.
 */
void
gtk_expression_watch_unref (GtkExpressionWatch *watch)
{
  g_atomic_rc_box_release_full (watch, gtk_expression_watch_finalize);
}

/**
 * gtk_expression_watch_unwatch:
 * @watch: (transfer none): watch to release
 *
 * Stops watching an expression.
 *
 * See [method@Gtk.Expression.watch] for how the watch
 * was established.
 */
void
gtk_expression_watch_unwatch (GtkExpressionWatch *watch)
{
  GObject *this;

  if (!gtk_expression_watch_is_watching (watch))
    return;

  gtk_expression_subwatch_finish (watch->expression, (GtkExpressionSubWatch *) watch->sub);

  this = g_weak_ref_get (&watch->this_wr);

  if (this)
    {
      g_object_weak_unref (this, gtk_expression_watch_this_cb, watch->guard);
      weak_ref_guard_unref (watch->guard);
      g_weak_ref_set (&watch->this_wr, NULL);
    }

  if (watch->user_destroy)
    watch->user_destroy (watch->user_data);

  g_clear_object (&this);

  g_clear_pointer (&watch->expression, gtk_expression_unref);

  gtk_expression_watch_unref (watch);
}

/**
 * gtk_expression_watch_evaluate:
 * @watch: a `GtkExpressionWatch`
 * @value: an empty `GValue` to be set
 *
 * Evaluates the watched expression and on success stores the result
 * in `value`.
 *
 * This is equivalent to calling [method@Gtk.Expression.evaluate] with the
 * expression and this pointer originally used to create `watch`.
 *
 * Returns: `TRUE` if the expression could be evaluated and `value` was set
 **/
gboolean
gtk_expression_watch_evaluate (GtkExpressionWatch *watch,
                               GValue             *value)
{
  GObject *this;
  gboolean ret;

  g_return_val_if_fail (watch != NULL, FALSE);

  if (!gtk_expression_watch_is_watching (watch))
    return FALSE;

  this = g_weak_ref_get (&watch->this_wr);
  ret = gtk_expression_evaluate (watch->expression, this, value);
  g_clear_object (&this);

  return ret;
}

typedef struct {
  GtkExpressionWatch *watch;
  GWeakRef target_wr;
  GParamSpec *pspec;
} GtkExpressionBind;

static void
invalidate_binds (gpointer unused,
                  GObject *object)
{
  GSList *l, *binds;

  l = binds = g_object_get_data (object, "gtk-expression-binds");
  while (l)
    {
      GtkExpressionBind *bind = l->data;

      l = l->next;

      /* This guarantees we neither try to update bindings
       * (which would wreck havoc because the object is
       * dispose()ing itself) nor try to destroy bindings
       * anymore, so destruction can be done in free_binds().
       */
      g_weak_ref_set (&bind->target_wr, NULL);
    }
}

static void
free_binds (gpointer data)
{
  GSList *l = data;

  while (l)
    {
      GtkExpressionBind *bind = l->data;

      l = l->next;

      if (bind->watch)
        gtk_expression_watch_unwatch (bind->watch);
      g_weak_ref_clear (&bind->target_wr);
      g_free (bind);
    }

  g_slist_free (data);
}

static void
gtk_expression_bind_free (gpointer data)
{
  GtkExpressionBind *bind = data;
  GObject *target = g_weak_ref_get (&bind->target_wr);

  g_weak_ref_set (&bind->target_wr, NULL);

  if (target)
    {
      GSList *binds;
      binds = g_object_steal_data (target, "gtk-expression-binds");
      binds = g_slist_remove (binds, bind);
      if (binds)
        g_object_set_data_full (target, "gtk-expression-binds", binds, free_binds);
      else
        g_object_weak_unref (target, invalidate_binds, NULL);

      g_object_unref (target);
      g_free (bind);
    }
  else
    {
      /* If a bind gets unwatched after invalidate_binds() but
       * before free_binds(), we end up here. This can happen if
       * the bind was watching itself or if the target's dispose()
       * function freed the object that was watched.
       * We make sure we don't destroy the binding or free_binds() will do
       * bad stuff, but we clear the watch, so free_binds() won't try to
       * unwatch() it.
       */
      bind->watch = NULL;
    }
}

static void
gtk_expression_bind_notify (gpointer data)
{
  GValue value = G_VALUE_INIT;
  GtkExpressionBind *bind = data;
  GObject *target = g_weak_ref_get (&bind->target_wr);

  if (target == NULL)
    return;

  if (!gtk_expression_watch_evaluate (bind->watch, &value))
    {
      g_object_unref (target);
      return;
    }

  g_object_set_property (target, bind->pspec->name, &value);
  g_object_unref (target);
  g_value_unset (&value);
}

/**
 * gtk_expression_bind:
 * @self: (transfer full): a `GtkExpression`
 * @target: (transfer none) (type GObject): the target object to bind to
 * @property: name of the property on `target` to bind to
 * @this_: (transfer none) (type GObject) (nullable): the this argument for
 *   the evaluation of `self`
 *
 * Bind `target`'s property named `property` to `self`.
 *
 * The value that `self` evaluates to is set via `g_object_set()` on
 * `target`. This is repeated whenever `self` changes to ensure that
 * the object's property stays synchronized with `self`.
 *
 * If `self`'s evaluation fails, `target`'s `property` is not updated.
 * You can ensure that this doesn't happen by using a fallback
 * expression.
 *
 * Note that this function takes ownership of `self`. If you want
 * to keep it around, you should [method@Gtk.Expression.ref] it beforehand.
 *
 * Returns: (transfer none): a `GtkExpressionWatch`
 **/
GtkExpressionWatch *
gtk_expression_bind (GtkExpression *self,
                     gpointer       target,
                     const char    *property,
                     gpointer       this_)
{
  GtkExpressionBind *bind;
  GParamSpec *pspec;
  GSList *binds;

  g_return_val_if_fail (GTK_IS_EXPRESSION (self), NULL);
  g_return_val_if_fail (G_IS_OBJECT (target), NULL);
  g_return_val_if_fail (property != NULL, NULL);
  g_return_val_if_fail (this_ == NULL || G_IS_OBJECT (this_), NULL);

  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (target), property);
  if (G_UNLIKELY (pspec == NULL))
    {
      g_critical ("%s: Class '%s' has no property named '%s'",
                  G_STRFUNC, G_OBJECT_TYPE_NAME (target), property);
      return NULL;
    }
  if (G_UNLIKELY ((pspec->flags & (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)) != G_PARAM_WRITABLE))
    {
      g_critical ("%s: property '%s' of class '%s' is not writable",
                 G_STRFUNC, pspec->name, G_OBJECT_TYPE_NAME (target));
      return NULL;
    }

  bind = g_new0 (GtkExpressionBind, 1);
  binds = g_object_steal_data (target, "gtk-expression-binds");
  if (binds == NULL)
    g_object_weak_ref (target, invalidate_binds, NULL);
  g_weak_ref_init (&bind->target_wr, target);
  bind->pspec = pspec;
  bind->watch = gtk_expression_watch (self,
                                      this_,
                                      gtk_expression_bind_notify,
                                      bind,
                                      gtk_expression_bind_free);
  binds = g_slist_prepend (binds, bind);
  g_object_set_data_full (target, "gtk-expression-binds", binds, free_binds);

  gtk_expression_unref (self);

  gtk_expression_bind_notify (bind);

  return bind->watch;
}

/* }}} */
