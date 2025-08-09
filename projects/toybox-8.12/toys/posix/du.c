/* du.c - disk usage program.
 *
 * Copyright 2012 Ashwini Kumar <ak.ashwini@gmail.com>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/du.html
 *
 * TODO: cleanup (should seen_inode be lib?)
 * 32 bit du -b maxes out at 4 gigs (instead of 2 terabytes via *512 trick)
 * because dirtree->extra is a long.

USE_DU(NEWTOY(du, "d#<0=-1hmlcaHkKLsxb[-HL][-kKmh]", TOYFLAG_USR|TOYFLAG_BIN))

config DU
  bool "du"
  default y
  help
    usage: du [-d N] [-abcHKkLlmsx] [FILE...]

    Show disk usage, space consumed by files and directories.

    Size in:
    -b	Apparent bytes (directory listing size, not space used)
    -h	Human readable (e.g., 1K 243M 2G)
    -k	1024 byte blocks (default)
    -K	512 byte blocks (posix)
    -m	Megabytes

    What to show:
    -a	All files, not just directories
    -c	Cumulative total
    -d N	Only depth < N
    -H	Follow symlinks on cmdline
    -L	Follow all symlinks
    -l	Disable hardlink filter
    -s	Only total size of each argument
    -x	Don't leave this filesystem
*/

#define FOR_du
#include "toys.h"

GLOBALS(
  long d;

  unsigned long depth, total;
  dev_t st_dev;
  void *inodes;
)

typedef struct node_size {
  struct dirtree *node;
  long size;
} node_size;

// Print the size and name, given size in bytes
static void print(long long size, struct dirtree *node)
{
  char *name = "total";

  if (TT.depth > TT.d) return;

  if (FLAG(h)) {
    human_readable(toybuf, size, 0);
    printf("%s", toybuf);
  } else {
    int bits = 10;

    if (FLAG(K)) bits = 9;
    else if (FLAG(m)) bits = 20;

    if (FLAG(b) && bits == 10 && !FLAG(k)) printf("%llu", size);
    else printf("%llu", (size>>bits)+!!(size&((1<<bits)-1)));
  }
  if (node) name = dirtree_path(node, NULL);
  xprintf("\t%s\n", name);
  if (node) free(name);
}

// Return whether or not we've seen this inode+dev, adding it to the list if
// we haven't.
static int seen_inode(void **list, struct stat *st)
{
  if (!st) llist_traverse(st, free);

  // Skipping dir nodes isn't _quite_ right. They're not hardlinked, but could
  // be bind mounted. Still, it's more efficient and the archivers can't use
  // hardlinked directory info anyway. (Note that we don't catch bind mounted
  // _files_ because it doesn't change st_nlink.)
  else if (!S_ISDIR(st->st_mode) && st->st_nlink > 1) {
    struct inode_list {
      struct inode_list *next;
      struct dev_ino di;
    } *new;

    for (new = *list; new; new = new->next)
      if(same_dev_ino(st, &new->di)) return 1;

    new = xzalloc(sizeof(*new));
    new->di.ino = st->st_ino;
    new->di.dev = st->st_dev;
    new->next = *list;
    *list = new;
  }

  return 0;
}

// dirtree callback, compute/display size of node
static int do_du(struct dirtree *node)
{
  unsigned long blocks, again = node->again&DIRTREE_COMEAGAIN;

  if (!node->parent) TT.st_dev = node->st.st_dev;
  else if (!dirtree_notdotdot(node)) return 0;

  // detect swiching filesystems
  if (FLAG(x) && TT.st_dev != node->st.st_dev) return 0;

  // Don't loop endlessly on recursive directory symlink
  if (FLAG(L)) {
    struct dirtree *try = node;

    while ((try = try->parent)) if (same_file(&node->st, &try->st)) return 0;
  }

  // Don't count hard links twice
  if (!FLAG(l) && !again)
    if (seen_inode(&TT.inodes, &node->st)) return 0;

  // Collect child info before printing directory size
  if (S_ISDIR(node->st.st_mode)) {
    if (!again) {
      TT.depth++;
      return DIRTREE_COMEAGAIN|DIRTREE_SYMFOLLOW*FLAG(L);
    } else TT.depth--;
  }

  // Modern compilers' optimizers are insane and think signed overflow
  // behaves differently than unsigned overflow. Sigh. Big hammer.
  blocks = FLAG(b) ? node->st.st_size : node->st.st_blocks;
  blocks += (unsigned long)node->extra;
  node->extra = blocks;
  if (node->parent)
    node->parent->extra = (unsigned long)node->parent->extra+blocks;
  else TT.total += node->extra;

  if (FLAG(a) || !node->parent || (S_ISDIR(node->st.st_mode) && !FLAG(s))) {
    blocks = node->extra;
    print(FLAG(b) ? blocks : blocks*512LL, node);
  }

  return 0;
}

void du_main(void)
{
  char *noargs[] = {".", 0}, **args;

  // Loop over command line arguments, recursing through children
  for (args = toys.optc ? toys.optargs : noargs; *args; args++)
    dirtree_flagread(*args, DIRTREE_SYMFOLLOW*(FLAG(H)|FLAG(L)), do_du);
  if (FLAG(c)) print(FLAG(b) ? TT.total : TT.total*512, 0);

  if (CFG_TOYBOX_FREE) seen_inode(TT.inodes, 0);
}
