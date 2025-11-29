/* convert-emoji.c: A tool to create an Emoji data GVariant
 * Copyright 2017, Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* Build with gcc -o convert-emoji convert-emoji.c `pkg-config --cflags --libs json-glib-1.0`
 */

/* Reads data from the json files in emojibase, expecting
 * language-specific data.raw.json as input
 */

/* The format of the generated data is: a(aussasasu).
 * Each member of the array has the following fields:
 * au - sequence of unicode codepoints, including the emoji presentation
 *      selector (FE0F) where necessary. skin tone variations are represented
 *      with either the first tone code point (1F3FB) or 0. the choice indicates
 *      the handling of the generic sequence (i.e., no tone), which may have a
 *      default text presentation and thus require the emoji presentation
 *      selector (unlike sequences with a tone, which are always presented as
 *      emojis). 0 indicates the text case, that is, replace this code point
 *      with FE0F, while 1F3FB indicates this code point should be omitted.
 * s  - name in english, e.g. "man worker"
 * s  - name in locale
 * as - keywords in english, e.g. "man", "worker"
 * as - keywords in locale
 * u  - the group that this item belongs to:
 *      0: smileys-emotion
 *      1: people-body
 *      2: component
 *      3: animals-nature
 *      4: food-drink
 *      5: travel-places
 *      6: activities
 *      7: objects
 *      8: symbols
 *      9: flags
 */
#include <json-glib/json-glib.h>
#include <string.h>

gboolean
parse_code (GVariantBuilder *b,
            const char      *code,
            gboolean         needs_presentation_selector)
{
  g_auto(GStrv) strv = NULL;
  int j;

  strv = g_strsplit (code, "-", -1);
  for (j = 0; strv[j]; j++)
    {
      guint32 u;
      char *end;

      u = (guint32) g_ascii_strtoull (strv[j], &end, 16);
      if (*end != '\0')
        {
          g_error ("failed to parse code: %s\n", strv[j]);
          return FALSE;
        }
      if (0x1f3fb <= u && u <= 0x1f3ff)
        {
          if (needs_presentation_selector)
            {
              if (strv[j+1])
                {
                  g_error ("unexpected inner skin tone in default-text generic sequence: %s\n", code);
                  return FALSE;
                }
              g_variant_builder_add (b, "u", 0);
              needs_presentation_selector = FALSE;
            }
          else
            {
              g_variant_builder_add (b, "u", 0x1f3fb);
            }
        }
      else
        {
          g_variant_builder_add (b, "u", u);
        }
    }

  if (needs_presentation_selector)
    g_variant_builder_add (b, "u", 0xfe0f);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  JsonParser *parser;
  JsonNode *root;
  JsonParser *parser_en;
  JsonNode *root_en;
  JsonObject *ro;
  JsonArray *array;
  JsonArray *array_en;
  JsonNode *node;
  const char *unicode;
  JsonObjectIter iter;
  GError *error = NULL;
  guint length, length_en, i;
  GVariantBuilder builder;
  GVariant *v;
  GString *s;
  GHashTable *names;

  if (argc != 4) //0 -> compiled file, 1 -> en/data.raw.json, 2 -> de/data.raw.json, 3 -> de.data
    {
      g_print ("Usage: emoji-convert INPUT1 INPUT2 OUTPUT\nINPUT1 should be raw json data for English\nINPUT2 should be raw json data for the locale\n");
      return 1;
    }

  parser = json_parser_new ();
  parser_en = json_parser_new ();

  if (!json_parser_load_from_file (parser_en, argv[1], &error))
    {
      g_error ("%s", error->message);
      return 1;
    }
  if (!json_parser_load_from_file (parser, argv[2], &error))
    {
      g_error ("%s", error->message);
      return 1;
    }
  root = json_parser_get_root (parser);
  array = json_node_get_array (root);
  length = json_array_get_length (array);
  root_en = json_parser_get_root (parser_en);
  array_en = json_node_get_array (root_en);
  length_en = json_array_get_length (array_en);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(aussasasu)"));
  for (i = 0; i < length; i++)
  {
    JsonObject *obj = json_array_get_object_element (array, i);
    JsonObject *obj_en = json_array_get_object_element (array_en, i);
    GVariantBuilder b1;
    GVariantBuilder b2;
    GVariantBuilder b3;
    guint group;
    const char *name;
    const char *name_en;
    char *code;
    const char *text;
    gboolean needs_presentation_selector;

    if (!json_object_has_member (obj, "group"))
      continue;
    if (!json_object_has_member (obj_en, "group"))
      continue;

    group = json_object_get_int_member (obj, "group");
    name = json_object_get_string_member (obj, "label");
    name_en = json_object_get_string_member (obj_en, "label");

    if (g_str_has_suffix (name_en, "skin tone"))
      continue;

    if (json_object_has_member (obj, "skins") && json_object_has_member (obj_en, "skins"))
      {
        JsonArray *a2 = json_object_get_array_member (obj, "skins");
        JsonNode *n2 = json_array_get_element (a2, 0);
        JsonObject *o2 = json_node_get_object (n2);
        code = g_strdup (json_object_get_string_member (o2, "hexcode"));
      }
    else
      {
        code = g_strdup (json_object_get_string_member (obj, "hexcode"));
      }

    text = json_object_get_string_member (obj, "text");
    needs_presentation_selector = *text != '\0' && json_object_get_int_member (obj, "type") == 0;

    g_variant_builder_init (&b1, G_VARIANT_TYPE ("au"));
    if (!parse_code (&b1, code, needs_presentation_selector))
      return 1;

    g_variant_builder_init (&b2, G_VARIANT_TYPE ("as"));
    if (json_object_has_member (obj_en, "tags"))
      {
        JsonArray *tags_en = json_object_get_array_member (obj_en, "tags");
        for (int j = 0; j < json_array_get_length (tags_en); j++)
        {
          g_variant_builder_add (&b2, "s", json_array_get_string_element (tags_en, j));
        }
      }

    g_variant_builder_init (&b3, G_VARIANT_TYPE ("as"));
    if (json_object_has_member (obj, "tags"))
      {
        JsonArray *tags = json_object_get_array_member (obj, "tags");
        for (int j = 0; j < json_array_get_length (tags); j++)
        {
          g_variant_builder_add (&b3, "s", json_array_get_string_element (tags, j));
        }
      }
    g_variant_builder_add (&builder, "(aussasasu)", &b1, name_en, name, &b2, &b3, group);
  }
  v = g_variant_builder_end (&builder);
  if (g_str_has_suffix (argv[3], ".json"))
    {
      JsonNode *node;
      char *out;

      node = json_gvariant_serialize (v);
      out = json_to_string (node, TRUE);
      if (!g_file_set_contents (argv[3], out, -1, &error))
        {
          g_error ("%s", error->message);
          return 1;
        }
    }
  else
    {
      GBytes *bytes;

      bytes = g_variant_get_data_as_bytes (v);
      if (!g_file_set_contents (argv[3], g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), &error))
        {
          g_error ("%s", error->message);
          return 1;
        }
    }

  return 0;
}
