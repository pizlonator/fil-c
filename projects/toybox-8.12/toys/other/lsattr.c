/* lsattr.c - List file attributes on a Linux second extended file system.
 *
 * Copyright 2013 Ranjan Kumar <ranjankumar.bth@gmail.com>
 * Copyright 2013 Kyungwan Han <asura321@gmail.com>
 *
 * No Standard.
 *
 * TODO cleanup

USE_LSATTR(NEWTOY(lsattr, "ldapvR", TOYFLAG_BIN))
USE_CHATTR(NEWTOY(chattr, "?p#v#R", TOYFLAG_BIN))

config LSATTR
  bool "lsattr"
  default y
  help
    usage: lsattr [-Radlpv] [FILE...]

    List file attributes on a Linux file system.
    Flag letters are defined in chattr help.

    -R	Recursively list attributes of directories and their contents
    -a	List all files in directories, including files that start with '.'
    -d	List directories like other files, rather than listing their contents
    -l	List long flag names
    -p	List the file's project number
    -v	List the file's version/generation number

config CHATTR
  bool "chattr"
  default y
  help
    usage: chattr [-R] [-+=AacDdijsStTu] [-p PROJID] [-v VERSION] [FILE...]

    Change file attributes on a Linux file system.

    -R	Recurse
    -p	Set the file's project number
    -v	Set the file's version/generation number

    Operators:
      '-' Remove attributes
      '+' Add attributes
      '=' Set attributes

    Attributes:
      A  No atime                     a  Append only
      C  No COW                       c  Compression
      D  Synchronous dir updates      d  No dump
      E  Encrypted                    e  Extents
      F  Case-insensitive (casefold)
      I  Indexed directory            i  Immutable
      j  Journal data
      N  Inline data in inode
      P  Project hierarchy
      S  Synchronous file updates     s  Secure delete
      T  Top of dir hierarchy         t  No tail-merging
      u  Allow undelete
      V  Verity
*/
#define FOR_lsattr
#include "toys.h"
#include <linux/fs.h>

GLOBALS(
  long v, p;

  unsigned add, rm, set;
  // !add and !rm tell us whether they were used, but `chattr =` is meaningful.
  int have_set;
)

// Added more recently than the 7 year support horizon. TODO: remove
#ifndef FS_CASEFOLD_FL
#define FS_CASEFOLD_FL    0x40000000 // commit 71e90b4654a92 2019-07-23
#endif
#ifndef FS_VERITY_FL
#define FS_VERITY_FL      0x00100000 // commit fe9918d3b228b 2019-07-22
#endif

static struct ext2_attr {
  char *name;
  unsigned flag;
  char opt;
} e2attrs[] = {
  // Do not sort! These are in the order that lsattr outputs them.
  {"Secure_Deletion",               FS_SECRM_FL,        's'},
  {"Undelete",                      FS_UNRM_FL,         'u'},
  {"Synchronous_Updates",           FS_SYNC_FL,         'S'},
  {"Synchronous_Directory_Updates", FS_DIRSYNC_FL,      'D'},
  {"Immutable",                     FS_IMMUTABLE_FL,    'i'},
  {"Append_Only",                   FS_APPEND_FL,       'a'},
  {"No_Dump",                       FS_NODUMP_FL,       'd'},
  {"No_Atime",                      FS_NOATIME_FL,      'A'},
  {"Compression_Requested",         FS_COMPR_FL,        'c'},
  {"Encrypted",                     FS_ENCRYPT_FL,      'E'},
  {"Journaled_Data",                FS_JOURNAL_DATA_FL, 'j'},
  {"Indexed_directory",             FS_INDEX_FL,        'I'},
  {"No_Tailmerging",                FS_NOTAIL_FL,       't'},
  {"Top_of_Directory_Hierarchies",  FS_TOPDIR_FL,       'T'},
  {"Extents",                       FS_EXTENT_FL,       'e'},
  {"No_COW",                        FS_NOCOW_FL,        'C'},
  {"Casefold",                      FS_CASEFOLD_FL,     'F'},
  {"Inline_Data",                   FS_INLINE_DATA_FL,  'N'},
  {"Project_Hierarchy",             FS_PROJINHERIT_FL,  'P'},
  {"Verity",                        FS_VERITY_FL,       'V'},
  {NULL,                            0,                  0},
};

// Get file flags on a Linux second extended file system.
static int ext2_getflag(int fd, struct stat *sb, unsigned *flag)
{
  if(!S_ISREG(sb->st_mode) && !S_ISDIR(sb->st_mode)) {
    errno = EOPNOTSUPP;
    return -1;
  }
  return ioctl(fd, FS_IOC_GETFLAGS, flag);
}

static char *attrstr(unsigned attrs, int full)
{
  struct ext2_attr *a = e2attrs;
  char *s = toybuf;

  for (; a->name; a++)
    if (attrs & a->flag) *s++ = a->opt;
    else if (full) *s++ = '-';
  *s = 0;

  return toybuf;
}

static void print_file_attr(char *path)
{
  unsigned flag = 0, version = 0;
  int fd = -1;
  struct stat sb;

  if (!stat(path, &sb) && !S_ISREG(sb.st_mode) && !S_ISDIR(sb.st_mode)) {
    errno = EOPNOTSUPP;
    goto error;
  }
  if (-1 == (fd=open(path, O_RDONLY | O_NONBLOCK))) goto error;

  if (FLAG(p)) {
    struct fsxattr fsx;

    if (ioctl(fd, FS_IOC_FSGETXATTR, &fsx)) goto error;
    xprintf("%5u ", fsx.fsx_projid);
  }
  if (FLAG(v)) {
    if (ioctl(fd, FS_IOC_GETVERSION, (void*)&version) < 0) goto error;
    xprintf("%-10u ", version);
  }

  if (ext2_getflag(fd, &sb, &flag) < 0) perror_msg("reading flags '%s'", path);
  else {
    struct ext2_attr *ptr = e2attrs;
    int name_found = 0;

    if (FLAG(l)) {
      xprintf("%-50s ", path);
      for (; ptr->name; ptr++) {
        if (flag & ptr->flag) {
          if (name_found) xprintf(", "); //for formatting.
          xprintf("%s", ptr->name);
          name_found = 1;
        }
      }
      if (!name_found) xprintf("---");
      xputc('\n');
    } else xprintf("%s %s\n", attrstr(flag, 1), path);
  }
  path = 0;
error:
  xclose(fd);
  if (path) perror_msg("reading '%s'", path);
}

// Get directory information.
static int retell_dir(struct dirtree *root)
{
  char *fpath = 0;

  if (root->again) {
    xputc('\n');

    return 0;
  }
  if (S_ISDIR(root->st.st_mode) && !root->parent)
    return DIRTREE_RECURSE|DIRTREE_COMEAGAIN;

  fpath = dirtree_path(root, NULL);
  if (*root->name != '.' || FLAG(a)) {
    print_file_attr(fpath);
    if (S_ISDIR(root->st.st_mode) && FLAG(R) && dirtree_notdotdot(root)) {
      xprintf("\n%s:\n", fpath);
      free(fpath);
      return DIRTREE_RECURSE|DIRTREE_COMEAGAIN;
    }
  }
  free(fpath);

  return 0;
}

void lsattr_main(void)
{
  if (!*toys.optargs) dirtree_read(".", retell_dir);
  else for (; *toys.optargs; toys.optargs++) {
    struct stat sb;

    if (lstat(*toys.optargs, &sb)) perror_msg("stat '%s'", *toys.optargs);
    else if (S_ISDIR(sb.st_mode) && !FLAG(d))
      dirtree_read(*toys.optargs, retell_dir);
    else print_file_attr(*toys.optargs);// to handle "./Filename" or "./Dir"
  }
}

// Switch gears from lsattr to chattr.
#define FOR_chattr
#include "generated/flags.h"

// Set file flags on a Linux second extended file system.
static inline int ext2_setflag(int fd, struct stat *sb, unsigned flag)
{
  if (!S_ISREG(sb->st_mode) && !S_ISDIR(sb->st_mode)) {
    errno = EOPNOTSUPP;
    return -1;
  }
  return ioctl(fd, FS_IOC_SETFLAGS, &flag);
}

static unsigned get_flag_val(char ch)
{
  struct ext2_attr *ptr = e2attrs;

  for (; ptr->name; ptr++) if (ptr->opt == ch) return ptr->flag;
  help_exit("bad '%c'", ch);
}

// Parse command line argument and fill the chattr structure.
static void parse_cmdline_arg(char ***argv)
{
  char *arg = **argv, *ptr;

  while (arg) {
    if (*arg=='-') for (ptr = ++arg; *ptr; ptr++) TT.rm |= get_flag_val(*ptr);
    else if (*arg=='+')
      for (ptr = ++arg; *ptr; ptr++) TT.add |= get_flag_val(*ptr);
    else if (*arg=='=') {
      TT.have_set = 1;
      for (ptr = ++arg; *ptr; ptr++) TT.set |= get_flag_val(*ptr);
    } else return;
    arg = *(*argv += 1);
  }
}

// Update attribute of given file.
static int update_attr(struct dirtree *root)
{
  char *fpath = 0;
  int vv = TT.v, fd;

  if (!dirtree_notdotdot(root)) return 0;

  // if file is a link and recursive is set or file is not regular+link+dir
  // (like fifo or dev file) then escape the file.
  if ((S_ISLNK(root->st.st_mode) && FLAG(R))
    || (!S_ISREG(root->st.st_mode) && !S_ISLNK(root->st.st_mode)
      && !S_ISDIR(root->st.st_mode)))
    return 0;

  fpath = dirtree_path(root, NULL);
  if (-1 == (fd=open(fpath, O_RDONLY | O_NONBLOCK))) {
    free(fpath);
    return DIRTREE_ABORT;
  }

  // Any potential flag changes?
  if (TT.have_set | TT.add | TT.rm) {
    unsigned orig, new;

    // Read current flags.
    if (ext2_getflag(fd, &(root->st), &orig) < 0) {
      perror_msg("read flags of '%s'", fpath);
      free(fpath);
      xclose(fd);
      return DIRTREE_ABORT;
    }
    // Apply the requested changes.
    if (TT.have_set) new = TT.set; // '='.
    else { // '-' and/or '+'.
      new = orig;
      new &= ~(TT.rm);
      new |= TT.add;
      if (!S_ISDIR(root->st.st_mode)) new &= ~FS_DIRSYNC_FL;
    }
    // Write them back if there was any change.
    if (orig != new && ext2_setflag(fd, &(root->st), new)<0)
      perror_msg("%s: setting flags to =%s failed", fpath, attrstr(new, 0));
  }

  // (FS_IOC_SETVERSION works all the way back to 2.6, but FS_IOC_FSSETXATTR
  // isn't available until 4.5.)
  if (FLAG(v) && (ioctl(fd, FS_IOC_SETVERSION, &vv)<0))
    perror_msg("%s: setting version to %d failed", fpath, vv);

  if (FLAG(p)) {
    struct fsxattr fsx;
    int fail = ioctl(fd, FS_IOC_FSGETXATTR, &fsx);

    fsx.fsx_projid = TT.p;
    if (fail || ioctl(fd, FS_IOC_FSSETXATTR, &fsx))
      perror_msg("%s: setting projid to %u failed", fpath, fsx.fsx_projid);
  }

  free(fpath);
  xclose(fd);
  return (FLAG(R) && S_ISDIR(root->st.st_mode)) ? DIRTREE_RECURSE : 0;
}

void chattr_main(void)
{
  char **argv = toys.optargs;

  parse_cmdline_arg(&argv);
  if (TT.p < 0 || TT.p > UINT_MAX) error_exit("bad projid %lu", TT.p);
  if (TT.v < 0 || TT.v > UINT_MAX) error_exit("bad version %ld", TT.v);
  if (!*argv) help_exit("no file");
  if (TT.have_set && (TT.add || TT.rm))
    error_exit("no '=' with '-' or '+'");
  if (TT.rm & TT.add) error_exit("set/unset same flag");
  if (!(TT.add || TT.rm || TT.have_set || FLAG(p) || FLAG(v)))
    error_exit("need '-p', '-v', '=', '-', or '+'");
  for (; *argv; argv++) dirtree_read(*argv, update_attr);
}
