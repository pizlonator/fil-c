/* df.c - report free disk space.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/df.html

USE_DF(NEWTOY(df, "HPkhit*a[-HPh]", TOYFLAG_BIN))

config DF
  bool "df"
  default y
  help
    usage: df [-aHhikP] [-t TYPE] [FILE...]

    The "disk free" command shows total/used/available disk space for
    each filesystem listed on the command line, or all currently mounted
    filesystems.

    -a	Show all (including /proc and friends)
    -H	Human readable (k=1000)
    -h	Human readable (K=1024)
    -i	Show inodes instead of blocks
    -k	Sets units back to 1024 bytes (the default without -P)
    -P	The SUSv3 "Pedantic" option (512 byte blocks)
    -t TYPE	Display only filesystems of this type

    Pedantic provides a slightly less useful output format dictated by POSIX,
    and sets the units to 512 bytes instead of the default 1024 bytes.
*/

#define FOR_df
#include "toys.h"

GLOBALS(
  struct arg_list *t;

  int units, width[6];
)

static void measure_columns(char *s[])
{
  int i;

  for (i = 0; i<5; i++) TT.width[i] = maxof(TT.width[i], strlen(s[i]));
}

static void print_columns(char **dsuapm)
{
  int i;

  for (i = 0; i<6; i++) printf(!i ? "%-*s" : " %*s", TT.width[i], dsuapm[i]);
  xputc('\n');
}

static void print_header()
{
  char *dsuapm[] = {"Filesystem", "Size", "Used", "Avail", "Use%","Mounted on"};

  // The filesystem column is always at least this wide.
  TT.width[0] = maxof(TT.width[0], 14+(FLAG(H)||FLAG(h)));

  if (FLAG(i)) memcpy(dsuapm+1, (char *[]){"Inodes", "IUsed", "IFree", "IUse%"},
                      sizeof(char *)*4);
  else {
    if (!(FLAG(H)||FLAG(h))) {
      dsuapm[1] = TT.units == 512 ? "512-blocks" :
        FLAG(P) ? "1024-blocks" : "1K-blocks";
      dsuapm[3] = "Available";
      if (FLAG(P)) dsuapm[4] = "Capacity";
    }
  }

  measure_columns(dsuapm);
  TT.width[5] = -1;
  print_columns(dsuapm);
}

static void show_mt(struct mtab_list *mt, int measuring)
{
  unsigned long long suap[4], block = 1, ll;
  char *dsuapm[6]; // device, size, used, avail, percent, mount
  int i;

  // If we don't have -a, skip overmounted and synthetic filesystems.
  if (!mt || (!FLAG(a) && (!mt->stat.st_dev || !mt->statvfs.f_blocks))) return;

  // If we have -t, skip other filesystem types
  if (TT.t) {
    struct arg_list *al;

    for (al = TT.t; al; al = al->next) if (!strcmp(mt->type, al->arg)) break;

    if (!al) return;
  }

  // Prepare filesystem display fields
  *dsuapm = *mt->device == '/' ? xabspath(mt->device, 0) : 0;
  if (!*dsuapm) *dsuapm = mt->device;
  if (!mt->stat.st_dev) for (i = 1; i<5; i++) dsuapm[i] = "-";
  else {
    if (FLAG(i)) {
      suap[0] = mt->statvfs.f_files;
      suap[1] = mt->statvfs.f_files - mt->statvfs.f_ffree;
      suap[2] = geteuid() ? mt->statvfs.f_favail : mt->statvfs.f_ffree;
    } else {
      block = maxof(mt->statvfs.f_frsize, 1);
      suap[0] = mt->statvfs.f_blocks;
      suap[1] = mt->statvfs.f_blocks - mt->statvfs.f_bfree;
      suap[2] = geteuid() ? mt->statvfs.f_bavail : mt->statvfs.f_bfree;
    }

    // Scale and convert to strings
    dsuapm[1] = toybuf;
    for (i = 0; i<3; i++) {
      suap[i] = (block*suap[i])/TT.units;

      if (FLAG(H)||FLAG(h))
        human_readable(dsuapm[i+1], suap[i], HR_1000*FLAG(H));
      else sprintf(dsuapm[i+1], "%llu", suap[i]);
      dsuapm[i+2] = strchr(dsuapm[i+1], 0)+1;
    }

    // percent
    if ((suap[3] = ll = suap[1]+suap[2])) {
      suap[3] = (block = suap[1]*100)/ll;
      if (block != suap[3]*ll) suap[3]++;
    }
    sprintf(dsuapm[4], "%llu%%", suap[3]);
  }
  dsuapm[5] = mt->dir;

  if (measuring) measure_columns(dsuapm);
  else print_columns(dsuapm);

  if (*dsuapm != mt->device) free(*dsuapm);
}

void df_main(void)
{
  struct mtab_list *mt, *mtstart, *mtend, *mt2, *mt3;
  int measuring;
  char **next;

  // Units are 512 bytes if you select "pedantic" without "kilobytes".
  if (FLAG(H)||FLAG(h)||FLAG(i)) TT.units = 1;
  else TT.units = FLAG(P) && !FLAG(k) ? 512 : 1024;

  if (!(mtstart = xgetmountlist(0))) return;
  mtend = dlist_terminate(mtstart);

  // If we have a list of filesystems on the command line, loop through them.
  if (*toys.optargs) {
    // Measure the names then output the table.
    for (measuring = 1;;) {
      for (next = toys.optargs; *next; next++) {
        struct stat st;

        // Stat it (complain if we can't).
        if (stat(*next, &st)) {
          if (!measuring) perror_msg("'%s'", *next);
        } else {
          // Find and display this filesystem.  Use _last_ hit in case of
          // overmounts (which is first hit in the reversed list).
          for (mt = mtend, mt2 = 0; mt; mt = mt->prev) {
            if (!mt2 && st.st_dev == mt->stat.st_dev) mt2 = mt;
            if (st.st_rdev && (st.st_rdev == mt->stat.st_dev)) break;
          }
          show_mt(mt ? : mt2, measuring);
        }
      }
      if (!measuring--) break;
      print_header();
    }
  } else {
    // Loop through mount list to filter out overmounts.
    for (mt = mtend; mt; mt = mt->prev) {
      for (mt3 = mt, mt2 = mt->prev; mt2; mt2 = mt2->prev) {
        if (mt->stat.st_dev == mt2->stat.st_dev) {
          // For --bind mounts, show earliest mount
          if (!strcmp(mt->device, mt2->device)) {
            mt3->stat.st_dev = 0;
            mt3 = mt2;
          } else mt2->stat.st_dev = 0;
        }
      }
    }

    // Measure the names then output the table (in filesystem creation order).
    for (measuring = 1;;) {
      for (mt = mtstart; mt; mt = mt->next) show_mt(mt, measuring);
      if (!measuring--) break;
      print_header();
    }
  }

  if (CFG_TOYBOX_FREE) llist_traverse(mtstart, free);
}
