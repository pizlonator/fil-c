/*
 * Copyright (C) 2024 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "cc-password-utils.h"

#include <gio/gio.h>
#include <glib.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>

#define DICEWARE_CORPUS_GLOB "/usr/share/dict/*"
#define SPECIAL_CHARACTERS "-!\"#$%&()*,./:;?@[]^_`{|}~+<=>"
#define WORD_SEPARATORS " -_&+,;:."

static char *
get_word_at_line (const char *file_path,
                  guint       line_number)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInputStream) input_stream = NULL;
  g_autoptr (GDataInputStream) data = NULL;
  g_autofree char *line = NULL;
  gsize length;
  guint line_count = 0;

  file = g_file_new_for_path (file_path);

  input_stream = g_file_read (file, NULL, &error);

  if (error != NULL)
    {
      g_warning ("Failed to open file: %s", error->message);
      return NULL;
    }

  data = g_data_input_stream_new (G_INPUT_STREAM (input_stream));

  while ((line = g_data_input_stream_read_line_utf8 (data, &length, NULL, &error)) != NULL)
    {
      if (line_count == line_number)
        return g_str_to_ascii (line, NULL);

      g_clear_pointer (&line, g_free);
      line_count++;
    }

  if (error != NULL)
    {
      g_warning ("Failed to read file: %s", error->message);
      return NULL;
    }

  if (line_count == 0)
    {
      g_warning ("File is empty");
      return NULL;
    }

  if (line_number >= line_count)
    return get_word_at_line (file_path, line_number % line_count);

  return NULL;
}

static gboolean
is_word_separator (char character)
{
  return strchr (WORD_SEPARATORS, character) != NULL;
}

static char
generate_word_separator (void)
{
  return WORD_SEPARATORS[g_random_int_range (0, strlen (WORD_SEPARATORS))];
}

static char
generate_special_character (void)
{
  return SPECIAL_CHARACTERS[g_random_int_range (0, strlen (SPECIAL_CHARACTERS))];
}

static char
generate_digit (void)
{
  return g_random_int_range (0, 10) + '0';
}

static char *
get_random_file_from_glob (const char *pattern)
{
  glob_t glob_result;
  int ret = 0;
  int index;
  g_autofree char *file = NULL;

  ret = glob (pattern, 0, NULL, &glob_result);
  if (ret != 0)
    goto out;

  if (glob_result.gl_pathc == 0)
    goto out;

  index = g_random_int_range (0, glob_result.gl_pathc);
  file = g_strdup (glob_result.gl_pathv[index]);

out:
  globfree (&glob_result);
  return g_steal_pointer (&file);
}

char *
cc_generate_password (void)
{
  g_autoptr(GString) password_string = NULL;
  g_autofree char *password = NULL;
  static const size_t min_number_of_words = 2;
  size_t i;
  char *p = NULL;
  gboolean needs_digit = TRUE;
  gboolean needs_special_character = TRUE;
  gboolean needs_uppercase = TRUE;
  gboolean last_character_trimmable = TRUE;

  password_string = g_string_new (NULL);

  i = 0;
  while (password_string->len < 16 || i < min_number_of_words)
    {
      g_autofree char *file_path = NULL;
      int word_offset = g_random_int ();
      g_autofree char *word = NULL;

      file_path = get_random_file_from_glob (DICEWARE_CORPUS_GLOB);

      if (!file_path)
        return NULL;

      word = get_word_at_line (file_path, word_offset);

      if (word == NULL)
        return NULL;

      if (strlen (word) > 10)
        continue;

      g_string_append (password_string, word);
      g_string_append_c (password_string, ' ');
      i++;
    }

  password = g_string_free_and_steal (g_steal_pointer (&password_string));

  while (needs_uppercase || needs_digit || needs_special_character || strstr (password, " ") != NULL)
    {
      for (p = password; *p != '\0'; p++)
        {
          if (p == password || is_word_separator (p[-1]))
            {
              if (g_random_int_range (0, 2) == 0)
                {
                  *p = g_ascii_toupper (*p);
                  needs_uppercase = FALSE;
                }
            }

          if (!is_word_separator (*p))
            continue;

          if (needs_digit && g_random_int_range (0, strlen (password)) == 0)
            {
              *p = generate_digit ();
              needs_digit = FALSE;

              if (p[1] == '\0')
                last_character_trimmable = FALSE;
            }
          else if (needs_special_character && g_random_int_range (0, strlen (password)) == 0)
            {
              *p = generate_special_character ();
              needs_special_character = FALSE;

              if (p[1] == '\0')
                last_character_trimmable = FALSE;
            }
          else if (!needs_digit && !needs_special_character)
            {
              *p = generate_word_separator ();
            }
        }
    }

  if (last_character_trimmable)
    password[strlen (password) - 1] = '\0';

  return g_steal_pointer (&password);
}
