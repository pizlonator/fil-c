/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: IDL generator
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

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "girwriter.h"
#include "girepository.h"
#include "gitypelib-internal.h"

typedef struct {
  FILE *file;
  GSList *stack;
  gboolean show_all;
} Xml;

typedef struct {
  char *name;
  guint has_children : 1;
} XmlElement;

static XmlElement *
xml_element_new (const char *name)
{
  XmlElement *elem;

  elem = g_slice_new (XmlElement);
  elem->name = g_strdup (name);
  elem->has_children = FALSE;
  return elem;
}

static void
xml_element_free (XmlElement *elem)
{
  g_free (elem->name);
  g_slice_free (XmlElement, elem);
}

static void
xml_printf (Xml *xml, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

static void
xml_printf (Xml *xml, const char *fmt, ...)
{
  va_list ap;
  char *s;

  va_start (ap, fmt);
  s = g_markup_vprintf_escaped (fmt, ap);
  fputs (s, xml->file);
  g_free (s);
  va_end (ap);
}

static void
xml_start_element (Xml *xml, const char *element_name)
{
  XmlElement *parent = NULL;

  if (xml->stack)
    {
      parent = xml->stack->data;

      if (!parent->has_children)
        xml_printf (xml, ">\n");

      parent->has_children = TRUE;
    }

  xml_printf (xml, "%*s<%s", g_slist_length(xml->stack)*2, "", element_name);

  xml->stack = g_slist_prepend (xml->stack, xml_element_new (element_name));
}

static void
xml_end_element (Xml *xml, const char *name)
{
  XmlElement *elem;

  g_assert (xml->stack != NULL);

  elem = xml->stack->data;
  xml->stack = g_slist_delete_link (xml->stack, xml->stack);

  if (name != NULL)
    g_assert_cmpstr (name, ==, elem->name);

  if (elem->has_children)
    xml_printf (xml, "%*s</%s>\n", g_slist_length (xml->stack)*2, "", elem->name);
  else
    xml_printf (xml, "/>\n");

  xml_element_free (elem);
}

static void
xml_end_element_unchecked (Xml *xml)
{
  xml_end_element (xml, NULL);
}

static Xml *
xml_open (FILE *file)
{
  Xml *xml;

  xml = g_slice_new (Xml);
  xml->file = file;
  xml->stack = NULL;

  return xml;
}

static void
xml_close (Xml *xml)
{
  g_assert (xml->stack == NULL);
  if (xml->file != NULL)
    {
      fflush (xml->file);
      if (xml->file != stdout)
        fclose (xml->file);
      xml->file = NULL;
    }
}

static void
xml_free (Xml *xml)
{
  xml_close (xml);
  g_slice_free (Xml, xml);
}


static void
check_unresolved (GIBaseInfo *info)
{
  if (g_base_info_get_type (info) != GI_INFO_TYPE_UNRESOLVED)
    return;

  g_critical ("Found unresolved type '%s' '%s'\n",
	      g_base_info_get_name (info), g_base_info_get_namespace (info));
}

static void
write_type_name (const gchar *namespace,
		 GIBaseInfo  *info,
		 Xml         *file)
{
  if (strcmp (namespace, g_base_info_get_namespace (info)) != 0)
    xml_printf (file, "%s.", g_base_info_get_namespace (info));

  xml_printf (file, "%s", g_base_info_get_name (info));
}

static void
write_type_name_attribute (const gchar *namespace,
			   GIBaseInfo  *info,
			   const char  *attr_name,
			   Xml         *file)
{
  xml_printf (file, " %s=\"", attr_name);
  write_type_name (namespace, info, file);
  xml_printf (file, "\"");
}

 static void
write_ownership_transfer (GITransfer transfer,
                          Xml       *file)
{
  switch (transfer)
    {
    case GI_TRANSFER_NOTHING:
      xml_printf (file, " transfer-ownership=\"none\"");
      break;
    case GI_TRANSFER_CONTAINER:
      xml_printf (file, " transfer-ownership=\"container\"");
      break;
    case GI_TRANSFER_EVERYTHING:
      xml_printf (file, " transfer-ownership=\"full\"");
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
write_type_info (const gchar *namespace,
		 GITypeInfo  *info,
		 Xml         *file)
{
  gint tag;
  GITypeInfo *type;
  gboolean is_pointer;

  check_unresolved ((GIBaseInfo*)info);

  tag = g_type_info_get_tag (info);
  is_pointer = g_type_info_is_pointer (info);

  if (tag == GI_TYPE_TAG_VOID)
    {
      xml_start_element (file, "type");

      xml_printf (file, " name=\"%s\"", is_pointer ? "any" : "none");

      xml_end_element (file, "type");
    }
  else if (GI_TYPE_TAG_IS_BASIC (tag))
    {
      xml_start_element (file, "type");
      xml_printf (file, " name=\"%s\"", g_type_tag_to_string (tag));
      xml_end_element (file, "type");
    }
  else if (tag == GI_TYPE_TAG_ARRAY)
    {
      gint length, size;
      const char *name = NULL;

      xml_start_element (file, "array");

      switch (g_type_info_get_array_type (info)) {
        case GI_ARRAY_TYPE_C:
            break;
        case GI_ARRAY_TYPE_ARRAY:
            name = "GLib.Array";
            break;
        case GI_ARRAY_TYPE_PTR_ARRAY:
            name = "GLib.PtrArray";
            break;
        case GI_ARRAY_TYPE_BYTE_ARRAY:
            name = "GLib.ByteArray";
            break;
        default:
            break;
      }

      if (name)
        xml_printf (file, " name=\"%s\"", name);

      type = g_type_info_get_param_type (info, 0);

      length = g_type_info_get_array_length (info);
      if (length >= 0)
        xml_printf (file, " length=\"%d\"", length);

      size = g_type_info_get_array_fixed_size (info);
      if (size >= 0)
        xml_printf (file, " fixed-size=\"%d\"", size);

      if (g_type_info_is_zero_terminated (info))
	xml_printf (file, " zero-terminated=\"1\"");

      write_type_info (namespace, type, file);

      g_base_info_unref ((GIBaseInfo *)type);

      xml_end_element (file, "array");
    }
  else if (tag == GI_TYPE_TAG_INTERFACE)
    {
      GIBaseInfo *iface = g_type_info_get_interface (info);
      xml_start_element (file, "type");
      write_type_name_attribute (namespace, iface, "name", file);
      xml_end_element (file, "type");
      g_base_info_unref (iface);
    }
  else if (tag == GI_TYPE_TAG_GLIST)
    {
      xml_start_element (file, "type");
      xml_printf (file, " name=\"GLib.List\"");
      type = g_type_info_get_param_type (info, 0);
      if (type)
	{
	  write_type_info (namespace, type, file);
	  g_base_info_unref ((GIBaseInfo *)type);
	}
      xml_end_element (file, "type");
    }
  else if (tag == GI_TYPE_TAG_GSLIST)
    {
      xml_start_element (file, "type");
      xml_printf (file, " name=\"GLib.SList\"");
      type = g_type_info_get_param_type (info, 0);
      if (type)
	{
	  write_type_info (namespace, type, file);
	  g_base_info_unref ((GIBaseInfo *)type);
	}
      xml_end_element (file, "type");
    }
  else if (tag == GI_TYPE_TAG_GHASH)
    {
      xml_start_element (file, "type");
      xml_printf (file, " name=\"GLib.HashTable\"");
      type = g_type_info_get_param_type (info, 0);
      if (type)
	{
	  write_type_info (namespace, type, file);
	  g_base_info_unref ((GIBaseInfo *)type);
	  type = g_type_info_get_param_type (info, 1);
	  write_type_info (namespace, type, file);
	  g_base_info_unref ((GIBaseInfo *)type);
	}
      xml_end_element (file, "type");
    }
  else if (tag == GI_TYPE_TAG_ERROR)
    {
      xml_start_element (file, "type");
      xml_printf (file, " name=\"GLib.Error\"");
      xml_end_element (file, "type");
    }
  else
    {
      g_printerr ("Unhandled type tag %d\n", tag);
      g_assert_not_reached ();
    }
}

static void
write_attributes (Xml *file,
                  GIBaseInfo *info)
{
  GIAttributeIter iter = { 0, };
  char *name, *value;

  while (g_base_info_iterate_attributes (info, &iter, &name, &value))
    {
      xml_start_element (file, "attribute");
      xml_printf (file, " name=\"%s\" value=\"%s\"", name, value);
      xml_end_element (file, "attribute");
    }
}

static void
write_return_value_attributes (Xml *file,
                               GICallableInfo *info)
{
  GIAttributeIter iter = { 0, };
  char *name, *value;

  while (g_callable_info_iterate_return_attributes (info, &iter, &name, &value))
    {
      xml_start_element (file, "attribute");
      xml_printf (file, " name=\"%s\" value=\"%s\"", name, value);
      xml_end_element (file, "attribute");
    }
}

static void
write_constant_value (const gchar *namespace,
		      GITypeInfo *info,
		      GIArgument *argument,
		      Xml *file);

static void
write_callback_info (const gchar    *namespace,
		     GICallbackInfo *info,
		     Xml            *file);

static void
write_field_info (const gchar *namespace,
		  GIFieldInfo *info,
		  GIConstantInfo *branch,
		  Xml         *file)
{
  const gchar *name;
  GIFieldInfoFlags flags;
  gint size;
  gint offset;
  GITypeInfo *type;
  GIBaseInfo *interface;
  GIArgument value;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_field_info_get_flags (info);
  size = g_field_info_get_size (info);
  offset = g_field_info_get_offset (info);

  xml_start_element (file, "field");
  xml_printf (file, " name=\"%s\"", name);

  /* Fields are assumed to be read-only
   * (see also girwriter.py and girparser.c)
   */
  if (!(flags & GI_FIELD_IS_READABLE))
    xml_printf (file, " readable=\"0\"");
  if (flags & GI_FIELD_IS_WRITABLE)
    xml_printf (file, " writable=\"1\"");

  if (size)
    xml_printf (file, " bits=\"%d\"", size);

  write_attributes (file, (GIBaseInfo*) info);

  type = g_field_info_get_type (info);

  if (branch)
    {
      xml_printf (file, " branch=\"");
      g_base_info_unref ((GIBaseInfo *)type);
      type = g_constant_info_get_type (branch);
      g_constant_info_get_value (branch, &value);
      write_constant_value (namespace, type, &value, file);
      xml_printf (file, "\"");
    }

  if (file->show_all)
    {
      if (offset >= 0)
        xml_printf (file, "offset=\"%d\"", offset);
    }

  interface = g_type_info_get_interface (type);
  if (interface && g_base_info_get_type(interface) == GI_INFO_TYPE_CALLBACK)
    write_callback_info (namespace, (GICallbackInfo *)interface, file);
  else
    write_type_info (namespace, type, file);

  if (interface)
    g_base_info_unref (interface);

  g_base_info_unref ((GIBaseInfo *)type);

  xml_end_element (file, "field");
}

static void
write_callable_info (const gchar    *namespace,
		     GICallableInfo *info,
		     Xml            *file)
{
  GITypeInfo *type;
  gint i;

  if (g_callable_info_can_throw_gerror (info))
    xml_printf (file, " throws=\"1\"");

  write_attributes (file, (GIBaseInfo*) info);

  type = g_callable_info_get_return_type (info);

  xml_start_element (file, "return-value");

  write_ownership_transfer (g_callable_info_get_caller_owns (info), file);

  if (g_callable_info_may_return_null (info))
    xml_printf (file, " allow-none=\"1\"");

  if (g_callable_info_skip_return (info))
    xml_printf (file, " skip=\"1\"");

  write_return_value_attributes (file, info);

  write_type_info (namespace, type, file);

  xml_end_element (file, "return-value");

  if (g_callable_info_get_n_args (info) <= 0)
    return;

  xml_start_element (file, "parameters");
  for (i = 0; i < g_callable_info_get_n_args (info); i++)
    {
      GIArgInfo *arg = g_callable_info_get_arg (info, i);

      xml_start_element (file, "parameter");
      xml_printf (file, " name=\"%s\"",
                  g_base_info_get_name ((GIBaseInfo *) arg));

      write_ownership_transfer (g_arg_info_get_ownership_transfer (arg), file);

      switch (g_arg_info_get_direction (arg))
	{
	case GI_DIRECTION_IN:
	  break;
	case GI_DIRECTION_OUT:
	  xml_printf (file, " direction=\"out\" caller-allocates=\"%s\"",
	              g_arg_info_is_caller_allocates (arg) ? "1" : "0");
	  break;
	case GI_DIRECTION_INOUT:
	  xml_printf (file, " direction=\"inout\"");
	  break;
	default:
	  g_assert_not_reached ();
	}

      if (g_arg_info_may_be_null (arg))
	xml_printf (file, " allow-none=\"1\"");

      if (g_arg_info_is_return_value (arg))
	xml_printf (file, " retval=\"1\"");

      if (g_arg_info_is_optional (arg))
	xml_printf (file, " optional=\"1\"");

      switch (g_arg_info_get_scope (arg))
        {
        case GI_SCOPE_TYPE_INVALID:
          break;
        case GI_SCOPE_TYPE_CALL:
          xml_printf (file, " scope=\"call\"");
          break;
        case GI_SCOPE_TYPE_ASYNC:
          xml_printf (file, " scope=\"async\"");
          break;
        case GI_SCOPE_TYPE_NOTIFIED:
          xml_printf (file, " scope=\"notified\"");
          break;
        case GI_SCOPE_TYPE_FOREVER:
          xml_printf (file, " scope=\"forever\"");
          break;
        default:
          g_assert_not_reached ();
        }

      if (g_arg_info_get_closure (arg) >= 0)
        xml_printf (file, " closure=\"%d\"", g_arg_info_get_closure (arg));

      if (g_arg_info_get_destroy (arg) >= 0)
        xml_printf (file, " destroy=\"%d\"", g_arg_info_get_destroy (arg));

      if (g_arg_info_is_skip (arg))
        xml_printf (file, " skip=\"1\"");

      write_attributes (file, (GIBaseInfo*) arg);

      type = g_arg_info_get_type (arg);
      write_type_info (namespace, type, file);

      xml_end_element (file, "parameter");

      g_base_info_unref ((GIBaseInfo *)arg);
    }

  xml_end_element (file, "parameters");
  g_base_info_unref ((GIBaseInfo *)type);
}

static void
write_function_info (const gchar    *namespace,
		     GIFunctionInfo *info,
		     Xml            *file)
{
  GIFunctionInfoFlags flags;
  const gchar *tag;
  const gchar *name;
  const gchar *symbol;
  gboolean deprecated;

  flags = g_function_info_get_flags (info);
  name = g_base_info_get_name ((GIBaseInfo *)info);
  symbol = g_function_info_get_symbol (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  if (flags & GI_FUNCTION_IS_CONSTRUCTOR)
    tag = "constructor";
  else if (flags & GI_FUNCTION_IS_METHOD)
    tag = "method";
  else
    tag = "function";

  xml_start_element (file, tag);
  xml_printf (file, " name=\"%s\" c:identifier=\"%s\"",
              name, symbol);

  if ((flags & GI_FUNCTION_IS_SETTER) || (flags & GI_FUNCTION_IS_GETTER))
    {
      GIPropertyInfo *property = g_function_info_get_property (info);

      if (property != NULL)
        {
          const char *property_name = g_base_info_get_name ((GIBaseInfo *)property);

          if (flags & GI_FUNCTION_IS_SETTER)
            xml_printf (file, " glib:set-property=\"%s\"", property_name);
          else if (flags & GI_FUNCTION_IS_GETTER)
            xml_printf (file, " glib:get-property=\"%s\"", property_name);

          g_base_info_unref (property);
        }
    }

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_callable_info (namespace, (GICallableInfo*)info, file);
  xml_end_element (file, tag);
}

static void
write_callback_info (const gchar    *namespace,
		     GICallbackInfo *info,
		     Xml            *file)
{
  const gchar *name;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "callback");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_callable_info (namespace, (GICallableInfo*)info, file);
  xml_end_element (file, "callback");
}

static void
write_struct_info (const gchar  *namespace,
		   GIStructInfo *info,
		   Xml          *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  const gchar *func;
  gboolean deprecated;
  gboolean is_gtype_struct;
  gboolean foreign;
  gint i;
  gint size;
  int n_elts;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);

  if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_BOXED)
    {
      xml_start_element (file, "glib:boxed");
      xml_printf (file, " glib:name=\"%s\"", name);
    }
  else
    {
      xml_start_element (file, "record");
      xml_printf (file, " name=\"%s\"", name);
    }

  if (type_name != NULL)
    xml_printf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  is_gtype_struct = g_struct_info_is_gtype_struct (info);
  if (is_gtype_struct)
    xml_printf (file, " glib:is-gtype-struct=\"1\"");

  func = g_struct_info_get_copy_function (info);
  if (func)
    xml_printf (file, " copy-function=\"%s\"", func);

  func = g_struct_info_get_free_function (info);
  if (func)
    xml_printf (file, " free-function=\"%s\"", func);

  write_attributes (file, (GIBaseInfo*) info);

  size = g_struct_info_get_size (info);
  if (file->show_all && size >= 0)
    xml_printf (file, " size=\"%d\"", size);

  foreign = g_struct_info_is_foreign (info);
  if (foreign)
    xml_printf (file, " foreign=\"1\"");

  n_elts = g_struct_info_get_n_fields (info) + g_struct_info_get_n_methods (info);
  if (n_elts > 0)
    {
      for (i = 0; i < g_struct_info_get_n_fields (info); i++)
	{
	  GIFieldInfo *field = g_struct_info_get_field (info, i);
	  write_field_info (namespace, field, NULL, file);
	  g_base_info_unref ((GIBaseInfo *)field);
	}

      for (i = 0; i < g_struct_info_get_n_methods (info); i++)
	{
	  GIFunctionInfo *function = g_struct_info_get_method (info, i);
	  write_function_info (namespace, function, file);
	  g_base_info_unref ((GIBaseInfo *)function);
	}

    }

  xml_end_element_unchecked (file);
}

static void
write_value_info (const gchar *namespace,
		  GIValueInfo *info,
		  Xml         *file)
{
  const gchar *name;
  gint64 value;
  gchar *value_str;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  value = g_value_info_get_value (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "member");
  value_str = g_strdup_printf ("%" G_GINT64_FORMAT, value);
  xml_printf (file, " name=\"%s\" value=\"%s\"", name, value_str);
  g_free (value_str);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_attributes (file, (GIBaseInfo*) info);

  xml_end_element (file, "member");
}

static void
write_constant_value (const gchar *namespace,
		      GITypeInfo *type,
		      GIArgument  *value,
		      Xml        *file)
{
  switch (g_type_info_get_tag (type))
    {
    case GI_TYPE_TAG_BOOLEAN:
      xml_printf (file, "%d", value->v_boolean);
      break;
    case GI_TYPE_TAG_INT8:
      xml_printf (file, "%d", value->v_int8);
      break;
    case GI_TYPE_TAG_UINT8:
      xml_printf (file, "%d", value->v_uint8);
      break;
    case GI_TYPE_TAG_INT16:
      xml_printf (file, "%" G_GINT16_FORMAT, value->v_int16);
      break;
    case GI_TYPE_TAG_UINT16:
      xml_printf (file, "%" G_GUINT16_FORMAT, value->v_uint16);
      break;
    case GI_TYPE_TAG_INT32:
      xml_printf (file, "%" G_GINT32_FORMAT, value->v_int32);
      break;
    case GI_TYPE_TAG_UINT32:
      xml_printf (file, "%" G_GUINT32_FORMAT, value->v_uint32);
      break;
    case GI_TYPE_TAG_INT64:
      xml_printf (file, "%" G_GINT64_FORMAT, value->v_int64);
      break;
    case GI_TYPE_TAG_UINT64:
      xml_printf (file, "%" G_GUINT64_FORMAT, value->v_uint64);
      break;
    case GI_TYPE_TAG_FLOAT:
      xml_printf (file, "%f", (double)value->v_float);
      break;
    case GI_TYPE_TAG_DOUBLE:
      xml_printf (file, "%f", value->v_double);
      break;
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
      xml_printf (file, "%s", value->v_string);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
write_constant_info (const gchar    *namespace,
		     GIConstantInfo *info,
		     Xml            *file)
{
  GITypeInfo *type;
  const gchar *name;
  GIArgument value;

  name = g_base_info_get_name ((GIBaseInfo *)info);

  xml_start_element (file, "constant");
  xml_printf (file, " name=\"%s\"", name);

  type = g_constant_info_get_type (info);
  xml_printf (file, " value=\"");

  g_constant_info_get_value (info, &value);
  write_constant_value (namespace, type, &value, file);
  xml_printf (file, "\"");

  write_type_info (namespace, type, file);

  write_attributes (file, (GIBaseInfo*) info);

  xml_end_element (file, "constant");

  g_base_info_unref ((GIBaseInfo *)type);
}


static void
write_enum_info (const gchar *namespace,
		 GIEnumInfo *info,
		 Xml         *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  const gchar *error_domain;
  gboolean deprecated;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  error_domain = g_enum_info_get_error_domain (info);

  if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_ENUM)
    xml_start_element (file, "enumeration");
  else
    xml_start_element (file, "bitfield");
  xml_printf (file, " name=\"%s\"", name);

  if (type_init)
    xml_printf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);
  if (error_domain)
    xml_printf (file, " glib:error-domain=\"%s\"", error_domain);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_attributes (file, (GIBaseInfo*) info);

  for (i = 0; i < g_enum_info_get_n_values (info); i++)
    {
      GIValueInfo *value = g_enum_info_get_value (info, i);
      write_value_info (namespace, value, file);
      g_base_info_unref ((GIBaseInfo *)value);
    }

  xml_end_element_unchecked (file);
}

static void
write_signal_info (const gchar  *namespace,
		   GISignalInfo *info,
		   Xml          *file)
{
  GSignalFlags flags;
  const gchar *name;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_signal_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "glib:signal");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  if (flags & G_SIGNAL_RUN_FIRST)
    xml_printf (file, " when=\"FIRST\"");
  else if (flags & G_SIGNAL_RUN_LAST)
    xml_printf (file, " when=\"LAST\"");
  else if (flags & G_SIGNAL_RUN_CLEANUP)
    xml_printf (file, " when=\"CLEANUP\"");

  if (flags & G_SIGNAL_NO_RECURSE)
    xml_printf (file, " no-recurse=\"1\"");

  if (flags & G_SIGNAL_DETAILED)
    xml_printf (file, " detailed=\"1\"");

  if (flags & G_SIGNAL_ACTION)
    xml_printf (file, " action=\"1\"");

  if (flags & G_SIGNAL_NO_HOOKS)
    xml_printf (file, " no-hooks=\"1\"");

  write_callable_info (namespace, (GICallableInfo*)info, file);

  xml_end_element (file, "glib:signal");
}

static void
write_vfunc_info (const gchar *namespace,
		  GIVFuncInfo *info,
		  Xml         *file)
{
  GIVFuncInfoFlags flags;
  const gchar *name;
  GIFunctionInfo *invoker;
  gboolean deprecated;
  gint offset;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_vfunc_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);
  offset = g_vfunc_info_get_offset (info);
  invoker = g_vfunc_info_get_invoker (info);

  xml_start_element (file, "virtual-method");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  if (flags & GI_VFUNC_MUST_CHAIN_UP)
    xml_printf (file, " must-chain-up=\"1\"");

  if (flags & GI_VFUNC_MUST_OVERRIDE)
    xml_printf (file, " override=\"always\"");
  else if (flags & GI_VFUNC_MUST_NOT_OVERRIDE)
    xml_printf (file, " override=\"never\"");

  xml_printf (file, " offset=\"%d\"", offset);

  if (invoker)
    {
      xml_printf (file, " invoker=\"%s\"", g_base_info_get_name ((GIBaseInfo*)invoker));
      g_base_info_unref ((GIBaseInfo *)invoker);
    }

  write_callable_info (namespace, (GICallableInfo*)info, file);

  xml_end_element (file, "virtual-method");
}

static void
write_property_info (const gchar    *namespace,
		     GIPropertyInfo *info,
		     Xml            *file)
{
  GParamFlags flags;
  const gchar *name;
  gboolean deprecated;
  GITypeInfo *type;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_property_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "property");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  /* Properties are assumed to be read-only (see also girwriter.py) */
  if (!(flags & G_PARAM_READABLE))
    xml_printf (file, " readable=\"0\"");
  if (flags & G_PARAM_WRITABLE)
    xml_printf (file, " writable=\"1\"");

  if (flags & G_PARAM_CONSTRUCT)
    xml_printf (file, " construct=\"1\"");

  if (flags & G_PARAM_CONSTRUCT_ONLY)
    xml_printf (file, " construct-only=\"1\"");

  if (flags & G_PARAM_READABLE)
    {
      GIFunctionInfo *getter = g_property_info_get_getter (info);

      if (getter != NULL)
        {
          xml_printf (file, " getter=\"%s\"", g_base_info_get_name ((GIBaseInfo *) getter));
          g_base_info_unref ((GIBaseInfo *) getter);
        }
    }

  if (flags & G_PARAM_WRITABLE)
    {
      GIFunctionInfo *setter = g_property_info_get_setter (info);

      if (setter != NULL)
        {
          xml_printf (file, " setter=\"%s\"", g_base_info_get_name ((GIBaseInfo *) setter));
          g_base_info_unref ((GIBaseInfo *) setter);
        }
    }

  write_ownership_transfer (g_property_info_get_ownership_transfer (info), file);

  write_attributes (file, (GIBaseInfo*) info);

  type = g_property_info_get_type (info);

  write_type_info (namespace, type, file);

  xml_end_element (file, "property");
}

static void
write_object_info (const gchar  *namespace,
		   GIObjectInfo *info,
		   Xml          *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  const gchar *func;
  gboolean deprecated;
  gboolean is_abstract;
  gboolean is_fundamental;
  gboolean is_final;
  GIObjectInfo *pnode;
  GIStructInfo *class_struct;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);
  is_abstract = g_object_info_get_abstract (info);
  is_fundamental = g_object_info_get_fundamental (info);
  is_final = g_object_info_get_final (info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  xml_start_element (file, "class");
  xml_printf (file, " name=\"%s\"", name);

  pnode = g_object_info_get_parent (info);
  if (pnode)
    {
      write_type_name_attribute (namespace, (GIBaseInfo *)pnode, "parent", file);
      g_base_info_unref ((GIBaseInfo *)pnode);
    }

  class_struct = g_object_info_get_class_struct (info);
  if (class_struct)
    {
      write_type_name_attribute (namespace, (GIBaseInfo*) class_struct, "glib:type-struct", file);
      g_base_info_unref ((GIBaseInfo*)class_struct);
    }

  if (is_abstract)
    xml_printf (file, " abstract=\"1\"");

  if (is_final)
    xml_printf (file, " final=\"1\"");

  xml_printf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);

  if (is_fundamental)
    xml_printf (file, " glib:fundamental=\"1\"");

  func = g_object_info_get_unref_function (info);
  if (func)
    xml_printf (file, " glib:unref-function=\"%s\"", func);

  func = g_object_info_get_ref_function (info);
  if (func)
    xml_printf (file, " glib:ref-function=\"%s\"", func);

  func = g_object_info_get_set_value_function (info);
  if (func)
    xml_printf (file, " glib:set-value-function=\"%s\"", func);

  func = g_object_info_get_get_value_function (info);
  if (func)
    xml_printf (file, " glib:get-value-function=\"%s\"", func);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_attributes (file, (GIBaseInfo*) info);

  if (g_object_info_get_n_interfaces (info) > 0)
    {
      for (i = 0; i < g_object_info_get_n_interfaces (info); i++)
	{
	  GIInterfaceInfo *imp = g_object_info_get_interface (info, i);
          xml_start_element (file, "implements");
	  write_type_name_attribute (namespace, (GIBaseInfo *)imp, "name", file);
          xml_end_element (file, "implements");
	  g_base_info_unref ((GIBaseInfo*)imp);
	}
    }

  for (i = 0; i < g_object_info_get_n_fields (info); i++)
    {
      GIFieldInfo *field = g_object_info_get_field (info, i);
      write_field_info (namespace, field, NULL, file);
      g_base_info_unref ((GIBaseInfo *)field);
    }

  for (i = 0; i < g_object_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_object_info_get_method (info, i);
      write_function_info (namespace, function, file);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  for (i = 0; i < g_object_info_get_n_properties (info); i++)
    {
      GIPropertyInfo *prop = g_object_info_get_property (info, i);
      write_property_info (namespace, prop, file);
      g_base_info_unref ((GIBaseInfo *)prop);
    }

  for (i = 0; i < g_object_info_get_n_signals (info); i++)
    {
      GISignalInfo *signal = g_object_info_get_signal (info, i);
      write_signal_info (namespace, signal, file);
      g_base_info_unref ((GIBaseInfo *)signal);
    }

  for (i = 0; i < g_object_info_get_n_vfuncs (info); i++)
    {
      GIVFuncInfo *vfunc = g_object_info_get_vfunc (info, i);
      write_vfunc_info (namespace, vfunc, file);
      g_base_info_unref ((GIBaseInfo *)vfunc);
    }

  for (i = 0; i < g_object_info_get_n_constants (info); i++)
    {
      GIConstantInfo *constant = g_object_info_get_constant (info, i);
      write_constant_info (namespace, constant, file);
      g_base_info_unref ((GIBaseInfo *)constant);
    }

  xml_end_element (file, "class");
}

static void
write_interface_info (const gchar     *namespace,
		      GIInterfaceInfo *info,
		      Xml             *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  GIStructInfo *class_struct;
  gboolean deprecated;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  xml_start_element (file, "interface");
  xml_printf (file, " name=\"%s\" glib:type-name=\"%s\" glib:get-type=\"%s\"",
	     name, type_name, type_init);

  class_struct = g_interface_info_get_iface_struct (info);
  if (class_struct)
    {
      write_type_name_attribute (namespace, (GIBaseInfo*) class_struct, "glib:type-struct", file);
      g_base_info_unref ((GIBaseInfo*)class_struct);
    }

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_attributes (file, (GIBaseInfo*) info);

  if (g_interface_info_get_n_prerequisites (info) > 0)
    {
      for (i = 0; i < g_interface_info_get_n_prerequisites (info); i++)
	{
	  GIBaseInfo *req = g_interface_info_get_prerequisite (info, i);

	  xml_start_element (file, "prerequisite");
	  write_type_name_attribute (namespace, req, "name", file);

          xml_end_element_unchecked (file);
	  g_base_info_unref (req);
	}
    }

  for (i = 0; i < g_interface_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_interface_info_get_method (info, i);
      write_function_info (namespace, function, file);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  for (i = 0; i < g_interface_info_get_n_properties (info); i++)
    {
      GIPropertyInfo *prop = g_interface_info_get_property (info, i);
      write_property_info (namespace, prop, file);
      g_base_info_unref ((GIBaseInfo *)prop);
    }

  for (i = 0; i < g_interface_info_get_n_signals (info); i++)
    {
      GISignalInfo *signal = g_interface_info_get_signal (info, i);
      write_signal_info (namespace, signal, file);
      g_base_info_unref ((GIBaseInfo *)signal);
    }

  for (i = 0; i < g_interface_info_get_n_vfuncs (info); i++)
    {
      GIVFuncInfo *vfunc = g_interface_info_get_vfunc (info, i);
      write_vfunc_info (namespace, vfunc, file);
      g_base_info_unref ((GIBaseInfo *)vfunc);
    }

  for (i = 0; i < g_interface_info_get_n_constants (info); i++)
    {
      GIConstantInfo *constant = g_interface_info_get_constant (info, i);
      write_constant_info (namespace, constant, file);
      g_base_info_unref ((GIBaseInfo *)constant);
    }

  xml_end_element (file, "interface");
}

static void
write_union_info (const gchar *namespace,
		  GIUnionInfo *info,
		  Xml         *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  const gchar *func;
  gboolean deprecated;
  gint i;
  gint size;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);

  xml_start_element (file, "union");
  xml_printf (file, " name=\"%s\"", name);

  if (type_name)
    xml_printf (file, " type-name=\"%s\" get-type=\"%s\"", type_name, type_init);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  size = g_union_info_get_size (info);
  if (file->show_all && size >= 0)
    xml_printf (file, " size=\"%d\"", size);

  func = g_union_info_get_copy_function (info);
  if (func)
    xml_printf (file, " copy-function=\"%s\"", func);

  func = g_union_info_get_free_function (info);
  if (func)
    xml_printf (file, " free-function=\"%s\"", func);

  write_attributes (file, (GIBaseInfo*) info);

  if (g_union_info_is_discriminated (info))
    {
      gint offset;
      GITypeInfo *type;

      offset = g_union_info_get_discriminator_offset (info);
      type = g_union_info_get_discriminator_type (info);

      xml_start_element (file, "discriminator");
      xml_printf (file, " offset=\"%d\" type=\"", offset);
      write_type_info (namespace, type, file);
      xml_end_element (file, "discriminator");
      g_base_info_unref ((GIBaseInfo *)type);
    }

  for (i = 0; i < g_union_info_get_n_fields (info); i++)
    {
      GIFieldInfo *field = g_union_info_get_field (info, i);
      GIConstantInfo *constant = g_union_info_get_discriminator (info, i);
      write_field_info (namespace, field, constant, file);
      g_base_info_unref ((GIBaseInfo *)field);
      if (constant)
	g_base_info_unref ((GIBaseInfo *)constant);
    }

  for (i = 0; i < g_union_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_union_info_get_method (info, i);
      write_function_info (namespace, function, file);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  xml_end_element (file, "union");
}


/**
 * gir_writer_write:
 * @filename: filename to write to
 * @namespace: GIR namespace to write
 * @needs_prefix: if the filename needs prefixing
 * @show_all: if field size calculations should be included
 *
 * Writes the output of a typelib represented by @namespace
 * into a GIR xml file named @filename.
 */
void
gir_writer_write (const char *filename,
		  const char *namespace,
		  gboolean    needs_prefix,
		  gboolean    show_all)
{
  FILE *ofile;
  gint i, j;
  char **dependencies;
  GIRepository *repository;
  Xml *xml;

  repository = g_irepository_get_default ();

  if (filename == NULL)
    ofile = stdout;
  else
    {
      gchar *full_filename;

      if (needs_prefix)
	full_filename = g_strdup_printf ("%s-%s", namespace, filename);
      else
	full_filename = g_strdup (filename);
      ofile = g_fopen (filename, "w");

      if (ofile == NULL)
	{
	  g_fprintf (stderr, "failed to open '%s': %s\n",
		     full_filename, g_strerror (errno));
	  g_free (full_filename);

	  return;
	}

      g_free (full_filename);
    }

  xml = xml_open (ofile);
  xml->show_all = show_all;
  xml_printf (xml, "<?xml version=\"1.0\"?>\n");
  xml_start_element (xml, "repository");
  xml_printf (xml, " version=\"1.0\"\n"
	      "            xmlns=\"http://www.gtk.org/introspection/core/1.0\"\n"
	      "            xmlns:c=\"http://www.gtk.org/introspection/c/1.0\"\n"
	      "            xmlns:glib=\"http://www.gtk.org/introspection/glib/1.0\"");

  dependencies = g_irepository_get_immediate_dependencies (repository,
                                                           namespace);
  if (dependencies != NULL)
    {
      for (i = 0; dependencies[i]; i++)
	{
	  char **parts = g_strsplit (dependencies[i], "-", 2);
	  xml_start_element (xml, "include");
	  xml_printf (xml, " name=\"%s\" version=\"%s\"", parts[0], parts[1]);
	  xml_end_element (xml, "include");
	  g_strfreev (parts);
	}
    }

  if (TRUE)
    {
      const gchar *shared_library;
      const gchar *c_prefix;
      const char *ns = namespace;
      const char *version;
      gint n_infos;

      version = g_irepository_get_version (repository, ns);

      shared_library = g_irepository_get_shared_library (repository, ns);
      c_prefix = g_irepository_get_c_prefix (repository, ns);
      xml_start_element (xml, "namespace");
      xml_printf (xml, " name=\"%s\" version=\"%s\"", ns, version);
      if (shared_library)
        xml_printf (xml, " shared-library=\"%s\"", shared_library);
      if (c_prefix)
        xml_printf (xml, " c:prefix=\"%s\"", c_prefix);

      n_infos = g_irepository_get_n_infos (repository, ns);
      for (j = 0; j < n_infos; j++)
	{
	  GIBaseInfo *info = g_irepository_get_info (repository, ns, j);
	  switch (g_base_info_get_type (info))
	    {
	    case GI_INFO_TYPE_FUNCTION:
	      write_function_info (ns, (GIFunctionInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_CALLBACK:
	      write_callback_info (ns, (GICallbackInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_STRUCT:
	    case GI_INFO_TYPE_BOXED:
	      write_struct_info (ns, (GIStructInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_UNION:
	      write_union_info (ns, (GIUnionInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_ENUM:
	    case GI_INFO_TYPE_FLAGS:
	      write_enum_info (ns, (GIEnumInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_CONSTANT:
	      write_constant_info (ns, (GIConstantInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_OBJECT:
	      write_object_info (ns, (GIObjectInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_INTERFACE:
	      write_interface_info (ns, (GIInterfaceInfo *)info, xml);
	      break;

	    default:
	      g_error ("unknown info type %d\n", g_base_info_get_type (info));
	    }

	  g_base_info_unref (info);
	}

      xml_end_element (xml, "namespace");
    }

  xml_end_element (xml, "repository");

  xml_free (xml);
}
