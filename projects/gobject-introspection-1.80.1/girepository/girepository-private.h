/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Private headers
 *
 * Copyright (C) 2010 Johan Dahlin
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

#ifndef __GIREPOSITORY_PRIVATE_H__
#define __GIREPOSITORY_PRIVATE_H__

#include <ffi.h>
#include <glib.h>

#define __GIREPOSITORY_H_INSIDE__

#include <gibaseinfo.h>
#include <girepository.h>
#include <gitypelib.h>

typedef struct _GIRealInfo GIRealInfo;

/* We changed a gint32 -> gint in the structure below, which should be
 * valid everywhere we care about.
 */
G_STATIC_ASSERT (sizeof (int) == sizeof (gint32));

/*
 * We just use one structure for all of the info object
 * types; in general, we should be reading data directly
 * from the typelib, and not having computed data in
 * per-type structures.
 */
struct _GIRealInfo
{
  /* Keep this part in sync with GIUnresolvedInfo below */
  gint32 type;
  volatile gint ref_count;
  GIRepository *repository;
  GIBaseInfo *container;

  /* Resolved specific */

  GITypelib *typelib;
  guint32 offset;

  guint32 type_is_embedded : 1; /* Used by GITypeInfo */
  guint32 reserved : 31;

  gpointer reserved2[4];
};

struct _GIUnresolvedInfo
{
  /* Keep this part in sync with GIBaseInfo above */
  gint32 type;
  volatile gint ref_count;
  GIRepository *repository;
  GIBaseInfo *container;

  /* Unresolved specific */

  const gchar *name;
  const gchar *namespace;
};

void         _g_info_init       (GIRealInfo   *info,
                                 GIInfoType    type,
                                 GIRepository *repository,
                                 GIBaseInfo   *container,
                                 GITypelib     *typelib,
                                 guint32       offset);

GIBaseInfo * _g_info_from_entry (GIRepository *repository,
                                 GITypelib     *typelib,
                                 guint16       index);

GIBaseInfo * _g_info_new_full   (GIInfoType    type,
				 GIRepository *repository,
				 GIBaseInfo   *container,
				 GITypelib     *typelib,
				 guint32       offset);

GITypeInfo * _g_type_info_new   (GIBaseInfo   *container,
				 GITypelib     *typelib,
				 guint32       offset);

void         _g_type_info_init  (GIBaseInfo   *info,
				 GIBaseInfo   *container,
				 GITypelib     *typelib,
				 guint32       offset);

GIFunctionInfo * _g_base_info_find_method (GIBaseInfo   *base,
					   guint32       offset,
					   gint          n_methods,
					   const gchar  *name);

GIVFuncInfo * _g_base_info_find_vfunc (GIRealInfo   *rinfo,
				       guint32       offset,
				       gint          n_vfuncs,
				       const gchar  *name);

#endif /* __GIREPOSITORY_PRIVATE_H__ */
