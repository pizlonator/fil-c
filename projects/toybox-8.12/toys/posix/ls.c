/* ls.c - list files
 *
 * Copyright 2012 Andre Renaud <andre@bluewatersys.com>
 * Copyright 2012 Rob Landley <rob@landley.net>
 *
 * See http://opengroup.org/onlinepubs/9699919799/utilities/ls.html
 *
 * Deviations from posix:
 *   add -b (as default instead of -q: -b is unambiguous without collisions)
 *   add -Z -ll --color
 *   Posix says the -l date format should vary based on how recent it is
 *   and we do --time-style=long-iso instead
 * Deviations from gnu: -N switches off -q (no --show-control-chars)
 *   No --quoting-style=shell-escape, mostly because no short or long opt for it

USE_LS(NEWTOY(ls, "(sort):(color):;(full-time)(show-control-chars)\377(block-size)#=1024<1\241(group-directories-first)\376ZgoACFHLNRSUXabcdfhikl@mnpqrstuw#=80<0x1[-Cxm1][-Cxml][-Cxmo][-Cxmg][-cu][-ftS][-HL][-Nqb][-k\377]", TOYFLAG_BIN))

config LS
  bool "ls"
  default y
  help
    usage: ls [-1ACFHLNRSUXZabcdfghilmnopqrstuwx] [--color[=auto]] [FILE...]

    List files

    what to show:
    -A  all files except . and ..      -a  all files including .hidden
    -b  escape nongraphic chars        -d  directory, not contents
    -F  append /dir *exe @sym |FIFO    -f  files (no sort/filter/format)
    -H  follow command line symlinks   -i  inode number
    -L  follow symlinks                -N  no escaping, even on tty
    -p  put '/' after dir names        -q  unprintable chars as '?'
    -R  recursively list in subdirs    -s  storage used (units of --block-size)
    -Z  security context

    output formats:
    -1  list one file per line         -C  columns (sorted vertically)
    -g  like -l but no owner           -h  human readable sizes
    -k  reset --block-size to default  -l  long (show full details)
    -m  comma separated                -ll long with nanoseconds (--full-time)
    -n  long with numeric uid/gid      -o  long without group column
    -r  reverse order                  -w  set column width
    -x  columns (horizontal sort)

    sort by:  (also --sort=longname,longname... ends with alphabetical)
    -c  ctime      -r  reverse    -S  size     -t  time    -u  atime    -U  none
    -X  extension  -!  dirfirst   -~  nocase

    --block-size N	block size for -s (default 1024, -k resets to 1024)
    --color  =always (default)  =auto (when stdout is tty) =never
        exe=green  suid=red  suidfile=redback  stickydir=greenback
        device=yellow  symlink=turquoise/red  dir=blue  socket=purple

    Long output uses -cu for display, use -ltc/-ltu to also sort by ctime/atime.
*/

#define FOR_ls
#include "toys.h"

// test sst output (suid/sticky in ls flaglist)

// ls -lR starts .: then ./subdir:

GLOBALS(
  long w, l, block_size;
  char *color, *sort;

  struct dirtree *files, *singledir;
  unsigned screen_width;
  int nl_title;
  char *escmore;
)

// Callback from crunch_str to represent unprintable chars
static int crunch_qb(FILE *out, int cols, int wc)
{
  int len = 1;
  char buf[32];

  if (FLAG(q)) *buf = '?';
  else {
    if (wc<256) *buf = wc;
    // scrute the inscrutable, eff the ineffable, print the unprintable
    else if ((len = wcrtomb(buf, wc, 0) ) == -1) len = 1;
    if (FLAG(b)) {
      char *to = buf, *from = buf+24;
      int i, j;

      memcpy(from, to, 8);
      for (i = 0; i<len; i++) {
        *to++ = '\\';
        if (strchr(TT.escmore, from[i])) *to++ = from[i];
        else if (-1 != (j = stridx("\\\a\b\e\f\n\r\t\v", from[i])))
          *to++ = "\\abefnrtv"[j];
        else to += sprintf(to, "%03o", from[i]);
      }
      len = to-buf;
    }
  }

  if (cols<len) len = cols;
  if (out) fwrite(buf, len, 1, out);

  return len;
}

// Returns wcwidth(utf8) version of strlen with -qb escapes
static int strwidth(char *s)
{
  return crunch_str(&s, INT_MAX, 0, TT.escmore, crunch_qb);
}

static char endtype(struct stat *st)
{
  mode_t mode = st->st_mode;
  if ((FLAG(F)||FLAG(p)) && S_ISDIR(mode)) return '/';
  if (FLAG(F)) {
    if (S_ISLNK(mode)) return '@';
    if (S_ISREG(mode) && (mode&0111)) return '*';
    if (S_ISFIFO(mode)) return '|';
    if (S_ISSOCK(mode)) return '=';
  }
  return 0;
}

static int numlen(long long ll)
{
  return snprintf(0, 0, "%llu", ll);
}

static int print_with_h(char *s, long long value, int blocks)
{
  if (blocks) value = (value * 1024) / TT.block_size;
  if (FLAG(h)) return human_readable(s, value, 0);
  else return sprintf(s, "%lld", value);
}

// Figure out size of printable entry fields for display indent/wrap

static void entrylen(struct dirtree *dt, unsigned *len)
{
  struct stat *st = &(dt->st);
  char tmp[64];

  *len = strwidth(dt->name);
  if (endtype(st)) ++*len;
  if (FLAG(m)) ++*len;

  len[1] = FLAG(i) ? numlen(st->st_ino) : 0;
  if (FLAG(l)||FLAG(o)||FLAG(n)||FLAG(g)) {
    len[2] = numlen(st->st_nlink);
    len[3] = FLAG(n) ? numlen(st->st_uid) : strwidth(getusername(st->st_uid));
    len[4] = FLAG(n) ? numlen(st->st_gid) : strwidth(getgroupname(st->st_gid));
    if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
      // cheating slightly here: assuming minor is always 3 digits to avoid
      // tracking another column
      len[5] = numlen(dev_major(st->st_rdev))+5;
    } else len[5] = print_with_h(tmp, st->st_size, 0);
  }

  len[6] = FLAG(s) ? print_with_h(tmp, st->st_blocks, 1) : 0;
  len[7] = FLAG(Z) ? strwidth((char *)dt->extra) : 0;
}

// Perform one or more comparisons on a pair of files.
// Reused FLAG_a to mean "alphabetical"
static int do_compare(struct dirtree *a, struct dirtree *b, long long flags)
{
  struct timespec *ts1 = 0, *ts2;
  char *s1, *s2;
  int ret;

// TODO -? nocase  -! dirfirst

  if (flags&FLAG_S) {
    if (a->st.st_size > b->st.st_size) return -1;
    else if (a->st.st_size < b->st.st_size) return 1;
  }
  if (flags&FLAG_t) ts1 = &a->st.st_mtim, ts2 = &b->st.st_mtim;
  if (flags&FLAG_u) ts1 = &a->st.st_atim, ts2 = &b->st.st_atim;
  if (flags&FLAG_c) ts1 = &a->st.st_ctim, ts2 = &b->st.st_ctim;
  if (ts1) {
    // Newest is first by default, so return values are backwards
    if (ts1->tv_sec > ts2->tv_sec) return -1;
    else if (ts1->tv_sec < ts2->tv_sec) return 1;
    else if (ts1->tv_nsec > ts2->tv_nsec) return -1;
    else if (ts1->tv_nsec < ts2->tv_nsec) return 1;
  }
  if (flags&FLAG_X21) // dirfirst
    if (S_ISDIR(a->st.st_mode)!=S_ISDIR(b->st.st_mode))
      return S_ISDIR(a->st.st_mode) ? -1 : 1;

  // -X is a form of alphabetical sort, without -~ do case sensitive comparison
  s1 = 0;
  if (flags&FLAG_X) {
    s1 = strrchr(a->name, '.');
    s2 = strrchr(b->name, '.');
    if (s2 && !s1) return -1;
    if (s1 && !s2) return 1;
    if (!(flags&FLAG_X7E)) flags |= FLAG_a;
  }
  if (!s1) {
    s1 = a->name;
    s2 = b->name;
  }

  // case insensitive sort falls back to case sensitive sort when equal
  ret = (flags&FLAG_X7E) ? strcasecmp(s1, s2) : 0;
  if (!ret && (flags&FLAG_a)) ret = strcmp(s1, s2);

  return ret;
}

int comma_start(char **aa, char *b)
{
  return strstart(aa, b) && (!**aa || **aa==',');
}

// callback for qsort
static int compare(void *a, void *b)
{
  struct dirtree *dta = *(struct dirtree **)a;
  struct dirtree *dtb = *(struct dirtree **)b;
  char *ss = TT.sort;
  long long ll = 0;
  int ret = 0;
// TODO: test --sort=reverse with fallback alphabetical

  if (ss) for (;;) {
    while (*ss==',') ss++;
    if (!*ss) break;
    if (comma_start(&ss, "reverse")) toys.optflags |= FLAG_r;
    else if (comma_start(&ss, "none")) goto skip;
    else if (comma_start(&ss, "ctime")) ll = FLAG_c;
    else if (comma_start(&ss, "size")) ll = FLAG_S;
    else if (comma_start(&ss, "time")) ll = FLAG_t;
    else if (comma_start(&ss, "atime")) ll = FLAG_u;
    else if (comma_start(&ss, "nocase")) ll = FLAG_X7E;
    else if (comma_start(&ss, "extension")) ll = FLAG_X;
    else if (comma_start(&ss, "dirfirst")) ll = FLAG_X21;
    else error_exit("bad --sort %s", ss);

    if (!ret) ret = do_compare(dta, dtb, ll);
  }

  // Alpha fallback sort, and handle short opts
  if (!ret) {
    ll = toys.optflags|FLAG_a;
    // historical nonsense: -lc displays -c but doesn't sort, -ltc sorts.
    if (FLAG(o)|FLAG(l)) {
      if (!FLAG(t)) ll &= ~(FLAG_c|FLAG_u);
      else if (FLAG(c)||FLAG(u)) ll &= ~FLAG_t;
    }
    ret = do_compare(dta, dtb, ll);
  }
skip:
  if (FLAG(r)) ret *= -1;

  return ret;
}

// callback from dirtree_recurse() determining how to handle this entry.

static int filter(struct dirtree *new)
{
  // Special case to handle enormous dirs without running out of memory.
  if (toys.optflags == (FLAG_1|FLAG_f)) {
    xprintf("%s\n", new->name);
    return 0;
  }

  if (FLAG(Z)) {
    if (!CFG_TOYBOX_LSM_NONE) {
      // Linux doesn't support fgetxattr(2) on O_PATH file descriptors (though
      // bionic works around that), and there are no *xattrat(2) calls, so we
      // just use lgetxattr(2).
      char *path = dirtree_path(new, 0);

      (FLAG(L) ? lsm_get_context : lsm_lget_context)(path,(char **)&new->extra);
      free(path);
    }
    if (CFG_TOYBOX_LSM_NONE || !new->extra) new->extra = (long)xstrdup("?");
  }

  if (FLAG(u)) new->st.st_mtime = new->st.st_atime;
  if (FLAG(c)) new->st.st_mtime = new->st.st_ctime;
  new->st.st_blocks >>= 1; // Use 1KiB blocks rather than 512B blocks.

  if (FLAG(a)||FLAG(f)) return DIRTREE_SAVE;
  if (!FLAG(A) && *new->name=='.') return 0;

  return dirtree_notdotdot(new) & DIRTREE_SAVE;
}

// For column view, calculate horizontal position (for padding) and return
// index of next entry to display.

static unsigned long next_column(unsigned long ul, unsigned long dtlen,
  unsigned columns, unsigned *xpos)
{
  unsigned height, extra;

  // Horizontal sort is easy
  if (!FLAG(C)) {
    *xpos = ul % columns;
    return ul;
  }

  // vertical sort (-x), uneven rounding goes along right edge
  height = (dtlen+columns-1)/columns; // round up
  extra = dtlen%height; // how many rows are wider?
  if (extra && ul >= extra*columns) ul -= extra*columns--;
  else extra = 0;

  return (*xpos = ul % columns)*height + extra + ul/columns;
}

static int color_from_mode(mode_t mode)
{
  int color = 0;

  if (S_ISDIR(mode)) color = 256+34;
  else if (S_ISLNK(mode)) color = 256+36;
  else if (S_ISBLK(mode) || S_ISCHR(mode)) color = 256+33;
  else if (S_ISREG(mode) && (mode&0111)) color = 256+32;
  else if (S_ISFIFO(mode)) color = 33;
  else if (S_ISSOCK(mode)) color = 256+35;

  return color;
}

static void zprint(int zap, char *pat, int len, unsigned long arg)
{
  char tmp[32];

  sprintf(tmp, "%%*%s", zap ? "s" : pat);
  if (zap && pat[strlen(pat)-1]==' ') strcat(tmp, " ");
  printf(tmp, len, zap ? (unsigned long)"?" : arg);
}

// Display a list of dirtree entries, according to current format
// Output types -1, -l, -C, or stream

static void listfiles(int dirfd, struct dirtree *indir)
{
  struct dirtree *dt, **sort;
  unsigned long dtlen, ul = 0;
  unsigned width, totals[8], len[8], totpad = 0,
    *colsizes = (unsigned *)toybuf, columns = sizeof(toybuf)/4;
  char tmp[64];

  if (-1 == dirfd) {
    perror_msg_raw(indir->name);

    return;
  }

  memset(totals, 0, sizeof(totals));
  memset(len, 0, sizeof(len));

  // Top level directory was already populated by main()
  if (!indir->parent) {
    // Silently descend into single directory listed by itself on command line.
    // In this case only show dirname/total header when given -R.
    dt = indir->child;
    if (dt && S_ISDIR(dt->st.st_mode) && !dt->next && !(FLAG(d)||FLAG(R))) {
      listfiles(open(dt->name, 0), TT.singledir = dt);

      return;
    }

    // Do preprocessing (Dirtree didn't populate, so callback wasn't called.)
    for (;dt; dt = dt->next) filter(dt);
    if (toys.optflags == (FLAG_1|FLAG_f)) return;
  // Read directory contents. We dup() the fd because this will close it.
  // This reads/saves contents to display later, except for in "ls -1f" mode.
  } else dirtree_recurse(indir, filter, dirfd,
      DIRTREE_STATLESS|DIRTREE_SYMFOLLOW*FLAG(L));

  // Copy linked list to array and sort it. Directories go in array because
  // we visit them in sorted order too. (The nested loops let us measure and
  // fill with the same inner loop.)
  for (sort = 0;;sort = xmalloc(dtlen*sizeof(void *))) {
    for (dtlen = 0, dt = indir->child; dt; dt = dt->next, dtlen++)
      if (sort) sort[dtlen] = dt;
    if (sort || !dtlen) break;
  }

  // Label directory if not top of tree, or if -R
  if (indir->parent && (TT.singledir!=indir || FLAG(R))) {
    char *path = dirtree_path(indir, 0);

    if (TT.nl_title++) xputc('\n');
    xprintf("%s:\n", path);
    free(path);
  }

  // Measure each entry to work out whitespace padding and total blocks
  if (!FLAG(f)) {
    unsigned long long blocks = 0;

    if (!FLAG(U)) qsort(sort, dtlen, sizeof(void *), (void *)compare);
    for (ul = 0; ul<dtlen; ul++) {
      entrylen(sort[ul], len);
      for (width = 0; width<8; width++)
        if (len[width]>totals[width]) totals[width] = len[width];
      blocks += sort[ul]->st.st_blocks;
    }
    totpad = totals[1]+!!totals[1]+totals[6]+!!totals[6]+totals[7]+!!totals[7];
    if ((FLAG(h)||FLAG(l)||FLAG(o)||FLAG(n)||FLAG(g)||FLAG(s)) && indir->parent)
    {
      print_with_h(tmp, blocks, 1);
      xprintf("total %s\n", tmp);
    }
  }

  // Find largest entry in each field for display alignment
  if (FLAG(C)||FLAG(x)) {

    // columns can't be more than toybuf can hold, or more than files,
    // or > 1/2 screen width (one char filename, one space).
    if (columns > TT.screen_width/2) columns = TT.screen_width/2;
    if (columns > dtlen) columns = dtlen;

    // Try to fit as many columns as we can, dropping down by one each time
    for (;columns > 1; columns--) {
      unsigned c, cc, totlen = columns;

      memset(colsizes, 0, columns*sizeof(unsigned));
      for (ul = 0; ul<dtlen; ul++) {
        cc = next_column(ul, dtlen, columns, &c);
        if (cc>=dtlen) break; // tilt: remainder bigger than height
        entrylen(sort[cc], len);
        if (c<columns-1) *len += totpad+2;  // 2 spaces between filenames
        // Expand this column if necessary, break if that puts us over budget
        if (*len > colsizes[c]) {
          totlen += (*len)-colsizes[c];
          colsizes[c] = *len;
          if (totlen > TT.screen_width) break;
        }
      }
      // If everything fit, stop here
      if (ul == dtlen) break;
    }
  }

  // Loop through again to produce output.
  width = 0;
  for (ul = 0; ul<dtlen; ul++) {
    int ii, zap;
    unsigned curcol, lastlen = *len, color = 0;
    struct stat *st = &((dt = sort[next_column(ul,dtlen,columns,&curcol)])->st);
    mode_t mode = st->st_mode;
    char et = endtype(st), *ss;

    // If we couldn't stat, output ? for most fields
    zap = !st->st_blksize && !st->st_dev && !st->st_ino;

    // Skip directories at the top of the tree when -d isn't set
    if (S_ISDIR(mode) && !indir->parent && !FLAG(d)) continue;
    TT.nl_title=1;

    // Handle padding and wrapping for display purposes
    entrylen(dt, len);
    if (ul) {
      if (FLAG(m)) xputc(',');
      if (FLAG(C)||FLAG(x)) {
        if (!curcol) xputc('\n');
        else {
          if (ul) next_column(ul-1, dtlen, columns, &curcol);
          printf("%*c", colsizes[ul ? curcol : 0]-lastlen-totpad, ' ');
        }
      } else if (FLAG(1) || width+1+*len > TT.screen_width) {
        xputc('\n');
        width = 0;
      } else {
        xputsn("  "+FLAG(m));
        width += 2-FLAG(m);
      }
    }
    width += *len;

    if (FLAG(i)) zprint(zap, "lu ", totals[1], st->st_ino);

    if (FLAG(s)) {
      print_with_h(tmp, st->st_blocks, 1);
      zprint(zap, "s ", totals[6], (unsigned long)tmp);
    }

    if (FLAG(l)||FLAG(o)||FLAG(n)||FLAG(g)) {
      mode_to_string(mode, tmp);
      if (zap) memset(tmp+1, '?', 9);
      printf("%s", tmp);
      zprint(zap, "ld", totals[2]+1, st->st_nlink);

      // print user
      if (!FLAG(g)) {
        putchar(' ');
        ii = -totals[3];
        if (zap || FLAG(n)) zprint(zap, "lu", ii, st->st_uid);
        else draw_trim_esc(getusername(st->st_uid), ii, abs(ii), TT.escmore,
                           crunch_qb);
      }

      // print group
      if (!FLAG(o)) {
        putchar(' ');
        ii = -totals[4];
        if (zap || FLAG(n)) zprint(zap, "lu", ii, st->st_gid);
        else draw_trim_esc(getgroupname(st->st_gid), ii, abs(ii), TT.escmore,
                           crunch_qb);
      }
    }
    if (FLAG(Z)) printf(" %-*s "+!FLAG(l), -(int)totals[7], (char *)dt->extra);

    if (FLAG(l)||FLAG(o)||FLAG(n)||FLAG(g)) {
      struct tm *tm;

      // print major/minor, or size
      if (!zap && (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)))
        printf("% *d,% 4d", totals[5]-4, dev_major(st->st_rdev),
          dev_minor(st->st_rdev));
      else {
        print_with_h(tmp, st->st_size, 0);
        zprint(zap, "s", totals[5]+1, (unsigned long)tmp);
      }

      // print time, always in --time-style=long-iso
      tm = localtime(&(st->st_mtime));
      strftime(tmp, sizeof(tmp), " %F %H:%M", tm);
      if (TT.l>1) {
        char *s = tmp+strlen(tmp);

        s += sprintf(s, ":%02d.%09d ", tm->tm_sec, (int)st->st_mtim.tv_nsec);
        strftime(s, sizeof(tmp)-(s-tmp), "%z", tm);
      }
      zprint(zap, "s ", 17+(TT.l>1)*13, (unsigned long)tmp);
    }

    if (FLAG(color)) {
      color = color_from_mode(st->st_mode);
      if (color) printf("\e[%d;%dm", color>>8, color&255);
    }

    ss = dt->name;
    crunch_str(&ss, INT_MAX, stdout, TT.escmore, crunch_qb);
    if (color) printf("\e[0m");

    if ((FLAG(l)||FLAG(o)||FLAG(n)||FLAG(g)) && S_ISLNK(mode)) {
      printf(" -> ");
      if (!zap && FLAG(color)) {
        struct stat st2;

        if (fstatat(dirfd, dt->symlink, &st2, 0)) color = 256+31;
        else color = color_from_mode(st2.st_mode);

        if (color) printf("\e[%d;%dm", color>>8, color&255);
      }

      zprint(zap, "s", 0, (unsigned long)dt->symlink);
      if (!zap && color) printf("\e[0m");
    }

    if (et) xputc(et);
  }

  if (width) xputc('\n');

  // Free directory entries, recursing first if necessary.

  for (ul = 0; ul<dtlen; free(sort[ul++])) {
    if (FLAG(d) || !S_ISDIR(sort[ul]->st.st_mode)) continue;

    // Recurse into dirs if at top of the tree or given -R
    if (!indir->parent || (FLAG(R) && dirtree_notdotdot(sort[ul])))
      listfiles(openat(dirfd, sort[ul]->name, 0), sort[ul]);
    free((void *)sort[ul]->extra);
  }
  free(sort);
  if (dirfd != AT_FDCWD) close(dirfd);
}

void ls_main(void)
{
  char **s, *noargs[] = {".", 0};
  struct dirtree *dt;

  if (FLAG(full_time)) {
    toys.optflags |= FLAG_l;
    TT.l = 2;
  }

  // Do we have an implied -1
  if (isatty(1)) {
    if (!FLAG(show_control_chars)) toys.optflags |= FLAG_b;
    if (FLAG(l)||FLAG(o)||FLAG(n)||FLAG(g)) toys.optflags |= FLAG_1;
    else if (!(FLAG(1)||FLAG(x)||FLAG(m))) toys.optflags |= FLAG_C;
  } else {
    if (!FLAG(m)) toys.optflags |= FLAG_1;
    if (TT.color) toys.optflags ^= FLAG_color;
  }

  // -N *doesn't* disable -q; you need --show-control-chars for that.
  if (FLAG(N)) toys.optflags &= ~FLAG_b;

  TT.screen_width = 80;
  if (FLAG(w)) TT.screen_width = TT.w+2;
  else terminal_size(&TT.screen_width, NULL);
  if (TT.screen_width<2) TT.screen_width = 2;
  if (FLAG(b)) TT.escmore = " \\";

  // The optflags parsing infrastructure should really do this for us,
  // but currently it has "switch off when this is set", so "-dR" and "-Rd"
  // behave differently
  if (FLAG(d)) toys.optflags &= ~FLAG_R;

  // Iterate through command line arguments, collecting directories and files.
  // Non-absolute paths are relative to current directory. Top of tree is
  // a dummy node to collect command line arguments into pseudo-directory.
  TT.files = dirtree_add_node(0, 0, 0);
  TT.files->dirfd = AT_FDCWD;
  for (s = *toys.optargs ? toys.optargs : noargs; *s; s++) {
    int sym = !(FLAG(l)||FLAG(d)||FLAG(F)) || FLAG(L) || FLAG(H);

    dt = dirtree_add_node(0, *s, DIRTREE_STATLESS|DIRTREE_SYMFOLLOW*sym);

    // note: double_list->prev temporarily goes in dirtree->parent
    if (dt) {
      if (dt->again&DIRTREE_STATLESS) {
        perror_msg_raw(*s);
        free(dt);
      } else dlist_add_nomalloc((void *)&TT.files->child, (void *)dt);
    } else toys.exitval = 1;
  }

  // Convert double_list into dirtree.
  dlist_terminate(TT.files->child);
  for (dt = TT.files->child; dt; dt = dt->next) dt->parent = TT.files;

  // Display the files we collected
  listfiles(AT_FDCWD, TT.files);

  if (CFG_TOYBOX_FREE) free(TT.files);
}
