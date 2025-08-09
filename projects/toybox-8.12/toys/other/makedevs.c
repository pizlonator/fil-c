/* makedevs.c - Make ranges of device files.
 *
 * Copyright 2014 Bilal Qureshi <bilal.jmi@gmail.com>
 * Copyright 2014 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard

USE_MAKEDEVS(NEWTOY(makedevs, "<1>1d:", TOYFLAG_USR|TOYFLAG_BIN))

config MAKEDEVS
  bool "makedevs"
  default y
  help
    usage: makedevs [-d device_table] rootdir

    Create a range of special files as specified in a device table.

    -d	File containing device table (default reads from stdin)

    Each line of the device table has the fields:
    <name> <type> <mode> <uid> <gid> <major> <minor> <start> <increment> <count>
    Where name is the file name, and type is one of the following:

    b	Block device
    c	Character device
    d	Directory
    f	Regular file
    p	Named pipe (fifo)

    Other fields specify permissions, user and group id owning the file,
    and additional fields for device special files. Use '-' for blank entries,
    unspecified fields are treated as '-'.
*/

#define FOR_makedevs
#include "toys.h"

GLOBALS(
  char *d;
)

void makedevs_main(void)
{
  FILE *fp = stdin;
  char *line = NULL;
  size_t allocated_length = 0;
  int line_no = 0, i;

  // Open file and chdir, verbosely
  xprintf("rootdir = %s\n", *toys.optargs);
  if (FLAG(d) && strcmp(TT.d, "-")) {
    fp = xfopen(TT.d, "r");
    xprintf("table = %s\n", TT.d);
  } else xprintf("table = <stdin>\n");
  xchdir(*toys.optargs);

  while (getline(&line, &allocated_length, fp) > 0) {
    char type=0, user[64], group[64], *node, *ptr = line;
    unsigned int mode = 0755, major = 0, minor = 0, cnt = 0, incr = 0,
                 st_val = 0;
    uid_t uid;
    gid_t gid;
    struct stat st;

    line_no++;
    while (isspace(*ptr)) ptr++;
    if (!*ptr || *ptr == '#') continue;
    node = ptr;

    while (*ptr && !isspace(*ptr)) ptr++;
    if (*ptr) *(ptr++) = 0;
    *user = *group = 0;
    sscanf(ptr, "%c %o %63s %63s %u %u %u %u %u", &type, &mode,
           user, group, &major, &minor, &st_val, &incr, &cnt);

    // type order here needs to line up with actions[] order.
    i = stridx("pcbdf", type);
    if (i == -1) {
      error_msg("line %d: bad type %c", line_no, type);
      continue;
    } else mode |= (mode_t[]){S_IFIFO, S_IFCHR, S_IFBLK, 0, 0}[i];

    uid = *user ? xgetuid(user) : getuid();
    gid = *group ? xgetgid(group) : getgid();

    while (*node == '/') node++; // using relative path

    for (i = 0; (!cnt && !i) || i < cnt; i++) {
      if (cnt>1) {
        snprintf(toybuf, sizeof(toybuf), "%.999s%u", node, st_val + i);
        ptr = toybuf;
      } else ptr = node;

      if (type == 'd') {
        if (mkpathat(AT_FDCWD, ptr, mode, MKPATHAT_MKLAST | MKPATHAT_MAKE))  {
          perror_msg("can't create directory '%s'", ptr);
          continue;
        }
      } else if (type == 'f') {
        if (stat(ptr, &st) || !S_ISREG(st.st_mode)) {
          perror_msg("line %d: file '%s' does not exist", line_no, ptr);
          continue;
        }
      } else if (mknod(ptr, mode, dev_makedev(major, minor + i*incr))) {
        perror_msg("line %d: can't create node '%s'", line_no, ptr);
        continue;
      }

      if (chown(ptr, uid, gid) || chmod(ptr, mode))
        perror_msg("line %d: can't chown/chmod '%s'", line_no, ptr);
    }
  }
  free(line);
  if (fp != stdin) fclose(fp);
}
