/* Copyright (C) 2019 Red Hat, Inc.
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

#include "gtkistringprivate.h"
#include "gtktexthistoryprivate.h"

/*
 * The GtkTextHistory works in a way that allows text widgets to deliver
 * information about changes to the underlying text at given offsets within
 * their text. The GtkTextHistory object uses a series of callback functions
 * (see GtkTextHistoryFuncs) to apply changes as undo/redo is performed.
 *
 * The GtkTextHistory object is careful to avoid tracking changes while
 * applying specific undo/redo actions.
 *
 * Changes are tracked within a series of actions, contained in groups.  The
 * group may be coalesced when gtk_text_history_end_user_action() is
 * called.
 *
 * Calling gtk_text_history_begin_irreversible_action() and
 * gtk_text_history_end_irreversible_action() can be used to denote a
 * section of operations that cannot be undone. This will cause all previous
 * changes tracked by the GtkTextHistory to be discarded.
 */

typedef struct _Action     Action;
typedef enum   _ActionKind ActionKind;

enum _ActionKind
{
  ACTION_KIND_BARRIER             = 1,
  ACTION_KIND_DELETE_BACKSPACE    = 2,
  ACTION_KIND_DELETE_KEY          = 3,
  ACTION_KIND_DELETE_PROGRAMMATIC = 4,
  ACTION_KIND_DELETE_SELECTION    = 5,
  ACTION_KIND_GROUP               = 6,
  ACTION_KIND_INSERT              = 7,
};

struct _Action
{
  ActionKind kind;
  GList link;
  guint is_modified : 1;
  guint is_modified_set : 1;
  union {
    struct {
      IString istr;
      guint begin;
      guint end;
    } insert;
    struct {
      IString istr;
      guint begin;
      guint end;
      struct {
        int insert;
        int bound;
      } selection;
    } delete;
    struct {
      GQueue actions;
      guint  depth;
    } group;
  } u;
};

struct _GtkTextHistory
{
  GObject             parent_instance;

  GtkTextHistoryFuncs funcs;
  gpointer            funcs_data;

  GQueue              undo_queue;
  GQueue              redo_queue;

  struct {
    int insert;
    int bound;
  } selection;

  guint               irreversible;
  guint               in_user;
  guint               max_undo_levels;

  guint               can_undo : 1;
  guint               can_redo : 1;
  guint               is_modified : 1;
  guint               is_modified_set : 1;
  guint               applying : 1;
  guint               enabled : 1;
};

static void action_free (Action *action);

G_DEFINE_TYPE (GtkTextHistory, gtk_text_history, G_TYPE_OBJECT)

#define return_if_applying(instance)     \
  G_STMT_START {                         \
    if ((instance)->applying)            \
      return;                            \
  } G_STMT_END
#define return_if_irreversible(instance) \
  G_STMT_START {                         \
    if ((instance)->irreversible)        \
      return;                            \
  } G_STMT_END
#define return_if_not_enabled(instance)  \
  G_STMT_START {                         \
    if (!(instance)->enabled)            \
      return;                            \
  } G_STMT_END

static const char *
action_kind_name (ActionKind kind)
{
  switch (kind)
    {
    case ACTION_KIND_BARRIER: return "Barrier";
    case ACTION_KIND_DELETE_BACKSPACE: return "Delete_Backspace";
    case ACTION_KIND_DELETE_KEY: return "Delete_Key";
    case ACTION_KIND_DELETE_PROGRAMMATIC: return "Delete_Programmatic";
    case ACTION_KIND_DELETE_SELECTION: return "Delete_Selection";
    case ACTION_KIND_GROUP: return "Group";
    case ACTION_KIND_INSERT: return "Insert";
    default: return "unknown";
    }
}

static inline void
gtk_text_history_printf_space (GString *str,
                               guint    depth)
{
  for (guint i = 0; i < depth; i++)
    g_string_append (str, "  ");
}

static void
gtk_text_history_printf_action (Action  *action,
                                GString *str,
                                guint    depth)
{
  gtk_text_history_printf_space (str, depth);
  g_string_append_printf (str, "%s {\n", action_kind_name (action->kind));

  gtk_text_history_printf_space (str, depth+1);
  g_string_append_printf (str, "is_modified: %s\n", action->is_modified ? "true" : "false");
  gtk_text_history_printf_space (str, depth+1);
  g_string_append_printf (str, "is_modified_set: %s\n", action->is_modified_set ? "true" : "false");

  switch (action->kind)
    {
    case ACTION_KIND_BARRIER:
      gtk_text_history_printf_space (str, depth+1);
      g_string_append_printf (str, "depth: %u\n", action->u.group.depth);
      break;

    case ACTION_KIND_DELETE_BACKSPACE:
    case ACTION_KIND_DELETE_KEY:
    case ACTION_KIND_DELETE_PROGRAMMATIC:
    case ACTION_KIND_DELETE_SELECTION:
      {
        char *escaped;

        gtk_text_history_printf_space (str, depth+1);
        g_string_append_printf (str, "begin: %u\n", action->u.delete.begin);
        gtk_text_history_printf_space (str, depth+1);
        g_string_append_printf (str, "end: %u\n", action->u.delete.end);
        gtk_text_history_printf_space (str, depth+1);
        g_string_append (str, "selection {\n");
        gtk_text_history_printf_space (str, depth+2);
        g_string_append_printf (str, "insert: %d\n", action->u.delete.selection.insert);
        gtk_text_history_printf_space (str, depth+2);
        g_string_append_printf (str, "bound: %d\n", action->u.delete.selection.bound);
        gtk_text_history_printf_space (str, depth+1);
        g_string_append (str, "}\n");
        gtk_text_history_printf_space (str, depth+1);
        escaped = g_strescape (istring_str (&action->u.delete.istr), NULL);
        g_string_append_printf (str, "text: \"%s\"\n", escaped);
        g_free (escaped);
      }
      break;

    case ACTION_KIND_INSERT:
      {
        char *escaped;

        gtk_text_history_printf_space (str, depth+1);
        g_string_append_printf (str, "begin: %u\n", action->u.insert.begin);
        gtk_text_history_printf_space (str, depth+1);
        g_string_append_printf (str, "end: %u\n", action->u.insert.end);
        gtk_text_history_printf_space (str, depth+1);
        escaped = g_strescape (istring_str (&action->u.insert.istr), NULL);
        g_string_append_printf (str, "text: \"%s\"\n", escaped);
        g_free (escaped);
      }
      break;

    case ACTION_KIND_GROUP:
      gtk_text_history_printf_space (str, depth+1);
      g_string_append_printf (str, "depth: %u\n", action->u.group.depth);

      for (const GList *iter = action->u.group.actions.head; iter; iter = iter->next)
        {
          Action *child = iter->data;

          gtk_text_history_printf_space (str, depth+1);
          g_string_append (str, "children {\n");
          gtk_text_history_printf_action (child, str, depth+2);
          gtk_text_history_printf_space (str, depth+1);
          g_string_append (str, "}\n");
        }

      break;

    default:
      break;
    }

  gtk_text_history_printf_space (str, depth);
  g_string_append (str, "}\n");
}

G_GNUC_UNUSED static char *
gtk_text_history_printf (GtkTextHistory *history)
{
  GString *str = g_string_new (NULL);

  g_string_append (str, "undo {\n");
  for (const GList *iter = history->undo_queue.head; iter; iter = iter->next)
    gtk_text_history_printf_action (iter->data, str, 1);
  g_string_append (str, "}\n");

  g_string_append (str, "redo {\n");
  for (const GList *iter = history->redo_queue.head; iter; iter = iter->next)
    gtk_text_history_printf_action (iter->data, str, 1);
  g_string_append (str, "}\n");

  return g_string_free (str, FALSE);
}

static inline void
uint_order (guint *a,
            guint *b)
{
  if (*a > *b)
    {
      guint tmp = *a;
      *a = *b;
      *b = tmp;
    }
}

static void
clear_action_queue (GQueue *queue)
{
  g_assert (queue != NULL);

  while (queue->length > 0)
    {
      Action *action = g_queue_peek_head (queue);
      g_queue_unlink (queue, &action->link);
      action_free (action);
    }
}

static Action *
action_new (ActionKind kind)
{
  Action *action;

  action = g_new0 (Action, 1);
  action->kind = kind;
  action->link.data = action;

  return action;
}

static void
action_free (Action *action)
{
  if (action->kind == ACTION_KIND_INSERT)
    istring_clear (&action->u.insert.istr);
  else if (action->kind == ACTION_KIND_DELETE_BACKSPACE ||
           action->kind == ACTION_KIND_DELETE_KEY ||
           action->kind == ACTION_KIND_DELETE_PROGRAMMATIC ||
           action->kind == ACTION_KIND_DELETE_SELECTION)
    istring_clear (&action->u.delete.istr);
  else if (action->kind == ACTION_KIND_GROUP)
    clear_action_queue (&action->u.group.actions);

  g_free (action);
}

static gboolean
action_group_is_empty (const Action *action)
{
  const GList *iter;

  g_assert (action->kind == ACTION_KIND_GROUP);

  for (iter = action->u.group.actions.head; iter; iter = iter->next)
    {
      const Action *child = iter->data;

      if (child->kind == ACTION_KIND_BARRIER)
        continue;

      if (child->kind == ACTION_KIND_GROUP && action_group_is_empty (child))
        continue;

      return FALSE;
    }

  return TRUE;
}

static gboolean
action_chain (Action   *action,
              Action   *other,
              gboolean  in_user_action)
{
  g_assert (action != NULL);
  g_assert (other != NULL);

  if (action->kind == ACTION_KIND_GROUP)
    {
      Action *tail = g_queue_peek_tail (&action->u.group.actions);

      /* Always push new items onto a group, so that we can coalesce
       * items when gtk_text_history_end_user_action() is called.
       *
       * But we don't care if this is a barrier since we will always
       * apply things as a group anyway.
       */

      if (other->kind == ACTION_KIND_BARRIER)
        {
          /* If we're not in a user action, this barrier is meant to
           * stop items from coalescing into this group.
           */
          if (!in_user_action && action->u.group.depth == 0)
            return FALSE;

          action_free (other);
          return TRUE;
        }

      /* Try to chain onto the tail item in the group to increase
       * the chances we have a single action within the group. That
       * way we are more likely to hoist out of the group when the
       * user action is ended.
       */
      if (tail != NULL && tail->kind == other->kind)
        {
          if (action_chain (tail, other, in_user_action))
            return TRUE;
        }

      g_queue_push_tail_link (&action->u.group.actions, &other->link);

      return TRUE;
    }

  /* The rest can only be merged to themselves */
  if (action->kind != other->kind)
    return FALSE;

  switch (action->kind)
    {
    case ACTION_KIND_INSERT: {

      /* Make sure the new insert is at the end of the previous */
      if (action->u.insert.end != other->u.insert.begin)
        return FALSE;

      /* If we are not within a user action, be more selective */
      if (!in_user_action)
        {
          /* Avoid pathological cases */
          if (other->u.insert.istr.n_chars > 1000)
            return FALSE;

          /* We will coalesce space, but not new lines. */
          if (istring_contains_unichar (&action->u.insert.istr, '\n') ||
              istring_contains_unichar (&other->u.insert.istr, '\n'))
            return FALSE;

          /* Chain space to items that ended in space. This is generally
           * just at the start of a line where we could have indentation
           * space.
           */
          if ((istring_empty (&action->u.insert.istr) ||
               istring_ends_with_space (&action->u.insert.istr)) &&
              istring_only_contains_space (&other->u.insert.istr))
            goto do_chain;

          /* Starting a new word, don't chain this */
          if (istring_starts_with_space (&other->u.insert.istr))
            return FALSE;

          /* Check for possible paste (multi-character input) or word input that
           * has spaces in it (and should treat as one operation).
           */
          if (other->u.insert.istr.n_chars > 1 &&
              istring_contains_space (&other->u.insert.istr))
            return FALSE;
        }

    do_chain:

      istring_append (&action->u.insert.istr, &other->u.insert.istr);
      action->u.insert.end += other->u.insert.end - other->u.insert.begin;
      action_free (other);

      return TRUE;
    }

    case ACTION_KIND_DELETE_PROGRAMMATIC:
      /* We can't tell if this should be chained because we don't
       * have a group to coalesce. But unless each action deletes
       * a single character, the overhead isn't too bad as we embed
       * the strings in the action.
       */
      return FALSE;

    case ACTION_KIND_DELETE_SELECTION:
      /* Don't join selection deletes as they should appear as a single
       * operation and have selection reinstanted when performing undo.
       */
      return FALSE;

    case ACTION_KIND_DELETE_BACKSPACE:
      if (other->u.delete.end == action->u.delete.begin)
        {
          istring_prepend (&action->u.delete.istr,
                           &other->u.delete.istr);
          action->u.delete.begin = other->u.delete.begin;
          action_free (other);
          return TRUE;
        }

      return FALSE;

    case ACTION_KIND_DELETE_KEY:
      if (action->u.delete.begin == other->u.delete.begin)
        {
          if (!istring_contains_space (&other->u.delete.istr) ||
              istring_only_contains_space (&action->u.delete.istr))
            {
              istring_append (&action->u.delete.istr, &other->u.delete.istr);
              action->u.delete.end += other->u.delete.istr.n_chars;
              action_free (other);
              return TRUE;
            }
        }

      return FALSE;

    case ACTION_KIND_BARRIER:
      /* Only allow a single barrier to be added. */
      action_free (other);
      return TRUE;

    case ACTION_KIND_GROUP:
    default:
      g_return_val_if_reached (FALSE);
    }
}

static void
gtk_text_history_do_change_state (GtkTextHistory *self,
                                  gboolean        is_modified,
                                  gboolean        can_undo,
                                  gboolean        can_redo)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));

  self->funcs.change_state (self->funcs_data, is_modified, can_undo, can_redo);
}

static void
gtk_text_history_do_insert (GtkTextHistory *self,
                            guint           begin,
                            guint           end,
                            const char     *text,
                            guint           len)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));
  g_assert (text != NULL);

  uint_order (&begin, &end);

  self->funcs.insert (self->funcs_data, begin, end, text, len);
}

static void
gtk_text_history_do_delete (GtkTextHistory *self,
                            guint           begin,
                            guint           end,
                            const char     *expected_text,
                            guint           len)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));

  uint_order (&begin, &end);

  self->funcs.delete (self->funcs_data, begin, end, expected_text, len);
}

static void
gtk_text_history_do_select (GtkTextHistory *self,
                            guint           selection_insert,
                            guint           selection_bound)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));

  self->funcs.select (self->funcs_data, selection_insert, selection_bound);
}

static void
gtk_text_history_truncate_one (GtkTextHistory *self)
{
  if (self->undo_queue.length > 0)
    {
      Action *action = g_queue_peek_head (&self->undo_queue);
      g_queue_unlink (&self->undo_queue, &action->link);
      action_free (action);
    }
  else if (self->redo_queue.length > 0)
    {
      Action *action = g_queue_peek_tail (&self->redo_queue);
      g_queue_unlink (&self->redo_queue, &action->link);
      action_free (action);
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
gtk_text_history_truncate (GtkTextHistory *self)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));

  if (self->max_undo_levels == 0)
    return;

  while (self->undo_queue.length + self->redo_queue.length > self->max_undo_levels)
    gtk_text_history_truncate_one (self);
}

static void
gtk_text_history_finalize (GObject *object)
{
  GtkTextHistory *self = (GtkTextHistory *)object;

  clear_action_queue (&self->undo_queue);
  clear_action_queue (&self->redo_queue);

  G_OBJECT_CLASS (gtk_text_history_parent_class)->finalize (object);
}

static void
gtk_text_history_class_init (GtkTextHistoryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_text_history_finalize;
}

static void
gtk_text_history_init (GtkTextHistory *self)
{
  self->enabled = TRUE;
  self->selection.insert = -1;
  self->selection.bound = -1;
}

static gboolean
has_actionable (const GQueue *queue)
{
  const GList *iter;

  for (iter = queue->head; iter; iter = iter->next)
    {
      const Action *action = iter->data;

      if (action->kind == ACTION_KIND_BARRIER)
        continue;

      if (action->kind == ACTION_KIND_GROUP)
        {
          if (has_actionable (&action->u.group.actions))
            return TRUE;
          else
            continue;
        }

      return TRUE;
    }

  return FALSE;
}

static void
gtk_text_history_update_state (GtkTextHistory *self)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));

  if (self->irreversible || self->in_user)
    {
      self->can_undo = FALSE;
      self->can_redo = FALSE;
    }
  else
    {
      self->can_undo = has_actionable (&self->undo_queue);
      self->can_redo = has_actionable (&self->redo_queue);
    }

  gtk_text_history_do_change_state (self, self->is_modified, self->can_undo, self->can_redo);
}

static void
gtk_text_history_push (GtkTextHistory *self,
                       Action         *action)
{
  Action *peek;
  gboolean in_user_action;

  g_assert (GTK_IS_TEXT_HISTORY (self));
  g_assert (self->enabled);
  g_assert (action != NULL);

  while (self->redo_queue.length > 0)
    {
      peek = g_queue_peek_head (&self->redo_queue);
      g_queue_unlink (&self->redo_queue, &peek->link);
      action_free (peek);
    }

  peek = g_queue_peek_tail (&self->undo_queue);
  in_user_action = self->in_user > 0;

  if (peek == NULL || !action_chain (peek, action, in_user_action))
    g_queue_push_tail_link (&self->undo_queue, &action->link);

  gtk_text_history_truncate (self);
  gtk_text_history_update_state (self);
}

GtkTextHistory *
gtk_text_history_new (const GtkTextHistoryFuncs *funcs,
                      gpointer                   funcs_data)
{
  GtkTextHistory *self;

  g_return_val_if_fail (funcs != NULL, NULL);

  self = g_object_new (GTK_TYPE_TEXT_HISTORY, NULL);
  self->funcs = *funcs;
  self->funcs_data = funcs_data;

  return g_steal_pointer (&self);
}

gboolean
gtk_text_history_get_can_undo (GtkTextHistory *self)
{
  g_return_val_if_fail (GTK_IS_TEXT_HISTORY (self), FALSE);

  return self->can_undo;
}

gboolean
gtk_text_history_get_can_redo (GtkTextHistory *self)
{
  g_return_val_if_fail (GTK_IS_TEXT_HISTORY (self), FALSE);

  return self->can_redo;
}

static void
gtk_text_history_apply (GtkTextHistory *self,
                        Action         *action,
                        Action         *peek)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));
  g_assert (action != NULL);

  switch (action->kind)
    {
    case ACTION_KIND_INSERT:
      gtk_text_history_do_insert (self,
                                  action->u.insert.begin,
                                  action->u.insert.end,
                                  istring_str (&action->u.insert.istr),
                                  action->u.insert.istr.n_bytes);

      /* If the next item is a DELETE_SELECTION, then we want to
       * pre-select the text for the user. Otherwise, just place
       * the cursor were we think it was.
       */
      if (peek != NULL && peek->kind == ACTION_KIND_DELETE_SELECTION)
        gtk_text_history_do_select (self,
                                    peek->u.delete.begin,
                                    peek->u.delete.end);
      else
        gtk_text_history_do_select (self,
                                    action->u.insert.end,
                                    action->u.insert.end);

      break;

    case ACTION_KIND_DELETE_BACKSPACE:
    case ACTION_KIND_DELETE_KEY:
    case ACTION_KIND_DELETE_PROGRAMMATIC:
    case ACTION_KIND_DELETE_SELECTION:
      gtk_text_history_do_delete (self,
                                  action->u.delete.begin,
                                  action->u.delete.end,
                                  istring_str (&action->u.delete.istr),
                                  action->u.delete.istr.n_bytes);
      gtk_text_history_do_select (self,
                                  action->u.delete.begin,
                                  action->u.delete.begin);
      break;

    case ACTION_KIND_GROUP: {
      const GList *actions = action->u.group.actions.head;

      for (const GList *iter = actions; iter; iter = iter->next)
        gtk_text_history_apply (self, iter->data, NULL);

      break;
    }

    case ACTION_KIND_BARRIER:
      break;

    default:
      g_assert_not_reached ();
    }

  if (action->is_modified_set)
    self->is_modified = action->is_modified;
}

static void
gtk_text_history_reverse (GtkTextHistory *self,
                          Action         *action)
{
  g_assert (GTK_IS_TEXT_HISTORY (self));
  g_assert (action != NULL);

  switch (action->kind)
    {
    case ACTION_KIND_INSERT:
      gtk_text_history_do_delete (self,
                                  action->u.insert.begin,
                                  action->u.insert.end,
                                  istring_str (&action->u.insert.istr),
                                  action->u.insert.istr.n_bytes);
      gtk_text_history_do_select (self,
                                  action->u.insert.begin,
                                  action->u.insert.begin);
      break;

    case ACTION_KIND_DELETE_BACKSPACE:
    case ACTION_KIND_DELETE_KEY:
    case ACTION_KIND_DELETE_PROGRAMMATIC:
    case ACTION_KIND_DELETE_SELECTION:
      gtk_text_history_do_insert (self,
                                  action->u.delete.begin,
                                  action->u.delete.end,
                                  istring_str (&action->u.delete.istr),
                                  action->u.delete.istr.n_bytes);
      if (action->u.delete.selection.insert != -1 &&
          action->u.delete.selection.bound != -1)
        gtk_text_history_do_select (self,
                                    action->u.delete.selection.insert,
                                    action->u.delete.selection.bound);
      else if (action->u.delete.selection.insert != -1)
        gtk_text_history_do_select (self,
                                    action->u.delete.selection.insert,
                                    action->u.delete.selection.insert);
      break;

    case ACTION_KIND_GROUP: {
      const GList *actions = action->u.group.actions.tail;

      for (const GList *iter = actions; iter; iter = iter->prev)
        gtk_text_history_reverse (self, iter->data);

      break;
    }

    case ACTION_KIND_BARRIER:
      break;

    default:
      g_assert_not_reached ();
    }

  if (action->is_modified_set)
    self->is_modified = !action->is_modified;
}

static void
move_barrier (GQueue   *from_queue,
              Action   *action,
              GQueue   *to_queue,
              gboolean  head)
{
  g_queue_unlink (from_queue, &action->link);

  if (head)
    g_queue_push_head_link (to_queue, &action->link);
  else
    g_queue_push_tail_link (to_queue, &action->link);
}

void
gtk_text_history_undo (GtkTextHistory *self)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

#if 0
  {
    char *str = gtk_text_history_printf (self);
    g_print ("%s\n", str);
    g_free (str);
  }
#endif

  if (gtk_text_history_get_can_undo (self))
    {
      Action *action;

      self->applying = TRUE;

      action = g_queue_peek_tail (&self->undo_queue);

      if (action->kind == ACTION_KIND_BARRIER)
        {
          move_barrier (&self->undo_queue, action, &self->redo_queue, TRUE);
          action = g_queue_peek_tail (&self->undo_queue);
        }

      g_queue_unlink (&self->undo_queue, &action->link);
      g_queue_push_head_link (&self->redo_queue, &action->link);
      gtk_text_history_reverse (self, action);
      gtk_text_history_update_state (self);

      self->applying = FALSE;
    }
}

void
gtk_text_history_redo (GtkTextHistory *self)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

  if (gtk_text_history_get_can_redo (self))
    {
      Action *action;
      Action *peek;

      self->applying = TRUE;

      action = g_queue_peek_head (&self->redo_queue);

      if (action->kind == ACTION_KIND_BARRIER)
        {
          move_barrier (&self->redo_queue, action, &self->undo_queue, FALSE);
          action = g_queue_peek_head (&self->redo_queue);
        }

      g_queue_unlink (&self->redo_queue, &action->link);
      g_queue_push_tail_link (&self->undo_queue, &action->link);

      peek = g_queue_peek_head (&self->redo_queue);

      gtk_text_history_apply (self, action, peek);
      gtk_text_history_update_state (self);

      self->applying = FALSE;
    }
}

void
gtk_text_history_begin_user_action (GtkTextHistory *self)
{
  Action *group;

  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

  self->in_user++;

  group = g_queue_peek_tail (&self->undo_queue);

  if (group == NULL || group->kind != ACTION_KIND_GROUP)
    {
      group = action_new (ACTION_KIND_GROUP);
      gtk_text_history_push (self, group);
    }

  group->u.group.depth++;

  gtk_text_history_update_state (self);
}

void
gtk_text_history_end_user_action (GtkTextHistory *self)
{
  Action *peek;

  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

  clear_action_queue (&self->redo_queue);

  peek = g_queue_peek_tail (&self->undo_queue);

  if (peek->kind != ACTION_KIND_GROUP)
    {
      g_warning ("miss-matched %s end_user_action. Expected group, got %d",
                 G_OBJECT_TYPE_NAME (self),
                 peek->kind);
      return;
    }

  self->in_user--;
  peek->u.group.depth--;

  /* Unless this is the last user action, short-circuit */
  if (peek->u.group.depth > 0)
    return;

  /* Unlikely, but if the group is empty, just remove it */
  if (action_group_is_empty (peek))
    {
      g_queue_unlink (&self->undo_queue, &peek->link);
      action_free (peek);
      goto update_state;
    }

  /* If there is a single item within the group, we can hoist
   * it up increasing the chances that we can join actions.
   */
  if (peek->u.group.actions.length == 1)
    {
      GList *link_ = peek->u.group.actions.head;
      Action *replaced = link_->data;

      replaced->is_modified = peek->is_modified;
      replaced->is_modified_set = peek->is_modified_set;

      g_queue_unlink (&peek->u.group.actions, link_);
      g_queue_unlink (&self->undo_queue, &peek->link);
      action_free (peek);

      gtk_text_history_push (self, replaced);

      goto update_state;
    }

  /* Now insert a barrier action so we don't allow
   * joining items to this node in the future.
   */
  gtk_text_history_push (self, action_new (ACTION_KIND_BARRIER));

update_state:
  gtk_text_history_update_state (self);
}

void
gtk_text_history_begin_irreversible_action (GtkTextHistory *self)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);

  if (self->in_user)
    {
      g_warning ("Cannot begin irreversible action while in user action");
      return;
    }

  self->irreversible++;

  clear_action_queue (&self->undo_queue);
  clear_action_queue (&self->redo_queue);

  gtk_text_history_update_state (self);
}

void
gtk_text_history_end_irreversible_action (GtkTextHistory *self)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);

  if (self->in_user)
    {
      g_warning ("Cannot end irreversible action while in user action");
      return;
    }

  self->irreversible--;

  clear_action_queue (&self->undo_queue);
  clear_action_queue (&self->redo_queue);

  gtk_text_history_update_state (self);
}

static void
gtk_text_history_clear_modified (GtkTextHistory *self)
{
  const GList *iter;

  for (iter = self->undo_queue.head; iter; iter = iter->next)
    {
      Action *action = iter->data;

      action->is_modified = FALSE;
      action->is_modified_set = FALSE;
    }

  for (iter = self->redo_queue.head; iter; iter = iter->next)
    {
      Action *action = iter->data;

      action->is_modified = FALSE;
      action->is_modified_set = FALSE;
    }
}

void
gtk_text_history_modified_changed (GtkTextHistory *self,
                                   gboolean        modified)
{
  Action *peek;

  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);

  /* If we have a new save point, clear all previous modified states. */
  gtk_text_history_clear_modified (self);

  if ((peek = g_queue_peek_tail (&self->undo_queue)))
    {
      if (peek->kind == ACTION_KIND_BARRIER)
        {
          if (!(peek = peek->link.prev->data))
            return;
        }

      peek->is_modified = !!modified;
      peek->is_modified_set = TRUE;
    }

  if ((peek = g_queue_peek_head (&self->redo_queue)))
    {
      if (peek->kind == ACTION_KIND_BARRIER)
        {
          if (peek->link.next == NULL ||
              !(peek = peek->link.next->data))
            return;
        }

      peek->is_modified = TRUE;
      peek->is_modified_set = TRUE;
    }

  self->is_modified = !!modified;
  self->is_modified_set = TRUE;

  if (!self->irreversible)
    gtk_text_history_update_state (self);
}

void
gtk_text_history_selection_changed (GtkTextHistory *self,
                                    int             selection_insert,
                                    int             selection_bound)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

  self->selection.insert = CLAMP (selection_insert, -1, G_MAXINT);
  self->selection.bound = CLAMP (selection_bound, -1, G_MAXINT);
}

void
gtk_text_history_text_inserted (GtkTextHistory *self,
                                guint           position,
                                const char     *text,
                                int             len)
{
  Action *action;
  guint n_chars;

  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

  if (len < 0)
    len = strlen (text);
  n_chars = g_utf8_strlen (text, len);

  action = action_new (ACTION_KIND_INSERT);
  action->u.insert.begin = position;
  action->u.insert.end = position + n_chars;
  istring_set (&action->u.insert.istr, text, len, n_chars);

  gtk_text_history_push (self, action);
}

void
gtk_text_history_text_deleted (GtkTextHistory *self,
                               guint           begin,
                               guint           end,
                               const char     *text,
                               int             len)
{
  Action *action;
  ActionKind kind;

  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  return_if_not_enabled (self);
  return_if_applying (self);
  return_if_irreversible (self);

  if (len < 0)
    len = strlen (text);

  if (self->selection.insert == -1 && self->selection.bound == -1)
    kind = ACTION_KIND_DELETE_PROGRAMMATIC;
  else if (self->selection.insert == end && self->selection.bound == -1)
    kind = ACTION_KIND_DELETE_BACKSPACE;
  else if (self->selection.insert == begin && self->selection.bound == -1)
    kind = ACTION_KIND_DELETE_KEY;
  else
    kind = ACTION_KIND_DELETE_SELECTION;

  action = action_new (kind);
  action->u.delete.begin = begin;
  action->u.delete.end = end;
  action->u.delete.selection.insert = self->selection.insert;
  action->u.delete.selection.bound = self->selection.bound;
  istring_set (&action->u.delete.istr, text, len, ABS (end - begin));

  gtk_text_history_push (self, action);
}

gboolean
gtk_text_history_get_enabled (GtkTextHistory *self)
{
  g_return_val_if_fail (GTK_IS_TEXT_HISTORY (self), FALSE);

  return self->enabled;
}

void
gtk_text_history_set_enabled (GtkTextHistory *self,
                              gboolean        enabled)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  enabled = !!enabled;

  if (self->enabled != enabled)
    {
      self->enabled = enabled;

      if (!self->enabled)
        {
          self->irreversible = 0;
          self->in_user = 0;
          clear_action_queue (&self->undo_queue);
          clear_action_queue (&self->redo_queue);
        }

      gtk_text_history_update_state (self);
    }
}

guint
gtk_text_history_get_max_undo_levels (GtkTextHistory *self)
{
  g_return_val_if_fail (GTK_IS_TEXT_HISTORY (self), 0);

  return self->max_undo_levels;
}

void
gtk_text_history_set_max_undo_levels (GtkTextHistory *self,
                                      guint           max_undo_levels)
{
  g_return_if_fail (GTK_IS_TEXT_HISTORY (self));

  if (self->max_undo_levels != max_undo_levels)
    {
      self->max_undo_levels = max_undo_levels;
      gtk_text_history_truncate (self);
    }
}
