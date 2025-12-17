/* Test of sequential list data type implementation.
   Copyright (C) 2006-2024 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2007.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "gl_array_list.h"

#include <stdlib.h>

#include "macros.h"

static const char *objects[15] =
  {
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o"
  };

#define RANDOM(n) (rand () % (n))
#define RANDOM_OBJECT() objects[RANDOM (SIZEOF (objects))]

static void
check_equals (gl_list_t list1, gl_list_t list2)
{
  size_t n, i;

  n = gl_list_size (list1);
  ASSERT (n == gl_list_size (list2));
  for (i = 0; i < n; i++)
    {
      ASSERT (gl_list_get_at (list1, i) == gl_list_get_at (list2, i));
    }
}

static void
check_equals_by_forward_iteration (gl_list_t list1, gl_list_t list2)
{
  size_t n = gl_list_size (list1);
  size_t i;
  gl_list_node_t node2;

  i = 0;
  node2 = gl_list_first_node (list2);
  while (i < n && node2 != NULL)
    {
      ASSERT (gl_list_get_at (list1, i) == gl_list_node_value (list2, node2));
      i++;
      node2 = gl_list_next_node (list2, node2);
    }
  ASSERT ((i == n) == (node2 == NULL));
}

static void
check_equals_by_backward_iteration (gl_list_t list1, gl_list_t list2)
{
  size_t n = gl_list_size (list1);
  size_t i;
  gl_list_node_t node2;

  i = n - 1;
  node2 = gl_list_last_node (list2);
  while (i != (size_t)(-1) && node2 != NULL)
    {
      ASSERT (gl_list_get_at (list1, i) == gl_list_node_value (list2, node2));
      i--;
      node2 = gl_list_previous_node (list2, node2);
    }
  ASSERT ((i == (size_t)(-1)) == (node2 == NULL));
}

int
main (int argc, char *argv[])
{
  gl_list_t list1, list2;

  /* Allow the user to provide a non-default random seed on the command line.  */
  if (argc > 1)
    srand (atoi (argv[1]));

  {
    size_t initial_size = RANDOM (50);
    const void **contents =
      (const void **) malloc (initial_size * sizeof (const void *));
    size_t i;
    unsigned int repeat;

    for (i = 0; i < initial_size; i++)
      contents[i] = RANDOM_OBJECT ();

    /* Create list1.  */
    list1 = gl_list_nx_create (GL_ARRAY_LIST, NULL, NULL, NULL, true,
                               initial_size, contents);
    ASSERT (list1 != NULL);
    /* Create list2.  */
    list2 = gl_list_nx_create_empty (GL_ARRAY_LIST, NULL, NULL, NULL, true);
    ASSERT (list2 != NULL);
    for (i = 0; i < initial_size; i++)
      ASSERT (gl_list_nx_add_last (list2, contents[i]) != NULL);

    check_equals (list1, list2);

    check_equals_by_forward_iteration (list1, list2);
    check_equals_by_backward_iteration (list1, list2);

    for (repeat = 0; repeat < 10000; repeat++)
      {
        unsigned int operation = RANDOM (18);
        switch (operation)
          {
          case 0:
            if (gl_list_size (list1) > 0)
              {
                size_t index = RANDOM (gl_list_size (list1));
                const char *obj = RANDOM_OBJECT ();
                gl_list_node_t node1, node2;

                node1 = gl_list_nx_set_at (list1, index, obj);
                ASSERT (node1 != NULL);
                ASSERT (gl_list_get_at (list1, index) == obj);
                ASSERT (gl_list_node_value (list1, node1) == obj);

                node2 = gl_list_nx_set_at (list2, index, obj);
                ASSERT (node2 != NULL);
                ASSERT (gl_list_get_at (list2, index) == obj);
                ASSERT (gl_list_node_value (list2, node2) == obj);

                if (index > 0)
                  {
                    ASSERT (gl_list_node_value (list1, gl_list_previous_node (list1, node1))
                            == gl_list_get_at (list1, index - 1));
                  }
                if (index + 1 < gl_list_size (list1))
                  {
                    ASSERT (gl_list_node_value (list1, gl_list_next_node (list1, node1))
                            == gl_list_get_at (list1, index + 1));
                  }
              }
            break;
          case 1:
            {
              const char *obj = RANDOM_OBJECT ();
              gl_list_node_t node1, node2;
              node1 = gl_list_search (list1, obj);
              node2 = gl_list_search (list2, obj);
              if (node1 == NULL)
                {
                  ASSERT (node2 == NULL);
                }
              else
                {
                  ASSERT (node2 != NULL);
                  ASSERT (gl_list_node_value (list1, node1) == obj);
                  ASSERT (gl_list_node_value (list2, node2) == obj);
                }
            }
            break;
          case 2:
            {
              const char *obj = RANDOM_OBJECT ();
              size_t index1, index2;
              index1 = gl_list_indexof (list1, obj);
              index2 = gl_list_indexof (list2, obj);
              if (index1 == (size_t)(-1))
                {
                  ASSERT (index2 == (size_t)(-1));
                }
              else
                {
                  ASSERT (index2 != (size_t)(-1));
                  ASSERT (gl_list_get_at (list1, index1) == obj);
                  ASSERT (gl_list_get_at (list2, index2) == obj);
                  ASSERT (index2 == index1);
                }
            }
            break;
          case 3: /* add 1 element */
            {
              const char *obj = RANDOM_OBJECT ();
              gl_list_node_t node1, node2;
              node1 = gl_list_nx_add_first (list1, obj);
              ASSERT (node1 != NULL);
              node2 = gl_list_nx_add_first (list2, obj);
              ASSERT (node2 != NULL);
              ASSERT (gl_list_node_value (list1, node1) == obj);
              ASSERT (gl_list_node_value (list2, node2) == obj);
              ASSERT (gl_list_get_at (list1, 0) == obj);
              ASSERT (gl_list_get_at (list2, 0) == obj);
            }
            break;
          case 4: /* add 1 element */
            {
              const char *obj = RANDOM_OBJECT ();
              gl_list_node_t node1, node2;
              node1 = gl_list_nx_add_last (list1, obj);
              ASSERT (node1 != NULL);
              node2 = gl_list_nx_add_last (list2, obj);
              ASSERT (node2 != NULL);
              ASSERT (gl_list_node_value (list1, node1) == obj);
              ASSERT (gl_list_node_value (list2, node2) == obj);
              ASSERT (gl_list_get_at (list1, gl_list_size (list1) - 1) == obj);
              ASSERT (gl_list_get_at (list2, gl_list_size (list2) - 1) == obj);
            }
            break;
          case 5: /* add 3 elements */
            {
              const char *obj0 = RANDOM_OBJECT ();
              const char *obj1 = RANDOM_OBJECT ();
              const char *obj2 = RANDOM_OBJECT ();
              gl_list_node_t node1, node2;
              node1 = gl_list_nx_add_first (list1, obj2);
              ASSERT (node1 != NULL);
              node1 = gl_list_nx_add_before (list1, node1, obj0);
              ASSERT (node1 != NULL);
              node1 = gl_list_nx_add_after (list1, node1, obj1);
              ASSERT (node1 != NULL);
              node2 = gl_list_nx_add_first (list2, obj2);
              ASSERT (node2 != NULL);
              node2 = gl_list_nx_add_before (list2, node2, obj0);
              ASSERT (node2 != NULL);
              node2 = gl_list_nx_add_after (list2, node2, obj1);
              ASSERT (node2 != NULL);
              ASSERT (gl_list_node_value (list1, node1) == obj1);
              ASSERT (gl_list_node_value (list2, node2) == obj1);
              ASSERT (gl_list_get_at (list1, 0) == obj0);
              ASSERT (gl_list_get_at (list1, 1) == obj1);
              ASSERT (gl_list_get_at (list1, 2) == obj2);
              ASSERT (gl_list_get_at (list2, 0) == obj0);
              ASSERT (gl_list_get_at (list2, 1) == obj1);
              ASSERT (gl_list_get_at (list2, 2) == obj2);
            }
            break;
          case 6: /* add 1 element */
            {
              size_t index = RANDOM (gl_list_size (list1) + 1);
              const char *obj = RANDOM_OBJECT ();
              gl_list_node_t node1, node2;
              node1 = gl_list_nx_add_at (list1, index, obj);
              ASSERT (node1 != NULL);
              node2 = gl_list_nx_add_at (list2, index, obj);
              ASSERT (node2 != NULL);
              ASSERT (gl_list_get_at (list1, index) == obj);
              ASSERT (gl_list_node_value (list1, node1) == obj);
              ASSERT (gl_list_get_at (list2, index) == obj);
              ASSERT (gl_list_node_value (list2, node2) == obj);
              if (index > 0)
                {
                  ASSERT (gl_list_node_value (list1, gl_list_previous_node (list1, node1))
                          == gl_list_get_at (list1, index - 1));
                }
              if (index + 1 < gl_list_size (list1))
                {
                  ASSERT (gl_list_node_value (list1, gl_list_next_node (list1, node1))
                          == gl_list_get_at (list1, index + 1));
                }
            }
            break;
          case 7: case 8: /* remove 1 element */
            if (gl_list_size (list1) > 0)
              {
                size_t n = gl_list_size (list1);
                const char *obj = gl_list_get_at (list1, RANDOM (n));
                gl_list_node_t node1, node2;
                node1 = gl_list_search (list1, obj);
                node2 = gl_list_search (list2, obj);
                ASSERT (node1 != NULL);
                ASSERT (node2 != NULL);
                ASSERT (gl_list_remove_node (list1, node1));
                ASSERT (gl_list_remove_node (list2, node2));
                ASSERT (gl_list_size (list1) == n - 1);
              }
            break;
          case 9: case 10: /* remove 1 element */
            if (gl_list_size (list1) > 0)
              {
                size_t n = gl_list_size (list1);
                size_t index = RANDOM (n);
                ASSERT (gl_list_remove_at (list1, index));
                ASSERT (gl_list_remove_at (list2, index));
                ASSERT (gl_list_size (list1) == n - 1);
              }
            break;
          case 11: /* remove first element */
            {
              size_t n = gl_list_size (list1);
              bool removed1 = gl_list_remove_first (list1);
              ASSERT (gl_list_remove_first (list2) == removed1);
              ASSERT (gl_list_size (list1) == n - (int) removed1);
            }
            break;
          case 12: /* remove last element */
            {
              size_t n = gl_list_size (list1);
              bool removed1 = gl_list_remove_last (list1);
              ASSERT (gl_list_remove_last (list2) == removed1);
              ASSERT (gl_list_size (list1) == n - (int) removed1);
            }
            break;
          case 13: case 14: /* remove 1 element */
            if (gl_list_size (list1) > 0)
              {
                size_t n = gl_list_size (list1);
                const char *obj = gl_list_get_at (list1, RANDOM (n));
                ASSERT (gl_list_remove (list1, obj));
                ASSERT (gl_list_remove (list2, obj));
                ASSERT (gl_list_size (list1) == n - 1);
              }
            break;
          case 15:
            if (gl_list_size (list1) > 0)
              {
                size_t n = gl_list_size (list1);
                const char *obj = "xyzzy";
                ASSERT (!gl_list_remove (list1, obj));
                ASSERT (!gl_list_remove (list2, obj));
                ASSERT (gl_list_size (list1) == n);
              }
            break;
          case 16:
            {
              size_t n = gl_list_size (list1);
              gl_list_iterator_t iter1, iter2;
              const void *elt;
              iter1 = gl_list_iterator (list1);
              iter2 = gl_list_iterator (list2);
              for (i = 0; i < n; i++)
                {
                  ASSERT (gl_list_iterator_next (&iter1, &elt, NULL));
                  ASSERT (gl_list_get_at (list1, i) == elt);
                  ASSERT (gl_list_iterator_next (&iter2, &elt, NULL));
                  ASSERT (gl_list_get_at (list2, i) == elt);
                }
              ASSERT (!gl_list_iterator_next (&iter1, &elt, NULL));
              ASSERT (!gl_list_iterator_next (&iter2, &elt, NULL));
              gl_list_iterator_free (&iter1);
              gl_list_iterator_free (&iter2);
            }
            break;
          case 17:
            {
              size_t end = RANDOM (gl_list_size (list1) + 1);
              size_t start = RANDOM (end + 1);
              gl_list_iterator_t iter1, iter2;
              const void *elt;
              iter1 = gl_list_iterator_from_to (list1, start, end);
              iter2 = gl_list_iterator_from_to (list2, start, end);
              for (i = start; i < end; i++)
                {
                  ASSERT (gl_list_iterator_next (&iter1, &elt, NULL));
                  ASSERT (gl_list_get_at (list1, i) == elt);
                  ASSERT (gl_list_iterator_next (&iter2, &elt, NULL));
                  ASSERT (gl_list_get_at (list2, i) == elt);
                }
              ASSERT (!gl_list_iterator_next (&iter1, &elt, NULL));
              ASSERT (!gl_list_iterator_next (&iter2, &elt, NULL));
              gl_list_iterator_free (&iter1);
              gl_list_iterator_free (&iter2);
            }
            break;
          }
        check_equals (list1, list2);
      }

    gl_list_free (list1);
    gl_list_free (list2);
    free (contents);
  }

  return test_exit_status;
}
