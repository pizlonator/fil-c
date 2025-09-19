/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Invoke functionality
 *
 * Copyright (C) 2005 Matthias Clasen
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>

#include <girepository/girepository.h>
#include "girffi.h"
#include <inttypes.h>

/**
 * value_to_ffi_type:
 * @gvalue: (transfer none): a [type@GObject.Value] to convert
 * @value: (out caller-allocates): return location for the ffi data
 *
 * Convert @gvalue to a format suitable for passing to ffi.
 *
 * @value is only valid as long as @gvalue is alive.
 *
 * Returns: pointer to the `ffi_type` associated with @value
 * Since: 2.80
 */
static ffi_type *
value_to_ffi_type (const GValue *gvalue, void **value)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (G_VALUE_TYPE (gvalue));
  g_assert (type != G_TYPE_INVALID);

  switch ((uintptr_t) type)
    {
    case (uintptr_t) G_TYPE_BOOLEAN:
    case (uintptr_t) G_TYPE_CHAR:
    case (uintptr_t) G_TYPE_INT:
      rettype = &ffi_type_sint;
      *value = (void *) &(gvalue->data[0].v_int);
      break;
    case (uintptr_t) G_TYPE_UCHAR:
    case (uintptr_t) G_TYPE_UINT:
      rettype = &ffi_type_uint;
      *value = (void *) &(gvalue->data[0].v_uint);
      break;
    case (uintptr_t) G_TYPE_STRING:
    case (uintptr_t) G_TYPE_OBJECT:
    case (uintptr_t) G_TYPE_BOXED:
    case (uintptr_t) G_TYPE_POINTER:
    case (uintptr_t) G_TYPE_PARAM:
      rettype = &ffi_type_pointer;
      *value = (void *) &(gvalue->data[0].v_pointer);
      break;
    case (uintptr_t) G_TYPE_FLOAT:
      rettype = &ffi_type_float;
      *value = (void *) &(gvalue->data[0].v_float);
      break;
    case (uintptr_t) G_TYPE_DOUBLE:
      rettype = &ffi_type_double;
      *value = (void *) &(gvalue->data[0].v_double);
      break;
    case (uintptr_t) G_TYPE_LONG:
      rettype = &ffi_type_slong;
      *value = (void *) &(gvalue->data[0].v_long);
      break;
    case (uintptr_t) G_TYPE_ULONG:
      rettype = &ffi_type_ulong;
      *value = (void *) &(gvalue->data[0].v_ulong);
      break;
    case (uintptr_t) G_TYPE_INT64:
      rettype = &ffi_type_sint64;
      *value = (void *) &(gvalue->data[0].v_int64);
      break;
    case (uintptr_t) G_TYPE_UINT64:
      rettype = &ffi_type_uint64;
      *value = (void *) &(gvalue->data[0].v_uint64);
      break;
    default:
      rettype = &ffi_type_pointer;
      *value = NULL;
      g_warning ("Unsupported fundamental type: %s", g_type_name (type));
      break;
    }
  return rettype;
}

/**
 * g_value_to_ffi_return_type:
 * @gvalue: (transfer none): a [type@GObject.Value] to convert
 * @ffi_value: (transfer none): a [type@GIRepository.Argument] containing the
 *   data to use
 * @value: (out caller-allocates): return location for the ffi data
 *
 * Convert @ffi_value to a format suitable for passing to ffi, using the type
 * data from @gvalue.
 *
 * @value is only valid as long as @gvalue and @ffi_value are alive.
 *
 * Returns: pointer to the `ffi_type` associated with @value
 * Since: 2.80
 */
static ffi_type *
g_value_to_ffi_return_type (const GValue *gvalue,
                            const GIArgument *ffi_value,
                            void **value)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (G_VALUE_TYPE (gvalue));
  g_assert (type != G_TYPE_INVALID);

  *value = (void *) &(ffi_value->v_long);

  switch ((uintptr_t) type) {
  case (uintptr_t) G_TYPE_CHAR:
    rettype = &ffi_type_sint8;
    break;
  case (uintptr_t) G_TYPE_UCHAR:
    rettype = &ffi_type_uint8;
    break;
  case (uintptr_t) G_TYPE_BOOLEAN:
  case (uintptr_t) G_TYPE_INT:
    rettype = &ffi_type_sint;
    break;
  case (uintptr_t) G_TYPE_UINT:
    rettype = &ffi_type_uint;
    break;
  case (uintptr_t) G_TYPE_STRING:
  case (uintptr_t) G_TYPE_OBJECT:
  case (uintptr_t) G_TYPE_BOXED:
  case (uintptr_t) G_TYPE_POINTER:
  case (uintptr_t) G_TYPE_PARAM:
    rettype = &ffi_type_pointer;
    break;
  case (uintptr_t) G_TYPE_FLOAT:
    rettype = &ffi_type_float;
    *value = (void *) &(ffi_value->v_float);
    break;
  case (uintptr_t) G_TYPE_DOUBLE:
    rettype = &ffi_type_double;
    *value = (void *) &(ffi_value->v_double);
    break;
  case (uintptr_t) G_TYPE_LONG:
    rettype = &ffi_type_slong;
    break;
  case (uintptr_t) G_TYPE_ULONG:
    rettype = &ffi_type_ulong;
    break;
  case (uintptr_t) G_TYPE_INT64:
    rettype = &ffi_type_sint64;
    *value = (void *) &(ffi_value->v_int64);
    break;
  case (uintptr_t) G_TYPE_UINT64:
    rettype = &ffi_type_uint64;
    *value = (void *) &(ffi_value->v_uint64);
    break;
  default:
    rettype = &ffi_type_pointer;
    *value = NULL;
    g_warning ("Unsupported fundamental type: %s", g_type_name (type));
    break;
  }
  return rettype;
}

/**
 * g_value_from_ffi_value:
 * @gvalue: (inout): a [type@GObject.Value] to set
 * @value: (transfer none): ffi data to convert
 *
 * Convert @value to a [type@GObject.Value] according to the type already set
 * on @gvalue.
 *
 * @gvalue is valid even after @value is finalised.
 *
 * Since: 2.80
 */
static void
g_value_from_ffi_value (GValue           *gvalue,
                        const GIArgument *value)
{
  switch ((uintptr_t) g_type_fundamental (G_VALUE_TYPE (gvalue))) {
  case (uintptr_t) G_TYPE_INT:
      g_value_set_int (gvalue, (gint)value->v_long);
      break;
  case (uintptr_t) G_TYPE_FLOAT:
      g_value_set_float (gvalue, (gfloat)value->v_float);
      break;
  case (uintptr_t) G_TYPE_DOUBLE:
      g_value_set_double (gvalue, (gdouble)value->v_double);
      break;
  case (uintptr_t) G_TYPE_BOOLEAN:
      g_value_set_boolean (gvalue, (gboolean)value->v_long);
      break;
  case (uintptr_t) G_TYPE_STRING:
      g_value_set_string (gvalue, (char*)value->v_pointer);
      break;
  case (uintptr_t) G_TYPE_CHAR:
      g_value_set_schar (gvalue, (char)value->v_long);
      break;
  case (uintptr_t) G_TYPE_UCHAR:
      g_value_set_uchar (gvalue, (guchar)value->v_ulong);
      break;
  case (uintptr_t) G_TYPE_UINT:
      g_value_set_uint (gvalue, (guint)value->v_ulong);
      break;
  case (uintptr_t) G_TYPE_POINTER:
      g_value_set_pointer (gvalue, (gpointer)value->v_pointer);
      break;
  case (uintptr_t) G_TYPE_LONG:
      g_value_set_long (gvalue, (glong)value->v_long);
      break;
  case (uintptr_t) G_TYPE_ULONG:
      g_value_set_ulong (gvalue, (gulong)value->v_ulong);
      break;
  case (uintptr_t) G_TYPE_INT64:
      g_value_set_int64 (gvalue, (gint64)value->v_int64);
      break;
  case (uintptr_t) G_TYPE_UINT64:
      g_value_set_uint64 (gvalue, (guint64)value->v_uint64);
      break;
  case (uintptr_t) G_TYPE_BOXED:
      g_value_set_boxed (gvalue, (gpointer)value->v_pointer);
      break;
  case (uintptr_t) G_TYPE_PARAM:
      g_value_set_param (gvalue, (gpointer)value->v_pointer);
      break;
  default:
    g_warning ("Unsupported fundamental type: %s",
               g_type_name (g_type_fundamental (G_VALUE_TYPE (gvalue))));
  }

}

/**
 * gi_cclosure_marshal_generic: (skip)
 * @closure: a [type@GObject.Closure]
 * @return_gvalue: (optional) (out caller-allocates): return location for the
 *   return value from the closure, or `NULL` to ignore
 * @n_param_values: number of param values
 * @param_values: (array length=n_param_values): values to pass to the closure
 *   parameters
 * @invocation_hint: invocation hint
 * @marshal_data: marshal data
 *
 * A generic C closure marshal function using ffi and
 * [type@GIRepository.Argument].
 *
 * Since: 2.80
 */
void
gi_cclosure_marshal_generic (GClosure *closure,
                             GValue *return_gvalue,
                             unsigned int n_param_values,
                             const GValue *param_values,
                             void *invocation_hint,
                             void *marshal_data)
{
  GIArgument return_ffi_value = { 0, };
  ffi_type *rtype;
  void *rvalue;
  unsigned int n_args;
  ffi_type **atypes;
  void **args;
  unsigned int i;
  ffi_cif cif;
  GCClosure *cc = (GCClosure*) closure;

  if (return_gvalue && G_VALUE_TYPE (return_gvalue))
    {
      rtype = g_value_to_ffi_return_type (return_gvalue, &return_ffi_value,
                                          &rvalue);
    }
  else
    {
      rtype = &ffi_type_void;
      rvalue = &return_ffi_value.v_long;
    }

  n_args = n_param_values + 1;
  atypes = g_alloca (sizeof (ffi_type *) * n_args);
  args =  g_alloca (sizeof (void *) * n_args);

  if (n_param_values > 0)
    {
      if (G_CCLOSURE_SWAP_DATA (closure))
        {
          atypes[n_args-1] = value_to_ffi_type (param_values + 0,
                                                &args[n_args-1]);
          atypes[0] = &ffi_type_pointer;
          args[0] = &closure->data;
        }
      else
        {
          atypes[0] = value_to_ffi_type (param_values + 0, &args[0]);
          atypes[n_args-1] = &ffi_type_pointer;
          args[n_args-1] = &closure->data;
        }
    }
  else
    {
      atypes[0] = &ffi_type_pointer;
      args[0] = &closure->data;
    }

  for (i = 1; i < n_args - 1; i++)
    atypes[i] = value_to_ffi_type (param_values + i, &args[i]);

  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, n_args, rtype, atypes) != FFI_OK)
    return;

  ffi_call (&cif, marshal_data ? marshal_data : cc->callback, rvalue, args);

  if (return_gvalue && G_VALUE_TYPE (return_gvalue))
    g_value_from_ffi_value (return_gvalue, &return_ffi_value);
}
