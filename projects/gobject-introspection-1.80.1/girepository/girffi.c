/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Helper functions for ffi integration
 *
 * Copyright (C) 2008 Red Hat, Inc
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

#include <sys/types.h>

#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "girffi.h"
#include "girepository.h"
#include "girepository-private.h"

/**
 * SECTION:girffi
 * @short_description: TODO
 * @title: girffi
 *
 * TODO
 */

static ffi_type *
gi_type_tag_get_ffi_type_internal (GITypeTag   tag,
                                   gboolean    is_pointer,
				   gboolean    is_enum)
{
  switch (tag)
    {
    case GI_TYPE_TAG_BOOLEAN:
      return &ffi_type_uint;
    case GI_TYPE_TAG_INT8:
      return &ffi_type_sint8;
    case GI_TYPE_TAG_UINT8:
      return &ffi_type_uint8;
    case GI_TYPE_TAG_INT16:
      return &ffi_type_sint16;
    case GI_TYPE_TAG_UINT16:
      return &ffi_type_uint16;
    case GI_TYPE_TAG_INT32:
      return &ffi_type_sint32;
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_UNICHAR:
      return &ffi_type_uint32;
    case GI_TYPE_TAG_INT64:
      return &ffi_type_sint64;
    case GI_TYPE_TAG_UINT64:
      return &ffi_type_uint64;
    case GI_TYPE_TAG_GTYPE:
#if GLIB_SIZEOF_SIZE_T == 4
      return &ffi_type_uint32;
#elif GLIB_SIZEOF_SIZE_T == 8
      return &ffi_type_uint64;
#else
#  error "Unexpected size for size_t: not 4 or 8"
#endif
    case GI_TYPE_TAG_FLOAT:
      return &ffi_type_float;
    case GI_TYPE_TAG_DOUBLE:
      return &ffi_type_double;
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
    case GI_TYPE_TAG_ARRAY:
    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
    case GI_TYPE_TAG_GHASH:
    case GI_TYPE_TAG_ERROR:
      return &ffi_type_pointer;
    case GI_TYPE_TAG_INTERFACE:
      {
	/* We need to handle enums specially:
	 * https://bugzilla.gnome.org/show_bug.cgi?id=665150
	 */
        if (!is_enum)
          return &ffi_type_pointer;
	else
	  return &ffi_type_sint32;
      }
    case GI_TYPE_TAG_VOID:
      if (is_pointer)
        return &ffi_type_pointer;
      else
        return &ffi_type_void;
    default:
      break;
    }

  g_assert_not_reached ();

  return NULL;
}

/**
 * gi_type_tag_get_ffi_type:
 * @type_tag: A #GITypeTag
 * @is_pointer: Whether or not this is a pointer type
 *
 * TODO
 *
 * Returns: A #ffi_type corresponding to the platform default C ABI for @tag and @is_pointer.
 */
ffi_type *
gi_type_tag_get_ffi_type (GITypeTag   type_tag,
			  gboolean    is_pointer)
{
  return gi_type_tag_get_ffi_type_internal (type_tag, is_pointer, FALSE);
}

/**
 * g_type_info_get_ffi_type:
 * @info: A #GITypeInfo
 *
 * TODO
 *
 * Returns: A #ffi_type corresponding to the platform default C ABI for @info.
 */
ffi_type *
g_type_info_get_ffi_type (GITypeInfo *info)
{
  gboolean is_enum = FALSE;
  GIBaseInfo *iinfo;

  if (g_type_info_get_tag (info) == GI_TYPE_TAG_INTERFACE)
    {
      iinfo = g_type_info_get_interface (info);
      switch (g_base_info_get_type (iinfo))
        {
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:
          is_enum = TRUE;
          break;
        default:
          break;
        }
      g_base_info_unref (iinfo);
    }

  return gi_type_tag_get_ffi_type_internal (g_type_info_get_tag (info), g_type_info_is_pointer (info), is_enum);
}

/**
 * g_callable_info_get_ffi_arg_types:
 * @callable_info: a callable info from a typelib
 * @n_args_p: (out): The number of arguments
 *
 * TODO
 *
 * Returns: an array of ffi_type*. The array itself
 * should be freed using g_free() after use.
 */
static ffi_type **
g_callable_info_get_ffi_arg_types (GICallableInfo *callable_info,
                                   int            *n_args_p)
{
    ffi_type **arg_types;
    gboolean is_method, throws;
    gint n_args, n_invoke_args, i, offset;

    g_return_val_if_fail (callable_info != NULL, NULL);

    n_args = g_callable_info_get_n_args (callable_info);
    is_method = g_callable_info_is_method (callable_info);
    throws = g_callable_info_can_throw_gerror (callable_info);
    offset = is_method ? 1 : 0;

    n_invoke_args = n_args;

    if (is_method)
      n_invoke_args++;
    if (throws)
      n_invoke_args++;

    if (n_args_p)
      *n_args_p = n_invoke_args;

    arg_types = (ffi_type **) g_new0 (ffi_type *, n_invoke_args + 1);

    if (is_method)
      arg_types[0] = &ffi_type_pointer;
    if (throws)
      arg_types[n_invoke_args - 1] = &ffi_type_pointer;

    for (i = 0; i < n_args; ++i)
      {
        GIArgInfo arg_info;
        GITypeInfo arg_type;

        g_callable_info_load_arg (callable_info, i, &arg_info);
        g_arg_info_load_type (&arg_info, &arg_type);
        switch (g_arg_info_get_direction (&arg_info))
          {
            case GI_DIRECTION_IN:
              arg_types[i + offset] = g_type_info_get_ffi_type (&arg_type);
              break;
            case GI_DIRECTION_OUT:
            case GI_DIRECTION_INOUT:
              arg_types[i + offset] = &ffi_type_pointer;
              break;
            default:
              g_assert_not_reached ();
          }
      }

    arg_types[n_invoke_args] = NULL;

    return arg_types;
}

/**
 * g_callable_info_get_ffi_return_type:
 * @callable_info: a callable info from a typelib
 *
 * Fetches the ffi_type for a corresponding return value of
 * a #GICallableInfo
 *
 * Returns: the ffi_type for the return value
 */
static ffi_type *
g_callable_info_get_ffi_return_type (GICallableInfo *callable_info)
{
  GITypeInfo *return_type;
  ffi_type *return_ffi_type;

  g_return_val_if_fail (callable_info != NULL, NULL);

  return_type = g_callable_info_get_return_type (callable_info);
  return_ffi_type = g_type_info_get_ffi_type (return_type);
  g_base_info_unref((GIBaseInfo*)return_type);

  return return_ffi_type;
}

/**
 * g_function_info_prep_invoker:
 * @info: A #GIFunctionInfo
 * @invoker: Output invoker structure
 * @error: A #GError
 *
 * Initialize the caller-allocated @invoker structure with a cache
 * of information needed to invoke the C function corresponding to
 * @info with the platform's default ABI.
 *
 * A primary intent of this function is that a dynamic structure allocated
 * by a language binding could contain a #GIFunctionInvoker structure
 * inside the binding's function mapping.
 *
 * Returns: %TRUE on success, %FALSE otherwise with @error set.
 */
gboolean
g_function_info_prep_invoker (GIFunctionInfo       *info,
                              GIFunctionInvoker    *invoker,
                              GError              **error)
{
  const char *symbol;
  gpointer addr;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (invoker != NULL, FALSE);

  symbol = g_function_info_get_symbol ((GIFunctionInfo*) info);

  if (!g_typelib_symbol (g_base_info_get_typelib((GIBaseInfo *) info),
                         symbol, &addr))
    {
      g_set_error (error,
                   G_INVOKE_ERROR,
                   G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
                   "Could not locate %s: %s", symbol, g_module_error ());

      return FALSE;
    }

  return g_function_invoker_new_for_address (addr, info, invoker, error);
}

/**
 * g_function_invoker_new_for_address:
 * @addr: The address
 * @info: A #GICallableInfo
 * @invoker: Output invoker structure
 * @error: A #GError
 *
 * Initialize the caller-allocated @invoker structure with a cache
 * of information needed to invoke the C function corresponding to
 * @info with the platform's default ABI.
 *
 * A primary intent of this function is that a dynamic structure allocated
 * by a language binding could contain a #GIFunctionInvoker structure
 * inside the binding's function mapping.
 *
 * Returns: %TRUE on success, %FALSE otherwise with @error set.
 */
gboolean
g_function_invoker_new_for_address (gpointer           addr,
                                    GICallableInfo    *info,
                                    GIFunctionInvoker *invoker,
                                    GError           **error)
{
  ffi_type **atypes;
  gint n_args;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (invoker != NULL, FALSE);

  invoker->native_address = addr;

  atypes = g_callable_info_get_ffi_arg_types (info, &n_args);

  return ffi_prep_cif (&(invoker->cif), FFI_DEFAULT_ABI, n_args,
                       g_callable_info_get_ffi_return_type (info),
                       atypes) == FFI_OK;
}

/**
 * g_function_invoker_destroy:
 * @invoker: A #GIFunctionInvoker
 *
 * Release all resources allocated for the internals of @invoker; callers
 * are responsible for freeing any resources allocated for the structure
 * itself however.
 */
void
g_function_invoker_destroy (GIFunctionInvoker    *invoker)
{
  g_free (invoker->cif.arg_types);
}

typedef struct {
  ffi_closure ffi_closure;
  gpointer writable_self;
  gpointer native_address;
} GIClosureWrapper;

/**
 * g_callable_info_create_closure:
 * @callable_info: a callable info from a typelib
 * @cif: a ffi_cif structure
 * @callback: the ffi callback
 * @user_data: data to be passed into the callback
 *
 * Prepares a callback for ffi invocation.
 *
 * Returns: the ffi_closure or NULL on error. The return value
 *   should be freed by calling g_callable_info_destroy_closure().
 */
ffi_closure *
g_callable_info_create_closure (GICallableInfo       *callable_info,
                                ffi_cif              *cif,
                                GIFFIClosureCallback  callback,
                                gpointer              user_data)
{
  gpointer exec_ptr;
  int n_args;
  ffi_type **atypes;
  GIClosureWrapper *closure;
  ffi_status status;

  g_return_val_if_fail (callable_info != NULL, FALSE);
  g_return_val_if_fail (cif != NULL, FALSE);
  g_return_val_if_fail (callback != NULL, FALSE);

  closure = ffi_closure_alloc (sizeof (GIClosureWrapper), &exec_ptr);
  if (!closure)
    {
      g_warning ("could not allocate closure\n");
      return NULL;
    }
  closure->writable_self = closure;
  closure->native_address = exec_ptr;


  atypes = g_callable_info_get_ffi_arg_types (callable_info, &n_args);
  status = ffi_prep_cif (cif, FFI_DEFAULT_ABI, n_args,
                         g_callable_info_get_ffi_return_type (callable_info),
                         atypes);
  if (status != FFI_OK)
    {
      g_warning ("ffi_prep_cif failed: %d\n", status);
      ffi_closure_free (closure);
      return NULL;
    }

  status = ffi_prep_closure_loc (&closure->ffi_closure, cif, callback, user_data, exec_ptr);
  if (status != FFI_OK)
    {
      g_warning ("ffi_prep_closure failed: %d\n", status);
      ffi_closure_free (closure);
      return NULL;
    }

  return &closure->ffi_closure;
}

/**
 * g_callable_info_get_closure_native_address:
 * @callable_info: a callable info from a typelib
 * @closure: ffi closure
 *
 * Gets callable code from ffi_closure prepared by g_callable_info_create_closure()
 */
gpointer *
g_callable_info_get_closure_native_address (GICallableInfo       *callable_info,
                                            ffi_closure          *closure)
{
  GIClosureWrapper *wrapper = (GIClosureWrapper *)closure;
  return wrapper->native_address;
}

/**
 * g_callable_info_destroy_closure:
 * @callable_info: a callable info from a typelib
 * @closure: ffi closure
 *
 * Frees a ffi_closure returned from g_callable_info_create_closure()
 */
void
g_callable_info_destroy_closure (GICallableInfo *callable_info,
                                 ffi_closure    *closure)
{
  GIClosureWrapper *wrapper = (GIClosureWrapper *)closure;

  g_free (wrapper->ffi_closure.cif->arg_types);
  ffi_closure_free (wrapper->writable_self);
}

/**
 * g_callable_info_prepare_closure:
 * @callable_info: a callable info from a typelib
 * @cif: a ffi_cif structure
 * @callback: the ffi callback
 * @user_data: data to be passed into the callback
 *
 * Prepares a callback for ffi invocation.
 *
 * Deprecated: 1.72: Use g_callable_info_create_closure() instead
 *
 * Returns: the native address of the closure or `NULL` on error. The return value
 *   should be freed by calling g_callable_info_free_closure().
 */
ffi_closure *
g_callable_info_prepare_closure (GICallableInfo       *callable_info,
                                 ffi_cif              *cif,
                                 GIFFIClosureCallback  callback,
                                 gpointer              user_data)
{
  ffi_closure * closure;

  closure = g_callable_info_create_closure (callable_info, cif, callback, user_data);
  if (!closure)
    {
      return NULL;
    }

  /* Return the native pointer which, on some systems and ffi versions without static exec trampolines,
   * points to the same underlying memory as closure, but via an executable-non-writable mapping.
   * Deprecated, and kept for backwards compatibility only. Risks segfaults on freeing the closure.
   */
  return (ffi_closure *) g_callable_info_get_closure_native_address(callable_info, closure);
}

/**
 * g_callable_info_free_closure:
 * @callable_info: a callable info from a typelib
 * @closure: ffi closure
 *
 * Deprecated: 1.72: Use g_callable_info_destroy_closure() instead, in
 * conjunction with g_callable_info_create_closure().
 *
 * Should free a ffi_closure returned from g_callable_info_prepare_closure(),
 * which may cause a segfault because the native address is returned instead of
 * the closure address. May do nothing and leak memory instead of freeing to
 * avoid segfaults.
 */
void
g_callable_info_free_closure (GICallableInfo *callable_info,
                              ffi_closure    *closure)
{
#ifdef LEGACY_GIRFFI_FREE
  g_callable_info_destroy_closure(callable_info, closure);
#endif
}
