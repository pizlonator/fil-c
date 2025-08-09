/* modinfo.c - Display module info
 *
 * Copyright 2012 Andre Renaud <andre@bluewatersys.com>
 *
 * TODO: cleanup

USE_MODINFO(NEWTOY(modinfo, "<1b:k:F:0", TOYFLAG_SBIN))

config MODINFO
  bool "modinfo"
  default y
  help
    usage: modinfo [-0] [-b basedir] [-k kernel] [-F field] [module|file...]

    Display module fields for modules specified by name or .ko path.

    -F  Only show the given field
    -0  Separate fields with NUL rather than newline
    -b  Use <basedir> as root for /lib/modules/
    -k  Look in given directory under /lib/modules/
*/

#define FOR_modinfo
#include "toys.h"

GLOBALS(
  char *F, *k, *b;

  long mod;
  int count;
)

static void output_field(char *field, char *value)
{
  if (!TT.F) xprintf("%s:%*c", field, 15-(int)strlen(field), ' ');
  else if (strcmp(TT.F, field)) return;
  xprintf("%s", value);
  xputc(FLAG(0) ? 0 : '\n');
}

static void modinfo_file(char *full_name)
{
  int fd, flen, i;
  char *buf = 0, *end, *modinfo_tags[] = {
    "license", "author", "description", "firmware", "alias", "srcversion",
    "depends", "retpoline", "intree", "name", "vermagic", "parm", "parmtype",
    "scmversion",
  };

  if (-1 != (fd = open(full_name, O_RDONLY))) {
    flen = fdlength(fd);
    buf = xmmap(0, flen, PROT_READ, MAP_SHARED, fd, 0);
    end = buf + flen;
    close(fd);
  }

  if (!buf) {
    perror_msg_raw(full_name);
    return;
  }

  TT.count++;
  output_field("filename", full_name);

  for (i=0; i<ARRAY_LEN(modinfo_tags); i++) {
    char *field = modinfo_tags[i], *p = buf;
    int slen = sprintf(toybuf, "%s=", field);

    while (p && p < end) {
      p = memmem(p, end-p, toybuf, slen);
      if (p) output_field(field, p += slen);
    }
  }

  munmap(buf, flen);
}

static int check_module(struct dirtree *new)
{
  char *s;
  int len;

  if (!dirtree_notdotdot(new)) return 0;

  if (!S_ISREG(new->st.st_mode)) return DIRTREE_RECURSE;

  s = toys.optargs[TT.mod];

  // The kernel treats - and _ the same, so we should too.
  for (len = 0; s[len]; len++) {
    if (s[len] == '-' && new->name[len] == '_') continue;
    if (s[len] == '_' && new->name[len] == '-') continue;
    if (s[len] != new->name[len]) break;
  }
  if (s[len] || strcmp(new->name+len, ".ko")) return DIRTREE_RECURSE;

  modinfo_file(s = dirtree_path(new, 0));
  free(s);

  return DIRTREE_ABORT;
}

void modinfo_main(void)
{
  struct utsname uts;

  // Android (as shipped by Google) currently only has modules on /vendor.
  // Android does not support multiple sets of modules for different kernels.
  if (CFG_TOYBOX_ON_ANDROID) {
   if (!TT.b) TT.b = "/vendor";
   if (!TT.k) TT.k = "";
  } else {
   uname(&uts);
   if (!TT.b) TT.b = "";
   if (!TT.k) TT.k = uts.release;
  }

  for (TT.mod = 0; TT.mod<toys.optc; TT.mod++) {
    if (strend(toys.optargs[TT.mod], ".ko")) modinfo_file(toys.optargs[TT.mod]);
    else {
      char *path = xmprintf("%s/lib/modules/%s", TT.b, TT.k);

      TT.count = 0;
      dirtree_read(path, check_module);
      if (!TT.count) error_msg("%s: not found", toys.optargs[TT.mod]);
      free(path);
    }
  }
}
