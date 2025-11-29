/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnodeprivate.h"

#include "gtkcssstaticstyleprivate.h"
#include "gtkcssanimatedstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkmarshalers.h"
#include "gtksettingsprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gdkprofilerprivate.h"

/*
 * CSS nodes are the backbone of the GtkStyleContext implementation and
 * replace the role that GtkWidgetPath played in the past. A CSS node has
 * an element name and a state, and can have an id and style classes, which
 * is what is needed to determine the matching CSS selectors. CSS nodes have
 * a 'visible' property, which makes it possible to temporarily 'hide' them
 * from CSS matching - e.g. an invisible node will not affect :nth-child
 * matching and so forth.
 *
 * The API to manage states, names, ids and classes of CSS nodes is:
 * - gtk_css_node_get/set_state. States are represented as GtkStateFlags
 * - gtk_css_node_get/set_name. Names are represented as interned strings
 * - gtk_css_node_get/set_id. Ids are represented as interned strings
 * - gtk_css_node_add/remove/has_class and gtk_css_node_list_classes. Style
 *   classes are represented as quarks.
 *
 * CSS nodes are organized in a dom-like tree, and there is API to navigate
 * and manipulate this tree:
 * - gtk_css_node_set_parent
 * - gtk_css_node_insert_before/after
 * - gtk_css_node_get_parent
 * - gtk_css_node_get_first/last_child
 * - gtk_css_node_get_previous/next_sibling
 * Note that parents keep a reference on their children in this tree.
 *
 * Every widget has one or more CSS nodes - the first one gets created
 * automatically by GtkStyleContext. To set the name of the main node,
 * call gtk_widget_class_set_css_name() in class_init(). Widget implementations
 * can and should add subnodes as suitable.
 *
 * Best practice is:
 * - For permanent subnodes, create them in init(), and keep a pointer
 *   to the node (you don't have to keep a reference, cleanup will be
 *   automatic by means of the parent node getting cleaned up by the
 *   style context).
 * - For transient nodes, create/destroy them when the conditions that
 *   warrant their existence change.
 * - Keep the state of all your nodes up-to-date. This probably requires
 *   a ::state-flags-changed (and possibly ::direction-changed) handler,
 *   as well as code to update the state in other places. Note that GTK
 *   does this automatically for the widget's main CSS node.
 * - The sibling ordering in the CSS node tree is supposed to correspond
 *   to the visible order of content: top-to-bottom and left-to-right.
 *   Reorder your nodes to maintain this correlation. In particular for
 *   horizontally laid out widgets, this will require listening to
 *   ::direction-changed.
 * - The draw function should just use gtk_style_context_save_to_node() to
 *   'switch' to the right node, not make any other changes to the style
 *   context.
 *
 * A noteworthy difference between gtk_style_context_save() and
 * gtk_style_context_save_to_node() is that the former inherits all the
 * style classes from the main CSS node, which often leads to unintended
 * inheritance.
 */

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_CSS_RADICAL_CHANGE (GTK_CSS_CHANGE_ID | GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | \
                                GTK_CSS_CHANGE_PARENT_ID | GTK_CSS_CHANGE_PARENT_NAME | GTK_CSS_CHANGE_PARENT_CLASS | \
                                GTK_CSS_CHANGE_SOURCE | GTK_CSS_CHANGE_PARENT_STYLE)

/* When these change, we need to recompute the change flags for the new style
 * since they may have changed.
 */
#define GTK_CSS_CHANGE_NEEDS_RECOMPUTE (GTK_CSS_RADICAL_CHANGE & ~GTK_CSS_CHANGE_PARENT_STYLE)

G_DEFINE_TYPE (GtkCssNode, gtk_css_node, G_TYPE_OBJECT)

enum {
  STYLE_CHANGED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_CLASSES,
  PROP_ID,
  PROP_NAME,
  PROP_STATE,
  PROP_VISIBLE,
  NUM_PROPERTIES
};

static guint cssnode_signals[LAST_SIGNAL] = { 0 };
static GParamSpec *cssnode_properties[NUM_PROPERTIES];

static GtkStyleProvider *
gtk_css_node_get_style_provider_or_null (GtkCssNode *cssnode)
{
  return GTK_CSS_NODE_GET_CLASS (cssnode)->get_style_provider (cssnode);
}

static int invalidated_nodes;
static int created_styles;
static guint invalidated_nodes_counter;
static guint created_styles_counter;

static void
gtk_css_node_set_invalid (GtkCssNode *node,
                          gboolean    invalid)
{
  if (node->invalid == invalid)
    return;

  node->invalid = invalid;

  if (invalid)
    invalidated_nodes++;

  if (node->visible)
    {
      if (node->parent)
        {
          if (invalid)
            gtk_css_node_set_invalid (node->parent, TRUE);
        }
      else
        {
          if (invalid)
            GTK_CSS_NODE_GET_CLASS (node)->queue_validate (node);
          else
            GTK_CSS_NODE_GET_CLASS (node)->dequeue_validate (node);
        }
    }
}

static void
gtk_css_node_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  switch (property_id)
    {
    case PROP_CLASSES:
      g_value_take_boxed (value, gtk_css_node_get_classes (cssnode));
      break;

    case PROP_ID:
      g_value_set_string (value, g_quark_to_string (gtk_css_node_get_id (cssnode)));
      break;

    case PROP_NAME:
      g_value_set_string (value, g_quark_to_string (gtk_css_node_get_name (cssnode)));
      break;

    case PROP_STATE:
      g_value_set_flags (value, gtk_css_node_get_state (cssnode));
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, gtk_css_node_get_visible (cssnode));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_css_node_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  switch (property_id)
    {
    case PROP_CLASSES:
      gtk_css_node_set_classes (cssnode, g_value_get_boxed (value));
      break;

    case PROP_ID:
      gtk_css_node_set_id (cssnode, g_quark_from_string (g_value_get_string (value)));
      break;

    case PROP_NAME:
      gtk_css_node_set_name (cssnode, g_quark_from_string (g_value_get_string (value)));
      break;

    case PROP_STATE:
      gtk_css_node_set_state (cssnode, g_value_get_flags (value));
      break;

    case PROP_VISIBLE:
      gtk_css_node_set_visible (cssnode, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_css_node_dispose (GObject *object)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  while (cssnode->first_child)
    {
      gtk_css_node_set_parent (cssnode->first_child, NULL);
    }

  gtk_css_node_set_invalid (cssnode, FALSE);

  g_clear_pointer (&cssnode->cache, gtk_css_node_style_cache_unref);

 if (cssnode->children_observer)
   gtk_list_list_model_clear (cssnode->children_observer);

  G_OBJECT_CLASS (gtk_css_node_parent_class)->dispose (object);
}

static void
gtk_css_node_finalize (GObject *object)
{
  GtkCssNode *cssnode = GTK_CSS_NODE (object);

  if (cssnode->style)
    g_object_unref (cssnode->style);
  gtk_css_node_declaration_unref (cssnode->decl);

  G_OBJECT_CLASS (gtk_css_node_parent_class)->finalize (object);
}

static gboolean
gtk_css_node_is_first_child (GtkCssNode *node)
{
  GtkCssNode *iter;

  for (iter = node->previous_sibling;
       iter != NULL;
       iter = iter->previous_sibling)
    {
      if (iter->visible)
        return FALSE;
    }
  return TRUE;
}

static gboolean
gtk_css_node_is_last_child (GtkCssNode *node)
{
  GtkCssNode *iter;

  for (iter = node->next_sibling;
       iter != NULL;
       iter = iter->next_sibling)
    {
      if (iter->visible)
        return FALSE;
    }
  return TRUE;
}

static gboolean
may_use_global_parent_cache (GtkCssNode *node)
{
  GtkStyleProvider *provider;
  GtkCssNode *parent;

  parent = gtk_css_node_get_parent (node);
  if (parent == NULL)
    return FALSE;

  provider = gtk_css_node_get_style_provider_or_null (node);
  if (provider != NULL && provider != gtk_css_node_get_style_provider (parent))
    return FALSE;

  return TRUE;
}

static GtkCssStyle *
lookup_in_global_parent_cache (GtkCssNode                  *node,
                               const GtkCssNodeDeclaration *decl)
{
  GtkCssNode *parent;

  parent = node->parent;

  if (parent == NULL ||
      !may_use_global_parent_cache (node))
    return NULL;

  if (parent->cache == NULL)
    return NULL;

  g_assert (node->cache == NULL);
  node->cache = gtk_css_node_style_cache_lookup (parent->cache,
                                                 decl,
                                                 gtk_css_node_is_first_child (node),
                                                 gtk_css_node_is_last_child (node));
  if (node->cache == NULL)
    return NULL;

  return gtk_css_node_style_cache_get_style (node->cache);
}

static void
store_in_global_parent_cache (GtkCssNode                  *node,
                              const GtkCssNodeDeclaration *decl,
                              GtkCssStyle                 *style)
{
  GtkCssNode *parent;

  g_assert (GTK_IS_CSS_STATIC_STYLE (style));

  parent = node->parent;

  if (parent == NULL ||
      !may_use_global_parent_cache (node))
    return;

  if (parent->cache == NULL)
    parent->cache = gtk_css_node_style_cache_new (parent->style);

  node->cache = gtk_css_node_style_cache_insert (parent->cache,
                                                 (GtkCssNodeDeclaration *) decl,
                                                 gtk_css_node_is_first_child (node),
                                                 gtk_css_node_is_last_child (node),
                                                 style);
}

static GtkCssStyle *
gtk_css_node_create_style (GtkCssNode                   *cssnode,
                           const GtkCountingBloomFilter *filter,
                           GtkCssChange                  change)
{
  const GtkCssNodeDeclaration *decl;
  GtkCssStyle *style;
  GtkCssChange style_change;

  decl = gtk_css_node_get_declaration (cssnode);

  style = lookup_in_global_parent_cache (cssnode, decl);
  if (style)
    return g_object_ref (style);

  created_styles++;

  if (change & GTK_CSS_CHANGE_NEEDS_RECOMPUTE)
    {
      /* Need to recompute the change flags */
      style_change = 0;
    }
  else
    {
      style_change = gtk_css_static_style_get_change (gtk_css_style_get_static_style (cssnode->style));
    }

  style = gtk_css_static_style_new_compute (gtk_css_node_get_style_provider (cssnode),
                                            filter,
                                            cssnode,
                                            style_change);

  store_in_global_parent_cache (cssnode, decl, style);

  return style;
}

static gboolean
should_create_transitions (GtkCssChange change)
{
  return (change & GTK_CSS_CHANGE_ANIMATIONS) == 0;
}

static gboolean
gtk_css_style_needs_recreation (GtkCssStyle  *style,
                                GtkCssChange  change)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STATIC_STYLE (style), TRUE);

  /* Try to avoid invalidating if we can */
  if (change & GTK_CSS_RADICAL_CHANGE)
    return TRUE;

  if (gtk_css_static_style_get_change (GTK_CSS_STATIC_STYLE (style)) & change)
    return TRUE;
  else
    return FALSE;
}

static GtkCssStyle *
gtk_css_node_real_update_style (GtkCssNode                   *cssnode,
                                const GtkCountingBloomFilter *filter,
                                GtkCssChange                  change,
                                gint64                        timestamp,
                                GtkCssStyle                  *style)
{
  GtkCssStyle *static_style, *new_static_style, *new_style;

  static_style = GTK_CSS_STYLE (gtk_css_style_get_static_style (style));

  if (gtk_css_style_needs_recreation (static_style, change))
    new_static_style = gtk_css_node_create_style (cssnode, filter, change);
  else
    new_static_style = g_object_ref (static_style);

  if (new_static_style != static_style || (change & GTK_CSS_CHANGE_ANIMATIONS))
    {
      GtkCssNode *parent = gtk_css_node_get_parent (cssnode);
      new_style = gtk_css_animated_style_new (new_static_style,
                                              parent ? gtk_css_node_get_style (parent) : NULL,
                                              timestamp,
                                              gtk_css_node_get_style_provider (cssnode),
                                              should_create_transitions (change) ? style : NULL);

      /* Clear the cache again, the static style we looked up above
       * may have populated it. */
      g_clear_pointer (&cssnode->cache, gtk_css_node_style_cache_unref);
    }
  else if (static_style != style && (change & GTK_CSS_CHANGE_TIMESTAMP))
    {
      new_style = gtk_css_animated_style_new_advance (GTK_CSS_ANIMATED_STYLE (style),
                                                      static_style,
                                                      timestamp);
    }
  else
    {
      new_style = g_object_ref (style);
    }

  if (!gtk_css_style_is_static (new_style))
    gtk_css_node_set_invalid (cssnode, TRUE);

  g_object_unref (new_static_style);

  return new_style;
}

static void
gtk_css_node_real_queue_validate (GtkCssNode *node)
{
}

static void
gtk_css_node_real_dequeue_validate (GtkCssNode *node)
{
}

static void
gtk_css_node_real_validate (GtkCssNode *node)
{
}

static GtkStyleProvider *
gtk_css_node_real_get_style_provider (GtkCssNode *cssnode)
{
  return NULL;
}

static GdkFrameClock *
gtk_css_node_real_get_frame_clock (GtkCssNode *cssnode)
{
  return NULL;
}

static void
gtk_css_node_real_node_removed (GtkCssNode *parent,
                                GtkCssNode *node,
                                GtkCssNode *previous)
{
  if (node->previous_sibling)
    node->previous_sibling->next_sibling = node->next_sibling;
  else
    node->parent->first_child = node->next_sibling;

  if (node->next_sibling)
    node->next_sibling->previous_sibling = node->previous_sibling;
  else
    node->parent->last_child = node->previous_sibling;

  node->previous_sibling = NULL;
  node->next_sibling = NULL;
  node->parent = NULL;
}

static void
gtk_css_node_real_node_added (GtkCssNode *parent,
                              GtkCssNode *node,
                              GtkCssNode *new_previous)
{
  if (new_previous)
    {
      node->previous_sibling = new_previous;
      node->next_sibling = new_previous->next_sibling;
      new_previous->next_sibling = node;
    }
  else
    {
      node->next_sibling = parent->first_child;
      parent->first_child = node;
    }

  if (node->next_sibling)
    node->next_sibling->previous_sibling = node;
  else
    parent->last_child = node;

  node->parent = parent;
}

static void
gtk_css_node_real_style_changed (GtkCssNode        *cssnode,
                                 GtkCssStyleChange *change)
{
  g_set_object (&cssnode->style, gtk_css_style_change_get_new_style (change));
}

static void
gtk_css_node_class_init (GtkCssNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_css_node_get_property;
  object_class->set_property = gtk_css_node_set_property;
  object_class->dispose = gtk_css_node_dispose;
  object_class->finalize = gtk_css_node_finalize;

  klass->update_style = gtk_css_node_real_update_style;
  klass->validate = gtk_css_node_real_validate;
  klass->queue_validate = gtk_css_node_real_queue_validate;
  klass->dequeue_validate = gtk_css_node_real_dequeue_validate;
  klass->get_style_provider = gtk_css_node_real_get_style_provider;
  klass->get_frame_clock = gtk_css_node_real_get_frame_clock;

  klass->node_added = gtk_css_node_real_node_added;
  klass->node_removed = gtk_css_node_real_node_removed;
  klass->style_changed = gtk_css_node_real_style_changed;

  cssnode_signals[STYLE_CHANGED] =
    g_signal_new (I_("style-changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkCssNodeClass, style_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_POINTER);

  cssnode_properties[PROP_CLASSES] =
    g_param_spec_boxed ("classes", NULL, NULL,
                         G_TYPE_STRV,
                         G_PARAM_READWRITE
                         | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE
                         | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE
                         | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_STATE] =
    g_param_spec_flags ("state", NULL, NULL,
                        GTK_TYPE_STATE_FLAGS,
                        0,
                        G_PARAM_READWRITE
                        | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  cssnode_properties[PROP_VISIBLE] =
    g_param_spec_boolean ("visible", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE
                          | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, cssnode_properties);

  if (invalidated_nodes_counter == 0)
    {
      invalidated_nodes_counter = gdk_profiler_define_int_counter ("invalidated-nodes", "CSS Node Invalidations");
      created_styles_counter = gdk_profiler_define_int_counter ("created-styles", "CSS Style Creations");
    }
}

static void
gtk_css_node_init (GtkCssNode *cssnode)
{
  cssnode->decl = gtk_css_node_declaration_new ();

  cssnode->style = g_object_ref (gtk_css_static_style_get_default ());

  cssnode->visible = TRUE;
}

/**
 * gtk_css_node_new:
 *
 * Creates a new CSS node.
 *
 * Returns: (transfer full): the new CSS node
 */
GtkCssNode *
gtk_css_node_new (void)
{
  return g_object_new (GTK_TYPE_CSS_NODE, NULL);
}

static GdkFrameClock *
gtk_css_node_get_frame_clock_or_null (GtkCssNode *cssnode)
{
  while (cssnode->parent)
    cssnode = cssnode->parent;

  return GTK_CSS_NODE_GET_CLASS (cssnode)->get_frame_clock (cssnode);
}

static gint64
gtk_css_node_get_timestamp (GtkCssNode *cssnode)
{
  GdkFrameClock *frameclock;

  frameclock = gtk_css_node_get_frame_clock_or_null (cssnode);
  if (frameclock == NULL)
    return 0;

  return gdk_frame_clock_get_frame_time (frameclock);
}

static void
gtk_css_node_parent_was_unset (GtkCssNode *node)
{
  if (node->visible && node->invalid)
    GTK_CSS_NODE_GET_CLASS (node)->queue_validate (node);
}

static void
gtk_css_node_parent_will_be_set (GtkCssNode *node)
{
  if (node->visible && node->invalid)
    GTK_CSS_NODE_GET_CLASS (node)->dequeue_validate (node);
}

static void
gtk_css_node_invalidate_style (GtkCssNode *cssnode)
{
  if (cssnode->style_is_invalid)
    return;

  cssnode->style_is_invalid = TRUE;
  gtk_css_node_set_invalid (cssnode, TRUE);

  if (cssnode->first_child)
    gtk_css_node_invalidate_style (cssnode->first_child);

  if (cssnode->next_sibling)
    gtk_css_node_invalidate_style (cssnode->next_sibling);
}

static void
gtk_css_node_reposition (GtkCssNode *node,
                         GtkCssNode *new_parent,
                         GtkCssNode *previous)
{
  GtkCssNode *old_parent;
  GtkCssNode *old_previous;

  g_assert (! (new_parent == NULL && previous != NULL));

  old_parent = node->parent;
  old_previous = node->previous_sibling;

  /* Take a reference here so the whole function has a reference */
  g_object_ref (node);

  if (node->visible)
    {
      if (node->next_sibling)
        gtk_css_node_invalidate (node->next_sibling,
                                 GTK_CSS_CHANGE_ANY_SIBLING
                                 | GTK_CSS_CHANGE_NTH_CHILD
                                 | (node->previous_sibling ? 0 : GTK_CSS_CHANGE_FIRST_CHILD));
      else if (node->previous_sibling)
        gtk_css_node_invalidate (node->previous_sibling, GTK_CSS_CHANGE_LAST_CHILD);
    }

  if (old_parent != NULL)
    {
      GTK_CSS_NODE_GET_CLASS (old_parent)->node_removed (old_parent, node, node->previous_sibling);
      if (old_parent->children_observer && old_parent != new_parent)
        gtk_list_list_model_item_removed (old_parent->children_observer, old_previous);
      if (old_parent->first_child && node->visible)
        gtk_css_node_invalidate (old_parent->first_child, GTK_CSS_CHANGE_NTH_LAST_CHILD);
    }

  if (old_parent != new_parent)
    {
      if (old_parent == NULL)
        {
          gtk_css_node_parent_will_be_set (node);
        }
      else
        {
          g_object_unref (node);
        }

      if (gtk_css_node_get_style_provider_or_null (node) == NULL)
        gtk_css_node_invalidate_style_provider (node);
      gtk_css_node_invalidate (node, GTK_CSS_CHANGE_TIMESTAMP | GTK_CSS_CHANGE_ANIMATIONS);

      if (new_parent)
        {
          g_object_ref (node);

          if (node->pending_changes)
            new_parent->needs_propagation = TRUE;
          if (node->invalid && node->visible)
            gtk_css_node_set_invalid (new_parent, TRUE);
        }
      else
        {
          gtk_css_node_parent_was_unset (node);
        }
    }

  if (new_parent)
    {
      GTK_CSS_NODE_GET_CLASS (new_parent)->node_added (new_parent, node, previous);
      if (node->visible)
        gtk_css_node_invalidate (new_parent->first_child, GTK_CSS_CHANGE_NTH_LAST_CHILD);
    }

  if (node->visible)
    {
      if (node->next_sibling)
        {
          if (node->previous_sibling == NULL)
            gtk_css_node_invalidate (node->next_sibling, GTK_CSS_CHANGE_FIRST_CHILD);
          else
            gtk_css_node_invalidate_style (node->next_sibling);
        }
      else if (node->previous_sibling)
        {
          gtk_css_node_invalidate (node->previous_sibling, GTK_CSS_CHANGE_LAST_CHILD);
        }
    }
  else
    {
      if (node->next_sibling)
        gtk_css_node_invalidate_style (node->next_sibling);
    }

  gtk_css_node_invalidate (node, (old_parent != new_parent ? GTK_CSS_CHANGE_ANY_PARENT : 0)
                                 | GTK_CSS_CHANGE_ANY_SIBLING
                                 | GTK_CSS_CHANGE_NTH_CHILD
                                 | (node->previous_sibling ? 0 : GTK_CSS_CHANGE_FIRST_CHILD)
                                 | (node->next_sibling ? 0 : GTK_CSS_CHANGE_LAST_CHILD));

  if (new_parent && new_parent->children_observer)
    {
      if (old_previous && old_parent == new_parent)
        gtk_list_list_model_item_moved (new_parent->children_observer, node, old_previous);
      else
        gtk_list_list_model_item_added (new_parent->children_observer, node);
    }

  g_object_unref (node);
}

void
gtk_css_node_set_parent (GtkCssNode *node,
                         GtkCssNode *parent)
{
  if (node->parent == parent)
    return;

  gtk_css_node_reposition (node, parent, parent ? parent->last_child : NULL);
}

/* If previous_sibling is NULL, insert at the beginning */
void
gtk_css_node_insert_after (GtkCssNode *parent,
                           GtkCssNode *cssnode,
                           GtkCssNode *previous_sibling)
{
  g_return_if_fail (previous_sibling == NULL || previous_sibling->parent == parent);
  g_return_if_fail (cssnode != previous_sibling);

  if (cssnode->previous_sibling == previous_sibling &&
      cssnode->parent == parent)
    return;

  gtk_css_node_reposition (cssnode,
                           parent,
                           previous_sibling);
}

/* If next_sibling is NULL, insert at the end */
void
gtk_css_node_insert_before (GtkCssNode *parent,
                            GtkCssNode *cssnode,
                            GtkCssNode *next_sibling)
{
  g_return_if_fail (next_sibling == NULL || next_sibling->parent == parent);
  g_return_if_fail (cssnode != next_sibling);

  if (cssnode->next_sibling == next_sibling &&
      cssnode->parent == parent)
    return;

  gtk_css_node_reposition (cssnode,
                           parent,
                           next_sibling ? next_sibling->previous_sibling : parent->last_child);
}

GtkCssNode *
gtk_css_node_get_parent (GtkCssNode *cssnode)
{
  return cssnode->parent;
}

GtkCssNode *
gtk_css_node_get_first_child (GtkCssNode *cssnode)
{
  return cssnode->first_child;
}

GtkCssNode *
gtk_css_node_get_last_child (GtkCssNode *cssnode)
{
  return cssnode->last_child;
}

GtkCssNode *
gtk_css_node_get_previous_sibling (GtkCssNode *cssnode)
{
  return cssnode->previous_sibling;
}

GtkCssNode *
gtk_css_node_get_next_sibling (GtkCssNode *cssnode)
{
  return cssnode->next_sibling;
}

static gboolean
gtk_css_node_set_style (GtkCssNode  *cssnode,
                        GtkCssStyle *style)
{
  GtkCssStyleChange change;
  gboolean style_changed;

  if (cssnode->style == style)
    return FALSE;

  gtk_css_style_change_init (&change, cssnode->style, style);

  style_changed = gtk_css_style_change_has_change (&change);
  if (style_changed)
    {
      g_signal_emit (cssnode, cssnode_signals[STYLE_CHANGED], 0, &change);
    }
  else if (GTK_IS_CSS_ANIMATED_STYLE (cssnode->style) || GTK_IS_CSS_ANIMATED_STYLE (style))
    {
      /* This is when animations are starting/stopping but they didn't change any CSS this frame */
      g_set_object (&cssnode->style, style);
    }
  else if (gtk_css_static_style_get_change (gtk_css_style_get_static_style (cssnode->style)) !=
           gtk_css_static_style_get_change (gtk_css_style_get_static_style (style)))
    {
      /* This is when we recomputed the change flags but the style didn't change */
      g_set_object (&cssnode->style, style);
    }

  gtk_css_style_change_finish (&change);

  return style_changed;
}

static void
gtk_css_node_propagate_pending_changes (GtkCssNode *cssnode,
                                        gboolean    style_changed)
{
  GtkCssChange change, child_change;
  GtkCssNode *child;

  change = _gtk_css_change_for_child (cssnode->pending_changes);
  if (style_changed)
    change |= GTK_CSS_CHANGE_PARENT_STYLE;

  if (!cssnode->needs_propagation && change == 0)
    return;

  for (child = gtk_css_node_get_first_child (cssnode);
       child;
       child = gtk_css_node_get_next_sibling (child))
    {
      child_change = child->pending_changes;
      gtk_css_node_invalidate (child, change);
      if (child->visible)
        change |= _gtk_css_change_for_sibling (child_change);
    }

  cssnode->needs_propagation = FALSE;
}

static gboolean
gtk_css_node_needs_new_style (GtkCssNode *cssnode)
{
  return cssnode->style_is_invalid || cssnode->needs_propagation;
}

static void
gtk_css_node_do_ensure_style (GtkCssNode                   *cssnode,
                              const GtkCountingBloomFilter *filter,
                              gint64                        current_time)
{
  gboolean style_changed;

  if (cssnode->style_is_invalid)
    {
      GtkCssStyle *new_style;

      g_clear_pointer (&cssnode->cache, gtk_css_node_style_cache_unref);

      new_style = GTK_CSS_NODE_GET_CLASS (cssnode)->update_style (cssnode,
                                                                  filter,
                                                                  cssnode->pending_changes,
                                                                  current_time,
                                                                  cssnode->style);

      style_changed = gtk_css_node_set_style (cssnode, new_style);
      g_object_unref (new_style);
    }
  else
    {
      style_changed = FALSE;
    }

  gtk_css_node_propagate_pending_changes (cssnode, style_changed);

  cssnode->pending_changes = 0;
  cssnode->style_is_invalid = FALSE;
}

static void
gtk_css_node_ensure_style (GtkCssNode                   *cssnode,
                           const GtkCountingBloomFilter *filter,
                           gint64                        current_time)
{
  GtkCssNode *sibling;

  if (!gtk_css_node_needs_new_style (cssnode))
    return;

  if (cssnode->parent)
    gtk_css_node_ensure_style (cssnode->parent, filter, current_time);

  /* Ensure all siblings before this have a valid style, in order
   * starting at the first that needs it. */
  sibling = cssnode;
  while (sibling->style_is_invalid &&
         sibling->previous_sibling != NULL &&
         gtk_css_node_needs_new_style (sibling->previous_sibling))
    sibling = sibling->previous_sibling;

  while (sibling != cssnode)
    {
      gtk_css_node_do_ensure_style (sibling, filter, current_time);
      sibling = sibling->next_sibling;
    }

  gtk_css_node_do_ensure_style (cssnode, filter, current_time);
}

GtkCssStyle *
gtk_css_node_get_style (GtkCssNode *cssnode)
{
  if (gtk_css_node_needs_new_style (cssnode))
    {
      gint64 timestamp = gtk_css_node_get_timestamp (cssnode);

      gtk_css_node_ensure_style (cssnode, NULL, timestamp);
    }

  return cssnode->style;
}

void
gtk_css_node_set_visible (GtkCssNode *cssnode,
                          gboolean    visible)
{
  GtkCssNode *iter;

  if (cssnode->visible == visible)
    return;

  cssnode->visible = visible;
  g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_VISIBLE]);

  if (cssnode->invalid)
    {
      if (cssnode->visible)
        {
          if (cssnode->parent)
            gtk_css_node_set_invalid (cssnode->parent, TRUE);
          else
            GTK_CSS_NODE_GET_CLASS (cssnode)->queue_validate (cssnode);
        }
      else
        {
          if (cssnode->parent == NULL)
            GTK_CSS_NODE_GET_CLASS (cssnode)->dequeue_validate (cssnode);
        }
    }

  if (cssnode->next_sibling)
    {
      gtk_css_node_invalidate (cssnode->next_sibling, GTK_CSS_CHANGE_ANY_SIBLING | GTK_CSS_CHANGE_NTH_CHILD);
      if (gtk_css_node_is_first_child (cssnode))
        {
          for (iter = cssnode->next_sibling;
               iter != NULL;
               iter = iter->next_sibling)
            {
              gtk_css_node_invalidate (iter, GTK_CSS_CHANGE_FIRST_CHILD);
              if (iter->visible)
                break;
            }
        }
    }

  if (cssnode->previous_sibling)
    {
      if (gtk_css_node_is_last_child (cssnode))
        {
          for (iter = cssnode->previous_sibling;
               iter != NULL;
               iter = iter->previous_sibling)
            {
              gtk_css_node_invalidate (iter, GTK_CSS_CHANGE_LAST_CHILD);
              if (iter->visible)
                break;
            }
        }
      gtk_css_node_invalidate (cssnode->parent->first_child, GTK_CSS_CHANGE_NTH_LAST_CHILD);
    }
}

gboolean
gtk_css_node_get_visible (GtkCssNode *cssnode)
{
  return cssnode->visible;
}

void
gtk_css_node_set_name (GtkCssNode *cssnode,
                       GQuark      name)
{
  if (gtk_css_node_declaration_set_name (&cssnode->decl, name))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_NAME);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_NAME]);
    }
}

GQuark
gtk_css_node_get_name (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_name (cssnode->decl);
}

void
gtk_css_node_set_id (GtkCssNode *cssnode,
                     GQuark      id)
{
  if (gtk_css_node_declaration_set_id (&cssnode->decl, id))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_ID);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_ID]);
    }
}

GQuark
gtk_css_node_get_id (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_id (cssnode->decl);
}

void
gtk_css_node_set_state (GtkCssNode    *cssnode,
                        GtkStateFlags  state_flags)
{
  GtkStateFlags old_state;

  old_state = gtk_css_node_declaration_get_state (cssnode->decl);

  if (gtk_css_node_declaration_set_state (&cssnode->decl, state_flags))
    {
      GtkStateFlags states = old_state ^ state_flags;
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

      gtk_css_node_invalidate (cssnode, change);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_STATE]);
    }
}

GtkStateFlags
gtk_css_node_get_state (GtkCssNode *cssnode)
{
  return gtk_css_node_declaration_get_state (cssnode->decl);
}

static void
gtk_css_node_clear_classes (GtkCssNode *cssnode)
{
  if (gtk_css_node_declaration_clear_classes (&cssnode->decl))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_CLASSES]);
    }
}

void
gtk_css_node_set_classes (GtkCssNode  *cssnode,
                          const char **classes)
{
  guint i;

  g_object_freeze_notify (G_OBJECT (cssnode));

  gtk_css_node_clear_classes (cssnode);

  if (classes)
    {
      for (i = 0; classes[i] != NULL; i++)
        {
          gtk_css_node_add_class (cssnode, g_quark_from_string (classes[i]));
        }
    }

  g_object_thaw_notify (G_OBJECT (cssnode));
}

char **
gtk_css_node_get_classes (GtkCssNode *cssnode)
{
  const GQuark *classes;
  char **result;
  guint n_classes, i, j;

  classes = gtk_css_node_declaration_get_classes (cssnode->decl, &n_classes);
  result = g_new (char *, n_classes + 1);

  for (i = n_classes, j = 0; i-- > 0; ++j)
    {
      result[j] = g_strdup (g_quark_to_string (classes[i]));
    }

  result[n_classes] = NULL;
  return result;
}

void
gtk_css_node_add_class (GtkCssNode *cssnode,
                        GQuark      style_class)
{
  if (gtk_css_node_declaration_add_class (&cssnode->decl, style_class))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_CLASSES]);
    }
}

void
gtk_css_node_remove_class (GtkCssNode *cssnode,
                           GQuark      style_class)
{
  if (gtk_css_node_declaration_remove_class (&cssnode->decl, style_class))
    {
      gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_CLASS);
      g_object_notify_by_pspec (G_OBJECT (cssnode), cssnode_properties[PROP_CLASSES]);
    }
}

gboolean
gtk_css_node_has_class (GtkCssNode *cssnode,
                        GQuark      style_class)
{
  return gtk_css_node_declaration_has_class (cssnode->decl, style_class);
}

const GQuark *
gtk_css_node_list_classes (GtkCssNode *cssnode,
                           guint      *n_classes)
{
  return gtk_css_node_declaration_get_classes (cssnode->decl, n_classes);
}

const GtkCssNodeDeclaration *
gtk_css_node_get_declaration (GtkCssNode *cssnode)
{
  return cssnode->decl;
}

void
gtk_css_node_invalidate_style_provider (GtkCssNode *cssnode)
{
  GtkCssNode *child;

  gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_SOURCE);

  for (child = cssnode->first_child;
       child;
       child = child->next_sibling)
    {
      if (gtk_css_node_get_style_provider_or_null (child) == NULL)
        gtk_css_node_invalidate_style_provider (child);
    }
}

static void
gtk_css_node_invalidate_timestamp (GtkCssNode *cssnode)
{
  GtkCssNode *child;

  if (!cssnode->invalid)
    return;

  if (!gtk_css_style_is_static (cssnode->style))
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_TIMESTAMP);

  for (child = cssnode->first_child; child; child = child->next_sibling)
    {
      gtk_css_node_invalidate_timestamp (child);
    }
}

void
gtk_css_node_invalidate_frame_clock (GtkCssNode *cssnode,
                                     gboolean    just_timestamp)
{
  /* frame clock is handled by the top level */
  if (cssnode->parent)
    return;

  gtk_css_node_invalidate_timestamp (cssnode);

  if (!just_timestamp)
    gtk_css_node_invalidate (cssnode, GTK_CSS_CHANGE_ANIMATIONS);
}

void
gtk_css_node_invalidate (GtkCssNode   *cssnode,
                         GtkCssChange  change)
{
  if (!cssnode->invalid)
    change &= ~GTK_CSS_CHANGE_TIMESTAMP;

  if (change == 0)
    return;

  cssnode->pending_changes |= change;

  if (cssnode->parent)
    cssnode->parent->needs_propagation = TRUE;
  gtk_css_node_invalidate_style (cssnode);
}

static void
gtk_css_node_validate_internal (GtkCssNode             *cssnode,
                                GtkCountingBloomFilter *filter,
                                gint64                  timestamp)
{
  GtkCssNode *child;
  gboolean bloomed = FALSE;

  if (!cssnode->invalid)
    return;

  gtk_css_node_ensure_style (cssnode, filter, timestamp);

  /* need to set to FALSE then to TRUE here to make it chain up */
  gtk_css_node_set_invalid (cssnode, FALSE);
  if (!gtk_css_style_is_static (cssnode->style))
    gtk_css_node_set_invalid (cssnode, TRUE);

  GTK_CSS_NODE_GET_CLASS (cssnode)->validate (cssnode);

  for (child = gtk_css_node_get_first_child (cssnode);
       child;
       child = gtk_css_node_get_next_sibling (child))
    {
      if (!child->visible)
        continue;

      if (!bloomed)
        {
          gtk_css_node_declaration_add_bloom_hashes (cssnode->decl, filter);
          bloomed = TRUE;
        }

      gtk_css_node_validate_internal (child, filter, timestamp);
    }

  if (bloomed)
    gtk_css_node_declaration_remove_bloom_hashes (cssnode->decl, filter);
}

void
gtk_css_node_validate (GtkCssNode *cssnode)
{
  GtkCountingBloomFilter filter = GTK_COUNTING_BLOOM_FILTER_INIT;
  gint64 timestamp;
  gint64 before G_GNUC_UNUSED;

  before = GDK_PROFILER_CURRENT_TIME;

  g_assert (cssnode->parent == NULL);

  timestamp = gtk_css_node_get_timestamp (cssnode);

  gtk_css_node_validate_internal (cssnode, &filter, timestamp);

  if (GDK_PROFILER_IS_RUNNING)
    {
      gdk_profiler_end_mark (before,  "Validate CSS", "");
      gdk_profiler_set_int_counter (invalidated_nodes_counter, invalidated_nodes);
      gdk_profiler_set_int_counter (created_styles_counter, created_styles);
      invalidated_nodes = 0;
      created_styles = 0;
    }
}

GtkStyleProvider *
gtk_css_node_get_style_provider (GtkCssNode *cssnode)
{
  GtkStyleProvider *result;

  result = gtk_css_node_get_style_provider_or_null (cssnode);
  if (result)
    return result;

  if (cssnode->parent)
    return gtk_css_node_get_style_provider (cssnode->parent);

  return GTK_STYLE_PROVIDER (_gtk_settings_get_style_cascade (gtk_settings_get_default (), 1));
}

void
gtk_css_node_print (GtkCssNode           *cssnode,
                    GtkCssNodePrintFlags  flags,
                    GString              *string,
                    guint                 indent)
{
  gboolean need_newline = FALSE;

  g_string_append_printf (string, "%*s", indent, "");

  if (!cssnode->visible)
    g_string_append_c (string, '[');

  gtk_css_node_declaration_print (cssnode->decl, string);

  if (!cssnode->visible)
    g_string_append_c (string, ']');

  if (flags & GTK_CSS_NODE_PRINT_SHOW_CHANGE)
    {
      GtkCssStyle *style = gtk_css_node_get_style (cssnode);
      GtkCssChange change;

      change = gtk_css_static_style_get_change (gtk_css_style_get_static_style (style));
      g_string_append (string, "    ");
      gtk_css_change_print (change, string);
    }

  g_string_append_c (string, '\n');

  if (flags & GTK_CSS_NODE_PRINT_SHOW_STYLE)
    need_newline = gtk_css_style_print (gtk_css_node_get_style (cssnode), string, indent + 2, TRUE);

  if (flags & GTK_CSS_NODE_PRINT_RECURSE)
    {
      GtkCssNode *node;

      if (need_newline && gtk_css_node_get_first_child (cssnode))
        g_string_append_c (string, '\n');

      for (node = gtk_css_node_get_first_child (cssnode); node; node = gtk_css_node_get_next_sibling (node))
        gtk_css_node_print (node, flags, string, indent + 2);
    }
}

static void
gtk_css_node_child_observer_destroyed (gpointer cssnode)
{
  GTK_CSS_NODE (cssnode)->children_observer = NULL;
}

GListModel *
gtk_css_node_observe_children (GtkCssNode *cssnode)
{
  if (cssnode->children_observer)
    return g_object_ref (G_LIST_MODEL (cssnode->children_observer));

  cssnode->children_observer = gtk_list_list_model_new ((gpointer) gtk_css_node_get_first_child,
                                                        (gpointer) gtk_css_node_get_next_sibling,
                                                        (gpointer) gtk_css_node_get_previous_sibling,
                                                        (gpointer) gtk_css_node_get_last_child,
                                                        (gpointer) g_object_ref,
                                                        cssnode,
                                                        gtk_css_node_child_observer_destroyed);

  return G_LIST_MODEL (cssnode->children_observer);
}

