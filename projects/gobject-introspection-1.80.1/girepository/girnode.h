/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Parsed GIR
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

#ifndef __G_IR_NODE_H__
#define __G_IR_NODE_H__

#include <glib.h>

#include "girmodule.h"

G_BEGIN_DECLS

typedef struct _GIrNode GIrNode;
typedef struct _GIrNodeFunction GIrNodeFunction;
typedef struct _GIrNodeParam GIrNodeParam;
typedef struct _GIrNodeType GIrNodeType;
typedef struct _GIrNodeInterface GIrNodeInterface;
typedef struct _GIrNodeSignal GIrNodeSignal;
typedef struct _GIrNodeProperty GIrNodeProperty;
typedef struct _GIrNodeVFunc GIrNodeVFunc;
typedef struct _GIrNodeField GIrNodeField;
typedef struct _GIrNodeValue GIrNodeValue;
typedef struct _GIrNodeEnum GIrNodeEnum;
typedef struct _GIrNodeBoxed GIrNodeBoxed;
typedef struct _GIrNodeStruct GIrNodeStruct;
typedef struct _GIrNodeConstant GIrNodeConstant;
typedef struct _GIrNodeXRef GIrNodeXRef;
typedef struct _GIrNodeUnion GIrNodeUnion;

typedef enum
{
  G_IR_NODE_INVALID      =  0,
  G_IR_NODE_FUNCTION     =  1,
  G_IR_NODE_CALLBACK     =  2,
  G_IR_NODE_STRUCT       =  3,
  G_IR_NODE_BOXED        =  4,
  G_IR_NODE_ENUM         =  5,
  G_IR_NODE_FLAGS        =  6,
  G_IR_NODE_OBJECT       =  7,
  G_IR_NODE_INTERFACE    =  8,
  G_IR_NODE_CONSTANT     =  9,
  G_IR_NODE_INVALID_0    = 10, /* DELETED - used to be ERROR_DOMAIN */
  G_IR_NODE_UNION        = 11,
  G_IR_NODE_PARAM        = 12,
  G_IR_NODE_TYPE         = 13,
  G_IR_NODE_PROPERTY     = 14,
  G_IR_NODE_SIGNAL       = 15,
  G_IR_NODE_VALUE        = 16,
  G_IR_NODE_VFUNC        = 17,
  G_IR_NODE_FIELD        = 18,
  G_IR_NODE_XREF         = 19
} GIrNodeTypeId;

struct _GIrNode
{
  GIrNodeTypeId type;
  gchar *name;
  GIrModule *module;

  guint32 offset; /* Assigned as we build the typelib */

  GHashTable *attributes;
};

struct _GIrNodeXRef
{
  GIrNode node;

  gchar *namespace;
};

struct _GIrNodeFunction
{
  GIrNode node;

  gboolean deprecated;
  gboolean is_varargs; /* Not in typelib yet */

  gboolean is_method;
  gboolean is_setter;
  gboolean is_getter;
  gboolean is_constructor;
  gboolean wraps_vfunc;
  gboolean throws;
  gboolean instance_transfer_full;

  gchar *symbol;
  char *property;

  GIrNodeParam *result;
  GList *parameters;
};

struct _GIrNodeType
{
  GIrNode node;

  gboolean is_pointer;
  gboolean is_basic;
  gboolean is_array;
  gboolean is_glist;
  gboolean is_gslist;
  gboolean is_ghashtable;
  gboolean is_interface;
  gboolean is_error;
  gint tag;

  gchar *unparsed;

  gboolean zero_terminated;
  gboolean has_length;
  gint length;
  gboolean has_size;
  gint size;
  gint array_type;

  GIrNodeType *parameter_type1;
  GIrNodeType *parameter_type2;

  gchar *giinterface;
  gchar **errors;
};

struct _GIrNodeParam
{
  GIrNode node;

  gboolean in;
  gboolean out;
  gboolean caller_allocates;
  gboolean optional;
  gboolean retval;
  gboolean nullable;
  gboolean skip;
  gboolean transfer;
  gboolean shallow_transfer;
  GIScopeType scope;

  gint8 closure;
  gint8 destroy;

  GIrNodeType *type;
};

struct _GIrNodeProperty
{
  GIrNode node;

  gboolean deprecated;

  gchar *name;
  gboolean readable;
  gboolean writable;
  gboolean construct;
  gboolean construct_only;
  gboolean transfer;
  gboolean shallow_transfer;

  char *setter;
  char *getter;

  GIrNodeType *type;
};

struct _GIrNodeSignal
{
  GIrNode node;

  gboolean deprecated;

  gboolean run_first;
  gboolean run_last;
  gboolean run_cleanup;
  gboolean no_recurse;
  gboolean detailed;
  gboolean action;
  gboolean no_hooks;
  gboolean instance_transfer_full;

  gboolean has_class_closure;
  gboolean true_stops_emit;

  gint class_closure;

  GList *parameters;
  GIrNodeParam *result;
};

struct _GIrNodeVFunc
{
  GIrNode node;

  gboolean is_varargs; /* Not in typelib yet */
  gboolean must_chain_up;
  gboolean must_be_implemented;
  gboolean must_not_be_implemented;
  gboolean is_class_closure;
  gboolean throws;
  gboolean instance_transfer_full;

  char *invoker;

  GList *parameters;
  GIrNodeParam *result;

  gint offset;
};

struct _GIrNodeField
{
  GIrNode node;

  gboolean readable;
  gboolean writable;
  gint bits;
  gint offset;
  GIrNodeFunction *callback;

  GIrNodeType *type;
};

struct _GIrNodeInterface
{
  GIrNode node;

  gboolean abstract;
  gboolean deprecated;
  gboolean fundamental;
  gboolean final_;

  gchar *gtype_name;
  gchar *gtype_init;

  gchar *ref_func;
  gchar *unref_func;
  gchar *set_value_func;
  gchar *get_value_func;

  gchar *parent;
  gchar *glib_type_struct;

  GList *interfaces;
  GList *prerequisites;

  gint alignment;
  gint size;

  GList *members;
};

struct _GIrNodeValue
{
  GIrNode node;

  gboolean deprecated;

  gint64 value;
};

struct _GIrNodeConstant
{
  GIrNode node;

  gboolean deprecated;

  GIrNodeType *type;

  gchar *value;
};

struct _GIrNodeEnum
{
  GIrNode node;

  gboolean deprecated;
  gint storage_type;

  gchar *gtype_name;
  gchar *gtype_init;
  gchar *error_domain;

  GList *values;
  GList *methods;
};

struct _GIrNodeBoxed
{
  GIrNode node;

  gboolean deprecated;

  gchar *gtype_name;
  gchar *gtype_init;

  gint alignment;
  gint size;

  GList *members;
};

struct _GIrNodeStruct
{
  GIrNode node;

  gboolean deprecated;
  gboolean disguised;
  gboolean opaque;
  gboolean pointer;
  gboolean is_gtype_struct;
  gboolean foreign;

  gchar *gtype_name;
  gchar *gtype_init;

  gchar *copy_func;
  gchar *free_func;

  gint alignment;
  gint size;

  GList *members;
};

struct _GIrNodeUnion
{
  GIrNode node;

  gboolean deprecated;

  GList *members;
  GList *discriminators;

  gchar *gtype_name;
  gchar *gtype_init;

  gchar *copy_func;
  gchar *free_func;

  gint alignment;
  gint size;

  gint discriminator_offset;
  GIrNodeType *discriminator_type;
};


GIrNode * _g_ir_node_new             (GIrNodeTypeId type,
				     GIrModule     *module);
void      _g_ir_node_free            (GIrNode    *node);
guint32   _g_ir_node_get_size        (GIrNode    *node);
guint32   _g_ir_node_get_full_size   (GIrNode    *node);
void      _g_ir_node_build_typelib   (GIrNode         *node,
				      GIrNode         *parent,
				      GIrTypelibBuild *build,
				      guint32         *offset,
				      guint32         *offset2,
				      guint16         *count2);
int       _g_ir_node_cmp             (GIrNode    *node,
				     GIrNode    *other);
gboolean  _g_ir_node_can_have_member (GIrNode    *node);
void      _g_ir_node_add_member      (GIrNode         *node,
				      GIrNodeFunction *member);
guint32   _g_ir_write_string              (const gchar *str,
					   GHashTable  *strings,
					   guchar      *data,
					   guint32     *offset);

const gchar * _g_ir_node_param_direction_string (GIrNodeParam * node);
const gchar * _g_ir_node_type_to_string         (GIrNodeTypeId type);

GIrNode *_g_ir_find_node (GIrTypelibBuild  *build,
			 GIrModule        *module,
			 const char       *name);

/* In giroffsets.c */

void _g_ir_node_compute_offsets (GIrTypelibBuild *build,
				GIrNode         *node);


G_END_DECLS

#endif  /* __G_IR_NODE_H__ */
