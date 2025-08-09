/* sed.c - stream editor. Thing that does s/// and other stuff.
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/sed.html
 *
 * xform See https://www.gnu.org/software/tar/manual/html_section/transform.html
 *
 * TODO: lines > 2G could wrap signed int length counters. Not just getline()
 * but N and s///
 * TODO: make y// handle unicode, unicode delimiters
 * TODO: handle error return from emit(), error_msg/exit consistently
 *       What's the right thing to do for -i when write fails? Skip to next?
 * test '//q' with no previous regex, also repeat previous regex?
 *
 * Deviations from POSIX: allow extended regular expressions with -r,
 * editing in place with -i, separate with -s, NUL-delimited strings with -z,
 * printf escapes in text, line continuations, semicolons after all commands,
 * 2-address anywhere an address is allowed, "T" command, multiline
 * continuations for [abc], \; to end [abc] argument before end of line.
 * Explicit violations of stuff posix says NOT to do: N at EOF does default
 * print, l escapes \n
 * Added --tarxform mode to support tar --xform

USE_SED(NEWTOY(sed, "(help)(version)(tarxform)e*f*i:;nErz(null-data)s[+Er]", TOYFLAG_BIN|TOYFLAG_AUTOCONF))

config SED
  bool "sed"
  default y
  help
    usage: sed [-inrszE] [-e SCRIPT]...|SCRIPT [-f SCRIPT_FILE]... [FILE...]

    Stream editor. Apply editing SCRIPTs to lines of input.

    -e	Add SCRIPT to list
    -f	Add contents of SCRIPT_FILE to list
    -i	Edit each file in place (-iEXT keeps backup file with extension EXT)
    -n	No default output (use the p command to output matched lines)
    -r	Use extended regular expression syntax
    -E	POSIX alias for -r
    -s	Treat input files separately (implied by -i)
    -z	Use \0 rather than \n as input line separator

    A SCRIPT is one or more COMMANDs separated by newlines or semicolons.
    All -e SCRIPTs and -f SCRIPT_FILE contents are combined in order as if
    separated by newlines. If no -e or -f then first argument is the SCRIPT.

    COMMANDs apply to every line unless prefixed with an ADDRESS of the form:

      [ADDRESS[,ADDRESS]][!]COMMAND

    ADDRESS is a line number (starting at 1), a /REGULAR EXPRESSION/, or $ for
    last line (-s or -i makes it last line of each file). One address matches one
    line, ADDRESS,ADDRESS matches from first to second inclusive. Two regexes can
    match multiple ranges. ADDRESS,+N ends N lines later. ! inverts the match.

    REGULAR EXPRESSIONS start and end with the same character (anything but
    backslash or newline). To use the delimiter in the regex escape it with a
    backslash, and printf escapes (\abcefnrtv and octal, hex, and unicode) work.
    An empty regex repeats the previous one. ADDRESS regexes require any
    first delimiter except / to be \escaped to distinguish it from COMMANDs.

    Sed reads each line of input, processes it, and writes it out or discards it
    before reading the next. Sed can remember one additional line in a separate
    buffer (the h, H, g, G, and x commands), and can read the next line of input
    early (the n and N commands), but otherwise operates on individual lines.

    Each COMMAND starts with a single character. Commands with no arguments are:

      !  Run this command when the ADDRESS _didn't_ match.
      {  Start new command block, continuing until a corresponding "}".
         Command blocks nest and can have ADDRESSes applying to the whole block.
      }  End command block (this COMMAND cannot have an address)
      d  Delete this line and move on to the next one
         (ignores remaining COMMANDs)
      D  Delete one line of input and restart command SCRIPT (same as "d"
         unless you've glued lines together with "N" or similar)
      g  Get remembered line (overwriting current line)
      G  Get remembered line (appending to current line)
      h  Remember this line (overwriting remembered line)
      H  Remember this line (appending to remembered line, if any)
      l  Print line escaping \abfrtvn, octal escape other nonprintng chars,
         wrap lines to terminal width with \, append $ to end of line.
      n  Print default output and read next line over current line (quit at EOF)
      N  Append \n and next line of input to this line. Quit at EOF without
         default output. Advances line counter for ADDRESS and "=".
      p  Print this line
      P  Print this line up to first newline (from "N")
      q  Quit (print default output, no more commands processed or lines read)
      x  Exchange this line with remembered line (overwrite in both directions)
      =  Print the current line number (plus newline)
      #  Comment, ignores rest of this line of SCRIPT (until newline)

    Commands that take an argument:

      : LABEL    Target for jump commands
      a TEXT     Append text to output before reading next line
      b LABEL    Branch, jumps to :LABEL (with no LABEL to end of SCRIPT)
      c TEXT     Delete matching ADDRESS range and output TEXT instead
      i TEXT     Insert text (output immediately)
      r FILE     Append contents of FILE to output before reading next line.
      s/S/R/F    Search for regex S replace match with R using flags F. Delimiter
                 is anything but \n or \, escape with \ to use in S or R. Printf
                 escapes work. Unescaped & in R becomes full matched text, \1
                 through \9 = parenthetical subexpression from S. \ at end of
                 line appends next line of SCRIPT. The flags in F are:
                 [0-9]    A number N, substitute only Nth match
                 g        Global, substitute all matches
                 i/I      Ignore case when matching
                 p        Print resulting line when match found and replaced
                 w [file] Write (append) line to file when match replaced
      t LABEL    Test, jump if s/// command matched this line since last test
      T LABEL    Test false, jump to :LABEL only if no s/// found a match
      w FILE     Write (append) line to file
      y/old/new/ Change each character in 'old' to corresponding character
                 in 'new' (with standard backslash escapes, delimiter can be
                 any repeated character except \ or \n)

    The TEXT arguments (to a c i) may end with an unescaped "\" to append
    the next line (leading whitespace is not skipped), and treat ";" as a
    literal character (use "\;" instead).
*/

#define FOR_sed
#include "toys.h"

GLOBALS(
  char *i;
  struct arg_list *f, *e;

  // processed pattern list
  struct double_list *pattern;

  char *nextline, *remember, *tarxform;
  void *restart, *lastregex;
  long nextlen, rememberlen, count;
  int fdout, noeol;
  unsigned xx, tarxlen, xflags;
  char delim, xftype;
)

// Linked list of parsed sed commands. Offset fields indicate location where
// regex or string starts, ala offset+(char *)struct, because we remalloc()
// these to expand them for multiline inputs, and pointers would have to be
// individually adjusted.

struct sedcmd {
  struct sedcmd *next, *prev;

  // Begin and end of each match
  long lmatch[2]; // line number of match
  int rmatch[2];  // offset of regex struct for prefix matches (/abc/,/def/p)
  int arg1, arg2, w; // offset of two arguments per command, plus s//w filename
  unsigned not, hit;
  unsigned sflags; // s///flag bits, see SFLAG macros below
  char c; // action
};

#define SFLAG_i 1
#define SFLAG_g 2
#define SFLAG_p 4
#define SFLAG_x 8
#define SFLAG_slash 16
#define SFLAG_R 32
#define SFLAG_S 64
#define SFLAG_H 128

// Write out line with potential embedded NUL, handling eol/noeol
static int emit(char *line, long len, int eol)
{
  int l = len, old = line[len];

  if (FLAG(tarxform)) {
    TT.tarxform = xrealloc(TT.tarxform, TT.tarxlen+len+TT.noeol+eol);
    if (TT.noeol) TT.tarxform[TT.tarxlen++] = TT.delim;
    memcpy(TT.tarxform+TT.tarxlen, line, len);
    TT.tarxlen += len;
    if (eol) TT.tarxform[TT.tarxlen++] = TT.delim;
  } else {
    if (TT.noeol && !writeall(TT.fdout, &TT.delim, 1)) return 1;
    if (eol) line[len++] = TT.delim;
    if (!len) return 0;
    l = writeall(TT.fdout, line, len);
    if (eol) line[len-1] = old;
  }
  TT.noeol = !eol;
  if (l != len) {
    if (TT.fdout != 1) perror_msg("short write");

    return 1;
  }

  return 0;
}

// Extend allocation to include new string, with newline between if newlen<0

static char *extend_string(char **old, char *new, int oldlen, int newlen)
{
  int newline = newlen < 0;
  char *s;

  if (newline) newlen = -newlen;
  s = *old = xrealloc(*old, oldlen+newlen+newline+1);
  if (newline) s[oldlen++] = TT.delim;
  memcpy(s+oldlen, new, newlen);
  s[oldlen+newlen] = 0;

  return s+oldlen+newlen+1;
}

// An empty regex repeats the previous one
static void *get_regex(void *command, int offset)
{
  if (!offset) {
    if (!TT.lastregex) error_exit("no previous regex");
    return TT.lastregex;
  }

  return TT.lastregex = offset+(char *)command;
}

// Apply pattern to line from input file
static void sed_line(char **pline, long plen)
{
  struct append {
    struct append *next, *prev;
    int file;
    char *str;
  } *append = 0;
  char *line;
  long len;
  struct sedcmd *command;
  int eol = 0, tea = 0;

  if (FLAG(tarxform)) {
    if (!pline) return;

    line = *pline;
    len = plen;
    *pline = 0;
    pline = 0;
  } else {
    line = TT.nextline;
    len = TT.nextlen;

    // Ignore EOF for all files before last unless -i or -s
    if (!pline && !FLAG(i) && !FLAG(s)) return;

    // Grab next line for deferred processing (EOF detection: we get a NULL
    // pline at EOF to flush last line). Note that only end of _last_ input
    // file matches $ (unless we're doing -i).
    TT.nextline = 0;
    TT.nextlen = 0;
    if (pline) {
      TT.nextline = *pline;
      TT.nextlen = plen;
      *pline = 0;
    }
  }

  if (!line || !len) return;
  if (line[len-1] == TT.delim) line[--len] = eol++;
  if (FLAG(tarxform) && len) {
    TT.xftype = line[--len];
    line[len] = 0;
  }
  TT.count++;

  // To prevent N as last command from restarting script, we added 1 to restart
  // so we'd use it here even when NULL. Alas, compilers that think C has
  // references instead of pointers assume ptr-1 can never be NULL (demonstrably
  // untrue) and inappropriately dead code eliminate, so use LP64 math until
  // we get a -fpointers-are-not-references compiler option.
  command = (void *)(TT.restart ? ((unsigned long)TT.restart)-1
    : (unsigned long)TT.pattern);
  TT.restart = 0;

  while (command) {
    char *str, c = command->c;

    // Have we got a line or regex matching range for this rule?
    if (*command->lmatch || *command->rmatch) {
      int miss = 0;
      long lm;

      // In a match that might end?
      if (command->hit) {
        if (!(lm = command->lmatch[1])) {
          if (!command->rmatch[1]) command->hit = 0;
          else {
            void *rm = get_regex(command, command->rmatch[1]);

            // regex match end includes matching line, so defer deactivation
            if (line && !regexec0(rm, line, len, 0, 0, 0)) miss = 1;
          }
        } else if (lm > 0 && lm < TT.count) command->hit = 0;
        else if (lm < -1 && TT.count == command->hit+(-lm-1)) command->hit = 0;

      // Start a new match?
      } else {
        if (!(lm = *command->lmatch)) {
          void *rm = get_regex(command, *command->rmatch);

          if (line && !regexec0(rm, line, len, 0, 0, 0))
            command->hit = TT.count;
        } else if (lm == TT.count || (lm == -1 && !pline))
          command->hit = TT.count;

        if (!command->lmatch[1] && !command->rmatch[1]) miss = 1;
      }

      // Didn't match?
      lm = !(command->not^!!command->hit);

      // Deferred disable from regex end match
      if (miss || command->lmatch[1] == TT.count) command->hit = 0;

      if (lm) {
        // Handle skipping curly bracket command group
        if (c == '{') {
          int curly = 1;

          while (curly) {
            command = command->next;
            if (command->c == '{') curly++;
            if (command->c == '}') curly--;
          }
        }
        command = command->next;
        continue;
      }
    }

    // A deleted line can still update line match state for later commands
    if (!line) {
      command = command->next;
      continue;
    }

    // Process command

    if (c=='a' || c=='r') {
      struct append *a = xzalloc(sizeof(struct append));
      if (command->arg1) a->str = command->arg1+(char *)command;
      a->file = c=='r';
      dlist_add_nomalloc((void *)&append, (void *)a);
    } else if (c=='b' || c=='t' || c=='T') {
      int t = tea;

      if (c != 'b') tea = 0;
      if (c=='b' || t^(c=='T')) {
        if (!command->arg1) break;
        str = command->arg1+(char *)command;
        for (command = (void *)TT.pattern; command; command = command->next)
          if (command->c == ':' && !strcmp(command->arg1+(char *)command, str))
            break;
        if (!command) error_exit("no :%s", str);
      }
    } else if (c=='c') {
      str = command->arg1+(char *)command;
      if (!command->hit) emit(str, strlen(str), 1);
      free(line);
      line = 0;
      continue;
    } else if (c=='d') {
      free(line);
      line = 0;
      continue;
    } else if (c=='D') {
      // Delete up to \n or end of buffer
      str = line;
      while ((str-line)<len) if (*(str++) == TT.delim) break;
      len -= str - line;
      memmove(line, str, len);

      // if "delete" blanks line, disable further processing
      // otherwise trim and restart script
      if (!len) {
        free(line);
        line = 0;
      } else {
        line[len] = 0;
        command = (void *)TT.pattern;
      }
      continue;
    } else if (c=='g') {
      free(line);
      line = xmemdup(TT.remember, TT.rememberlen+1);
      len = TT.rememberlen;
    } else if (c=='G') {
      line = xrealloc(line, len+TT.rememberlen+2);
      line[len++] = TT.delim;
      memcpy(line+len, TT.remember, TT.rememberlen);
      line[len += TT.rememberlen] = 0;
    } else if (c=='h') {
      free(TT.remember);
      TT.remember = xstrdup(line);
      TT.rememberlen = len;
    } else if (c=='H') {
      TT.remember = xrealloc(TT.remember, TT.rememberlen+len+2);
      TT.remember[TT.rememberlen++] = TT.delim;
      memcpy(TT.remember+TT.rememberlen, line, len);
      TT.remember[TT.rememberlen += len] = 0;
    } else if (c=='i') {
      str = command->arg1+(char *)command;
      emit(str, strlen(str), 1);
    } else if (c=='l') {
      int i, x, off;

      if (!TT.xx) {
        terminal_size(&TT.xx, 0);
        if (!TT.xx) TT.xx = 80;
        if (TT.xx > sizeof(toybuf)-10) TT.xx = sizeof(toybuf)-10;
        if (TT.xx > 4) TT.xx -= 4;
      }

      for (i = off = 0; i<len; i++) {
        if (off >= TT.xx) {
          toybuf[off++] = '\\';
          emit(toybuf, off, 1);
          off = 0;
        }
        x = stridx("\\\a\b\f\r\t\v\n", line[i]);
        if (x != -1) {
          toybuf[off++] = '\\';
          toybuf[off++] = "\\abfrtvn"[x];
        } else if (line[i] >= ' ') toybuf[off++] = line[i];
        else off += sprintf(toybuf+off, "\\%03o", line[i]);
      }
      toybuf[off++] = '$';
      emit(toybuf, off, 1);
    } else if (c=='n') {
      // The +1 forces restart processing even when next is null
      TT.restart = (void *)(((unsigned long)command->next)+1);

      break;
    } else if (c=='N') {
      // Can't just grab next line because we could have multiple N and
      // we need to actually read ahead to get N;$p EOF detection right.
      if (pline) {
        // The +1 forces restart processing even when  next is null
        TT.restart = (void *)(((unsigned long)command->next)+1);
        extend_string(&line, TT.nextline, len, -TT.nextlen);
        free(TT.nextline);
        TT.nextline = line;
        TT.nextlen += len + 1;
        line = 0;
      }

      // Pending append goes out right after N
      goto done;
    } else if (c=='p' || c=='P') {
      char *l = (c=='P') ? strchr(line, TT.delim) : 0;

      if (emit(line, l ? l-line : len, eol)) break;
    } else if (c=='q' || c=='Q') {
      if (pline) *pline = (void *)1;
      free(TT.nextline);
      if (!toys.exitval && command->arg1)
        toys.exitval = atoi(command->arg1+(char *)command);
      TT.nextline = 0;
      TT.nextlen = 0;
      if (c=='Q') line = 0;

      break;
    } else if (c=='s') {
      char *rline = line, *new = command->arg2 + (char *)command, *l2 = 0;
      regmatch_t *match = (void *)toybuf;
      regex_t *reg = get_regex(command, command->arg1);
      int mflags = 0, count = 0, l2used = 0, zmatch = 1, l2l = len, l2old = 0,
        bonk = 0, mlen, off, newlen;

      // Skip suppressed --tarxform types
      if (TT.xftype && (command->sflags & (SFLAG_R<<stridx("rsh", TT.xftype))));

      // Loop finding match in remaining line (up to remaining len)
      else while (!regexec0(reg, rline, len-(rline-line), 10, match, mflags)) {
        mlen = match[0].rm_eo-match[0].rm_so;

        // xform matches ending in / aren't allowed to match entire line
        if ((command->sflags & SFLAG_slash) && mlen==len) {
          while (len && ++bonk && line[--len]=='/');
          continue;
        }

        mflags = REG_NOTBOL;

        // Zero length matches don't count immediately after a previous match
        if (!mlen && !zmatch) {
          if (rline-line == len) break;
          if (l2) l2[l2used++] = *rline++;
          zmatch++;
          continue;
        } else zmatch = 0;

        // If we're replacing only a specific match, skip if this isn't it
        off = command->sflags>>8;
        if (off && off != ++count) {
          if (l2) memcpy(l2+l2used, rline, match[0].rm_eo);
          l2used += match[0].rm_eo;
          rline += match[0].rm_eo;

          continue;
        }
        // The fact getline() can allocate unbounded amounts of memory is
        // a bigger issue, but while we're here check for integer overflow
        if (match[0].rm_eo > INT_MAX) perror_exit(0);

        // newlen = strlen(new) but with \1 and & and printf escapes
        for (off = newlen = 0; new[off]; off++) {
          int cc = -1;

          if (new[off] == '&') cc = 0;
          else if (new[off] == '\\') cc = new[++off] - '0';
          if (cc < 0 || cc > 9) {
            newlen++;
            continue;
          }
          newlen += match[cc].rm_eo-match[cc].rm_so;
        }

        // Copy changed data to new string

        // Adjust allocation size of new string, copy data we know we'll keep
        l2l += newlen-mlen;
        if ((mlen = l2l|0xfff) > l2old) {
          l2 = xrealloc(l2, ++mlen);
          if (l2used && !l2old) memcpy(l2, rline-l2used, l2used);
          l2old = mlen;
        }
        if (match[0].rm_so) {
          memcpy(l2+l2used, rline, match[0].rm_so);
          l2used += match[0].rm_so;
        }

        // copy in new replacement text
        for (off = mlen = 0; new[off]; off++) {
          int cc = 0, ll;

          if (new[off] == '\\') {
            cc = new[++off] - '0';
            if (cc<0 || cc>9) {
              if (!(l2[l2used+mlen++] = unescape(new[off])))
                l2[l2used+mlen-1] = new[off];

              continue;
            } else if (cc > reg->re_nsub) error_exit("no s//\\%d/", cc);
          } else if (new[off] != '&') {
            l2[l2used+mlen++] = new[off];

            continue;
          }

          if (match[cc].rm_so != -1) {
            ll = match[cc].rm_eo-match[cc].rm_so;
            memcpy(l2+l2used+mlen, rline+match[cc].rm_so, ll);
            mlen += ll;
          }
        }
        l2used += newlen;
        rline += match[0].rm_eo;

        if (!(command->sflags & SFLAG_g)) break;
      }
      len += bonk;

      // If we made any changes, finish off l2 and swap it for line
      if (l2) {
        // grab trailing unmatched data and null terminator, swap with original
        mlen = len-(rline-line);
        memcpy(l2+l2used, rline, mlen+1);
        len = l2used + mlen;
        free(line);
        line = l2;
      }

      if (mflags) {
        if (command->sflags & SFLAG_p) emit(line, len, eol);

        tea = 1;
        if (command->w) goto writenow;
      }
    } else if (c=='w') {
      int fd, noeol;
      char *name;

writenow:
      if (FLAG(tarxform)) error_exit("tilt");

      // Swap out emit() context
      fd = TT.fdout;
      noeol = TT.noeol;

      // We save filehandle and newline status before filename
      name = command->w + (char *)command;
      memcpy(&TT.fdout, name, 4);
      name += 4;
      TT.noeol = *(name++);

      // write, then save/restore context
      if (emit(line, len, eol))
        perror_exit("w '%s'", command->arg1+(char *)command);
      *(--name) = TT.noeol;
      TT.noeol = noeol;
      TT.fdout = fd;
    } else if (c=='x') {
      long swap = TT.rememberlen;

      str = TT.remember;
      TT.remember = line;
      line = str;
      TT.rememberlen = len;
      len = swap;
    } else if (c=='y') {
      char *from, *to = (char *)command;
      int i, j;

      from = to+command->arg1;
      to += command->arg2;

      for (i = 0; i < len; i++) {
        j = stridx(from, line[i]);
        if (j != -1) line[i] = to[j];
      }
    } else if (c=='=') {
      sprintf(toybuf, "%ld", TT.count);
      if (emit(toybuf, strlen(toybuf), 1)) break;
    }

    command = command->next;
  }

done:
  if (line && !FLAG(n)) emit(line, len, eol);

  // TODO: should "sed -z ax" use \n instead of NUL?
  if (dlist_terminate(append)) while (append) {
    struct append *a = append->next;

    if (append->file) {
      int fd = open(append->str, O_RDONLY);

      // Force newline if noeol pending
      if (fd != -1) {
        if (TT.noeol) xwrite(TT.fdout, &TT.delim, 1);
        TT.noeol = 0;
        xsendfile(fd, TT.fdout);
        close(fd);
      }
    } else if (append->str) emit(append->str, strlen(append->str), 1);
    else emit(line, 0, 0);
    free(append);
    append = a;
  }
  free(line);

  if (TT.tarxlen) {
    dprintf(TT.fdout, "%08x", --TT.tarxlen);
    writeall(TT.fdout, TT.tarxform, TT.tarxlen);
    TT.tarxlen = 0;
  }
}

// Callback called on each input file
static void do_sed_file(int fd, char *name)
{
  char *tmp, *s;

  if (FLAG(i)) {
    if (!fd) return error_msg("-i on stdin");
    TT.fdout = copy_tempfile(fd, name, &tmp);
  }
  if (FLAG(i) || FLAG(s)) {
    struct sedcmd *command;

    TT.count = 0;
    for (command = (void *)TT.pattern; command; command = command->next)
      command->hit = 0;
  }
  do_lines(fd, TT.delim, sed_line);
  if (FLAG(i)) {
    if (TT.i && *TT.i) {
      xrename(name, s = xmprintf("%s%s", name, TT.i));
      free(s);
    }
    replace_tempfile(-1, TT.fdout, &tmp);
    TT.fdout = 1;
  }
  if (FLAG(i) || FLAG(s)) {
    TT.nextline = 0;
    TT.nextlen = TT.noeol = 0;
  }
}

// Copy chunk of string between two delimiters, converting printf escapes.
// returns processed copy of string (0 if error), *pstr advances to next
// unused char. if delim (or *delim) is 0 uses/saves starting char as delimiter
// if regxex, ignore delimiter in [ranges]
static char *unescape_delimited_string(char **pstr, char *delim)
{
  char *to, *from, mode = 0, d;

  // Grab leading delimiter (if necessary), allocate space for new string
  from = *pstr;
  if (!delim || !*delim) {
    if (!(d = *(from++))) return 0;
    if (d == '\\') d = *(from++);
    if (!d || d == '\\') return 0;
    if (delim) *delim = d;
  } else d = *delim;
  to = delim = xmalloc(strlen(*pstr)+1);

  while (mode || *from != d) {
    if (!*from) return 0;

    // delimiter in regex character range doesn't count
    if (*from == '[') {
      if (!mode) {
        mode = ']';
        if (from[1]=='-' || from[1]==']') *(to++) = *(from++);
      } else if (mode == ']' && strchr(".=:", from[1])) {
        *(to++) = *(from++);
        mode = *from;
      }
    } else if (*from == mode) {
      if (mode == ']') mode = 0;
      else {
        *(to++) = *(from++);
        mode = ']';
      }
    // Length 1 range (X-X with same X) is "undefined" and makes regcomp err,
    // but the perl build does it, so we need to filter it out.
    } else if (mode && *from == '-' && from[-1] == from[1]) {
      from+=2;
      continue;
    } else if (*from == '\\') {
      if (!from[1]) return 0;

      // Check escaped end delimiter before printf style escapes.
      if (from[1] == d) from++;
      else if (from[1]=='\\') *(to++) = *(from++);
      else {
        char c = unescape(from[1]);

        if (c) {
          *(to++) = c;
          from+=2;
          continue;
        } else if (!mode) *(to++) = *(from++);
      }
    }
    *(to++) = *(from++);
  }
  *to = 0;
  *pstr = from+1;

  return delim;
}

// Translate pattern strings into command structures. Each command structure
// is a single allocation (which requires some math and remalloc at times).
static void parse_pattern(char **pline, long len)
{
  struct sedcmd *command = (void *)TT.pattern;
  char *line, *reg, c, *errstart;
  int i;

  line = errstart = pline ? *pline : "";
  if (len && line[len-1]=='\n') line[--len] = 0;

  // Append this line to previous multiline command? (hit indicates type.)
  // During parsing "hit" stores data about line continuations, but in
  // sed_line() it means the match range attached to this command
  // is active, so processing the continuation must zero it again.
  if (command && command->prev->hit) {
    // Remove half-finished entry from list so remalloc() doesn't confuse it
    TT.pattern = TT.pattern->prev;
    command = dlist_pop(&TT.pattern);
    c = command->c;
    reg = (char *)command;
    reg += command->arg1 + strlen(reg + command->arg1);

    // Resume parsing for 'a' or 's' command. (Only two that can do this.)
    // TODO: using 256 to indicate 'a' means our s/// delimiter can't be
    // a unicode character.
    if (command->hit < 256) goto resume_s;
    else goto resume_a;
  }

  // Loop through commands in this line.

  command = 0;
  for (;;) {
    if (command) dlist_add_nomalloc(&TT.pattern, (void *)command);

    // If there's no more data on this line, return.
    for (;;) {
      while (isspace(*line) || *line == ';') line++;
      if (*line == '#') while (*line && *line != '\n') line++;
      else break;
    }
    if (!*line) return;

    if (FLAG(tarxform) && strstart(&line, "flags=")) {
      TT.xflags = 7;
      while (0<=(i = stridx("rRsShH", *line))) {
        if (i&1) TT.xflags |= 1<<(i>>1);
        else TT.xflags &= ~(1<<(i>>1));
        line++;
      }
      continue;
    }

    // Start by writing data into toybuf.

    errstart = line;
    memset(toybuf, 0, sizeof(struct sedcmd));
    command = (void *)toybuf;
    reg = toybuf + sizeof(struct sedcmd);

    // Parse address range (if any)
    for (i = 0; i < 2; i++) {
      if (*line == ',') line++;
      else if (i) break;

      if (i && *line == '+' && isdigit(line[1])) {
        line++;
        command->lmatch[i] = -2-strtol(line, &line, 0);
      } else if (isdigit(*line)) command->lmatch[i] = strtol(line, &line, 0);
      else if (*line == '$') {
        command->lmatch[i] = -1;
        line++;
      } else if (*line == '/' || *line == '\\') {
        char *s = line;

        if (!(s = unescape_delimited_string(&line, 0))) goto error;
        if (!*s) command->rmatch[i] = 0;
        else {
          xregcomp((void *)reg, s, REG_EXTENDED*FLAG(r));
          command->rmatch[i] = reg-toybuf;
          reg += sizeof(regex_t);
        }
        free(s);
      } else break;
    }

    while (isspace(*line)) line++;
    if (!*line) break;

    if (*line == '!') {
      command->not = 1;
      line++;
    }
    while (isspace(*line)) line++;
    if (!*line) break;

    c = command->c = *(line++);
    if (strchr("}:", c) && i) break;
    if (strchr("aiqQr=", c) && i>1) break;

    // Allocate memory and copy out of toybuf now that we know how big it is
    command = xmemdup(toybuf, reg-toybuf);
    reg = (reg-toybuf) + (char *)command;

    // Parse arguments by command type
    if (c == '{') TT.nextlen++;
    else if (c == '}') {
      if (!TT.nextlen--) break;
    } else if (c == 's') {
      char *end, delim = 0;
      int flags;

      // s/pattern/replacement/flags

      // line continuations use arg1 (back at the start of the function),
      // so let's fill out arg2 first (since the regex part can't be multiple
      // lines) and swap them back later.

      // get pattern (just record, we parse it later)
      command->arg2 = reg - (char *)command;
      if (!(TT.remember = unescape_delimited_string(&line, &delim)))
        goto error;

      reg += sizeof(regex_t);
      command->arg1 = reg-(char *)command;
      command->hit = delim;
resume_s:
      // get replacement - don't replace escapes yet because \1 and \& need
      // processing later, after we replace \\ with \ we can't tell \\1 from \1
      end = line;
      while (*end != command->hit) {
        if (!*end) goto error;
        if (*end++ == '\\') {
          if (!*end || *end == '\n') {
            end[-1] = '\n';
            break;
          }
          end++;
        }
      }

      reg = extend_string((void *)&command, line, reg-(char *)command,end-line);
      line = end;
      // line continuation? (note: '\n' can't be a valid delim).
      if (*line == command->hit) command->hit = 0;
      else {
        if (!*line) continue;
        reg--;
        line++;
        goto resume_s;
      }

      // swap arg1/arg2 so they're back in order arguments occur.
      i = command->arg1;
      command->arg1 = command->arg2;
      command->arg2 = i;
      command->sflags = TT.xflags*SFLAG_R;

      // get flags
      for (line++; *line; line++) {
        long l;

        if (isspace(*line) && *line != '\n') continue;
        if (0 <= (l = stridx("igpx", *line))) command->sflags |= 1<<l;
        else if (*line == 'I') command->sflags |= 1<<0;
        else if (FLAG(tarxform) && 0 <= (l = stridx("RSH", *line)))
          command->sflags |= SFLAG_R<<l;
        // Given that the default is rsh all enabled... why do these exist?
        else if (FLAG(tarxform) && 0 <= (l = stridx("rsh", *line)))
          command->sflags &= ~(SFLAG_R<<l);
        else if (!(command->sflags>>8) && 0<(l = strtol(line, &line, 10))) {
          command->sflags |= l << 8;
          line--;
        } else break;
      }
      flags = (FLAG(r) || (command->sflags & SFLAG_x)) ? REG_EXTENDED : 0;
      if (command->sflags & SFLAG_i) flags |= REG_ICASE;

      // We deferred actually parsing the regex until we had the s///i flag
      // allocating the space was done by extend_string() above
      if (!*TT.remember) command->arg1 = 0;
      else {
        xregcomp((void *)(command->arg1+(char *)command), TT.remember, flags);
        if (FLAG(tarxform) && TT.remember[strlen(TT.remember)-1]=='/')
          command->sflags |= SFLAG_slash;
      }
      free(TT.remember);
      TT.remember = 0;
      if (*line == 'w') {
        line++;
        goto writenow;
      }
    } else if (c == 'w') {
      int fd, delim;
      char *cc;

      // Since s/// uses arg1 and arg2, and w needs a persistent filehandle and
      // eol status, and to retain the filename for error messages, we'd need
      // to go up to arg5 just for this. Compromise: dynamically allocate the
      // filehandle and eol status.

writenow:
      while (isspace(*line)) line++;
      if (!*line) goto error;
      for (cc = line; *cc; cc++) if (*cc == '\\' && cc[1] == ';') break;
      delim = *cc;
      *cc = 0;
      fd = xcreate(line, O_WRONLY|O_CREAT|O_TRUNC|O_APPEND, 0644);
      *cc = delim;

      command->w = reg - (char *)command;
      command = xrealloc(command, command->w+(cc-line)+6);
      reg = command->w + (char *)command;

      memcpy(reg, &fd, 4);
      reg += 4;
      *(reg++) = 0;
      memcpy(reg, line, delim);
      reg += delim;
      *(reg++) = 0;

      line = cc;
      if (delim) line += 2;
    } else if (c == 'y') {
      char *s, delim = 0;
      int len;

      if (!(s = unescape_delimited_string(&line, &delim))) goto error;
      command->arg1 = reg-(char *)command;
      len = strlen(s);
      reg = extend_string((void *)&command, s, reg-(char *)command, len);
      free(s);
      command->arg2 = reg-(char *)command;
      if (!(s = unescape_delimited_string(&line, &delim))) goto error;
      if (len != strlen(s)) goto error;
      reg = extend_string((void *)&command, s, reg-(char*)command, len);
      free(s);
    } else if (strchr("abcirtTqQw:", c)) {
      int end;

      // trim leading spaces
      while (isspace(*line) && *line != '\n') line++;

      // Resume logic differs from 's' case because we don't add a newline
      // unless it's after something, so we add it on return instead.
resume_a:
      command->hit = 0;

      // btTqQ: end with space or semicolon, aicrw continue to newline.
      if (!(end = strcspn(line, strchr(":btTqQ", c) ? "}; \t\r\n\v\f" : "\n"))){
        // Argument's optional for btTqQ
        if (strchr("btTqQ", c)) continue;
        else if (!command->arg1) break;
      }
      // Error checking: qQ can only have digits after them
      if (c=='q' || c=='Q') {
        for (i = 0; i<end && isdigit(line[i]); i++);
        if (i != end) {
          line += i;
          break;
        }
      }

      // Extend allocation to include new string. We use offsets instead of
      // pointers so realloc() moving stuff doesn't break things. Ok to write
      // \n over NUL terminator because call to extend_string() adds it back.
      if (!command->arg1) command->arg1 = reg - (char*)command;
      else if (*(command->arg1+(char *)command)) *(reg++) = '\n';
      else if (!pline) {
        command->arg1 = 0;
        continue;
      }
      reg = extend_string((void *)&command, line, reg - (char *)command, end);

      // Recopy data to remove escape sequences and handle line continuation.
      if (strchr("aci", c)) {
        reg -= end+1;
        for (i = end; i; i--) {
          if ((*reg++ = *line++)=='\\') {

            // escape at end of line: resume if -e escaped literal newline,
            // else request callback and resume with next line
            if (!--i) {
              *--reg = 0;
              if (*line) {
                line++;
                goto resume_a;
              }
              command->hit = 256;
              break;
            }
            if (!(reg[-1] = unescape(*line))) reg[-1] = *line;
            line++;
          }
        }
        *reg = 0;
      } else line += end;

    // Commands that take no arguments
    } else if (!strchr("{dDgGhHlnNpPx=", c)) break;
  }

error:
  error_exit("bad pattern '%s'@%ld (%c)", errstart, line-errstart+1L, *line);
}

// Is the pointer "find" within the string "range".
static int instr(char *find, char *range)
{
  return find>=range && range+strlen(range)>=find;
}

void sed_main(void)
{
  char **args = toys.optargs, **aa;

  if (FLAG(tarxform)) toys.optflags |= FLAG_z;
  if (!FLAG(z)) TT.delim = '\n';

  // Parse pattern into commands.

  // If no -e or -f, first argument is the pattern.
  if (!TT.e && !TT.f) {
    if (!*toys.optargs) error_exit("no pattern");
    (TT.e = xzalloc(sizeof(struct arg_list)))->arg = *(args++);
  }

  // -e and -f care about order, so use argv[] to recreate original order
  for (aa = toys.argv+1; *aa; aa++) {
    if (TT.e && instr(TT.e->arg, *aa)) {
      parse_pattern(&TT.e->arg, strlen(TT.e->arg));
      free(llist_pop(&TT.e));
    }
    if (TT.f && instr(TT.f->arg, *aa)) {
      do_lines(xopenro(TT.f->arg), TT.delim, parse_pattern);
      free(llist_pop(&TT.f));
    }
  }
  parse_pattern(0, 0);
  dlist_terminate(TT.pattern);
  if (TT.nextlen) error_exit("no }");

  TT.fdout = 1;
  TT.remember = xstrdup("");

  // Inflict pattern upon input files. Long version because !O_CLOEXEC
  loopfiles_rw(args, O_RDONLY|WARN_ONLY, 0, do_sed_file);

  // Provide EOF flush at end of cumulative input for non-i mode.
  if (!FLAG(i) && !FLAG(s)) {
    toys.optflags |= FLAG_s;
    sed_line(0, 0);
  }

  // TODO: need to close fd when done for TOYBOX_FREE?
}
