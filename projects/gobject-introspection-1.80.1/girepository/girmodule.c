/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Typelib creation
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "girmodule.h"
#include "gitypelib-internal.h"
#include "girnode.h"

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

#define NUM_SECTIONS 2

GIrModule *
_g_ir_module_new (const gchar *name,
		  const gchar *version,
		  const gchar *shared_library,
		  const gchar *c_prefix)
{
  GIrModule *module;

  module = g_slice_new0 (GIrModule);

  module->name = g_strdup (name);
  module->version = g_strdup (version);
  if (shared_library)
      module->shared_library = g_strdup (shared_library);
  else
      module->shared_library = NULL;
  module->c_prefix = g_strdup (c_prefix);
  module->dependencies = NULL;
  module->entries = NULL;

  module->include_modules = NULL;
  module->aliases = NULL;

  return module;
}

void
_g_ir_module_free (GIrModule *module)
{
  GList *e;

  g_free (module->name);

  for (e = module->entries; e; e = e->next)
    _g_ir_node_free ((GIrNode *)e->data);

  g_list_free (module->entries);
  /* Don't free dependencies, we inherit that from the parser */

  g_list_free (module->include_modules);

  g_hash_table_destroy (module->aliases);
  g_hash_table_destroy (module->pointer_structures);
  g_hash_table_destroy (module->disguised_structures);

  g_slice_free (GIrModule, module);
}

/**
 * _g_ir_module_fatal:
 * @build: Current build
 * @line: Origin line number, or 0 if unknown
 * @msg: printf-format string
 * @args: Remaining arguments
 *
 * Report a fatal error, then exit.
 */
void
_g_ir_module_fatal (GIrTypelibBuild  *build,
		    guint       line,
		    const char *msg,
		    ...)
{
  GString *context;
  char *formatted;
  GList *link;

  va_list args;

  va_start (args, msg);

  formatted = g_strdup_vprintf (msg, args);

  context = g_string_new ("");
  if (line > 0)
    g_string_append_printf (context, "%d: ", line);
  if (build->stack)
    g_string_append (context, "In ");
  for (link = g_list_last (build->stack); link; link = link->prev)
    {
      GIrNode *node = link->data;
      const char *name = node->name;
      if (name)
	g_string_append (context, name);
      if (link->prev)
	g_string_append (context, ".");
    }
  if (build->stack)
    g_string_append (context, ": ");

  g_printerr ("%s-%s.gir:%serror: %s\n", build->module->name, 
	      build->module->version,
	      context->str, formatted);
  g_string_free (context, TRUE);

  exit (1);

  va_end (args);
}

static void
add_alias_foreach (gpointer key,
		   gpointer value,
		   gpointer data)
{
  GIrModule *module = data;

  g_hash_table_replace (module->aliases, g_strdup (key), g_strdup (value));
}

static void
add_pointer_structure_foreach (gpointer key,
                               gpointer value,
                               gpointer data)
{
  GIrModule *module = data;

  g_hash_table_replace (module->pointer_structures, g_strdup (key), value);
}

static void
add_disguised_structure_foreach (gpointer key,
				 gpointer value,
				 gpointer data)
{
  GIrModule *module = data;

  g_hash_table_replace (module->disguised_structures, g_strdup (key), value);
}

void
_g_ir_module_add_include_module (GIrModule  *module,
				 GIrModule  *include_module)
{
  module->include_modules = g_list_prepend (module->include_modules,
					    include_module);

  g_hash_table_foreach (include_module->aliases,
			add_alias_foreach,
			module);

  g_hash_table_foreach (include_module->pointer_structures,
			add_pointer_structure_foreach,
			module);
  g_hash_table_foreach (include_module->disguised_structures,
			add_disguised_structure_foreach,
			module);
}

struct AttributeWriteData
{
  guint count;
  guchar *databuf;
  GIrNode *node;
  GHashTable *strings;
  guint32 *offset;
  guint32 *offset2;
};

static void
write_attribute (gpointer key, gpointer value, gpointer datap)
{
  struct AttributeWriteData *data = datap;
  guint32 old_offset = *(data->offset);
  AttributeBlob *blob = (AttributeBlob*)&(data->databuf[old_offset]);

  *(data->offset) += sizeof (AttributeBlob);

  blob->offset = data->node->offset;
  blob->name = _g_ir_write_string ((const char*) key, data->strings, data->databuf, data->offset2);
  blob->value = _g_ir_write_string ((const char*) value, data->strings, data->databuf, data->offset2);

  data->count++;
}

static guint
write_attributes (GIrModule *module,
                   GIrNode   *node,
                   GHashTable *strings,
                   guchar    *data,
                   guint32   *offset,
                   guint32   *offset2)
{
  struct AttributeWriteData wdata;
  wdata.count = 0;
  wdata.databuf = data;
  wdata.node = node;
  wdata.offset = offset;
  wdata.offset2 = offset2;
  wdata.strings = strings;

  g_hash_table_foreach (node->attributes, write_attribute, &wdata);

  return wdata.count;
}

static gint
node_cmp_offset_func (gconstpointer a,
                      gconstpointer b)
{
  const GIrNode *na = a;
  const GIrNode *nb = b;
  return na->offset - nb->offset;
}

static void
alloc_section (guint8 *data, SectionType section_id, guint32 offset)
{
  int i;
  Header *header = (Header*)data;
  Section *section_data = (Section*)&data[header->sections];

  g_assert (section_id != GI_SECTION_END);

  for (i = 0; i < NUM_SECTIONS; i++)
    {
      if (section_data->id == GI_SECTION_END)
	{
	  section_data->id = section_id;
	  section_data->offset = offset;
	  return;
	}
      section_data++;
    }
  g_assert_not_reached ();
}

static guint8*
add_directory_index_section (guint8 *data, GIrModule *module, guint32 *offset2)
{
  DirEntry *entry;
  Header *header = (Header*)data;
  GITypelibHashBuilder *dirindex_builder;
  guint i, n_interfaces;
  guint16 required_size;
  guint32 new_offset;

  dirindex_builder = _gi_typelib_hash_builder_new ();

  n_interfaces = ((Header *)data)->n_local_entries;

  for (i = 0; i < n_interfaces; i++)
    {
      const char *str;
      entry = (DirEntry *)&data[header->directory + (i * header->entry_blob_size)];
      str = (const char *) (&data[entry->name]);
      _gi_typelib_hash_builder_add_string (dirindex_builder, str, i);
    }

  if (!_gi_typelib_hash_builder_prepare (dirindex_builder))
    {
      /* This happens if CMPH couldn't create a perfect hash.  So
       * we just punt and leave no directory index section.
       */
      _gi_typelib_hash_builder_destroy (dirindex_builder);
      return data;
    }

  alloc_section (data, GI_SECTION_DIRECTORY_INDEX, *offset2);

  required_size = _gi_typelib_hash_builder_get_buffer_size (dirindex_builder);
  required_size = ALIGN_VALUE (required_size, 4);

  new_offset = *offset2 + required_size;

  data = g_realloc (data, new_offset);

  _gi_typelib_hash_builder_pack (dirindex_builder, ((guint8*)data) + *offset2, required_size);

  *offset2 = new_offset;

  _gi_typelib_hash_builder_destroy (dirindex_builder);
  return data;
}

GITypelib *
_g_ir_module_build_typelib (GIrModule  *module)
{
  GError *error = NULL;
  GITypelib *typelib;
  gsize length;
  guint i;
  GList *e;
  Header *header;
  DirEntry *entry;
  guint32 header_size;
  guint32 dir_size;
  guint32 n_entries;
  guint32 n_local_entries;
  guint32 size, offset, offset2, old_offset;
  GHashTable *strings;
  GHashTable *types;
  GList *nodes_with_attributes;
  char *dependencies;
  guchar *data;
  Section *section;

  header_size = ALIGN_VALUE (sizeof (Header), 4);
  n_local_entries = g_list_length (module->entries);

  /* Serialize dependencies into one string; this is convenient
   * and not a major change to the typelib format. */
  {
    GString *dependencies_str = g_string_new ("");
    GList *link;
    for (link = module->dependencies; link; link = link->next)
      {
	const char *dependency = link->data;
	if (!strcmp (dependency, module->name))
	  continue;
	g_string_append (dependencies_str, dependency);
	if (link->next)
	  g_string_append_c (dependencies_str, '|');
      }
    dependencies = g_string_free (dependencies_str, FALSE);
    if (!dependencies[0])
      {
	g_free (dependencies);
	dependencies = NULL;
      }
  }

 restart:
  _g_irnode_init_stats ();
  strings = g_hash_table_new (g_str_hash, g_str_equal);
  types = g_hash_table_new (g_str_hash, g_str_equal);
  nodes_with_attributes = NULL;
  n_entries = g_list_length (module->entries);

  g_message ("%d entries (%d local), %d dependencies\n", n_entries, n_local_entries,
	     g_list_length (module->dependencies));

  dir_size = n_entries * sizeof (DirEntry);
  size = header_size + dir_size;

  size += ALIGN_VALUE (strlen (module->name) + 1, 4);

  for (e = module->entries; e; e = e->next)
    {
      GIrNode *node = e->data;

      size += _g_ir_node_get_full_size (node);

      /* Also reset the offset here */
      node->offset = 0;
    }

  /* Adjust size for strings allocated in header below specially */
  size += ALIGN_VALUE (strlen (module->name) + 1, 4);
  if (module->shared_library)
    size += ALIGN_VALUE (strlen (module->shared_library) + 1, 4);
  if (dependencies != NULL)
    size += ALIGN_VALUE (strlen (dependencies) + 1, 4);
  if (module->c_prefix != NULL)
    size += ALIGN_VALUE (strlen (module->c_prefix) + 1, 4);

  size += sizeof (Section) * NUM_SECTIONS;

  g_message ("allocating %d bytes (%d header, %d directory, %d entries)\n",
	  size, header_size, dir_size, size - header_size - dir_size);

  data = g_malloc0 (size);

  /* fill in header */
  header = (Header *)data;
  memcpy (header, G_IR_MAGIC, 16);
  header->major_version = 4;
  header->minor_version = 0;
  header->reserved = 0;
  header->n_entries = n_entries;
  header->n_local_entries = n_local_entries;
  header->n_attributes = 0;
  header->attributes = 0; /* filled in later */
  /* NOTE: When writing strings to the typelib here, you should also update
   * the size calculations above.
   */
  if (dependencies != NULL)
    header->dependencies = _g_ir_write_string (dependencies, strings, data, &header_size);
  else
    header->dependencies = 0;
  header->size = 0; /* filled in later */
  header->namespace = _g_ir_write_string (module->name, strings, data, &header_size);
  header->nsversion = _g_ir_write_string (module->version, strings, data, &header_size);
  header->shared_library = (module->shared_library?
                             _g_ir_write_string (module->shared_library, strings, data, &header_size)
                             : 0);
  if (module->c_prefix != NULL)
    header->c_prefix = _g_ir_write_string (module->c_prefix, strings, data, &header_size);
  else
    header->c_prefix = 0;
  header->entry_blob_size = sizeof (DirEntry);
  header->function_blob_size = sizeof (FunctionBlob);
  header->callback_blob_size = sizeof (CallbackBlob);
  header->signal_blob_size = sizeof (SignalBlob);
  header->vfunc_blob_size = sizeof (VFuncBlob);
  header->arg_blob_size = sizeof (ArgBlob);
  header->property_blob_size = sizeof (PropertyBlob);
  header->field_blob_size = sizeof (FieldBlob);
  header->value_blob_size = sizeof (ValueBlob);
  header->constant_blob_size = sizeof (ConstantBlob);
  header->error_domain_blob_size = 16; /* No longer used */
  header->attribute_blob_size = sizeof (AttributeBlob);
  header->signature_blob_size = sizeof (SignatureBlob);
  header->enum_blob_size = sizeof (EnumBlob);
  header->struct_blob_size = sizeof (StructBlob);
  header->object_blob_size = sizeof(ObjectBlob);
  header->interface_blob_size = sizeof (InterfaceBlob);
  header->union_blob_size = sizeof (UnionBlob);

  offset2 = ALIGN_VALUE (header_size, 4);
  header->sections = offset2;

  /* Initialize all the sections to _END/0; we fill them in later using
   * alloc_section().  (Right now there's just the directory index
   * though, note)
   */
  for (i = 0; i < NUM_SECTIONS; i++)
    {
      section = (Section*) &data[offset2];
      section->id = GI_SECTION_END;
      section->offset = 0;
      offset2 += sizeof(Section);
    }
  header->directory = offset2;

  /* fill in directory and content */
  entry = (DirEntry *)&data[header->directory];

  offset2 += dir_size;

  for (e = module->entries, i = 0; e; e = e->next, i++)
    {
      GIrTypelibBuild build;
      GIrNode *node = e->data;

      if (strchr (node->name, '.'))
        {
	  g_error ("Names may not contain '.'");
	}

      /* we picked up implicit xref nodes, start over */
      if (i == n_entries)
	{
	  GList *link;
	  g_message ("Found implicit cross references, starting over");

	  g_hash_table_destroy (strings);
	  g_hash_table_destroy (types);

	  /* Reset the cached offsets */
	  for (link = nodes_with_attributes; link; link = link->next)
	    ((GIrNode *) link->data)->offset = 0;

	  g_list_free (nodes_with_attributes);
	  strings = NULL;

	  g_free (data);
	  data = NULL;

	  goto restart;
	}

      offset = offset2;

      if (node->type == G_IR_NODE_XREF)
	{
	  const char *namespace = ((GIrNodeXRef*)node)->namespace;

	  entry->blob_type = 0;
	  entry->local = FALSE;
	  entry->offset = _g_ir_write_string (namespace, strings, data, &offset2);
	  entry->name = _g_ir_write_string (node->name, strings, data, &offset2);
	}
      else
	{
	  old_offset = offset;
	  offset2 = offset + _g_ir_node_get_size (node);

	  entry->blob_type = node->type;
	  entry->local = TRUE;
	  entry->offset = offset;
	  entry->name = _g_ir_write_string (node->name, strings, data, &offset2);

	  memset (&build, 0, sizeof (build));
	  build.module = module;
	  build.strings = strings;
	  build.types = types;
	  build.nodes_with_attributes = nodes_with_attributes;
	  build.n_attributes = header->n_attributes;
	  build.data = data;
	  _g_ir_node_build_typelib (node, NULL, &build, &offset, &offset2, NULL);

	  nodes_with_attributes = build.nodes_with_attributes;
	  header->n_attributes = build.n_attributes;

	  if (offset2 > old_offset + _g_ir_node_get_full_size (node))
	    g_error ("left a hole of %d bytes\n", offset2 - old_offset - _g_ir_node_get_full_size (node));
	}

      entry++;
    }

  /* GIBaseInfo expects the AttributeBlob array to be sorted on the field (offset) */
  nodes_with_attributes = g_list_sort (nodes_with_attributes, node_cmp_offset_func);

  g_message ("header: %d entries, %d attributes", header->n_entries, header->n_attributes);

  _g_irnode_dump_stats ();

  /* Write attributes after the blobs */
  offset = offset2;
  header->attributes = offset;
  offset2 = offset + header->n_attributes * header->attribute_blob_size;

  for (e = nodes_with_attributes; e; e = e->next)
    {
      GIrNode *node = e->data;
      write_attributes (module, node, strings, data, &offset, &offset2);
    }

  g_message ("reallocating to %d bytes", offset2);

  data = g_realloc (data, offset2);
  header = (Header*) data;

  data = add_directory_index_section (data, module, &offset2);
  header = (Header *)data;

  length = header->size = offset2;
  typelib = g_typelib_new_from_memory (data, length, &error);
  if (!typelib)
    {
      g_error ("error building typelib: %s",
	       error->message);
    }

  g_hash_table_destroy (strings);
  g_hash_table_destroy (types);
  g_list_free (nodes_with_attributes);

  return typelib;
}

