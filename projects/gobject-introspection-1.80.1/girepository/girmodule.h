/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Parsed IDL
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

#ifndef __G_IR_MODULE_H__
#define __G_IR_MODULE_H__

#include <glib.h>
#include "gitypelib-internal.h"

G_BEGIN_DECLS

typedef struct _GIrTypelibBuild GIrTypelibBuild;
typedef struct _GIrModule GIrModule;

struct _GIrTypelibBuild {
  GIrModule  *module;
  GHashTable  *strings;
  GHashTable  *types;
  GList       *nodes_with_attributes;
  guint32      n_attributes;
  guchar      *data;
  GList       *stack; 
};

struct _GIrModule
{
  gchar *name;
  gchar *version;
  gchar *shared_library;
  gchar *c_prefix;
  GList *dependencies;
  GList *entries;

  /* All modules that are included directly or indirectly */
  GList *include_modules;

  /* Aliases defined in the module or in included modules */
  GHashTable *aliases;

  /* Structures with the 'pointer' flag (typedef struct _X *X)
   * in the module or in included modules
   */
  GHashTable *pointer_structures;
  /* Same as 'pointer' structures, but with the deprecated
   * 'disguised' flag
   */
  GHashTable *disguised_structures;
};

GIrModule *_g_ir_module_new            (const gchar *name,
					const gchar *nsversion,
					const gchar *module_filename,
					const gchar *c_prefix);
void       _g_ir_module_free           (GIrModule  *module);

void       _g_ir_module_add_include_module (GIrModule  *module,
					   GIrModule  *include_module);

GITypelib * _g_ir_module_build_typelib  (GIrModule  *module);

void       _g_ir_module_fatal (GIrTypelibBuild  *build, guint line, const char *msg, ...) G_GNUC_PRINTF (3, 4) G_GNUC_NORETURN;

void _g_irnode_init_stats (void);
void _g_irnode_dump_stats (void);

G_END_DECLS

#endif  /* __G_IR_MODULE_H__ */
