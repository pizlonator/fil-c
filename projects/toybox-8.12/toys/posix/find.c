/* find.c - Search directories for matching files.
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/find.c
 *
 * Our "unspecified" behavior for no paths is to use "."
 * Parentheses can only stack 4096 deep
 * Not treating two {} as an error, but only using last
 * TODO: -context

USE_FIND(NEWTOY(find, "?^HL[-HL]", TOYFLAG_USR|TOYFLAG_BIN))

config FIND
  bool "find"
  default y
  help
    usage: find [-HL] [DIR...] [<options>]

    Search directories for matching files.
    Default: search ".", match all, -print matches.

    -H  Follow command line symlinks         -L  Follow all symlinks

    Match filters:
    -name  PATTERN   filename with wildcards   -iname      ignore case -name
    -path  PATTERN   path name with wildcards  -ipath      ignore case -path
    -user  UNAME     belongs to user UNAME     -nouser     user ID not known
    -group GROUP     belongs to group GROUP    -nogroup    group ID not known
    -perm  [-/]MODE  permissions (-=min /=any) -prune      ignore dir contents
    -size  N[c]      512 byte blocks (c=bytes) -xdev       only this filesystem
    -links N         hardlink count            -empty      empty files and dirs
    -atime N[u]      accessed N units ago      -true       always true
    -ctime N[u]      created N units ago       -false      always false
    -mtime N[u]      modified N units ago      -executable access(X_OK) perm+ACL
    -inum  N         inode number N            -readable   access(R_OK) perm+ACL
    -context PATTERN security context          -depth      contents before dir
    -samefile FILE   hardlink to FILE          -maxdepth N at most N dirs down
    -newer    FILE   newer mtime than FILE     -mindepth N at least N dirs down
    -newerXY  FILE   X=acm time > FILE's Y=acm time (Y=t: FILE is literal time)
    -type [bcdflps]  type is (block, char, dir, file, symlink, pipe, socket)

    Numbers N may be prefixed by - (less than) or + (greater than). Units for
    -[acm]time are d (days, default), h (hours), m (minutes), or s (seconds).

    Combine matches with:
    !, -a, -o, ( )    not, and, or, group expressions

    Actions:
    -print  Print match with newline  -print0        Print match with null
    -exec   Run command with path     -execdir       Run command in file's dir
    -ok     Ask before exec           -okdir         Ask before execdir
    -delete Remove matching file/dir  -printf FORMAT Print using format string
    -quit   Exit immediately

    Commands substitute "{}" with matched file. End with ";" to run each file,
    or "+" (next argument after "{}") to collect and run with multiple files.

    -printf FORMAT characters are \ escapes and:
    %b  512 byte blocks used
    %f  basename            %g  textual gid          %G  numeric gid
    %i  decimal inode       %l  target of symlink    %m  octal mode
    %M  ls format type/mode %p  path to file         %P  path to file minus DIR
    %s  size in bytes       %T@ mod time as unixtime
    %u  username            %U  numeric uid          %Z  security context
*/

#define FOR_find
#include "toys.h"

GLOBALS(
  char **filter;
  struct double_list *argdata;
  int topdir, xdev, depth;
  time_t now;
  long max_bytes;
  char *start;
)

struct execdir_data {
  struct execdir_data *next;

  int namecount;
  struct double_list *names;
};

// None of this can go in TT because you can have more than one -exec
struct exec_range {
  char *next, *prev;  // layout compatible with struct double_list

  int dir, plus, arglen, argsize, curly;
  char **argstart;
  struct execdir_data exec, *execdir;
};

// Perform pending -exec (if any)
static int flush_exec(struct dirtree *new, struct exec_range *aa)
{
  struct execdir_data *bb = aa->execdir ? aa->execdir : &aa->exec;
  char **newargs;
  int rc, revert = 0;

  if (!bb->namecount) return 0;

  dlist_terminate(bb->names);

  // switch to directory for -execdir, or back to top if we have an -execdir
  // _and_ a normal -exec, or are at top of tree in -execdir
  if (TT.topdir != -1) {
    if (aa->dir && new && new->parent) {
      revert++;
      rc = fchdir(new->parent->dirfd);
    } else rc = fchdir(TT.topdir);
    if (rc) {
      perror_msg_raw(revert ? new->name : ".");

      return rc;
    }
  }

  // execdir: accumulated execs in this directory's children.
  newargs = xmalloc(sizeof(char *)*(aa->arglen+bb->namecount+1));
  if (aa->curly < 0) {
    memcpy(newargs, aa->argstart, sizeof(char *)*aa->arglen);
    newargs[aa->arglen] = 0;
  } else {
    int pos = aa->curly, rest = aa->arglen - aa->curly;
    struct double_list *dl;

    // Collate argument list
    memcpy(newargs, aa->argstart, sizeof(char *)*pos);
    for (dl = bb->names; dl; dl = dl->next) newargs[pos++] = dl->data;
    rest = aa->arglen - aa->curly - 1;
    memcpy(newargs+pos, aa->argstart+aa->curly+1, sizeof(char *)*rest);
    newargs[pos+rest] = 0;
  }

  rc = xrun(newargs);

  llist_traverse(bb->names, llist_free_double);
  bb->names = 0;
  bb->namecount = 0;

  if (revert) revert = fchdir(TT.topdir);

  return rc;
}

// Return numeric value with explicit sign
static int compare_numsign(long val, long units, char *str)
{
  char sign = 0;
  long myval;

  if (*str == '+' || *str == '-') sign = *(str++);
  else if (!isdigit(*str)) error_exit("%s not [+-]N", str);
  myval = atolx(str);
  if (units && isdigit(str[strlen(str)-1])) myval *= units;

  if (sign == '+') return val > myval;
  if (sign == '-') return val < myval;
  return val == myval;
}

static void do_print(struct dirtree *new, char c)
{
  char *s=dirtree_path(new, 0);

  xprintf("%s%c", s, c);
  free(s);
}

// Descend or ascend -execdir + directory level
static void execdir(struct dirtree *new, int flush)
{
  struct double_list *dl;
  struct exec_range *aa;
  struct execdir_data *bb;

  if (new && TT.topdir == -1) return;

  for (dl = TT.argdata; dl; dl = dl->next) {
    if (dl->prev != (void *)1) continue;
    aa = (void *)dl;
    if (!aa->plus || (new && !aa->dir)) continue;

    if (flush) {

      // Flush pending "-execdir +" instances for this dir
      // or flush everything for -exec at top
      toys.exitval |= flush_exec(new, aa);

      // pop per-directory struct
      if ((bb = aa->execdir)) {
        aa->execdir = bb->next;
        free(bb);
      }
    } else if (aa->dir) {

      // Push new per-directory struct for -execdir/okdir + codepath. (Can't
      // use new->extra because command line may have multiple -execdir)
      bb = xzalloc(sizeof(struct execdir_data));
      bb->next = aa->execdir;
      aa->execdir = bb;
    }
  }
}

// Call this with 0 for first pass argument parsing and syntax checking (which
// populates argdata). Later commands traverse argdata (in order) when they
// need "do once" results.
static int do_find(struct dirtree *new)
{
  int pcount = 0, print = 0, not = 0, active = !!new, test = active, recurse;
  struct double_list *argdata = TT.argdata;
  char *s, **ss, *arg;

  recurse = DIRTREE_STATLESS|DIRTREE_COMEAGAIN|DIRTREE_SYMFOLLOW*FLAG(L);

  // skip . and .. below topdir, handle -xdev and -depth
  if (new) {
    // Handle stat failures first.
    if (new->again&DIRTREE_STATLESS) {
      if (!new->parent || errno != ENOENT) {
        perror_msg("'%s'", s = dirtree_path(new, 0));
        free(s);
      }
      return 0;
    }
    if (new->parent) {
      if (!dirtree_notdotdot(new)) return 0;
      if (TT.xdev && new->st.st_dev != new->parent->st.st_dev) recurse = 0;
    } else TT.start = new->name;

    if (S_ISDIR(new->st.st_mode)) {
      // Descending into new directory
      if (!(new->again&DIRTREE_COMEAGAIN)) {
        struct dirtree *n;

        for (n = new->parent; n; n = n->parent) {
          if (same_file(&n->st, &new->st)) {
            error_msg("'%s': loop detected", s = dirtree_path(new, 0));
            free(s);

            return 0;
          }
        }

        if (TT.depth) {
          execdir(new, 0);

          return recurse;
        }
      // Done with directory (COMEAGAIN call)
      } else {
        execdir(new, 1);
        recurse = 0;
        if (!TT.depth) return 0;
      }
    }
  }

  // pcount: parentheses stack depth (using toybuf bytes, 4096 max depth)
  // test: result of most recent test
  // active: if 0 don't perform tests
  // not: a pending ! applies to this test (only set if performing tests)
  // print: saw one of print/ok/exec, no need for default -print

  if (TT.filter) for (ss = TT.filter; (s = *ss); ss++) {
    int check = active && test;

    // if (!new) perform one-time setup, if (check) perform test

    // handle ! ( ) using toybuf as a stack
    if (*s != '-') {
      if (s[1]) goto error;

      if (*s == '!') {
        // Don't invert if we're not making a decision
        if (check) not = !not;

      // Save old "not" and "active" on toybuf stack.
      // Deactivate this parenthetical if !test
      // Note: test value should never change while !active
      } else if (*s == '(') {
        if (pcount == sizeof(toybuf)) goto error;
        toybuf[pcount++] = not+(active<<1);
        if (!check) active = 0;
        not = 0;

      // Pop status, apply deferred not to test
      } else if (*s == ')') {
        if (--pcount < 0) goto error;
        // Pop active state, apply deferred not (which was only set if checking)
        active = (toybuf[pcount]>>1)&1;
        if (active && (toybuf[pcount]&1)) test = !test;
        not = 0;
      } else goto error;

      continue;
    } else s++;

    if (!strcmp(s, "xdev")) TT.xdev = 1;
    else if (!strcmp(s, "delete")) {
      // Delete forces depth first
      TT.depth = 1;
      if (new && check)
        test = !unlinkat(dirtree_parentfd(new), new->name,
          S_ISDIR(new->st.st_mode) ? AT_REMOVEDIR : 0);
    } else if (!strcmp(s, "depth") || !strcmp(s, "d")) TT.depth = 1;
    else if (!strcmp(s, "o") || !strcmp(s, "or")) {
      if (not) goto error;
      if (active) {
        if (!test) test = 1;
        else active = 0;     // decision has been made until next ")"
      }
    } else if (!strcmp(s, "not")) {
      if (check) not = !not;
      continue;
    } else if (!strcmp(s, "true")) {
      if (check) test = 1;
    } else if (!strcmp(s, "false")) {
      if (check) test = 0;

    // Mostly ignore NOP argument
    } else if (!strcmp(s, "a") || !strcmp(s, "and") || !strcmp(s, "noleaf")) {
      if (not) goto error;

    } else if (!strcmp(s, "print") || !strcmp("print0", s)) {
      print++;
      if (check) do_print(new, s[5] ? 0 : '\n');

    } else if (!strcmp(s, "empty")) {
      if (check) {
        // Alas neither st_size nor st_blocks reliably show an empty directory
        if (S_ISDIR(new->st.st_mode)) {
          int fd = openat(dirtree_parentfd(new), new->name, O_RDONLY);
          DIR *dfd = fdopendir(fd);
          struct dirent *de = (void *)1;
          if (dfd) {
            while ((de = readdir(dfd)) && isdotdot(de->d_name));
            closedir(dfd);
          }
          if (de) test = 0;
        } else if (S_ISREG(new->st.st_mode)) {
          if (new->st.st_size) test = 0;
        } else test = 0;
      }
    } else if (!strcmp(s, "nouser")) {
      if (check && bufgetpwuid(new->st.st_uid)) test = 0;
    } else if (!strcmp(s, "nogroup")) {
      if (check && bufgetgrgid(new->st.st_gid)) test = 0;
    } else if (!strcmp(s, "prune")) {
      if (check && S_ISDIR(new->st.st_mode) && !TT.depth) recurse = 0;
    } else if (!strcmp(s, "executable") || !strcmp(s, "readable")) {
      if (check && faccessat(dirtree_parentfd(new), new->name,
          *s=='r' ? R_OK : X_OK, 0)) test = 0;
    } else if (!strcmp(s, "quit")) {
      if (check) {
        execdir(0, 1);
        xexit();
      }

    // Remaining filters take an argument
    } else {
      arg = *++ss;
      if (!strcmp(s, "name") || !strcmp(s, "iname")
        || !strcmp(s, "wholename") || !strcmp(s, "iwholename")
        || !strcmp(s, "path") || !strcmp(s, "ipath")
        || !strcmp(s, "lname") || !strcmp(s, "ilname"))
      {
        int i = (*s == 'i'), is_path = (s[i] != 'n');
        char *path = 0, *name = new ? new->name : arg;

        // Handle path expansion and case flattening
        if (new && s[i] == 'l')
          name = path = xreadlinkat(dirtree_parentfd(new), new->name);
        else if (new && is_path) name = path = dirtree_path(new, 0);
        if (i) {
          if ((check || !new) && name) name = strlower(name);
          if (!new) dlist_add(&TT.argdata, name);
          else arg = ((struct double_list *)llist_pop(&argdata))->data;
        }

        if (check) {
          test = !fnmatch(arg, path ? name : basename(name),
            FNM_PATHNAME*(!is_path));
          if (i) free(name);
        }
        free(path);
      } else if (!CFG_TOYBOX_LSM_NONE && !strcmp(s, "context")) {
        if (check) {
          char *path = dirtree_path(new, 0), *context;

          if (lsm_get_context(path, &context) != -1) {
            test = !fnmatch(arg, context, 0);
            free(context);
          } else test = 0;
          free(path);
        }
      } else if (!strcmp(s, "perm")) {
        if (check) {
          int match_min = *arg == '-', match_any = *arg == '/';
          mode_t m1 = string_to_mode(arg+(match_min || match_any), 0),
                 m2 = new->st.st_mode & 07777;

          if (match_min || match_any) m2 &= m1;
          test = match_any ? !m1 || m2 : m1 == m2;
        }
      } else if (!strcmp(s, "type")) {
        if (check) {
          int types[] = {S_IFBLK, S_IFCHR, S_IFDIR, S_IFLNK, S_IFIFO,
                         S_IFREG, S_IFSOCK}, i;

          for (; *arg; arg++) {
            if (*arg == ',') continue;
            i = stridx("bcdlpfs", *arg);
            if (i<0) error_exit("bad -type '%c'", *arg);
            if ((new->st.st_mode & S_IFMT) == types[i]) break;
          }
          test = *arg;
        }

      } else if (strchr("acm", *s)
        && (!strcmp(s+1, "time") || !strcmp(s+1, "min")))
      {
        if (check) {
          time_t thyme = (int []){new->st.st_atime, new->st.st_ctime,
                                  new->st.st_mtime}[stridx("acm", *s)];
          int len = strlen(arg), uu, units = (s[1]=='m') ? 60 : 86400;

          if (len && -1!=(uu = stridx("dhms",tolower(arg[len-1])))) {
            arg = xstrdup(arg);
            arg[--len] = 0;
            units = (int []){86400, 3600, 60, 1}[uu];
          }
          test = compare_numsign(TT.now - thyme, units, arg);
          if (*ss != arg) free(arg);
        }
      } else if (!strcmp(s, "size")) {
        if (check) test = compare_numsign(new->st.st_size, -512, arg) &&
                          S_ISREG(new->st.st_mode);
      } else if (!strcmp(s, "links")) {
        if (check) test = compare_numsign(new->st.st_nlink, 0, arg);
      } else if (!strcmp(s, "inum")) {
        if (check) test = compare_numsign(new->st.st_ino, 0, arg);
      } else if (!strcmp(s, "mindepth") || !strcmp(s, "maxdepth")) {
        if (check) {
          struct dirtree *dt = new;
          int i = 0, d = atolx(arg);

          while ((dt = dt->parent)) i++;
          if (s[1] == 'i') {
            test = i >= d;
            if (i == d && not) recurse = 0;
          } else {
            test = i <= d;
            if (i == d && !not) recurse = 0;
          }
        }
      } else if (!strcmp(s, "user") || !strcmp(s, "group")
              || !strncmp(s, "newer", 5) || !strcmp(s, "samefile"))
      {
        int macoff[] = {offsetof(struct stat, st_mtim),
          offsetof(struct stat, st_atim), offsetof(struct stat, st_ctim)};
        struct {
          void *next, *prev;
          union {
            uid_t uid;
            gid_t gid;
            struct timespec tm;
            struct dev_ino di;
          };
        } *udl;
        struct stat st;

        if (!new) {
          if (arg) {
            udl = xmalloc(sizeof(*udl));
            dlist_add_nomalloc(&TT.argdata, (void *)udl);

            if (strchr("sn", *s)) {
              if (*s=='n' && s[5] && (s[7] || !strchr("Bmac", s[5]) || !strchr("tBmac", s[6])))
                goto error;
              if (*s=='s' || !s[5] || s[6]!='t') {
                xstat(arg, &st);
                if (*s=='s') udl->di.dev = st.st_dev, udl->di.ino = st.st_ino;
                else udl->tm = *(struct timespec *)(((char *)&st)
                               + macoff[!s[5] ? 0 : stridx("ac", s[6])+1]);
              } else if (s[6] == 't') {
                unsigned nano;

                xparsedate(arg, &(udl->tm.tv_sec), &nano, 1);
                udl->tm.tv_nsec = nano;
              }
            } else if (*s == 'u') udl->uid = xgetuid(arg);
            else udl->gid = xgetgid(arg);
          }
        } else {
          udl = (void *)llist_pop(&argdata);
          if (check) {
            if (*s == 'u') test = new->st.st_uid == udl->uid;
            else if (*s == 'g') test = new->st.st_gid == udl->gid;
            else if (*s == 's') test = same_dev_ino(&new->st, &udl->di);
            else {
              struct timespec *tm = (void *)(((char *)&new->st)
                + macoff[!s[5] ? 0 : stridx("ac", s[5])+1]);

              if (s[5] == 'B') test = 0;
              else test = (tm->tv_sec == udl->tm.tv_sec)
                ? tm->tv_nsec > udl->tm.tv_nsec
                : tm->tv_sec > udl->tm.tv_sec;
            }
          }
        }
      } else if (!strcmp(s, "exec") || !strcmp("ok", s)
              || !strcmp(s, "execdir") || !strcmp(s, "okdir"))
      {
        struct exec_range *aa;

        print++;

        // Initial argument parsing pass
        if (!new) {
          int len;

          // catch "-exec" with no args and "-exec \;"
          if (!arg || !strcmp(arg, ";")) error_exit("'%s' needs 1 arg", s);

          dlist_add_nomalloc(&TT.argdata, (void *)(aa = xzalloc(sizeof(*aa))));
          aa->argstart = ss;
          aa->curly = -1;

          // Record command line arguments to -exec
          for (len = 0; ss[len]; len++) {
            if (!strcmp(ss[len], ";")) break;
            else if (!strcmp(ss[len], "{}")) {
              aa->curly = len;
              if (ss[len+1] && !strcmp(ss[len+1], "+")) {
                aa->plus++;
                len++;
                break;
              }
            } else aa->argsize += sizeof(char *) + strlen(ss[len]) + 1;
          }
          if (!ss[len]) error_exit("-exec without %s",
            aa->curly!=-1 ? "\\;" : "{}");
          ss += len;
          aa->arglen = len;
          aa->dir = !!strchr(s, 'd');
          if (TT.topdir == -1) TT.topdir = xopenro(".");

        // collect names and execute commands
        } else {
          char *name;
          struct execdir_data *bb;

          // Grab command line exec argument list
          aa = (void *)llist_pop(&argdata);
          ss += aa->arglen;

          if (!check) goto cont;
          // name is always a new malloc, so we can always free it.
          name = aa->dir ? xstrdup(new->name) : dirtree_path(new, 0);

          if (*s == 'o') {
            fprintf(stderr, "[%s] %s", arg, name);
            if (!(test = yesno(0))) {
              free(name);
              goto cont;
            }
          }

          // Add next name to list (global list without -dir, local with)
          bb = aa->execdir ? aa->execdir : &aa->exec;
          dlist_add(&bb->names, name);
          bb->namecount++;

          // -exec + collates and saves result in exitval
          if (aa->plus) {
            // Mark entry so COMEAGAIN can call flush_exec() in parent.
            // This is never a valid pointer value for prev to have otherwise
            // Done here vs argument parsing pass so it's after dlist_terminate
            aa->prev = (void *)1;

            // Flush if the child's environment space gets too large.
            // Linux caps individual arguments/variables at 131072 bytes,
            // so this counter can't wrap.
            if ((aa->plus += sizeof(char *)+strlen(name)+1) > TT.max_bytes) {
              aa->plus = 1;
              toys.exitval |= flush_exec(new, aa);
            }
          } else test = !flush_exec(new, aa);
        }

        // Argument consumed, skip the check.
        goto cont;
      } else if (!strcmp(s, "printf")) {
        char *fmt, *ff, next[32], buf[64], ch;
        long ll;
        int len;

        print++;
        if (check) for (fmt = arg; *fmt; fmt++) {
          // Print the parts that aren't escapes
          if (*fmt == '\\') {
            unsigned u;

            if (fmt[1] == 'c') break;
            if ((u = unescape2(&fmt, 0))<128) putchar(u);
            else printf("%.*s", (int)wcrtomb(buf, u, 0), buf);
            fmt--;
          } else if (*fmt != '%') putchar(*fmt);
          else if (*++fmt == '%') putchar('%');
          else {
            fmt = next_printf(ff = fmt-1, 0);
            if ((len = fmt-ff)>28) error_exit("bad %.*s", len+1, ff);
            memcpy(next, ff, len);
            ff = 0;
            ch = *fmt;

            // long long is its own stack size on LP64, so handle separately
            if (ch == 'i' || ch == 's') {
              strcpy(next+len, "lld");
              printf(next, (ch == 'i') ? (long long)new->st.st_ino
                : (long long)new->st.st_size);
            } else {

              // LP64 says these are all a single "long" argument to printf
              strcpy(next+len, "s");
              if (ch == 'G') next[len] = 'd', ll = new->st.st_gid;
              else if (ch == 'm') next[len] = 'o', ll = new->st.st_mode&~S_IFMT;
              else if (ch == 'U') next[len] = 'd', ll = new->st.st_uid;
              else if (ch == 'f') ll = (long)new->name;
              else if (ch == 'g') ll = (long)getgroupname(new->st.st_gid);
              else if (ch == 'u') ll = (long)getusername(new->st.st_uid);
              else if (ch == 'l') {
                ll = (long)(ff = xreadlinkat(dirtree_parentfd(new), new->name));
                if (!ll) ll = (long)"";
              } else if (ch == 'M') {
                mode_to_string(new->st.st_mode, buf);
                ll = (long)buf;
              } else if (ch == 'P') {
                ch = *TT.start;
                *TT.start = 0;
                ll = (long)(ff = dirtree_path(new, 0));
                *TT.start = ch;
              } else if (ch == 'p') ll = (long)(ff = dirtree_path(new, 0));
              else if (ch == 'T') {
                if (*++fmt!='@') error_exit("bad -printf %%T: %%T%c", *fmt);
                sprintf(buf, "%lld.%ld", (long long)new->st.st_mtim.tv_sec,
                             new->st.st_mtim.tv_nsec);
                ll = (long)buf;
              } else if (ch == 'Z') {
                char *path = dirtree_path(new, 0);

                ll = (lsm_get_context(path, &ff) != -1) ? (long)ff : (long)"?";
                free(path);
              } else error_exit("bad -printf %%%c", ch);

              printf(next, ll);
              free(ff);
            }
          }
        }
      } else goto error;

      // This test can go at the end because we do a syntax checking
      // pass first. Putting it here gets the error message (-unknown
      // vs -known noarg) right.
      if (!check && !arg) error_exit("'%s' needs 1 arg", s-1);
    }
cont:
    // Apply pending "!" to result
    if (active && not) test = !test;
    not = 0;
  }

  if (new) {
    // If there was no action, print
    if (!print && test) do_print(new, '\n');

    if (S_ISDIR(new->st.st_mode)) execdir(new, 0);

  } else dlist_terminate(TT.argdata);

  return recurse;

error:
  if (!*ss) --ss;
  error_exit("bad arg '%s'", *ss);
}

void find_main(void)
{
  int i, len;
  char **ss = (char *[]){"."};

  TT.topdir = -1;
  TT.max_bytes = sysconf(_SC_ARG_MAX) - environ_bytes();

  // Distinguish paths from filters
  for (len = 0; toys.optargs[len]; len++)
    if (*toys.optargs[len] && strchr("-!(", *toys.optargs[len])) break;
  TT.filter = toys.optargs+len;

  // use "." if no paths
  if (len) ss = toys.optargs;
  else len = 1;

  // first pass argument parsing, verify args match up, handle "evaluate once"
  TT.now = time(0);
  do_find(0);

  // Loop through paths
  for (i = 0; i < len; i++)
    dirtree_flagread(ss[i],
      DIRTREE_STATLESS|(DIRTREE_SYMFOLLOW*!!(toys.optflags&(FLAG_H|FLAG_L))),
      do_find);

  execdir(0, 1);

  if (CFG_TOYBOX_FREE) {
    close(TT.topdir);
    llist_traverse(TT.argdata, free);
  }
}
