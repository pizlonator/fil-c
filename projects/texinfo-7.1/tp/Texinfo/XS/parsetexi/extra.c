/* Copyright 2010-2023 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <string.h>

#include "parser.h"

static void
add_associated_info_key (ASSOCIATED_INFO *a, char *key, void *value,
                         enum extra_type type)
{
  int i;
  for (i = 0; i < a->info_number; i++)
    {
      if (!strcmp (a->info[i].key, key))
        break;
    }
  if (i == a->info_number)
    {
      if (a->info_number == a->info_space)
        {
          a->info = realloc (a->info,
                              (a->info_space += 5) * sizeof (KEY_PAIR));
          if (!a->info)
            fatal ("realloc failed");
        }
      a->info_number++;
    }

  a->info[i].key = key;
  a->info[i].value = value;
  a->info[i].type = type;
}

/* Add an extra key that is a reference to another element (for example, 
   'associated_section' on a node command element. */
void
add_extra_element (ELEMENT *e, char *key, ELEMENT *value)
{
  add_associated_info_key (&e->extra_info, key,
                           value, extra_element);
}

/* Add an extra key that is a reference to another element that is
   out-of-tree, i.e., not referenced anywhere in the tree.
   Unused in 2023.
*/
void
add_extra_element_oot (ELEMENT *e, char *key, ELEMENT *value)
{
  add_associated_info_key (&e->extra_info,
                           key, value, extra_element_oot);
}

void
add_info_element_oot (ELEMENT *e, char *key, ELEMENT *value)
{
  add_associated_info_key (&e->info_info,
                           key, value, extra_element_oot);
}

/* Add an extra key that is a reference to the contents array of another
   element (for example, 'node_content' on a node command element). */
void
add_extra_contents (ELEMENT *e, char *key, ELEMENT *value)
{
  add_associated_info_key (&e->extra_info,
                           key, value, extra_contents);
}

/* Add an extra key that is a reference to the text field of another
   element. */
void
add_extra_text (ELEMENT *e, char *key, ELEMENT *value)
{
  add_associated_info_key (&e->extra_info, key, value, extra_text);
}

void
add_extra_misc_args (ELEMENT *e, char *key, ELEMENT *value)
{
  if (!value) return;
  add_associated_info_key (&e->extra_info,
                           key, value, extra_misc_args);
}

void
add_extra_string (ELEMENT *e, char *key, char *value)
{
  add_associated_info_key (&e->extra_info, key,
                           value, extra_string);
}

void
add_info_string (ELEMENT *e, char *key, char *value)
{
  add_associated_info_key (&e->info_info, key, value, extra_string);
}

void
add_extra_string_dup (ELEMENT *e, char *key, char *value)
{
  add_associated_info_key (&e->extra_info,
                           key, strdup (value), extra_string);
}

void
add_info_string_dup (ELEMENT *e, char *key, char *value)
{
  add_associated_info_key (&e->info_info,
                           key, strdup (value), extra_string);
}

void
add_extra_integer (ELEMENT *e, char *key, long value)
{
  add_associated_info_key (&e->extra_info,
                           key, (void *) value, extra_integer);
}

KEY_PAIR *
lookup_associated_info (ASSOCIATED_INFO *a, char *key)
{
  int i;
  for (i = 0; i < a->info_number; i++)
    {
      if (!strcmp (a->info[i].key, key))
        return &a->info[i];
    }
  return 0;
}

ELEMENT *
lookup_extra_element (ELEMENT *e, char *key)
{
  KEY_PAIR *k;
  k = lookup_associated_info (&e->extra_info, key);
  if (!k)
    return 0;
  return (ELEMENT *) k->value;
}

KEY_PAIR *
lookup_extra (ELEMENT *e, char *key)
{
  return lookup_associated_info (&e->extra_info, key);
}

ELEMENT *
lookup_info_element (ELEMENT *e, char *key)
{
  KEY_PAIR *k;
  k = lookup_associated_info (&e->info_info, key);
  if (!k)
    return 0;
  return (ELEMENT *) k->value;
}


KEY_PAIR *
lookup_info (ELEMENT *e, char *key)
{
  return lookup_associated_info (&e->info_info, key);
}

