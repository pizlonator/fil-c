/* gtkbuilderparser.c
 * Copyright (C) 2006-2007 Async Open Source,
 *                         Johan Dahlin <jdahlin@async.com.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkbuilderprivate.h"

#include "gtkbuildableprivate.h"
#include "gtkbuilderscopeprivate.h"
#include "gtkdebug.h"
#include "gtktypebuiltins.h"
#include "gtkversion.h"
#include "gdkprofilerprivate.h"

#include "gtkprivate.h"

#include <gio/gio.h>
#include <string.h>


typedef struct
{
  const GtkBuildableParser *last_parser;
  gpointer last_user_data;
  int last_depth;
} GtkBuildableParserStack;

static void
pop_subparser_stack (GtkBuildableParseContext *context)
{
  GtkBuildableParserStack *stack = &g_array_index (context->subparser_stack, GtkBuildableParserStack,
                                                   context->subparser_stack->len - 1);

  context->awaiting_pop = TRUE;
  context->held_user_data = context->user_data;

  context->user_data = stack->last_user_data;
  context->parser = stack->last_parser;

  g_array_set_size (context->subparser_stack, context->subparser_stack->len - 1);
}

static void
possibly_finish_subparser (GtkBuildableParseContext *context)
{
  GtkBuildableParserStack *stack;

  if (!context->subparser_stack ||
      context->subparser_stack->len == 0)
    return;

  stack = &g_array_index (context->subparser_stack, GtkBuildableParserStack,
                          context->subparser_stack->len - 1);

  if (stack->last_depth == context->tag_stack->len)
    pop_subparser_stack (context);
}

static void
proxy_start_element (GMarkupParseContext *gm_context,
                     const char          *element_name,
                     const char         **attribute_names,
                     const char         **attribute_values,
                     gpointer             user_data,
                     GError             **error)
{
  GtkBuildableParseContext *context = user_data;

  // Due to the way GMarkup works we're sure this will live until the end_element callback
  g_ptr_array_add (context->tag_stack, (char *)element_name);

  if (context->parser->start_element)
    context->parser->start_element (context, element_name,
                                    attribute_names, attribute_values,
                                    context->user_data, error);
}

static void
proxy_end_element (GMarkupParseContext *gm_context,
                   const char          *element_name,
                   gpointer             user_data,
                   GError             **error)
{
  GtkBuildableParseContext *context = user_data;

  possibly_finish_subparser (context);

  if (context->parser->end_element)
    context->parser->end_element (context, element_name, context->user_data, error);

  g_ptr_array_set_size (context->tag_stack, context->tag_stack->len - 1);
}

static void
proxy_text (GMarkupParseContext *gm_context,
            const char          *text,
            gsize                text_len,
            gpointer             user_data,
            GError             **error)
{
  GtkBuildableParseContext *context = user_data;

  if (context->parser->text)
    context->parser->text (context, text, text_len,  context->user_data, error);
}

static void
proxy_error (GMarkupParseContext *gm_context,
             GError              *error,
             gpointer             user_data)
{
  GtkBuildableParseContext *context = user_data;

  if (context->parser->error)
    context->parser->error (context, error, context->user_data);

  /* report the error all the way up to free all the user-data */

  if (!context->subparser_stack)
    return;

  while (context->subparser_stack->len > 0)
    {
      pop_subparser_stack (context);
      context->awaiting_pop = FALSE; /* already been freed */

      if (context->parser->error)
        context->parser->error (context, error, context->user_data);
    }
}

static const GMarkupParser gmarkup_parser = {
  proxy_start_element,
  proxy_end_element,
  proxy_text,
  NULL,
  proxy_error,
};

static void
gtk_buildable_parse_context_init (GtkBuildableParseContext *context,
                                  const GtkBuildableParser *parser,
                                  gpointer user_data)
{
  context->internal_callbacks = &gmarkup_parser;
  context->ctx = NULL;

  context->parser = parser;
  context->user_data = user_data;

  context->subparser_stack = NULL;
  context->tag_stack = g_ptr_array_new ();
  context->held_user_data = NULL;
  context->awaiting_pop = FALSE;
}

static void
gtk_buildable_parse_context_free (GtkBuildableParseContext *context)
{
  if (context->subparser_stack)
    g_array_unref (context->subparser_stack);

  g_ptr_array_unref (context->tag_stack);
}

static gboolean
gtk_buildable_parse_context_parse (GtkBuildableParseContext *context,
                                   const char           *text,
                                   gssize                text_len,
                                   GError              **error)
{
  gboolean res;

  if (_gtk_buildable_parser_is_precompiled (text, text_len))
    {
      res = _gtk_buildable_parser_replay_precompiled (context, text, text_len, error);
    }
  else
    {
      context->ctx = g_markup_parse_context_new (context->internal_callbacks,
                                                 G_MARKUP_TREAT_CDATA_AS_TEXT,
                                                 context, NULL);
      res = g_markup_parse_context_parse (context->ctx, text, text_len, error);
      g_markup_parse_context_free  (context->ctx);
    }

  return res;
}


/**
 * gtk_buildable_parse_context_push:
 * @context: a `GtkBuildableParseContext`
 * @parser: a `GtkBuildableParser`
 * @user_data: user data to pass to `GtkBuildableParser` functions
 *
 * Temporarily redirects markup data to a sub-parser.
 *
 * This function may only be called from the start_element handler of
 * a `GtkBuildableParser`. It must be matched with a corresponding call to
 * gtk_buildable_parse_context_pop() in the matching end_element handler
 * (except in the case that the parser aborts due to an error).
 *
 * All tags, text and other data between the matching tags is
 * redirected to the subparser given by @parser. @user_data is used
 * as the user_data for that parser. @user_data is also passed to the
 * error callback in the event that an error occurs. This includes
 * errors that occur in subparsers of the subparser.
 *
 * The end tag matching the start tag for which this call was made is
 * handled by the previous parser (which is given its own user_data)
 * which is why gtk_buildable_parse_context_pop() is provided to allow "one
 * last access" to the @user_data provided to this function. In the
 * case of error, the @user_data provided here is passed directly to
 * the error callback of the subparser and gtk_buildable_parse_context_pop()
 * should not be called. In either case, if @user_data was allocated
 * then it ought to be freed from both of these locations.
 *
 * This function is not intended to be directly called by users
 * interested in invoking subparsers. Instead, it is intended to be
 * used by the subparsers themselves to implement a higher-level
 * interface.
 *
 * For an example of how to use this, see g_markup_parse_context_push() which
 * has the same kind of API.
 **/
void
gtk_buildable_parse_context_push (GtkBuildableParseContext *context,
                                  const GtkBuildableParser *parser,
                                  gpointer                  user_data)
{
  GtkBuildableParserStack stack = { 0 };

  stack.last_parser = context->parser;
  stack.last_user_data = context->user_data;
  stack.last_depth = context->tag_stack->len; // If at end_element time we're this deep, then pop it

  context->parser = parser;
  context->user_data = user_data;

  if (!context->subparser_stack)
    context->subparser_stack = g_array_new (FALSE, FALSE, sizeof (GtkBuildableParserStack));

  g_array_append_val (context->subparser_stack, stack);
}

/**
 * gtk_buildable_parse_context_pop:
 * @context: a `GtkBuildableParseContext`
 *
 * Completes the process of a temporary sub-parser redirection.
 *
 * This function exists to collect the user_data allocated by a
 * matching call to gtk_buildable_parse_context_push(). It must be called
 * in the end_element handler corresponding to the start_element
 * handler during which gtk_buildable_parse_context_push() was called.
 * You must not call this function from the error callback -- the
 * @user_data is provided directly to the callback in that case.
 *
 * This function is not intended to be directly called by users
 * interested in invoking subparsers. Instead, it is intended to
 * be used by the subparsers themselves to implement a higher-level
 * interface.
 *
 * Returns: the user data passed to gtk_buildable_parse_context_push()
 */
gpointer
gtk_buildable_parse_context_pop (GtkBuildableParseContext *context)
{
  gpointer user_data;

  if (!context->awaiting_pop)
    possibly_finish_subparser (context);

  g_assert (context->awaiting_pop);

  context->awaiting_pop = FALSE;

  user_data = context->held_user_data;
  context->held_user_data = NULL;

  return user_data;
}

/**
 * gtk_buildable_parse_context_get_element:
 * @context: a `GtkBuildablParseContext`
 *
 * Retrieves the name of the currently open element.
 *
 * If called from the start_element or end_element handlers this will
 * give the element_name as passed to those functions. For the parent
 * elements, see gtk_buildable_parse_context_get_element_stack().
 *
 * Returns: (nullable): the name of the currently open element
 */
const char *
gtk_buildable_parse_context_get_element (GtkBuildableParseContext *context)
{
  if (context->tag_stack->len > 0)
    return g_ptr_array_index (context->tag_stack, context->tag_stack->len - 1);
  return NULL;
}

/**
 * gtk_buildable_parse_context_get_element_stack:
 * @context: a `GtkBuildableParseContext`
 *
 * Retrieves the element stack from the internal state of the parser.
 *
 * The returned `GPtrArray` is an array of strings where the last item is
 * the currently open tag (as would be returned by
 * gtk_buildable_parse_context_get_element()) and the previous item is its
 * immediate parent.
 *
 * This function is intended to be used in the start_element and
 * end_element handlers where gtk_buildable_parse_context_get_element()
 * would merely return the name of the element that is being
 * processed.
 *
 * Returns: (transfer none) (element-type utf8): the element stack, which must not be modified
 */
GPtrArray *
gtk_buildable_parse_context_get_element_stack (GtkBuildableParseContext *context)
{
  return context->tag_stack;
}

/**
 * gtk_buildable_parse_context_get_position:
 * @context: a `GtkBuildableParseContext`
 * @line_number: (out) (optional): return location for a line number
 * @char_number: (out) (optional): return location for a char-on-line number
 *
 * Retrieves the current line number and the number of the character on
 * that line. Intended for use in error messages; there are no strict
 * semantics for what constitutes the "current" line number other than
 * "the best number we could come up with for error messages."
 */
void
gtk_buildable_parse_context_get_position (GtkBuildableParseContext *context,
                                          int                 *line_number,
                                          int                 *char_number)

{
  if (context->ctx)
    g_markup_parse_context_get_position (context->ctx, line_number, char_number);
  else
    {
      if (line_number)
        *line_number = 0;
      if (char_number)
        *char_number = 0;
    }
}

static void free_property_info (PropertyInfo *info);
static void free_object_info (ObjectInfo *info);


static inline void
state_push (ParserData *data, gpointer info)
{
  g_ptr_array_add (data->stack, info);
}

static inline gpointer
state_peek (ParserData *data)
{
  if (!data->stack ||
      data->stack->len == 0)
    return NULL;

  return g_ptr_array_index (data->stack, data->stack->len - 1);
}

static inline gpointer
state_pop (ParserData *data)
{
  gpointer old = NULL;

  g_assert (data->stack);

  old = state_peek (data);
  g_assert (old);
  data->stack->len --;
  return old;
}
#define state_peek_info(data, st) ((st*)state_peek(data))
#define state_pop_info(data, st) ((st*)state_pop(data))

static void
error_missing_attribute (ParserData   *data,
                         const char   *tag,
                         const char   *attribute,
                         GError      **error)
{
  int line, col;

  gtk_buildable_parse_context_get_position (&data->ctx, &line, &col);

  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
               "%s:%d:%d <%s> requires attribute '%s'",
               data->filename, line, col, tag, attribute);
}

static void
error_invalid_tag (ParserData   *data,
                   const char   *tag,
                   const char   *expected,
                   GError      **error)
{
  int line, col;

  gtk_buildable_parse_context_get_position (&data->ctx, &line, &col);

  if (expected)
    g_set_error (error,
                 GTK_BUILDER_ERROR,
                 GTK_BUILDER_ERROR_INVALID_TAG,
                 "%s:%d:%d <%s> is not a valid tag here, expected a <%s> tag",
                 data->filename, line, col, tag, expected);
  else
    g_set_error (error,
                 GTK_BUILDER_ERROR,
                 GTK_BUILDER_ERROR_INVALID_TAG,
                 "%s:%d:%d <%s> is not a valid tag here",
                 data->filename, line, col, tag);
}

static void
error_unhandled_tag (ParserData   *data,
                     const char   *tag,
                     GError      **error)
{
  int line, col;

  gtk_buildable_parse_context_get_position (&data->ctx, &line, &col);
  g_set_error (error,
               GTK_BUILDER_ERROR,
               GTK_BUILDER_ERROR_UNHANDLED_TAG,
               "%s:%d:%d Unhandled tag: <%s>",
               data->filename, line, col, tag);
}

static GObject *
builder_construct (ParserData  *data,
                   ObjectInfo  *object_info,
                   GError     **error)
{
  GObject *object;

  g_assert (object_info != NULL);

  if (object_info->object == NULL)
    {
      object = _gtk_builder_construct (data->builder, object_info, error);
      if (!object)
        return NULL;
    }
  else
    {
      /* We're building a template, the object is already set and
       * we just want to resolve the properties at the right time
       */
      object = object_info->object;
      _gtk_builder_apply_properties (data->builder, object_info, error);
    }

  g_assert (G_IS_OBJECT (object));

  object_info->object = object;

  return object;
}

static void
parse_requires (ParserData   *data,
                const char   *element_name,
                const char **names,
                const char **values,
                GError      **error)
{
  RequiresInfo *req_info;
  const char   *library = NULL;
  const char   *version = NULL;
  char **split;
  int version_major = 0;
  int version_minor = 0;

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING, "lib", &library,
                                    G_MARKUP_COLLECT_STRING, "version", &version,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (!(split = g_strsplit (version, ".", 2)) || !split[0] || !split[1])
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_VALUE,
                   "'version' attribute has malformed value '%s'", version);
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }
  version_major = g_ascii_strtoll (split[0], NULL, 10);
  version_minor = g_ascii_strtoll (split[1], NULL, 10);
  g_strfreev (split);

  req_info = g_new0 (RequiresInfo, 1);
  req_info->library = g_strdup (library);
  req_info->major = version_major;
  req_info->minor = version_minor;
  state_push (data, req_info);
  req_info->tag_type = TAG_REQUIRES;
}

static gboolean
is_requested_object (const char *object,
                     ParserData  *data)
{
  int i;

  for (i = 0; data->requested_objects[i]; ++i)
    {
      if (g_strcmp0 (data->requested_objects[i], object) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
parse_object (GtkBuildableParseContext  *context,
              ParserData                *data,
              const char                *element_name,
              const char               **names,
              const char               **values,
              GError                   **error)
{
  ObjectInfo *object_info;
  ChildInfo* child_info;
  GType object_type = G_TYPE_INVALID;
  const char *object_class = NULL;
  const char *constructor = NULL;
  const char *type_func = NULL;
  const char *object_id = NULL;
  char *internal_id = NULL;
  int line;
  gpointer line_ptr;
  gboolean has_duplicate;

  child_info = state_peek_info (data, ChildInfo);
  if (child_info && child_info->tag_type == TAG_OBJECT)
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  /* Even though 'class' is a mandatory attribute, we don't flag its
   * absence here because it's supposed to throw
   * GTK_BUILDER_ERROR_MISSING_ATTRIBUTE, not
   * G_MARKUP_ERROR_MISSING_ATTRIBUTE. It's handled immediately
   * afterwards.
   */
  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "class", &object_class,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "constructor", &constructor,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "type-func", &type_func,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "id", &object_id,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (!object_class)
    {
      error_missing_attribute (data, element_name, "class", error);
      return;
    }

  if (type_func)
    {
      /* Call the GType function, and return the GType, it's guaranteed afterwards
       * that g_type_from_name on the name will return our GType
       */
      object_type = gtk_builder_scope_get_type_from_function (gtk_builder_get_scope (data->builder), data->builder, type_func);
      if (object_type == G_TYPE_INVALID)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_TYPE_FUNCTION,
                       "Invalid type function '%s'", type_func);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }
    }
  else
    {
      g_assert_nonnull (object_class);

      object_type = gtk_builder_get_type_from_name (data->builder, object_class);
      if (object_type == G_TYPE_INVALID)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Invalid object type '%s'", object_class);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
       }
    }

  if (!object_id)
    {
      internal_id = g_strdup_printf ("___object_%d___", ++data->object_counter);
      object_id = internal_id;
    }

  ++data->cur_object_level;

  /* check if we reached a requested object (if it is specified) */
  if (data->requested_objects && !data->inside_requested_object)
    {
      if (is_requested_object (object_id, data))
        {
          data->requested_object_level = data->cur_object_level;

          GTK_DEBUG (BUILDER, "requested object \"%s\" found at level %d",
                              object_id, data->requested_object_level);

          data->inside_requested_object = TRUE;
        }
      else
        {
          g_free (internal_id);
          return;
        }
    }

  object_info = g_new0 (ObjectInfo, 1);
  object_info->tag_type = TAG_OBJECT;
  object_info->type = object_type;
  object_info->oclass = g_type_class_ref (object_type);
  object_info->id = (internal_id) ? internal_id : g_strdup (object_id);
  object_info->constructor = g_strdup (constructor);
  object_info->parent = (CommonInfo*)child_info;
  state_push (data, object_info);

  has_duplicate = g_hash_table_lookup_extended (data->object_ids, object_id, NULL, &line_ptr);
  if (has_duplicate != 0)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_DUPLICATE_ID,
                   "Duplicate object ID '%s' (previously on line %d)",
                   object_id, GPOINTER_TO_INT (line_ptr));
      _gtk_builder_prefix_error (data->builder, context, error);
      return;
    }

  gtk_buildable_parse_context_get_position (context, &line, NULL);
  g_hash_table_insert (data->object_ids, g_strdup (object_id), GINT_TO_POINTER (line));
}

static void
parse_template (GtkBuildableParseContext  *context,
                ParserData                *data,
                const char                *element_name,
                const char               **names,
                const char               **values,
                GError                   **error)
{
  ObjectInfo *object_info;
  const char *object_class = NULL;
  const char *parent_class = NULL;
  int line;
  gpointer line_ptr;
  gboolean has_duplicate, allow_parents;
  GType template_type;
  GType parsed_type;

  template_type = gtk_builder_get_template_type (data->builder, &allow_parents);

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING, "class", &object_class,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "parent", &parent_class,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (template_type == 0)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_UNHANDLED_TAG,
                   "Template declaration (class '%s', parent '%s') where templates aren't supported",
                   object_class, parent_class ? parent_class : "GtkWidget");
      _gtk_builder_prefix_error (data->builder, context, error);
      return;
    }
  else if (state_peek (data) != NULL)
    {
      error_invalid_tag (data, "template", NULL, error);
      return;
    }

  parsed_type = g_type_from_name (object_class);
  if (template_type != parsed_type &&
      (!allow_parents || !g_type_is_a (template_type, parsed_type)))
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_TEMPLATE_MISMATCH,
                   "Parsed template definition for type '%s', expected type '%s'",
                   object_class, g_type_name (template_type));
      _gtk_builder_prefix_error (data->builder, context, error);
      return;
    }

  if (parent_class)
    {
      GType parent_type = g_type_from_name (parent_class);
      GType expected_type = g_type_parent (parsed_type);

      if (parent_type == G_TYPE_INVALID)
        {
          g_set_error (error, GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Invalid template parent type '%s'", parent_class);
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }
      if (parent_type != expected_type)
        {
          g_set_error (error, GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_TEMPLATE_MISMATCH,
                       "Template parent type '%s' does not match instance parent type '%s'.",
                       parent_class, g_type_name (expected_type));
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }
    }

  ++data->cur_object_level;

  object_info = g_new0 (ObjectInfo, 1);
  object_info->tag_type = TAG_TEMPLATE;
  object_info->object = gtk_builder_get_object (data->builder, object_class);
  object_info->type = template_type;
  object_info->oclass = g_type_class_ref (template_type);
  object_info->id = g_strdup (object_class);
  g_assert (object_info->object);
  state_push (data, object_info);

  has_duplicate = g_hash_table_lookup_extended (data->object_ids, object_class, NULL, &line_ptr);
  if (has_duplicate != 0)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_DUPLICATE_ID,
                   "Duplicate object ID '%s' (previously on line %d)",
                   object_class, GPOINTER_TO_INT (line_ptr));
      _gtk_builder_prefix_error (data->builder, context, error);
      return;
    }

  gtk_buildable_parse_context_get_position (context, &line, NULL);
  g_hash_table_insert (data->object_ids, g_strdup (object_class), GINT_TO_POINTER (line));
}


static void
free_object_info (ObjectInfo *info)
{
  /* Do not free the signal items, which GtkBuilder takes ownership of */
  g_type_class_unref (info->oclass);
  if (info->signals)
    g_ptr_array_free (info->signals, TRUE);
  if (info->properties)
    g_ptr_array_free (info->properties, TRUE);
  g_free (info->constructor);
  g_free (info->id);
  g_free (info);
}

static void
parse_child (ParserData  *data,
             const char  *element_name,
             const char **names,
             const char **values,
             GError     **error)

{
  ObjectInfo* object_info;
  ChildInfo *child_info;
  const char *type = NULL;
  const char *internal_child = NULL;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info ||
      !(object_info->tag_type == TAG_OBJECT ||
        object_info->tag_type == TAG_TEMPLATE))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "type", &type,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "internal-child", &internal_child,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  child_info = g_new0 (ChildInfo, 1);
  child_info->tag_type = TAG_CHILD;
  child_info->type = g_strdup (type);
  child_info->internal_child = g_strdup (internal_child);
  child_info->parent = (CommonInfo*)object_info;
  state_push (data, child_info);

  object_info->object = builder_construct (data, object_info, error);
}

static void
free_child_info (ChildInfo *info)
{
  g_free (info->type);
  g_free (info->internal_child);
  g_free (info);
}

static void
parse_property (ParserData   *data,
                const char   *element_name,
                const char **names,
                const char **values,
                GError      **error)
{
  PropertyInfo *info;
  const char *name = NULL;
  const char *context = NULL;
  const char *bind_source = NULL;
  const char *bind_property = NULL;
  const char *bind_flags_str = NULL;
  GBindingFlags bind_flags = G_BINDING_DEFAULT;
  gboolean translatable = FALSE;
  ObjectInfo *object_info;
  GParamSpec *pspec = NULL;
  int line, col;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info ||
      !(object_info->tag_type == TAG_OBJECT ||
        object_info->tag_type == TAG_TEMPLATE))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING, "name", &name,
                                    G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "translatable", &translatable,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "comments", NULL,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "context", &context,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "bind-source", &bind_source,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "bind-property", &bind_property,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "bind-flags", &bind_flags_str,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  pspec = g_object_class_find_property (object_info->oclass, name);

  if (!pspec)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_PROPERTY,
                   "Invalid property: %s.%s",
                   g_type_name (object_info->type), name);
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (bind_flags_str)
    {
      if (!_gtk_builder_flags_from_string (G_TYPE_BINDING_FLAGS, bind_flags_str, &bind_flags, error))
        {
          _gtk_builder_prefix_error (data->builder, &data->ctx, error);
          return;
        }
    }

  gtk_buildable_parse_context_get_position (&data->ctx, &line, &col);

  if (bind_source)
    {
      BindingInfo *binfo;

      binfo = g_new0 (BindingInfo, 1);
      binfo->tag_type = TAG_BINDING;
      binfo->target = NULL;
      binfo->target_pspec = pspec;
      binfo->source = g_strdup (bind_source);
      binfo->source_property = bind_property ? g_strdup (bind_property) : g_strdup (name);
      binfo->flags = bind_flags;
      binfo->line = line;
      binfo->col = col;

      object_info->bindings = g_slist_prepend (object_info->bindings, binfo);
    }
  else if (bind_property)
    {
      error_missing_attribute (data, element_name,
                               "bind-source",
                               error);
      return;
    }

  info = g_new0 (PropertyInfo, 1);
  info->tag_type = TAG_PROPERTY;
  info->pspec = pspec;
  info->text = g_string_new ("");
  info->translatable = translatable;
  info->bound = bind_source != NULL;
  info->context = g_strdup (context);
  info->line = line;
  info->col = col;

  state_push (data, info);
}

static void
parse_binding (ParserData   *data,
               const char   *element_name,
               const char **names,
               const char **values,
               GError      **error)
{
  BindingExpressionInfo *info;
  const char *name = NULL;
  const char *object_name = NULL;
  ObjectInfo *object_info;
  GParamSpec *pspec = NULL;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info ||
      !(object_info->tag_type == TAG_OBJECT ||
        object_info->tag_type == TAG_TEMPLATE))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING, "name", &name,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "object", &object_name,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  pspec = g_object_class_find_property (object_info->oclass, name);

  if (!pspec)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_PROPERTY,
                   "Invalid property: %s.%s",
                   g_type_name (object_info->type), name);
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }
  else if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_PROPERTY,
                   "%s.%s is a construct-only property",
                   g_type_name (object_info->type), name);
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_PROPERTY,
                   "%s.%s is a non-writable property",
                   g_type_name (object_info->type), name);
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }


  info = g_new0 (BindingExpressionInfo, 1);
  info->tag_type = TAG_BINDING_EXPRESSION;
  info->target = NULL;
  info->target_pspec = pspec;
  info->object_name = g_strdup (object_name);
  gtk_buildable_parse_context_get_position (&data->ctx, &info->line, &info->col);

  state_push (data, info);
}

static void
free_property_info (PropertyInfo *info)
{
  if (info->value)
    {
      if (G_PARAM_SPEC_VALUE_TYPE (info->pspec) == GTK_TYPE_EXPRESSION)
        gtk_expression_unref (info->value);
      else
        g_assert_not_reached();
    }
  g_string_free (info->text, TRUE);
  g_free (info->context);
  g_free (info);
}

static void
free_expression_info (ExpressionInfo *info)
{
  switch (info->expression_type)
    {
    case EXPRESSION_EXPRESSION:
      g_clear_pointer (&info->expression, gtk_expression_unref);
      break;

    case EXPRESSION_CONSTANT:
      g_string_free (info->constant.text, TRUE);
      break;

    case EXPRESSION_CLOSURE:
      g_free (info->closure.function_name);
      g_free (info->closure.object_name);
      g_slist_free_full (info->closure.params, (GDestroyNotify) free_expression_info);
      break;

    case EXPRESSION_PROPERTY:
      g_clear_pointer (&info->property.expression, free_expression_info);
      g_free (info->property.property_name);
      break;

    default:
      g_assert_not_reached ();
      break;
    }
  g_free (info);
}

static gboolean
check_expression_parent (ParserData *data)
{
  CommonInfo *common_info = state_peek_info (data, CommonInfo);

  if (common_info == NULL)
    return FALSE;

  if (common_info->tag_type == TAG_PROPERTY)
    {
      PropertyInfo *prop_info = (PropertyInfo *) common_info;

      return G_PARAM_SPEC_VALUE_TYPE (prop_info->pspec) == GTK_TYPE_EXPRESSION;
    }
  else if (common_info->tag_type == TAG_BINDING_EXPRESSION)
    {
      BindingExpressionInfo *expr_info = (BindingExpressionInfo *) common_info;

      return expr_info->expr == NULL;
    }
  else if (common_info->tag_type == TAG_EXPRESSION)
    {
      ExpressionInfo *expr_info = (ExpressionInfo *) common_info;

      switch (expr_info->expression_type)
        {
        case EXPRESSION_CLOSURE:
          return TRUE;
        case EXPRESSION_CONSTANT:
          return FALSE;
        case EXPRESSION_PROPERTY:
          return expr_info->property.expression == NULL;
        case EXPRESSION_EXPRESSION:
        default:
          g_assert_not_reached ();
          return FALSE;
        }
    }

  return FALSE;
}

static void
parse_constant_expression (ParserData   *data,
                           const char   *element_name,
                           const char **names,
                           const char **values,
                           GError      **error)
{
  ExpressionInfo *info;
  const char *type_name = NULL;
  GType type;

  if (!check_expression_parent (data))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "type", &type_name,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (type_name == NULL)
    type = G_TYPE_INVALID;
  else
    {
      type = gtk_builder_get_type_from_name (data->builder, type_name);
      if (type == G_TYPE_INVALID)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Invalid type '%s'", type_name);
          _gtk_builder_prefix_error (data->builder, &data->ctx, error);
          return;
        }
    }

  info = g_new0 (ExpressionInfo, 1);
  info->tag_type = TAG_EXPRESSION;
  info->expression_type = EXPRESSION_CONSTANT;
  info->constant.type = type;
  info->constant.text = g_string_new (NULL);

  state_push (data, info);
}

static void
parse_closure_expression (ParserData   *data,
                          const char   *element_name,
                          const char **names,
                          const char **values,
                          GError      **error)
{
  ExpressionInfo *info;
  const char *type_name;
  const char *function_name;
  const char *object_name = NULL;
  gboolean swapped = -1;
  GType type;

  if (!check_expression_parent (data))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING, "type", &type_name,
                                    G_MARKUP_COLLECT_STRING, "function", &function_name,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "object", &object_name,
                                    G_MARKUP_COLLECT_TRISTATE|G_MARKUP_COLLECT_OPTIONAL, "swapped", &swapped,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  type = gtk_builder_get_type_from_name (data->builder, type_name);
  if (type == G_TYPE_INVALID)
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_VALUE,
                   "Invalid type '%s'", type_name);
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  /* Swapped defaults to FALSE except when object is set */
  if (swapped == -1)
    {
      if (object_name)
        swapped = TRUE;
      else
        swapped = FALSE;
    }

  info = g_new0 (ExpressionInfo, 1);
  info->tag_type = TAG_EXPRESSION;
  info->expression_type = EXPRESSION_CLOSURE;
  info->closure.type = type;
  info->closure.swapped = swapped;
  info->closure.function_name = g_strdup (function_name);
  info->closure.object_name = g_strdup (object_name);

  state_push (data, info);
}

static void
parse_lookup_expression (ParserData   *data,
                         const char   *element_name,
                         const char **names,
                         const char **values,
                         GError      **error)
{
  ExpressionInfo *info;
  const char *property_name;
  const char *type_name = NULL;
  GType type;

  if (!check_expression_parent (data))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "type", &type_name,
                                    G_MARKUP_COLLECT_STRING, "name", &property_name,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (type_name == NULL)
    {
      type = G_TYPE_INVALID;
    }
  else
    {
      type = gtk_builder_get_type_from_name (data->builder, type_name);
      if (type == G_TYPE_INVALID)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Invalid type '%s'", type_name);
          _gtk_builder_prefix_error (data->builder, &data->ctx, error);
          return;
        }
    }

  info = g_new0 (ExpressionInfo, 1);
  info->tag_type = TAG_EXPRESSION;
  info->expression_type = EXPRESSION_PROPERTY;
  info->property.this_type = type;
  info->property.property_name = g_strdup (property_name);

  state_push (data, info);
}

GtkExpression *
expression_info_construct (GtkBuilder      *builder,
                           ExpressionInfo  *info,
                           GError         **error)
{
  switch (info->expression_type)
    {
    case EXPRESSION_EXPRESSION:
      break;

    case EXPRESSION_CONSTANT:
      {
        GtkExpression *expr;

        if (info->constant.type == G_TYPE_INVALID)
          {
            GObject *o = gtk_builder_lookup_object (builder, info->constant.text->str, 0, 0, error);
            if (o == NULL)
              return NULL;

            expr = gtk_object_expression_new (o);
          }
        else
          {
            GValue value = G_VALUE_INIT;

            if (!gtk_builder_value_from_string_type (builder,
                                                     info->constant.type,
                                                     info->constant.text->str,
                                                     &value,
                                                     error))
              return  NULL;

            if (G_VALUE_HOLDS_OBJECT (&value))
              expr = gtk_object_expression_new (g_value_get_object (&value));
            else
              expr = gtk_constant_expression_new_for_value (&value);

            g_value_unset (&value);
          }

        g_string_free (info->constant.text, TRUE);
        info->expression_type = EXPRESSION_EXPRESSION;
        info->expression = expr;
      }
      break;

    case EXPRESSION_CLOSURE:
      {
        GObject *object;
        GClosure *closure;
        guint i, n_params;
        GtkExpression **params;
        GtkExpression *expression;
        GSList *l;

        if (info->closure.object_name)
          {
            object = gtk_builder_lookup_object (builder, info->closure.object_name, 0, 0, error);
            if (object == NULL)
              return NULL;
          }
        else
          {
            object = NULL;
          }

        closure = gtk_builder_create_closure (builder, 
                                              info->closure.function_name,
                                              info->closure.swapped,
                                              object,
                                              error);
        if (closure == NULL)
          return NULL;
        n_params = g_slist_length (info->closure.params);
        params = g_newa (GtkExpression *, n_params);
        i = n_params;
        for (l = info->closure.params; l; l = l->next)
          {
            params[--i] = expression_info_construct (builder, l->data, error);
            if (params[i] == NULL)
              return NULL;
          }
        expression = gtk_closure_expression_new (info->closure.type, closure, n_params, params);
        g_free (info->closure.function_name);
        g_free (info->closure.object_name);
        g_slist_free_full (info->closure.params, (GDestroyNotify) free_expression_info);
        info->expression_type = EXPRESSION_EXPRESSION;
        info->expression = expression;
      }
      break;

    case EXPRESSION_PROPERTY:
      {
        GtkExpression *expression;
        GType type;
        GParamSpec *pspec;

        if (info->property.expression)
          {
            expression = expression_info_construct (builder, info->property.expression, error);
            if (expression == NULL)
              return NULL;
            g_clear_pointer (&info->property.expression, free_expression_info);
          }
        else
          expression = NULL;

        if (info->property.this_type != G_TYPE_INVALID)
          type = info->property.this_type;
        else if (expression != NULL)
          type = gtk_expression_get_value_type (expression);
        else
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
                         "Lookups require a type attribute if they don't have an expression.");
            return NULL;
          }

        if (g_type_fundamental (type) == G_TYPE_OBJECT)
          {
            GObjectClass *class = g_type_class_ref (type);
            pspec = g_object_class_find_property (class, info->property.property_name);
            g_type_class_unref (class);
          }
        else if (g_type_fundamental (type) == G_TYPE_INTERFACE)
          {
            GTypeInterface *iface = g_type_default_interface_ref (type);
            pspec = g_object_interface_find_property (iface, info->property.property_name);
            g_type_default_interface_unref (iface);
          }
        else
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
                         "Type `%s` does not support properties",
                         g_type_name (type));
            return NULL;
          }

        if (pspec == NULL)
          {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
                         "Type `%s` does not have a property name `%s`",
                         g_type_name (type), info->property.property_name);
            return NULL;
          }

        expression = gtk_property_expression_new_for_pspec (expression, pspec);

        g_free (info->property.property_name);
        info->expression_type = EXPRESSION_EXPRESSION;
        info->expression = expression;
      }
      break;

    default:
      g_return_val_if_reached (NULL);
    }

  return gtk_expression_ref (info->expression);
}

static void
parse_signal (ParserData   *data,
              const char   *element_name,
              const char **names,
              const char **values,
              GError      **error)
{
  SignalInfo *info;
  const char *name;
  const char *handler = NULL;
  const char *object = NULL;
  gboolean after = FALSE;
  gboolean swapped = -1;
  ObjectInfo *object_info;
  guint id = 0;
  GQuark detail = 0;

  object_info = state_peek_info (data, ObjectInfo);
  if (!object_info ||
      !(object_info->tag_type == TAG_OBJECT||
        object_info->tag_type == TAG_TEMPLATE))
    {
      error_invalid_tag (data, element_name, NULL, error);
      return;
    }

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING, "name", &name,
                                    G_MARKUP_COLLECT_STRING, "handler", &handler,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "object", &object,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "last_modification_time", NULL,
                                    G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "after", &after,
                                    G_MARKUP_COLLECT_TRISTATE|G_MARKUP_COLLECT_OPTIONAL, "swapped", &swapped,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (!g_signal_parse_name (name, object_info->type, &id, &detail, TRUE))
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_INVALID_SIGNAL,
                   "Invalid signal '%s' for type '%s'",
                   name, g_type_name (object_info->type));
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  /* Swapped defaults to FALSE except when object is set */
  if (swapped == -1)
    {
      if (object)
        swapped = TRUE;
      else
        swapped = FALSE;
    }

  info = g_new0 (SignalInfo, 1);
  info->id = id;
  info->detail = detail;
  info->handler = g_strdup (handler);
  if (after)
    info->flags |= G_CONNECT_AFTER;
  if (swapped)
    info->flags |= G_CONNECT_SWAPPED;
  info->connect_object_name = g_strdup (object);
  state_push (data, info);

  info->tag_type = TAG_SIGNAL;
}

/* Called by GtkBuilder */
void
_free_signal_info (SignalInfo *info,
                   gpointer    user_data)
{
  g_free (info->handler);
  g_free (info->connect_object_name);
  g_free (info->object_name);
  g_free (info);
}

void
_free_binding_info (BindingInfo *info,
                    gpointer     user)
{
  g_free (info->source);
  g_free (info->source_property);
  g_free (info);
}

void
free_binding_expression_info (BindingExpressionInfo *info)
{
  if (info->expr)
    free_expression_info (info->expr);
  g_free (info->object_name);
  g_free (info);
}

static void
free_requires_info (RequiresInfo *info,
                    gpointer      user_data)
{
  g_free (info->library);
  g_free (info);
}

static void
parse_interface (ParserData   *data,
                 const char   *element_name,
                 const char **names,
                 const char **values,
                 GError      **error)
{
  const char *domain = NULL;

  if (!g_markup_collect_attributes (element_name, names, values, error,
                                    G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "domain", &domain,
                                    G_MARKUP_COLLECT_INVALID))
    {
      _gtk_builder_prefix_error (data->builder, &data->ctx, error);
      return;
    }

  if (domain)
    {
      if (data->domain && strcmp (data->domain, domain) != 0)
        {
          g_warning ("%s: interface domain '%s' overrides programmatic value '%s'",
                     data->filename, domain, data->domain);
          g_free (data->domain);
        }

      data->domain = g_strdup (domain);
      gtk_builder_set_translation_domain (data->builder, data->domain);
    }
}

static SubParser *
create_subparser (GObject       *object,
                  GObject       *child,
                  const char    *element_name,
                  GtkBuildableParser *parser,
                  gpointer       user_data)
{
  SubParser *subparser;

  subparser = g_new0 (SubParser, 1);
  subparser->object = object;
  subparser->child = child;
  subparser->tagname = g_strdup (element_name);
  subparser->level = 1;
  subparser->start = element_name;
  subparser->parser = g_memdup2 (parser, sizeof (GtkBuildableParser));
  subparser->data = user_data;

  return subparser;
}

static void
free_subparser (SubParser *subparser)
{
  g_free (subparser->tagname);
  g_free (subparser);
}

static gboolean
subparser_start (GtkBuildableParseContext  *context,
                 const char                *element_name,
                 const char               **names,
                 const char               **values,
                 ParserData                *data,
                 GError                   **error)
{
  SubParser *subparser = data->subparser;

  if (!subparser->start &&
      strcmp (element_name, subparser->tagname) == 0)
    subparser->start = element_name;

  if (subparser->start)
    {
      subparser->level++;

      if (subparser->parser->start_element)
        subparser->parser->start_element (context,
                                          element_name, names, values,
                                          subparser->data, error);
      return FALSE;
    }
  return TRUE;
}

static void
subparser_end (GtkBuildableParseContext  *context,
               const char                *element_name,
               ParserData                *data,
               GError                   **error)
{
  data->subparser->level--;

  if (data->subparser->parser->end_element)
    data->subparser->parser->end_element (context, element_name,
                                          data->subparser->data, error);

  if (*error)
    return;

  if (data->subparser->level > 0)
    return;

  g_assert (strcmp (data->subparser->start, element_name) == 0);

  gtk_buildable_custom_tag_end (GTK_BUILDABLE (data->subparser->object),
                                data->builder,
                                data->subparser->child,
                                element_name,
                                data->subparser->data);
  g_clear_pointer (&data->subparser->parser, g_free);

  if (_gtk_builder_lookup_failed (data->builder, error))
    return;

  if (GTK_BUILDABLE_GET_IFACE (data->subparser->object)->custom_finished)
    data->custom_finalizers = g_slist_prepend (data->custom_finalizers,
                                               data->subparser);
  else
    free_subparser (data->subparser);

  data->subparser = NULL;
}

static gboolean
parse_custom (GtkBuildableParseContext  *context,
              const char                *element_name,
              const char               **names,
              const char               **values,
              ParserData                *data,
              GError                   **error)
{
  CommonInfo* parent_info;
  GtkBuildableParser parser;
  gpointer subparser_data;
  GObject *object;
  GObject *child;

  parent_info = state_peek_info (data, CommonInfo);
  if (!parent_info)
    return FALSE;

  if (parent_info->tag_type == TAG_OBJECT ||
      parent_info->tag_type == TAG_TEMPLATE)
    {
      ObjectInfo* object_info = (ObjectInfo*)parent_info;
      if (!object_info->object)
        {
          object_info->object = builder_construct (data, object_info, error);
          if (!object_info->object)
            return TRUE; /* A GError is already set */
        }
      g_assert (object_info->object);
      object = object_info->object;
      child = NULL;
    }
  else if (parent_info->tag_type == TAG_CHILD)
    {
      ChildInfo* child_info = (ChildInfo*)parent_info;

      _gtk_builder_add (data->builder, child_info);

      object = ((ObjectInfo*)child_info->parent)->object;
      child  = child_info->object;
    }
  else
    return FALSE;

  if (!gtk_buildable_custom_tag_start (GTK_BUILDABLE (object),
                                       data->builder,
                                       child,
                                       element_name,
                                       &parser,
                                       &subparser_data))
    return FALSE;

  data->subparser = create_subparser (object, child, element_name,
                                      &parser, subparser_data);

  if (parser.start_element)
    parser.start_element (context,
                          element_name, names, values,
                          subparser_data, error);
  return TRUE;
}

static void
start_element (GtkBuildableParseContext  *context,
               const char                *element_name,
               const char               **names,
               const char               **values,
               gpointer                   user_data,
               GError                   **error)
{
  ParserData *data = (ParserData*)user_data;

  if (GTK_DEBUG_CHECK (BUILDER))
    {
      GString *tags = g_string_new ("");
      int i;
      for (i = 0; names[i]; i++)
        g_string_append_printf (tags, "%s=\"%s\" ", names[i], values[i]);

      if (i)
        {
          g_string_insert_c (tags, 0, ' ');
          g_string_truncate (tags, tags->len - 1);
        }
      g_message ("<%s%s>", element_name, tags->str);
      g_string_free (tags, TRUE);
    }

  if (!data->last_element && strcmp (element_name, "interface") != 0)
    {
      error_unhandled_tag (data, element_name, error);
      return;
    }
  data->last_element = element_name;

  if (data->subparser)
    {
      if (!subparser_start (context, element_name, names, values, data, error))
        return;
    }

  if (strcmp (element_name, "object") == 0)
    parse_object (context, data, element_name, names, values, error);
  else if (data->requested_objects && !data->inside_requested_object)
    {
      /* If outside a requested object, simply ignore this tag */
    }
  else if (strcmp (element_name, "property") == 0)
    parse_property (data, element_name, names, values, error);
  else if (strcmp (element_name, "binding") == 0)
    parse_binding (data, element_name, names, values, error);
  else if (strcmp (element_name, "child") == 0)
    parse_child (data, element_name, names, values, error);
  else if (strcmp (element_name, "signal") == 0)
    parse_signal (data, element_name, names, values, error);
  else if (strcmp (element_name, "template") == 0)
    parse_template (context, data, element_name, names, values, error);
  else if (strcmp (element_name, "requires") == 0)
    parse_requires (data, element_name, names, values, error);
  else if (strcmp (element_name, "interface") == 0)
    parse_interface (data, element_name, names, values, error);
  else if (strcmp (element_name, "constant") == 0)
    parse_constant_expression (data, element_name, names, values, error);
  else if (strcmp (element_name, "closure") == 0)
    parse_closure_expression (data, element_name, names, values, error);
  else if (strcmp (element_name, "lookup") == 0)
    parse_lookup_expression (data, element_name, names, values, error);
  else if (strcmp (element_name, "menu") == 0)
    _gtk_builder_menu_start (data, element_name, names, values, error);
  else if (strcmp (element_name, "placeholder") == 0)
    {
      /* placeholder has no special treatmeant, but it needs an
       * if clause to avoid an error below.
       */
    }
  else if (!parse_custom (context, element_name, names, values, data, error))
    error_unhandled_tag (data, element_name, error);
}

const char *
_gtk_builder_parser_translate (const char *domain,
                               const char *context,
                               const char *text)
{
  const char *s;

  if (context)
    s = g_dpgettext2 (domain, context, text);
  else
    s = g_dgettext (domain, text);

  return s;
}

static void
end_element (GtkBuildableParseContext  *context,
             const char                *element_name,
             gpointer                   user_data,
             GError                   **error)
{
  ParserData *data = (ParserData*)user_data;

  GTK_DEBUG (BUILDER, "</%s>", element_name);

  if (data->subparser && data->subparser->start)
    {
      subparser_end (context, element_name, data, error);
      return;
    }

  if (data->requested_objects && !data->inside_requested_object)
    {
      /* If outside a requested object, simply ignore this tag */
    }
  else if (strcmp (element_name, "property") == 0)
    {
      PropertyInfo *prop_info = state_pop_info (data, PropertyInfo);
      CommonInfo *info = state_peek_info (data, CommonInfo);

      g_assert (info != NULL);

      /* Normal properties */
      if (info->tag_type == TAG_OBJECT ||
          info->tag_type == TAG_TEMPLATE)
        {
          ObjectInfo *object_info = (ObjectInfo*)info;

          if (prop_info->translatable && prop_info->text->len)
            {
              const char *translated;

              translated = _gtk_builder_parser_translate (data->domain,
                                                          prop_info->context,
                                                          prop_info->text->str);
              g_string_assign (prop_info->text, translated);
            }

          if (G_UNLIKELY (!object_info->properties))
            object_info->properties = g_ptr_array_new_with_free_func ((GDestroyNotify)free_property_info);

          g_ptr_array_add (object_info->properties, prop_info);
        }
      else
        g_assert_not_reached ();
    }
  else if (strcmp (element_name, "binding") == 0)
    {
      BindingExpressionInfo *binfo = state_pop_info (data, BindingExpressionInfo);
      CommonInfo *info = state_peek_info (data, CommonInfo);

      g_assert (info != NULL);

      if (binfo->expr == NULL)
        {
            g_set_error (error,
                         GTK_BUILDER_ERROR,
                         GTK_BUILDER_ERROR_INVALID_TAG,
                         "Binding tag requires an expression");
            free_binding_expression_info (binfo);
        }
      else if (info->tag_type == TAG_OBJECT ||
          info->tag_type == TAG_TEMPLATE)
        {
          ObjectInfo *object_info = (ObjectInfo*)info;
          object_info->bindings = g_slist_prepend (object_info->bindings, binfo);
        }
      else
        g_assert_not_reached ();
    }
  else if (strcmp (element_name, "object") == 0 ||
           strcmp (element_name, "template") == 0)
    {
      ObjectInfo *object_info = state_pop_info (data, ObjectInfo);
      ChildInfo* child_info = state_peek_info (data, ChildInfo);
      PropertyInfo* prop_info = state_peek_info (data, PropertyInfo);

      if (child_info && child_info->tag_type != TAG_CHILD)
        child_info = NULL;
      if (prop_info && prop_info->tag_type != TAG_PROPERTY)
        prop_info = NULL;

      if (data->requested_objects && data->inside_requested_object &&
          (data->cur_object_level == data->requested_object_level))
        {
          GTK_DEBUG (BUILDER, "requested object end found at level %d",
                              data->requested_object_level);

          data->inside_requested_object = FALSE;
        }

      --data->cur_object_level;

      g_assert (data->cur_object_level >= 0);

      object_info->object = builder_construct (data, object_info, error);
      if (!object_info->object)
        {
          free_object_info (object_info);
          return;
        }
      if (child_info)
        child_info->object = object_info->object;
      if (prop_info)
        g_string_assign (prop_info->text, object_info->id);

      if (GTK_IS_BUILDABLE (object_info->object) &&
          GTK_BUILDABLE_GET_IFACE (object_info->object)->parser_finished)
        g_ptr_array_add (data->finalizers, object_info->object);

      if (object_info->signals)
        {
          _gtk_builder_add_signals (data->builder, object_info->signals);
          object_info->signals = NULL;
        }

      if (object_info->bindings)
        {
          gtk_builder_take_bindings (data->builder, object_info->object, object_info->bindings);
          object_info->bindings = NULL;
        }

      free_object_info (object_info);
    }
  else if (strcmp (element_name, "child") == 0)
    {
      ChildInfo *child_info = state_pop_info (data, ChildInfo);

      _gtk_builder_add (data->builder, child_info);

      free_child_info (child_info);
    }
  else if (strcmp (element_name, "signal") == 0)
    {
      SignalInfo *signal_info = state_pop_info (data, SignalInfo);
      ObjectInfo *object_info = (ObjectInfo*)state_peek_info (data, CommonInfo);
      g_assert (object_info != NULL);
      signal_info->object_name = g_strdup (object_info->id);

      if (G_UNLIKELY (!object_info->signals))
        object_info->signals = g_ptr_array_new ();

      g_ptr_array_add (object_info->signals, signal_info);
    }
  else if (strcmp (element_name, "constant") == 0 ||
           strcmp (element_name, "closure") == 0 ||
           strcmp (element_name, "lookup") == 0)
    {
      ExpressionInfo *expression_info = state_pop_info (data, ExpressionInfo);
      CommonInfo *parent_info = state_peek_info (data, CommonInfo);
      g_assert (parent_info != NULL);

      if (parent_info->tag_type == TAG_BINDING_EXPRESSION)
        {
          BindingExpressionInfo *expr_info = (BindingExpressionInfo *) parent_info;

          expr_info->expr = expression_info;
        }
      else if (parent_info->tag_type == TAG_PROPERTY)
        {
          PropertyInfo *prop_info = (PropertyInfo *) parent_info;

          prop_info->value = expression_info_construct (data->builder, expression_info, error);
          free_expression_info (expression_info);
        }
      else if (parent_info->tag_type == TAG_EXPRESSION)
        {
          ExpressionInfo *expr_info = (ExpressionInfo *) parent_info;

          switch (expr_info->expression_type)
            {
            case EXPRESSION_CLOSURE:
              expr_info->closure.params = g_slist_prepend (expr_info->closure.params, expression_info);
              break;
            case EXPRESSION_PROPERTY:
              expr_info->property.expression = expression_info;
              break;
            case EXPRESSION_EXPRESSION:
            case EXPRESSION_CONSTANT:
            default:
              g_assert_not_reached ();
              break;
            }
        }
      else
        {
          g_assert_not_reached ();
        }
    }
  else if (strcmp (element_name, "requires") == 0)
    {
      RequiresInfo *req_info = state_pop_info (data, RequiresInfo);

      /* TODO: Allow third party widget developers to check their
       * required versions, possibly throw a signal allowing them
       * to check their library versions here.
       */
      if (!strcmp (req_info->library, "gtk"))
        {
          if (req_info->major == 4 && req_info->minor == 0)
            {
              /* We allow 3.99.x to pass as 4.0 */
            }
          else if (gtk_check_version (req_info->major, req_info->minor, 0) != NULL)
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR,
                           GTK_BUILDER_ERROR_VERSION_MISMATCH,
                           "Required GTK version %d.%d, current version is %d.%d",
                           req_info->major, req_info->minor,
                           GTK_MAJOR_VERSION, GTK_MINOR_VERSION);
              _gtk_builder_prefix_error (data->builder, context, error);
           }
        }
      free_requires_info (req_info, NULL);
    }
  else if (strcmp (element_name, "interface") == 0)
    {
    }
  else if (strcmp (element_name, "menu") == 0)
    {
      _gtk_builder_menu_end (data);
    }
  else if (strcmp (element_name, "placeholder") == 0)
    {
    }
  else
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR,
                   GTK_BUILDER_ERROR_UNHANDLED_TAG,
                   "Unhandled tag: <%s>", element_name);
      _gtk_builder_prefix_error (data->builder, context, error);
    }
}

/* Called for character data */
/* text is not nul-terminated */
static void
text (GtkBuildableParseContext  *context,
      const char                *text,
      gsize                      text_len,
      gpointer                   user_data,
      GError                   **error)
{
  ParserData *data = (ParserData*)user_data;
  CommonInfo *info;

  if (data->subparser && data->subparser->start)
    {
      GError *tmp_error = NULL;

      if (data->subparser->parser->text)
        data->subparser->parser->text (context, text, text_len,
                                       data->subparser->data, &tmp_error);
      if (tmp_error)
        g_propagate_error (error, tmp_error);
      return;
    }

  if (!data->stack || data->stack->len == 0)
    return;

  info = state_peek_info (data, CommonInfo);
  g_assert (info != NULL);

  if (strcmp (gtk_buildable_parse_context_get_element (context), "property") == 0)
    {
      PropertyInfo *prop_info = (PropertyInfo*)info;

      g_string_append_len (prop_info->text, text, text_len);
    }
  else if (strcmp (gtk_buildable_parse_context_get_element (context), "constant") == 0)
    {
      ExpressionInfo *expr_info = (ExpressionInfo *) info;

      g_string_append_len (expr_info->constant.text, text, text_len);
    }
  else if (strcmp (gtk_buildable_parse_context_get_element (context), "lookup") == 0)
    {
      ExpressionInfo *expr_info = (ExpressionInfo *) info;

      while (g_ascii_isspace (*text) && text_len > 0)
        {
          text++;
          text_len--;
        }
      while (text_len > 0 && g_ascii_isspace (text[text_len - 1]))
        text_len--;
      if (expr_info->property.expression == NULL && text_len > 0)
        {
          ExpressionInfo *constant = g_new0 (ExpressionInfo, 1);
          constant->tag_type = TAG_EXPRESSION;
          constant->expression_type = EXPRESSION_CONSTANT;
          constant->constant.type = G_TYPE_INVALID;
          constant->constant.text = g_string_new_len (text, text_len);
          expr_info->property.expression = constant;
        }
    }
}

static const GtkBuildableParser parser = {
  start_element,
  end_element,
  text,
  NULL,
};

void
_gtk_builder_parser_parse_buffer (GtkBuilder   *builder,
                                  const char   *filename,
                                  const char   *buffer,
                                  gssize        length,
                                  const char  **requested_objs,
                                  GError      **error)
{
  const char * domain;
  ParserData data;
  GSList *l;
  gint64 before = GDK_PROFILER_CURRENT_TIME;

  /* Store the original domain so that interface domain attribute can be
   * applied for the builder and the original domain can be restored after
   * parsing has finished. This allows subparsers to translate elements with
   * gtk_builder_get_translation_domain() without breaking the ABI or API
   */
  domain = gtk_builder_get_translation_domain (builder);

  memset (&data, 0, sizeof (ParserData));
  data.builder = builder;
  data.filename = filename;
  data.domain = g_strdup (domain);
  data.object_ids = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify)g_free, NULL);
  data.stack = g_ptr_array_new ();
  data.finalizers = g_ptr_array_new ();

  if (requested_objs)
    {
      data.inside_requested_object = FALSE;
      data.requested_objects = requested_objs;
    }
  else
    {
      /* get all the objects */
      data.inside_requested_object = TRUE;
    }

  gtk_buildable_parse_context_init (&data.ctx, &parser, &data);

  if (!gtk_buildable_parse_context_parse (&data.ctx, buffer, length, error))
    goto out;

  if (_gtk_builder_lookup_failed (builder, error))
    goto out;

  if (!_gtk_builder_finish (builder, error))
    goto out;

  /* Custom parser_finished */
  data.custom_finalizers = g_slist_reverse (data.custom_finalizers);
  for (l = data.custom_finalizers; l; l = l->next)
    {
      SubParser *sub = (SubParser*)l->data;

      gtk_buildable_custom_finished (GTK_BUILDABLE (sub->object),
                                     builder,
                                     sub->child,
                                     sub->tagname,
                                     sub->data);
      if (_gtk_builder_lookup_failed (builder, error))
        goto out;
    }

  /* Common parser_finished, for all created objects */
  for (guint i = 0; i < data.finalizers->len; i++)
    {
      GtkBuildable *buildable = g_ptr_array_index (data.finalizers, i);

      gtk_buildable_parser_finished (GTK_BUILDABLE (buildable), builder);
      if (_gtk_builder_lookup_failed (builder, error))
        goto out;
    }

 out:

  g_slist_free_full (data.custom_finalizers, (GDestroyNotify)free_subparser);
  g_free (data.domain);
  g_hash_table_destroy (data.object_ids);
  g_ptr_array_free (data.stack, TRUE);
  g_ptr_array_free (data.finalizers, TRUE);
  gtk_buildable_parse_context_free (&data.ctx);

  /* restore the original domain */
  gtk_builder_set_translation_domain (builder, domain);

  if (GDK_PROFILER_IS_RUNNING)
    {
      guint64 after = GDK_PROFILER_CURRENT_TIME;
      if (after - before > 500000) /* half a millisecond */
        {
          gdk_profiler_add_mark (before, after - before, "Builder load", filename);
        }
    }
}
