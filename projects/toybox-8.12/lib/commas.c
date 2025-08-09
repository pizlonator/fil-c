/* commas.c - Deal with comma separated lists
 *
 * Copyright 2018 Rob Landley <rob@landley.net>
 */

#include "toys.h"

// Traverse arg_list of csv, calling callback on each value
void comma_args(struct arg_list *al, void *data, char *err,
  char *(*callback)(void *data, char *str, int len))
{
  char *next, *arg;
  int len;

  if (CFG_TOYBOX_DEBUG && !err) err = "INTERNAL";

  while (al) {
    arg = al->arg;
    while ((next = comma_iterate(&arg, &len)))
      if ((next = callback(data, next, len)))
        error_exit("%s '%s'\n%*c", err, al->arg,
          (int)(5+strlen(toys.which->name)+strlen(err)+next-al->arg), '^');
    al = al->next;
  }
}

// Realloc *old with oldstring,newstring

void comma_collate(char **old, char *new)
{
  char *temp, *atold = *old;

  // Only add a comma if old string didn't end with one
  if (atold && *atold) {
    char *comma = ",";

    if (atold[strlen(atold)-1] == ',') comma = "";
    temp = xmprintf("%s%s%s", atold, comma, new);
  } else temp = xstrdup(new);
  free (atold);
  *old = temp;
}

// iterate through strings in a comma separated list.
// returns start of next entry or NULL if none
// sets *len to length of entry (not including comma)
// advances *list to start of next entry
char *comma_iterate(char **list, int *len)
{
  char *start = *list, *end;

  if (!*list || !**list) return 0;

  if (!(end = strchr(*list, ','))) {
    *len = strlen(*list);
    *list = 0;
  } else *list += (*len = end-start)+1;

  return start;
}

// Check all instances of opt and "no"opt in optlist, return true if opt
// found and last instance wasn't no. If clean, remove each instance from list.
int comma_scan(char *optlist, char *opt, int clean)
{
  int optlen = strlen(opt), len, no, got = 0;

  if (optlist) for (;;) {
    char *s = comma_iterate(&optlist, &len);

    if (!s) break;
    no = 2*(*s == 'n' && s[1] == 'o');
    if (optlen == len-no && !strncmp(opt, s+no, optlen)) {
      got = !no;
      if (clean) {
        if (optlist) memmove(s, optlist, strlen(optlist)+1);
        else *s = 0;
      }
    }
  }

  return got;
}

// return true if all scanlist options enabled in optlist
int comma_scanall(char *optlist, char *scanlist)
{
  int i = 1;

  while (scanlist && *scanlist) {
    char *opt = comma_iterate(&scanlist, &i), *s = xstrndup(opt, i);

    i = comma_scan(optlist, s, 0);
    free(s);
    if (!i) break;
  }

  return i;
}

// Returns true and removes `opt` from `optlist` if present, false otherwise.
// Doesn't have the magic "no" behavior of comma_scan.
int comma_remove(char *optlist, char *opt)
{
  int optlen = strlen(opt), len, got = 0;

  if (optlist) for (;;) {
    char *s = comma_iterate(&optlist, &len);

    if (!s) break;
    if (optlen == len && !strncmp(opt, s, optlen)) {
      got = 1;
      if (optlist) memmove(s, optlist, strlen(optlist)+1);
      else *s = 0;
    }
  }

  return got;
}
