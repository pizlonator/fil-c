/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Dump introspection data
 *
 * Copyright (C) 2008 Colin Walters <walters@verbum.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

/* This file is both compiled into libgirepository.so, and installed
 * on the filesystem.  But for the dumper, we want to avoid linking
 * to libgirepository; see
 * https://bugzilla.gnome.org/show_bug.cgi?id=630342
 */
#ifdef G_IREPOSITORY_COMPILATION
#include "config.h"
#include "girepository.h"
#endif

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Analogue of g_output_stream_write_all(). */
static gboolean
write_all (FILE          *out,
           const void    *buffer,
           gsize          count,
           gsize         *bytes_written,
           GError       **error)
{
  size_t ret;

  ret = fwrite (buffer, 1, count, out);

  if (bytes_written != NULL)
    *bytes_written = ret;

  if (ret < count)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Failed to write to file");
      return FALSE;
    }

  return TRUE;
}

/* Analogue of g_data_input_stream_read_line(). */
static char *
read_line (FILE   *input,
           size_t *len_out)
{
  GByteArray *buffer = g_byte_array_new ();
  const guint8 nul = '\0';

  while (TRUE)
    {
      size_t ret;
      guint8 byte;

      ret = fread (&byte, 1, 1, input);
      if (ret == 0)
        break;

      if (byte == '\n')
        break;

      g_byte_array_append (buffer, &byte, 1);
    }

  g_byte_array_append (buffer, &nul, 1);

  if (len_out != NULL)
    *len_out = buffer->len - 1;  /* don’t include terminating nul */

  return (char *) g_byte_array_free (buffer, FALSE);
}

static void
escaped_printf (FILE *out, const char *fmt, ...) G_GNUC_PRINTF (2, 3);

static void
escaped_printf (FILE *out, const char *fmt, ...)
{
  char *str;
  va_list args;
  gsize written;
  GError *error = NULL;

  va_start (args, fmt);

  str = g_markup_vprintf_escaped (fmt, args);
  if (!write_all (out, str, strlen (str), &written, &error))
    {
      g_critical ("failed to write to iochannel: %s", error->message);
      g_clear_error (&error);
    }
  g_free (str);

  va_end (args);
}

static void
goutput_write (FILE *out, const char *str)
{
  gsize written;
  GError *error = NULL;
  if (!write_all (out, str, strlen (str), &written, &error))
    {
      g_critical ("failed to write to iochannel: %s", error->message);
      g_clear_error (&error);
    }
}

typedef GType (*GetTypeFunc)(void);
typedef GQuark (*ErrorQuarkFunc)(void);

static GType
invoke_get_type (GModule *self, const char *symbol, GError **error)
{
  GetTypeFunc sym;
  GType ret;

  if (!g_module_symbol (self, symbol, (void**)&sym))
    {
      g_set_error (error,
		   G_FILE_ERROR,
		   G_FILE_ERROR_FAILED,
		   "Failed to find symbol '%s'", symbol);
      return G_TYPE_INVALID;
    }

  ret = sym ();
  if (ret == G_TYPE_INVALID)
    {
      g_set_error (error,
		   G_FILE_ERROR,
		   G_FILE_ERROR_FAILED,
		   "Function '%s' returned G_TYPE_INVALID", symbol);
    }
  return ret;
}

static GQuark
invoke_error_quark (GModule *self, const char *symbol, GError **error)
{
  ErrorQuarkFunc sym;

  if (!g_module_symbol (self, symbol, (void**)&sym))
    {
      g_set_error (error,
		   G_FILE_ERROR,
		   G_FILE_ERROR_FAILED,
		   "Failed to find symbol '%s'", symbol);
      return G_TYPE_INVALID;
    }

  return sym ();
}

static char *
value_transform_to_string (const GValue *value)
{
  GValue tmp = G_VALUE_INIT;
  char *s = NULL;

  g_value_init (&tmp, G_TYPE_STRING);

  if (g_value_transform (value, &tmp))
    {
      const char *str = g_value_get_string (&tmp);

      if (str != NULL)
        s = g_strescape (str, NULL);
    }

  g_value_unset (&tmp);

  return s;
}

/* A simpler version of g_strdup_value_contents(), but with stable
 * output and less complex semantics
 */
static char *
value_to_string (const GValue *value)
{
  if (value == NULL)
    return NULL;

  if (G_VALUE_HOLDS_STRING (value))
    {
      const char *s = g_value_get_string (value);

      if (s == NULL)
        return g_strdup ("NULL");

      return g_strescape (s, NULL);
    }
  else
    {
      GType value_type = G_VALUE_TYPE (value);

      switch (G_TYPE_FUNDAMENTAL (value_type))
        {
        case G_TYPE_BOXED:
          if (g_value_get_boxed (value) == NULL)
            return NULL;
          else
            return value_transform_to_string (value);
          break;

        case G_TYPE_OBJECT:
          if (g_value_get_object (value) == NULL)
            return NULL;
          else
            return value_transform_to_string (value);
          break;

        case G_TYPE_POINTER:
          return NULL;

        default:
          return value_transform_to_string (value);
        }
    }

  return NULL;
}

static void
dump_properties (GType type, FILE *out)
{
  guint i;
  guint n_properties = 0;
  GParamSpec **props;

  if (G_TYPE_FUNDAMENTAL (type) == G_TYPE_OBJECT)
    {
      GObjectClass *klass;
      klass = g_type_class_ref (type);
      props = g_object_class_list_properties (klass, &n_properties);
    }
  else
    {
      void *klass;
      klass = g_type_default_interface_ref (type);
      props = g_object_interface_list_properties (klass, &n_properties);
    }

  for (i = 0; i < n_properties; i++)
    {
      GParamSpec *prop;

      prop = props[i];
      if (prop->owner_type != type)
	continue;

      const GValue *v = g_param_spec_get_default_value (prop);
      char *default_value = value_to_string (v);

      if (v != NULL && default_value != NULL)
        {
          escaped_printf (out, "    <property name=\"%s\" type=\"%s\" flags=\"%d\" default-value=\"%s\"/>\n",
                          prop->name,
                          g_type_name (prop->value_type),
                          prop->flags,
                          default_value);
        }
      else
        {
          escaped_printf (out, "    <property name=\"%s\" type=\"%s\" flags=\"%d\"/>\n",
                          prop->name,
                          g_type_name (prop->value_type),
                          prop->flags);
        }

      g_free (default_value);
    }

  g_free (props);
}

static void
dump_signals (GType type, FILE *out)
{
  guint i;
  guint n_sigs;
  guint *sig_ids;

  sig_ids = g_signal_list_ids (type, &n_sigs);
  for (i = 0; i < n_sigs; i++)
    {
      guint sigid;
      GSignalQuery query;
      guint j;

      sigid = sig_ids[i];
      g_signal_query (sigid, &query);

      escaped_printf (out, "    <signal name=\"%s\" return=\"%s\"",
		      query.signal_name, g_type_name (query.return_type));

      if (query.signal_flags & G_SIGNAL_RUN_FIRST)
        escaped_printf (out, " when=\"first\"");
      else if (query.signal_flags & G_SIGNAL_RUN_LAST)
        escaped_printf (out, " when=\"last\"");
      else if (query.signal_flags & G_SIGNAL_RUN_CLEANUP)
        escaped_printf (out, " when=\"cleanup\"");
      else if (query.signal_flags & G_SIGNAL_MUST_COLLECT)
        escaped_printf (out, " when=\"must-collect\"");
      if (query.signal_flags & G_SIGNAL_NO_RECURSE)
        escaped_printf (out, " no-recurse=\"1\"");

      if (query.signal_flags & G_SIGNAL_DETAILED)
        escaped_printf (out, " detailed=\"1\"");

      if (query.signal_flags & G_SIGNAL_ACTION)
        escaped_printf (out, " action=\"1\"");

      if (query.signal_flags & G_SIGNAL_NO_HOOKS)
        escaped_printf (out, " no-hooks=\"1\"");

      goutput_write (out, ">\n");

      for (j = 0; j < query.n_params; j++)
	{
	  escaped_printf (out, "      <param type=\"%s\"/>\n",
			  g_type_name (query.param_types[j]));
	}
      goutput_write (out, "    </signal>\n");
    }
  g_free (sig_ids);
}

static void
dump_object_type (GType type, const char *symbol, FILE *out)
{
  guint n_interfaces;
  guint i;
  GType *interfaces;

  escaped_printf (out, "  <class name=\"%s\" get-type=\"%s\"",
		  g_type_name (type), symbol);
  if (type != G_TYPE_OBJECT)
    {
      GString *parent_str;
      GType parent;
      gboolean first = TRUE;

      parent = g_type_parent (type);
      parent_str = g_string_new ("");
      while (parent != G_TYPE_INVALID)
        {
          if (first)
            first = FALSE;
          else
            g_string_append_c (parent_str, ',');
          g_string_append (parent_str, g_type_name (parent));
          parent = g_type_parent (parent);
        }

      escaped_printf (out, " parents=\"%s\"", parent_str->str);

      g_string_free (parent_str, TRUE);
    }

  if (G_TYPE_IS_ABSTRACT (type))
    escaped_printf (out, " abstract=\"1\"");

  if (G_TYPE_IS_FINAL (type))
    escaped_printf (out, " final=\"1\"");

  goutput_write (out, ">\n");

  interfaces = g_type_interfaces (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    {
      GType itype = interfaces[i];
      escaped_printf (out, "    <implements name=\"%s\"/>\n",
		      g_type_name (itype));
    }
  g_free (interfaces);

  dump_properties (type, out);
  dump_signals (type, out);
  goutput_write (out, "  </class>\n");
}

static void
dump_interface_type (GType type, const char *symbol, FILE *out)
{
  guint n_interfaces;
  guint i;
  GType *interfaces;

  escaped_printf (out, "  <interface name=\"%s\" get-type=\"%s\">\n",
		  g_type_name (type), symbol);

  interfaces = g_type_interface_prerequisites (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    {
      GType itype = interfaces[i];
      if (itype == G_TYPE_OBJECT)
	{
	  /* Treat this as implicit for now; in theory GInterfaces are
	   * supported on things like GstMiniObject, but right now
	   * the introspection system only supports GObject.
	   * http://bugzilla.gnome.org/show_bug.cgi?id=559706
	   */
	  continue;
	}
      escaped_printf (out, "    <prerequisite name=\"%s\"/>\n",
		      g_type_name (itype));
    }
  g_free (interfaces);

  dump_properties (type, out);
  dump_signals (type, out);
  goutput_write (out, "  </interface>\n");
}

static void
dump_boxed_type (GType type, const char *symbol, FILE *out)
{
  escaped_printf (out, "  <boxed name=\"%s\" get-type=\"%s\"/>\n",
		  g_type_name (type), symbol);
}

static void
dump_flags_type (GType type, const char *symbol, FILE *out)
{
  guint i;
  GFlagsClass *klass;

  klass = g_type_class_ref (type);
  escaped_printf (out, "  <flags name=\"%s\" get-type=\"%s\">\n",
		  g_type_name (type), symbol);

  for (i = 0; i < klass->n_values; i++)
    {
      GFlagsValue *value = &(klass->values[i]);

      escaped_printf (out, "    <member name=\"%s\" nick=\"%s\" value=\"%u\"/>\n",
		      value->value_name, value->value_nick, value->value);
    }
  goutput_write (out, "  </flags>\n");
}

static void
dump_enum_type (GType type, const char *symbol, FILE *out)
{
  guint i;
  GEnumClass *klass;

  klass = g_type_class_ref (type);
  escaped_printf (out, "  <enum name=\"%s\" get-type=\"%s\">\n",
		  g_type_name (type), symbol);

  for (i = 0; i < klass->n_values; i++)
    {
      GEnumValue *value = &(klass->values[i]);

      escaped_printf (out, "    <member name=\"%s\" nick=\"%s\" value=\"%d\"/>\n",
		      value->value_name, value->value_nick, value->value);
    }
  goutput_write (out, "  </enum>");
}

static void
dump_fundamental_type (GType type, const char *symbol, FILE *out)
{
  guint n_interfaces;
  guint i;
  GType *interfaces;
  GString *parent_str;
  GType parent;
  gboolean first = TRUE;


  escaped_printf (out, "  <fundamental name=\"%s\" get-type=\"%s\"",
		  g_type_name (type), symbol);

  if (G_TYPE_IS_ABSTRACT (type))
    escaped_printf (out, " abstract=\"1\"");

  if (G_TYPE_IS_FINAL (type))
    escaped_printf (out, " final=\"1\"");

  if (G_TYPE_IS_INSTANTIATABLE (type))
    escaped_printf (out, " instantiatable=\"1\"");

  parent = g_type_parent (type);
  parent_str = g_string_new ("");
  while (parent != G_TYPE_INVALID)
    {
      if (first)
        first = FALSE;
      else
        g_string_append_c (parent_str, ',');
      if (!g_type_name (parent))
        break;
      g_string_append (parent_str, g_type_name (parent));
      parent = g_type_parent (parent);
    }

  if (parent_str->len > 0)
    escaped_printf (out, " parents=\"%s\"", parent_str->str);
  g_string_free (parent_str, TRUE);

  goutput_write (out, ">\n");

  interfaces = g_type_interfaces (type, &n_interfaces);
  for (i = 0; i < n_interfaces; i++)
    {
      GType itype = interfaces[i];
      escaped_printf (out, "    <implements name=\"%s\"/>\n",
		      g_type_name (itype));
    }
  g_free (interfaces);
  goutput_write (out, "  </fundamental>\n");
}

static void
dump_type (GType type, const char *symbol, FILE *out)
{
  /* XXX: We need to ensure that global ancillary structures like
   * GParamSpecPool are initialized before we use the GType and
   * GObject reflection API to poke at things like properties and
   * signals; we cannot rely on GObject being instantiated at this
   * point.
   */
  GObjectClass *gobject_class = g_type_class_ref (G_TYPE_OBJECT);

  switch (g_type_fundamental (type))
    {
    case G_TYPE_OBJECT:
      dump_object_type (type, symbol, out);
      break;
    case G_TYPE_INTERFACE:
      dump_interface_type (type, symbol, out);
      break;
    case G_TYPE_BOXED:
      dump_boxed_type (type, symbol, out);
      break;
    case G_TYPE_FLAGS:
      dump_flags_type (type, symbol, out);
      break;
    case G_TYPE_ENUM:
      dump_enum_type (type, symbol, out);
      break;
    case G_TYPE_POINTER:
      /* GValue, etc.  Just skip them. */
      break;
    default:
      dump_fundamental_type (type, symbol, out);
      break;
    }

  g_type_class_unref ((GTypeClass *) gobject_class);
}

static void
dump_error_quark (GQuark quark, const char *symbol, FILE *out)
{
  escaped_printf (out, "  <error-quark function=\"%s\" domain=\"%s\"/>\n",
		  symbol, g_quark_to_string (quark));
}

/**
 * g_irepository_dump:
 * @input_filename: (type filename): Input filename (for example `input.txt`)
 * @output_filename: (type filename): Output filename (for example `output.xml`)
 * @error: a %GError
 *
 * Dump the introspection data from the types specified in @input_filename to
 * @output_filename.
 *
 * The input file should be a
 * UTF-8 Unix-line-ending text file, with each line containing either
 * `get-type:` followed by the name of a [type@GObject.Type] `_get_type`
 * function, or `error-quark:` followed by the name of an error quark function.
 * No extra whitespace is allowed.
 *
 * This function will overwrite the contents of the output file.
 *
 * Returns: true on success, false on error
 * Since: 2.80
 */
#ifndef G_IREPOSITORY_COMPILATION
static gboolean
dump_irepository (const char  *arg,
                  GError     **error) G_GNUC_UNUSED;
static gboolean
dump_irepository (const char  *arg,
                  GError     **error)
#else
gboolean
g_irepository_dump (const char  *arg,
                    GError     **error)
#endif
{
  GHashTable *output_types;
  FILE *input;
  FILE *output;
  GModule *self;
  gboolean caught_error = FALSE;

  char **args = g_strsplit (arg, ",", 2);
  if (args == NULL)
    return FALSE;

  const char *input_filename = args[0];
  const char *output_filename = args[1];

  self = g_module_open (NULL, 0);
  if (!self)
    {
      g_set_error (error,
		   G_FILE_ERROR,
		   G_FILE_ERROR_FAILED,
		   "failed to open self: %s",
		   g_module_error ());
      g_strfreev (args);
      return FALSE;
    }

  input = fopen (input_filename, "rb");
  if (input == NULL)
    {
      int saved_errno = errno;
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (saved_errno),
                   "Failed to open ‘%s’: %s", input_filename, g_strerror (saved_errno));

      g_module_close (self);
      g_strfreev (args);

      return FALSE;
    }

  output = fopen (output_filename, "wb");
  if (output == NULL)
    {
      int saved_errno = errno;
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (saved_errno),
                   "Failed to open ‘%s’: %s", output_filename, g_strerror (saved_errno));

      fclose (input);
      g_module_close (self);
      g_strfreev (args);

      return FALSE;
    }

  goutput_write (output, "<?xml version=\"1.0\"?>\n");
  goutput_write (output, "<dump>\n");

  output_types = g_hash_table_new (NULL, NULL);

  while (TRUE)
    {
      gsize len;
      char *line = read_line (input, &len);
      const char *function;

      if (line == NULL || *line == '\0')
        {
          g_free (line);
          break;
        }

      g_strchomp (line);

      if (strncmp (line, "get-type:", strlen ("get-type:")) == 0)
        {
          GType type;

          function = line + strlen ("get-type:");

          type = invoke_get_type (self, function, error);

          if (type == G_TYPE_INVALID)
            {
              g_printerr ("Invalid GType function: '%s'\n", function);
              caught_error = TRUE;
              g_free (line);
              break;
            }

          if (g_hash_table_lookup (output_types, (gpointer) type))
            goto next;
          g_hash_table_insert (output_types, (gpointer) type, (gpointer) type);

          dump_type (type, function, output);
        }
      else if (strncmp (line, "error-quark:", strlen ("error-quark:")) == 0)
        {
          GQuark quark;
          function = line + strlen ("error-quark:");
          quark = invoke_error_quark (self, function, error);

          if (quark == 0)
            {
              g_printerr ("Invalid error quark function: '%s'\n", function);
              caught_error = TRUE;
              g_free (line);
              break;
            }

          dump_error_quark (quark, function, output);
        }


    next:
      g_free (line);
    }

  g_hash_table_destroy (output_types);

  goutput_write (output, "</dump>\n");

  {
    /* Avoid overwriting an earlier set error */
    if (fclose (input) != 0 && !caught_error)
      {
        int saved_errno = errno;

        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (saved_errno),
                     "Error closing input file ‘%s’: %s", input_filename,
                     g_strerror (saved_errno));
        caught_error = TRUE;
      }

    if (fclose (output) != 0 && !caught_error)
      {
        int saved_errno = errno;

        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (saved_errno),
                     "Error closing output file ‘%s’: %s", output_filename,
                     g_strerror (saved_errno));
        caught_error = TRUE;
      }
  }

  g_strfreev (args);

  return !caught_error;
}
