/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkcssselectorprivate.h"
#include "gtkcssnodeprivate.h"

#include <stdlib.h>
#include <string.h>

#include "gtkcssprovider.h"

#include <errno.h>
#if defined(_MSC_VER) && _MSC_VER >= 1500
# include <intrin.h>
#endif

/*
 * @GTK_CSS_SELECTOR_CATEGORY_SIMPLE: A simple selector
 * @GTK_CSS_SELECTOR_CATEGORY_SIMPLE_RADICAL: A simple selector that matches
 *   what change tracking considers a "radical change"
 * @GTK_CSS_SELECTOR_SIBLING: A selector matching siblings
 * @GTK_CSS_SELECTOR_CATEGORY_PARENT: A selector matching a parent or other
 *   ancestor
 *
 * Categorize the selectors. This helps in various loops when matching.
 */
typedef enum {
  GTK_CSS_SELECTOR_CATEGORY_SIMPLE,
  GTK_CSS_SELECTOR_CATEGORY_SIMPLE_RADICAL,
  GTK_CSS_SELECTOR_CATEGORY_PARENT,
  GTK_CSS_SELECTOR_CATEGORY_SIBLING,
} GtkCssSelectorCategory;

typedef struct _GtkCssSelectorClass GtkCssSelectorClass;

struct _GtkCssSelectorClass {
  const char        *name;
  GtkCssSelectorCategory category;

  void              (* print)                   (const GtkCssSelector       *selector,
                                                 GString                    *string);
  /* NULL or an iterator that returns the next node or %NULL if there are no
   * more nodes.
   * Call it like:
   * for (iter = gtk_css_selector_iterator (node, NULL);
   *      iter;
   *      iter = gtk_css_selector_iterator (node, iter))
   *   {
   *     do_stuff();
   *   }
   */
  GtkCssNode *      (* iterator)                (const GtkCssSelector       *selector,
                                                 GtkCssNode                 *node,
                                                 GtkCssNode                 *current);
  gboolean          (* match_one)               (const GtkCssSelector       *selector,
                                                 GtkCssNode                 *node);
  GtkCssChange      (* get_change)              (const GtkCssSelector       *selector,
			                         GtkCssChange                previous_change);
  void              (* add_specificity)         (const GtkCssSelector       *selector,
                                                 guint                      *ids,
                                                 guint                      *classes,
                                                 guint                      *elements);
  guint             (* hash_one)                (const GtkCssSelector       *selector);
  int               (* compare_one)             (const GtkCssSelector       *a,
				                 const GtkCssSelector       *b);
};

typedef enum {
  POSITION_FORWARD,
  POSITION_BACKWARD,
  POSITION_ONLY,
} PositionType;
#define POSITION_TYPE_BITS 4
#define POSITION_NUMBER_BITS ((sizeof (gpointer) * 8 - POSITION_TYPE_BITS) / 2)

union _GtkCssSelector
{
  const GtkCssSelectorClass     *class;         /* type of check this selector does */
  struct {
    const GtkCssSelectorClass   *class;
    GQuark                       name;
  }                              id;
  struct {
    const GtkCssSelectorClass   *class;
    GQuark                       style_class;
  }                              style_class;
  struct {
    const GtkCssSelectorClass   *class;
    GQuark                       name;
  }                              name;
  struct {
    const GtkCssSelectorClass   *class;
    GtkStateFlags                state;
  }                              state;
  struct {
    const GtkCssSelectorClass   *class;
    PositionType                 type :POSITION_TYPE_BITS;
    gssize                       a :POSITION_NUMBER_BITS;
    gssize                       b :POSITION_NUMBER_BITS;
  }                              position;
};

#define GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET G_MAXINT32
struct _GtkCssSelectorTree
{
  GtkCssSelector selector;
  gint32 parent_offset;
  gint32 previous_offset;
  gint32 sibling_offset;
  gint32 matches_offset; /* pointers that we return as matches if selector matches */
};

static gboolean
gtk_css_selector_equal (const GtkCssSelector *a,
			const GtkCssSelector *b)
{
  return
    a->class == b->class &&
    a->class->compare_one (a, b) == 0;
}

static guint
gtk_css_selector_hash_one (const GtkCssSelector *selector)
{
  return selector->class->hash_one (selector);
}

static inline gpointer *
gtk_css_selector_tree_get_matches (const GtkCssSelectorTree *tree)
{
  if (tree->matches_offset == GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
    return NULL;

  return (gpointer *) ((guint8 *)tree + tree->matches_offset);
}

static void
gtk_css_selector_matches_insert_sorted (GtkCssSelectorMatches *matches,
                                        gpointer               data)
{
  guint i;

  for (i = 0; i < gtk_css_selector_matches_get_size (matches); i++)
    {
      gpointer elem = gtk_css_selector_matches_get (matches, i);

      if (data == elem)
        return;

      if (data < elem)
        break;
    }

  gtk_css_selector_matches_splice (matches, i, 0, FALSE, (gpointer[1]) { data }, 1);
}

static inline gboolean
gtk_css_selector_match_one (const GtkCssSelector *selector,
                            GtkCssNode           *node)
{
  return selector->class->match_one (selector, node);
}

static inline GtkCssNode *
gtk_css_selector_iterator (const GtkCssSelector *selector,
                           GtkCssNode           *node,
                           GtkCssNode           *current)
{
  return selector->class->iterator (selector, node, current);
}

static int
gtk_css_selector_compare_one (const GtkCssSelector *a, const GtkCssSelector *b)
{
  if (a->class != b->class)
    return strcmp (a->class->name, b->class->name);
  else
    return a->class->compare_one (a, b);
}
  
static const GtkCssSelector *
gtk_css_selector_previous (const GtkCssSelector *selector)
{
  selector = selector + 1;

  return selector->class ? selector : NULL;
}

static inline const GtkCssSelectorTree *
gtk_css_selector_tree_at_offset (const GtkCssSelectorTree *tree,
				 gint32 offset)
{
  if (offset == GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
    return NULL;

  return (GtkCssSelectorTree *) ((guint8 *)tree + offset);
}

static inline const GtkCssSelectorTree *
gtk_css_selector_tree_get_parent (const GtkCssSelectorTree *tree)
{
  return gtk_css_selector_tree_at_offset (tree, tree->parent_offset);
}

static inline const GtkCssSelectorTree *
gtk_css_selector_tree_get_previous (const GtkCssSelectorTree *tree)
{
  return gtk_css_selector_tree_at_offset (tree, tree->previous_offset);
}

static inline const GtkCssSelectorTree *
gtk_css_selector_tree_get_sibling (const GtkCssSelectorTree *tree)
{
  return gtk_css_selector_tree_at_offset (tree, tree->sibling_offset);
}

/* DEFAULTS */

static void
gtk_css_selector_default_add_specificity (const GtkCssSelector *selector,
                                          guint                *ids,
                                          guint                *classes,
                                          guint                *elements)
{
  /* no specificity changes */
}
 
static GtkCssNode *
gtk_css_selector_default_iterator (const GtkCssSelector *selector,
                                   GtkCssNode           *node,
                                   GtkCssNode           *current)
{
  if (current)
    return NULL;
  else
    return node;
}

static gboolean
gtk_css_selector_default_match_one (const GtkCssSelector *selector,
                                    GtkCssNode           *node)
{
  return TRUE;
}

static guint
gtk_css_selector_default_hash_one (const GtkCssSelector *selector)
{
  return GPOINTER_TO_UINT (selector->class);
}

static int
gtk_css_selector_default_compare_one (const GtkCssSelector *a,
                                      const GtkCssSelector *b)
{
  return 0;
}

/* DESCENDANT */

static void
gtk_css_selector_descendant_print (const GtkCssSelector *selector,
                                   GString              *string)
{
  g_string_append_c (string, ' ');
}

static GtkCssNode *
gtk_css_selector_descendant_iterator (const GtkCssSelector *selector,
                                      GtkCssNode           *node,
                                      GtkCssNode           *current)
{
  return gtk_css_node_get_parent (current ? current : node);
}

static GtkCssChange
gtk_css_selector_descendant_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_child (previous_change);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_DESCENDANT = {
  "descendant",
  GTK_CSS_SELECTOR_CATEGORY_PARENT,
  gtk_css_selector_descendant_print,
  gtk_css_selector_descendant_iterator,
  gtk_css_selector_default_match_one,
  gtk_css_selector_descendant_get_change,
  gtk_css_selector_default_add_specificity,
  gtk_css_selector_default_hash_one,
  gtk_css_selector_default_compare_one,
};

/* CHILD */

static void
gtk_css_selector_child_print (const GtkCssSelector *selector,
                              GString              *string)
{
  g_string_append (string, " > ");
}

static GtkCssNode *
gtk_css_selector_child_iterator (const GtkCssSelector *selector,
                                 GtkCssNode           *node,
                                 GtkCssNode           *current)
{
  if (current == NULL)
    return gtk_css_node_get_parent (node);

  return NULL;
}

static GtkCssChange
gtk_css_selector_child_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_child (previous_change);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_CHILD = {
  "child",
  GTK_CSS_SELECTOR_CATEGORY_PARENT,
  gtk_css_selector_child_print,
  gtk_css_selector_child_iterator,
  gtk_css_selector_default_match_one,
  gtk_css_selector_child_get_change,
  gtk_css_selector_default_add_specificity,
  gtk_css_selector_default_hash_one,
  gtk_css_selector_default_compare_one,
};

/* SIBLING */

static void
gtk_css_selector_sibling_print (const GtkCssSelector *selector,
                                GString              *string)
{
  g_string_append (string, " ~ ");
}

static GtkCssNode *
get_previous_visible_sibling (GtkCssNode *node)
{
  do {
    node = gtk_css_node_get_previous_sibling (node);
  } while (node && !gtk_css_node_get_visible (node));

  return node;
}

static GtkCssNode *
get_next_visible_sibling (GtkCssNode *node)
{
  do {
    node = gtk_css_node_get_next_sibling (node);
  } while (node && !gtk_css_node_get_visible (node));

  return node;
}

static GtkCssNode *
gtk_css_selector_sibling_iterator (const GtkCssSelector *selector,
                                   GtkCssNode           *node,
                                   GtkCssNode           *current)
{
  return get_previous_visible_sibling (current ? current : node);
}

static GtkCssChange
gtk_css_selector_sibling_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_sibling (previous_change);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_SIBLING = {
  "sibling",
  GTK_CSS_SELECTOR_CATEGORY_SIBLING,
  gtk_css_selector_sibling_print,
  gtk_css_selector_sibling_iterator,
  gtk_css_selector_default_match_one,
  gtk_css_selector_sibling_get_change,
  gtk_css_selector_default_add_specificity,
  gtk_css_selector_default_hash_one,
  gtk_css_selector_default_compare_one,
};

/* ADJACENT */

static void
gtk_css_selector_adjacent_print (const GtkCssSelector *selector,
                                 GString              *string)
{
  g_string_append (string, " + ");
}

static GtkCssNode *
gtk_css_selector_adjacent_iterator (const GtkCssSelector *selector,
                                    GtkCssNode           *node,
                                    GtkCssNode           *current)
{
  if (current == NULL)
    return get_previous_visible_sibling (node);

  return NULL;
}

static GtkCssChange
gtk_css_selector_adjacent_get_change (const GtkCssSelector *selector, GtkCssChange previous_change)
{
  return _gtk_css_change_for_sibling (previous_change);
}

static const GtkCssSelectorClass GTK_CSS_SELECTOR_ADJACENT = {
  "adjacent",
  GTK_CSS_SELECTOR_CATEGORY_SIBLING,
  gtk_css_selector_adjacent_print,
  gtk_css_selector_adjacent_iterator,
  gtk_css_selector_default_match_one,
  gtk_css_selector_adjacent_get_change,
  gtk_css_selector_default_add_specificity,
  gtk_css_selector_default_hash_one,
  gtk_css_selector_default_compare_one,
};

/* SIMPLE SELECTOR DEFINE */

#define DEFINE_SIMPLE_SELECTOR(n, \
                               c, \
                               print_func, \
                               match_func, \
                               hash_func, \
                               comp_func, \
                               increase_id_specificity, \
                               increase_class_specificity, \
                               increase_element_specificity, \
                               ignore_for_change) \
static void \
gtk_css_selector_ ## n ## _print (const GtkCssSelector *selector, \
                                    GString              *string) \
{ \
  print_func (selector, string); \
} \
\
static void \
gtk_css_selector_not_ ## n ## _print (const GtkCssSelector *selector, \
                                      GString              *string) \
{ \
  g_string_append (string, ":not("); \
  print_func (selector, string); \
  g_string_append (string, ")"); \
} \
\
static gboolean \
gtk_css_selector_not_ ## n ## _match_one (const GtkCssSelector *selector, \
                                          GtkCssNode           *node) \
{ \
  return !match_func (selector, node); \
} \
\
static GtkCssChange \
gtk_css_selector_ ## n ## _get_change (const GtkCssSelector *selector, GtkCssChange previous_change) \
{ \
  return previous_change | GTK_CSS_CHANGE_ ## c; \
} \
\
static void \
gtk_css_selector_ ## n ## _add_specificity (const GtkCssSelector *selector, \
                                            guint                *ids, \
                                            guint                *classes, \
                                            guint                *elements) \
{ \
  if (increase_id_specificity) \
    { \
      (*ids)++; \
    } \
  if (increase_class_specificity) \
    { \
      (*classes)++; \
    } \
  if (increase_element_specificity) \
    { \
      (*elements)++; \
    } \
} \
\
static const GtkCssSelectorClass GTK_CSS_SELECTOR_ ## c = { \
  G_STRINGIFY(n), \
  ignore_for_change ? GTK_CSS_SELECTOR_CATEGORY_SIMPLE : GTK_CSS_SELECTOR_CATEGORY_SIMPLE_RADICAL, \
  gtk_css_selector_ ## n ## _print, \
  gtk_css_selector_default_iterator, \
  match_func, \
  gtk_css_selector_ ## n ## _get_change, \
  gtk_css_selector_ ## n ## _add_specificity, \
  hash_func, \
  comp_func, \
};\
\
static const GtkCssSelectorClass GTK_CSS_SELECTOR_NOT_ ## c = { \
  "not_" G_STRINGIFY(n), \
  GTK_CSS_SELECTOR_CATEGORY_SIMPLE, \
  gtk_css_selector_not_ ## n ## _print, \
  gtk_css_selector_default_iterator, \
  gtk_css_selector_not_ ## n ## _match_one, \
  gtk_css_selector_ ## n ## _get_change, \
  gtk_css_selector_ ## n ## _add_specificity, \
  hash_func, \
  comp_func, \
};

/* ANY */

static void
print_any (const GtkCssSelector *selector,
           GString              *string)
{
  g_string_append_c (string, '*');
}

static gboolean
match_any (const GtkCssSelector *selector,
           GtkCssNode           *node)
{
  return TRUE;
}

#undef GTK_CSS_CHANGE_ANY
#define GTK_CSS_CHANGE_ANY 0
DEFINE_SIMPLE_SELECTOR(any, ANY, print_any, match_any, 
                       gtk_css_selector_default_hash_one, gtk_css_selector_default_compare_one,
                       FALSE, FALSE, FALSE, TRUE)
#undef GTK_CSS_CHANGE_ANY

/* NAME */

static void
print_name (const GtkCssSelector *selector,
            GString              *string)
{
  g_string_append (string, g_quark_to_string (selector->name.name));
}

static gboolean
match_name (const GtkCssSelector *selector,
            GtkCssNode           *node)
{
  return gtk_css_node_get_name (node) == selector->name.name;
}

static guint
hash_name (const GtkCssSelector *a)
{
  return gtk_css_hash_name (a->name.name);
}

static int
comp_name (const GtkCssSelector *a,
           const GtkCssSelector *b)
{
  return a->name.name - b->name.name;
}

DEFINE_SIMPLE_SELECTOR(name, NAME, print_name, match_name, hash_name, comp_name, FALSE, FALSE, TRUE, FALSE)

/* CLASS */

static void
print_class (const GtkCssSelector *selector,
             GString              *string)
{
  g_string_append_c (string, '.');
  g_string_append (string, g_quark_to_string (selector->style_class.style_class));
}

static gboolean
match_class (const GtkCssSelector *selector,
             GtkCssNode           *node)
{
  return gtk_css_node_has_class (node, selector->style_class.style_class);
}

static guint
hash_class (const GtkCssSelector *a)
{
  return gtk_css_hash_class (a->style_class.style_class);
}

static int
comp_class (const GtkCssSelector *a,
            const GtkCssSelector *b)
{
  if (a->style_class.style_class < b->style_class.style_class)
    return -1;
  if (a->style_class.style_class > b->style_class.style_class)
    return 1;
  else
    return 0;
}

DEFINE_SIMPLE_SELECTOR(class, CLASS, print_class, match_class, hash_class, comp_class, FALSE, TRUE, FALSE, FALSE)

/* ID */

static void
print_id (const GtkCssSelector *selector,
          GString              *string)
{
  g_string_append_c (string, '#');
  g_string_append (string, g_quark_to_string (selector->id.name));
}

static gboolean
match_id (const GtkCssSelector *selector,
          GtkCssNode           *node)
{
  return gtk_css_node_get_id (node) == selector->id.name;
}

static guint
hash_id (const GtkCssSelector *a)
{
  return gtk_css_hash_id (a->id.name);
}

static int
comp_id (const GtkCssSelector *a,
	 const GtkCssSelector *b)
{
  return a->id.name - b->id.name;
}

DEFINE_SIMPLE_SELECTOR(id, ID, print_id, match_id, hash_id, comp_id, TRUE, FALSE, FALSE, FALSE)

/* PSEUDOCLASS FOR STATE */
static void
print_pseudoclass_state (const GtkCssSelector *selector,
                         GString              *string)
{
  g_string_append_c (string, ':');
  g_string_append (string, gtk_css_pseudoclass_name (selector->state.state));
}

static gboolean
match_pseudoclass_state (const GtkCssSelector *selector,
                         GtkCssNode           *node)
{
  return (gtk_css_node_get_state (node) & selector->state.state) == selector->state.state;
}

static guint
hash_pseudoclass_state (const GtkCssSelector *selector)
{
  return selector->state.state;
}


static int
comp_pseudoclass_state (const GtkCssSelector *a,
		        const GtkCssSelector *b)
{
  return a->state.state - b->state.state;
}

static GtkCssChange
change_pseudoclass_state (const GtkCssSelector *selector)
{
  GtkStateFlags states = selector->state.state;
  GtkCssChange change = 0;

  if (states & GTK_STATE_FLAG_PRELIGHT)
    change |= GTK_CSS_CHANGE_HOVER;
  if (states & GTK_STATE_FLAG_INSENSITIVE)
    change |= GTK_CSS_CHANGE_DISABLED;
  if (states & GTK_STATE_FLAG_BACKDROP)
    change |= GTK_CSS_CHANGE_BACKDROP;
  if (states & GTK_STATE_FLAG_SELECTED)
    change |= GTK_CSS_CHANGE_SELECTED;
  if (states & ~(GTK_STATE_FLAG_PRELIGHT |
                 GTK_STATE_FLAG_INSENSITIVE |
                 GTK_STATE_FLAG_BACKDROP |
                 GTK_STATE_FLAG_SELECTED))
  change |= GTK_CSS_CHANGE_STATE;

  return change;
}

#define GTK_CSS_CHANGE_PSEUDOCLASS_STATE change_pseudoclass_state (selector)
DEFINE_SIMPLE_SELECTOR(pseudoclass_state, PSEUDOCLASS_STATE, print_pseudoclass_state,
                       match_pseudoclass_state, hash_pseudoclass_state, comp_pseudoclass_state,
                       FALSE, TRUE, FALSE, TRUE)
#undef GTK_CSS_CHANGE_PSEUDOCLASS_STATE

/* PSEUDOCLASS FOR POSITION */

static void
print_pseudoclass_position (const GtkCssSelector *selector,
                            GString              *string)
{
  switch (selector->position.type)
    {
    case POSITION_FORWARD:
      if (selector->position.a == 0)
        {
          if (selector->position.b == 1)
            g_string_append (string, ":first-child");
          else
            g_string_append_printf (string, ":nth-child(%d)", selector->position.b);
        }
      else if (selector->position.a == 2 && selector->position.b == 0)
        g_string_append (string, ":nth-child(even)");
      else if (selector->position.a == 2 && selector->position.b == 1)
        g_string_append (string, ":nth-child(odd)");
      else
        {
          g_string_append (string, ":nth-child(");
          if (selector->position.a == 1)
            g_string_append (string, "n");
          else if (selector->position.a == -1)
            g_string_append (string, "-n");
          else
            g_string_append_printf (string, "%dn", selector->position.a);
          if (selector->position.b > 0)
            g_string_append_printf (string, "+%d)", selector->position.b);
          else if (selector->position.b < 0)
            g_string_append_printf (string, "%d)", selector->position.b);
          else
            g_string_append (string, ")");
        }
      break;
    case POSITION_BACKWARD:
      if (selector->position.a == 0)
        {
          if (selector->position.b == 1)
            g_string_append (string, ":last-child");
          else
            g_string_append_printf (string, ":nth-last-child(%d)", selector->position.b);
        }
      else if (selector->position.a == 2 && selector->position.b == 0)
        g_string_append (string, ":nth-last-child(even)");
      else if (selector->position.a == 2 && selector->position.b == 1)
        g_string_append (string, ":nth-last-child(odd)");
      else
        {
          g_string_append (string, ":nth-last-child(");
          if (selector->position.a == 1)
            g_string_append (string, "n");
          else if (selector->position.a == -1)
            g_string_append (string, "-n");
          else
            g_string_append_printf (string, "%dn", selector->position.a);
          if (selector->position.b > 0)
            g_string_append_printf (string, "+%d)", selector->position.b);
          else if (selector->position.b < 0)
            g_string_append_printf (string, "%d)", selector->position.b);
          else
            g_string_append (string, ")");
        }
      break;
    case POSITION_ONLY:
      g_string_append (string, ":only-child");
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
match_position (GtkCssNode *node,
                GtkCssNode *(* prev_node_func) (GtkCssNode *),
                int         a,
                int         b)
{
  int pos, x;

  /* special-case the common "first-child" and "last-child" */
  if (a == 0)
    {
      while (b > 0 && node != NULL)
        {
          b--;
          node = prev_node_func (node);
        }

      return b == 0 && node == NULL;
    }

  /* count nodes */
  for (pos = 0; node != NULL; pos++)
    node = prev_node_func (node);

  /* solve pos = a * X + b
   * and return TRUE if X is integer >= 0 */
  x = pos - b;

  if (x % a)
    return FALSE;

  return x / a >= 0;
}

static gboolean
match_pseudoclass_position (const GtkCssSelector *selector,
		            GtkCssNode           *node)
{
  switch (selector->position.type)
    {
    case POSITION_FORWARD:
      if (!match_position (node, get_previous_visible_sibling, selector->position.a, selector->position.b))
        return FALSE;
      break;
    case POSITION_BACKWARD:
      if (!match_position (node, get_next_visible_sibling, selector->position.a, selector->position.b))
        return FALSE;
      break;
    case POSITION_ONLY:
      if (get_previous_visible_sibling (node) ||
          get_next_visible_sibling (node))
        return FALSE;
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
    }

  return TRUE;
}

static guint
hash_pseudoclass_position (const GtkCssSelector *a)
{
  return (guint)(((((gulong)a->position.type) << POSITION_NUMBER_BITS) | a->position.a) << POSITION_NUMBER_BITS) | a->position.b;
}

static int
comp_pseudoclass_position (const GtkCssSelector *a,
			   const GtkCssSelector *b)
{
  int diff;
  
  diff = a->position.type - b->position.type;
  if (diff)
    return diff;

  diff = a->position.a - b->position.a;
  if (diff)
    return diff;

  return a->position.b - b->position.b;
}

static GtkCssChange
change_pseudoclass_position (const GtkCssSelector *selector)
{
  switch (selector->position.type)
    {
    case POSITION_FORWARD:
      if (selector->position.a == 0 && selector->position.b == 1)
        return GTK_CSS_CHANGE_FIRST_CHILD;
      else
        return GTK_CSS_CHANGE_NTH_CHILD;
    case POSITION_BACKWARD:
      if (selector->position.a == 0 && selector->position.b == 1)
        return GTK_CSS_CHANGE_LAST_CHILD;
      else
        return GTK_CSS_CHANGE_NTH_LAST_CHILD;
    case POSITION_ONLY:
      return GTK_CSS_CHANGE_FIRST_CHILD | GTK_CSS_CHANGE_LAST_CHILD;
    default:
      g_assert_not_reached ();
      return 0;
    }
}

#define GTK_CSS_CHANGE_PSEUDOCLASS_POSITION change_pseudoclass_position(selector)
DEFINE_SIMPLE_SELECTOR(pseudoclass_position, PSEUDOCLASS_POSITION, print_pseudoclass_position,
                       match_pseudoclass_position, hash_pseudoclass_position, comp_pseudoclass_position,
                       FALSE, TRUE, FALSE, TRUE)
#undef GTK_CSS_CHANGE_PSEUDOCLASS_POSITION
/* API */

static guint
gtk_css_selector_size (const GtkCssSelector *selector)
{
  guint size = 0;

  while (selector)
    {
      selector = gtk_css_selector_previous (selector);
      size++;
    }

  return size;
}

static GtkCssSelector *
gtk_css_selector_new (const GtkCssSelectorClass *class,
                      GtkCssSelector            *selector)
{
  guint size;

  size = gtk_css_selector_size (selector);
  selector = g_realloc (selector, sizeof (GtkCssSelector) * (size + 1) + sizeof (gpointer));
  if (size == 0)
    selector[1].class = NULL;
  else
    memmove (selector + 1, selector, sizeof (GtkCssSelector) * size + sizeof (gpointer));

  memset (selector, 0, sizeof (GtkCssSelector));
  selector->class = class;

  return selector;
}

static GtkCssSelector *
gtk_css_selector_parse_selector_class (GtkCssParser   *parser,
                                       GtkCssSelector *selector,
                                       gboolean        negate)
{
  const GtkCssToken *token;

  gtk_css_parser_consume_token (parser);
  for (token = gtk_css_parser_peek_token (parser);
       gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT);
       token = gtk_css_parser_peek_token (parser))
    {
      gtk_css_parser_consume_token (parser);
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_CLASS
                                              : &GTK_CSS_SELECTOR_CLASS,
                                       selector);
      selector->style_class.style_class = g_quark_from_string (gtk_css_token_get_string (token));
      gtk_css_parser_consume_token (parser);
      return selector;
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "No class name after '.' in selector");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }
}

static gboolean
string_has_number (const char *string,
                   const char *prefix,
                   int        *number)
{
  gsize len = strlen (prefix);
  char *end;

  if (g_ascii_strncasecmp (string, prefix, len) != 0)
    return FALSE;

  errno = 0;
  *number = strtoul (string + len, &end, 10);
  if (*end != '\0' || errno != 0)
    return FALSE;

  return TRUE;
}

static gboolean
parse_plus_b (GtkCssParser *parser,
              gboolean      negate,
              int          *b)
{
  const GtkCssToken *token;
  gboolean has_seen_sign;

  token = gtk_css_parser_get_token (parser);

  if (negate)
    {
      has_seen_sign = TRUE;
    }
  else
    {
      if (gtk_css_token_is_delim (token, '+'))
        {
          gtk_css_parser_consume_token (parser);
          has_seen_sign = TRUE;
        }
      else if (gtk_css_token_is_delim (token, '-'))
        {
          gtk_css_parser_consume_token (parser);
          negate = TRUE;
          has_seen_sign = TRUE;
        }
      else
        {
          has_seen_sign = FALSE;
        }
    }

  token = gtk_css_parser_get_token (parser);
  if (!has_seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER))
    {
      *b = token->number.number;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (has_seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      *b = token->number.number;
      if (negate)
        *b = - *b;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (!has_seen_sign)
    {
      *b = 0;
      return TRUE;
    }
  
  gtk_css_parser_error_syntax (parser, "Not a valid an+b type");
  return FALSE;
}

static gboolean
parse_n_plus_b (GtkCssParser *parser,
                int           before,
                int          *a,
                int          *b)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is_ident (token, "n"))
    {
      *a = before;
      gtk_css_parser_consume_token (parser);
      return parse_plus_b (parser, FALSE, b);
    }
  else if (gtk_css_token_is_ident (token, "n-"))
    {
      *a = before;
      gtk_css_parser_consume_token (parser);
      return parse_plus_b (parser, TRUE, b);
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT) &&
           string_has_number (gtk_css_token_get_string (token), "n-", b))
    {
      *a = before;
      *b = -*b;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else
    {
      *b = before;
      *a = 0;
      return TRUE;
    }
}

static gboolean
parse_a_n_plus_b (GtkCssParser *parser,
                  int           seen_sign,
                  int          *a,
                  int          *b)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);

  if (!seen_sign && gtk_css_token_is_ident (token, "even"))
    {
      *a = 2;
      *b = 0;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (!seen_sign && gtk_css_token_is_ident (token, "odd"))
    {
      *a = 2;
      *b = 1;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (!seen_sign && gtk_css_token_is_delim (token, '+'))
    {
      gtk_css_parser_consume_token (parser);
      return parse_a_n_plus_b (parser, 1, a, b);
    }
  else if (!seen_sign && gtk_css_token_is_delim (token, '-'))
    {
      gtk_css_parser_consume_token (parser);
      return parse_a_n_plus_b (parser, -1, a, b);
    }
  else if ((!seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER)) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      int x = token->number.number * (seen_sign ? seen_sign : 1);
      gtk_css_parser_consume_token (parser);

      return parse_n_plus_b (parser, x , a, b);
    }
  else if (((!seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION)) ||
            gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)) &&
           g_ascii_strcasecmp (token->dimension.dimension, "n") == 0)
    {
      *a = token->dimension.value * (seen_sign ? seen_sign : 1);
      gtk_css_parser_consume_token (parser);
      return parse_plus_b (parser, FALSE, b);
    }
  else if (((!seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION)) ||
            gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)) &&
           g_ascii_strcasecmp (token->dimension.dimension, "n-") == 0)
    {
      *a = token->dimension.value * (seen_sign ? seen_sign : 1);
      gtk_css_parser_consume_token (parser);
      return parse_plus_b (parser, TRUE, b);
    }
  else if (((!seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION)) ||
            gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION)) &&
           string_has_number (token->dimension.dimension, "n-", b))
    {
      *a = token->dimension.value * (seen_sign ? seen_sign : 1);
      *b = -*b;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (!seen_sign && gtk_css_token_is_ident (token, "-n"))
    {
      *a = -1;
      gtk_css_parser_consume_token (parser);
      return parse_plus_b (parser, FALSE, b);
    }
  else if (!seen_sign && gtk_css_token_is_ident (token, "-n-"))
    {
      *a = -1;
      gtk_css_parser_consume_token (parser);
      return parse_plus_b (parser, TRUE, b);
    }
  else if (!seen_sign &&
           gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT) &&
           string_has_number (gtk_css_token_get_string (token), "-n-", b))
    {
      *a = -1;
      *b = -*b;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (gtk_css_token_is_ident (token, "n") ||
           gtk_css_token_is_ident (token, "n-"))
    {
      return parse_n_plus_b (parser, seen_sign ? seen_sign : 1, a, b);
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT) &&
           string_has_number (gtk_css_token_get_string (token), "n-", b))
    {
      *a = seen_sign ? seen_sign : 1;
      *b = -*b;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  else if (!seen_sign && gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT) &&
           string_has_number (gtk_css_token_get_string (token), "-n-", b))
    {
      *a = -1;
      *b = -*b;
      gtk_css_parser_consume_token (parser);
      return TRUE;
    }
  
  gtk_css_parser_error_syntax (parser, "Not a valid an+b type");
  return FALSE;
}

static guint
parse_a_n_plus_b_arg (GtkCssParser *parser,
                      guint         arg,
                      gpointer      data)
{
  int *ab = data;

  if (!parse_a_n_plus_b (parser, FALSE, &ab[0], &ab[1]))
    return 0;

  return 1;
}

static guint
parse_dir_arg (GtkCssParser *parser,
               guint         arg,
               gpointer      data)
{
  GtkStateFlags *flag = data;

  if (gtk_css_parser_try_ident (parser, "ltr"))
    {
      *flag = GTK_STATE_FLAG_DIR_LTR;
      return 1;
    }
  else if (gtk_css_parser_try_ident (parser, "rtl"))
    {
      *flag = GTK_STATE_FLAG_DIR_RTL;
      return 1;
    }
  else
    {
      gtk_css_parser_error_value (parser, "Expected \"ltr\" or \"rtl\"");
      return 0;
    }
}

static guint
parse_identifier_arg (GtkCssParser *parser,
                      guint         arg,
                      gpointer      data)
{
  const char *ident = data;
  
  if (!gtk_css_parser_try_ident (parser, ident))
    {
      gtk_css_parser_error_value (parser, "Expected \"%s\"", ident);
      return 0;
    }

  return 1;
}

static GtkCssSelector *
gtk_css_selector_parse_selector_pseudo_class (GtkCssParser   *parser,
                                              GtkCssSelector *selector,
                                              gboolean        negate)
{
  GtkCssLocation start_location;
  const GtkCssToken *token;

  start_location = *gtk_css_parser_get_start_location (parser);
  gtk_css_parser_consume_token (parser);
  for (token = gtk_css_parser_peek_token (parser);
       gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT);
       token = gtk_css_parser_peek_token (parser))
    {
      gtk_css_parser_consume_token (parser);
    }

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      static const struct {
        const char    *name;
        GtkStateFlags  state_flag;
        PositionType   position_type;
        int            position_a;
        int            position_b;
      } pseudo_classes[] = {
        { "first-child",    0,                           POSITION_FORWARD,  0, 1 },
        { "last-child",     0,                           POSITION_BACKWARD, 0, 1 },
        { "only-child",     0,                           POSITION_ONLY,     0, 0 },
        { "active",         GTK_STATE_FLAG_ACTIVE, },
        { "hover",          GTK_STATE_FLAG_PRELIGHT, },
        { "selected",       GTK_STATE_FLAG_SELECTED, },
        { "disabled",       GTK_STATE_FLAG_INSENSITIVE, },
        { "indeterminate",  GTK_STATE_FLAG_INCONSISTENT, },
        { "focus",          GTK_STATE_FLAG_FOCUSED, },
        { "backdrop",       GTK_STATE_FLAG_BACKDROP, },
        { "link",           GTK_STATE_FLAG_LINK, },
        { "visited",        GTK_STATE_FLAG_VISITED, },
        { "checked",        GTK_STATE_FLAG_CHECKED, },
        { "focus-visible",  GTK_STATE_FLAG_FOCUS_VISIBLE, },
        { "focus-within",   GTK_STATE_FLAG_FOCUS_WITHIN, },
      };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (pseudo_classes); i++)
        {
          if (g_ascii_strcasecmp (pseudo_classes[i].name, gtk_css_token_get_string (token)) == 0)
            {
              if (pseudo_classes[i].state_flag)
                {
                  selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_STATE
                                                            : &GTK_CSS_SELECTOR_PSEUDOCLASS_STATE,
                                                   selector);
                  selector->state.state = pseudo_classes[i].state_flag;
                }
              else
                {
                  selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_POSITION
                                                          : &GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                                   selector);
                  selector->position.type = pseudo_classes[i].position_type;
                  selector->position.a = pseudo_classes[i].position_a;
                  selector->position.b = pseudo_classes[i].position_b;
                }
              gtk_css_parser_consume_token (parser);
              return selector;
            }
        }
          
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                            &start_location,
                            gtk_css_parser_get_end_location (parser),
                            "Unknown name of pseudo-class");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION))
    {
      if (gtk_css_token_is_function (token, "nth-child"))
        {
          int ab[2];

          if (!gtk_css_parser_consume_function (parser, 1, 1, parse_a_n_plus_b_arg, ab))
            {
              if (selector)
                _gtk_css_selector_free (selector);
              return NULL;
            }

          selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_POSITION
                                                  : &GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                           selector);
          selector->position.type = POSITION_FORWARD;
          selector->position.a = ab[0];
          selector->position.b = ab[1];
        }
      else if (gtk_css_token_is_function (token, "nth-last-child"))
        {
          int ab[2];

          if (!gtk_css_parser_consume_function (parser, 1, 1, parse_a_n_plus_b_arg, ab))
            {
              if (selector)
                _gtk_css_selector_free (selector);
              return NULL;
            }

          selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_POSITION
                                                  : &GTK_CSS_SELECTOR_PSEUDOCLASS_POSITION,
                                           selector);
          selector->position.type = POSITION_BACKWARD;
          selector->position.a = ab[0];
          selector->position.b = ab[1];
        }
      else if (gtk_css_token_is_function (token, "not"))
        {
          if (negate)
            {
              gtk_css_parser_error_syntax (parser, "Nesting of :not() not allowed");
              if (selector)
                _gtk_css_selector_free (selector);
              return NULL;
            }
          else
            {
              gtk_css_parser_start_block (parser);
              token = gtk_css_parser_get_token (parser);

              if (gtk_css_token_is_delim (token, '*'))
                {
                  selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NOT_ANY, selector);
                  gtk_css_parser_consume_token (parser);
                }
              else if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
                {
                  selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NOT_NAME, selector);
                  selector->name.name = g_quark_from_string (gtk_css_token_get_string (token));
                  gtk_css_parser_consume_token (parser);
                }
              else if (gtk_css_token_is (token, GTK_CSS_TOKEN_HASH_ID))
                {
                  selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NOT_ID, selector);
                  selector->id.name = g_quark_from_string (gtk_css_token_get_string (token));
                  gtk_css_parser_consume_token (parser);
                }
              else if (gtk_css_token_is_delim (token, '.'))
                {
                  selector = gtk_css_selector_parse_selector_class (parser, selector, TRUE);
                }
              else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
                {
                  selector = gtk_css_selector_parse_selector_pseudo_class (parser, selector, TRUE);
                }
              else
                {
                  gtk_css_parser_error_syntax (parser, "Invalid contents of :not() selector");
                  gtk_css_parser_end_block (parser);
                  if (selector)
                    _gtk_css_selector_free (selector);
                  selector = NULL;
                  return NULL;
                }

              token = gtk_css_parser_get_token (parser);
              if (!gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
                {
                  gtk_css_parser_error_syntax (parser, "Invalid contents of :not() selector");
                  gtk_css_parser_end_block (parser);
                  if (selector)
                    _gtk_css_selector_free (selector);
                  selector = NULL;
                  return NULL;
                }
              gtk_css_parser_end_block (parser);
            }
        }
      else if (gtk_css_token_is_function (token, "dir"))
        {
          GtkStateFlags flag;

          if (!gtk_css_parser_consume_function (parser, 1, 1, parse_dir_arg, &flag))
            {
              if (selector)
                _gtk_css_selector_free (selector);
              return NULL;
            }

          selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_STATE
                                                  : &GTK_CSS_SELECTOR_PSEUDOCLASS_STATE,
                                           selector);
          selector->state.state = flag;
        }
      else if (gtk_css_token_is_function (token, "drop"))
        {
          if (!gtk_css_parser_consume_function (parser, 1, 1, parse_identifier_arg, (gpointer) "active"))
            {
              if (selector)
                _gtk_css_selector_free (selector);
              return NULL;
            }
          selector = gtk_css_selector_new (negate ? &GTK_CSS_SELECTOR_NOT_PSEUDOCLASS_STATE
                                                  : &GTK_CSS_SELECTOR_PSEUDOCLASS_STATE,
                                           selector);
          selector->state.state = GTK_STATE_FLAG_DROP_ACTIVE;
        }
      else
        {
          gtk_css_parser_error (parser,
                                GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                                &start_location,
                                gtk_css_parser_get_end_location (parser),
                                "Unknown pseudoclass");
          if (selector)
            _gtk_css_selector_free (selector);
          return NULL;
        }
    }
  else
    {
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                            &start_location,
                            gtk_css_parser_get_end_location (parser),
                            "Unknown pseudoclass");
      if (selector)
        _gtk_css_selector_free (selector);
      return NULL;
    }

  return selector;
}

static GtkCssSelector *
gtk_css_selector_parse_simple_selector (GtkCssParser   *parser,
                                        GtkCssSelector *selector)
{
  gboolean parsed_something = FALSE;
  const GtkCssToken *token;

  do {
      for (token = gtk_css_parser_peek_token (parser);
           gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT);
           token = gtk_css_parser_peek_token (parser))
        {
          gtk_css_parser_consume_token (parser);
        }

      if (!parsed_something && gtk_css_token_is_delim (token, '*'))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ANY, selector);
          gtk_css_parser_consume_token (parser);
        }
      else if (!parsed_something && gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_NAME, selector);
          selector->name.name = g_quark_from_string (gtk_css_token_get_string (token));
          gtk_css_parser_consume_token (parser);
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_HASH_ID))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ID, selector);
          selector->id.name = g_quark_from_string (gtk_css_token_get_string (token));
          gtk_css_parser_consume_token (parser);
        }
      else if (gtk_css_token_is_delim (token, '.'))
        {
          selector = gtk_css_selector_parse_selector_class (parser, selector, FALSE);
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
        {
          selector = gtk_css_selector_parse_selector_pseudo_class (parser, selector, FALSE);
        }
      else
        {
          if (!parsed_something)
            {
              gtk_css_parser_error_syntax (parser, "Expected a valid selector");
              if (selector)
                _gtk_css_selector_free (selector);
              selector = NULL;
            }
          break;
        }

      parsed_something = TRUE;
    }
  while (TRUE);

  return selector;
}

GtkCssSelector *
_gtk_css_selector_parse (GtkCssParser *parser)
{
  GtkCssSelector *selector = NULL;
  const GtkCssToken *token;

  while (TRUE)
    {
      gboolean seen_whitespace = FALSE;

      /* skip all whitespace and comments */
      gtk_css_parser_get_token (parser);

      selector = gtk_css_selector_parse_simple_selector (parser, selector);
      if (selector == NULL)
        return NULL;

      for (token = gtk_css_parser_peek_token (parser);
           gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT) || 
           gtk_css_token_is (token, GTK_CSS_TOKEN_WHITESPACE);
           token = gtk_css_parser_peek_token (parser))
        {
          seen_whitespace |= gtk_css_token_is (token, GTK_CSS_TOKEN_WHITESPACE);
          gtk_css_parser_consume_token (parser);
        }

      if (gtk_css_token_is_delim (token, '+'))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_ADJACENT, selector);
          gtk_css_parser_consume_token (parser);
        }
      else if (gtk_css_token_is_delim (token, '~'))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_SIBLING, selector);
          gtk_css_parser_consume_token (parser);
        }
      else if (gtk_css_token_is_delim (token, '>'))
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_CHILD, selector);
          gtk_css_parser_consume_token (parser);
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF) ||
               gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA) ||
               gtk_css_token_is (token, GTK_CSS_TOKEN_OPEN_CURLY))
        {
          break;
        }
      else if (seen_whitespace)
        {
          selector = gtk_css_selector_new (&GTK_CSS_SELECTOR_DESCENDANT, selector);
        }
      else
        {
           gtk_css_parser_error_syntax (parser, "Expected a valid selector");
           _gtk_css_selector_free (selector);
          return NULL;
        }
    }

  return selector;
}

void
_gtk_css_selector_free (GtkCssSelector *selector)
{
  g_return_if_fail (selector != NULL);

  g_free (selector);
}

void
_gtk_css_selector_print (const GtkCssSelector *selector,
                         GString *             str)
{
  const GtkCssSelector *previous;

  g_return_if_fail (selector != NULL);

  previous = gtk_css_selector_previous (selector);
  if (previous)
    _gtk_css_selector_print (previous, str);

  selector->class->print (selector, str);
}

char *
_gtk_css_selector_to_string (const GtkCssSelector *selector)
{
  GString *string;

  g_return_val_if_fail (selector != NULL, NULL);

  string = g_string_new (NULL);

  _gtk_css_selector_print (selector, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_css_selector_matches:
 * @selector: the selector
 * @node: The node to match
 *
 * Checks if the @selector matches the given @node.
 *
 * Returns: %TRUE if the selector matches @node
 **/
gboolean
gtk_css_selector_matches (const GtkCssSelector *selector,
                          GtkCssNode           *node)
{
  GtkCssNode *child;
  const GtkCssSelector *prev;

  g_return_val_if_fail (selector != NULL, FALSE);
  g_return_val_if_fail (node != NULL, FALSE);

  if (!gtk_css_selector_match_one (selector, node))
    return FALSE;

  prev = gtk_css_selector_previous (selector);
  if (prev == NULL)
    return TRUE;

  for (child = gtk_css_selector_iterator (selector, node, NULL);
       child;
       child = gtk_css_selector_iterator (selector, node, child))
   {
     if (gtk_css_selector_matches (prev, child))
       return TRUE;
   }

  return FALSE;
}

/* Computes specificity according to CSS 2.1.
 * The arguments must be initialized to 0 */
static void
_gtk_css_selector_get_specificity (const GtkCssSelector *selector,
                                   guint                *ids,
                                   guint                *classes,
                                   guint                *elements)
{
  for (; selector; selector = gtk_css_selector_previous (selector))
    {
      selector->class->add_specificity (selector, ids, classes, elements);
    }
}

int
_gtk_css_selector_compare (const GtkCssSelector *a,
                           const GtkCssSelector *b)
{
  guint a_ids = 0, a_classes = 0, a_elements = 0;
  guint b_ids = 0, b_classes = 0, b_elements = 0;
  int compare;

  _gtk_css_selector_get_specificity (a, &a_ids, &a_classes, &a_elements);
  _gtk_css_selector_get_specificity (b, &b_ids, &b_classes, &b_elements);

  compare = a_ids - b_ids;
  if (compare)
    return compare;

  compare = a_classes - b_classes;
  if (compare)
    return compare;

  return a_elements - b_elements;
}

GtkCssChange
_gtk_css_selector_get_change (const GtkCssSelector *selector)
{
  if (selector == NULL)
    return 0;

  return selector->class->get_change (selector, _gtk_css_selector_get_change (gtk_css_selector_previous (selector)));
}

/******************** SelectorTree handling *****************/

static gboolean
gtk_css_selector_is_simple (const GtkCssSelector *selector)
{
  switch (selector->class->category)
  {
    case GTK_CSS_SELECTOR_CATEGORY_SIMPLE:
    case GTK_CSS_SELECTOR_CATEGORY_SIMPLE_RADICAL:
      return TRUE;
    case GTK_CSS_SELECTOR_CATEGORY_PARENT:
    case GTK_CSS_SELECTOR_CATEGORY_SIBLING:
      return FALSE;
    default:
      g_assert_not_reached ();
      return FALSE;
  }
}

static GHashTable *
gtk_css_selectors_count_initial_init (void)
{
  return g_hash_table_new ((GHashFunc)gtk_css_selector_hash_one, (GEqualFunc)gtk_css_selector_equal);
}

static void
gtk_css_selectors_count_initial (const GtkCssSelector *selector, GHashTable *hash_one)
{
  if (!gtk_css_selector_is_simple (selector))
    {
      guint count = GPOINTER_TO_INT (g_hash_table_lookup (hash_one, selector));
      g_hash_table_replace (hash_one, (gpointer)selector, GUINT_TO_POINTER (count + 1));
      return;
    }

  for (;
       selector && gtk_css_selector_is_simple (selector);
       selector = gtk_css_selector_previous (selector))
    {
      guint count = GPOINTER_TO_INT (g_hash_table_lookup (hash_one, selector));
      g_hash_table_replace (hash_one, (gpointer)selector, GUINT_TO_POINTER (count + 1));
    }
}

static gboolean
gtk_css_selectors_has_initial_selector (const GtkCssSelector *selector, const GtkCssSelector *initial)
{
  if (!gtk_css_selector_is_simple (selector))
    return gtk_css_selector_equal (selector, initial);

  for (;
       selector && gtk_css_selector_is_simple (selector);
       selector = gtk_css_selector_previous (selector))
    {
      if (gtk_css_selector_equal (selector, initial))
	return TRUE;
    }

  return FALSE;
}

static GtkCssSelector *
gtk_css_selectors_skip_initial_selector (GtkCssSelector *selector, const GtkCssSelector *initial)
{
  GtkCssSelector *found;
  GtkCssSelector tmp;

  /* If the initial simple selector is not first, move it there so we can skip it
     without losing any other selectors */
  if (!gtk_css_selector_equal (selector, initial))
    {
      for (found = selector; found && gtk_css_selector_is_simple (found); found = (GtkCssSelector *)gtk_css_selector_previous (found))
	{
	  if (gtk_css_selector_equal (found, initial))
	    break;
	}

      g_assert (found != NULL && gtk_css_selector_is_simple (found));

      tmp = *found;
      *found = *selector;
      *selector = tmp;
    }

  return (GtkCssSelector *)gtk_css_selector_previous (selector);
}

/* When checking for changes via the tree we need to know if a rule further
   down the tree matched, because if so we need to add "our bit" to the
   Change. For instance in a match like *.class:active we'll
   get a tree that first checks :active, if that matches we continue down
   to the tree, and if we get a match we add CHANGE_CLASS. However, the
   end of the tree where we have a match is an ANY which doesn't actually
   modify the change, so we don't know if we have a match or not. We fix
   this by setting GTK_CSS_CHANGE_GOT_MATCH which lets us guarantee
   that change != 0 on any match. */
#define GTK_CSS_CHANGE_GOT_MATCH GTK_CSS_CHANGE_RESERVED_BIT

/* The code for collecting matches assumes that the name, id and classes
 * of a node remain unchanged, and anything else can change. This needs to
 * be kept in sync with the definition of 'radical change' in gtkcssnode.c.
 */

static GtkCssChange
gtk_css_selector_tree_get_change (const GtkCssSelectorTree     *tree,
                                  const GtkCountingBloomFilter *filter,
				  GtkCssNode                   *node,
                                  gboolean                      skipping)
{
  GtkCssChange change = 0;
  const GtkCssSelectorTree *prev;

  switch (tree->selector.class->category)
    {
      case GTK_CSS_SELECTOR_CATEGORY_SIMPLE:
        break;
      case GTK_CSS_SELECTOR_CATEGORY_SIMPLE_RADICAL:
        if (skipping)
          break;
        if (node)
          {
            if (!tree->selector.class->match_one (&tree->selector, node))
              return 0;
          }
        else if (filter)
          {
            if (!gtk_counting_bloom_filter_may_contain (filter,
                                                        gtk_css_selector_hash_one (&tree->selector)))
              return 0;
          }
        break;
      case GTK_CSS_SELECTOR_CATEGORY_PARENT:
        skipping = FALSE;
        node = NULL;
        break;
      case GTK_CSS_SELECTOR_CATEGORY_SIBLING:
        skipping = TRUE;
        node = NULL;
        break;
      default:
        g_assert_not_reached ();
        return 0;
    }

  for (prev = gtk_css_selector_tree_get_previous (tree);
       prev != NULL;
       prev = gtk_css_selector_tree_get_sibling (prev))
    change |= gtk_css_selector_tree_get_change (prev, filter, node, skipping);

  if (change || gtk_css_selector_tree_get_matches (tree))
    change = tree->selector.class->get_change (&tree->selector, change & ~GTK_CSS_CHANGE_GOT_MATCH) | GTK_CSS_CHANGE_GOT_MATCH;

  return change;
}

static void
gtk_css_selector_tree_found_match (const GtkCssSelectorTree  *tree,
                                   GtkCssSelectorMatches     *results)
{
  int i;
  gpointer *matches;

  matches = gtk_css_selector_tree_get_matches (tree);
  if (!matches)
    return;

  for (i = 0; matches[i] != NULL; i++)
    gtk_css_selector_matches_insert_sorted (results, matches[i]);
}

static gboolean
gtk_css_selector_tree_match (const GtkCssSelectorTree      *tree,
                             const GtkCountingBloomFilter  *filter,
                             gboolean                       match_filter,
                             GtkCssNode                    *node,
                             GtkCssSelectorMatches         *results)
{
  const GtkCssSelectorTree *prev;
  GtkCssNode *child;

  if (match_filter && tree->selector.class->category == GTK_CSS_SELECTOR_CATEGORY_SIMPLE_RADICAL &&
      !gtk_counting_bloom_filter_may_contain (filter, gtk_css_selector_hash_one (&tree->selector)))
    return FALSE;

  if (!gtk_css_selector_match_one (&tree->selector, node))
    return TRUE;

  gtk_css_selector_tree_found_match (tree, results);

  if (filter && !gtk_css_selector_is_simple (&tree->selector))
    match_filter = tree->selector.class->category == GTK_CSS_SELECTOR_CATEGORY_PARENT;

  for (prev = gtk_css_selector_tree_get_previous (tree);
       prev != NULL;
       prev = gtk_css_selector_tree_get_sibling (prev))
    {
      for (child = gtk_css_selector_iterator (&tree->selector, node, NULL);
           child;
           child = gtk_css_selector_iterator (&tree->selector, node, child))
        {
          if (!gtk_css_selector_tree_match (prev, filter, match_filter, child, results))
            break;
        }
    }

  return TRUE;
}

void
_gtk_css_selector_tree_match_all (const GtkCssSelectorTree     *tree,
                                  const GtkCountingBloomFilter *filter,
                                  GtkCssNode                   *node,
                                  GtkCssSelectorMatches        *out_tree_rules)
{
  const GtkCssSelectorTree *iter;

  for (iter = tree;
       iter != NULL;
       iter = gtk_css_selector_tree_get_sibling (iter))
    {
      gtk_css_selector_tree_match (iter, filter, FALSE, node, out_tree_rules);
    }
}

gboolean
_gtk_css_selector_tree_is_empty (const GtkCssSelectorTree *tree)
{
  return tree == NULL;
}

GtkCssChange
gtk_css_selector_tree_get_change_all (const GtkCssSelectorTree     *tree,
                                      const GtkCountingBloomFilter *filter,
				      GtkCssNode                   *node)
{
  GtkCssChange change = 0;

  for (; tree != NULL;
       tree = gtk_css_selector_tree_get_sibling (tree))
    change |= gtk_css_selector_tree_get_change (tree, filter, node, FALSE);

  /* Never return reserved bit set */
  return change & ~GTK_CSS_CHANGE_RESERVED_BIT;
}

#ifdef PRINT_TREE
static void
_gtk_css_selector_tree_print (const GtkCssSelectorTree *tree, GString *str, const char *prefix)
{
  gboolean first = TRUE;
  int len, i;
  gpointer *matches;

  for (; tree != NULL; tree = gtk_css_selector_tree_get_sibling (tree), first = FALSE)
    {
      if (!first)
	g_string_append (str, prefix);

      if (first)
	{
	  if (gtk_css_selector_tree_get_sibling (tree))
	    g_string_append (str, "─┬─");
	  else
	    g_string_append (str, "───");
	}
      else
	{
	  if (gtk_css_selector_tree_get_sibling (tree))
	    g_string_append (str, " ├─");
	  else
	    g_string_append (str, " └─");
	}

      len = str->len;
      tree->selector.class->print (&tree->selector, str);
      matches = gtk_css_selector_tree_get_matches (tree);
      if (matches)
        {
          int n;
          for (n = 0; matches[n] != NULL; n++) ;
          if (n == 1)
            g_string_append (str, " (1 match)");
          else
            g_string_append_printf (str, " (%d matches)", n);
        }
      len = str->len - len;

      if (gtk_css_selector_tree_get_previous (tree))
	{
	  GString *prefix2 = g_string_new (prefix);

	  if (gtk_css_selector_tree_get_sibling (tree))
	    g_string_append (prefix2, " │ ");
	  else
	    g_string_append (prefix2, "   ");
	  for (i = 0; i < len; i++)
	    g_string_append_c (prefix2, ' ');

	  _gtk_css_selector_tree_print (gtk_css_selector_tree_get_previous (tree), str, prefix2->str);
	  g_string_free (prefix2, TRUE);
	}
      else
	g_string_append (str, "\n");
    }
}
#endif

void
_gtk_css_selector_tree_match_print (const GtkCssSelectorTree *tree,
				    GString *str)
{
  const GtkCssSelectorTree *iter;

  g_return_if_fail (tree != NULL);

  /* print name and * selector before others */
  for (iter = tree; 
       iter && gtk_css_selector_is_simple (&iter->selector);
       iter = gtk_css_selector_tree_get_parent (iter))
    {
      if (iter->selector.class == &GTK_CSS_SELECTOR_NAME ||
          iter->selector.class == &GTK_CSS_SELECTOR_ANY)
        {
          iter->selector.class->print (&iter->selector, str);
        }
    }
  /* now print other simple selectors */
  for (iter = tree; 
       iter && gtk_css_selector_is_simple (&iter->selector);
       iter = gtk_css_selector_tree_get_parent (iter))
    {
      if (iter->selector.class != &GTK_CSS_SELECTOR_NAME &&
          iter->selector.class != &GTK_CSS_SELECTOR_ANY)
        {
          iter->selector.class->print (&iter->selector, str);
        }
    }

  /* now if there's a combinator, print that one */
  if (iter != NULL)
    {
      iter->selector.class->print (&iter->selector, str);
      tree = gtk_css_selector_tree_get_parent (iter);
      if (tree)
        _gtk_css_selector_tree_match_print (tree, str);
    }
}

void
_gtk_css_selector_tree_free (GtkCssSelectorTree *tree)
{
  if (tree == NULL)
    return;

  g_free (tree);
}


typedef struct {
  gpointer match;
  GtkCssSelector *current_selector;
  GtkCssSelectorTree **selector_match;
} GtkCssSelectorRuleSetInfo;

static GtkCssSelectorTree *
get_tree (GByteArray *array, gint32 offset)
{
  return (GtkCssSelectorTree *) (array->data + offset);
}

static GtkCssSelectorTree *
alloc_tree (GByteArray *array, gint32 *offset)
{
  GtkCssSelectorTree tree = { { NULL} };

  *offset = array->len;
  g_byte_array_append (array, (guint8 *)&tree, sizeof (GtkCssSelectorTree));
  return get_tree (array, *offset);
}

static gint32
subdivide_infos (GByteArray                 *array,
                 GtkCssSelectorRuleSetInfo **infos,
                 guint                       n_infos,
                 gint32                      parent_offset)
{
  guint n_matched = 0;
  guint n_remaining = 0;
  guint n_exact = 0;
  GHashTable *ht;
  gint32 tree_offset;
  GtkCssSelectorTree *tree;
  GtkCssSelector max_selector;
  GHashTableIter iter;
  guint max_count;
  gpointer key, value;
  GtkCssSelectorMatches exact_matches;
  gint32 res;
  guint i;

  if (n_infos == 0)
    return GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET;

  ht = gtk_css_selectors_count_initial_init ();

  for (i = 0; i < n_infos; i++)
    {
      const GtkCssSelectorRuleSetInfo *info = infos[i];
      gtk_css_selectors_count_initial (info->current_selector, ht);
    }

  /* Pick the selector with highest count, and use as decision on this level
     as that makes it possible to skip the largest amount of checks later */

  max_count = 0;

  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkCssSelector *selector = key;
      if (GPOINTER_TO_UINT (value) > max_count ||
          (GPOINTER_TO_UINT (value) == max_count &&
           gtk_css_selector_compare_one (selector, &max_selector) < 0))
        {
          max_count = GPOINTER_TO_UINT (value);
          max_selector = *selector;
        }
    }

  tree = alloc_tree (array, &tree_offset);
  tree->parent_offset = parent_offset;
  tree->selector = max_selector;

  gtk_css_selector_matches_init (&exact_matches);

  i = 0;
  while (i < n_infos - n_exact - n_matched)
    {
      GtkCssSelectorRuleSetInfo *info = infos[i];

      if (gtk_css_selectors_has_initial_selector (info->current_selector, &max_selector))
        {
          info->current_selector = gtk_css_selectors_skip_initial_selector (info->current_selector, &max_selector);
          if (info->current_selector == NULL)
            {
              /* Matches current node */
              gtk_css_selector_matches_append (&exact_matches, info->match);
              if (info->selector_match != NULL)
                *info->selector_match = GUINT_TO_POINTER (tree_offset);

              infos[i] = infos[n_infos - n_exact - n_matched - 1];
              infos[n_infos - n_exact - n_matched - 1] = infos[n_infos - n_exact - 1];
              infos[n_infos - n_exact - 1] = info;
              n_exact++;
            }
          else
            {
              infos[i] = infos[n_infos - n_exact - n_matched - 1];
              infos[n_infos - n_exact - n_matched - 1] = info;
              n_matched++;
            }
        }
      else
        {
          i++;
          n_remaining++;
        }
    }

  if (!gtk_css_selector_matches_is_empty (&exact_matches))
    {
      gtk_css_selector_matches_append (&exact_matches, NULL); /* Null terminate */
      res = array->len;
      g_byte_array_append (array, (guint8 *) gtk_css_selector_matches_get_data (&exact_matches),
                           gtk_css_selector_matches_get_size (&exact_matches) * sizeof (gpointer));
    }
  else
    res = GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET;

  gtk_css_selector_matches_clear (&exact_matches);
  get_tree (array, tree_offset)->matches_offset = res;

  res = subdivide_infos (array, infos + n_remaining, n_matched, tree_offset);
  get_tree (array, tree_offset)->previous_offset = res;

  res = subdivide_infos (array, infos, n_remaining, parent_offset);
  get_tree (array, tree_offset)->sibling_offset = res;

  g_hash_table_unref (ht);

  return tree_offset;
}

struct _GtkCssSelectorTreeBuilder {
  GArray *infos;
};

GtkCssSelectorTreeBuilder *
_gtk_css_selector_tree_builder_new (void)
{
  GtkCssSelectorTreeBuilder *builder = g_new0 (GtkCssSelectorTreeBuilder, 1);

  builder->infos = g_array_new (FALSE, TRUE, sizeof (GtkCssSelectorRuleSetInfo));

  return builder;
}

void
_gtk_css_selector_tree_builder_free  (GtkCssSelectorTreeBuilder *builder)
{
  g_array_free (builder->infos, TRUE);
  g_free (builder);
}

void
_gtk_css_selector_tree_builder_add (GtkCssSelectorTreeBuilder *builder,
				    GtkCssSelector            *selectors,
				    GtkCssSelectorTree       **selector_match,
				    gpointer                   match)
{
  GtkCssSelectorRuleSetInfo *info;

  g_array_set_size (builder->infos, builder->infos->len + 1);
  info = &g_array_index (builder->infos, GtkCssSelectorRuleSetInfo, builder->infos->len - 1);
  info->match = match;
  info->current_selector = selectors;
  info->selector_match = selector_match;
}

/* Convert all offsets to node-relative */
static void
fixup_offsets (GtkCssSelectorTree *tree, guint8 *data)
{
  while (tree != NULL)
    {
      if (tree->parent_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->parent_offset -= ((guint8 *)tree - data);

      if (tree->previous_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->previous_offset -= ((guint8 *)tree - data);

      if (tree->sibling_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->sibling_offset -= ((guint8 *)tree - data);

      if (tree->matches_offset != GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET)
	tree->matches_offset -= ((guint8 *)tree - data);

      fixup_offsets ((GtkCssSelectorTree *)gtk_css_selector_tree_get_previous (tree), data);

      tree = (GtkCssSelectorTree *)gtk_css_selector_tree_get_sibling (tree);
    }
}

GtkCssSelectorTree *
_gtk_css_selector_tree_builder_build (GtkCssSelectorTreeBuilder *builder)
{
  GtkCssSelectorTree *tree;
  GByteArray *array;
  guint8 *data;
  guint len;
  guint i;
  GtkCssSelectorRuleSetInfo **infos_array;

  array = g_byte_array_new ();

  infos_array = g_alloca (sizeof (GtkCssSelectorRuleSetInfo *) * builder->infos->len);
  for (i = 0; i < builder->infos->len; i++)
    infos_array[i] = &g_array_index (builder->infos, GtkCssSelectorRuleSetInfo, i);

  subdivide_infos (array, infos_array, builder->infos->len, GTK_CSS_SELECTOR_TREE_EMPTY_OFFSET);

  len = array->len;
  data = g_byte_array_free (array, FALSE);

  /* shrink to final size */
  data = g_realloc (data, len);

  tree = (GtkCssSelectorTree *)data;

  fixup_offsets (tree, data);

  /* Convert offsets to final pointers */
  for (i = 0; i < builder->infos->len; i++)
    {
      GtkCssSelectorRuleSetInfo *info = &g_array_index (builder->infos, GtkCssSelectorRuleSetInfo, i);

      if (info->selector_match)
	*info->selector_match = (GtkCssSelectorTree *)(data + GPOINTER_TO_UINT (*info->selector_match));
    }


#ifdef PRINT_TREE
  {
    GString *s = g_string_new ("");
    _gtk_css_selector_tree_print (tree, s, "");
    g_print ("%s", s->str);
    g_string_free (s, TRUE);
  }
#endif

  return tree;
}
