/* grep.c - show lines matching regular expressions
 *
 * Copyright 2013 CE Strake <strake888 at gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/grep.html
 *
 * Posix doesn't even specify -r: too many deviations to document.
 * TODO: -i is only ascii case insensitive, not unicode.

USE_GREP(NEWTOY(grep, "(line-buffered)(color):;(exclude-dir)*S(exclude)*M(include)*ZzEFHIab(byte-offset)h(no-filename)ino(only-matching)rRsvwc(count)L(files-without-match)l(files-with-matches)q(quiet)(silent)e*f*C#B#A#m#x[!wx][!EF]", TOYFLAG_BIN|TOYFLAG_ARGFAIL(2)|TOYFLAG_LINEBUF|TOYFLAG_AUTOCONF))
USE_EGREP(OLDTOY(egrep, grep, TOYFLAG_BIN|TOYFLAG_ARGFAIL(2)|TOYFLAG_LINEBUF|TOYFLAG_AUTOCONF))
USE_FGREP(OLDTOY(fgrep, grep, TOYFLAG_BIN|TOYFLAG_ARGFAIL(2)|TOYFLAG_LINEBUF|TOYFLAG_AUTOCONF))

config GREP
  bool "grep"
  default y
  help
    usage: grep [-abcEFHhIiLlnoqrsvwxZz] [-ABC NUM] [-m MAX] [-e REGEX]... [-MS PATTERN]... [-f REGFILE]... [FILE]...

    Show lines matching regular expressions. If no -e, first argument is
    regular expression to match. With no files (or "-" filename) read stdin.
    Returns 0 if matched, 1 if no match found, 2 for command errors.

    -e  Regex(es) to match.       -f  File(s) of regexes to match (1 per line).

    file search:
    -r  Recurse into subdirs     -R  Recurse following symlinks
    -M  Match files (--include)  -S  Skip files (--exclude)
    -I  Ignore binary files      --exclude-dir=PATTERN  Skip directories

    match type:
    -A  Show NUM lines after     -B  Show NUM lines before match
    -C  NUM lines context (A+B)  -E  extended regex syntax
    -F  fixed (literal match)    -a  always text (not binary)
    -i  case insensitive         -m  match MAX many lines
    -v  invert match             -w  whole word (implies -E)
    -x  whole line               -z  input NUL terminated

    display modes: (default: matched line)
    -L  filenames with no match  -Z  output is NUL terminated
    -c  count of matching lines  -l  filenames with a match
    -o  only matching part       -q  quiet (errors only)
    -s  silent (no error msg)

    output prefix (default: filename if checking more than 1 file)
    -H  force filename           -b  byte offset of match
    -h  hide filename            -n  line number of match

config EGREP
  bool
  default y
  depends on GREP

config FGREP
  bool
  default y
  depends on GREP
*/

#define FOR_grep
#include "toys.h"

GLOBALS(
  long m, A, B, C;
  struct arg_list *f, *e, *M, *S, *exclude_dir;
  char *color;

  char *purple, *cyan, *red, *green, *grey;
  struct double_list *reg;
  int found, tried, delim;
  struct arg_list **fixed;
)

struct reg {
  struct reg *next, *prev;
  int rc;
  regex_t r;
  regmatch_t m;
};

static void numdash(long num, char dash)
{
  printf("%s%ld%s%c", TT.green, num, TT.cyan, dash);
}

// Emit line with various potential prefixes and delimiter
static void outline(char *line, char dash, char *name, long lcount, long bcount,
  unsigned trim)
{
  if (!trim && FLAG(o)) return;
  if (name && FLAG(H)) printf("%s%s%s%c", TT.purple, name, TT.cyan, dash);
  if (FLAG(c)) xprintf("%s%ld%c", TT.grey, lcount, TT.delim);
  else if (lcount && FLAG(n)) numdash(lcount, dash);
  if (bcount && FLAG(b)) numdash(bcount-1, dash);
  if (line) {
    if (FLAG(color)) xputsn(FLAG(o) ? TT.red : TT.grey);
    // support embedded NUL bytes in output
    xputsl(line, trim);
    xputc(TT.delim);
  }
}

static int matchw(char *line, char *start, long so, long eo)
{
  if (FLAG(w)) {
    if (so+(start-line)) if (isalnum(start[so-1]) || start[so-1]=='_') return 0;
    if (isalnum(start[eo]) || start[eo]=='_') return 0;
  }

  return 1;
}

// Show matches in one file
static void do_grep(int fd, char *name)
{
  long lcount = 0, mcount = 0, offset = 0, after = 0, before = 0, new = 1;
  struct double_list *dlb = 0;
  char *bars = 0;
  FILE *file;
  int bin = 0;

  if (!FLAG(r)) TT.tried++;
  if (!fd) name = "(standard input)";

  // Only run binary file check on lseekable files.
  if (!FLAG(a) && !lseek(fd, 0, SEEK_CUR)) {
    char buf[256];
    int len, i = 0;
    unsigned wc;

    // If the first 256 bytes don't parse as utf8, call it binary.
    if (0<(len = read(fd, buf, 256))) {
      lseek(fd, -len, SEEK_CUR);
      while (i<len) {
        bin = utf8towc(&wc, buf+i, len-i);
        if (bin == -2) i = len;
        if (bin<1) break;
        i += bin;
      }
      bin = i!=len;
    }
    if (bin && FLAG(I)) return;
  }

  if (!(file = fdopen(fd, "r"))) return perror_msg_raw(name);

  // Loop through lines of input
  for (;;) {
    char *line = 0, *start, *ss, *pp;
    struct reg *shoe;
    size_t ulen;
    long len;
    int matched = 0, rc = 1, move = 0, ii;

    // get next line, check and trim delimiter
    lcount++;
    errno = 0;
    ulen = len = getdelim(&line, &ulen, TT.delim, file);
    if (len == -1 && errno) perror_msg_raw(name);
    if (len<1) break;
    if (line[ulen-1] == TT.delim) line[--ulen] = 0;

    // Prepare for next line
    start = line;
    for (shoe = (void *)TT.reg; shoe; shoe = shoe->next) shoe->rc = 0;

    // Loop to handle multiple matches in same line
    if (new) do {
      regmatch_t *mm = (void *)toybuf;
      struct arg_list *seek;

      mm->rm_so = mm->rm_eo = 0;
      rc = 1;

      // Handle "fixed" (literal) matches (if any)
      if (TT.e) for (ss = start; ss-line<=ulen; ss++) {
        ii = FLAG(i) ? toupper(*ss) : *ss;
        for (seek = TT.fixed[ii]; seek; seek = seek->next) {
          if (*(pp = seek->arg)=='^' && !FLAG(F)) {
            if (ss!=start) continue;
            pp++;
          }
          for (ii = 0; pp[ii] && ss[ii]; ii++) {
            if (!FLAG(F)) {
              if (pp[ii]=='.') continue;
              if (pp[ii]=='\\' && pp[ii+1]) pp++;
              else if (pp[ii]=='$' && !pp[ii+1]) break;
            }
            if (FLAG(i)) {
              if (toupper(pp[ii])!=toupper(ss[ii])) break;
            } else if (pp[ii]!=ss[ii]) break;
          }
          if (pp[ii] && (pp[ii]!='$' || pp[ii+1] || ss[ii])) continue;
          mm->rm_eo = (mm->rm_so = ss-start)+ii;
          if (!matchw(line, start, mm->rm_so, mm->rm_eo)) continue;
          rc = 0;

          goto got;
        }
        if (FLAG(x)) break;
      }

got:
      // Handle regex matches (if any)
      for (shoe = (void *)TT.reg; shoe; shoe = shoe->next) {
        // Do we need to re-check this regex?
        if (!shoe->rc) {
          shoe->m.rm_so -= move;
          shoe->m.rm_eo -= move;
          if (!matched || shoe->m.rm_so<0)
            shoe->rc = regexec0(&shoe->r, start, ulen-(start-line), 1,
                                &shoe->m, start==line ? 0 : REG_NOTBOL);
        }

        if (!matchw(line, start, shoe->m.rm_so, shoe->m.rm_eo)) continue;
        // If we got a match, is it a _better_ match?
        if (!shoe->rc && (rc || shoe->m.rm_so < mm->rm_so ||
            (shoe->m.rm_so == mm->rm_so && shoe->m.rm_eo >= mm->rm_eo)))
        {
          mm = &shoe->m;
          rc = 0;
        }
      }

      if (!rc && FLAG(o) && !mm->rm_eo && ulen>start-line) {
        move = 1;
        continue;
      }

      if (!rc && FLAG(x) && (mm->rm_so || ulen-(start-line)!=mm->rm_eo)) rc = 1;

      if (FLAG(v)) {
        if (FLAG(o)) {
          if (rc) mm->rm_eo = ulen-(start-line);
          else if (!mm->rm_so) {
            move = mm->rm_eo;
            continue;
          } else mm->rm_eo = mm->rm_so;
        } else {
          if (!rc) break;
          mm->rm_eo = ulen-(start-line);
        }
        mm->rm_so = 0;
      } else if (rc) break;

      // At least one line we didn't print since match while -ABC active
      if (bars) {
        xputs(bars);
        bars = 0;
      }
      matched++;
      TT.found = 1;

      // Are we NOT showing the matching text?
      if (FLAG(q)) {
        toys.exitval = 0;
        xexit();
      }
      if (FLAG(L) || FLAG(l)) {
        if (FLAG(l)) xprintf("%s%c", name, '\n'*!FLAG(Z));
        free(line);
        fclose(file);
        return;
      }

      if (!FLAG(c)) {
        long bcount = 1 + offset + (start-line) + (FLAG(o) ? mm->rm_so : 0);

        if (bin) printf("Binary file %s matches\n", name);
        else if (FLAG(o))
          outline(start+mm->rm_so, ':', name, lcount, bcount,
                  mm->rm_eo-mm->rm_so);
        else {
          while (dlb) {
            struct double_list *dl = dlist_pop(&dlb);
            unsigned *uu = (void *)(dl->data+(strlen(dl->data)|3)+1);

            outline(dl->data, '-', name, lcount-before, uu[0]+1, uu[1]);
            free(dl->data);
            free(dl);
            before--;
          }

          if (matched==1)
            outline(FLAG(color) ? 0 : line, ':', name, lcount, bcount, ulen);
          if (FLAG(color)) {
            xputsn(TT.grey);
            if (mm->rm_so) xputsl(start, mm->rm_so);
            xputsn(TT.red);
            xputsl(start+mm->rm_so, mm->rm_eo-mm->rm_so);
          }

          if (TT.A) after = TT.A+1;
        }
      }

      if (mm->rm_so == (move = mm->rm_eo)) break;
    } while (*(start += move));
    offset += len;

    if (matched) {
      // Finish off pending line color fragment.
      if (FLAG(color) && !FLAG(o)) {
        xputsn(TT.grey);
        if (ulen > start-line) xputsl(start, ulen-(start-line));
        xputc(TT.delim);
      }
      mcount++;
    } else {
      int discard = (after || TT.B);

      if (after && --after) {
        outline(line, '-', name, lcount, 0, ulen);
        discard = 0;
      }
      if (discard && TT.B) {
        unsigned *uu, ul = (ulen|3)+1;

        line = xrealloc(line, ul+8);
        uu = (void *)(line+ul);
        uu[0] = offset-len;
        uu[1] = ulen;
        dlist_add(&dlb, line);
        line = 0;
        if (++before>TT.B) {
          struct double_list *dl;

          dl = dlist_pop(&dlb);
          free(dl->data);
          free(dl);
          before--;
        } else discard = 0;
      }
      // If we discarded a line while displaying context, show bars before next
      // line (but don't show them now in case that was last match in file)
      if (discard && mcount) bars = "--";
    }
    free(line);

    if (FLAG(m) && mcount >= TT.m) {
      if (!after) break;
      new = 0;
    }
  }

  if (FLAG(L)) xprintf("%s%c", name, TT.delim);
  else if (FLAG(c)) outline(0, ':', name, mcount, 0, 1);

  // loopfiles will also close the fd, but this frees an (opaque) struct.
  fclose(file);
  llist_traverse(dlb, llist_free_double);
}

static int lensort(struct arg_list **a, struct arg_list **b)
{
  long la = strlen((*a)->arg), lb = strlen((*b)->arg);

  if (la<lb) return -1;
  if (la>lb) return 1;

  return 0;
}

static void parse_regex(void)
{
  struct arg_list *al, *new, *list = NULL, **last;
  char *s, *ss, *special = "\\.^$[()|*+?{";
  int len, ii, key;

  // Add all -f lines to -e list. (Yes, this is leaking allocation context for
  // exit to free. Not supporting nofork for this command any time soon.)
  al = TT.f ? TT.f : TT.e;
  while (al) {
    if (TT.f) {
      if (!*(s = xreadfile(al->arg, 0, 0))) {
        free(s);
        s = 0;
      } else if (*(ss = s+strlen(s)-1)=='\n') *ss = 0;
    } else s = al->arg;

    // Advance, when we run out of -f switch to -e.
    al = al->next;
    if (!al && TT.f) {
      TT.f = 0;
      al = TT.e;
    }
    if (!s) continue;

    // NOTE: even with -z, -f is still \n delimited. Blank line = match all
    // Split lines at \n, add individual lines to new list.
    do {
      if ((ss = strchr(s, '\n'))) *(ss++) = 0;
      new = xmalloc(sizeof(struct arg_list));
      new->next = list;
      new->arg = s;
      list = new;
      s = ss;
    } while (s);
  }
  TT.e = list;

  // Convert to regex where appropriate
  for (last = &TT.e; *last;) {
    // Can we use the fast path?
    s = (*last)->arg;
    if ('.'!=*s && !FLAG(F) && strcmp(s, "^$")) for (; *s; s++) {
      if (*s=='\\') {
        if (!s[1] || !strchr(special, *++s)) break;
        if (!FLAG(E) && *s=='(') break;
      } else if (*s>127 || strchr(special+4, *s)) break;
    }

    // Leave entry in fast path (literal-ish match) or move to slow path (regex)
    if (!*s || FLAG(F)) last = &((*last)->next);
    else {
      struct reg *shoe;

      dlist_add_nomalloc(&TT.reg, (void *)(shoe = xmalloc(sizeof(struct reg))));
      xregcomp(&shoe->r, (*last)->arg, REG_EXTENDED*FLAG(E)|REG_ICASE*FLAG(i));
      al = *last;
      *last = (*last)->next;
      free(al);
    }
  }
  dlist_terminate(TT.reg);

  // Sort fast path patterns into buckets by first character
  for (al = TT.e; al; al = new) {
    new = al->next;
    if (FLAG(F)) key = 0;
    else {
      key = '^'==*al->arg;
      if ('\\'==al->arg[key]) key++;
      else if ('$'==al->arg[key] && !al->arg[key+1]) key++;
    }
    key = al->arg[key];
    if (FLAG(i)) key = toupper(key);
    al->next = TT.fixed[key];
    TT.fixed[key] = al;
  }

  // Sort each fast path pattern set by length so first hit is longest match
  // Zero length matches aren't sorted, instead appended to every list.
  if (TT.e) for (key = 1; key<256; key++) {
    if (!TT.fixed[key]) {
      TT.fixed[key] = *TT.fixed;
      continue;
    }
    for (len = 0, al = TT.fixed[key]; al; al = al->next) len++;
    last = xmalloc(len*sizeof(void *));
    for (len = 0, al = TT.fixed[key]; al; al = al->next) last[len++] = al;
    qsort(last, len, sizeof(void *), (void *)lensort);
    for (ii = 0; ii<len; ii++) last[ii]->next = ii ? last[ii-1] : *TT.fixed;
    TT.fixed[key] = last[len-1];
    free(last);
  }
}

static int do_grep_r(struct dirtree *new)
{
  struct arg_list *al;
  char *name;

  if (!new->parent) TT.tried++;
  if (!dirtree_notdotdot(new)) return 0;
  if (S_ISDIR(new->st.st_mode)) {
    for (al = TT.exclude_dir; al; al = al->next)
      if (!fnmatch(al->arg, new->name, 0)) return 0;
    return DIRTREE_RECURSE|DIRTREE_SYMFOLLOW*FLAG(R);
  }
  if (TT.S || TT.M) {
    for (al = TT.S; al; al = al->next)
      if (!fnmatch(al->arg, new->name, 0)) return 0;

    if (TT.M) {
      for (al = TT.M; al; al = al->next)
        if (!fnmatch(al->arg, new->name, 0)) break;

      if (!al) return 0;
    }
  }

  // "grep -r onefile" doesn't show filenames, but "grep -r onedir" should.
  if (new->parent && !FLAG(h)) toys.optflags |= FLAG_H;

  name = dirtree_path(new, 0);
  do_grep(openat(dirtree_parentfd(new), new->name, O_NONBLOCK|O_NOCTTY), name);
  free(name);

  return 0;
}

void grep_main(void)
{
  char **ss = toys.optargs;

  TT.fixed = xzalloc(256*sizeof(*TT.fixed));

  if (FLAG(color) && (!TT.color || !strcmp(TT.color, "auto")) && !isatty(1))
    toys.optflags &= ~FLAG_color;

  if (FLAG(color)) {
    TT.purple = "\e[35m";
    TT.cyan = "\e[36m";
    TT.red = "\e[1;31m";
    TT.green = "\e[32m";
    TT.grey = "\e[m";
  } else TT.purple = TT.cyan = TT.red = TT.green = TT.grey = "";

  if (FLAG(R)) toys.optflags |= FLAG_r;

  // Grep exits with 2 for errors
  toys.exitval = 2;

  if (!TT.A) TT.A = TT.C;
  if (!TT.B) TT.B = TT.C;

  TT.delim = '\n' * !FLAG(z);

  // Handle egrep and fgrep
  if (*toys.which->name == 'e') toys.optflags |= FLAG_E;
  if (*toys.which->name == 'f') toys.optflags |= FLAG_F;

  if (!TT.e && !TT.f) {
    if (!*ss) error_exit("no REGEX");
    TT.e = xzalloc(sizeof(struct arg_list));
    TT.e->arg = *(ss++);
    toys.optc--;
  }

  parse_regex();

  if (!FLAG(h) && toys.optc>1) toys.optflags |= FLAG_H;

  if (FLAG(s)) {
    close(2);
    xopen_stdio("/dev/null", O_RDWR);
  }

  if (FLAG(r)) {
    // Iterate through -r arguments. Use "." as default if none provided.
    for (ss = *ss ? ss : (char *[]){".", 0}; *ss; ss++) {
      if (!strcmp(*ss, "-")) do_grep(0, *ss);
      else dirtree_read(*ss, do_grep_r);
    }
  } else loopfiles_rw(ss, O_RDONLY|WARN_ONLY, 0, do_grep);
  if (TT.tried >= toys.optc || (FLAG(q)&&TT.found)) toys.exitval = !TT.found;
}
