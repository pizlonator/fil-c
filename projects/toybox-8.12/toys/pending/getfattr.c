/* getfattr.c - Read POSIX extended attributes.
 *
 * Copyright 2016 Android Open Source Project.
 *
 * No standard

USE_GETFATTR(NEWTOY(getfattr, "(only-values)dhn:", TOYFLAG_USR|TOYFLAG_BIN))

config GETFATTR
  bool "getfattr"
  default n
  help
    usage: getfattr [-d] [-h] [-n NAME] FILE...

    Read POSIX extended attributes.

    -d	Show values as well as names
    -h	Do not dereference symbolic links
    -n	Show only attributes with the given name
    --only-values	Don't show names
*/

#define FOR_getfattr
#include "toys.h"

GLOBALS(
  char *n;
)

// TODO: factor out the lister and getter loops and use them in cp too.
static void do_getfattr(char *file)
{
  ssize_t (*getter)(const char *, const char *, void *, size_t) = getxattr;
  ssize_t (*lister)(const char *, char *, size_t) = listxattr;
  char **sorted_keys;
  ssize_t keys_len;
  char *keys, *key;
  int i, key_count;

  if (FLAG(h)) {
    getter = lgetxattr;
    lister = llistxattr;
  }

  // Collect the keys.
  while ((keys_len = lister(file, NULL, 0))) {
    if (keys_len == -1) perror_msg("listxattr failed");
    keys = xmalloc(keys_len);
    if (lister(file, keys, keys_len) == keys_len) break;
    free(keys);
  }

  if (keys_len == 0) return;

  // Sort the keys.
  for (key = keys, key_count = 0; key-keys < keys_len; key += strlen(key)+1)
    key_count++;
  sorted_keys = xmalloc(key_count * sizeof(char *));
  for (key = keys, i = 0; key-keys < keys_len; key += strlen(key)+1)
    sorted_keys[i++] = key;
  qsort(sorted_keys, key_count, sizeof(char *), qstrcmp);

  if (!FLAG(only_values)) printf("# file: %s\n", file);

  for (i = 0; i < key_count; i++) {
    key = sorted_keys[i];

    if (TT.n && strcmp(TT.n, key)) continue;

    if (FLAG(d) || FLAG(only_values)) {
      ssize_t value_len;
      char *value = NULL;

      while ((value_len = getter(file, key, NULL, 0))) {
        if (value_len == -1) perror_msg("getxattr failed");
        value = xzalloc(value_len+1);
        if (getter(file, key, value, value_len) == value_len) break;
        free(value);
      }

      if (FLAG(only_values)) {
        if (value) printf("%s", value);
      } else if (!value) puts(key);
      else printf("%s=\"%s\"\n", key, value);
      free(value);
    } else puts(key); // Just list names.
  }

  if (!FLAG(only_values)) xputc('\n');
  free(sorted_keys);
}

void getfattr_main(void)
{
  char **s;

  for (s=toys.optargs; *s; s++) do_getfattr(*s);
}
