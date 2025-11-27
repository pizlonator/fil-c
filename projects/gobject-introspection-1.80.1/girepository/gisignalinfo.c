/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Signal implementation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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

#include <glib.h>

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

/**
 * SECTION:gisignalinfo
 * @title: GISignalInfo
 * @short_description: Struct representing a signal
 *
 * GISignalInfo represents a signal.
 *
 * It's a sub-struct of #GICallableInfo and contains a set of flags and
 * a class closure.
 *
 * See #GICallableInfo for information on how to retreive arguments
 * and other metadata from the signal.
 */

/**
 * g_signal_info_get_flags:
 * @info: a #GISignalInfo
 *
 * Obtain the flags for this signal info. See #GSignalFlags for
 * more information about possible flag values.
 *
 * Returns: the flags
 */
GSignalFlags
g_signal_info_get_flags (GISignalInfo *info)
{
  GSignalFlags flags;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_SIGNAL_INFO (info), 0);

  blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];
  flags = 0;

  if (blob->run_first)
    flags = flags | G_SIGNAL_RUN_FIRST;

  if (blob->run_last)
    flags = flags | G_SIGNAL_RUN_LAST;

  if (blob->run_cleanup)
    flags = flags | G_SIGNAL_RUN_CLEANUP;

  if (blob->no_recurse)
    flags = flags | G_SIGNAL_NO_RECURSE;

  if (blob->detailed)
    flags = flags | G_SIGNAL_DETAILED;

  if (blob->action)
    flags = flags | G_SIGNAL_ACTION;

  if (blob->no_hooks)
    flags = flags | G_SIGNAL_NO_HOOKS;

  return flags;
}

/**
 * g_signal_info_get_class_closure:
 * @info: a #GISignalInfo
 *
 * Obtain the class closure for this signal if one is set. The class
 * closure is a virtual function on the type that the signal belongs to.
 * If the signal lacks a closure %NULL will be returned.
 *
 * Returns: (transfer full): the class closure or %NULL
 */
GIVFuncInfo *
g_signal_info_get_class_closure (GISignalInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_SIGNAL_INFO (info), 0);

  blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->has_class_closure)
    return g_interface_info_get_vfunc ((GIInterfaceInfo *)rinfo->container, blob->class_closure);

  return NULL;
}

/**
 * g_signal_info_true_stops_emit:
 * @info: a #GISignalInfo
 *
 * Obtain if the returning true in the signal handler will
 * stop the emission of the signal.
 *
 * Returns: %TRUE if returning true stops the signal emission
 */
gboolean
g_signal_info_true_stops_emit (GISignalInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_SIGNAL_INFO (info), 0);

  blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->true_stops_emit;
}

