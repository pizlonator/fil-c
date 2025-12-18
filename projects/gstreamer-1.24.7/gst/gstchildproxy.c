/* GStreamer
 * Copyright (C) 2005 Stefan Kost <ensonic@users.sf.net>
 *
 * gstchildproxy.c: interface for multi child elements
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstchildproxy
 * @title: GstChildProxy
 * @short_description: Interface for multi child elements.
 * @see_also: #GstBin
 *
 * This interface abstracts handling of property sets for elements with
 * children. Imagine elements such as mixers or polyphonic generators. They all
 * have multiple #GstPad or some kind of voice objects. Another use case are
 * container elements like #GstBin.
 * The element implementing the interface acts as a parent for those child
 * objects.
 *
 * By implementing this interface the child properties can be accessed from the
 * parent element by using gst_child_proxy_get() and gst_child_proxy_set().
 *
 * Property names are written as `child-name::property-name`. The whole naming
 * scheme is recursive. Thus `child1::child2::property` is valid too, if
 * `child1` and `child2` implement the #GstChildProxy interface.
 */

#include "gst_private.h"

#include "gstchildproxy.h"
#include <gobject/gvaluecollector.h>

/* signals */
enum
{
  CHILD_ADDED,
  CHILD_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/**
 * gst_child_proxy_get_child_by_name_recurse:
 * @child_proxy: the parent object to get the child from
 * @name: the full-path child's name
 *
 * Looks up a child element by the given full-path name.
 *
 * Similar to gst_child_proxy_get_child_by_name(), this method
 * searches and returns a child given a name. The difference is that
 * this method allows a hierarchical path in the form of
 * child1::child2::child3. In the later example this method would
 * return a reference to child3, if found. The name should be made of
 * element names only and should not contain any property names.
 *
 * Returns: (transfer full) (nullable): the child object or %NULL if
 *     not found.
 *
 * Since: 1.22
 */
GObject *
gst_child_proxy_get_child_by_name_recurse (GstChildProxy * child_proxy,
    const gchar * name)
{
  gchar **names = NULL, **current = NULL;
  GObject *obj = NULL, *next = NULL;

  g_return_val_if_fail (child_proxy != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  current = names = g_strsplit (name, "::", -1);
  if (current[0]) {
    obj = G_OBJECT (g_object_ref (child_proxy));
  }

  /* walk through the children hierarchy until the requested one is found */
  while (current[0]) {

    /* Cannot ask for the child of a non-childproxy */
    if (!GST_IS_CHILD_PROXY (obj)) {
      gst_object_unref (obj);
      next = NULL;
      break;
    }

    next = gst_child_proxy_get_child_by_name (GST_CHILD_PROXY (obj),
        current[0]);
    gst_object_unref (obj);

    /* The child does not exist */
    if (!next) {
      GST_INFO ("Unable to find child %s", current[0]);
      break;
    }

    obj = next;
    current++;
  }

  g_strfreev (names);

  return next;
}

static GObject *
gst_child_proxy_default_get_child_by_name (GstChildProxy * parent,
    const gchar * name)
{
  guint count, i;
  GObject *object, *result;
  gchar *object_name;

  g_return_val_if_fail (GST_IS_CHILD_PROXY (parent), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  result = NULL;

  count = gst_child_proxy_get_children_count (parent);
  for (i = 0; i < count; i++) {
    gboolean eq;

    if (!(object = gst_child_proxy_get_child_by_index (parent, i)))
      continue;

    if (!GST_IS_OBJECT (object)) {
      goto next;
    }
    object_name = gst_object_get_name (GST_OBJECT_CAST (object));
    if (object_name == NULL) {
      g_warning ("child %u of parent %s has no name", i,
          GST_OBJECT_NAME (parent));
      goto next;
    }
    eq = g_str_equal (object_name, name);
    g_free (object_name);

    if (eq) {
      result = object;
      break;
    }
  next:
    gst_object_unref (object);
  }
  return result;
}


/**
 * gst_child_proxy_get_child_by_name:
 * @parent: the parent object to get the child from
 * @name: the child's name
 *
 * Looks up a child element by the given name.
 *
 * This virtual method has a default implementation that uses #GstObject
 * together with gst_object_get_name(). If the interface is to be used with
 * #GObjects, this methods needs to be overridden.
 *
 * Returns: (transfer full) (nullable): the child object or %NULL if
 *     not found.
 */
GObject *
gst_child_proxy_get_child_by_name (GstChildProxy * parent, const gchar * name)
{
  GstChildProxyInterface *iface;

  g_return_val_if_fail (GST_IS_CHILD_PROXY (parent), 0);

  iface = GST_CHILD_PROXY_GET_INTERFACE (parent);

  if (iface->get_child_by_name != NULL)
    return iface->get_child_by_name (parent, name);

  return NULL;
}

/**
 * gst_child_proxy_get_child_by_index:
 * @parent: the parent object to get the child from
 * @index: the child's position in the child list
 *
 * Fetches a child by its number.
 *
 * Returns: (transfer full) (nullable): the child object or %NULL if
 *     not found (index too high).
 */
GObject *
gst_child_proxy_get_child_by_index (GstChildProxy * parent, guint index)
{
  GstChildProxyInterface *iface;

  g_return_val_if_fail (GST_IS_CHILD_PROXY (parent), NULL);

  iface = GST_CHILD_PROXY_GET_INTERFACE (parent);

  if (iface->get_child_by_index != NULL)
    return iface->get_child_by_index (parent, index);

  return NULL;
}

/**
 * gst_child_proxy_get_children_count:
 * @parent: the parent object
 *
 * Gets the number of child objects this parent contains.
 *
 * Returns: the number of child objects
 */
guint
gst_child_proxy_get_children_count (GstChildProxy * parent)
{
  GstChildProxyInterface *iface;

  g_return_val_if_fail (GST_IS_CHILD_PROXY (parent), 0);

  iface = GST_CHILD_PROXY_GET_INTERFACE (parent);

  if (iface->get_children_count != NULL)
    return iface->get_children_count (parent);

  return 0;
}

/**
 * gst_child_proxy_lookup:
 * @object: child proxy object to lookup the property in
 * @name: name of the property to look up
 * @target: (out) (allow-none) (transfer full): pointer to a #GObject that
 *     takes the real object to set property on
 * @pspec: (out) (allow-none) (transfer none): pointer to take the #GParamSpec
 *     describing the property
 *
 * Looks up which object and #GParamSpec would be effected by the given @name.
 *
 * Returns: %TRUE if @target and @pspec could be found. %FALSE otherwise. In that
 * case the values for @pspec and @target are not modified. Unref @target after
 * usage. For plain #GObject @target is the same as @object.
 */
gboolean
gst_child_proxy_lookup (GstChildProxy * object, const gchar * name,
    GObject ** target, GParamSpec ** pspec)
{
  gboolean res = FALSE;
  gchar *children = NULL;
  const gchar *prop = NULL;
  GObject *child = NULL;
  static const gchar *separator = "::";

  g_return_val_if_fail (GST_IS_CHILD_PROXY (object), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);

  /* If the requested name does not include a "::" then it is not a
     child proxy path, but the property name directly */
  prop = g_strrstr (name, separator);
  if (!prop) {
    child = gst_object_ref (G_OBJECT (object));
    prop = name;
  } else {
    /* Skip "::" */
    prop += 2;

    /* Extract "child1::child2" from "child1::child2::prop" */
    children = g_strndup (name, prop - name - 2);
    GST_INFO ("Looking for property %s in %s", prop, children);

    child = gst_child_proxy_get_child_by_name_recurse (object, children);
    g_free (children);

    if (!child) {
      GST_INFO ("Child not found");
      goto out;
    }
  }

  GParamSpec *spec =
      g_object_class_find_property (G_OBJECT_GET_CLASS (child), prop);
  if (spec == NULL) {
    GST_INFO ("no param spec named %s", prop);
  } else {
    if (pspec)
      *pspec = spec;
    if (target) {
      g_object_ref (child);
      *target = child;
    }
    res = TRUE;
  }
  gst_object_unref (child);

out:
  return res;
}

/**
 * gst_child_proxy_get_property:
 * @object: object to query
 * @name: name of the property
 * @value: (out caller-allocates): a #GValue that should take the result.
 *
 * Gets a single property using the GstChildProxy mechanism.
 * You are responsible for freeing it by calling g_value_unset()
 */
void
gst_child_proxy_get_property (GstChildProxy * object, const gchar * name,
    GValue * value)
{
  GParamSpec *pspec;
  GObject *target;

  g_return_if_fail (GST_IS_CHILD_PROXY (object));
  g_return_if_fail (name != NULL);
  g_return_if_fail (value != NULL);

  if (!gst_child_proxy_lookup (object, name, &target, &pspec))
    goto not_found;

  if (!G_IS_VALUE (value)) {
    g_value_init (value, pspec->value_type);
  }

  g_object_get_property (target, pspec->name, value);
  gst_object_unref (target);

  return;

not_found:
  {
    g_warning ("no property %s in object %s", name,
        (GST_IS_OBJECT (object) ? GST_OBJECT_NAME (object) : ""));
    return;
  }
}

/**
 * gst_child_proxy_get_valist:
 * @object: the object to query
 * @first_property_name: name of the first property to get
 * @var_args: return location for the first property, followed optionally by more name/return location pairs, followed by %NULL
 *
 * Gets properties of the parent object and its children.
 */
void
gst_child_proxy_get_valist (GstChildProxy * object,
    const gchar * first_property_name, va_list var_args)
{
  const gchar *name;
  gchar *error = NULL;
  GValue value = { 0, };
  GParamSpec *pspec;
  GObject *target;

  g_return_if_fail (GST_IS_CHILD_PROXY (object));

  name = first_property_name;

  /* iterate over pairs */
  while (name) {
    if (!gst_child_proxy_lookup (object, name, &target, &pspec))
      goto not_found;

    g_value_init (&value, pspec->value_type);
    g_object_get_property (target, pspec->name, &value);
    gst_object_unref (target);

    G_VALUE_LCOPY (&value, var_args, 0, &error);
    if (error)
      goto cant_copy;
    g_value_unset (&value);
    name = va_arg (var_args, gchar *);
  }
  return;

not_found:
  {
    g_warning ("no property %s in object %s", name,
        (GST_IS_OBJECT (object) ? GST_OBJECT_NAME (object) : ""));
    return;
  }
cant_copy:
  {
    g_warning ("error copying value %s in object %s: %s", pspec->name,
        (GST_IS_OBJECT (object) ? GST_OBJECT_NAME (object) : ""), error);
    g_value_unset (&value);
    return;
  }
}

/**
 * gst_child_proxy_get:
 * @object: the parent object
 * @first_property_name: name of the first property to get
 * @...: return location for the first property, followed optionally by more name/return location pairs, followed by %NULL
 *
 * Gets properties of the parent object and its children.
 */
void
gst_child_proxy_get (GstChildProxy * object, const gchar * first_property_name,
    ...)
{
  va_list var_args;

  g_return_if_fail (GST_IS_CHILD_PROXY (object));

  va_start (var_args, first_property_name);
  gst_child_proxy_get_valist (object, first_property_name, var_args);
  va_end (var_args);
}

/**
 * gst_child_proxy_set_property:
 * @object: the parent object
 * @name: name of the property to set
 * @value: new #GValue for the property
 *
 * Sets a single property using the GstChildProxy mechanism.
 */
void
gst_child_proxy_set_property (GstChildProxy * object, const gchar * name,
    const GValue * value)
{
  GParamSpec *pspec;
  GObject *target;

  g_return_if_fail (GST_IS_CHILD_PROXY (object));
  g_return_if_fail (name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  if (!gst_child_proxy_lookup (object, name, &target, &pspec))
    goto not_found;

  g_object_set_property (target, pspec->name, value);
  gst_object_unref (target);
  return;

not_found:
  {
    g_warning ("cannot set property %s on object %s", name,
        (GST_IS_OBJECT (object) ? GST_OBJECT_NAME (object) : ""));
    return;
  }
}

/**
 * gst_child_proxy_set_valist:
 * @object: the parent object
 * @first_property_name: name of the first property to set
 * @var_args: value for the first property, followed optionally by more name/value pairs, followed by %NULL
 *
 * Sets properties of the parent object and its children.
 */
void
gst_child_proxy_set_valist (GstChildProxy * object,
    const gchar * first_property_name, va_list var_args)
{
  const gchar *name;
  gchar *error = NULL;
  GValue value = { 0, };
  GParamSpec *pspec;
  GObject *target;

  g_return_if_fail (GST_IS_CHILD_PROXY (object));

  name = first_property_name;

  /* iterate over pairs */
  while (name) {
    if (!gst_child_proxy_lookup (object, name, &target, &pspec))
      goto not_found;

    G_VALUE_COLLECT_INIT (&value, pspec->value_type, var_args,
        G_VALUE_NOCOPY_CONTENTS, &error);

    if (error)
      goto cant_copy;

    g_object_set_property (target, pspec->name, &value);
    gst_object_unref (target);

    g_value_unset (&value);
    name = va_arg (var_args, gchar *);
  }
  return;

not_found:
  {
    g_warning ("no property %s in object %s", name,
        (GST_IS_OBJECT (object) ? GST_OBJECT_NAME (object) : ""));
    return;
  }
cant_copy:
  {
    g_warning ("error copying value %s in object %s: %s", pspec->name,
        (GST_IS_OBJECT (object) ? GST_OBJECT_NAME (object) : ""), error);
    g_value_unset (&value);
    gst_object_unref (target);
    g_free (error);
    return;
  }
}

/**
 * gst_child_proxy_set:
 * @object: the parent object
 * @first_property_name: name of the first property to set
 * @...: value for the first property, followed optionally by more name/value pairs, followed by %NULL
 *
 * Sets properties of the parent object and its children.
 */
void
gst_child_proxy_set (GstChildProxy * object, const gchar * first_property_name,
    ...)
{
  va_list var_args;

  g_return_if_fail (GST_IS_CHILD_PROXY (object));

  va_start (var_args, first_property_name);
  gst_child_proxy_set_valist (object, first_property_name, var_args);
  va_end (var_args);
}

/**
 * gst_child_proxy_child_added:
 * @parent: the parent object
 * @child: the newly added child
 * @name: the name of the new child
 *
 * Emits the #GstChildProxy::child-added signal.
 */
void
gst_child_proxy_child_added (GstChildProxy * parent, GObject * child,
    const gchar * name)
{
  g_signal_emit (parent, signals[CHILD_ADDED], 0, child, name);
}

/**
 * gst_child_proxy_child_removed:
 * @parent: the parent object
 * @child: the removed child
 * @name: the name of the old child
 *
 * Emits the #GstChildProxy::child-removed signal.
 */
void
gst_child_proxy_child_removed (GstChildProxy * parent, GObject * child,
    const gchar * name)
{
  g_signal_emit (parent, signals[CHILD_REMOVED], 0, child, name);
}

/* gobject methods */

static void
gst_child_proxy_class_init (gpointer g_class, gpointer class_data)
{
  GstChildProxyInterface *iface = (GstChildProxyInterface *) g_class;

  iface->get_child_by_name = gst_child_proxy_default_get_child_by_name;
}

static void
gst_child_proxy_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (!initialized) {
    /* create interface signals and properties here. */
    /**
     * GstChildProxy::child-added:
     * @self: the #GstChildProxy
     * @object: the #GObject that was added
     * @name: the name of the new child
     *
     * Will be emitted after the @object was added to the @child_proxy.
     */
    signals[CHILD_ADDED] =
        g_signal_new ("child-added", G_TYPE_FROM_CLASS (g_class),
        G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GstChildProxyInterface,
            child_added), NULL, NULL, NULL, G_TYPE_NONE,
        2, G_TYPE_OBJECT, G_TYPE_STRING);

    /**
     * GstChildProxy::child-removed:
     * @self: the #GstChildProxy
     * @object: the #GObject that was removed
     * @name: the name of the old child
     *
     * Will be emitted after the @object was removed from the @child_proxy.
     */
    signals[CHILD_REMOVED] =
        g_signal_new ("child-removed", G_TYPE_FROM_CLASS (g_class),
        G_SIGNAL_RUN_FIRST, G_STRUCT_OFFSET (GstChildProxyInterface,
            child_removed), NULL, NULL, NULL, G_TYPE_NONE,
        2, G_TYPE_OBJECT, G_TYPE_STRING);

    initialized = TRUE;
  }
}

GType
gst_child_proxy_get_type (void)
{
  static gpointer type = 0;

  if (g_once_init_enter_pointer (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstChildProxyInterface),
      gst_child_proxy_base_init,        /* base_init */
      NULL,                     /* base_finalize */
      gst_child_proxy_class_init,       /* class_init */
      NULL,                     /* class_finalize */
      NULL,                     /* class_data */
      0,
      0,                        /* n_preallocs */
      NULL                      /* instance_init */
    };

    _type =
        g_type_register_static (G_TYPE_INTERFACE, "GstChildProxy", &info, 0);

    g_type_interface_add_prerequisite (_type, G_TYPE_OBJECT);
    g_once_init_leave_pointer (&type, _type);
  }
  return type;
}
