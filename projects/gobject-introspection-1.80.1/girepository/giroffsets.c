/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Compute structure offsets
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#include "girffi.h"
#include "girnode.h"
#include <string.h>

/* The C standard specifies that an enumeration can be any char or any signed
 * or unsigned integer type capable of representing all the values of the
 * enumeration. We use test enumerations to figure out what choices the
 * compiler makes. (Ignoring > 32 bit enumerations)
 */

typedef enum {
  ENUM_1 = 1 /* compiler could use int8, uint8, int16, uint16, int32, uint32 */
} Enum1;

typedef enum {
  ENUM_2 = 128 /* compiler could use uint8, int16, uint16, int32, uint32 */
} Enum2;

typedef enum {
  ENUM_3 = 257 /* compiler could use int16, uint16, int32, uint32 */
} Enum3;

typedef enum {
  ENUM_4 = G_MAXSHORT + 1 /* compiler could use uint16, int32, uint32 */
} Enum4;

typedef enum {
  ENUM_5 = G_MAXUSHORT + 1 /* compiler could use int32, uint32 */
} Enum5;

typedef enum {
  ENUM_6 = ((guint)G_MAXINT) + 1 /* compiler could use uint32 */
} Enum6;

typedef enum {
  ENUM_7 = -1 /* compiler could use int8, int16, int32 */
} Enum7;

typedef enum {
  ENUM_8 = -129 /* compiler could use int16, int32 */
} Enum8;

typedef enum {
  ENUM_9 = G_MINSHORT - 1 /* compiler could use int32 */
} Enum9;

static void
compute_enum_storage_type (GIrNodeEnum *enum_node)
{
  GList *l;
  gint64 max_value = 0;
  gint64 min_value = 0;
  int width;
  gboolean signed_type;

  if (enum_node->storage_type != GI_TYPE_TAG_VOID) /* already done */
    return;

  for (l = enum_node->values; l; l = l->next)
    {
      GIrNodeValue *value = l->data;
      if (value->value > max_value)
        max_value = value->value;
      if (value->value < min_value)
        min_value = value->value;
    }

  if (min_value < 0)
    {
      signed_type = TRUE;

      if (min_value > -128 && max_value <= 127)
	width = sizeof(Enum7);
      else if (min_value >= G_MINSHORT && max_value <= G_MAXSHORT)
	width = sizeof(Enum8);
      else
	width = sizeof(Enum9);
    }
  else
    {
      if (max_value <= 127)
	{
	  width = sizeof (Enum1);
	  signed_type = (gint64)(Enum1)(-1) < 0;
	}
      else if (max_value <= 255)
	{
	  width = sizeof (Enum2);
	  signed_type = (gint64)(Enum2)(-1) < 0;
	}
      else if (max_value <= G_MAXSHORT)
	{
	  width = sizeof (Enum3);
	  signed_type = (gint64)(Enum3)(-1) < 0;
	}
      else if (max_value <= G_MAXUSHORT)
	{
	  width = sizeof (Enum4);
	  signed_type = (gint64)(Enum4)(-1) < 0;
	}
      else if (max_value <= G_MAXINT)
	{
	  width = sizeof (Enum5);
	  signed_type = (gint64)(Enum5)(-1) < 0;
	}
      else
	{
	  width = sizeof (Enum6);
	  signed_type = (gint64)(Enum6)(-1) < 0;
	}
    }

  if (width == 1)
    enum_node->storage_type = signed_type ? GI_TYPE_TAG_INT8 : GI_TYPE_TAG_UINT8;
  else if (width == 2)
    enum_node->storage_type = signed_type ? GI_TYPE_TAG_INT16 : GI_TYPE_TAG_UINT16;
  else if (width == 4)
    enum_node->storage_type = signed_type ? GI_TYPE_TAG_INT32 : GI_TYPE_TAG_UINT32;
  else if (width == 8)
    enum_node->storage_type = signed_type ? GI_TYPE_TAG_INT64 : GI_TYPE_TAG_UINT64;
  else
    g_error ("Unexpected enum width %d", width);
}

static gboolean
get_enum_size_alignment (GIrNodeEnum *enum_node,
			 gint        *size,
			 gint        *alignment)
{
  ffi_type *type_ffi;

  compute_enum_storage_type (enum_node);

  switch (enum_node->storage_type)
    {
    case GI_TYPE_TAG_INT8:
    case GI_TYPE_TAG_UINT8:
      type_ffi = &ffi_type_uint8;
      break;
    case GI_TYPE_TAG_INT16:
    case GI_TYPE_TAG_UINT16:
      type_ffi = &ffi_type_uint16;
      break;
    case GI_TYPE_TAG_INT32:
    case GI_TYPE_TAG_UINT32:
      type_ffi = &ffi_type_uint32;
      break;
    case GI_TYPE_TAG_INT64:
    case GI_TYPE_TAG_UINT64:
      type_ffi = &ffi_type_uint64;
      break;
    default:
      g_error ("Unexpected enum storage type %s",
	       g_type_tag_to_string (enum_node->storage_type));
    }

  *size = type_ffi->size;
  *alignment = type_ffi->alignment;

  return TRUE;
}

static gboolean
get_interface_size_alignment (GIrTypelibBuild   *build,
			      GIrNodeType *type,
			      gint        *size,
			      gint        *alignment,
                              const char  *who)
{
  GIrNode *iface;

  iface = _g_ir_find_node (build, ((GIrNode*)type)->module, type->giinterface);
  if (!iface)
    {
      _g_ir_module_fatal (build, 0, "Can't resolve type '%s' for %s", type->giinterface, who);
      *size = -1;
      *alignment = -1;
      return FALSE;
    }

  _g_ir_node_compute_offsets (build, iface);

  switch (iface->type)
    {
    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)iface;
	*size = boxed->size;
	*alignment = boxed->alignment;
	break;
      }
    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)iface;
	*size = struct_->size;
	*alignment = struct_->alignment;
	break;
      }
    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *interface = (GIrNodeInterface *)iface;
	*size = interface->size;
	*alignment = interface->alignment;
	break;
      }
    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)iface;
	*size = union_->size;
	*alignment = union_->alignment;
	break;
      }
    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	return get_enum_size_alignment ((GIrNodeEnum *)iface,
					size, alignment);
      }
    case G_IR_NODE_CALLBACK:
      {
	*size = ffi_type_pointer.size;
	*alignment = ffi_type_pointer.alignment;
	break;
      }
    default:
      {
	g_warning ("%s has is not a pointer and is of type %s",
                   who,
		   _g_ir_node_type_to_string (iface->type));
	*size = -1;
	*alignment = -1;
	break;
      }
    }

  return *alignment > 0;
}

static gboolean
get_type_size_alignment (GIrTypelibBuild   *build,
			 GIrNodeType *type,
                         gint        *size,
                         gint        *alignment,
                         const char  *who)
{
  ffi_type *type_ffi;

  if (type->is_pointer)
    {
      type_ffi = &ffi_type_pointer;
    }
  else if (type->tag == GI_TYPE_TAG_ARRAY)
    {
      gint elt_size, elt_alignment;

      if (!type->has_size
          || !get_type_size_alignment(build, type->parameter_type1,
                                      &elt_size, &elt_alignment, who))
        {
          *size = -1;
          *alignment = -1;
          return FALSE;
        }

      *size = type->size * elt_size;
      *alignment = elt_alignment;

      return TRUE;
    }
  else
    {
      if (type->tag == GI_TYPE_TAG_INTERFACE)
	{
	  return get_interface_size_alignment (build, type, size, alignment, who);
	}
      else
	{
	  type_ffi = gi_type_tag_get_ffi_type (type->tag, type->is_pointer);

	  if (type_ffi == &ffi_type_void)
	    {
	      g_warning ("%s has void type", who);
	      *size = -1;
	      *alignment = -1;
	      return FALSE;
	    }
	  else if (type_ffi == &ffi_type_pointer)
	    {
	      g_warning ("%s has is not a pointer and is of type %s",
                         who,
			 g_type_tag_to_string (type->tag));
	      *size = -1;
	      *alignment = -1;
	      return FALSE;
	    }
	}
    }

  g_assert (type_ffi);
  *size = type_ffi->size;
  *alignment = type_ffi->alignment;

  return TRUE;
}

static gboolean
get_field_size_alignment (GIrTypelibBuild    *build,
			  GIrNodeField *field,
			  GIrNode      *parent_node,
			  gint         *size,
			  gint         *alignment)
{
  GIrModule *module = build->module;
  gchar *who;
  gboolean success;

  who = g_strdup_printf ("field %s.%s.%s", module->name, parent_node->name, ((GIrNode *)field)->name);

  if (field->callback)
    {
      *size = ffi_type_pointer.size;
      *alignment = ffi_type_pointer.alignment;
      success = TRUE;
    }
  else
    success = get_type_size_alignment (build, field->type, size, alignment, who);
  g_free (who);

  return success;
}

#define GI_ALIGN(n, align) (((n) + (align) - 1) & ~((align) - 1))

static gboolean
compute_struct_field_offsets (GIrTypelibBuild   *build,
			      GIrNode     *node,
			      GList       *members,
			      gint        *size_out,
			      gint        *alignment_out)
{
  int size = 0;
  int alignment = 1;
  GList *l;
  gboolean have_error = FALSE;

  *alignment_out = -2; /* mark to detect recursion */

  for (l = members; l; l = l->next)
    {
      GIrNode *member = (GIrNode *)l->data;

      if (member->type == G_IR_NODE_FIELD)
	{
	  GIrNodeField *field = (GIrNodeField *)member;

	  if (!have_error)
	    {
	      int member_size;
	      int member_alignment;

	      if (get_field_size_alignment (build, field, node,
					    &member_size, &member_alignment))
		{
		  size = GI_ALIGN (size, member_alignment);
		  alignment = MAX (alignment, member_alignment);
		  field->offset = size;
		  size += member_size;
		}
	      else
		have_error = TRUE;
	    }

	  if (have_error)
	    field->offset = -1;
	}
      else if (member->type == G_IR_NODE_CALLBACK)
	{
          size = GI_ALIGN (size, ffi_type_pointer.alignment);
          alignment = MAX (alignment, ffi_type_pointer.alignment);
	  size += ffi_type_pointer.size;
	}
    }

  /* Structs are tail-padded out to a multiple of their alignment */
  size = GI_ALIGN (size, alignment);

  if (!have_error)
    {
      *size_out = size;
      *alignment_out = alignment;
    }
  else
    {
      *size_out = -1;
      *alignment_out = -1;
    }

  return !have_error;
}

static gboolean
compute_union_field_offsets (GIrTypelibBuild *build,
			     GIrNode     *node,
			     GList       *members,
			     gint        *size_out,
			     gint        *alignment_out)
{
  int size = 0;
  int alignment = 1;
  GList *l;
  gboolean have_error = FALSE;

  *alignment_out = -2; /* mark to detect recursion */

  for (l = members; l; l = l->next)
    {
      GIrNode *member = (GIrNode *)l->data;

      if (member->type == G_IR_NODE_FIELD)
	{
	  GIrNodeField *field = (GIrNodeField *)member;

	  if (!have_error)
	    {
	      int member_size;
	      int member_alignment;

	      if (get_field_size_alignment (build,field, node,
					    &member_size, &member_alignment))
		{
		  size = MAX (size, member_size);
		  alignment = MAX (alignment, member_alignment);
		}
	      else
		have_error = TRUE;
	    }
	}
    }

  /* Unions are tail-padded out to a multiple of their alignment */
  size = GI_ALIGN (size, alignment);

  if (!have_error)
    {
      *size_out = size;
      *alignment_out = alignment;
    }
  else
    {
      *size_out = -1;
      *alignment_out = -1;
    }

  return !have_error;
}

static gboolean
check_needs_computation (GIrTypelibBuild *build,
			 GIrNode   *node,
			 gint       alignment)
{
  GIrModule *module = build->module;
  /*
   *  0: Not yet computed
   * >0: Previously succeeded
   * -1: Previously failed
   * -2: In progress
   */
  if (alignment == -2)
    {
      g_warning ("Recursion encountered when computing the size of %s.%s",
		 module->name, node->name);
    }

  return alignment == 0;
}

/*
 * _g_ir_node_compute_offsets:
 * @build: Current typelib build
 * @node: a #GIrNode
 *
 * If a node is a a structure or union, makes sure that the field
 * offsets have been computed, and also computes the overall size and
 * alignment for the type.
 */
void
_g_ir_node_compute_offsets (GIrTypelibBuild *build,
			    GIrNode         *node)
{
  gboolean appended_stack;

  if (build->stack)
    appended_stack = node != (GIrNode*)build->stack->data; 
  else
    appended_stack = TRUE;
  if (appended_stack)
    build->stack = g_list_prepend (build->stack, node);

  switch (node->type)
    {
    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;

	if (!check_needs_computation (build, node, boxed->alignment))
	  return;

	compute_struct_field_offsets (build, node, boxed->members,
				      &boxed->size, &boxed->alignment);
	break;
      }
    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;

	if (!check_needs_computation (build, node, struct_->alignment))
	  return;

	compute_struct_field_offsets (build, node, struct_->members,
				      &struct_->size, &struct_->alignment);
	break;
      }
    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *iface = (GIrNodeInterface *)node;

	if (!check_needs_computation (build, node, iface->alignment))
	  return;

	compute_struct_field_offsets (build, node, iface->members,
				      &iface->size, &iface->alignment);
	break;
      }
    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;

	if (!check_needs_computation (build, node, union_->alignment))
	  return;

	compute_union_field_offsets (build, (GIrNode*)union_, union_->members,
				     &union_->size, &union_->alignment);
	break;
      }
    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	GIrNodeEnum *enum_ = (GIrNodeEnum *)node;

	if (enum_->storage_type != GI_TYPE_TAG_VOID) /* already done */
	  return;

	compute_enum_storage_type (enum_);

	break;
      }
    default:
      break;
    }
  
  if (appended_stack)
    build->stack = g_list_delete_link (build->stack, build->stack);
}
