/* tar.c - create/extract archives
 *
 * Copyright 2014 Ashwini Kumar <ak.ashwini81@gmail.com>
 *
 * For the command, see
 *   http://pubs.opengroup.org/onlinepubs/007908799/xcu/tar.html
 * For the modern file format, see
 *   http://pubs.opengroup.org/onlinepubs/9699919799/utilities/pax.html#tag_20_92_13_06
 *   https://en.wikipedia.org/wiki/Tar_(computing)#File_format
 *   https://www.gnu.org/software/tar/manual/html_node/Tar-Internals.html
 *
 * For writing to external program
 * http://www.gnu.org/software/tar/manual/html_node/Writing-to-an-External-Program.html
 *
 * Toybox will never implement the "pax" command as a matter of policy.
 *
 * TODO: --wildcard state changes aren't positional.
 * We always --verbatim-files-from
 * Why --exclude pattern but no --include? tar cvzf a.tgz dir --include '*.txt'
 * No --no-null because the args infrastructure isn't ready.
 * Until args.c learns about no- toggles, --no-thingy always wins over --thingy

USE_TAR(NEWTOY(tar, "&(one-file-system)(no-ignore-case)(ignore-case)(no-anchored)(anchored)(no-wildcards)(wildcards)(no-wildcards-match-slash)(wildcards-match-slash)(show-transformed-names)(selinux)(restrict)(full-time)(no-recursion)(null)(numeric-owner)(no-same-permissions)(overwrite)(exclude)*(sort);:(mode):(mtime):(group):(owner):(to-command):~(strip-components)(strip)#~(transform)(xform)*Z(zstd)o(no-same-owner)p(same-permissions)k(keep-old)c(create)|h(dereference)x(extract)|t(list)|v(verbose)J(xz)j(bzip2)z(gzip)S(sparse)O(to-stdout)P(absolute-names)m(touch)X(exclude-from)*T(files-from)*I(use-compress-program):C(directory):f(file):as[!txc][!jzJa]", TOYFLAG_USR|TOYFLAG_BIN|TOYFLAG_UMASK))

config TAR
  bool "tar"
  default y
  help
    usage: tar [-cxt] [-fvohmjkOS] [-XTCf NAME] [--selinux] [FILE...]

    Create, extract, or list files in a .tar (or compressed t?z) file.

    Options:
    c  Create                x  Extract               t  Test (list)
    f  tar FILE (default -)  C  Change to DIR first   v  Verbose display
    J  xz compression        j  bzip2 compression     z  gzip compression
    o  Ignore owner          h  Follow symlinks       m  Ignore mtime
    O  Extract to stdout     X  exclude names in FILE T  include names in FILE
    s  Sort dirs (--sort)    Z  zstd compression

    --exclude        FILENAME to exclude  --full-time         Show seconds with -tv
    --mode MODE      Adjust permissions   --owner NAME[:UID]  Set file ownership
    --mtime TIME     Override timestamps  --group NAME[:GID]  Set file group
    --sparse         Record sparse files  --selinux           Save/restore labels
    --restrict       All under one dir    --no-recursion      Skip dir contents
    --numeric-owner  Use numeric uid/gid, not user/group names
    --null           Filenames in -T FILE are null-separated, not newline
    --strip-components NUM  Ignore first NUM directory components when extracting
    --xform=SED      Modify filenames via SED expression (ala s/find/replace/g)
    -I PROG          Filter through PROG to compress or PROG -d to decompress

    Filename filter types. Create command line args aren't filtered, extract
    defaults to --anchored, --exclude defaults to --wildcards-match-slash,
    use no- prefix to disable:

    --anchored  Match name not path       --ignore-case       Case insensitive
    --wildcards Expand *?[] like shell    --wildcards-match-slash
*/

#define FOR_tar
#include "toys.h"

GLOBALS(
  char *f, *C, *I;
  struct arg_list *T, *X, *xform;
  long strip;
  char *to_command, *owner, *group, *mtime, *mode, *sort;
  struct arg_list *exclude;

  struct double_list *incl, *excl, *seen;
  struct string_list *dirs;
  char *cwd, **xfsed;
  int fd, ouid, ggid, hlc, warn, sparselen, pid, xfpipe[2];
  struct dev_ino archive_di;
  long long *sparse;
  time_t mtt;

  // hardlinks seen so far (hlc many)
  struct {
    char *arg;
    struct dev_ino di;
  } *hlx;

  // Parsed information about a tar header.
  struct tar_header {
    char *name, *link_target, *uname, *gname;
    long long size, ssize, oldsparse;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    time_t mtime;
    dev_t device;
  } hdr;
)

// The on-disk 512 byte record structure.
struct tar_hdr {
  char name[100], mode[8], uid[8], gid[8], size[12], mtime[12], chksum[8],
       type, link[100], magic[8], uname[32], gname[32], major[8], minor[8],
       prefix[155], padd[12];
};

// Tar uses ASCII octal when it fits, base-256 otherwise.
static int ascii_fits(unsigned long long val, int len)
{
  return !(val>>(3*(len-1)));
}

// convert from int to octal (or base-256)
static void itoo(char *str, int len, unsigned long long val)
{
  if (ascii_fits(val, len)) sprintf(str, "%0*llo", len-1, val);
  else {
    for (str += len; len--; val >>= 8) *--str = val;
    *str = 128;
  }
}
#define ITOO(x, y) itoo(x, sizeof(x), y)

// convert octal (or base-256) to int
static unsigned long long otoi(char *str, unsigned len)
{
  unsigned long long val = 0;

  // When tar value too big or octal, use binary encoding with high bit set
  if (128&*str) while (--len) {
    if (val<<8 < val) error_exit("bad header");
    val = (val<<8)+*++str;
  } else {
    while (len && *str == ' ') str++;
    while (len && *str>='0' && *str<='7') val = val*8+*str++-'0', len--;
    if (len && *str && *str != ' ') error_exit("bad header");
  }

  return val;
}
#define OTOI(x) otoi(x, sizeof(x))

static void write_prefix_block(char *data, int len, char type)
{
  struct tar_hdr tmp;

  memset(&tmp, 0, sizeof(tmp));
  sprintf(tmp.name, "././@%s", type=='x' ? "PaxHeaders" : "LongLink");
  ITOO(tmp.uid, 0);
  ITOO(tmp.gid, 0);
  ITOO(tmp.size, len);
  ITOO(tmp.mtime, 0);
  tmp.type = type;
  strcpy(tmp.magic, "ustar  ");

  // Historical nonsense to match other implementations. Never used.
  ITOO(tmp.mode, 0644);
  strcpy(tmp.uname, "root");
  strcpy(tmp.gname, "root");

  // Calculate checksum. Since 512*255 = 0377000 in octal, this can never
  // use more than 6 digits. The last byte is ' ' for historical reasons.
  itoo(tmp.chksum, sizeof(tmp.chksum)-1, tar_cksum(&tmp));
  tmp.chksum[7] = ' ';

  // write header and name, padded with NUL to block size
  xwrite(TT.fd, &tmp, 512);
  xwrite(TT.fd, data, len);
  if (len%512) xwrite(TT.fd, toybuf, 512-(len%512));
}

static void maybe_prefix_block(char *data, int check, int type)
{
  int len = strlen(data);

  if (len>check) write_prefix_block(data, len+1, type);
}

static int do_filter(char *pattern, char *name, long long flags)
{
  int ign = !!(flags&FLAG_ignore_case), wild = !!(flags&FLAG_wildcards),
      slash = !!(flags&FLAG_wildcards_match_slash), len;

  if (wild || slash) {
    // 1) match can end with / 2) maybe case insensitive 2) maybe * matches /
    if (!fnmatch(pattern, name, FNM_LEADING_DIR+FNM_CASEFOLD*ign+FNM_PATHNAME*slash))
      return 1;
  } else {
    len = strlen(pattern);
    if (!(ign ? strncasecmp : strncmp)(pattern, name, len))
      if (!name[len] || name[len]=='/') return 1;
  }

  return 0;
}

static struct double_list *filter(struct double_list *lst, char *name)
{
  struct double_list *end = lst;
  long long flags = toys.optflags;
  char *ss;

  if (!lst || !*name) return 0;

  // --wildcards-match-slash implies --wildcards because I couldn't figure
  // out a graceful way to explain why it DIDN'T in the help text. We don't
  // do the positional enable/disable thing (would need to annotate at list
  // creation, maybe a TODO item).

  // Set defaults for filter type, and apply --no-flags
  if (lst == TT.excl) flags |= FLAG_wildcards_match_slash;
  else flags |= FLAG_anchored;
  flags &= (~(flags&(FLAG_no_ignore_case|FLAG_no_anchored|FLAG_no_wildcards|FLAG_no_wildcards_match_slash)))>>1;
  if (flags&FLAG_no_wildcards) flags &= ~FLAG_wildcards_match_slash;

  // The +1 instead of ++ is in case of conseutive slashes
  do {
    if (do_filter(lst->data, name, flags)) return lst;
    if (!(flags & FLAG_anchored)) for (ss = name; *ss; ss++) {
      if (*ss!='/' || !ss[1]) continue;
      if (do_filter(lst->data, ss+1, flags)) return lst;
    }
  } while (end != (lst = lst->next));

  return 0;
}

static void skippy(long long len)
{
  if (lskip(TT.fd, len)) perror_exit("EOF");
}

// allocate and read data from TT.fd
static void alloread(void *buf, int len)
{
  // actually void **, but automatic typecasting doesn't work with void ** :(
  char **b = buf;

  free(*b);
  *b = xmalloc(len+1);
  xreadall(TT.fd, *b, len);
  (*b)[len] = 0;
}

static char *xform(char **name, char type)
{
  char buf[9], *end;
  off_t len;

  if (!TT.xform) return 0;

  buf[8] = 0;
  if (dprintf(TT.xfpipe[0], "%s%c%c", *name, type, 0) != strlen(*name)+2
    || readall(TT.xfpipe[1], buf, 8) != 8
    || !(len = estrtol(buf, &end, 16)) || errno ||*end) error_exit("bad xform");
  xreadall(TT.xfpipe[1], *name = xmalloc(len+1), len);
  (*name)[len] = 0;

  return *name;
}

int dirtree_sort(struct dirtree **aa, struct dirtree **bb)
{
  return (FLAG(ignore_case) ? strcasecmp : strcmp)(aa[0]->name, bb[0]->name);
}

// callback from dirtree to create archive
static int add_to_tar(struct dirtree *node)
{
  struct stat *st = &(node->st);
  struct tar_hdr hdr;
  struct passwd *pw = pw;
  struct group *gr = gr;
  int i, fd = -1, recurse = 0;
  char *name, *lnk, *hname, *xfname = 0;

  if (!dirtree_notdotdot(node)) return 0;
  if (same_dev_ino(st, &TT.archive_di)) {
    error_msg("'%s' file is the archive; not dumped", node->name);
    return 0;
  }

  i = 1;
  name = hname = dirtree_path(node, &i);
  if (filter(TT.excl, name)) goto done;

  if ((FLAG(s)|FLAG(sort)) && !FLAG(no_recursion)) {
    if (S_ISDIR(st->st_mode) && !node->again) {
      free(name);

      return DIRTREE_BREADTH|DIRTREE_SYMFOLLOW*FLAG(h);

    } else if ((node->again&DIRTREE_BREADTH) && node->child) {
      struct dirtree *dt, **sort = xmalloc(sizeof(void *)*node->extra);

      for (node->extra = 0, dt = node->child; dt; dt = dt->next)
        sort[node->extra++] = dt;
      qsort(sort, node->extra--, sizeof(void *), (void *)dirtree_sort);
      node->child = *sort;
      for (i = 0; i<node->extra; i++) sort[i]->next = sort[i+1];
      sort[i]->next = 0;
      free(sort);

      // fall through to add directory
    }
  }

  // Consume the 1 extra byte alocated in dirtree_path()
  if (S_ISDIR(st->st_mode) && (lnk = name+strlen(name))[-1] != '/')
    strcpy(lnk, "/");

  // remove leading / and any .. entries from saved name
  if (!FLAG(P)) {
    while (*hname == '/') hname++;
    for (lnk = hname;;) {
      if (!(lnk = strstr(lnk, ".."))) break;
      if (lnk == hname || lnk[-1] == '/') {
        if (!lnk[2]) goto done;
        if (lnk[2]=='/') {
          lnk = hname = lnk+3;
          continue;
        }
      }
      lnk += 2;
    }
    if (!*hname) hname = "./";
  }
  if (!*hname) goto done;

  if (TT.warn && hname != name) {
    dprintf(2, "removing leading '%.*s' from member names\n",
           (int)(hname-name), name);
    TT.warn = 0;
  }

  // Override dentry data from command line and fill out header data
  if (TT.owner) st->st_uid = TT.ouid;
  if (TT.group) st->st_gid = TT.ggid;
  if (TT.mode) st->st_mode = string_to_mode(TT.mode, st->st_mode);
  if (TT.mtime) st->st_mtime = TT.mtt;
  memset(&hdr, 0, sizeof(hdr));
  ITOO(hdr.mode, st->st_mode &07777);
  ITOO(hdr.uid, st->st_uid);
  ITOO(hdr.gid, st->st_gid);
  ITOO(hdr.size, 0); //set size later
  ITOO(hdr.mtime, st->st_mtime);
  strcpy(hdr.magic, "ustar  ");

  // Are there hardlinks to a non-directory entry?
  lnk = 0;
  if ((st->st_nlink>1 || FLAG(h)) && !S_ISDIR(st->st_mode)) {
    // Have we seen this dev&ino before?
    for (i = 0; i<TT.hlc; i++) if (same_dev_ino(st, &TT.hlx[i].di)) break;
    if (i != TT.hlc) lnk = TT.hlx[i].arg;
    else {
      // first time we've seen it. Store as normal file, but remember it.
      if (!(TT.hlc&255))
        TT.hlx = xrealloc(TT.hlx, sizeof(*TT.hlx)*(TT.hlc+256));
      TT.hlx[TT.hlc].arg = xstrdup(hname);
      TT.hlx[TT.hlc].di.ino = st->st_ino;
      TT.hlx[TT.hlc].di.dev = st->st_dev;
      TT.hlc++;
    }
  }

  xfname = xform(&hname, 'r');
  strncpy(hdr.name, hname, sizeof(hdr.name));

  // Handle file types: 0=reg, 1=hardlink, 2=sym, 3=chr, 4=blk, 5=dir, 6=fifo
  if (lnk || S_ISLNK(st->st_mode)) {
    hdr.type = '1'+!lnk;
    if (lnk) {
      if (!xform(&lnk, 'h')) lnk = xstrdup(lnk);
    } else if (!(lnk = xreadlink(name))) {
      perror_msg("readlink");
      goto done;
    } else xform(&lnk, 's');

    maybe_prefix_block(lnk, sizeof(hdr.link), 'K');
    strncpy(hdr.link, lnk, sizeof(hdr.link));
    free(lnk);
  } else if (S_ISREG(st->st_mode)) {
    hdr.type = '0';
    ITOO(hdr.size, st->st_size);
  } else if (S_ISDIR(st->st_mode)) hdr.type = '5';
  else if (S_ISFIFO(st->st_mode)) hdr.type = '6';
  else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
    hdr.type = (S_ISCHR(st->st_mode))?'3':'4';
    ITOO(hdr.major, dev_major(st->st_rdev));
    ITOO(hdr.minor, dev_minor(st->st_rdev));
  } else {
    error_msg("unknown file type '%o'", st->st_mode & S_IFMT);
    goto done;
  }

  // write out 'x' prefix header for --selinux data
  if (FLAG(selinux)) {
    int start = 0, sz = 0, temp, len = 0;
    char *buf = 0, *sec = "security.selinux";

    for (;;) {
      // First time get length, second time read data into prepared buffer
      len = (S_ISLNK(st->st_mode) ? xattr_lget : xattr_get)
        (name, sec, buf+start, sz);

      // Handle data or error
      if (len>999999 || (sz && len>sz)) len = -1, errno = E2BIG;
      if (buf || len<1) {
        if (len>0) {
          strcpy(buf+start+sz, "\n");
          write_prefix_block(buf, start+sz+2, 'x');
        } else if (errno==ENODATA || errno==ENOTSUP) len = 0;
        if (len) perror_msg("getfilecon %s", name);

        free(buf);
        break;
      }

      // Allocate buffer. Length includes prefix: calculate twice (wrap 99->100)
      temp = snprintf(0, 0, "%d", sz = (start = 22)+len+1);
      start += temp + (temp != snprintf(0, 0, "%d", temp+sz));
      buf = xmprintf("%u RHT.%s=%.*s", start+len+1, sec, sz = len, "");
    }
  }

  maybe_prefix_block(hname, sizeof(hdr.name), 'L');
  if (!FLAG(numeric_owner)) {
    if ((TT.owner || (pw = bufgetpwuid(st->st_uid))) &&
        ascii_fits(st->st_uid, sizeof(hdr.uid)))
      strncpy(hdr.uname, TT.owner ? : pw->pw_name, sizeof(hdr.uname));
    if ((TT.group || (gr = bufgetgrgid(st->st_gid))) &&
        ascii_fits(st->st_gid, sizeof(hdr.gid)))
      strncpy(hdr.gname, TT.group ? : gr->gr_name, sizeof(hdr.gname));
  }

  TT.sparselen = 0;
  if (hdr.type == '0') {
    // Before we write the header, make sure we can read the file
    if ((fd = open(name, O_RDONLY)) < 0) {
      perror_msg("can't open '%s'", name);
      free(name);

      return 0;
    }
    if (FLAG(S)) {
      long long lo, ld = 0, len = 0;

      // Enumerate the extents
      while ((lo = lseek(fd, ld, SEEK_HOLE)) != -1) {
        if (!(TT.sparselen&511))
          TT.sparse = xrealloc(TT.sparse, (TT.sparselen+514)*sizeof(long long));
        if (ld != lo) {
          TT.sparse[TT.sparselen++] = ld;
          len += TT.sparse[TT.sparselen++] = lo-ld;
        }
        if (lo == st->st_size || (ld = lseek(fd, lo, SEEK_DATA)) < lo) break;
      }

      // If there were extents, change type to S record
      if (TT.sparselen>2) {
        TT.sparse[TT.sparselen++] = st->st_size;
        TT.sparse[TT.sparselen++] = 0;
        hdr.type = 'S';
        lnk = (char *)&hdr;
        for (i = 0; i<TT.sparselen && i<8; i++)
          itoo(lnk+386+12*i, 12, TT.sparse[i]);

        // Record if there's overflow records, change length to sparse length,
        // record apparent length
        if (TT.sparselen>8) lnk[482] = 1;
        itoo(lnk+483, 12, st->st_size);
        ITOO(hdr.size, len);
      } else TT.sparselen = 0;
      lseek(fd, 0, SEEK_SET);
    }
  }

  itoo(hdr.chksum, sizeof(hdr.chksum)-1, tar_cksum(&hdr));
  hdr.chksum[7] = ' ';

  if (FLAG(v)) dprintf(1+(TT.fd==1), "%s\n", hname);

  // Write header and data to archive
  xwrite(TT.fd, &hdr, 512);
  if (TT.sparselen>8) {
    char buf[512];

    // write extent overflow blocks
    for (i=8;;i++) {
      int j = (i-8)%42;

      if (!j || i==TT.sparselen) {
        if (i!=8) {
          if (i!=TT.sparselen) buf[504] = 1;
          xwrite(TT.fd, buf, 512);
        }
        if (i==TT.sparselen) break;
        memset(buf, 0, sizeof(buf));
      }
      itoo(buf+12*j, 12, TT.sparse[i]);
    }
  }
  TT.sparselen >>= 1;
  if (hdr.type == '0' || hdr.type == 'S') {
    if (hdr.type == '0') xsendfile_pad(fd, TT.fd, st->st_size);
    else for (i = 0; i<TT.sparselen; i++) {
      if (TT.sparse[i*2] != lseek(fd, TT.sparse[i*2], SEEK_SET))
        perror_msg("%s: seek %lld", name, TT.sparse[i*2]);
      xsendfile_pad(fd, TT.fd, TT.sparse[i*2+1]);
    }
    if (st->st_size%512) writeall(TT.fd, toybuf, (512-(st->st_size%512)));
    close(fd);
  }
  recurse = !FLAG(no_recursion);

done:
  free(xfname);
  free(name);

  if (FLAG(one_file_system) && node->parent
      && node->parent->st.st_dev != node->st.st_dev) recurse = 0;
  return recurse*(DIRTREE_RECURSE|DIRTREE_SYMFOLLOW*FLAG(h));
}

static void wsettime(char *s, long long sec)
{
  struct timespec times[2] = {{sec, 0},{sec, 0}};

  if (utimensat(AT_FDCWD, s, times, AT_SYMLINK_NOFOLLOW))
    perror_msg("settime %lld %s", sec, s);
}

static void freedup(char **to, char *from)
{
  free(*to);
  *to = xstrdup(from);
}

// Do pending directory utimes(), NULL to flush all.
static int dirflush(char *name, int isdir)
{
  char *s = 0, *ss;

  // Barf if name not in TT.cwd
  if (name) {
    if (!(ss = s = xabspath(name, isdir ? ABS_LAST : 0))) {
      error_msg("'%s' bad symlink", name);

      return 1;
    }
    if (TT.cwd[1] && (!strstart(&ss, TT.cwd) || (*ss && *ss!='/'))) {
      error_msg("'%s' %s not under '%s'", name, s, TT.cwd);
      free(s);

      return 1;
    }

    // --restrict means first entry extracted is what everything must be under
    if (FLAG(restrict)) {
      freedup(&TT.cwd, s);
      toys.optflags ^= FLAG_restrict;
    }
    // use resolved name so trailing / is stripped
    if (isdir) unlink(s);
  }

  // Set deferred utimes() for directories this file isn't under.
  // (Files must be depth-first ordered in tarball for this to matter.)
  while (TT.dirs) {

    // If next file is under (or equal to) this dir, keep waiting
    if (name && strstart(&ss, ss = s) && (!*ss || *ss=='/')) break;

    wsettime(TT.dirs->str+sizeof(long long), *(long long *)TT.dirs->str);
    free(llist_pop(&TT.dirs));
  }
  free(s);

  // name was under TT.cwd
  return 0;
}

// write data to file
static void sendfile_sparse(int fd)
{
  long long len, used = 0, sent;
  int i = 0, j;

  do {
    if (TT.sparselen) {
      // Seek past holes or fill output with zeroes.
      if (-1 == lseek(fd, len = TT.sparse[i*2], SEEK_SET)) {
        sent = 0;
        while (len) {
          // first/last 512 bytes used, rest left zeroes
          j = (len>3072) ? 3072 : len;
          if (j != writeall(fd, toybuf+512, j)) goto error;
          len -= j;
        }
      } else {
        sent = len;
        if (!(len = TT.sparse[i*2+1]) && ftruncate(fd, sent))
          perror_msg("ftruncate");
      }
      if (len+used>TT.hdr.size) error_exit("sparse overflow");
    } else len = TT.hdr.size;

    len -= sendfile_len(TT.fd, fd, len, &sent);
    used += sent;
    if (len) {
error:
      if (fd!=1) perror_msg(0);
      skippy(TT.hdr.size-used);

      break;
    }
  } while (++i<TT.sparselen);

  close(fd);
}

static void extract_to_disk(char *name)
{
  int ala = TT.hdr.mode;

  if (dirflush(name, S_ISDIR(ala))) {
    if (S_ISREG(ala) && !TT.hdr.link_target) skippy(TT.hdr.size);

    return;
  }

  // create path before file if necessary
  if (strrchr(name, '/') && mkpath(name) && errno!=EEXIST)
      return perror_msg(":%s: can't mkdir", name);

  // remove old file, if exists
  if (!FLAG(k) && !S_ISDIR(ala) && rmdir(name) && errno!=ENOENT && unlink(name))
    return perror_msg("can't remove: %s", name);

  if (S_ISREG(ala)) {
    // hardlink?
    if (TT.hdr.link_target) {
      if (link(TT.hdr.link_target, name))
        return perror_msg("can't link '%s' -> '%s'", name, TT.hdr.link_target);
    // write contents
    } else {
      int fd = WARN_ONLY|O_WRONLY|O_CREAT|(FLAG(overwrite) ? O_TRUNC : O_EXCL);

      if ((fd = xcreate(name, fd, 0700)) != -1) sendfile_sparse(fd);
      else return skippy(TT.hdr.size);
    }
  } else if (S_ISDIR(ala)) {
    if ((mkdir(name, 0700) == -1) && errno != EEXIST)
      return perror_msg("%s: can't create", name);
  } else if (S_ISLNK(ala)) {
    if (symlink(TT.hdr.link_target, name))
      return perror_msg("can't link '%s' -> '%s'", name, TT.hdr.link_target);
  } else if (mknod(name, ala&~toys.old_umask, TT.hdr.device))
    return perror_msg("can't create '%s'", name);

  // Set ownership
  if (!FLAG(o) && !geteuid()) {
    int u = TT.hdr.uid, g = TT.hdr.gid;

    if (TT.owner) TT.hdr.uid = TT.ouid;
    else if (!FLAG(numeric_owner) && *TT.hdr.uname) {
      struct passwd *pw = bufgetpwnamuid(TT.hdr.uname, 0);
      if (pw) TT.hdr.uid = pw->pw_uid;
    }

    if (TT.group) TT.hdr.gid = TT.ggid;
    else if (!FLAG(numeric_owner) && *TT.hdr.uname) {
      struct group *gr = bufgetgrnamgid(TT.hdr.gname, 0);
      if (gr) TT.hdr.gid = gr->gr_gid;
    }

    if (lchown(name, u, g)) perror_msg("chown %d:%d '%s'", u, g, name);;
  }

  if (!S_ISLNK(ala)) chmod(name, FLAG(p) ? ala : ala&0777&~toys.old_umask);

  // Apply mtime.
  if (!FLAG(m)) {
    if (S_ISDIR(ala)) {
      struct string_list *sl;

      // Writing files into a directory changes directory timestamps, so
      // defer mtime updates until contents written.

      sl = xmalloc(sizeof(struct string_list)+sizeof(long long)+strlen(name)+1);
      *(long long *)sl->str = TT.hdr.mtime;
      strcpy(sl->str+sizeof(long long), name);
      sl->next = TT.dirs;
      TT.dirs = sl;
    } else wsettime(name, TT.hdr.mtime);
  }
}

static void unpack_tar(char *first)
{
  struct double_list *walk, *delete;
  struct tar_hdr tar;
  int i, sefd = -1, and = 0;
  unsigned maj, min;
  char *s, *name;

  for (;;) {
    if (first) {
      memcpy(&tar, first, i = 512);
      first = 0;
    } else {
      // align to next block and read it
      if (TT.hdr.size%512) skippy(512-TT.hdr.size%512);
      i = readall(TT.fd, &tar, 512);
    }

    if (i && i!=512) error_exit("short header");

    // Two consecutive empty headers ends tar even if there's more data
    if (!i || !*tar.name) {
      if (!i || and++) return;
      TT.hdr.size = 0;
      continue;
    }
    // ensure null temination even of pathological packets
    tar.padd[0] = and = 0;

    // Is this a valid TAR header?
    if (!is_tar_header(&tar)) error_exit("bad header");
    TT.hdr.size = OTOI(tar.size);

    // If this header isn't writing something to the filesystem
    if ((tar.type<'0' || tar.type>'7') && tar.type!='S'
        && (*tar.magic && tar.type))
    {
      // Skip to next record if unknown type or payload > 1 megabyte
      if (!strchr("KLx", tar.type) || TT.hdr.size>1<<20) skippy(TT.hdr.size);
      // Read link or long name
      else if (tar.type != 'x')
        alloread(tar.type=='K'?&TT.hdr.link_target:&TT.hdr.name, TT.hdr.size);
      // Loop through 'x' payload records in "LEN NAME=VALUE\n" format
      else {
        char *p, *pp, *buf = 0;
        unsigned i, len, n;

        alloread(&buf, TT.hdr.size);
        for (p = buf; (p-buf)<TT.hdr.size; p += len) {
          i = TT.hdr.size-(p-buf);
          if (1!=sscanf(p, "%u %n", &len, &n) || len<n+4 || len>i || n>i) {
            error_msg("bad header");
            break;
          }
          p[len-1] = 0;
          pp = p+n;
          // Ignore "RHT." prefix, if any.
          strstart(&pp, "RHT.");
          if ((FLAG(selinux) && !(FLAG(t)|FLAG(O)))
              && strstart(&pp, "security.selinux="))
          {
            i = strlen(pp);
            sefd = xopen("/proc/self/attr/fscreate", O_WRONLY|WARN_ONLY);
            if (sefd==-1 ||  i!=write(sefd, pp, i))
              perror_msg("setfscreatecon %s", pp);
          } else if (strstart(&pp, "path=")) freedup(&TT.hdr.name, pp);
          // legacy sparse format circa 2005
          else if (strstart(&pp, "GNU.sparse.name=")) freedup(&TT.hdr.name, pp);
          else if (strstart(&pp, "GNU.sparse.realsize="))
            TT.hdr.oldsparse = atoll(pp);
        }
        free(buf);
      }

      continue;
    }

    // Handle sparse file type
    TT.sparselen = 0;
    if (tar.type == 'S') {
      char sparse[512];
      int max = 8;

      // Load 4 pairs of offset/len from S block, plus 21 pairs from each
      // continuation block, list says where to seek/write sparse file contents
      s = 386+(char *)&tar;
      *sparse = i = 0;

      for (;;) {
        if (!(TT.sparselen&511))
          TT.sparse = xrealloc(TT.sparse, (TT.sparselen+512)*sizeof(long long));

        // If out of data in block check continue flag, stop or load next block
        if (++i>max || !*s) {
          if (!(*sparse ? sparse[504] : ((char *)&tar)[482])) break;
          xreadall(TT.fd, s = sparse, 512);
          max = 41;
          i = 0;
        }
        // Load next entry
        TT.sparse[TT.sparselen++] = otoi(s, 12);
        s += 12;
      }

      // Odd number of entries (from corrupted tar) would be dropped here
      TT.sparselen /= 2;
      if (TT.sparselen)
        TT.hdr.ssize = TT.sparse[2*TT.sparselen-1]+TT.sparse[2*TT.sparselen-2];
    } else {
      TT.hdr.ssize = TT.hdr.size;

      // Handle obsolete sparse format
      if (TT.hdr.oldsparse>0) {
        char sparse[512], c;
        long long ll = 0;

        s = sparse+512;
        for (i = 0;;) {
          if (s == sparse+512) {
            if (TT.hdr.size<512) break;
            xreadall(TT.fd, s = sparse, 512);
            TT.hdr.size -= 512;
          } else if (!(c = *s++)) break;
          else if (isdigit(c)) ll = (10*ll)+c-'0';
          else {
            if (!TT.sparselen)
              TT.sparse = xzalloc(((TT.sparselen = ll)+1)*2*sizeof(long long));
            else TT.sparse[i++] = ll;
            ll = 0;
            if (i == TT.sparselen*2) break;
          }
        }
        if (TT.sparselen) {
          ll = TT.sparse[2*(TT.sparselen-1)]+TT.sparse[2*TT.sparselen-1];
          if (TT.hdr.oldsparse>ll)
            TT.sparse[2*TT.sparselen++] = TT.hdr.oldsparse;
        }
        TT.hdr.oldsparse = 0;
      }
    }

    // At this point, we have something to output. Convert metadata.
    TT.hdr.mode = OTOI(tar.mode)&0xfff;
    if (tar.type == 'S' || !tar.type || !*tar.magic) TT.hdr.mode |= 0x8000;
    else TT.hdr.mode |= (char []){8,8,10,2,6,4,1,8}[tar.type-'0']<<12;
    TT.hdr.uid = OTOI(tar.uid);
    TT.hdr.gid = OTOI(tar.gid);
    TT.hdr.mtime = OTOI(tar.mtime);
    maj = OTOI(tar.major);
    min = OTOI(tar.minor);
    TT.hdr.device = dev_makedev(maj, min);
    TT.hdr.uname = xstrndup(TT.owner ? : tar.uname, sizeof(tar.uname));
    TT.hdr.gname = xstrndup(TT.group ? : tar.gname, sizeof(tar.gname));

    if (TT.owner) TT.hdr.uid = TT.ouid;
    else if (!FLAG(numeric_owner)) {
      struct passwd *pw = bufgetpwnamuid(TT.hdr.uname, 0);
      if (pw && (TT.owner || !FLAG(numeric_owner))) TT.hdr.uid = pw->pw_uid;
    }

    if (TT.group) TT.hdr.gid = TT.ggid;
    else if (!FLAG(numeric_owner)) {
      struct group *gr = bufgetgrnamgid(TT.hdr.gname, 0);
      if (gr) TT.hdr.gid = gr->gr_gid;
    }

    if (!TT.hdr.link_target && *tar.link)
      TT.hdr.link_target = xstrndup(tar.link, sizeof(tar.link));
    if (!TT.hdr.name) {
      // Glue prefix and name fields together with / if necessary
      i = (tar.type=='S') ? 0 : strnlen(tar.prefix, sizeof(tar.prefix));
      TT.hdr.name = xmprintf("%.*s%s%.*s", i, tar.prefix,
        (i && tar.prefix[i-1] != '/') ? "/" : "",
        (int)sizeof(tar.name), tar.name);
    }

    // Old broken tar recorded dir as "file with trailing slash"
    if (S_ISREG(TT.hdr.mode) && (s = strend(TT.hdr.name, "/"))) {
      *s = 0;
      TT.hdr.mode = (TT.hdr.mode & ~S_IFMT) | S_IFDIR;
    }

    // Non-regular files don't have contents stored in archive.
    if ((TT.hdr.link_target && *TT.hdr.link_target)
      || (tar.type && !S_ISREG(TT.hdr.mode)))
        TT.hdr.size = 0;

    // Files are seen even if excluded, so check them here.
    // TT.seen points to first seen entry in TT.incl, or NULL if none yet.

    if ((delete = filter(TT.incl, TT.hdr.name)) && TT.incl != TT.seen) {
      if (!TT.seen) TT.seen = delete;

      // Move seen entry to end of list.
      if (TT.incl == delete) TT.incl = TT.incl->next;
      else for (walk = TT.incl; walk != TT.seen; walk = walk->next) {
        if (walk == delete) {
          dlist_pop(&walk);
          dlist_add_nomalloc(&TT.incl, delete);
        }
      }
    }

    // Skip excluded files, filtering on the untransformed name.
    if (filter(TT.excl, name = TT.hdr.name) || (TT.incl && !delete)) {
      skippy(TT.hdr.size);
      goto done;
    }

    // We accept --show-transformed but always do, so it's a NOP.
    name = TT.hdr.name;
    if (xform(&name, 'r')) {
      free(TT.hdr.name);
      TT.hdr.name = name;
    }
    if ((i = "\0hs"[stridx("12", tar.type)+1])) xform(&TT.hdr.link_target, i);

    for (i = 0; i<TT.strip; i++) {
      char *s = strchr(name, '/');

      if (s && s[1]) name = s+1;
      else {
        if (S_ISDIR(TT.hdr.mode)) *name = 0;
        break;
      }
    }

    if (!*name) skippy(TT.hdr.size);
    else if (FLAG(t)) {
      if (FLAG(v)) {
        struct tm *lc = localtime(TT.mtime ? &TT.mtt : &TT.hdr.mtime);
        char perm[12], gname[12];

        mode_to_string(TT.hdr.mode, perm);
        printf("%s", perm);
        sprintf(perm, "%u", TT.hdr.uid);
        sprintf(gname, "%u", TT.hdr.gid);
        printf(" %s/%s ", *TT.hdr.uname ? TT.hdr.uname : perm,
          *TT.hdr.gname ? TT.hdr.gname : gname);
        if (tar.type=='3' || tar.type=='4') printf("%u,%u", maj, min);
        else printf("%9lld", TT.hdr.ssize);
        sprintf(perm, ":%02d", lc->tm_sec);
        printf("  %d-%02d-%02d %02d:%02d%s ", 1900+lc->tm_year, 1+lc->tm_mon,
          lc->tm_mday, lc->tm_hour, lc->tm_min, FLAG(full_time) ? perm : "");
      }
      printf("%s", name);
      if (TT.hdr.link_target)
        printf(" %s %s", tar.type=='2' ? "->" : "link to", TT.hdr.link_target);
      xputc('\n');
      skippy(TT.hdr.size);
    } else {
      if (FLAG(v)) printf("%s\n", name);
      if (FLAG(O)) sendfile_sparse(1);
      else if (FLAG(to_command)) {
        if (S_ISREG(TT.hdr.mode)) {
          int fd, pid;

          xsetenv("TAR_FILETYPE", "f");
          xsetenv(xmprintf("TAR_MODE=%o", TT.hdr.mode), 0);
          xsetenv(xmprintf("TAR_SIZE=%lld", TT.hdr.ssize), 0);
          xsetenv("TAR_FILENAME", name);
          xsetenv("TAR_UNAME", TT.hdr.uname);
          xsetenv("TAR_GNAME", TT.hdr.gname);
          xsetenv(xmprintf("TAR_MTIME=%llo", (long long)TT.hdr.mtime), 0);
          xsetenv(xmprintf("TAR_UID=%o", TT.hdr.uid), 0);
          xsetenv(xmprintf("TAR_GID=%o", TT.hdr.gid), 0);

          pid = xpopen((char *[]){"sh", "-c", TT.to_command, NULL}, &fd, 0);
          // TODO: short write exits tar here, other skips data.
          sendfile_sparse(fd);
          fd = xpclose_both(pid, 0);
          if (fd) error_msg("%d: Child returned %d", pid, fd);
        }
      } else extract_to_disk(name);
    }

done:
    if (sefd != -1) {
      // zero length write resets fscreate context to default
      (void)write(sefd, 0, 0);
      close(sefd);
      sefd = -1;
    }
    free(TT.hdr.name);
    free(TT.hdr.link_target);
    free(TT.hdr.uname);
    free(TT.hdr.gname);
    TT.hdr.name = TT.hdr.link_target = 0;
  }
}

// Add copy of filename (minus trailing \n and /) to dlist **
static void trim2list(void *list, char *pline)
{
  char *n = xstrdup(pline);
  int i = strlen(n);

  dlist_add(list, n);
  if (i && n[i-1]=='\n') i--;
  while (i>1 && n[i-1] == '/') i--;
  n[i] = 0;
}

// do_lines callback, selects TT.incl or TT.excl based on call order
static void do_XT(char **pline, long len)
{
  if (pline) trim2list(TT.X ? &TT.excl : &TT.incl, *pline);
}

static char *get_archiver()
{
  return TT.I ? : FLAG(z)?"gzip" : FLAG(j)?"bzip2" : FLAG(Z)?"zstd" : "xz";
}

void tar_main(void)
{
  char *s, **xfsed, **args = toys.optargs;
  int len = 0, ii;

  // Needed when extracting to command
  signal(SIGPIPE, SIG_IGN);

  // Get possible early errors out of the way
  if (!geteuid()) toys.optflags |= FLAG_p;
  if (TT.owner) {
    if (!(s = strchr(TT.owner, ':'))) TT.ouid = xgetuid(TT.owner);
    else {
      TT.owner = xstrndup(TT.owner, s++-TT.owner);
      TT.ouid = atolx_range(s, 0, INT_MAX);
    }
  }
  if (TT.group) {
    if (!(s = strchr(TT.group, ':'))) TT.ggid = xgetgid(TT.group);
    else {
      TT.group = xstrndup(TT.group, s++-TT.group);
      TT.ggid = atolx_range(s, 0, INT_MAX);
    }
  }
  if (TT.mtime) xparsedate(TT.mtime, &TT.mtt, (void *)&s, 1);

  // TODO: collect filter types here and annotate saved include/exclude?

  // Collect file list.
  for (; TT.exclude; TT.exclude = TT.exclude->next)
    trim2list(&TT.excl, TT.exclude->arg);
  for (;TT.X; TT.X = TT.X->next) do_lines(xopenro(TT.X->arg), '\n', do_XT);
  for (args = toys.optargs; *args; args++) trim2list(&TT.incl, *args);
  // -T is always --verbatim-files-from: no quote removal or -arg handling
  for (;TT.T; TT.T = TT.T->next)
    do_lines(xopenro(TT.T->arg), '\n'*!FLAG(null), do_XT);

  // If include file list empty, don't create empty archive
  if (FLAG(c)) {
    if (!TT.incl) error_exit("empty archive");
    TT.fd = 1;
  }

  if (TT.xform) {
    struct arg_list *al;

    for (ii = 0, al = TT.xform; al; al = al->next) ii++;
    xfsed = xmalloc((ii+2)*2*sizeof(char *));
    xfsed[0] = "sed";
    xfsed[1] = "--tarxform";
    for (ii = 2, al = TT.xform; al; al = al->next) {
      xfsed[ii++] = "-e";
      xfsed[ii++] = al->arg;
    }
    xfsed[ii] = 0;
    TT.xfpipe[0] = TT.xfpipe[1] = -1;
    xpopen_both(xfsed, TT.xfpipe);
    free(xfsed);
  }

  // nommu reentry for nonseekable input skips this, parent did it for us
  if (toys.stacktop) {
    if (TT.f && strcmp(TT.f, "-"))
      TT.fd = xcreate(TT.f, TT.fd*(O_WRONLY|O_CREAT|O_TRUNC),
                      0666&~toys.old_umask);
    // Get destination directory
    if (TT.C) xchdir(TT.C);
  }

  // Get destination directory
  TT.cwd = xabspath(s = xgetcwd(), ABS_PATH);
  free(s);

  // Remember archive inode so we don't overwrite it or add it to itself
  {
    struct stat st;

    if (!fstat(TT.fd, &st)) {
      TT.archive_di.ino = st.st_ino;
      TT.archive_di.dev = st.st_dev;
    }
  }

  // Are we reading?
  if (FLAG(x)||FLAG(t)) {
    char *hdr = 0;

    // autodetect compression type when not specified
    if (!(FLAG(j)||FLAG(z)||FLAG(I)||FLAG(J)||FLAG(Z))) {
      len = xread(TT.fd, hdr = toybuf+sizeof(toybuf)-512, 512);
      if (len!=512 || !is_tar_header(hdr)) {
        // detect gzip and bzip signatures
        if (SWAP_BE16(*(short *)hdr)==0x1f8b) toys.optflags |= FLAG_z;
        else if (!smemcmp(hdr, "BZh", 3)) toys.optflags |= FLAG_j;
	else if (!smemcmp(hdr, "\x28\xb5\x2f\xfd", 4)) toys.optflags|=FLAG_Z;
        else if (peek_be(hdr, 7) == 0xfd377a585a0000ULL) toys.optflags |= FLAG_J;
        else error_exit("Not tar");

        // if we can seek back we don't need to loop and copy data
        if (!lseek(TT.fd, -len, SEEK_CUR)) hdr = 0;
      }
    }

    if (FLAG(j)||FLAG(z)||FLAG(I)||FLAG(J)||FLAG(Z)) {
      int pipefd[2] = {hdr ? -1 : TT.fd, -1}, i, pid;
      struct string_list *zcat = FLAG(I) ? 0 : find_in_path(getenv("PATH"),
        FLAG(z)?"zcat" : FLAG(j)?"bzcat" : FLAG(Z)?"zstdcat" : "xzcat");

      // Toybox provides more decompressors than compressors, so try them first
      TT.pid = xpopen_both(zcat ? (char *[]){zcat->str, 0} :
        (char *[]){get_archiver(), "-d", 0}, pipefd);
      if (CFG_TOYBOX_FREE) llist_traverse(zcat, free);

      if (!hdr) {
        // If we could seek, child gzip inherited fd and we read its output
        close(TT.fd);
        TT.fd = pipefd[1];

      } else {

        // If we autodetected type but then couldn't lseek to put the data back
        // we have to loop reading data from TT.fd and pass it to gzip ourselves
        // (starting with the block of data we read to autodetect).

        // dirty trick: move gzip input pipe to stdin so child closes spare copy
        dup2(pipefd[0], 0);
        if (pipefd[0]) close(pipefd[0]);

        // Fork a copy of ourselves to handle extraction (reads from zip output
        // pipe, writes to stdout).
        pipefd[0] = pipefd[1];
        pipefd[1] = 1;
        pid = xpopen_both(0, pipefd);
        close(pipefd[1]);

        // loop writing collated data to zip proc
        xwrite(0, hdr, len);
        for (;;) {
          if ((i = read(TT.fd, toybuf, sizeof(toybuf)))<1) {
            close(0);
            xwaitpid(pid);
            return;
          }
          xwrite(0, toybuf, i);
        }
      }
    }

    unpack_tar(hdr);
    dirflush(0, 0);
    // Shut up archiver about inability to write all trailing NULs to pipe buf
    while (0<read(TT.fd, toybuf, sizeof(toybuf)));

    // Each time a TT.incl entry is seen it's moved to the end of the list,
    // with TT.seen pointing to first seen list entry. Anything between
    // TT.incl and TT.seen wasn't encountered in archive..
    if (TT.seen != TT.incl) {
      if (!TT.seen) TT.seen = TT.incl;
      while (TT.incl != TT.seen) {
        error_msg("'%s' not in archive", TT.incl->data);
        TT.incl = TT.incl->next;
      }
    }

  // are we writing? (Don't have to test flag here, one of 3 must be set)
  } else {
    struct double_list *dl = TT.incl;

    // autodetect compression type based on -f name. (Use > to avoid.)
    if (TT.f && !(FLAG(j)||FLAG(z)||FLAG(I)||FLAG(J)||FLAG(Z))) {
      char *tbz[] = {".tbz", ".tbz2", ".tar.bz", ".tar.bz2"};
      if (strend(TT.f, ".tgz") || strend(TT.f, ".tar.gz"))
        toys.optflags |= FLAG_z;
      else if (strend(TT.f, ".txz") || strend(TT.f, ".tar.xz"))
        toys.optflags |= FLAG_J;
      else if (strend(TT.f, ".tzst") || strend(TT.f, ".tar.zst"))
        toys.optflags |= FLAG_Z;
      else for (len = 0; len<ARRAY_LEN(tbz); len++)
        if (strend(TT.f, tbz[len])) toys.optflags |= FLAG_j;
    }

    if (FLAG(j)||FLAG(z)||FLAG(I)||FLAG(J)||FLAG(Z)) {
      int pipefd[2] = {-1, TT.fd};

      TT.pid = xpopen_both((char *[]){get_archiver(), 0}, pipefd);
      close(TT.fd);
      TT.fd = pipefd[0];
    }
    do {
      TT.warn = 1;
      dirtree_flagread(dl->data,
        DIRTREE_SYMFOLLOW*FLAG(h)|DIRTREE_BREADTH*(FLAG(sort)|FLAG(s)),
        add_to_tar);
    } while (TT.incl != (dl = dl->next));

    writeall(TT.fd, toybuf, 1024);
    close(TT.fd);
  }
  if (TT.pid) {
    TT.pid = xpclose_both(TT.pid, 0);
    if (TT.pid) toys.exitval = TT.pid;
  }
  if (toys.exitval) error_msg("had errors");

  if (CFG_TOYBOX_FREE) {
    llist_traverse(TT.excl, llist_free_double);
    llist_traverse(TT.incl, llist_free_double);
    while(TT.hlc) free(TT.hlx[--TT.hlc].arg);
    free(TT.hlx);
    free(TT.cwd);
    close(TT.fd);
  }
}
