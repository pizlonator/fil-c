/* llist.c - Linked list functions
 *
 * Linked list structures have a next pointer as their first element.
 */

#include "toys.h"

// Callback function to free data pointer of double_list or arg_list

void llist_free_arg(void *node)
{
  struct arg_list *d = node;

  free(d->arg);
  free(d);
}

void llist_free_double(void *node)
{
  struct double_list *d = node;

  free(d->data);
  free(d);
}

// Call a function (such as free()) on each element of a linked list.
void llist_traverse(void *list, void (*using)(void *node))
{
  void *old = list;

  while (list) {
    void *pop = llist_pop(&list);
    using(pop);

    // End doubly linked list too.
    if (old == list) break;
  }
}

// Return the first item from the list, advancing the list (which must be called
// as &list)
void *llist_pop(void *list)
{
  void **llist = list, **next;

  if (!list || !*llist) return 0;
  next = (void **)*llist;
  *llist = *next;

  return next;
}

// Remove first item from &list and return it
void *dlist_pop(void *list)
{
  struct double_list **pdlist = (struct double_list **)list, *dlist = *pdlist;

  if (!dlist) return 0;
  if (dlist->next == dlist) *pdlist = 0;
  else {
    if (dlist->next) dlist->next->prev = dlist->prev;
    if (dlist->prev) dlist->prev->next = dlist->next;
    *pdlist = dlist->next;
  }

  return dlist;
}

// remove last item from &list and return it (stack pop)
void *dlist_lpop(void *list)
{
  struct double_list *dl = *(struct double_list **)list;
  void *v = 0;

  if (dl) {
    dl = dl->prev;
    v = dlist_pop(&dl);
    if (!dl) *(void **)list = 0;
  }

  return v;
}

// Append to list in-order (*list unchanged unless empty, ->prev is new node)
void dlist_add_nomalloc(struct double_list **list, struct double_list *new)
{
  if (*list) {
    new->next = *list;
    new->prev = (*list)->prev;
    (*list)->prev->next = new;
    (*list)->prev = new;
  } else *list = new->next = new->prev = new;
}

// Add an entry to the end of a doubly linked list
struct double_list *dlist_add(struct double_list **list, char *data)
{
  struct double_list *new = xmalloc(sizeof(struct double_list));

  new->data = data;
  dlist_add_nomalloc(list, new);

  return new;
}

// Terminate circular list for traversal in either direction. Returns end *.
void *dlist_terminate(void *list)
{
  struct double_list *end = list;

  if (!end || !end->prev) return 0;

  end = end->prev;
  end->next->prev = 0;
  end->next = 0;

  return end;
}
