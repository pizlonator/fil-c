/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Typelib creation
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "girmodule.h"
#include "girnode.h"
#include "gitypelib-internal.h"

#ifdef _MSC_VER
#define strtoll _strtoi64
#define strtoull _strtoui64
#endif

static gulong string_count = 0;
static gulong unique_string_count = 0;
static gulong string_size = 0;
static gulong unique_string_size = 0;
static gulong types_count = 0;
static gulong unique_types_count = 0;

void
_g_irnode_init_stats (void)
{
  string_count = 0;
  unique_string_count = 0;
  string_size = 0;
  unique_string_size = 0;
  types_count = 0;
  unique_types_count = 0;
}

void
_g_irnode_dump_stats (void)
{
  g_message ("%lu strings (%lu before sharing), %lu bytes (%lu before sharing)",
	     unique_string_count, string_count, unique_string_size, string_size);
  g_message ("%lu types (%lu before sharing)", unique_types_count, types_count);
}

#define DO_ALIGNED_COPY(dest_addr, value, type) \
do {                                            \
	type tmp_var;	                        \
	tmp_var = value;		        \
	memcpy(dest_addr, &tmp_var, sizeof(type));	\
} while(0)

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))


const gchar *
_g_ir_node_type_to_string (GIrNodeTypeId type)
{
  switch (type)
    {
    case G_IR_NODE_FUNCTION:
      return "function";
    case G_IR_NODE_CALLBACK:
      return "callback";
    case G_IR_NODE_PARAM:
      return "param";
    case G_IR_NODE_TYPE:
      return "type";
    case G_IR_NODE_OBJECT:
      return "object";
    case G_IR_NODE_INTERFACE:
      return "interface";
    case G_IR_NODE_SIGNAL:
      return "signal";
    case G_IR_NODE_PROPERTY:
      return "property";
    case G_IR_NODE_VFUNC:
      return "vfunc";
    case G_IR_NODE_FIELD:
      return "field";
    case G_IR_NODE_ENUM:
      return "enum";
    case G_IR_NODE_FLAGS:
      return "flags";
    case G_IR_NODE_BOXED:
      return "boxed";
    case G_IR_NODE_STRUCT:
      return "struct";
    case G_IR_NODE_VALUE:
      return "value";
    case G_IR_NODE_CONSTANT:
      return "constant";
    case G_IR_NODE_XREF:
      return "xref";
    case G_IR_NODE_UNION:
      return "union";
    default:
      return "unknown";
    }
}

GIrNode *
_g_ir_node_new (GIrNodeTypeId  type,
	       GIrModule     *module)
{
  GIrNode *node = NULL;

  switch (type)
    {
   case G_IR_NODE_FUNCTION:
   case G_IR_NODE_CALLBACK:
      node = g_malloc0 (sizeof (GIrNodeFunction));
      break;

   case G_IR_NODE_PARAM:
      node = g_malloc0 (sizeof (GIrNodeParam));
      break;

   case G_IR_NODE_TYPE:
      node = g_malloc0 (sizeof (GIrNodeType));
      break;

    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
      node = g_malloc0 (sizeof (GIrNodeInterface));
      break;

    case G_IR_NODE_SIGNAL:
      node = g_malloc0 (sizeof (GIrNodeSignal));
      break;

    case G_IR_NODE_PROPERTY:
      node = g_malloc0 (sizeof (GIrNodeProperty));
      break;

    case G_IR_NODE_VFUNC:
      node = g_malloc0 (sizeof (GIrNodeFunction));
      break;

    case G_IR_NODE_FIELD:
      node = g_malloc0 (sizeof (GIrNodeField));
      break;

    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      node = g_malloc0 (sizeof (GIrNodeEnum));
      break;

    case G_IR_NODE_BOXED:
      node = g_malloc0 (sizeof (GIrNodeBoxed));
      break;

    case G_IR_NODE_STRUCT:
      node = g_malloc0 (sizeof (GIrNodeStruct));
      break;

    case G_IR_NODE_VALUE:
      node = g_malloc0 (sizeof (GIrNodeValue));
      break;

    case G_IR_NODE_CONSTANT:
      node = g_malloc0 (sizeof (GIrNodeConstant));
      break;

    case G_IR_NODE_XREF:
      node = g_malloc0 (sizeof (GIrNodeXRef));
      break;

    case G_IR_NODE_UNION:
      node = g_malloc0 (sizeof (GIrNodeUnion));
      break;

    default:
      g_error ("Unhandled node type %d\n", type);
      break;
    }

  node->type = type;
  node->module = module;
  node->offset = 0;
  node->attributes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, g_free);

  return node;
}

void
_g_ir_node_free (GIrNode *node)
{
  GList *l;

  if (node == NULL)
    return;

  switch (node->type)
    {
    case G_IR_NODE_FUNCTION:
    case G_IR_NODE_CALLBACK:
      {
	GIrNodeFunction *function = (GIrNodeFunction *)node;

	g_free (node->name);
	g_free (function->symbol);
        g_free (function->property);
	_g_ir_node_free ((GIrNode *)function->result);
	for (l = function->parameters; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (function->parameters);
      }
      break;

    case G_IR_NODE_TYPE:
      {
	GIrNodeType *type = (GIrNodeType *)node;

	g_free (node->name);
	_g_ir_node_free ((GIrNode *)type->parameter_type1);
	_g_ir_node_free ((GIrNode *)type->parameter_type2);

	g_free (type->giinterface);
	g_strfreev (type->errors);

      }
      break;

    case G_IR_NODE_PARAM:
      {
	GIrNodeParam *param = (GIrNodeParam *)node;

	g_free (node->name);
	_g_ir_node_free ((GIrNode *)param->type);
      }
      break;

    case G_IR_NODE_PROPERTY:
      {
	GIrNodeProperty *property = (GIrNodeProperty *)node;

	g_free (node->name);
        g_free (property->setter);
        g_free (property->getter);
	_g_ir_node_free ((GIrNode *)property->type);
      }
      break;

    case G_IR_NODE_SIGNAL:
      {
	GIrNodeSignal *signal = (GIrNodeSignal *)node;

	g_free (node->name);
	for (l = signal->parameters; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (signal->parameters);
	_g_ir_node_free ((GIrNode *)signal->result);
      }
      break;

    case G_IR_NODE_VFUNC:
      {
	GIrNodeVFunc *vfunc = (GIrNodeVFunc *)node;

	g_free (node->name);
	g_free (vfunc->invoker);
	for (l = vfunc->parameters; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (vfunc->parameters);
	_g_ir_node_free ((GIrNode *)vfunc->result);
      }
      break;

    case G_IR_NODE_FIELD:
      {
	GIrNodeField *field = (GIrNodeField *)node;

	g_free (node->name);
	_g_ir_node_free ((GIrNode *)field->type);
	_g_ir_node_free ((GIrNode *)field->callback);
      }
      break;

    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;

	g_free (node->name);
	g_free (iface->gtype_name);
	g_free (iface->gtype_init);
	g_free (iface->ref_func);
	g_free (iface->unref_func);
	g_free (iface->set_value_func);
	g_free (iface->get_value_func);


	g_free (iface->glib_type_struct);
	g_free (iface->parent);

	for (l = iface->interfaces; l; l = l->next)
	  g_free ((GIrNode *)l->data);
	g_list_free (iface->interfaces);

	for (l = iface->members; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (iface->members);

      }
      break;

    case G_IR_NODE_VALUE:
      {
	g_free (node->name);
      }
      break;

    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	GIrNodeEnum *enum_ = (GIrNodeEnum *)node;

	g_free (node->name);
	g_free (enum_->gtype_name);
	g_free (enum_->gtype_init);
	g_free (enum_->error_domain);

	for (l = enum_->values; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (enum_->values);

	for (l = enum_->methods; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (enum_->methods);
      }
      break;

    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;

	g_free (node->name);
	g_free (boxed->gtype_name);
	g_free (boxed->gtype_init);

	for (l = boxed->members; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (boxed->members);
      }
      break;

    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;

	g_free (node->name);
	g_free (struct_->gtype_name);
	g_free (struct_->gtype_init);
        g_free (struct_->copy_func);
        g_free (struct_->free_func);

	for (l = struct_->members; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	g_list_free (struct_->members);
      }
      break;

    case G_IR_NODE_CONSTANT:
      {
	GIrNodeConstant *constant = (GIrNodeConstant *)node;

	g_free (node->name);
	g_free (constant->value);
	_g_ir_node_free ((GIrNode *)constant->type);
      }
      break;

    case G_IR_NODE_XREF:
      {
	GIrNodeXRef *xref = (GIrNodeXRef *)node;

	g_free (node->name);
	g_free (xref->namespace);
      }
      break;

    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;

	g_free (node->name);
	g_free (union_->gtype_name);
	g_free (union_->gtype_init);
        g_free (union_->copy_func);
        g_free (union_->free_func);

	_g_ir_node_free ((GIrNode *)union_->discriminator_type);
	for (l = union_->members; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
	for (l = union_->discriminators; l; l = l->next)
	  _g_ir_node_free ((GIrNode *)l->data);
      }
      break;

    default:
      g_error ("Unhandled node type %d\n", node->type);
      break;
    }

  g_hash_table_destroy (node->attributes);

  g_free (node);
}

/* returns the fixed size of the blob */
guint32
_g_ir_node_get_size (GIrNode *node)
{
  GList *l;
  gint size, n;

  switch (node->type)
    {
    case G_IR_NODE_CALLBACK:
      size = sizeof (CallbackBlob);
      break;

    case G_IR_NODE_FUNCTION:
      size = sizeof (FunctionBlob);
      break;

    case G_IR_NODE_PARAM:
      /* See the comment in the G_IR_NODE_PARAM/ArgBlob writing below */
      size = sizeof (ArgBlob) - sizeof (SimpleTypeBlob);
      break;

    case G_IR_NODE_TYPE:
      size = sizeof (SimpleTypeBlob);
      break;

    case G_IR_NODE_OBJECT:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;

	n = g_list_length (iface->interfaces);
	size = sizeof (ObjectBlob) + 2 * (n + (n % 2));

	for (l = iface->members; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;

	n = g_list_length (iface->prerequisites);
	size = sizeof (InterfaceBlob) + 2 * (n + (n % 2));

	for (l = iface->members; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	GIrNodeEnum *enum_ = (GIrNodeEnum *)node;

	size = sizeof (EnumBlob);
	for (l = enum_->values; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
	for (l = enum_->methods; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_VALUE:
      size = sizeof (ValueBlob);
      break;

    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;

	size = sizeof (StructBlob);
	for (l = struct_->members; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;

	size = sizeof (StructBlob);
	for (l = boxed->members; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_PROPERTY:
      size = sizeof (PropertyBlob);
      break;

    case G_IR_NODE_SIGNAL:
      size = sizeof (SignalBlob);
      break;

    case G_IR_NODE_VFUNC:
      size = sizeof (VFuncBlob);
      break;

    case G_IR_NODE_FIELD:
      {
	GIrNodeField *field = (GIrNodeField *)node;

        size = sizeof (FieldBlob);
        if (field->callback)
          size += _g_ir_node_get_size ((GIrNode *)field->callback);
      }
      break;

    case G_IR_NODE_CONSTANT:
      size = sizeof (ConstantBlob);
      break;

    case G_IR_NODE_XREF:
      size = 0;
      break;

    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;

	size = sizeof (UnionBlob);
	for (l = union_->members; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
	for (l = union_->discriminators; l; l = l->next)
	  size += _g_ir_node_get_size ((GIrNode *)l->data);
      }
      break;

    default:
      g_error ("Unhandled node type '%s'\n",
	       _g_ir_node_type_to_string (node->type));
      size = 0;
    }

  g_debug ("node %p type '%s' size %d", node,
	   _g_ir_node_type_to_string (node->type), size);

  return size;
}

static void
add_attribute_size (gpointer key, gpointer value, gpointer data)
{
  const gchar *key_str = key;
  const gchar *value_str = value;
  gint *size_p = data;

  *size_p += sizeof (AttributeBlob);
  *size_p += ALIGN_VALUE (strlen (key_str) + 1, 4);
  *size_p += ALIGN_VALUE (strlen (value_str) + 1, 4);
}

/* returns the full size of the blob including variable-size parts (including attributes) */
static guint32
_g_ir_node_get_full_size_internal (GIrNode *parent,
				  GIrNode *node)
{
  GList *l;
  gint size, n;

  if (node == NULL && parent != NULL)
    g_error ("Caught NULL node, parent=%s", parent->name);

  g_debug ("node %p type '%s'", node,
	   _g_ir_node_type_to_string (node->type));

  switch (node->type)
    {
    case G_IR_NODE_CALLBACK:
      {
	GIrNodeFunction *function = (GIrNodeFunction *)node;
	size = sizeof (CallbackBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	for (l = function->parameters; l; l = l->next)
	  {
	    size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
	  }
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)function->result);
      }
      break;

    case G_IR_NODE_FUNCTION:
      {
	GIrNodeFunction *function = (GIrNodeFunction *)node;
	size = sizeof (FunctionBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	size += ALIGN_VALUE (strlen (function->symbol) + 1, 4);
	for (l = function->parameters; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)function->result);
      }
      break;

    case G_IR_NODE_PARAM:
      {
	GIrNodeParam *param = (GIrNodeParam *)node;

	/* See the comment in the G_IR_NODE_PARAM/ArgBlob writing below */
	size = sizeof (ArgBlob) - sizeof (SimpleTypeBlob);
	if (node->name)
	  size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)param->type);
      }
      break;

    case G_IR_NODE_TYPE:
      {
	GIrNodeType *type = (GIrNodeType *)node;
        size = sizeof (SimpleTypeBlob);
        if (!GI_TYPE_TAG_IS_BASIC (type->tag))
	  {
	    g_debug ("node %p type tag '%s'", node,
		     g_type_tag_to_string (type->tag));

	    switch (type->tag)
	      {
	      case GI_TYPE_TAG_ARRAY:
		size = sizeof (ArrayTypeBlob);
		if (type->parameter_type1)
		  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)type->parameter_type1);
		break;
	      case GI_TYPE_TAG_INTERFACE:
		size += sizeof (InterfaceTypeBlob);
		break;
	      case GI_TYPE_TAG_GLIST:
	      case GI_TYPE_TAG_GSLIST:
		size += sizeof (ParamTypeBlob);
		if (type->parameter_type1)
		  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)type->parameter_type1);
		break;
	      case GI_TYPE_TAG_GHASH:
		size += sizeof (ParamTypeBlob) * 2;
		if (type->parameter_type1)
		  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)type->parameter_type1);
		if (type->parameter_type2)
		  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)type->parameter_type2);
		break;
	      case GI_TYPE_TAG_ERROR:
		size += sizeof (ErrorTypeBlob);
		break;
	      default:
		g_error ("Unknown type tag %d\n", type->tag);
		break;
	      }
	  }
      }
      break;

    case G_IR_NODE_OBJECT:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;

	n = g_list_length (iface->interfaces);
	size = sizeof(ObjectBlob);
	if (iface->parent)
	  size += ALIGN_VALUE (strlen (iface->parent) + 1, 4);
        if (iface->glib_type_struct)
          size += ALIGN_VALUE (strlen (iface->glib_type_struct) + 1, 4);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	size += ALIGN_VALUE (strlen (iface->gtype_name) + 1, 4);
	if (iface->gtype_init)
	  size += ALIGN_VALUE (strlen (iface->gtype_init) + 1, 4);
	if (iface->ref_func)
	  size += ALIGN_VALUE (strlen (iface->ref_func) + 1, 4);
	if (iface->unref_func)
	  size += ALIGN_VALUE (strlen (iface->unref_func) + 1, 4);
	if (iface->set_value_func)
	  size += ALIGN_VALUE (strlen (iface->set_value_func) + 1, 4);
	if (iface->get_value_func)
	  size += ALIGN_VALUE (strlen (iface->get_value_func) + 1, 4);
	size += 2 * (n + (n % 2));

	for (l = iface->members; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;

	n = g_list_length (iface->prerequisites);
	size = sizeof (InterfaceBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	size += ALIGN_VALUE (strlen (iface->gtype_name) + 1, 4);
	size += ALIGN_VALUE (strlen (iface->gtype_init) + 1, 4);
	size += 2 * (n + (n % 2));

	for (l = iface->members; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	GIrNodeEnum *enum_ = (GIrNodeEnum *)node;

	size = sizeof (EnumBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	if (enum_->gtype_name)
	  {
	    size += ALIGN_VALUE (strlen (enum_->gtype_name) + 1, 4);
	    size += ALIGN_VALUE (strlen (enum_->gtype_init) + 1, 4);
	  }
	if (enum_->error_domain)
	  size += ALIGN_VALUE (strlen (enum_->error_domain) + 1, 4);

	for (l = enum_->values; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
	for (l = enum_->methods; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_VALUE:
      {
	size = sizeof (ValueBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
      }
      break;

    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;

	size = sizeof (StructBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	if (struct_->gtype_name)
	  size += ALIGN_VALUE (strlen (struct_->gtype_name) + 1, 4);
	if (struct_->gtype_init)
	  size += ALIGN_VALUE (strlen (struct_->gtype_init) + 1, 4);
	if (struct_->copy_func)
	  size += ALIGN_VALUE (strlen (struct_->copy_func) + 1, 4);
	if (struct_->free_func)
	  size += ALIGN_VALUE (strlen (struct_->free_func) + 1, 4);
	for (l = struct_->members; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;

	size = sizeof (StructBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	if (boxed->gtype_name)
	  {
	    size += ALIGN_VALUE (strlen (boxed->gtype_name) + 1, 4);
	    size += ALIGN_VALUE (strlen (boxed->gtype_init) + 1, 4);
	  }
	for (l = boxed->members; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
      }
      break;

    case G_IR_NODE_PROPERTY:
      {
	GIrNodeProperty *prop = (GIrNodeProperty *)node;

	size = sizeof (PropertyBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)prop->type);
      }
      break;

    case G_IR_NODE_SIGNAL:
      {
	GIrNodeSignal *signal = (GIrNodeSignal *)node;

	size = sizeof (SignalBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	for (l = signal->parameters; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)signal->result);
      }
      break;

    case G_IR_NODE_VFUNC:
      {
	GIrNodeVFunc *vfunc = (GIrNodeVFunc *)node;

	size = sizeof (VFuncBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	for (l = vfunc->parameters; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)vfunc->result);
      }
      break;

    case G_IR_NODE_FIELD:
      {
	GIrNodeField *field = (GIrNodeField *)node;

	size = sizeof (FieldBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	if (field->callback)
          size += _g_ir_node_get_full_size_internal (node, (GIrNode *)field->callback);
	else
          size += _g_ir_node_get_full_size_internal (node, (GIrNode *)field->type);
      }
      break;

    case G_IR_NODE_CONSTANT:
      {
	GIrNodeConstant *constant = (GIrNodeConstant *)node;

	size = sizeof (ConstantBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	/* FIXME non-string values */
	size += ALIGN_VALUE (strlen (constant->value) + 1, 4);
	size += _g_ir_node_get_full_size_internal (node, (GIrNode *)constant->type);
      }
      break;

    case G_IR_NODE_XREF:
      {
	GIrNodeXRef *xref = (GIrNodeXRef *)node;

	size = 0;
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	size += ALIGN_VALUE (strlen (xref->namespace) + 1, 4);
      }
      break;

    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;

	size = sizeof (UnionBlob);
	size += ALIGN_VALUE (strlen (node->name) + 1, 4);
	if (union_->gtype_name)
	  size += ALIGN_VALUE (strlen (union_->gtype_name) + 1, 4);
	if (union_->gtype_init)
	  size += ALIGN_VALUE (strlen (union_->gtype_init) + 1, 4);
	if (union_->copy_func)
	  size += ALIGN_VALUE (strlen (union_->copy_func) + 1, 4);
	if (union_->free_func)
	  size += ALIGN_VALUE (strlen (union_->free_func) + 1, 4);
	for (l = union_->members; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
	for (l = union_->discriminators; l; l = l->next)
	  size += _g_ir_node_get_full_size_internal (node, (GIrNode *)l->data);
      }
      break;

    default:
      g_error ("Unknown type tag %d\n", node->type);
      size = 0;
    }

  g_debug ("node %s%s%s%p type '%s' full size %d",
	   node->name ? "'" : "",
	   node->name ? node->name : "",
	   node->name ? "' " : "",
	   node, _g_ir_node_type_to_string (node->type), size);

  g_hash_table_foreach (node->attributes, add_attribute_size, &size);

  return size;
}

guint32
_g_ir_node_get_full_size (GIrNode *node)
{
  return _g_ir_node_get_full_size_internal (NULL, node);
}

int
_g_ir_node_cmp (GIrNode *node,
		GIrNode *other)
{
  if (node->type < other->type)
    return -1;
  else if (node->type > other->type)
    return 1;
  else
    return strcmp (node->name, other->name);
}

gboolean
_g_ir_node_can_have_member (GIrNode    *node)
{
  switch (node->type)
    {
    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
    case G_IR_NODE_BOXED:
    case G_IR_NODE_STRUCT:
    case G_IR_NODE_UNION:
      return TRUE;
    /* list others individually rather than with default: so that compiler
     * warns if new node types are added without adding them to the switch
     */
    case G_IR_NODE_INVALID:
    case G_IR_NODE_FUNCTION:
    case G_IR_NODE_CALLBACK:
    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
    case G_IR_NODE_CONSTANT:
    case G_IR_NODE_INVALID_0:
    case G_IR_NODE_PARAM:
    case G_IR_NODE_TYPE:
    case G_IR_NODE_PROPERTY:
    case G_IR_NODE_SIGNAL:
    case G_IR_NODE_VALUE:
    case G_IR_NODE_VFUNC:
    case G_IR_NODE_FIELD:
    case G_IR_NODE_XREF:
      return FALSE;
    default:
      g_assert_not_reached ();
    };
  return FALSE;
}

void
_g_ir_node_add_member (GIrNode         *node,
		      GIrNodeFunction *member)
{
  g_return_if_fail (node != NULL);
  g_return_if_fail (member != NULL);

  switch (node->type)
    {
    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;
	iface->members =
	  g_list_insert_sorted (iface->members, member,
				(GCompareFunc) _g_ir_node_cmp);
	break;
      }
    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;
	boxed->members =
	  g_list_insert_sorted (boxed->members, member,
				(GCompareFunc) _g_ir_node_cmp);
	break;
      }
    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;
	struct_->members =
	  g_list_insert_sorted (struct_->members, member,
				(GCompareFunc) _g_ir_node_cmp);
	break;
      }
    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;
	union_->members =
	  g_list_insert_sorted (union_->members, member,
				(GCompareFunc) _g_ir_node_cmp);
	break;
      }
    default:
      g_error ("Cannot add a member to unknown type tag type %d\n",
	       node->type);
      break;
    }
}

const gchar *
_g_ir_node_param_direction_string (GIrNodeParam * node)
{
  if (node->out)
    {
      if (node->in)
	return "in-out";
      else
	return "out";
    }
  return "in";
}

static gint64
parse_int_value (const gchar *str)
{
  return g_ascii_strtoll (str, NULL, 0);
}

static guint64
parse_uint_value (const gchar *str)
{
  return g_ascii_strtoull (str, NULL, 0);
}

static gdouble
parse_float_value (const gchar *str)
{
  return g_ascii_strtod (str, NULL);
}

static gboolean
parse_boolean_value (const gchar *str)
{
  if (g_ascii_strcasecmp (str, "TRUE") == 0)
    return TRUE;

  if (g_ascii_strcasecmp (str, "FALSE") == 0)
    return FALSE;

  return parse_int_value (str) ? TRUE : FALSE;
}

static GIrNode *
find_entry_node (GIrTypelibBuild   *build,
		 const gchar *name,
		 guint16     *idx)

{
  GIrModule *module = build->module;
  GList *l;
  gint i;
  gchar **names;
  gint n_names;
  GIrNode *result = NULL;

  g_assert (name != NULL);
  g_assert (strlen (name) > 0);

  names = g_strsplit (name, ".", 0);
  n_names = g_strv_length (names);
  if (n_names > 2)
    g_error ("Too many name parts");

  for (l = module->entries, i = 1; l; l = l->next, i++)
    {
      GIrNode *node = (GIrNode *)l->data;

      if (n_names > 1)
	{
	  if (node->type != G_IR_NODE_XREF)
	    continue;

	  if (((GIrNodeXRef *)node)->namespace == NULL ||
	      strcmp (((GIrNodeXRef *)node)->namespace, names[0]) != 0)
	    continue;
	}

      if (strcmp (node->name, names[n_names - 1]) == 0)
	{
	  if (idx)
	    *idx = i;

	  result = node;
	  goto out;
	}
    }

  if (n_names > 1)
    {
      GIrNode *node = _g_ir_node_new (G_IR_NODE_XREF, module);

      ((GIrNodeXRef *)node)->namespace = g_strdup (names[0]);
      node->name = g_strdup (names[1]);

      module->entries = g_list_append (module->entries, node);

      if (idx)
	*idx = g_list_length (module->entries);

      result = node;

      g_debug ("Creating XREF: %s %s", names[0], names[1]);

      goto out;
    }

  
  _g_ir_module_fatal (build, -1, "type reference '%s' not found",
		      name);
 out:

  g_strfreev (names);

  return result;
}

static guint16
find_entry (GIrTypelibBuild   *build,
	    const gchar *name)
{
  guint16 idx = 0;

  find_entry_node (build, name, &idx);

  return idx;
}

static GIrModule *
find_namespace (GIrModule  *module,
		const char *name)
{
  GIrModule *target;
  GList *l;
  
  if (strcmp (module->name, name) == 0)
    return module;

  for (l = module->include_modules; l; l = l->next)
    {
      GIrModule *submodule = l->data;

     if (strcmp (submodule->name, name) == 0)
       return submodule;

      target = find_namespace (submodule, name);
      if (target)
       return target;
    }
  return NULL;
}

GIrNode *
_g_ir_find_node (GIrTypelibBuild  *build,
		GIrModule        *src_module,
		const char       *name)
{
  GList *l;
  GIrNode *return_node = NULL;
  char **names = g_strsplit (name, ".", 0);
  gint n_names = g_strv_length (names);
  const char *target_name;
  GIrModule *target_module;

  if (n_names == 1)
    {
      target_module = src_module;
      target_name = name;
    }
  else
    {
      target_module = find_namespace (build->module, names[0]);
      target_name = names[1];
    }

  /* find_namespace() may return NULL. */
  if (target_module == NULL)
      goto done;

  for (l = target_module->entries; l; l = l->next)
    {
      GIrNode *node = (GIrNode *)l->data;

      if (strcmp (node->name, target_name) == 0)
	{
	  return_node = node;
	  break;
	}
    }

done:
  g_strfreev (names);

  return return_node;
}

static int
get_index_of_member_type (GIrNodeInterface *node,
                          GIrNodeTypeId type,
                          const char *name)
{
  guint index = -1;
  GList *l;

  for (l = node->members; l; l = l->next)
    {
      GIrNode *member_node = l->data;

      if (member_node->type != type)
        continue;

      index++;

      if (strcmp (member_node->name, name) == 0)
        break;
    }

  return index;
}

static void
serialize_type (GIrTypelibBuild    *build,
		GIrNodeType  *node,
		GString      *str)
{
  gint i;

  if (GI_TYPE_TAG_IS_BASIC (node->tag))
    {
      g_string_append_printf (str, "%s%s", g_type_tag_to_string (node->tag),
			      node->is_pointer ? "*" : "");
    }
  else if (node->tag == GI_TYPE_TAG_ARRAY)
    {
      if (node->array_type == GI_ARRAY_TYPE_C)
	{
	  serialize_type (build, node->parameter_type1, str);
	  g_string_append (str, "[");

	  if (node->has_length)
	    g_string_append_printf (str, "length=%d", node->length);
	  else if (node->has_size)
	    g_string_append_printf (str, "fixed-size=%d", node->size);

	  if (node->zero_terminated)
	    g_string_append_printf (str, "%szero-terminated=1",
				    node->has_length ? "," : "");

	  g_string_append (str, "]");
	  if (node->is_pointer)
	    g_string_append (str, "*");
	}
      else if (node->array_type == GI_ARRAY_TYPE_BYTE_ARRAY)
	{
	  /* We on purpose skip serializing parameter_type1, which should
	     always be void*
	  */
	  g_string_append (str, "GByteArray");
	}
      else
	{
	  if (node->array_type == GI_ARRAY_TYPE_ARRAY)
	    g_string_append (str, "GArray");
	  else
	    g_string_append (str, "GPtrArray");
	  if (node->parameter_type1)
	    {
	      g_string_append (str, "<");
	      serialize_type (build, node->parameter_type1, str);
	      g_string_append (str, ">");
	    }
	}
    }
  else if (node->tag == GI_TYPE_TAG_INTERFACE)
    {
      GIrNode *iface;
      gchar *name;

      iface = find_entry_node (build, node->giinterface, NULL);
      if (iface)
        {
          if (iface->type == G_IR_NODE_XREF)
            g_string_append_printf (str, "%s.", ((GIrNodeXRef *)iface)->namespace);
          name = iface->name;
        }
      else
	{
	  g_warning ("Interface for type reference %s not found", node->giinterface);
	  name = node->giinterface;
	}

      g_string_append_printf (str, "%s%s", name,
			      node->is_pointer ? "*" : "");
    }
  else if (node->tag == GI_TYPE_TAG_GLIST)
    {
      g_string_append (str, "GList");
      if (node->parameter_type1)
	{
	  g_string_append (str, "<");
	  serialize_type (build, node->parameter_type1, str);
	  g_string_append (str, ">");
	}
    }
  else if (node->tag == GI_TYPE_TAG_GSLIST)
    {
      g_string_append (str, "GSList");
      if (node->parameter_type1)
	{
	  g_string_append (str, "<");
	  serialize_type (build, node->parameter_type1, str);
	  g_string_append (str, ">");
	}
    }
  else if (node->tag == GI_TYPE_TAG_GHASH)
    {
      g_string_append (str, "GHashTable");
      if (node->parameter_type1)
	{
	  g_string_append (str, "<");
	  serialize_type (build, node->parameter_type1, str);
	  g_string_append (str, ",");
	  serialize_type (build, node->parameter_type2, str);
	  g_string_append (str, ">");
	}
    }
  else if (node->tag == GI_TYPE_TAG_ERROR)
    {
      g_string_append (str, "GError");
      if (node->errors)
	{
	  g_string_append (str, "<");
	  for (i = 0; node->errors[i]; i++)
	    {
	      if (i > 0)
		g_string_append (str, ",");
	      g_string_append (str, node->errors[i]);
	    }
	  g_string_append (str, ">");
	}
    }
}

static void
_g_ir_node_build_members (GList         **members,
			 GIrNodeTypeId   type,
			 guint16        *count,
			 GIrNode        *parent,
                         GIrTypelibBuild *build,
			 guint32        *offset,
			 guint32        *offset2,
			 guint16        *count2)
{
  GList *l = *members;

  while (l)
    {
      GIrNode *member = (GIrNode *)l->data;
      GList *next = l->next;

      if (member->type == type)
	{
	  (*count)++;
	  _g_ir_node_build_typelib (member, parent, build, offset, offset2, count2);
	  *members = g_list_delete_link (*members, l);
	}
      l = next;
    }
}

static void
_g_ir_node_check_unhandled_members (GList         **members,
				    GIrNodeTypeId   container_type)
{
#if 0
  if (*members)
    {
      GList *l;

      for (l = *members; l; l = l->next)
	{
	  GIrNode *member = (GIrNode *)l->data;
	  g_printerr ("Unhandled '%s' member '%s' type '%s'\n",
		      _g_ir_node_type_to_string (container_type),
		      member->name,
		      _g_ir_node_type_to_string (member->type));
	}

      g_list_free (*members);
      *members = NULL;

      g_error ("Unhandled members. Aborting.");
    }
#else
  g_list_free (*members);
  *members = NULL;
#endif
}

void
_g_ir_node_build_typelib (GIrNode         *node,
			  GIrNode         *parent,
			  GIrTypelibBuild *build,
			  guint32         *offset,
			  guint32         *offset2,
			  guint16         *count2)
{
  gboolean appended_stack;
  GHashTable *strings = build->strings;
  GHashTable *types = build->types;
  guchar *data = build->data;
  GList *l;
  guint32 old_offset = *offset;
  guint32 old_offset2 = *offset2;

  g_assert (node != NULL);

  g_debug ("build_typelib: %s%s(%s)",
	   node->name ? node->name : "",
	   node->name ? " " : "",
	   _g_ir_node_type_to_string (node->type));

  if (build->stack)
    appended_stack = node != (GIrNode*)build->stack->data; 
  else
    appended_stack = TRUE;
  if (appended_stack)
    build->stack = g_list_prepend (build->stack, node);
  
  _g_ir_node_compute_offsets (build, node);

  /* We should only be building each node once.  If we do a typelib expansion, we also
   * reset the offset in girmodule.c.
   */
  g_assert (node->offset == 0);
  node->offset = *offset;
  build->nodes_with_attributes = g_list_prepend (build->nodes_with_attributes, node);

  build->n_attributes += g_hash_table_size (node->attributes);

  switch (node->type)
    {
    case G_IR_NODE_TYPE:
      {
	GIrNodeType *type = (GIrNodeType *)node;
	SimpleTypeBlob *blob = (SimpleTypeBlob *)&data[*offset];

	*offset += sizeof (SimpleTypeBlob);

	if (GI_TYPE_TAG_IS_BASIC (type->tag))
	  {
	    blob->flags.reserved = 0;
	    blob->flags.reserved2 = 0;
	    blob->flags.pointer = type->is_pointer;
	    blob->flags.reserved3 = 0;
	    blob->flags.tag = type->tag;
	  }
	else
	  {
	    GString *str;
	    gchar *s;
	    gpointer value;

	    str = g_string_new (0);
	    serialize_type (build, type, str);
	    s = g_string_free (str, FALSE);

	    types_count += 1;
	    value = g_hash_table_lookup (types, s);
	    if (value)
	      {
		blob->offset = GPOINTER_TO_UINT (value);
		g_free (s);
	      }
	    else
	      {
		unique_types_count += 1;
		g_hash_table_insert (types, s, GUINT_TO_POINTER(*offset2));

		blob->offset = *offset2;
		switch (type->tag)
		  {
		  case GI_TYPE_TAG_ARRAY:
		    {
		      ArrayTypeBlob *array = (ArrayTypeBlob *)&data[*offset2];
		      guint32 pos;

		      array->pointer = type->is_pointer;
		      array->reserved = 0;
		      array->tag = type->tag;
		      array->zero_terminated = type->zero_terminated;
		      array->has_length = type->has_length;
                      array->has_size = type->has_size;
                      array->array_type = type->array_type;
		      array->reserved2 = 0;
                      if (array->has_length)
                        array->dimensions.length = type->length;
                      else if (array->has_size)
                        array->dimensions.size  = type->size;
                      else
                        array->dimensions.length = -1;

		      pos = *offset2 + G_STRUCT_OFFSET (ArrayTypeBlob, type);
		      *offset2 += sizeof (ArrayTypeBlob);

		      _g_ir_node_build_typelib ((GIrNode *)type->parameter_type1,
		                               node, build, &pos, offset2, NULL);
		    }
		    break;

		  case GI_TYPE_TAG_INTERFACE:
		    {
		      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&data[*offset2];
		      *offset2 += sizeof (InterfaceTypeBlob);

		      iface->pointer = type->is_pointer;
		      iface->reserved = 0;
		      iface->tag = type->tag;
		      iface->reserved2 = 0;
		      iface->interface = find_entry (build, type->giinterface);

		    }
		    break;

		  case GI_TYPE_TAG_GLIST:
		  case GI_TYPE_TAG_GSLIST:
		    {
		      ParamTypeBlob *param = (ParamTypeBlob *)&data[*offset2];
		      guint32 pos;

		      param->pointer = 1;
		      param->reserved = 0;
		      param->tag = type->tag;
		      param->reserved2 = 0;
		      param->n_types = 1;

		      pos = *offset2 + G_STRUCT_OFFSET (ParamTypeBlob, type);
		      *offset2 += sizeof (ParamTypeBlob) + sizeof (SimpleTypeBlob);

		      _g_ir_node_build_typelib ((GIrNode *)type->parameter_type1,
					       node, build, &pos, offset2, NULL);
		    }
		    break;

		  case GI_TYPE_TAG_GHASH:
		    {
		      ParamTypeBlob *param = (ParamTypeBlob *)&data[*offset2];
		      guint32 pos;

		      param->pointer = 1;
		      param->reserved = 0;
		      param->tag = type->tag;
		      param->reserved2 = 0;
		      param->n_types = 2;

		      pos = *offset2 + G_STRUCT_OFFSET (ParamTypeBlob, type);
		      *offset2 += sizeof (ParamTypeBlob) + sizeof (SimpleTypeBlob)*2;

		      _g_ir_node_build_typelib ((GIrNode *)type->parameter_type1,
					       node, build, &pos, offset2, NULL);
		      _g_ir_node_build_typelib ((GIrNode *)type->parameter_type2,
					       node, build, &pos, offset2, NULL);
		    }
		    break;

		  case GI_TYPE_TAG_ERROR:
		    {
		      ErrorTypeBlob *error_blob = (ErrorTypeBlob *)&data[*offset2];

		      error_blob->pointer = 1;
		      error_blob->reserved = 0;
		      error_blob->tag = type->tag;
		      error_blob->reserved2 = 0;
		      error_blob->n_domains = 0;

		      *offset2 += sizeof (ErrorTypeBlob);
		    }
		    break;

		  default:
		    g_error ("Unknown type tag %d\n", type->tag);
		    break;
		  }
	      }
	  }
      }
      break;

    case G_IR_NODE_FIELD:
      {
	GIrNodeField *field = (GIrNodeField *)node;
	FieldBlob *blob;

	blob = (FieldBlob *)&data[*offset];

	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->readable = field->readable;
	blob->writable = field->writable;
	blob->reserved = 0;
	blob->bits = 0;
	if (field->offset >= 0)
	  blob->struct_offset = field->offset;
	else
	  blob->struct_offset = 0xFFFF; /* mark as unknown */

        if (field->callback)
          {
            blob->has_embedded_type = TRUE;
            blob->type.offset = GI_INFO_TYPE_CALLBACK;
	    *offset += sizeof (FieldBlob);
            _g_ir_node_build_typelib ((GIrNode *)field->callback,
			             node, build, offset, offset2, NULL);
            /* Fields with callbacks are bigger than normal, update count2
             * as an extra hint which represents the number of fields which are
             * callbacks. This allows us to gain constant time performance in the
             * repository for skipping over the fields section.
             */
            if (count2)
              (*count2)++;
          }
        else
          {
            blob->has_embedded_type = FALSE;
            /* We handle the size member specially below, so subtract it */
	    *offset += sizeof (FieldBlob) - sizeof (SimpleTypeBlob);
            _g_ir_node_build_typelib ((GIrNode *)field->type,
				     node, build, offset, offset2, NULL);
          }
      }
      break;

    case G_IR_NODE_PROPERTY:
      {
	GIrNodeProperty *prop = (GIrNodeProperty *)node;
	PropertyBlob *blob = (PropertyBlob *)&data[*offset];
        /* We handle the size member specially below, so subtract it */
	*offset += sizeof (PropertyBlob) - sizeof (SimpleTypeBlob);

	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->deprecated = prop->deprecated;
	blob->readable = prop->readable;
	blob->writable = prop->writable;
	blob->construct = prop->construct;
	blob->construct_only = prop->construct_only;
	blob->transfer_ownership = prop->transfer;
	blob->transfer_container_ownership = prop->shallow_transfer;
	blob->reserved = 0;

        if (prop->setter != NULL)
          {
            int index = get_index_of_member_type ((GIrNodeInterface*)parent,
                                                  G_IR_NODE_FUNCTION,
                                                  prop->setter);
            if (index == -1)
              {
                g_error ("Unknown setter %s for property %s:%s", prop->setter, parent->name, node->name);
              }

            blob->setter = (guint) index;
          }
        else
          blob->setter = ACCESSOR_SENTINEL;

        if (prop->getter != NULL)
          {
            int index = get_index_of_member_type ((GIrNodeInterface*)parent,
                                                  G_IR_NODE_FUNCTION,
                                                  prop->getter);
            if (index == -1)
              {
                g_error ("Unknown getter %s for property %s:%s", prop->getter, parent->name, node->name);
              }

            blob->getter = (guint) index;
          }
        else
          blob->getter = ACCESSOR_SENTINEL;

        _g_ir_node_build_typelib ((GIrNode *)prop->type,
				 node, build, offset, offset2, NULL);
      }
      break;

    case G_IR_NODE_FUNCTION:
      {
	FunctionBlob *blob = (FunctionBlob *)&data[*offset];
	SignatureBlob *blob2 = (SignatureBlob *)&data[*offset2];
	GIrNodeFunction *function = (GIrNodeFunction *)node;
	guint32 signature;
	gint n;

	signature = *offset2;
	n = g_list_length (function->parameters);

	*offset += sizeof (FunctionBlob);
	*offset2 += sizeof (SignatureBlob) + n * sizeof (ArgBlob);

	blob->blob_type = BLOB_TYPE_FUNCTION;
	blob->deprecated = function->deprecated;
        blob->is_static = !function->is_method;
	blob->setter = FALSE;
	blob->getter = FALSE;
	blob->constructor = function->is_constructor;
	blob->wraps_vfunc = function->wraps_vfunc;
	blob->throws = function->throws; /* Deprecated. Also stored in SignatureBlob. */
	blob->index = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->symbol = _g_ir_write_string (function->symbol, strings, data, offset2);
	blob->signature = signature;

        if (function->is_setter || function->is_getter)
          {
            int index = get_index_of_member_type ((GIrNodeInterface*)parent,
                                                  G_IR_NODE_PROPERTY,
                                                  function->property);
            if (index == -1)
              {
                g_error ("Unknown property %s:%s for accessor %s", parent->name, function->property, function->symbol);
              }

            blob->setter = function->is_setter;
            blob->getter = function->is_getter;
            blob->index = (guint) index;
          }

        /* function->result is special since it doesn't appear in the serialized format but
         * we do want the attributes for it to appear
         */
        build->nodes_with_attributes = g_list_prepend (build->nodes_with_attributes, function->result);
        build->n_attributes += g_hash_table_size (((GIrNode *) function->result)->attributes);
        g_assert (((GIrNode *) function->result)->offset == 0);
        ((GIrNode *) function->result)->offset = signature;

	g_debug ("building function '%s'", function->symbol);

        _g_ir_node_build_typelib ((GIrNode *)function->result->type,
				 node, build, &signature, offset2, NULL);

	blob2->may_return_null = function->result->nullable;
	blob2->caller_owns_return_value = function->result->transfer;
	blob2->caller_owns_return_container = function->result->shallow_transfer;
	blob2->skip_return = function->result->skip;
        blob2->instance_transfer_ownership = function->instance_transfer_full;
	blob2->reserved = 0;
	blob2->n_arguments = n;
	blob2->throws = function->throws;

	signature += 4;

	for (l = function->parameters; l; l = l->next)
	  {
	    GIrNode *param = (GIrNode *)l->data;

	    _g_ir_node_build_typelib (param, node, build, &signature, offset2, NULL);
	  }

      }
      break;

    case G_IR_NODE_CALLBACK:
      {
	CallbackBlob *blob = (CallbackBlob *)&data[*offset];
	SignatureBlob *blob2 = (SignatureBlob *)&data[*offset2];
	GIrNodeFunction *function = (GIrNodeFunction *)node;
	guint32 signature;
	gint n;

	signature = *offset2;
	n = g_list_length (function->parameters);

	*offset += sizeof (CallbackBlob);
	*offset2 += sizeof (SignatureBlob) + n * sizeof (ArgBlob);

	blob->blob_type = BLOB_TYPE_CALLBACK;
	blob->deprecated = function->deprecated;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->signature = signature;

        _g_ir_node_build_typelib ((GIrNode *)function->result->type,
				 node, build, &signature, offset2, NULL);

	blob2->may_return_null = function->result->nullable;
	blob2->caller_owns_return_value = function->result->transfer;
	blob2->caller_owns_return_container = function->result->shallow_transfer;
	blob2->reserved = 0;
	blob2->n_arguments = n;
	blob2->throws = function->throws;

	signature += 4;

	for (l = function->parameters; l; l = l->next)
	  {
	    GIrNode *param = (GIrNode *)l->data;

	    _g_ir_node_build_typelib (param, node, build, &signature, offset2, NULL);
	  }
      }
      break;

    case G_IR_NODE_SIGNAL:
      {
	SignalBlob *blob = (SignalBlob *)&data[*offset];
	SignatureBlob *blob2 = (SignatureBlob *)&data[*offset2];
	GIrNodeSignal *signal = (GIrNodeSignal *)node;
	guint32 signature;
	gint n;

	signature = *offset2;
	n = g_list_length (signal->parameters);

	*offset += sizeof (SignalBlob);
	*offset2 += sizeof (SignatureBlob) + n * sizeof (ArgBlob);

	blob->deprecated = signal->deprecated;
	blob->run_first = signal->run_first;
	blob->run_last = signal->run_last;
	blob->run_cleanup = signal->run_cleanup;
	blob->no_recurse = signal->no_recurse;
	blob->detailed = signal->detailed;
	blob->action = signal->action;
	blob->no_hooks = signal->no_hooks;
	blob->has_class_closure = 0; /* FIXME */
	blob->true_stops_emit = 0; /* FIXME */
	blob->reserved = 0;
	blob->class_closure = 0; /* FIXME */
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->signature = signature;

        /* signal->result is special since it doesn't appear in the serialized format but
         * we do want the attributes for it to appear
         */
        build->nodes_with_attributes = g_list_prepend (build->nodes_with_attributes, signal->result);
        build->n_attributes += g_hash_table_size (((GIrNode *) signal->result)->attributes);
        g_assert (((GIrNode *) signal->result)->offset == 0);
        ((GIrNode *) signal->result)->offset = signature;

        _g_ir_node_build_typelib ((GIrNode *)signal->result->type,
				 node, build, &signature, offset2, NULL);

	blob2->may_return_null = signal->result->nullable;
	blob2->caller_owns_return_value = signal->result->transfer;
	blob2->caller_owns_return_container = signal->result->shallow_transfer;
        blob2->instance_transfer_ownership = signal->instance_transfer_full;
	blob2->reserved = 0;
	blob2->n_arguments = n;

	signature += 4;

	for (l = signal->parameters; l; l = l->next)
	  {
	    GIrNode *param = (GIrNode *)l->data;

	    _g_ir_node_build_typelib (param, node, build, &signature, offset2, NULL);
	  }
      }
      break;

    case G_IR_NODE_VFUNC:
      {
	VFuncBlob *blob = (VFuncBlob *)&data[*offset];
	SignatureBlob *blob2 = (SignatureBlob *)&data[*offset2];
	GIrNodeVFunc *vfunc = (GIrNodeVFunc *)node;
	guint32 signature;
	gint n;

	signature = *offset2;
	n = g_list_length (vfunc->parameters);

	*offset += sizeof (VFuncBlob);
	*offset2 += sizeof (SignatureBlob) + n * sizeof (ArgBlob);

	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->must_chain_up = 0; /* FIXME */
	blob->must_be_implemented = 0; /* FIXME */
	blob->must_not_be_implemented = 0; /* FIXME */
	blob->class_closure = 0; /* FIXME */
	blob->throws = vfunc->throws; /* Deprecated. Also stored in SignatureBlob. */
	blob->reserved = 0;

	if (vfunc->invoker)
	  {
	    int index = get_index_of_member_type ((GIrNodeInterface*)parent, G_IR_NODE_FUNCTION, vfunc->invoker);
	    if (index == -1)
	      {
	        g_error ("Unknown member function %s for vfunc %s", vfunc->invoker, node->name);
	      }
            blob->invoker = (guint) index;
	  }
	else
	  blob->invoker = 0x3ff; /* max of 10 bits */

	blob->struct_offset = vfunc->offset;
	blob->reserved2 = 0;
	blob->signature = signature;

        _g_ir_node_build_typelib ((GIrNode *)vfunc->result->type,
				 node, build, &signature, offset2, NULL);

	blob2->may_return_null = vfunc->result->nullable;
	blob2->caller_owns_return_value = vfunc->result->transfer;
	blob2->caller_owns_return_container = vfunc->result->shallow_transfer;
        blob2->instance_transfer_ownership = vfunc->instance_transfer_full;
	blob2->reserved = 0;
	blob2->n_arguments = n;
	blob2->throws = vfunc->throws;

	signature += 4;

	for (l = vfunc->parameters; l; l = l->next)
	  {
	    GIrNode *param = (GIrNode *)l->data;

	    _g_ir_node_build_typelib (param, node, build, &signature, offset2, NULL);
	  }
      }
      break;

    case G_IR_NODE_PARAM:
      {
	ArgBlob *blob = (ArgBlob *)&data[*offset];
	GIrNodeParam *param = (GIrNodeParam *)node;

	/* The offset for this one is smaller than the struct because
	 * we recursively build the simple type inline here below.
	 */
	*offset += sizeof (ArgBlob) - sizeof (SimpleTypeBlob);

	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->in = param->in;
	blob->out = param->out;
	blob->caller_allocates = param->caller_allocates;
	blob->nullable = param->nullable;
	blob->skip = param->skip;
	blob->optional = param->optional;
	blob->transfer_ownership = param->transfer;
	blob->transfer_container_ownership = param->shallow_transfer;
	blob->return_value = param->retval;
        blob->scope = param->scope;
	blob->reserved = 0;
        blob->closure = param->closure;
        blob->destroy = param->destroy;

        _g_ir_node_build_typelib ((GIrNode *)param->type, node, build, offset, offset2, NULL);
      }
      break;

    case G_IR_NODE_STRUCT:
      {
	StructBlob *blob = (StructBlob *)&data[*offset];
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;
	GList *members;

	blob->blob_type = BLOB_TYPE_STRUCT;
        blob->foreign = struct_->foreign;
	blob->deprecated = struct_->deprecated;
	blob->is_gtype_struct = struct_->is_gtype_struct;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->alignment = struct_->alignment;
	blob->size = struct_->size;

	if (struct_->gtype_name)
	  {
	    blob->unregistered = FALSE;
	    blob->gtype_name = _g_ir_write_string (struct_->gtype_name, strings, data, offset2);
	    blob->gtype_init = _g_ir_write_string (struct_->gtype_init, strings, data, offset2);
	  }
	else
	  {
	    blob->unregistered = TRUE;
	    blob->gtype_name = 0;
	    blob->gtype_init = 0;
	  }

        if (struct_->copy_func)
          blob->copy_func = _g_ir_write_string (struct_->copy_func, strings, data, offset2);
        if (struct_->free_func)
          blob->free_func = _g_ir_write_string (struct_->free_func, strings, data, offset2);

	blob->n_fields = 0;
	blob->n_methods = 0;

	*offset += sizeof (StructBlob);

	members = g_list_copy (struct_->members);

	_g_ir_node_build_members (&members, G_IR_NODE_FIELD, &blob->n_fields,
	                         node, build, offset, offset2, NULL);

	_g_ir_node_build_members (&members, G_IR_NODE_FUNCTION, &blob->n_methods,
				 node, build, offset, offset2, NULL);

	_g_ir_node_check_unhandled_members (&members, node->type);

	g_assert (members == NULL);
      }
      break;

    case G_IR_NODE_BOXED:
      {
	StructBlob *blob = (StructBlob *)&data[*offset];
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;
	GList *members;

	blob->blob_type = BLOB_TYPE_BOXED;
	blob->deprecated = boxed->deprecated;
	blob->unregistered = FALSE;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->gtype_name = _g_ir_write_string (boxed->gtype_name, strings, data, offset2);
	blob->gtype_init = _g_ir_write_string (boxed->gtype_init, strings, data, offset2);
	blob->alignment = boxed->alignment;
	blob->size = boxed->size;

	blob->n_fields = 0;
	blob->n_methods = 0;

	*offset += sizeof (StructBlob);

	members = g_list_copy (boxed->members);

	_g_ir_node_build_members (&members, G_IR_NODE_FIELD, &blob->n_fields,
				  node, build, offset, offset2, NULL);

	_g_ir_node_build_members (&members, G_IR_NODE_FUNCTION, &blob->n_methods,
				  node, build, offset, offset2, NULL);

	_g_ir_node_check_unhandled_members (&members, node->type);

	g_assert (members == NULL);
      }
      break;

    case G_IR_NODE_UNION:
      {
	UnionBlob *blob = (UnionBlob *)&data[*offset];
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;
	GList *members;

	blob->blob_type = BLOB_TYPE_UNION;
	blob->deprecated = union_->deprecated;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->alignment = union_->alignment;
	blob->size = union_->size;
	if (union_->gtype_name)
	  {
	    blob->unregistered = FALSE;
	    blob->gtype_name = _g_ir_write_string (union_->gtype_name, strings, data, offset2);
	    blob->gtype_init = _g_ir_write_string (union_->gtype_init, strings, data, offset2);
	  }
	else
	  {
	    blob->unregistered = TRUE;
	    blob->gtype_name = 0;
	    blob->gtype_init = 0;
	  }

	blob->n_fields = 0;
	blob->n_functions = 0;

	blob->discriminator_offset = union_->discriminator_offset;

        if (union_->copy_func)
          blob->copy_func = _g_ir_write_string (union_->copy_func, strings, data, offset2);
        if (union_->free_func)
          blob->free_func = _g_ir_write_string (union_->free_func, strings, data, offset2);

	/* We don't support Union discriminators right now. */
	/*
	if (union_->discriminator_type)
	  {
	    *offset += 28;
	    blob->discriminated = TRUE;
	    _g_ir_node_build_typelib ((GIrNode *)union_->discriminator_type,
				     build, offset, offset2, NULL);
	  }
	else
	  {
        */
	*offset += sizeof (UnionBlob);
	blob->discriminated = FALSE;
	blob->discriminator_type.offset = 0;

	members = g_list_copy (union_->members);

	_g_ir_node_build_members (&members, G_IR_NODE_FIELD, &blob->n_fields,
				 node, build, offset, offset2, NULL);

	_g_ir_node_build_members (&members, G_IR_NODE_FUNCTION, &blob->n_functions,
				 node, build, offset, offset2, NULL);

	_g_ir_node_check_unhandled_members (&members, node->type);

	g_assert (members == NULL);

	if (union_->discriminator_type)
	  {
	    for (l = union_->discriminators; l; l = l->next)
	      {
		GIrNode *member = (GIrNode *)l->data;

		_g_ir_node_build_typelib (member, node, build, offset, offset2, NULL);
	      }
	  }
      }
      break;

    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	EnumBlob *blob = (EnumBlob *)&data[*offset];
	GIrNodeEnum *enum_ = (GIrNodeEnum *)node;

	*offset += sizeof (EnumBlob);

	if (node->type == G_IR_NODE_ENUM)
	  blob->blob_type = BLOB_TYPE_ENUM;
	else
	  blob->blob_type = BLOB_TYPE_FLAGS;

	blob->deprecated = enum_->deprecated;
	blob->reserved = 0;
	blob->storage_type = enum_->storage_type;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	if (enum_->gtype_name)
	  {
	    blob->unregistered = FALSE;
	    blob->gtype_name = _g_ir_write_string (enum_->gtype_name, strings, data, offset2);
	    blob->gtype_init = _g_ir_write_string (enum_->gtype_init, strings, data, offset2);
	  }
	else
	  {
	    blob->unregistered = TRUE;
	    blob->gtype_name = 0;
	    blob->gtype_init = 0;
	  }
	if (enum_->error_domain)
	  blob->error_domain = _g_ir_write_string (enum_->error_domain, strings, data, offset2);
	else
	  blob->error_domain = 0;

	blob->n_values = 0;
	blob->n_methods = 0;

	for (l = enum_->values; l; l = l->next)
	  {
	    GIrNode *value = (GIrNode *)l->data;

	    blob->n_values++;
	    _g_ir_node_build_typelib (value, node, build, offset, offset2, NULL);
	  }

	for (l = enum_->methods; l; l = l->next)
	  {
	    GIrNode *method = (GIrNode *)l->data;

	    blob->n_methods++;
	    _g_ir_node_build_typelib (method, node, build, offset, offset2, NULL);
	  }
      }
      break;

    case G_IR_NODE_OBJECT:
      {
	ObjectBlob *blob = (ObjectBlob *)&data[*offset];
	GIrNodeInterface *object = (GIrNodeInterface *)node;
	GList *members;

	blob->blob_type = BLOB_TYPE_OBJECT;
	blob->abstract = object->abstract;
        blob->fundamental = object->fundamental;
        blob->final_ = object->final_;
	blob->deprecated = object->deprecated;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->gtype_name = _g_ir_write_string (object->gtype_name, strings, data, offset2);
	blob->gtype_init = _g_ir_write_string (object->gtype_init, strings, data, offset2);
        if (object->ref_func)
          blob->ref_func = _g_ir_write_string (object->ref_func, strings, data, offset2);
        if (object->unref_func)
          blob->unref_func = _g_ir_write_string (object->unref_func, strings, data, offset2);
        if (object->set_value_func)
          blob->set_value_func = _g_ir_write_string (object->set_value_func, strings, data, offset2);
        if (object->get_value_func)
          blob->get_value_func = _g_ir_write_string (object->get_value_func, strings, data, offset2);
	if (object->parent)
	  blob->parent = find_entry (build, object->parent);
	else
	  blob->parent = 0;
	if (object->glib_type_struct)
	  blob->gtype_struct = find_entry (build, object->glib_type_struct);
	else
	  blob->gtype_struct = 0;

	blob->n_interfaces = 0;
	blob->n_fields = 0;
	blob->n_properties = 0;
	blob->n_methods = 0;
	blob->n_signals = 0;
	blob->n_vfuncs = 0;
	blob->n_constants = 0;
	blob->n_field_callbacks = 0;

	*offset += sizeof(ObjectBlob);
	for (l = object->interfaces; l; l = l->next)
	  {
	    blob->n_interfaces++;
	    *(guint16*)&data[*offset] = find_entry (build, (gchar *)l->data);
	    *offset += 2;
	  }

	members = g_list_copy (object->members);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_FIELD, &blob->n_fields,
				 node, build, offset, offset2, &blob->n_field_callbacks);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_PROPERTY, &blob->n_properties,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_FUNCTION, &blob->n_methods,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_SIGNAL, &blob->n_signals,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_VFUNC, &blob->n_vfuncs,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_CONSTANT, &blob->n_constants,
				 node, build, offset, offset2, NULL);

	_g_ir_node_check_unhandled_members (&members, node->type);

	g_assert (members == NULL);
      }
      break;

    case G_IR_NODE_INTERFACE:
      {
	InterfaceBlob *blob = (InterfaceBlob *)&data[*offset];
	GIrNodeInterface *iface = (GIrNodeInterface *)node;
	GList *members;

	blob->blob_type = BLOB_TYPE_INTERFACE;
	blob->deprecated = iface->deprecated;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->gtype_name = _g_ir_write_string (iface->gtype_name, strings, data, offset2);
	blob->gtype_init = _g_ir_write_string (iface->gtype_init, strings, data, offset2);
	if (iface->glib_type_struct)
	  blob->gtype_struct = find_entry (build, iface->glib_type_struct);
	else
	  blob->gtype_struct = 0;
	blob->n_prerequisites = 0;
	blob->n_properties = 0;
	blob->n_methods = 0;
	blob->n_signals = 0;
	blob->n_vfuncs = 0;
	blob->n_constants = 0;

	*offset += sizeof (InterfaceBlob);
	for (l = iface->prerequisites; l; l = l->next)
	  {
	    blob->n_prerequisites++;
	    *(guint16*)&data[*offset] = find_entry (build, (gchar *)l->data);
	    *offset += 2;
	  }

	members = g_list_copy (iface->members);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_PROPERTY, &blob->n_properties,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_FUNCTION, &blob->n_methods,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_SIGNAL, &blob->n_signals,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_VFUNC, &blob->n_vfuncs,
				 node, build, offset, offset2, NULL);

	*offset = ALIGN_VALUE (*offset, 4);
	_g_ir_node_build_members (&members, G_IR_NODE_CONSTANT, &blob->n_constants,
				 node, build, offset, offset2, NULL);

	_g_ir_node_check_unhandled_members (&members, node->type);

	g_assert (members == NULL);
      }
      break;


    case G_IR_NODE_VALUE:
      {
	GIrNodeValue *value = (GIrNodeValue *)node;
	ValueBlob *blob = (ValueBlob *)&data[*offset];
	*offset += sizeof (ValueBlob);

	blob->deprecated = value->deprecated;
	blob->reserved = 0;
	blob->unsigned_value = value->value >= 0 ? 1 : 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);
	blob->value = (gint32)value->value;
      }
      break;

    case G_IR_NODE_CONSTANT:
      {
	GIrNodeConstant *constant = (GIrNodeConstant *)node;
	ConstantBlob *blob = (ConstantBlob *)&data[*offset];
	guint32 pos;

	pos = *offset + G_STRUCT_OFFSET (ConstantBlob, type);
	*offset += sizeof (ConstantBlob);

	blob->blob_type = BLOB_TYPE_CONSTANT;
	blob->deprecated = constant->deprecated;
	blob->reserved = 0;
	blob->name = _g_ir_write_string (node->name, strings, data, offset2);

	blob->offset = *offset2;
	switch (constant->type->tag)
	  {
	  case GI_TYPE_TAG_BOOLEAN:
	    blob->size = 4;
	    *(gboolean*)&data[blob->offset] = parse_boolean_value (constant->value);
	    break;
	    case GI_TYPE_TAG_INT8:
	    blob->size = 1;
	      *(gint8*)&data[blob->offset] = (gint8) parse_int_value (constant->value);
	    break;
	  case GI_TYPE_TAG_UINT8:
	    blob->size = 1;
	    *(guint8*)&data[blob->offset] = (guint8) parse_uint_value (constant->value);
	    break;
	  case GI_TYPE_TAG_INT16:
	    blob->size = 2;
	    *(gint16*)&data[blob->offset] = (gint16) parse_int_value (constant->value);
	    break;
	  case GI_TYPE_TAG_UINT16:
	    blob->size = 2;
	    *(guint16*)&data[blob->offset] = (guint16) parse_uint_value (constant->value);
	    break;
	  case GI_TYPE_TAG_INT32:
	    blob->size = 4;
	    *(gint32*)&data[blob->offset] = (gint32) parse_int_value (constant->value);
	    break;
	  case GI_TYPE_TAG_UINT32:
	    blob->size = 4;
	    *(guint32*)&data[blob->offset] = (guint32) parse_uint_value (constant->value);
	    break;
	  case GI_TYPE_TAG_INT64:
	    blob->size = 8;
	    DO_ALIGNED_COPY(&data[blob->offset], parse_int_value (constant->value), gint64);
	    break;
	  case GI_TYPE_TAG_UINT64:
	    blob->size = 8;
	    DO_ALIGNED_COPY(&data[blob->offset], parse_uint_value (constant->value), guint64);
	    break;
	  case GI_TYPE_TAG_FLOAT:
	    blob->size = sizeof (gfloat);
	    DO_ALIGNED_COPY(&data[blob->offset], parse_float_value (constant->value), gfloat);
	    break;
	  case GI_TYPE_TAG_DOUBLE:
	    blob->size = sizeof (gdouble);
	    DO_ALIGNED_COPY(&data[blob->offset], parse_float_value (constant->value), gdouble);
	    break;
	  case GI_TYPE_TAG_UTF8:
	  case GI_TYPE_TAG_FILENAME:
	    blob->size = strlen (constant->value) + 1;
	    memcpy (&data[blob->offset], constant->value, blob->size);
	    break;
	  default:
	    break;
	  }
	*offset2 += ALIGN_VALUE (blob->size, 4);

	_g_ir_node_build_typelib ((GIrNode *)constant->type, node, build, &pos, offset2, NULL);
      }
      break;
    default:
      g_assert_not_reached ();
    }

  g_debug ("node %s%s%s%p type '%s', offset %d -> %d, offset2 %d -> %d",
	   node->name ? "'" : "",
	   node->name ? node->name : "",
	   node->name ? "' " : "",
	   node, _g_ir_node_type_to_string (node->type),
	   old_offset, *offset, old_offset2, *offset2);

  if (*offset2 - old_offset2 + *offset - old_offset > _g_ir_node_get_full_size (node))
    g_error ("exceeding space reservation; offset: %d (prev %d) offset2: %d (prev %d) nodesize: %d",
             *offset, old_offset, *offset2, old_offset2, _g_ir_node_get_full_size (node));

  if (appended_stack)
    build->stack = g_list_delete_link (build->stack, build->stack);
}

/* if str is already in the pool, return previous location, otherwise write str
 * to the typelib at offset, put it in the pool and update offset. If the
 * typelib is not large enough to hold the string, reallocate it.
 */
guint32
_g_ir_write_string (const gchar *str,
		    GHashTable  *strings,
		    guchar      *data,
		    guint32     *offset)
{
  gpointer value;
  guint32 start;

  string_count += 1;
  string_size += strlen (str);

  value = g_hash_table_lookup (strings, str);

  if (value)
    return GPOINTER_TO_UINT (value);

  unique_string_count += 1;
  unique_string_size += strlen (str);

  g_hash_table_insert (strings, (gpointer)str, GUINT_TO_POINTER (*offset));

  start = *offset;
  *offset = ALIGN_VALUE (start + strlen (str) + 1, 4);

  strcpy ((gchar*)&data[start], str);

  return start;
}

