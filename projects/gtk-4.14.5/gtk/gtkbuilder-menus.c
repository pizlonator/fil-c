/*
 * Copyright © 2011, 2012 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkbuilderprivate.h"
#include "gtkbuildableprivate.h"
#include <glib/gi18n-lib.h>

#include <gio/gio.h>
#include <string.h>

struct frame
{
  GMenu        *menu;
  GMenuItem    *item;
  struct frame *prev;
};

typedef struct
{
  ParserData *parser_data;
  struct frame frame;

  /* attributes */
  char         *attribute;
  GVariantType *type;
  GString      *string;

  /* translation */
  char         *context;
  gboolean      translatable;
} GtkBuilderMenuState;

static void
gtk_builder_menu_push_frame (GtkBuilderMenuState *state,
                             GMenu               *menu,
                             GMenuItem           *item)
{
  struct frame *new;

  new = g_new (struct frame, 1);
  *new = state->frame;

  state->frame.menu = menu;
  state->frame.item = item;
  state->frame.prev = new;
}

static void
gtk_builder_menu_pop_frame (GtkBuilderMenuState *state)
{
  struct frame *prev = state->frame.prev;

  if (state->frame.item)
    {
      g_assert (prev->menu != NULL);
      g_menu_append_item (prev->menu, state->frame.item);
      g_object_unref (state->frame.item);
    }

  state->frame = *prev;

  g_free (prev);
}

static void
gtk_builder_menu_start_element (GtkBuildableParseContext  *context,
                                const char                *element_name,
                                const char               **attribute_names,
                                const char               **attribute_values,
                                gpointer                   user_data,
                                GError                   **error)
{
  GtkBuilderMenuState *state = user_data;

#define COLLECT(first, ...) \
  g_markup_collect_attributes (element_name,                                 \
                               attribute_names, attribute_values, error,     \
                               first, __VA_ARGS__, G_MARKUP_COLLECT_INVALID)
#define OPTIONAL   G_MARKUP_COLLECT_OPTIONAL
#define BOOLEAN    G_MARKUP_COLLECT_BOOLEAN
#define STRING     G_MARKUP_COLLECT_STRING

  if (state->frame.menu)
    {
      /* Can have '<item>', '<submenu>' or '<section>' here. */
      if (g_str_equal (element_name, "item"))
        {
          GMenuItem *item;

          if (COLLECT (G_MARKUP_COLLECT_INVALID, NULL))
            {
              item = g_menu_item_new (NULL, NULL);
              gtk_builder_menu_push_frame (state, NULL, item);
            }

          return;
        }

      else if (g_str_equal (element_name, "submenu"))
        {
          const char *id;

          if (COLLECT (STRING | OPTIONAL, "id", &id))
            {
              GMenuItem *item;
              GMenu *menu;

              menu = g_menu_new ();
              item = g_menu_item_new_submenu (NULL, G_MENU_MODEL (menu));
              gtk_builder_menu_push_frame (state, menu, item);

              if (id != NULL)
                _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
              g_object_unref (menu);
            }

          return;
        }

      else if (g_str_equal (element_name, "section"))
        {
          const char *id;

          if (COLLECT (STRING | OPTIONAL, "id", &id))
            {
              GMenuItem *item;
              GMenu *menu;

              menu = g_menu_new ();
              item = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
              gtk_builder_menu_push_frame (state, menu, item);

              if (id != NULL)
                _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
              g_object_unref (menu);
            }

          return;
        }
    }

  if (state->frame.item)
    {
      /* Can have '<attribute>' or '<link>' here. */
      if (g_str_equal (element_name, "attribute"))
        {
          const char *typestr;
          const char *name;
          const char *ctxt;

          if (COLLECT (STRING,             "name", &name,
                       OPTIONAL | BOOLEAN, "translatable", &state->translatable,
                       OPTIONAL | STRING,  "context", &ctxt,
                       OPTIONAL | STRING,  "comments", NULL, /* ignore, just for translators */
                       OPTIONAL | STRING,  "type", &typestr))
            {
              if (typestr && !g_variant_type_string_is_valid (typestr))
                {
                  g_set_error (error, G_VARIANT_PARSE_ERROR,
                               G_VARIANT_PARSE_ERROR_INVALID_TYPE_STRING,
                               "Invalid GVariant type string '%s'", typestr);
                  return;
                }

              state->type = typestr ? g_variant_type_new (typestr) : NULL;
              state->string = g_string_new (NULL);
              state->attribute = g_strdup (name);
              state->context = g_strdup (ctxt);

              gtk_builder_menu_push_frame (state, NULL, NULL);
            }

          return;
        }

      if (g_str_equal (element_name, "link"))
        {
          const char *name;
          const char *id;

          if (COLLECT (STRING,            "name", &name,
                       STRING | OPTIONAL, "id",   &id))
            {
              GMenu *menu;

              menu = g_menu_new ();
              g_menu_item_set_link (state->frame.item, name, G_MENU_MODEL (menu));
              gtk_builder_menu_push_frame (state, menu, NULL);

              if (id != NULL)
                _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
              g_object_unref (menu);
            }

          return;
        }
    }

  {
    GPtrArray *element_stack;

    element_stack = gtk_buildable_parse_context_get_element_stack (context);

    if (element_stack->len > 1)
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Element <%s> not allowed inside <%s>"),
                   element_name,
                   (const char *) g_ptr_array_index (element_stack, element_stack->len - 2));

    else
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Element <%s> not allowed at toplevel"), element_name);
  }
}

static void
gtk_builder_menu_end_element (GtkBuildableParseContext  *context,
                              const char                *element_name,
                              gpointer                   user_data,
                              GError                   **error)
{
  GtkBuilderMenuState *state = user_data;

  gtk_builder_menu_pop_frame (state);

  if (state->string)
    {
      GVariant *value;
      char *text;

      text = g_string_free (state->string, FALSE);
      state->string = NULL;

      /* do the translation if necessary */
      if (state->translatable)
        {
          const char *translated;

          if (state->context)
            translated = g_dpgettext2 (state->parser_data->domain, state->context, text);
          else
            translated = g_dgettext (state->parser_data->domain, text);

         if (translated != text)
           {
             /* it's safe because we know that translated != text */
             g_free (text);
             text = g_strdup (translated);
           }
        }

      if (state->type == NULL)
        /* No type string specified -> it's a normal string. */
        g_menu_item_set_attribute (state->frame.item, state->attribute, "s", text);

      /* Else, we try to parse it according to the type string.  If
       * error is set here, it will follow us out, ending the parse.
       *
       * We still need to free everything, though, so ignore it here.
       */
      else if ((value = g_variant_parse (state->type, text, NULL, NULL, error)))
        {
          g_menu_item_set_attribute_value (state->frame.item, state->attribute, value);
          g_variant_unref (value);
        }

      if (state->type)
        {
          g_variant_type_free (state->type);
          state->type = NULL;
        }

      g_free (state->context);
      state->context = NULL;

      g_free (state->attribute);
      state->attribute = NULL;

      g_free (text);
    }
}

static void
gtk_builder_menu_text (GtkBuildableParseContext  *context,
                       const char                *text,
                       gsize                      text_len,
                       gpointer                   user_data,
                       GError                   **error)
{
  GtkBuilderMenuState *state = user_data;
  int i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      {
        if (state->string)
          g_string_append_len (state->string, text, text_len);

        else
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("Text may not appear inside <%s>"),
                       gtk_buildable_parse_context_get_element (context));
        break;
      }
}

static void
gtk_builder_menu_error (GtkBuildableParseContext *context,
                        GError                   *error,
                        gpointer                  user_data)
{
  GtkBuilderMenuState *state = user_data;

  while (state->frame.prev)
    {
      struct frame *prev = state->frame.prev;

      state->frame = *prev;

      g_free (prev);
    }

  if (state->string)
    g_string_free (state->string, TRUE);

  if (state->type)
    g_variant_type_free (state->type);

  g_free (state->attribute);
  g_free (state->context);

  g_free (state);
}

static GtkBuildableParser gtk_builder_menu_subparser =
{
  gtk_builder_menu_start_element,
  gtk_builder_menu_end_element,
  gtk_builder_menu_text,
  gtk_builder_menu_error
};

void
_gtk_builder_menu_start (ParserData   *parser_data,
                         const char   *element_name,
                         const char **attribute_names,
                         const char **attribute_values,
                         GError      **error)
{
  GtkBuilderMenuState *state;
  char *id;

  state = g_new0 (GtkBuilderMenuState, 1);
  state->parser_data = parser_data;
  gtk_buildable_parse_context_push (&parser_data->ctx, &gtk_builder_menu_subparser, state);

  if (COLLECT (STRING, "id", &id))
    {
      GMenu *menu;

      menu = g_menu_new ();
      _gtk_builder_add_object (state->parser_data->builder, id, G_OBJECT (menu));
      gtk_builder_menu_push_frame (state, menu, NULL);
      g_object_unref (menu);
    }
}

void
_gtk_builder_menu_end (ParserData *parser_data)
{
  GtkBuilderMenuState *state;

  state = gtk_buildable_parse_context_pop (&parser_data->ctx);
  gtk_builder_menu_pop_frame (state);

  g_assert (state->frame.prev == NULL);
  g_assert (state->frame.item == NULL);
  g_assert (state->frame.menu == NULL);
  g_free (state);
}
