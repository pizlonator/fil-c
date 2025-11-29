/*
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmenutrackerprivate.h"

/*< private >
 * GtkMenuTracker:
 *
 * GtkMenuTracker is a simple object to ease implementations of GMenuModel.
 * Given a GtkActionObservable (usually a GActionMuxer) along with a
 * GMenuModel, it will tell you which menu items to create and where to place
 * them. If a menu item is removed, it will tell you the position of the menu
 * item to remove.
 *
 * Using GtkMenuTracker is fairly simple. The only guarantee you must make
 * to GtkMenuTracker is that you must obey all insert signals and track the
 * position of items that GtkMenuTracker gives you. That is, GtkMenuTracker
 * expects positions of all the latter items to change when it calls your
 * insertion callback with an early position, as it may ask you to remove
 * an item with a readjusted position later.
 *
 * GtkMenuTracker will give you a GtkMenuTrackerItem in your callback. You
 * must hold onto this object until a remove signal is emitted. This item
 * represents a single menu item, which can be one of three classes: normal item,
 * separator, or submenu.
 *
 * Certain properties on the GtkMenuTrackerItem are mutable, and you must
 * listen for changes in the item. For more details, see the documentation
 * for GtkMenuTrackerItem along with https://wiki.gnome.org/Projects/GLib/GApplication/GMenuModel.
 *
 * The idea of @with_separators is for special cases where menu models may
 * be tracked in places where separators are not available, like in toplevel
 * "File", “Edit” menu bars. Ignoring separator items is wrong, as GtkMenuTracker
 * expects the position to change, so we must tell GtkMenuTracker to ignore
 * separators itself.
 */

typedef struct _GtkMenuTrackerSection GtkMenuTrackerSection;

struct _GtkMenuTracker
{
  GtkActionObservable      *observable;
  guint                     merge_sections : 1;
  guint                     mac_os_mode    : 1;
  GtkMenuTrackerInsertFunc  insert_func;
  GtkMenuTrackerRemoveFunc  remove_func;
  gpointer                  user_data;

  GtkMenuTrackerSection    *toplevel;
};

struct _GtkMenuTrackerSection
{
  gpointer    model;   /* may be a GtkMenuTrackerItem or a GMenuModel */
  GSList     *items;
  char       *action_namespace;

  guint       separator_label : 1;
  guint       with_separators : 1;
  guint       has_separator   : 1;
  guint       is_fake         : 1;

  gulong      handler;
};

static GtkMenuTrackerSection *  gtk_menu_tracker_section_new    (GtkMenuTracker        *tracker,
                                                                 GMenuModel            *model,
                                                                 gboolean               with_separators,
                                                                 gboolean               separator_label,
                                                                 int                    offset,
                                                                 const char            *action_namespace);
static void                    gtk_menu_tracker_section_free    (GtkMenuTrackerSection *section);

static GtkMenuTrackerSection *
gtk_menu_tracker_section_find_model (GtkMenuTrackerSection *section,
                                     gpointer               model,
                                     int                   *offset)
{
  GSList *item;

  if (section->has_separator)
    (*offset)++;

  if (section->model == model)
    return section;

  for (item = section->items; item; item = item->next)
    {
      GtkMenuTrackerSection *subsection = item->data;

      if (subsection)
        {
          GtkMenuTrackerSection *found_section;

          found_section = gtk_menu_tracker_section_find_model (subsection, model, offset);

          if (found_section)
            return found_section;
        }
      else
        (*offset)++;
    }

  return NULL;
}

/* this is responsible for syncing the showing of a separator for a
 * single subsection (and its children).
 *
 * we only ever show separators if we have _actual_ children (ie: we do
 * not show a separator if the section contains only empty child
 * sections).  it’s difficult to determine this on-the-fly, so we have
 * this separate function to come back later and figure it out.
 *
 * 'section' is that section.
 *
 * 'tracker' is passed in so that we can emit callbacks when we decide
 * to add/remove separators.
 *
 * 'offset' is passed in so we know which position to emit in our
 * callbacks.  ie: if we add a separator right at the top of this
 * section then we would emit it with this offset.  deeper inside, we
 * adjust accordingly.
 *
 * could_have_separator is true in two situations:
 *
 *  - our parent section had with_separators defined and there are items
 *    before us (ie: we should add a separator if we have content in
 *    order to divide us from the items above)
 *
 *  - if we had a “label” attribute set for this section
 *
 * parent_model and parent_index are passed in so that we can give them
 * to the insertion callback so that it can see the label (and anything
 * else that happens to be defined on the section).
 *
 * we iterate each item in ourselves.  for subsections, we recursively
 * run ourselves to sync separators.  after we are done, we notice if we
 * have any items in us or if we are completely empty and sync if our
 * separator is shown or not.
 */
static int
gtk_menu_tracker_section_sync_separators (GtkMenuTrackerSection *section,
                                          GtkMenuTracker        *tracker,
                                          int                    offset,
                                          gboolean               could_have_separator,
                                          GMenuModel            *parent_model,
                                          int                    parent_index)
{
  gboolean should_have_separator;
  int n_items = 0;
  GSList *item;
  int i = 0;

  for (item = section->items; item; item = item->next)
    {
      GtkMenuTrackerSection *subsection = item->data;

      if (subsection)
        {
          gboolean separator;

          separator = (section->with_separators && n_items > 0) || subsection->separator_label;

          /* Only pass the parent_model and parent_index in case they may be used to create the separator. */
          n_items += gtk_menu_tracker_section_sync_separators (subsection, tracker, offset + n_items,
                                                               separator,
                                                               separator ? section->model : NULL,
                                                               separator ? i : 0);
        }
      else
        n_items++;

      i++;
    }

  should_have_separator = !section->is_fake && could_have_separator && n_items != 0;

  if (should_have_separator > section->has_separator)
    {
      /* Add a separator */
      GtkMenuTrackerItem *separator;

      separator = _gtk_menu_tracker_item_new (tracker->observable, parent_model, parent_index, FALSE, NULL, TRUE);
      (* tracker->insert_func) (separator, offset, tracker->user_data);
      g_object_unref (separator);

      section->has_separator = TRUE;
    }
  else if (should_have_separator < section->has_separator)
    {
      /* Remove a separator */
      (* tracker->remove_func) (offset, tracker->user_data);
      section->has_separator = FALSE;
    }

  n_items += section->has_separator;

  return n_items;
}

static void
gtk_menu_tracker_item_visibility_changed (GtkMenuTrackerItem *item,
                                          GParamSpec         *pspec,
                                          gpointer            user_data)
{
  GtkMenuTracker *tracker = user_data;
  GtkMenuTrackerSection *section;
  gboolean is_now_visible;
  gboolean was_visible;
  int offset = 0;

  is_now_visible = gtk_menu_tracker_item_get_is_visible (item);

  /* remember: the item is our model */
  section = gtk_menu_tracker_section_find_model (tracker->toplevel, item, &offset);
  g_assert (section);

  was_visible = section->items != NULL;

  if (is_now_visible == was_visible)
    return;

  if (is_now_visible)
    {
      section->items = g_slist_prepend (NULL, NULL);
      (* tracker->insert_func) (section->model, offset, tracker->user_data);
    }
  else
    {
      section->items = g_slist_delete_link (section->items, section->items);
      (* tracker->remove_func) (offset, tracker->user_data);
    }

  gtk_menu_tracker_section_sync_separators (tracker->toplevel, tracker, 0, FALSE, NULL, 0);
}

static int
gtk_menu_tracker_section_measure (GtkMenuTrackerSection *section)
{
  GSList *item;
  int n_items;

  if (section == NULL)
    return 1;

  n_items = 0;

  if (section->has_separator)
    n_items++;

  for (item = section->items; item; item = item->next)
    n_items += gtk_menu_tracker_section_measure (item->data);

  return n_items;
}

static void
gtk_menu_tracker_remove_items (GtkMenuTracker  *tracker,
                               GSList         **change_point,
                               int              offset,
                               int              n_items)
{
  int i;

  for (i = 0; i < n_items; i++)
    {
      GtkMenuTrackerSection *subsection;
      int n;

      subsection = (*change_point)->data;
      *change_point = g_slist_delete_link (*change_point, *change_point);

      n = gtk_menu_tracker_section_measure (subsection);
      gtk_menu_tracker_section_free (subsection);

      while (n--)
        (* tracker->remove_func) (offset, tracker->user_data);
    }
}

static void
gtk_menu_tracker_add_items (GtkMenuTracker         *tracker,
                            GtkMenuTrackerSection  *section,
                            GSList                **change_point,
                            int                     offset,
                            GMenuModel             *model,
                            int                     position,
                            int                     n_items)
{
  while (n_items--)
    {
      GMenuModel *submenu;

      submenu = g_menu_model_get_item_link (model, position + n_items, G_MENU_LINK_SECTION);
      g_assert (submenu != model);

      if (submenu != NULL && tracker->merge_sections)
        {
          GtkMenuTrackerSection *subsection;
          char *action_namespace = NULL;
          gboolean has_label;

          has_label = g_menu_model_get_item_attribute (model, position + n_items,
                                                       G_MENU_ATTRIBUTE_LABEL, "s", NULL);

          g_menu_model_get_item_attribute (model, position + n_items,
                                           G_MENU_ATTRIBUTE_ACTION_NAMESPACE, "s", &action_namespace);

          if (section->action_namespace)
            {
              char *namespace;

              namespace = g_strjoin (".", section->action_namespace, action_namespace, NULL);
              subsection = gtk_menu_tracker_section_new (tracker, submenu, FALSE, has_label, offset, namespace);
              g_free (namespace);
            }
          else
            subsection = gtk_menu_tracker_section_new (tracker, submenu, FALSE, has_label, offset, action_namespace);

          *change_point = g_slist_prepend (*change_point, subsection);
          g_free (action_namespace);
          g_object_unref (submenu);
        }
      else
        {
          GtkMenuTrackerItem *item;

          item = _gtk_menu_tracker_item_new (tracker->observable, model, position + n_items,
                                             tracker->mac_os_mode,
                                             section->action_namespace, submenu != NULL);

          /* In the case that the item may disappear we handle that by
           * treating the item that we just created as being its own
           * subsection.  This happens as so:
           *
           *  - the subsection is created without the possibility of
           *    showing a separator
           *
           *  - the subsection will have either 0 or 1 item in it at all
           *    times: either the shown item or not (in the case it is
           *    hidden)
           *
           *  - the created item acts as the "model" for this section
           *    and we use its "visiblity-changed" signal in the same
           *    way that we use the "items-changed" signal from a real
           *    GMenuModel
           *
           * We almost never use the '->model' stored in the section for
           * anything other than lookups and for dropped the ref and
           * disconnecting the signal when we destroy the menu, and we
           * need to do exactly those things in this case as well.
           *
           * The only other thing that '->model' is used for is in the
           * case that we want to show a separator, but we will never do
           * that because separators are not shown for this fake section.
           */
          if (gtk_menu_tracker_item_may_disappear (item))
            {
              GtkMenuTrackerSection *fake_section;

              fake_section = g_new0 (GtkMenuTrackerSection, 1);
              fake_section->is_fake = TRUE;
              fake_section->model = g_object_ref (item);
              fake_section->handler = g_signal_connect (item, "notify::is-visible",
                                                        G_CALLBACK (gtk_menu_tracker_item_visibility_changed),
                                                        tracker);
              *change_point = g_slist_prepend (*change_point, fake_section);

              if (gtk_menu_tracker_item_get_is_visible (item))
                {
                  (* tracker->insert_func) (item, offset, tracker->user_data);
                  fake_section->items = g_slist_prepend (NULL, NULL);
                }
            }
          else
            {
              /* In the normal case, we store NULL in the linked list.
               * The measurement and lookup code count NULL always as
               * exactly 1: an item that will always be there.
               */
              (* tracker->insert_func) (item, offset, tracker->user_data);
              *change_point = g_slist_prepend (*change_point, NULL);
            }

          g_object_unref (item);
        }
    }
}

static void
gtk_menu_tracker_model_changed (GMenuModel *model,
                                int         position,
                                int         removed,
                                int         added,
                                gpointer    user_data)
{
  GtkMenuTracker *tracker = user_data;
  GtkMenuTrackerSection *section;
  GSList **change_point;
  int offset = 0;
  int i;

  /* First find which section the changed model corresponds to, and the
   * position of that section within the overall menu.
   */
  section = gtk_menu_tracker_section_find_model (tracker->toplevel, model, &offset);
  g_assert (section);

  /* Next, seek through that section to the change point.  This gives us
   * the correct GSList** to make the change to and also finds the final
   * offset at which we will make the changes (by measuring the number
   * of items within each item of the section before the change point).
   */
  change_point = &section->items;
  for (i = 0; i < position; i++)
    {
      offset += gtk_menu_tracker_section_measure ((*change_point)->data);
      change_point = &(*change_point)->next;
    }

  /* We remove items in order and add items in reverse order.  This
   * means that the offset used for all inserts and removes caused by a
   * single change will be the same.
   *
   * This also has a performance advantage: GtkMenuShell stores the
   * menu items in a linked list.  In the case where we are creating a
   * menu for the first time, adding the items in reverse order means
   * that we only ever insert at index zero, prepending the list.  This
   * means that we can populate in O(n) time instead of O(n^2) that we
   * would do by appending.
   */
  gtk_menu_tracker_remove_items (tracker, change_point, offset, removed);
  gtk_menu_tracker_add_items (tracker, section, change_point, offset, model, position, added);

  /* The offsets for insertion/removal of separators will be all over
   * the place, however...
   */
  gtk_menu_tracker_section_sync_separators (tracker->toplevel, tracker, 0, FALSE, NULL, 0);
}

static void
gtk_menu_tracker_section_free (GtkMenuTrackerSection *section)
{
  if (section == NULL)
    return;

  g_signal_handler_disconnect (section->model, section->handler);
  g_slist_free_full (section->items, (GDestroyNotify) gtk_menu_tracker_section_free);
  g_free (section->action_namespace);
  g_object_unref (section->model);
  g_free (section);
}

static GtkMenuTrackerSection *
gtk_menu_tracker_section_new (GtkMenuTracker *tracker,
                              GMenuModel     *model,
                              gboolean        with_separators,
                              gboolean        separator_label,
                              int             offset,
                              const char     *action_namespace)
{
  GtkMenuTrackerSection *section;

  section = g_new0 (GtkMenuTrackerSection, 1);
  section->model = g_object_ref (model);
  section->with_separators = with_separators;
  section->action_namespace = g_strdup (action_namespace);
  section->separator_label = separator_label;

  gtk_menu_tracker_add_items (tracker, section, &section->items, offset, model, 0, g_menu_model_get_n_items (model));
  section->handler = g_signal_connect (model, "items-changed", G_CALLBACK (gtk_menu_tracker_model_changed), tracker);

  return section;
}

/*< private >
 * gtk_menu_tracker_new:
 * @model: the model to flatten
 * @with_separators: if the toplevel should have separators (ie: TRUE
 *   for menus, FALSE for menubars)
 * @merge_sections: if sections should have their items merged in the
 *   usual way or reported only as separators (which can be queried to
 *   manually handle the items)
 * @mac_os_mode: if this is on behalf of the Mac OS menubar
 * @action_namespace: the passed-in action namespace
 * @insert_func: insert callback
 * @remove_func: remove callback
 * @user_data user data for callbacks
 *
 * Creates a GtkMenuTracker for @model, holding a ref on @model for as
 * long as the tracker is alive.
 *
 * This flattens out the model, merging sections and inserting
 * separators where appropriate.  It monitors for changes and performs
 * updates on the fly.  It also handles action_namespace for subsections
 * (but you will need to handle it yourself for submenus).
 *
 * When the tracker is first created, @insert_func will be called many
 * times to populate the menu with the initial contents of @model
 * (unless it is empty), before gtk_menu_tracker_new() returns.  For
 * this reason, the menu that is using the tracker ought to be empty
 * when it creates the tracker.
 *
 * Future changes to @model will result in more calls to @insert_func
 * and @remove_func.
 *
 * The position argument to both functions is the linear 0-based
 * position in the menu at which the item in question should be inserted
 * or removed.
 *
 * For @insert_func, @model and @item_index are used to get the
 * information about the menu item to insert.  @action_namespace is the
 * action namespace that actions referred to from that item should place
 * themselves in.  Note that if the item is a submenu and the
 * “action-namespace” attribute is defined on the item, it will _not_ be
 * applied to the @action_namespace argument as it is meant for the
 * items inside of the submenu, not the submenu item itself.
 *
 * @is_separator is set to %TRUE in case the item being added is a
 * separator.  @model and @item_index will still be meaningfully set in
 * this case -- to the section menu item corresponding to the separator.
 * This is useful if the section specifies a label, for example.  If
 * there is an “action-namespace” attribute on this menu item then it
 * should be ignored by the consumer because GtkMenuTracker has already
 * handled it.
 *
 * When using GtkMenuTracker there is no need to hold onto @model or
 * monitor it for changes.  The model will be unreffed when
 * gtk_menu_tracker_free() is called.
 */
GtkMenuTracker *
gtk_menu_tracker_new (GtkActionObservable      *observable,
                      GMenuModel               *model,
                      gboolean                  with_separators,
                      gboolean                  merge_sections,
                      gboolean                  mac_os_mode,
                      const char               *action_namespace,
                      GtkMenuTrackerInsertFunc  insert_func,
                      GtkMenuTrackerRemoveFunc  remove_func,
                      gpointer                  user_data)
{
  GtkMenuTracker *tracker;

  tracker = g_new (GtkMenuTracker, 1);
  tracker->merge_sections = merge_sections;
  tracker->mac_os_mode = mac_os_mode;
  tracker->observable = g_object_ref (observable);
  tracker->insert_func = insert_func;
  tracker->remove_func = remove_func;
  tracker->user_data = user_data;

  tracker->toplevel = gtk_menu_tracker_section_new (tracker, model, with_separators, FALSE, 0, action_namespace);
  gtk_menu_tracker_section_sync_separators (tracker->toplevel, tracker, 0, FALSE, NULL, 0);

  return tracker;
}

GtkMenuTracker *
gtk_menu_tracker_new_for_item_link (GtkMenuTrackerItem       *item,
                                    const char               *link_name,
                                    gboolean                  merge_sections,
                                    gboolean                  mac_os_mode,
                                    GtkMenuTrackerInsertFunc  insert_func,
                                    GtkMenuTrackerRemoveFunc  remove_func,
                                    gpointer                  user_data)
{
  GtkMenuTracker *tracker;
  GMenuModel *submenu;
  char *namespace;

  submenu = _gtk_menu_tracker_item_get_link (item, link_name);
  namespace = _gtk_menu_tracker_item_get_link_namespace (item);

  tracker = gtk_menu_tracker_new (_gtk_menu_tracker_item_get_observable (item), submenu,
                                  TRUE, merge_sections, mac_os_mode,
                                  namespace, insert_func, remove_func, user_data);

  g_object_unref (submenu);
  g_free (namespace);

  return tracker;
}

/*< private >
 * gtk_menu_tracker_free:
 * @tracker: a GtkMenuTracker
 *
 * Frees the tracker, ...
 */
void
gtk_menu_tracker_free (GtkMenuTracker *tracker)
{
  gtk_menu_tracker_section_free (tracker->toplevel);
  g_object_unref (tracker->observable);
  g_free (tracker);
}
