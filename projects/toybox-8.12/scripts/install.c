/* Wrapper to make installation easier with cross-compiling.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "generated/config.h"
#include "lib/toyflags.h"

#define NEWTOY(name, opts, flags) {#name, flags},
#define OLDTOY(name, oldname, flags) {#name, flags},

// Populate toy_list[].

struct {char *name; int flags;} toy_list[] = {
#include "generated/newtoys.h"
};

#undef NEWTOY
#undef OLDTOY
#define NEWTOY(name,opt,flags) HELP_##name "\0"
#if CFG_TOYBOX
#define OLDTOY(name,oldname,flags) "\xff" #oldname "\0"
#else
#define OLDTOY(name, oldname, flags) HELP_##oldname "\0"
#endif

#include "generated/help.h"
static char help_data[] =
#include "generated/newtoys.h"
;

int main(int argc, char *argv[])
{
  static char *toy_paths[]={"usr/","bin/","sbin/",0};
  int i, len = 0;

  if (argc>1 && !strcmp(argv[1], "--help"))
    exit(sizeof(help_data)!=write(1, help_data, sizeof(help_data)));

  // Output list of applets.
  for (i=1; i<sizeof(toy_list)/sizeof(*toy_list); i++) {
    int fl = toy_list[i].flags;
    if (fl & TOYMASK_LOCATION) {
      if (argc>1) {
        int j;
        for (j=0; toy_paths[j]; j++)
          if (fl & (1<<j)) len += printf("%s", toy_paths[j]);
      }
      len += printf("%s\n",toy_list[i].name);
    }
  }
  return 0;
}
