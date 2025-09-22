/* GNU sdiff - side-by-side merge of file differences

   Copyright (C) 1992-1996, 1998, 2001-2002, 2004, 2006-2007, 2009-2013,
   2015-2023 Free Software Foundation, Inc.

   This file is part of GNU DIFF.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "system.h"
#include "paths.h"

#include <stdio.h>
#include <unlocked-io.h>

#include <c-stack.h>
#include <dirname.h>
#include "die.h"
#include <error.h>
#include <exitfail.h>
#include <file-type.h>
#include <getopt.h>
#include <progname.h>
#include <system-quote.h>
#include <version-etc.h>
#include <xalloc.h>
#include <xstdopen.h>

/* The official name of this program (e.g., no 'g' prefix).  */
static char const PROGRAM_NAME[] = "sdiff";

#define AUTHORS \
  proper_name ("Thomas Lord")

/* Size of chunks read from files which must be parsed into lines.  */
enum { SDIFF_BUFSIZE = 65536 };

static char const *editor_program = DEFAULT_EDITOR_PROGRAM;
static char const **diffargv;

static char * volatile tmpname;
static FILE *tmp;

#if HAVE_WORKING_FORK
static pid_t volatile diffpid;
#endif

struct line_filter;

static void catchsig (int);
static bool edit (struct line_filter *, char const *, lin, lin, struct line_filter *, char const *, lin, lin, FILE *);
static bool interact (struct line_filter *, struct line_filter *, char const *, struct line_filter *, char const *, FILE *);
static void checksigs (void);
static void diffarg (char const *);
static _Noreturn void fatal (char const *);
static _Noreturn void perror_fatal (char const *);
static void trapsigs (void);
static void untrapsig (int);

static int const sigs[] = {
#ifdef SIGHUP
       SIGHUP,
#endif
#ifdef SIGQUIT
       SIGQUIT,
#endif
#ifdef SIGTERM
       SIGTERM,
#endif
#ifdef SIGXCPU
       SIGXCPU,
#endif
#ifdef SIGXFSZ
       SIGXFSZ,
#endif
#ifdef SIGPIPE
       SIGPIPE,
#endif
       SIGINT
};
enum
  {
    NUM_SIGS = sizeof sigs / sizeof *sigs,
    handler_index_of_SIGINT = NUM_SIGS - 1
  };

#if HAVE_SIGACTION
  /* Prefer 'sigaction' if available, since 'signal' can lose signals.  */
  static struct sigaction initial_action[NUM_SIGS];
# define initial_handler(i) (initial_action[i].sa_handler)
  static void signal_handler (int, void (*) (int));
#else
  static void (*initial_action[NUM_SIGS]) ();
# define initial_handler(i) (initial_action[i])
# define signal_handler(sig, handler) signal (sig, handler)
#endif

static bool diraccess (char const *);
static int temporary_file (void);

/* Options: */

/* Name of output file if -o specified.  */
static char const *output;

/* Do not print common lines.  */
static bool suppress_common_lines;

/* Value for the long option that does not have single-letter equivalents.  */
enum
{
  DIFF_PROGRAM_OPTION = CHAR_MAX + 1,
  HELP_OPTION,
  STRIP_TRAILING_CR_OPTION,
  TABSIZE_OPTION
};

static struct option const longopts[] =
{
  {"diff-program", 1, 0, DIFF_PROGRAM_OPTION},
  {"expand-tabs", 0, 0, 't'},
  {"help", 0, 0, HELP_OPTION},
  {"ignore-all-space", 0, 0, 'W'}, /* swap W and w for historical reasons */
  {"ignore-blank-lines", 0, 0, 'B'},
  {"ignore-case", 0, 0, 'i'},
  {"ignore-matching-lines", 1, 0, 'I'},
  {"ignore-space-change", 0, 0, 'b'},
  {"ignore-tab-expansion", 0, 0, 'E'},
  {"ignore-trailing-space", 0, 0, 'Z'},
  {"left-column", 0, 0, 'l'},
  {"minimal", 0, 0, 'd'},
  {"output", 1, 0, 'o'},
  {"speed-large-files", 0, 0, 'H'},
  {"strip-trailing-cr", 0, 0, STRIP_TRAILING_CR_OPTION},
  {"suppress-common-lines", 0, 0, 's'},
  {"tabsize", 1, 0, TABSIZE_OPTION},
  {"text", 0, 0, 'a'},
  {"version", 0, 0, 'v'},
  {"width", 1, 0, 'w'},
  {0, 0, 0, 0}
};

static _Noreturn void
try_help (char const *reason_msgid, char const *operand)
{
  if (reason_msgid)
    error (0, 0, _(reason_msgid), operand);
  die (EXIT_TROUBLE, 0, _("Try '%s --help' for more information."),
         program_name);
}

static void
check_stdout (void)
{
  if (ferror (stdout))
    fatal ("write failed");
  else if (fclose (stdout) != 0)
    perror_fatal (_("standard output"));
}

static char const * const option_help_msgid[] = {
  N_("-o, --output=FILE            operate interactively, sending output to FILE"),
  "",
  N_("-i, --ignore-case            consider upper- and lower-case to be the same"),
  N_("-E, --ignore-tab-expansion   ignore changes due to tab expansion"),
  N_("-Z, --ignore-trailing-space  ignore white space at line end"),
  N_("-b, --ignore-space-change    ignore changes in the amount of white space"),
  N_("-W, --ignore-all-space       ignore all white space"),
  N_("-B, --ignore-blank-lines     ignore changes whose lines are all blank"),
  N_("-I, --ignore-matching-lines=RE  ignore changes all whose lines match RE"),
  N_("    --strip-trailing-cr      strip trailing carriage return on input"),
  N_("-a, --text                   treat all files as text"),
  "",
  N_("-w, --width=NUM              output at most NUM (default 130) print columns"),
  N_("-l, --left-column            output only the left column of common lines"),
  N_("-s, --suppress-common-lines  do not output common lines"),
  "",
  N_("-t, --expand-tabs            expand tabs to spaces in output"),
  N_("    --tabsize=NUM            tab stops at every NUM (default 8) print columns"),
  "",
  N_("-d, --minimal                try hard to find a smaller set of changes"),
  N_("-H, --speed-large-files      assume large files, many scattered small changes"),
  N_("    --diff-program=PROGRAM   use PROGRAM to compare files"),
  "",
  N_("    --help                   display this help and exit"),
  N_("-v, --version                output version information and exit"),
  0
};

static void
usage (void)
{
  char const * const *p;

  printf (_("Usage: %s [OPTION]... FILE1 FILE2\n"), program_name);
  printf ("%s\n\n",
          _("Side-by-side merge of differences between FILE1 and FILE2."));

  fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
  for (p = option_help_msgid;  *p;  p++)
    if (**p)
      printf ("  %s\n", _(*p));
    else
      putchar ('\n');
  printf ("\n%s\n%s\n",
          _("If a FILE is '-', read standard input."),
          _("Exit status is 0 if inputs are the same, 1 if different, 2 if trouble."));
  emit_bug_reporting_address ();
}

/* Clean up after a signal or other failure.  This function is
   async-signal-safe.  */
static void
cleanup (_GL_UNUSED int signo)
{
#if HAVE_WORKING_FORK
  if (0 < diffpid)
    kill (diffpid, SIGPIPE);
#endif
  if (tmpname)
    unlink (tmpname);
}

static _Noreturn void
exiterr (void)
{
  cleanup (0);
  untrapsig (0);
  checksigs ();
  exit (EXIT_TROUBLE);
}

static void
fatal (char const *msgid)
{
  error (0, 0, "%s", _(msgid));
  exiterr ();
}

static void
perror_fatal (char const *msg)
{
  int e = errno;
  checksigs ();
  error (0, e, "%s", msg);
  exiterr ();
}

static void
check_child_status (int werrno, int wstatus, int max_ok_status,
                    char const *subsidiary_program)
{
  int status = (! werrno && WIFEXITED (wstatus)
                ? WEXITSTATUS (wstatus)
                : INT_MAX);

  if (max_ok_status < status)
    {
      error (0, werrno,
             _(status == 126
               ? "subsidiary program '%s' could not be invoked"
               : status == 127
               ? "subsidiary program '%s' not found"
               : status == INT_MAX
               ? "subsidiary program '%s' failed"
               : "subsidiary program '%s' failed (exit status %d)"),
             subsidiary_program, status);
      exiterr ();
    }
}

static FILE *
ck_fopen (char const *fname, char const *type)
{
  FILE *r = fopen (fname, type);
  if (! r)
    perror_fatal (fname);
  return r;
}

static void
ck_fclose (FILE *f)
{
  if (fclose (f))
    perror_fatal ("fclose");
}

static size_t
ck_fread (char *buf, size_t size, FILE *f)
{
  size_t r = fread (buf, sizeof (char), size, f);
  if (r == 0 && ferror (f))
    perror_fatal (_("read failed"));
  return r;
}

static void
ck_fwrite (char const *buf, size_t size, FILE *f)
{
  if (fwrite (buf, sizeof (char), size, f) != size)
    perror_fatal (_("write failed"));
}

static void
ck_fflush (FILE *f)
{
  if (fflush (f) != 0)
    perror_fatal (_("write failed"));
}

static char const *
expand_name (char *name, bool is_dir, char const *other_name)
{
  if (STREQ (name, "-"))
    fatal ("cannot interactively merge standard input");
  if (! is_dir)
    return name;
  else
    {
      /* Yield NAME/BASE, where BASE is OTHER_NAME's basename.  */
      char const *base = last_component (other_name);
      size_t namelen = strlen (name), baselen = base_len (base);
      bool insert_slash = *last_component (name) && name[namelen - 1] != '/';
      char *r = xmalloc (namelen + insert_slash + baselen + 1);
      char *p = stpcpy (r, name);
      *p = '/';
      p = mempcpy (p + insert_slash, base, baselen);
      *p = '\0';
      return r;
    }
}

struct line_filter {
  FILE *infile;
  char *bufpos;
  char *buffer;
  char *buflim;
};

static void
lf_init (struct line_filter *lf, FILE *infile)
{
  lf->infile = infile;
  lf->bufpos = lf->buffer = lf->buflim = xmalloc (SDIFF_BUFSIZE + 1);
  lf->buflim[0] = '\n';
}

/* Fill an exhausted line_filter buffer from its INFILE */
static size_t
lf_refill (struct line_filter *lf)
{
  size_t s = ck_fread (lf->buffer, SDIFF_BUFSIZE, lf->infile);
  lf->bufpos = lf->buffer;
  lf->buflim = lf->buffer + s;
  lf->buflim[0] = '\n';
  checksigs ();
  return s;
}

/* Advance LINES on LF's infile, copying lines to OUTFILE */
static void
lf_copy (struct line_filter *lf, lin lines, FILE *outfile)
{
  char *start = lf->bufpos;

  while (lines)
    {
      lf->bufpos = rawmemchr (lf->bufpos, '\n');
      if (lf->bufpos == lf->buflim)
        {
          ck_fwrite (start, lf->buflim - start, outfile);
          if (! lf_refill (lf))
            return;
          start = lf->bufpos;
        }
      else
        {
          --lines;
          ++lf->bufpos;
        }
    }

  ck_fwrite (start, lf->bufpos - start, outfile);
}

/* Advance LINES on LF's infile without doing output */
static void
lf_skip (struct line_filter *lf, lin lines)
{
  while (lines)
    {
      lf->bufpos = rawmemchr (lf->bufpos, '\n');
      if (lf->bufpos == lf->buflim)
        {
          if (! lf_refill (lf))
            break;
        }
      else
        {
          --lines;
          ++lf->bufpos;
        }
    }
}

/* Snarf a line into a buffer.  Return EOF if EOF, 0 if error, 1 if OK.  */
static int
lf_snarf (struct line_filter *lf, char *buffer, size_t bufsize)
{
  for (;;)
    {
      char *start = lf->bufpos;
      char *next = rawmemchr (start, '\n');
      size_t s = next - start;
      if (bufsize <= s)
        return 0;
      buffer = mempcpy (buffer, start, s);
      bufsize -= s;
      if (next < lf->buflim)
        {
          *buffer = 0;
          lf->bufpos = next + 1;
          return 1;
        }
      if (! lf_refill (lf))
        return s ? 0 : EOF;
    }
}

int
main (int argc, char *argv[])
{
  int opt;
  char const *prog;

  exit_failure = EXIT_TROUBLE;
  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  c_stack_action (cleanup);
  xstdopen ();

  prog = getenv ("EDITOR");
  if (prog)
    editor_program = prog;

  diffarg (DEFAULT_DIFF_PROGRAM);

  /* parse command line args */
  while ((opt = getopt_long (argc, argv, "abBdEHiI:lo:stvw:WZ", longopts, 0))
         != -1)
    {
      switch (opt)
        {
        case 'a':
          diffarg ("-a");
          break;

        case 'b':
          diffarg ("-b");
          break;

        case 'B':
          diffarg ("-B");
          break;

        case 'd':
          diffarg ("-d");
          break;

        case 'E':
          diffarg ("-E");
          break;

        case 'H':
          diffarg ("-H");
          break;

        case 'i':
          diffarg ("-i");
          break;

        case 'I':
          diffarg ("-I");
          diffarg (optarg);
          break;

        case 'l':
          diffarg ("--left-column");
          break;

        case 'o':
          output = optarg;
          break;

        case 's':
          suppress_common_lines = true;
          break;

        case 't':
          diffarg ("-t");
          break;

        case 'v':
          version_etc (stdout, PROGRAM_NAME, PACKAGE_NAME, Version,
                       AUTHORS, nullptr);
          check_stdout ();
          return EXIT_SUCCESS;

        case 'w':
          diffarg ("-W");
          diffarg (optarg);
          break;

        case 'W':
          diffarg ("-w");
          break;

        case 'Z':
          diffarg ("-Z");
          break;

        case DIFF_PROGRAM_OPTION:
          diffargv[0] = optarg;
          break;

        case HELP_OPTION:
          usage ();
          check_stdout ();
          return EXIT_SUCCESS;

        case STRIP_TRAILING_CR_OPTION:
          diffarg ("--strip-trailing-cr");
          break;

        case TABSIZE_OPTION:
          diffarg ("--tabsize");
          diffarg (optarg);
          break;

        default:
          try_help (0, 0);
        }
    }

  if (argc - optind != 2)
    {
      if (argc - optind < 2)
        try_help ("missing operand after '%s'", argv[argc - 1]);
      else
        try_help ("extra operand '%s'", argv[optind + 2]);
    }

  if (! output)
    {
      /* easy case: diff does everything for us */
      if (suppress_common_lines)
        diffarg ("--suppress-common-lines");
      diffarg ("-y");
      diffarg ("--");
      diffarg (argv[optind]);
      diffarg (argv[optind + 1]);
      diffarg (0);
      execvp (diffargv[0], (char **) diffargv);
      perror_fatal (diffargv[0]);
    }
  else
    {
      char const *lname, *rname;
      FILE *left, *right, *out, *diffout;
      bool interact_ok;
      struct line_filter lfilt;
      struct line_filter rfilt;
      struct line_filter diff_filt;
      bool leftdir = diraccess (argv[optind]);
      bool rightdir = diraccess (argv[optind + 1]);

      if (leftdir & rightdir)
        fatal ("both files to be compared are directories");

      lname = expand_name (argv[optind], leftdir, argv[optind + 1]);
      left = ck_fopen (lname, "r");
      rname = expand_name (argv[optind + 1], rightdir, argv[optind]);
      right = ck_fopen (rname, "r");
      out = ck_fopen (output, "w");

      diffarg ("--sdiff-merge-assist");
      diffarg ("--");
      diffarg (argv[optind]);
      diffarg (argv[optind + 1]);
      diffarg (0);

      trapsigs ();

#if ! HAVE_WORKING_FORK
      {
        char *command = system_quote_argv (SCI_SYSTEM, (char **) diffargv);
        errno = 0;
        diffout = popen (command, "r");
        if (! diffout)
          perror_fatal (command);
        free (command);
      }
#else
      {
        int diff_fds[2];

        if (pipe (diff_fds) != 0)
          perror_fatal ("pipe");

        diffpid = fork ();
        if (diffpid < 0)
          perror_fatal ("fork");
        if (! diffpid)
          {
            /* Alter the child's SIGINT and SIGPIPE handlers;
               this may munge the parent.
               The child ignores SIGINT in case the user interrupts the editor.
               The child does not ignore SIGPIPE, even if the parent does.  */
            if (initial_handler (handler_index_of_SIGINT) != SIG_IGN)
              signal_handler (SIGINT, SIG_IGN);
            signal_handler (SIGPIPE, SIG_DFL);
            close (diff_fds[0]);
            if (diff_fds[1] != STDOUT_FILENO)
              {
                dup2 (diff_fds[1], STDOUT_FILENO);
                close (diff_fds[1]);
              }

            execvp (diffargv[0], (char **) diffargv);
            _exit (errno == ENOENT ? 127 : 126);
          }

        close (diff_fds[1]);
        diffout = fdopen (diff_fds[0], "r");
        if (! diffout)
          perror_fatal ("fdopen");
      }
#endif

      lf_init (&diff_filt, diffout);
      lf_init (&lfilt, left);
      lf_init (&rfilt, right);

      interact_ok = interact (&diff_filt, &lfilt, lname, &rfilt, rname, out);

      ck_fclose (left);
      ck_fclose (right);
      ck_fclose (out);

      {
        int wstatus;
        int werrno = 0;

#if ! HAVE_WORKING_FORK
        wstatus = pclose (diffout);
        if (wstatus == -1)
          werrno = errno;
#else
        ck_fclose (diffout);
        while (waitpid (diffpid, &wstatus, 0) < 0)
          if (errno == EINTR)
            checksigs ();
          else
            perror_fatal ("waitpid");
        diffpid = 0;
#endif

        if (tmpname)
          {
            unlink (tmpname);
            tmpname = 0;
          }

        if (! interact_ok)
          exiterr ();

        check_child_status (werrno, wstatus, EXIT_FAILURE, diffargv[0]);
        untrapsig (0);
        checksigs ();
        exit (WEXITSTATUS (wstatus));
      }
    }
  return EXIT_SUCCESS;			/* Fool '-Wall'.  */
}

static void
diffarg (char const *a)
{
  static size_t diffargs, diffarglim;

  if (diffargs == diffarglim)
    {
      if (! diffarglim)
        diffarglim = 16;
      else if (PTRDIFF_MAX / (2 * sizeof *diffargv) <= diffarglim)
        xalloc_die ();
      else
        diffarglim *= 2;
      diffargv = xrealloc (diffargv, diffarglim * sizeof *diffargv);
    }
  diffargv[diffargs++] = a;
}

/* Signal handling */

static bool volatile ignore_SIGINT;
static int volatile signal_received;
static bool sigs_trapped;

static void
catchsig (int s)
{
#if ! HAVE_SIGACTION
  signal (s, SIG_IGN);
#endif
  if (! (s == SIGINT && ignore_SIGINT))
    signal_received = s;
}

#if HAVE_SIGACTION
static struct sigaction catchaction;

static void
signal_handler (int sig, void (*handler) (int))
{
  catchaction.sa_handler = handler;
  sigaction (sig, &catchaction, 0);
}
#endif

static void
trapsigs (void)
{
  int i;

#if HAVE_SIGACTION
  catchaction.sa_flags = SA_RESTART;
  sigemptyset (&catchaction.sa_mask);
  for (i = 0;  i < NUM_SIGS;  i++)
    sigaddset (&catchaction.sa_mask, sigs[i]);
#endif

  for (i = 0;  i < NUM_SIGS;  i++)
    {
#if HAVE_SIGACTION
      sigaction (sigs[i], 0, &initial_action[i]);
#else
      initial_action[i] = signal (sigs[i], SIG_IGN);
#endif
      if (initial_handler (i) != SIG_IGN)
        signal_handler (sigs[i], catchsig);
    }

#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif

  sigs_trapped = true;
}

/* Untrap signal S, or all trapped signals if S is zero.  */
static void
untrapsig (int s)
{
  int i;

  if (sigs_trapped)
    for (i = 0;  i < NUM_SIGS;  i++)
      if ((! s || sigs[i] == s)  &&  initial_handler (i) != SIG_IGN)
        {
#if HAVE_SIGACTION
          sigaction (sigs[i], &initial_action[i], 0);
#else
          signal (sigs[i], initial_action[i]);
#endif
        }
}

/* Exit if a signal has been received.  */
static void
checksigs (void)
{
  int s = signal_received;
  if (s)
    {
      cleanup (0);

      /* Yield an exit status indicating that a signal was received.  */
      untrapsig (s);
      raise (s);

      /* That didn't work, so exit with error status.  */
      exit (EXIT_TROUBLE);
    }
}

static void
give_help (void)
{
  fprintf (stderr, "%s", _("\
ed:\tEdit then use both versions, each decorated with a header.\n\
eb:\tEdit then use both versions.\n\
el or e1:\tEdit then use the left version.\n\
er or e2:\tEdit then use the right version.\n\
e:\tDiscard both versions then edit a new one.\n\
l or 1:\tUse the left version.\n\
r or 2:\tUse the right version.\n\
s:\tSilently include common lines.\n\
v:\tVerbosely include common lines.\n\
q:\tQuit.\n\
"));
}

static int
skip_white (void)
{
  int c;
  for (;;)
    {
      c = getchar ();
      if (! isspace (c) || c == '\n')
        break;
      checksigs ();
    }
  if (ferror (stdin))
    perror_fatal (_("read failed"));
  return c;
}

static void
flush_line (void)
{
  int c;
  while ((c = getchar ()) != '\n' && c != EOF)
    continue;
  if (ferror (stdin))
    perror_fatal (_("read failed"));
}


/* interpret an edit command */
static bool
edit (struct line_filter *left, char const *lname, lin lline, lin llen,
      struct line_filter *right, char const *rname, lin rline, lin rlen,
      FILE *outfile)
{
  for (;;)
    {
      int cmd0;
      int cmd1 IF_LINT (= 0);  /* IF_LINT due to GCC bug 101770.  */
      bool gotcmd = false;

      while (! gotcmd)
        {
          if (putchar ('%') != '%')
            perror_fatal (_("write failed"));
          ck_fflush (stdout);

          cmd0 = skip_white ();
          switch (cmd0)
            {
            case '1': case '2': case 'l': case 'r':
            case 's': case 'v': case 'q':
              if (skip_white () != '\n')
                {
                  give_help ();
                  flush_line ();
                  continue;
                }
              gotcmd = true;
              break;

            case 'e':
              cmd1 = skip_white ();
              switch (cmd1)
                {
                case '1': case '2': case 'b': case 'd': case 'l': case 'r':
                  if (skip_white () != '\n')
                    {
                      give_help ();
                      flush_line ();
                      continue;
                    }
                  gotcmd = true;
                  break;
                case '\n':
                  gotcmd = true;
                  break;
                default:
                  give_help ();
                  flush_line ();
                  continue;
                }
              break;

            case EOF:
              if (feof (stdin))
                {
                  gotcmd = true;
                  cmd0 = 'q';
                  break;
                }
              FALLTHROUGH;
            default:
              flush_line ();
              FALLTHROUGH;
            case '\n':
              give_help ();
              continue;
            }
        }

      switch (cmd0)
        {
        case '1': case 'l':
          lf_copy (left, llen, outfile);
          lf_skip (right, rlen);
          return true;
        case '2': case 'r':
          lf_copy (right, rlen, outfile);
          lf_skip (left, llen);
          return true;
        case 's':
          suppress_common_lines = true;
          break;
        case 'v':
          suppress_common_lines = false;
          break;
        case 'q':
          return false;
        case 'e':
          {
            int fd;

            if (tmpname)
              tmp = fopen (tmpname, "w");
            else
              {
                if ((fd = temporary_file ()) < 0)
                  perror_fatal ("mkstemp");
                tmp = fdopen (fd, "w");
              }

            if (! tmp)
              perror_fatal (tmpname);

            switch (cmd1)
              {
              case 'd':
                if (llen)
		  {
		    if (llen == 1)
		      fprintf (tmp, "--- %s %"pI"d\n", lname, lline);
		    else
		      fprintf (tmp, "--- %s %"pI"d,%"pI"d\n", lname, lline,
			       lline + llen - 1);
		  }
                FALLTHROUGH;
              case '1': case 'b': case 'l':
                lf_copy (left, llen, tmp);
                break;

              default:
                lf_skip (left, llen);
                break;
              }

            switch (cmd1)
              {
              case 'd':
                if (rlen)
		  {
		    if (rlen == 1)
		      fprintf (tmp, "+++ %s %"pI"d\n", rname, rline);
		    else
		      fprintf (tmp, "+++ %s %"pI"d,%"pI"d\n", rname, rline,
			     rline + rlen - 1);
		  }
                FALLTHROUGH;
              case '2': case 'b': case 'r':
                lf_copy (right, rlen, tmp);
                break;

              default:
                lf_skip (right, rlen);
                break;
              }

            ck_fclose (tmp);

            {
              int wstatus;
              int werrno = 0;
              char const *argv[3];

              ignore_SIGINT = true;
              checksigs ();
              argv[0] = editor_program;
              argv[1] = tmpname;
              argv[2] = 0;

              {
#if ! HAVE_WORKING_FORK
                char *command = system_quote_argv (SCI_SYSTEM, (char **) argv);
                wstatus = system (command);
                if (wstatus == -1)
                  werrno = errno;
                free (command);
#else
                pid_t pid;

                pid = fork ();
                if (pid == 0)
                  {
                    execvp (editor_program, (char **) argv);
                    _exit (errno == ENOENT ? 127 : 126);
                  }

                if (pid < 0)
                  perror_fatal ("fork");

                while (waitpid (pid, &wstatus, 0) < 0)
                  if (errno == EINTR)
                    checksigs ();
                  else
                    perror_fatal ("waitpid");
#endif
              }

              ignore_SIGINT = false;
              check_child_status (werrno, wstatus, EXIT_SUCCESS,
                                  editor_program);
            }

            {
              char buf[SDIFF_BUFSIZE];
              size_t size;
              tmp = ck_fopen (tmpname, "r");
              while ((size = ck_fread (buf, SDIFF_BUFSIZE, tmp)) != 0)
                {
                  checksigs ();
                  ck_fwrite (buf, size, outfile);
                }
              ck_fclose (tmp);
            }
            return true;
          }
        default:
          give_help ();
          break;
        }
    }
}

/* Alternately reveal bursts of diff output and handle user commands.  */
static bool
interact (struct line_filter *diff,
          struct line_filter *left, char const *lname,
          struct line_filter *right, char const *rname,
          FILE *outfile)
{
  lin lline = 1, rline = 1;

  for (;;)
    {
      char diff_help[256];
      int snarfed = lf_snarf (diff, diff_help, sizeof diff_help);

      if (snarfed <= 0)
        return snarfed != 0;

      checksigs ();

      if (diff_help[0] == ' ')
        puts (diff_help + 1);
      else
        {
          char *numend;
          intmax_t val;
          lin llen, rlen, lenmax;
          errno = 0;
          val = strtoimax (diff_help + 1, &numend, 10);
          if (! (0 <= val && val <= LIN_MAX) || errno || *numend != ',')
            fatal (diff_help);
          llen = val;
          val = strtoimax (numend + 1, &numend, 10);
          if (! (0 <= val && val <= LIN_MAX) || errno || *numend)
            fatal (diff_help);
          rlen = val;

          lenmax = MAX (llen, rlen);

          switch (diff_help[0])
            {
            case 'i':
              if (suppress_common_lines)
                lf_skip (diff, lenmax);
              else
                lf_copy (diff, lenmax, stdout);

              lf_copy (left, llen, outfile);
              lf_skip (right, rlen);
              break;

            case 'c':
              lf_copy (diff, lenmax, stdout);
              if (! edit (left, lname, lline, llen,
                          right, rname, rline, rlen,
                          outfile))
                return false;
              break;

            default:
              fatal (diff_help);
            }

          lline += llen;
          rline += rlen;
        }
    }
}

/* Return true if DIR is an existing directory.  */
static bool
diraccess (char const *dir)
{
  struct stat buf;
  return stat (dir, &buf) == 0 && S_ISDIR (buf.st_mode);
}

#ifndef P_tmpdir
# define P_tmpdir "/tmp"
#endif
#ifndef TMPDIR_ENV
# define TMPDIR_ENV "TMPDIR"
#endif

/* Open a temporary file and return its file descriptor.  Put into
   tmpname the address of a newly allocated buffer that holds the
   file's name.  Use the prefix "sdiff".  */
static int
temporary_file (void)
{
  char const *tmpdir = getenv (TMPDIR_ENV);
  char const *dir = tmpdir ? tmpdir : P_tmpdir;
  char *buf = xmalloc (strlen (dir) + 1 + 5 + 6 + 1);
  strcpy (stpcpy (buf, dir), "/sdiffXXXXXX");
  int fd = mkstemp (buf);
  if (fd < 0)
    free (buf);
  else
    tmpname = buf;
  return fd;
}
