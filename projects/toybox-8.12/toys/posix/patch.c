/* patch.c - Apply a "universal" diff.
 *
 * Copyright 2007 Rob Landley <rob@landley.net>
 *
 * see http://opengroup.org/onlinepubs/9699919799/utilities/patch.html
 * (But only does -u, because who still cares about "ed"?)
 *
 * TODO:
 * -b backup
 * -N ignore already applied
 * -D define wrap #ifdef and #ifndef around changes
 * -o outfile output here instead of in place
 * -r rejectfile write rejected hunks to this file
 * -E remove empty files --remove-empty-files
 * git syntax (rename, etc)

USE_PATCH(NEWTOY(patch, ">2(no-backup-if-mismatch)(dry-run)F#g#fulp#v(verbose)@d:i:Rs(quiet)[!sv]", TOYFLAG_USR|TOYFLAG_BIN))

config PATCH
  bool "patch"
  default y
  help
    usage: patch [-Rlsuv] [-d DIR] [-i FILE] [-p DEPTH] [-F FUZZ] [--dry-run] [FILE [PATCH]]

    Apply a unified diff to one or more files.

    -d	Modify files in DIR
    -F	Fuzz factor (number of non-matching context lines allowed per hunk)
    -i	Input patch from FILE (default=stdin)
    -l	Loose match (ignore whitespace)
    -p	Number of '/' to strip from start of file paths (default=all)
    -R	Reverse patch
    -s	Silent except for errors
    -v	Verbose (-vv to see decisions)
    --dry-run Don't change files, just confirm patch applies

    Only handles "unified" diff format (-u is assumed and ignored). Only
    modifies files when all hunks to that file apply. Prints failed hunks
    to stderr, and exits with nonzero status if any hunks fail.

    Files compared against /dev/null (or with a date <= the unix epoch) are
    created/deleted as appropriate. Default -F value is the number of
    leading/trailing context lines minus one (usually 2).
*/

#define FOR_patch
#include "toys.h"

GLOBALS(
  char *i, *d;
  long v, p, g, F;

  void *current_hunk;
  long oldline, oldlen, newline, newlen, linenum, outnum;
  int context, state, filein, fileout, filepatch, hunknum;
  char *tempname;
)

// TODO xgetline() instead, but replace_tempfile() wants fd...
char *get_line(int fd)
{
  char c, *buf = 0;
  long len = 0;

  for (;;) {
    if (1>read(fd, &c, 1)) break;
    if (!(len & 63)) buf=xrealloc(buf, len+65);
    if ((buf[len++]=c) == '\n') break;
  }
  if (buf) {
    buf[len]=0;
    if (buf[--len]=='\n') buf[len]=0;
  }

  return buf;
}

// Dispose of a line of input, either by writing it out or discarding it.

// state < 2: just free
// state = 2: write whole line to stderr
// state = 3: write whole line to fileout
// state > 3: write line+1 to fileout when *line != state

static void do_line(void *data)
{
  struct double_list *dlist = data;

  TT.outnum++;
  if (TT.state>1)
    if (0>dprintf(TT.state==2 ? 2 : TT.fileout,"%s\n",dlist->data+(TT.state>3)))
      perror_exit("write");

  llist_free_double(data);
}

static void finish_oldfile(void)
{
  if (TT.tempname) replace_tempfile(TT.filein, TT.fileout, &TT.tempname);
  TT.fileout = TT.filein = -1;
}

static void fail_hunk(void)
{
  if (!TT.current_hunk) return;

  fprintf(stderr, "Hunk %d FAILED %ld/%ld.\n",
      TT.hunknum, TT.oldline, TT.newline);
  toys.exitval = 1;

  // If we got to this point, we've seeked to the end.  Discard changes to
  // this file and advance to next file.

  TT.state = 2;
  llist_traverse(TT.current_hunk, do_line);
  TT.current_hunk = 0;
  if (!FLAG(dry_run)) delete_tempfile(TT.filein, TT.fileout, &TT.tempname);
  TT.state = 0;
}

// Compare ignoring whitespace. Just returns 0/1, no > or <
static int loosecmp(char *aa, char *bb)
{
  int a = 0, b = 0;

  for (;;) {
    while (isspace(aa[a])) a++;
    while (isspace(bb[b])) b++;
    if (aa[a] != bb[b]) return 1;
    if (!aa[a]) return 0;
    a++, b++;
  }
}

// Given a hunk of a unified diff, make the appropriate change to the file.
// This does not use the location information, but instead treats a hunk
// as a sort of regex. Copies data from input to output until it finds
// the change to be made, then outputs the changed data and returns.
// (Finding EOF first is an error.) This is a single pass operation, so
// multiple hunks must occur in order in the file.

static int apply_one_hunk(void)
{
  struct double_list *plist, *buf = 0, *check = 0;
  int matcheof, trail = 0, allfuzz = 0, fuzz, ii;
  int (*lcmp)(char *aa, char *bb) = FLAG(l) ? (void *)loosecmp : (void *)strcmp;
  long backwarn = 0;
  char *data = toybuf;

  if (TT.v>1) printf("START %d\n", TT.hunknum);

  // Match EOF if there aren't as many ending context lines as beginning
  dlist_terminate(TT.current_hunk);
  for (fuzz = 0, plist = TT.current_hunk; plist; plist = plist->next) {
    char *s = plist->data, c = *s;

    if (c==' ') trail++;
    else trail = 0;

    // Only allow fuzz if 2 context lines have multiple nonwhitespace chars.
    // avoids the "all context was blank or } lines" issue. Removed lines
    // count as context since they're matched.
    if (c==' ' || c=="-+"[FLAG(R)]) {
      while (isspace(*++s));
      if (*s && s[1] && !isspace(s[1])) fuzz++;
    }
  }
  matcheof = !trail || trail < TT.context;
  if (FLAG(F) && !TT.F) fuzz = 0;
  if (fuzz>1) allfuzz = TT.F ? : TT.context ? TT.context-1 : 0;

  // Loop through input data searching for this hunk. Match all context
  // lines and lines to be removed until we've found end of complete hunk.
  plist = TT.current_hunk;
  fuzz = 0;
  for (;;) {
    if (data) {
      data = get_line(TT.filein);
      check = data ? dlist_add(&buf, data) : 0;
      TT.linenum++;
    }
    if (TT.v>1) printf("READ[%ld] %s\n", TT.linenum, data ? : "(NULL)");

    // Compare buffered line(s) with expected lines of hunk. Match can fail
    // because next line doesn't match, or because we hit end of a hunk that
    // needed EOF and this isn't EOF.
    for (;;) {
      // Find hunk line to match (skip added lines) and detect reverse matches
      while (plist && *plist->data == "+-"[FLAG(R)]) {
        // TODO: proper backwarn = full hunk applies in reverse, not just 1 line
        if (data) {
          ii = strcspn(data, " \t");
          if (data[ii+!!data[ii]] && !lcmp(data, plist->data+1))
            backwarn = TT.linenum;
        }
        plist = plist->next;
      }
      if (TT.v>1 && plist)
        printf("HUNK %s\nLINE %s\n", plist->data+1, check ? check->data : "");

      // End of hunk?
      if (!plist) {
        if (TT.v>1) printf("END OF HUNK\n");
        if (matcheof == !data) goto out;

      // Compare line and handle match
      } else if (check && !lcmp(check->data, plist->data+1)) {
        if (TT.v>1) printf("MATCH\n");
handle_match:
        plist = plist->next;
        if ((check = check->next) == buf) {
          if (plist || matcheof) break;
          goto out;
        } else continue;
      }

      // Did we hit EOF?
      if (!data) {
        if (TT.v>1) printf("EOF\n");
        if (backwarn && !FLAG(s))
          fprintf(stderr, "Possibly reversed hunk %d at %ld\n",
              TT.hunknum, backwarn);

        // File ended before we found a place for this hunk.
        fail_hunk();
        goto done;
      }
      if (TT.v>1) printf("NOT MATCH\n");

      // Match failed: can we fuzz it?
      if (plist && *plist->data == ' ' && fuzz<allfuzz) {
        fuzz++;
        if (TT.v>1) printf("FUZZ %d %s\n", fuzz, check->data);
        goto handle_match;
      }

      // If this hunk must match start of file, fail if it didn't.
      if (!TT.context || trail>TT.context) {
        fail_hunk();
        goto done;
      }

      // Write out first line of buffer and recheck rest for new match.
      TT.state = 3;
      if (TT.v>1) printf("WRITE %s\n", buf->data);
      do_line(check = dlist_pop(&buf));
      plist = TT.current_hunk;
      fuzz = 0;

      // If end of the buffer without finishing a match, read more lines.
      if (!buf) break;
      check = buf;
    }
  }
out:
  if (TT.v) xprintf("Hunk #%d succeeded at %ld.\n", TT.hunknum, TT.linenum);
  // We have a match.  Emit changed data.
  TT.state = "-+"[FLAG(R)];
  while ((plist = dlist_pop(&TT.current_hunk))) {
    if (TT.state == *plist->data || *plist->data == ' ') {
      if (*plist->data == ' ') dprintf(TT.fileout, "%s\n", buf->data);
      llist_free_double(dlist_pop(&buf));
    } else dprintf(TT.fileout, "%s\n", plist->data+1);
    llist_free_double(plist);
  }
  TT.current_hunk = 0;
  TT.state = 1;
done:
  llist_traverse(buf, do_line);

  return TT.state;
}

// read a filename that has been quoted or escaped
static char *unquote_file(char *filename)
{
  char *s = filename, *t, *newfile;

  // Return copy of file that wasn't quoted
  if (*s++ != '"' || !*s) return xstrdup(filename);

  // quoted and escaped filenames are larger than the original
  for (t = newfile = xmalloc(strlen(s) + 1); *s != '"'; s++) {
    if (!s[1]) error_exit("bad %s", filename);

    // don't accept escape sequences unless the filename is quoted
    if (*s != '\\') *t++ = *s;
    else if (*++s >= '0' && *s < '8') {
      *t++ = strtoul(s, &s, 8);
      s--;
    } else {
      if (!(*t = unescape(*s))) *t = *s;;
      t++;
    }
  }
  *t = 0;

  return newfile;
}

// Read a patch file and find hunks, opening/creating/deleting files.
// Call apply_one_hunk() on each hunk.

// state 0: Not in a hunk, look for +++.
// state 1: Found +++ file indicator, look for @@
// state 2: In hunk: counting initial context lines
// state 3: In hunk: getting body

void patch_main(void)
{
  int state = 0, patchlinenum = 0, strip = 0;
  char *oldname = 0, *newname = 0;

  if (toys.optc == 2) TT.i = toys.optargs[1];
  if (TT.i) TT.filepatch = xopenro(TT.i);
  TT.filein = TT.fileout = -1;

  if (TT.d) xchdir(TT.d);

  // Loop through the lines in the patch file (-i or stdin) collecting hunks
  for (;;) {
    char *patchline;

    if (!(patchline = get_line(TT.filepatch))) break;

    // Other versions of patch accept damaged patches, so we need to also.
    if (strip || !patchlinenum++) {
      int len = strlen(patchline);
      if (len && patchline[len-1] == '\r') {
        if (!strip && !FLAG(s)) fprintf(stderr, "Removing DOS newlines\n");
        strip = 1;
        patchline[len-1] = 0;
      }
    }
    if (!*patchline) {
      free(patchline);
      patchline = xstrdup(" ");
    }

    // Are we assembling a hunk?
    if (state >= 2) {
      if (*patchline==' ' || *patchline=='+' || *patchline=='-') {
        dlist_add((void *)&TT.current_hunk, patchline);

        if (*patchline != '+') TT.oldlen--;
        if (*patchline != '-') TT.newlen--;

        // Context line?
        if (*patchline==' ' && state==2) TT.context++;
        else state=3;

        // If we've consumed all expected hunk lines, apply the hunk.
        if (!TT.oldlen && !TT.newlen) state = apply_one_hunk();
      } else {
        dlist_terminate(TT.current_hunk);
        fail_hunk();
        state = 0;
      }
      continue;
    }

    // Open a new file?
    if (!strncmp("--- ", patchline, 4) || !strncmp("+++ ", patchline, 4)) {
      char *s, **name = &oldname;
      int i;

      if (*patchline == '+') {
        name = &newname;
        state = 1;
      }

      free(*name);
      finish_oldfile();

      // Trim date from end of filename (if any). Date<=epoch means delete.
      for (s = patchline+4; *s && *s!='\t'; s++);
      i = atoi(s);
      if (i>1900 && i<=1970) *name = xstrdup("/dev/null");
      else {
        *s = 0;
        *name = unquote_file(patchline+4);
      }

      // We defer actually opening the file because svn produces broken
      // patches that don't signal they want to create a new file the
      // way the patch man page says, so you have to read the first hunk
      // and _guess_.

    // Start a new hunk?  Usually @@ -oldline,oldlen +newline,newlen @@
    // but a missing ,value means the value is 1.
    } else if (state == 1 && !strncmp("@@ -", patchline, 4)) {
      int i;
      char *s = patchline+4;

      // Read oldline[,oldlen] +newline[,newlen]

      TT.oldlen = TT.newlen = 1;
      TT.oldline = strtol(s, &s, 10);
      if (*s == ',') TT.oldlen=strtol(s+1, &s, 10);
      TT.newline = strtol(s+2, &s, 10);
      if (*s == ',') TT.newlen = strtol(s+1, &s, 10);

      TT.context = 0;
      state = 2;

      // If this is the first hunk, open the file.
      if (TT.filein == -1) {
        int oldsum, newsum, del = 0;
        char *name;

        oldsum = TT.oldline + TT.oldlen;
        newsum = TT.newline + TT.newlen;

        // If an original file was provided on the command line, it overrides
        // *all* files mentioned in the patch, not just the first.
        if (toys.optc) {
          char **which = FLAG(R) ? &oldname : &newname;

          free(*which);
          *which = xstrdup(toys.optargs[0]);
          // The supplied path should be taken literally with or without -p.
          toys.optflags |= FLAG_p;
          TT.p = 0;
        }

        name = FLAG(R) ? oldname : newname;

        // We're deleting oldname if new file is /dev/null (before -p)
        // or if new hunk is empty (zero context) after patching
        if (!strcmp(name, "/dev/null") || !(FLAG(R) ? oldsum : newsum)) {
          name = FLAG(R) ? newname : oldname;
          del++;
        }

        // handle -p path truncation.
        for (i = 0, s = name; *s;) {
          if (FLAG(p) && TT.p == i) break;
          if (*s++ != '/') continue;
          while (*s == '/') s++;
          name = s;
          i++;
        }

        if (del) {
          if (!FLAG(s)) printf("removing %s\n", name);
          if (!FLAG(dry_run)) xunlink(name);
          state = 0;
        // If we've got a file to open, do so.
        } else if (!FLAG(p) || i <= TT.p) {
          // If the old file was null, we're creating a new one.
          if ((!strcmp(oldname, "/dev/null") || !oldsum) && access(name, F_OK))
          {
            if (!FLAG(s)) printf("creating %s\n", name);
            if (FLAG(dry_run)) TT.filein = xopen("/dev/null", O_RDWR);
            else {
              if (mkpath(name)) perror_exit("mkpath %s", name);
              TT.filein = xcreate(name, O_CREAT|O_EXCL|O_RDWR, 0666);
            }
          } else {
            if (!FLAG(s)) printf("patching %s\n", name);
            TT.filein = xopenro(name);
          }
          if (FLAG(dry_run)) TT.fileout = xopen("/dev/null", O_RDWR);
          else TT.fileout = copy_tempfile(TT.filein, name, &TT.tempname);
          TT.linenum = TT.outnum = TT.hunknum = 0;
        }
      }

      TT.hunknum++;

      continue;
    }

    // If we didn't continue above, discard this line.
    free(patchline);
  }

  finish_oldfile();

  if (CFG_TOYBOX_FREE) {
    close(TT.filepatch);
    free(oldname);
    free(newname);
  }
}
