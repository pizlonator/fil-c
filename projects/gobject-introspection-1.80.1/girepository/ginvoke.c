/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Invoke functionality
 *
 * Copyright (C) 2005 Matthias Clasen
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

#include <girepository.h>
#include "girffi.h"

/**
 * value_to_ffi_type:
 * @gvalue: TODO
 * @value: TODO
 *
 * TODO
 */
static ffi_type *
value_to_ffi_type (const GValue *gvalue, gpointer *value)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (G_VALUE_TYPE (gvalue));
  g_assert (type != G_TYPE_INVALID);

  switch (type)
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_CHAR:
    case G_TYPE_INT:
      rettype = &ffi_type_sint;
      *value = (gpointer)&(gvalue->data[0].v_int);
      break;
    case G_TYPE_UCHAR:
    case G_TYPE_UINT:
      rettype = &ffi_type_uint;
      *value = (gpointer)&(gvalue->data[0].v_uint);
      break;
    case G_TYPE_STRING:
    case G_TYPE_OBJECT:
    case G_TYPE_BOXED:
    case G_TYPE_POINTER:
    case G_TYPE_PARAM:
      rettype = &ffi_type_pointer;
      *value = (gpointer)&(gvalue->data[0].v_pointer);
      break;
    case G_TYPE_FLOAT:
      rettype = &ffi_type_float;
      *value = (gpointer)&(gvalue->data[0].v_float);
      break;
    case G_TYPE_DOUBLE:
      rettype = &ffi_type_double;
      *value = (gpointer)&(gvalue->data[0].v_double);
      break;
    case G_TYPE_LONG:
      rettype = &ffi_type_slong;
      *value = (gpointer)&(gvalue->data[0].v_long);
      break;
    case G_TYPE_ULONG:
      rettype = &ffi_type_ulong;
      *value = (gpointer)&(gvalue->data[0].v_ulong);
      break;
    case G_TYPE_INT64:
      rettype = &ffi_type_sint64;
      *value = (gpointer)&(gvalue->data[0].v_int64);
      break;
    case G_TYPE_UINT64:
      rettype = &ffi_type_uint64;
      *value = (gpointer)&(gvalue->data[0].v_uint64);
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
 * @gvalue: TODO
 * @ffi_value: TODO
 * @value: TODO
 *
 * TODO
 */
static ffi_type *
g_value_to_ffi_return_type (const GValue *gvalue,
			    const GIArgument *ffi_value,
			    gpointer *value)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (G_VALUE_TYPE (gvalue));
  g_assert (type != G_TYPE_INVALID);

  *value = (gpointer)&(ffi_value->v_long);

  switch (type) {
  case G_TYPE_CHAR:
    rettype = &ffi_type_sint8;
    break;
  case G_TYPE_UCHAR:
    rettype = &ffi_type_uint8;
    break;
  case G_TYPE_BOOLEAN:
  case G_TYPE_INT:
    rettype = &ffi_type_sint;
    break;
  case G_TYPE_UINT:
    rettype = &ffi_type_uint;
    break;
  case G_TYPE_STRING:
  case G_TYPE_OBJECT:
  case G_TYPE_BOXED:
  case G_TYPE_POINTER:
  case G_TYPE_PARAM:
    rettype = &ffi_type_pointer;
    break;
  case G_TYPE_FLOAT:
    rettype = &ffi_type_float;
    *value = (gpointer)&(ffi_value->v_float);
    break;
  case G_TYPE_DOUBLE:
    rettype = &ffi_type_double;
    *value = (gpointer)&(ffi_value->v_double);
    break;
  case G_TYPE_LONG:
    rettype = &ffi_type_slong;
    break;
  case G_TYPE_ULONG:
    rettype = &ffi_type_ulong;
    break;
  case G_TYPE_INT64:
    rettype = &ffi_type_sint64;
    *value = (gpointer)&(ffi_value->v_int64);
    break;
  case G_TYPE_UINT64:
    rettype = &ffi_type_uint64;
    *value = (gpointer)&(ffi_value->v_uint64);
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
 * @gvalue: TODO
 * @value: TODO
 *
 * TODO
 */
static void
g_value_from_ffi_value (GValue           *gvalue,
                        const GIArgument *value)
{
  switch (g_type_fundamental (G_VALUE_TYPE (gvalue))) {
  case G_TYPE_INT:
      g_value_set_int (gvalue, (gint)value->v_long);
      break;
  case G_TYPE_FLOAT:
      g_value_set_float (gvalue, (gfloat)value->v_float);
      break;
  case G_TYPE_DOUBLE:
      g_value_set_double (gvalue, (gdouble)value->v_double);
      break;
  case G_TYPE_BOOLEAN:
      g_value_set_boolean (gvalue, (gboolean)value->v_long);
      break;
  case G_TYPE_STRING:
      g_value_set_string (gvalue, (gchar*)value->v_pointer);
      break;
  case G_TYPE_CHAR:
      g_value_set_schar (gvalue, (gchar)value->v_long);
      break;
  case G_TYPE_UCHAR:
      g_value_set_uchar (gvalue, (guchar)value->v_ulong);
      break;
  case G_TYPE_UINT:
      g_value_set_uint (gvalue, (guint)value->v_ulong);
      break;
  case G_TYPE_POINTER:
      g_value_set_pointer (gvalue, (gpointer)value->v_pointer);
      break;
  case G_TYPE_LONG:
      g_value_set_long (gvalue, (glong)value->v_long);
      break;
  case G_TYPE_ULONG:
      g_value_set_ulong (gvalue, (gulong)value->v_ulong);
      break;
  case G_TYPE_INT64:
      g_value_set_int64 (gvalue, (gint64)value->v_int64);
      break;
  case G_TYPE_UINT64:
      g_value_set_uint64 (gvalue, (guint64)value->v_uint64);
      break;
  case G_TYPE_BOXED:
      g_value_set_boxed (gvalue, (gpointer)value->v_pointer);
      break;
  case G_TYPE_PARAM:
      g_value_set_param (gvalue, (gpointer)value->v_pointer);
      break;
  default:
    g_warning ("Unsupported fundamental type: %s",
	       g_type_name (g_type_fundamental (G_VALUE_TYPE (gvalue))));
  }

}

/**
 * gi_cclosure_marshal_generic: (skip)
 * @closure: TODO
 * @return_gvalue: TODO
 * @n_param_values: TODO
 * @param_values: TODO
 * @invocation_hint: TODO
 * @marshal_data: TODO
 *
 * TODO
 */
void
gi_cclosure_marshal_generic (GClosure *closure,
                             GValue *return_gvalue,
                             guint n_param_values,
                             const GValue *param_values,
                             gpointer invocation_hint,
                             gpointer marshal_data)
{
  GIArgument return_ffi_value = { 0, };
  ffi_type *rtype;
  void *rvalue;
  int n_args;
  ffi_type **atypes;
  void **args;
  int i;
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
  args =  g_alloca (sizeof (gpointer) * n_args);

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
