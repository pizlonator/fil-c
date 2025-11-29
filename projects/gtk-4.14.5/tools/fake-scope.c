/*  Copyright 2015 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "fake-scope.h"
#include "gtk-builder-tool.h"


#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>

/* {{{ Scope implementation */

struct _FakeScope
{
  GtkBuilderCScope parent;

  GPtrArray *types;
  GPtrArray *callbacks;
};

static GtkBuilderScopeInterface *parent_scope_iface;

static void
dummy_cb (void)
{
}

static GClosure *
fake_scope_create_closure (GtkBuilderScope         *scope,
                           GtkBuilder              *builder,
                           const char              *function_name,
                           GtkBuilderClosureFlags   flags,
                           GObject                 *object,
                           GError                 **error)
{
  FakeScope *self = FAKE_SCOPE (scope);
  GClosure *closure;
  gboolean swapped = flags & GTK_BUILDER_CLOSURE_SWAPPED;

  g_ptr_array_add (self->callbacks, g_strdup (function_name));

  if (object == NULL)
    object = gtk_builder_get_current_object (builder);

  if (object)
    {
      if (swapped)
        closure = g_cclosure_new_object_swap (dummy_cb, object);
      else
        closure = g_cclosure_new_object (dummy_cb, object);
    }
  else
    {
      if (swapped)
        closure = g_cclosure_new_swap (dummy_cb, NULL, NULL);
      else
        closure = g_cclosure_new (dummy_cb, NULL, NULL);
    }

  return closure;
}

static GType
fake_scope_get_type_from_name (GtkBuilderScope *scope,
                               GtkBuilder      *builder,
                               const char      *type_name)
{
  FakeScope *self = FAKE_SCOPE (scope);
  GType type;

  type = parent_scope_iface->get_type_from_name (scope, builder, type_name);

  g_ptr_array_add (self->types, g_strdup (type_name));

  return type;
}

static GType
fake_scope_get_type_from_function (GtkBuilderScope *scope,
                                   GtkBuilder      *builder,
                                   const char      *function_name)
{
  FakeScope *self = FAKE_SCOPE (scope);
  GType type;

  type = parent_scope_iface->get_type_from_function (scope, builder, function_name);

  if (type != G_TYPE_INVALID)
    g_ptr_array_add (self->types, g_strdup (g_type_name (type)));

  return type;
}

static void
fake_scope_scope_init (GtkBuilderScopeInterface *iface)
{
  parent_scope_iface = g_type_interface_peek_parent (iface);

  iface->get_type_from_name = fake_scope_get_type_from_name;
  iface->get_type_from_function = fake_scope_get_type_from_function;
  iface->create_closure = fake_scope_create_closure;
}

G_DEFINE_TYPE_WITH_CODE (FakeScope, fake_scope, GTK_TYPE_BUILDER_CSCOPE,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDER_SCOPE,
                                                fake_scope_scope_init))

static void
fake_scope_init (FakeScope *scope)
{
  scope->types = g_ptr_array_new_with_free_func (g_free);
  scope->callbacks = g_ptr_array_new_with_free_func (g_free);
}

static void
fake_scope_finalize (GObject *object)
{
  FakeScope *self = FAKE_SCOPE (object);

  g_ptr_array_unref (self->types);
  g_ptr_array_unref (self->callbacks);

  G_OBJECT_CLASS (fake_scope_parent_class)->finalize (object);
}

static void
fake_scope_class_init (FakeScopeClass *class)
{
  G_OBJECT_CLASS (class)->finalize = fake_scope_finalize;
}

/* }}} */
/* {{{ API */

FakeScope *
fake_scope_new (void)
{
  return g_object_new (fake_scope_get_type (), NULL);
}

static int
cmp_strings (gconstpointer a,
             gconstpointer b)
{
  const char **aa = (const char **)a;
  const char **bb = (const char **)b;

  return strcmp (*aa, *bb);
}

static void
g_ptr_array_unique (GPtrArray    *array,
                    GCompareFunc  cmp)
{
  int i;

  i = 1;
  while (i < array->len)
    {
      gconstpointer *one = g_ptr_array_index (array, i - 1);
      gconstpointer *two = g_ptr_array_index (array, i);

      if (cmp (&one, &two) == 0)
        g_ptr_array_remove_index (array, i);
      else
        i++;
    }
}

GPtrArray *
fake_scope_get_types (FakeScope *self)
{
  g_ptr_array_sort (self->types, cmp_strings);
  g_ptr_array_unique (self->types, cmp_strings);

  return self->types;
}

GPtrArray *
fake_scope_get_callbacks (FakeScope *self)
{
  g_ptr_array_sort (self->callbacks, cmp_strings);
  g_ptr_array_unique (self->callbacks, cmp_strings);

  return self->callbacks;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
