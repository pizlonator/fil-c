/* GNU diff3 - compare three files line by line

   Copyright (C) 1988-1989, 1992-1996, 1998, 2001-2002, 2004, 2006, 2009-2013,
   2015-2023 Free Software Foundation, Inc.

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

/* As of GCC 11.2.1, gcc -Wanalyzer-too-complex reports that this
   program's code is too complicated for gcc -fanalyzer.
   FIXME: Compile with -DANALYZER_NULL_DEFERENCE and see whether the
   resulting diagnostics are false alarms.  */
#if 10 <= __GNUC__ && !ANALYZER_NULL_DEREFERENCE
# pragma GCC diagnostic ignored "-Wanalyzer-null-dereference"
#endif

#include "system.h"
#include "paths.h"

#include <stdio.h>
#include <unlocked-io.h>

#include <c-stack.h>
#include <cmpbuf.h>
#include "die.h"
#include <error.h>
#include <exitfail.h>
#include <file-type.h>
#include <getopt.h>
#include <progname.h>
#include <system-quote.h>
#include <version-etc.h>
#include <xalloc.h>
#include <xfreopen.h>
#include <xstdopen.h>

/* The official name of this program (e.g., no 'g' prefix).  */
static char const PROGRAM_NAME[] = "diff3";

#define AUTHORS \
  proper_name ("Randy Smith")

/* Internal data structures and macros for the diff3 program; includes
   data structures for both diff3 diffs and normal diffs.  */

/* Different files within a three way diff.  */
enum { FILE0, FILE1, FILE2 };

/* A three way diff is built from two two-way diffs; the file which
   the two two-way diffs share is:  */
enum { FILEC = FILE2 };

/* Different files within a two way diff.
   FC is the common file, FO the other file.  */
enum { FO, FC };

/* The ranges are indexed by */
enum { RANGE_START, RANGE_END };

enum diff_type {
  DIFF_ERROR,			/* Should not be used */
  DIFF_ADD,			/* Two way diff add */
  DIFF_CHANGE,			/* Two way diff change */
  DIFF_DELETE,			/* Two way diff delete */
  DIFF_ALL,			/* All three are different */
  DIFF_1ST,			/* Only the first is different */
  DIFF_2ND,			/* Only the second */
  DIFF_3RD			/* Only the third */
};

/* Two way diff */
struct diff_block {
  lin ranges[2][2];		/* Ranges are inclusive */
  char **lines[2];		/* The actual lines (may contain nulls) */
  size_t *lengths[2];		/* Line lengths (including newlines, if any) */
  struct diff_block *next;
#ifdef GCC_LINT
  struct diff_block *n2;	/* Used only when freeing.  */
#endif
};

/* Three way diff */

struct diff3_block {
  enum diff_type correspond;	/* Type of diff */
  lin ranges[3][2];		/* Ranges are inclusive */
  char **lines[3];		/* The actual lines (may contain nulls) */
  size_t *lengths[3];		/* Line lengths (including newlines, if any) */
  struct diff3_block *next;
};

/* The following are macros, not functions, as they may be used as
   lvalues, or they may be polymorphic in that they work with either
   diff or diff3 blocks.  */

/* Access the ranges on a diff or diff3 block.  */
#define	D_LOWLINE(diff, filenum)	\
  ((diff)->ranges[filenum][RANGE_START])
#define	D_HIGHLINE(diff, filenum)	\
  ((diff)->ranges[filenum][RANGE_END])
#define	D_NUMLINES(diff, filenum)	\
  (D_HIGHLINE (diff, filenum) - D_LOWLINE (diff, filenum) + 1)

/* Access the line numbers in a file in a diff or diff3 block by relative line
   numbers (i.e. line number within the diff itself).  Note that these
   are lvalues and can be used for assignment.  */
#define	D_RELNUM(diff, filenum, linenum)	\
  ((diff)->lines[filenum][linenum])
#define	D_RELLEN(diff, filenum, linenum)	\
  ((diff)->lengths[filenum][linenum])

/* And get at them directly, when that should be necessary.  */
#define	D_LINEARRAY(diff, filenum)	\
  ((diff)->lines[filenum])
#define	D_LENARRAY(diff, filenum)	\
  ((diff)->lengths[filenum])

/* Next diff or diff3 block.  */
#define	D_NEXT(diff)	((diff)->next)

/* Access the type of a diff3 block.  */
#define	D3_TYPE(diff)	((diff)->correspond)

/* Line mappings based on diff or diff3 blocks.  The first maps off
   the top of the diff, the second off of the bottom.  */
#define	D_HIGH_MAPLINE(diff, fromfile, tofile, linenum)	\
  ((linenum)						\
   - D_HIGHLINE (diff, fromfile)			\
   + D_HIGHLINE (diff, tofile))

#define	D_LOW_MAPLINE(diff, fromfile, tofile, linenum)	\
  ((linenum)						\
   - D_LOWLINE (diff, fromfile)				\
   + D_LOWLINE (diff, tofile))

/* Options variables for flags set on command line.  */

/* If nonzero, treat all files as text files, never as binary.  */
static bool text;

/* Remove trailing carriage returns from input.  */
static bool strip_trailing_cr;

/* If nonzero, write out an ed script instead of the standard diff3 format.  */
static bool edscript;

/* If nonzero, in the case of overlapping diffs (type DIFF_ALL),
   preserve the lines which would normally be deleted from
   file 1 with a special flagging mechanism.  */
static bool flagging;

/* Use a tab to align output lines (-T).  */
static bool initial_tab;

/* If nonzero, do not output information for overlapping diffs.  */
static bool simple_only;

/* If nonzero, do not output information for non-overlapping diffs.  */
static bool overlap_only;

/* If nonzero, show information for DIFF_2ND diffs.  */
static bool show_2nd;

/* If nonzero, include ':wq' at the end of the script
   to write out the file being edited.   */
static bool finalwrite;

/* If nonzero, output a merged file.  */
static bool merge;

static char *read_diff (char const *, char const *, char **);
static char *scan_diff_line (char *, char **, size_t *, char *, char);
static enum diff_type process_diff_control (char **, struct diff_block *);
static bool compare_line_list (char * const[], size_t const[], char * const[], size_t const[], lin);
static bool copy_stringlist (char * const[], size_t const[], char *[], size_t[], lin);
static bool output_diff3_edscript (FILE *, struct diff3_block *, int const[3], int const[3], char const *, char const *, char const *);
static bool output_diff3_merge (FILE *, FILE *, struct diff3_block *, int const[3], int const[3], char const *, char const *, char const *);
static struct diff3_block *create_diff3_block (lin, lin, lin, lin, lin, lin);
static struct diff3_block *make_3way_diff (struct diff_block *, struct diff_block *);
static struct diff3_block *reverse_diff3_blocklist (struct diff3_block *);
static struct diff3_block *using_to_diff3_block (struct diff_block *[2], struct diff_block *[2], int, int, struct diff3_block const *);
static struct diff_block *process_diff (char const *, char const *, char **);
static void check_stdout (void);
static _Noreturn void fatal (char const *);
static void output_diff3 (FILE *, struct diff3_block *, int const[3], int const[3]);
static _Noreturn void perror_with_exit (char const *);
static _Noreturn void try_help (char const *, char const *);
static void usage (void);

static char const *diff_program = DEFAULT_DIFF_PROGRAM;

/* Values for long options that do not have single-letter equivalents.  */
enum
{
  DIFF_PROGRAM_OPTION = CHAR_MAX + 1,
  HELP_OPTION,
  STRIP_TRAILING_CR_OPTION
};

static struct option const longopts[] =
{
  {"diff-program", 1, 0, DIFF_PROGRAM_OPTION},
  {"easy-only", 0, 0, '3'},
  {"ed", 0, 0, 'e'},
  {"help", 0, 0, HELP_OPTION},
  {"initial-tab", 0, 0, 'T'},
  {"label", 1, 0, 'L'},
  {"merge", 0, 0, 'm'},
  {"overlap-only", 0, 0, 'x'},
  {"show-all", 0, 0, 'A'},
  {"show-overlap", 0, 0, 'E'},
  {"strip-trailing-cr", 0, 0, STRIP_TRAILING_CR_OPTION},
  {"text", 0, 0, 'a'},
  {"version", 0, 0, 'v'},
  {0, 0, 0, 0}
};

static void
free_diff_block (struct diff_block *p)
{
#ifndef GCC_LINT
  (void)p;
#else
  while (p)
    {
      free (p->lines[0]);
      free (p->lines[1]);
      free (p->lengths[0]);
      free (p->lengths[1]);
      struct diff_block *next = p->n2;
      free (p);
      p = next;
    }
#endif
}

/* Copy each next pointer to n2, since make_3way_diff would clobber the former,
   yet we will still need something to free these buffers.  */
static void
next_to_n2 (struct diff_block *p)
{
#ifndef GCC_LINT
  (void)p;
#else
  while (p)
    p = p->n2 = p->next;
#endif
}

int
main (int argc, char **argv)
{
  int c, i;
  int common;
  int mapping[3];
  int rev_mapping[3];
  int incompat = 0;
  enum { OPTION_3, OPTION_A, OPTION_E, OPTION_X, OPTION_e, OPTION_x };
  bool conflicts_found;
  struct diff_block *thread0, *thread1;
  struct diff3_block *diff3;
  int tag_count = 0;
  char *tag_strings[3];
  char *commonname;
  char **file;

  exit_failure = EXIT_TROUBLE;
  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
  c_stack_action (0);
  xstdopen ();

  while ((c = getopt_long (argc, argv, "aeimvx3AEL:TX", longopts, 0)) != -1)
    {
      switch (c)
        {
        case 'a':
          text = true;
          break;
        case 'A':
          show_2nd = true;
          flagging = true;
          incompat |= 1 << OPTION_A;
          break;
        case 'x':
          overlap_only = true;
          incompat |= 1 << OPTION_x;
          break;
        case '3':
          simple_only = true;
          incompat |= 1 << OPTION_3;
          break;
        case 'i':
          finalwrite = true;
          break;
        case 'm':
          merge = true;
          break;
        case 'X':
          overlap_only = true;
          incompat |= 1 << OPTION_X;
          break;
        case 'E':
          flagging = true;
          incompat |= 1 << OPTION_E;
          break;
        case 'e':
          incompat |= 1 << OPTION_e;
          break;
        case 'T':
          initial_tab = true;
          break;
        case STRIP_TRAILING_CR_OPTION:
          strip_trailing_cr = true;
          break;
        case 'v':
          version_etc (stdout, PROGRAM_NAME, PACKAGE_NAME, Version,
                       AUTHORS, nullptr);
          check_stdout ();
          return EXIT_SUCCESS;
        case DIFF_PROGRAM_OPTION:
          diff_program = optarg;
          break;
        case HELP_OPTION:
          usage ();
          check_stdout ();
          return EXIT_SUCCESS;
        case 'L':
          /* Handle up to three -L options.  */
          if (tag_count < 3)
            {
              tag_strings[tag_count++] = optarg;
              break;
            }
          try_help ("too many file label options", 0);
        default:
          try_help (0, 0);
        }
    }

  /* -AeExX3 without -m implies ed script.  */
  edscript = !!incompat & !merge;

  show_2nd |= !incompat & merge;  /* -m without -AeExX3 implies -A.  */
  flagging |= !incompat & merge;

  if (incompat & (incompat - 1)  /* Ensure at most one of -AeExX3.  */
      || finalwrite & merge /* -i -m would rewrite input file.  */
      || (tag_count && ! flagging)) /* -L requires one of -AEX.  */
    try_help ("incompatible options", 0);

  if (argc - optind != 3)
    {
      if (argc - optind < 3)
        try_help ("missing operand after '%s'", argv[argc - 1]);
      else
        try_help ("extra operand '%s'", argv[optind + 3]);
    }

  file = &argv[optind];

  for (i = tag_count; i < 3; i++)
    tag_strings[i] = file[i];

  /* Always compare file1 to file2, even if file2 is "-".
     This is needed for -mAeExX3.  Using the file0 as
     the common file would produce wrong results, because if the
     file0-file1 diffs didn't line up with the file0-file2 diffs
     (which is entirely possible since we don't use diff's -n option),
     diff3 might report phantom changes from file1 to file2.

     Also, try to compare file0 to file1, because this is where
     changes are expected to come from.  Diffing between these pairs
     of files is more likely to avoid phantom changes from file0 to file1.

     Historically, the default common file was file2, so some older
     applications (e.g. Emacs ediff) used file2 as the ancestor.  So,
     for compatibility, if this is a 3-way diff (not a merge or
     edscript), prefer file2 as the common file.  */

  common = 2 - (edscript | merge);

  if (STREQ (file[common], "-"))
    {
      /* Sigh.  We've got standard input as the common file.  We can't
         call diff twice on stdin.  Use the other arg as the common
         file instead.  */
      common = 3 - common;
      if (STREQ (file[0], "-") || STREQ (file[common], "-"))
        fatal ("'-' specified for more than one input file");
    }

  mapping[0] = 0;
  mapping[1] = 3 - common;
  mapping[2] = common;

  for (i = 0; i < 3; i++)
    rev_mapping[mapping[i]] = i;

#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif

  /* Invoke diff twice on two pairs of input files, combine the two
     diffs, and output them.  */

  char *b0, *b1;
  commonname = file[rev_mapping[FILEC]];
  thread1 = process_diff (file[rev_mapping[FILE1]], commonname, &b1);
  thread0 = process_diff (file[rev_mapping[FILE0]], commonname, &b0);

  next_to_n2 (thread0);
  next_to_n2 (thread1);

  diff3 = make_3way_diff (thread0, thread1);

  free_diff_block (thread0);
  free_diff_block (thread1);

  if (edscript)
    conflicts_found
      = output_diff3_edscript (stdout, diff3, mapping, rev_mapping,
                               tag_strings[0], tag_strings[1], tag_strings[2]);
  else if (merge)
    {
      xfreopen (file[rev_mapping[FILE0]], "r", stdin);
      conflicts_found
        = output_diff3_merge (stdin, stdout, diff3, mapping, rev_mapping,
                              tag_strings[0], tag_strings[1], tag_strings[2]);
      if (ferror (stdin))
        fatal ("read failed");
    }
  else
    {
      output_diff3 (stdout, diff3, mapping, rev_mapping);
      conflicts_found = false;
    }

  free (b0);
  free (b1);
  check_stdout ();
  exit (conflicts_found);
}

static void
try_help (char const *reason_msgid, char const *operand)
{
  if (reason_msgid)
    error (0, 0, _(reason_msgid), operand);
  die (EXIT_TROUBLE, 0,
         _("Try '%s --help' for more information."), program_name);
}

static void
check_stdout (void)
{
  if (ferror (stdout))
    fatal ("write failed");
  else if (fclose (stdout) != 0)
    perror_with_exit (_("standard output"));
}

static char const * const option_help_msgid[] = {
  N_("-A, --show-all              output all changes, bracketing conflicts"),
  "",
  N_("-e, --ed                    output ed script incorporating changes\n"
     "                                from OLDFILE to YOURFILE into MYFILE"),
  N_("-E, --show-overlap          like -e, but bracket conflicts"),
  N_("-3, --easy-only             like -e, but incorporate only nonoverlapping changes"),
  N_("-x, --overlap-only          like -e, but incorporate only overlapping changes"),
  N_("-X                          like -x, but bracket conflicts"),
  N_("-i                          append 'w' and 'q' commands to ed scripts"),
  "",
  N_("-m, --merge                 output actual merged file, according to\n"
     "                                -A if no other options are given"),
  "",
  N_("-a, --text                  treat all files as text"),
  N_("    --strip-trailing-cr     strip trailing carriage return on input"),
  N_("-T, --initial-tab           make tabs line up by prepending a tab"),
  N_("    --diff-program=PROGRAM  use PROGRAM to compare files"),
  N_("-L, --label=LABEL           use LABEL instead of file name\n"
     "                                (can be repeated up to three times)"),
  "",
  N_("    --help                  display this help and exit"),
  N_("-v, --version               output version information and exit"),
  0
};

static void
usage (void)
{
  char const * const *p;

  printf (_("Usage: %s [OPTION]... MYFILE OLDFILE YOURFILE\n"),
          program_name);
  printf ("%s\n\n", _("Compare three files line by line."));

  fputs (_("\
Mandatory arguments to long options are mandatory for short options too.\n\
"), stdout);
  for (p = option_help_msgid;  *p;  p++)
    if (**p)
      printf ("  %s\n", _(*p));
    else
      putchar ('\n');
  fputs (_("\n\
The default output format is a somewhat human-readable representation of\n\
the changes.\n\
\n\
The -e, -E, -x, -X (and corresponding long) options cause an ed script\n\
to be output instead of the default.\n\
\n\
Finally, the -m (--merge) option causes diff3 to do the merge internally\n\
and output the actual merged file.  For unusual input, this is more\n\
robust than using ed.\n"), stdout);
  printf ("\n%s\n%s\n",
          _("If a FILE is '-', read standard input."),
          _("Exit status is 0 if successful, 1 if conflicts, 2 if trouble."));
  emit_bug_reporting_address ();
}

/* Combine the two diffs together into one.
   Here is the algorithm:

     File2 is shared in common between the two diffs.
     Diff02 is the diff between 0 and 2.
     Diff12 is the diff between 1 and 2.

        1) Find the range for the first block in File2.
            a) Take the lowest of the two ranges (in File2) in the two
               current blocks (one from each diff) as being the low
               water mark.  Assign the upper end of this block as
               being the high water mark and move the current block up
               one.  Mark the block just moved over as to be used.
            b) Check the next block in the diff that the high water
               mark is *not* from.

               *If* the high water mark is above
               the low end of the range in that block,

                   mark that block as to be used and move the current
                   block up.  Set the high water mark to the max of
                   the high end of this block and the current.  Repeat b.

         2) Find the corresponding ranges in File0 (from the blocks
            in diff02; line per line outside of diffs) and in File1.
            Create a diff3_block, reserving space as indicated by the ranges.

         3) Copy all of the pointers for file2 in.  At least for now,
            do memcmp's between corresponding strings in the two diffs.

         4) Copy all of the pointers for file0 and 1 in.  Get what is
            needed from file2 (when there isn't a diff block, it's
            identical to file2 within the range between diff blocks).

         5) If the diff blocks used came from only one of the two
            strings of diffs, then that file (i.e. the one other than
            the common file in that diff) is the odd person out.  If
            diff blocks are used from both sets, check to see if files
            0 and 1 match:

                Same number of lines?  If so, do a set of memcmp's (if
            a memcmp matches; copy the pointer over; it'll be easier
            later during comparisons).  If they match, 0 & 1 are the
            same.  If not, all three different.

     Then do it again, until the blocks are exhausted.  */


/* Make a three way diff (chain of diff3_block's) from two two way
   diffs (chains of diff_block's).  Assume that each of the two diffs
   passed are onto the same file (i.e. that each of the diffs were
   made "to" the same file).  Return a three way diff pointer with
   numbering FILE0 = the other file in diff02, FILE1 = the other file
   in diff12, and FILEC = the common file.  */

static struct diff3_block *
make_3way_diff (struct diff_block *thread0, struct diff_block *thread1)
{
  /* Work on the two diffs passed to it as threads.  Thread number 0
     is diff02, thread number 1 is diff12.  USING is the base of the
     list of blocks to be used to construct each block of the three
     way diff; if no blocks from a particular thread are to be used,
     that element of USING is 0.  LAST_USING contains the last
     elements on each of the using lists.

     HIGH_WATER_MARK is the highest line number in the common file
     described in any of the diffs in either of the USING lists.
     HIGH_WATER_THREAD names the thread.  Similarly BASE_WATER_MARK
     and BASE_WATER_THREAD describe the lowest line number in the
     common file described in any of the diffs in either of the USING
     lists.  HIGH_WATER_DIFF is the diff from which the
     HIGH_WATER_MARK was taken.

     HIGH_WATER_DIFF should always be equal to
     LAST_USING[HIGH_WATER_THREAD].  OTHER_DIFF is the next diff to
     check for higher water, and should always be equal to
     CURRENT[HIGH_WATER_THREAD ^ 1].  OTHER_THREAD is the thread in
     which the OTHER_DIFF is, and hence should always be equal to
     HIGH_WATER_THREAD ^ 1.

     LAST_DIFF is the last diff block produced by this routine, for
     line correspondence purposes between that diff and the one
     currently being worked on.  It is ZERO_DIFF before any blocks
     have been created.  */

  struct diff_block *using[2];
  struct diff_block *last_using[2];
  struct diff_block *current[2];

  lin high_water_mark;

  int high_water_thread;
  int base_water_thread;
  int other_thread;

  struct diff_block *high_water_diff;
  struct diff_block *other_diff;

  struct diff3_block *result;
  struct diff3_block *tmpblock;
  struct diff3_block **result_end;

  struct diff3_block const *last_diff3;

  static struct diff3_block const zero_diff3;

  /* Initialization */
  result = 0;
  result_end = &result;
  current[0] = thread0; current[1] = thread1;
  last_diff3 = &zero_diff3;

  /* Sniff up the threads until we reach the end */

  while (current[0] || current[1])
    {
      using[0] = using[1] = last_using[0] = last_using[1] = 0;

      /* Setup low and high water threads, diffs, and marks.  */
      if (!current[0])
        base_water_thread = 1;
      else if (!current[1])
        base_water_thread = 0;
      else
        base_water_thread =
          (D_LOWLINE (current[0], FC) > D_LOWLINE (current[1], FC));

      high_water_thread = base_water_thread;

      high_water_diff = current[high_water_thread];

      high_water_mark = D_HIGHLINE (high_water_diff, FC);

      /* Make the diff you just got info from into the using class */
      using[high_water_thread]
        = last_using[high_water_thread]
        = high_water_diff;
      current[high_water_thread] = high_water_diff->next;
      last_using[high_water_thread]->next = 0;

      /* And mark the other diff */
      other_thread = high_water_thread ^ 0x1;
      other_diff = current[other_thread];

      /* Shuffle up the ladder, checking the other diff to see if it
         needs to be incorporated.  */
      while (other_diff
             && D_LOWLINE (other_diff, FC) <= high_water_mark + 1)
        {

          /* Incorporate this diff into the using list.  Note that
             this doesn't take it off the current list */
          if (using[other_thread])
            last_using[other_thread]->next = other_diff;
          else
            using[other_thread] = other_diff;
          last_using[other_thread] = other_diff;

          /* Take it off the current list.  Note that this following
             code assumes that other_diff enters it equal to
             current[high_water_thread ^ 0x1] */
          current[other_thread] = current[other_thread]->next;
          other_diff->next = 0;

          /* Set the high_water stuff
             If this comparison is equal, then this is the last pass
             through this loop; since diff blocks within a given
             thread cannot overlap, the high_water_mark will be
             *below* the range_start of either of the next diffs.  */

          if (high_water_mark < D_HIGHLINE (other_diff, FC))
            {
              high_water_thread ^= 1;
              high_water_mark = D_HIGHLINE (other_diff, FC);
            }

          /* Set the other diff */
          other_thread = high_water_thread ^ 0x1;
          other_diff = current[other_thread];
        }

      /* The using lists contain a list of all of the blocks to be
         included in this diff3_block.  Create it.  */

      tmpblock = using_to_diff3_block (using, last_using,
                                       base_water_thread, high_water_thread,
                                       last_diff3);

      if (!tmpblock)
        fatal ("internal error: screwup in format of diff blocks");

      /* Put it on the list.  */
      *result_end = tmpblock;
      result_end = &tmpblock->next;

      /* Set up corresponding lines correctly.  */
      last_diff3 = tmpblock;
    }
  return result;
}

/* Take two lists of blocks (from two separate diff threads) and put
   them together into one diff3 block.  Return a pointer to this diff3
   block or 0 for failure.

   All arguments besides using are for the convenience of the routine;
   they could be derived from the using array.  LAST_USING is a pair
   of pointers to the last blocks in the using structure.  LOW_THREAD
   and HIGH_THREAD tell which threads contain the lowest and highest
   line numbers for File0.  LAST_DIFF3 contains the last diff produced
   in the calling routine.  This is used for lines mappings that
   would still be identical to the state that diff ended in.

   A distinction should be made in this routine between the two diffs
   that are part of a normal two diff block, and the three diffs that
   are part of a diff3_block.  */

static struct diff3_block *
using_to_diff3_block (struct diff_block *using[2],
                      struct diff_block *last_using[2],
                      int low_thread, int high_thread,
                      struct diff3_block const *last_diff3)
{
  lin low[2], high[2];
  struct diff3_block *result;
  struct diff_block *ptr;
  int d;
  lin i;

  /* Find the range in the common file.  */
  lin lowc = D_LOWLINE (using[low_thread], FC);
  lin highc = D_HIGHLINE (last_using[high_thread], FC);

  /* Find the ranges in the other files.
     If using[d] is null, that means that the file to which that diff
     refers is equivalent to the common file over this range.  */

  for (d = 0; d < 2; d++)
    if (using[d])
      {
        low[d] = D_LOW_MAPLINE (using[d], FC, FO, lowc);
        high[d] = D_HIGH_MAPLINE (last_using[d], FC, FO, highc);
      }
    else
      {
        low[d] = D_HIGH_MAPLINE (last_diff3, FILEC, FILE0 + d, lowc);
        high[d] = D_HIGH_MAPLINE (last_diff3, FILEC, FILE0 + d, highc);
      }

  /* Create a block with the appropriate sizes */
  result = create_diff3_block (low[0], high[0], low[1], high[1], lowc, highc);

  /* Copy information for the common file.
     Return with a zero if any of the compares failed.  */

  for (d = 0; d < 2; d++)
    for (ptr = using[d]; ptr; ptr = D_NEXT (ptr))
      {
        lin result_offset = D_LOWLINE (ptr, FC) - lowc;

        if (!copy_stringlist (D_LINEARRAY (ptr, FC),
                              D_LENARRAY (ptr, FC),
                              D_LINEARRAY (result, FILEC) + result_offset,
                              D_LENARRAY (result, FILEC) + result_offset,
                              D_NUMLINES (ptr, FC)))
          return 0;
      }

  /* Copy information for file d.  First deal with anything that might be
     before the first diff.  */

  for (d = 0; d < 2; d++)
    {
      struct diff_block *u = using[d];
      lin lo = low[d], hi = high[d];

      for (i = 0;
           i + lo < (u ? D_LOWLINE (u, FO) : hi + 1);
           i++)
        {
          D_RELNUM (result, FILE0 + d, i) = D_RELNUM (result, FILEC, i);
          D_RELLEN (result, FILE0 + d, i) = D_RELLEN (result, FILEC, i);
        }

      for (ptr = u; ptr; ptr = D_NEXT (ptr))
        {
          lin result_offset = D_LOWLINE (ptr, FO) - lo;
          lin linec;

          if (!copy_stringlist (D_LINEARRAY (ptr, FO),
                                D_LENARRAY (ptr, FO),
                                D_LINEARRAY (result, FILE0 + d) + result_offset,
                                D_LENARRAY (result, FILE0 + d) + result_offset,
                                D_NUMLINES (ptr, FO)))
            return 0;

          /* Catch the lines between here and the next diff */
          linec = D_HIGHLINE (ptr, FC) + 1 - lowc;
          for (i = D_HIGHLINE (ptr, FO) + 1 - lo;
               i < (D_NEXT (ptr) ? D_LOWLINE (D_NEXT (ptr), FO) : hi + 1) - lo;
               i++)
            {
              D_RELNUM (result, FILE0 + d, i) = D_RELNUM (result, FILEC, linec);
              D_RELLEN (result, FILE0 + d, i) = D_RELLEN (result, FILEC, linec);
              linec++;
            }
        }
    }

  /* Set correspond */
  if (!using[0])
    D3_TYPE (result) = DIFF_2ND;
  else if (!using[1])
    D3_TYPE (result) = DIFF_1ST;
  else
    {
      lin nl0 = D_NUMLINES (result, FILE0);
      lin nl1 = D_NUMLINES (result, FILE1);

      if (nl0 != nl1
          || !compare_line_list (D_LINEARRAY (result, FILE0),
                                 D_LENARRAY (result, FILE0),
                                 D_LINEARRAY (result, FILE1),
                                 D_LENARRAY (result, FILE1),
                                 nl0))
        D3_TYPE (result) = DIFF_ALL;
      else
        D3_TYPE (result) = DIFF_3RD;
    }

  return result;
}

/* Copy pointers from a list of strings to a different list of
   strings.  If a spot in the second list is already filled, make sure
   that it is filled with the same string; if not, return false, the copy
   incomplete.  Upon successful completion of the copy, return true.  */

static bool
copy_stringlist (char * const fromptrs[], size_t const fromlengths[],
                 char *toptrs[], size_t tolengths[],
                 lin copynum)
{
  register char * const *f = fromptrs;
  register char **t = toptrs;
  register size_t const *fl = fromlengths;
  register size_t *tl = tolengths;

  while (copynum--)
    {
      if (*t)
        {
          if (*fl != *tl || memcmp (*f, *t, *fl) != 0)
            return false;
        }
      else
        {
          *t = *f;
          *tl = *fl;
        }

      t++; f++; tl++; fl++;
    }

  return true;
}

/* Create a diff3_block, with ranges as specified in the arguments.
   Allocate the arrays for the various pointers (and zero them) based
   on the arguments passed.  Return the block as a result.  */

static struct diff3_block *
create_diff3_block (lin low0, lin high0,
                    lin low1, lin high1,
                    lin low2, lin high2)
{
  struct diff3_block *result = xmalloc (sizeof *result);
  lin numlines;

  D3_TYPE (result) = DIFF_ERROR;
  D_NEXT (result) = 0;

  /* Assign ranges */
  D_LOWLINE (result, FILE0) = low0;
  D_HIGHLINE (result, FILE0) = high0;
  D_LOWLINE (result, FILE1) = low1;
  D_HIGHLINE (result, FILE1) = high1;
  D_LOWLINE (result, FILE2) = low2;
  D_HIGHLINE (result, FILE2) = high2;

  /* Allocate and zero space */
  numlines = D_NUMLINES (result, FILE0);
  if (numlines)
    {
      D_LINEARRAY (result, FILE0) = xcalloc (numlines, sizeof (char *));
      D_LENARRAY (result, FILE0) = xcalloc (numlines, sizeof (size_t));
    }
  else
    {
      D_LINEARRAY (result, FILE0) = 0;
      D_LENARRAY (result, FILE0) = 0;
    }

  numlines = D_NUMLINES (result, FILE1);
  if (numlines)
    {
      D_LINEARRAY (result, FILE1) = xcalloc (numlines, sizeof (char *));
      D_LENARRAY (result, FILE1) = xcalloc (numlines, sizeof (size_t));
    }
  else
    {
      D_LINEARRAY (result, FILE1) = 0;
      D_LENARRAY (result, FILE1) = 0;
    }

  numlines = D_NUMLINES (result, FILE2);
  if (numlines)
    {
      D_LINEARRAY (result, FILE2) = xcalloc (numlines, sizeof (char *));
      D_LENARRAY (result, FILE2) = xcalloc (numlines, sizeof (size_t));
    }
  else
    {
      D_LINEARRAY (result, FILE2) = 0;
      D_LENARRAY (result, FILE2) = 0;
    }

  /* Return */
  return result;
}

/* Compare two lists of lines of text.
   Return 1 if they are equivalent, 0 if not.  */

static bool
compare_line_list (char * const list1[], size_t const lengths1[],
                   char * const list2[], size_t const lengths2[],
                   lin nl)
{
  char * const *l1 = list1;
  char * const *l2 = list2;
  size_t const *lgths1 = lengths1;
  size_t const *lgths2 = lengths2;

  while (nl--)
    if (!*l1 || !*l2 || *lgths1 != *lgths2++
        || memcmp (*l1++, *l2++, *lgths1++) != 0)
      return false;
  return true;
}

/* Input and parse two way diffs.  */

static struct diff_block *
process_diff (char const *filea,
              char const *fileb,
              char **buf_to_free)
{
  char *diff_contents;
  char *diff_limit;
  char *scan_diff;
  enum diff_type dt;
  lin i;
  struct diff_block *block_list;
  struct diff_block **block_list_end = &block_list;

  diff_limit = read_diff (filea, fileb, &diff_contents);
  *buf_to_free = diff_contents;
  scan_diff = diff_contents;

  while (scan_diff < diff_limit)
    {
      struct diff_block *bptr = xmalloc (sizeof *bptr);
      bptr->lines[0] = bptr->lines[1] = 0;
      bptr->lengths[0] = bptr->lengths[1] = 0;

      dt = process_diff_control (&scan_diff, bptr);
      if (dt == DIFF_ERROR || *scan_diff != '\n')
        {
          fprintf (stderr, _("%s: diff failed: "), program_name);
          do
            {
              putc (*scan_diff, stderr);
            }
          while (*scan_diff++ != '\n');
          exit (EXIT_TROUBLE);
        }
      scan_diff++;

      /* Force appropriate ranges to be null, if necessary */
      switch (dt)
        {
        case DIFF_ADD:
          bptr->ranges[0][0]++;
          break;
        case DIFF_DELETE:
          bptr->ranges[1][0]++;
          break;
        case DIFF_CHANGE:
          break;
        default:
          fatal ("internal error: invalid diff type in process_diff");
          break;
        }

      /* Allocate space for the pointers for the lines from filea, and
         parcel them out among these pointers */
      if (dt != DIFF_ADD)
        {
          lin numlines = D_NUMLINES (bptr, 0);
          bptr->lines[0] = xnmalloc (numlines, sizeof *bptr->lines[0]);
          bptr->lengths[0] = xnmalloc (numlines, sizeof *bptr->lengths[0]);
          for (i = 0; i < numlines; i++)
            scan_diff = scan_diff_line (scan_diff,
                                        &(bptr->lines[0][i]),
                                        &(bptr->lengths[0][i]),
                                        diff_limit,
                                        '<');
        }

      /* Get past the separator for changes */
      if (dt == DIFF_CHANGE)
        {
          if (strncmp (scan_diff, "---\n", 4))
            fatal ("invalid diff format; invalid change separator");
          scan_diff += 4;
        }

      /* Allocate space for the pointers for the lines from fileb, and
         parcel them out among these pointers */
      if (dt != DIFF_DELETE)
        {
          lin numlines = D_NUMLINES (bptr, 1);
          bptr->lines[1] = xnmalloc (numlines, sizeof *bptr->lines[1]);
          bptr->lengths[1] = xnmalloc (numlines, sizeof *bptr->lengths[1]);
          for (i = 0; i < numlines; i++)
            scan_diff = scan_diff_line (scan_diff,
                                        &(bptr->lines[1][i]),
                                        &(bptr->lengths[1][i]),
                                        diff_limit,
                                        '>');
        }

      /* Place this block on the blocklist.  */
      *block_list_end = bptr;
      block_list_end = &bptr->next;
    }

  *block_list_end = nullptr;
  return block_list;
}

/* Skip tabs and spaces, and return the first character after them.  */

static char * ATTRIBUTE_PURE
skipwhite (char *s)
{
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

/* Read a nonnegative line number from S, returning the address of the
   first character after the line number, and storing the number into
   *PNUM.  Return 0 if S does not point to a valid line number.  */

static char *
readnum (char *s, lin *pnum)
{
  unsigned char c = *s;
  lin num = 0;

  if (! ISDIGIT (c))
    return 0;

  do
    {
      num = c - '0' + num * 10;
      c = *++s;
    }
  while (ISDIGIT (c));

  *pnum = num;
  return s;
}

/* Parse a normal format diff control string.  Return the type of the
   diff (DIFF_ERROR if the format is bad).  All of the other important
   information is filled into to the structure pointed to by db, and
   the string pointer (whose location is passed to this routine) is
   updated to point beyond the end of the string parsed.  Note that
   only the ranges in the diff_block will be set by this routine.

   If some specific pair of numbers has been reduced to a single
   number, then both corresponding numbers in the diff block are set
   to that number.  In general these numbers are interpreted as ranges
   inclusive, unless being used by the DIFF_ADD or DIFF_DELETE commands.
   It is assumed that these will be special cased in a superior routine.  */

static enum diff_type
process_diff_control (char **string, struct diff_block *db)
{
  char *s = *string;
  enum diff_type type;

  /* Read first set of digits */
  s = readnum (skipwhite (s), &db->ranges[0][RANGE_START]);
  if (! s)
    return DIFF_ERROR;

  /* Was that the only digit? */
  s = skipwhite (s);
  if (*s == ',')
    {
      s = readnum (s + 1, &db->ranges[0][RANGE_END]);
      if (! s)
        return DIFF_ERROR;
    }
  else
    db->ranges[0][RANGE_END] = db->ranges[0][RANGE_START];

  /* Get the letter */
  s = skipwhite (s);
  switch (*s)
    {
    case 'a':
      type = DIFF_ADD;
      break;
    case 'c':
      type = DIFF_CHANGE;
      break;
    case 'd':
      type = DIFF_DELETE;
      break;
    default:
      return DIFF_ERROR;	/* Bad format */
    }
  s++;				/* Past letter */

  /* Read second set of digits */
  s = readnum (skipwhite (s), &db->ranges[1][RANGE_START]);
  if (! s)
    return DIFF_ERROR;

  /* Was that the only digit? */
  s = skipwhite (s);
  if (*s == ',')
    {
      s = readnum (s + 1, &db->ranges[1][RANGE_END]);
      if (! s)
        return DIFF_ERROR;
      s = skipwhite (s);		/* To move to end */
    }
  else
    db->ranges[1][RANGE_END] = db->ranges[1][RANGE_START];

  *string = s;
  return type;
}

static char *
read_diff (char const *filea,
           char const *fileb,
           char **output_placement)
{
  char *diff_result;
  size_t current_chunk_size, total;
  int fd, wstatus, status;
  int werrno = 0;
  struct stat pipestat;
  char const *argv[10];
  char const **ap;
#if HAVE_WORKING_FORK
  int fds[2];
  pid_t pid;
#else
  FILE *fpipe;
  char *command;
#endif

  ap = argv;
  *ap++ = diff_program;
  if (text)
    *ap++ = "-a";
  if (strip_trailing_cr)
    *ap++ = "--strip-trailing-cr";
  *ap++ = "--horizon-lines=100";
  *ap++ = "---no-directory";
  *ap++ = "--";
  *ap++ = filea;
  *ap++ = fileb;
  *ap = 0;

#if HAVE_WORKING_FORK

  if (pipe (fds) != 0)
    perror_with_exit ("pipe");

  pid = fork ();
  if (pid == 0)
    {
      /* Child */
      close (fds[0]);
      if (fds[1] != STDOUT_FILENO)
        {
          dup2 (fds[1], STDOUT_FILENO);
          close (fds[1]);
        }

      /* The cast to (char **) is needed for portability to older
         hosts with a nonstandard prototype for execvp.  */
      execvp (diff_program, (char **) argv);

      _exit (errno == ENOENT ? 127 : 126);
    }

  if (pid == -1)
    perror_with_exit ("fork");

  close (fds[1]);		/* Prevent erroneous lack of EOF */
  fd = fds[0];

#else

  command = system_quote_argv (SCI_SYSTEM, (char **) argv);
  errno = 0;
  fpipe = popen (command, "r");
  if (!fpipe)
    perror_with_exit (command);
  free (command);
  fd = fileno (fpipe);

#endif

  current_chunk_size = (fstat (fd, &pipestat) == 0
                        ? MAX (1, STAT_BLOCKSIZE (pipestat))
                        : 8 * 1024);
  diff_result = xmalloc (current_chunk_size);
  total = 0;

  for (;;)
    {
      size_t bytes_to_read = current_chunk_size - total;
      size_t bytes = block_read (fd, diff_result + total, bytes_to_read);
      total += bytes;
      if (bytes != bytes_to_read)
        {
          if (bytes == SIZE_MAX)
            perror_with_exit (_("read failed"));
          break;
        }
      if (PTRDIFF_MAX / 2 <= current_chunk_size)
        xalloc_die ();
      current_chunk_size *= 2;
      diff_result = xrealloc (diff_result, current_chunk_size);
    }

  if (total != 0 && diff_result[total-1] != '\n')
    fatal ("invalid diff format; incomplete last line");

  *output_placement = diff_result;

#if ! HAVE_WORKING_FORK

  wstatus = pclose (fpipe);
  if (wstatus == -1)
    werrno = errno;

#else

  if (close (fd) != 0)
    perror_with_exit ("close");
  if (waitpid (pid, &wstatus, 0) < 0)
    perror_with_exit ("waitpid");

#endif

  status = ! werrno && WIFEXITED (wstatus) ? WEXITSTATUS (wstatus) : INT_MAX;

  if (EXIT_TROUBLE <= status)
    die (EXIT_TROUBLE, werrno,
           _(status == 126
             ? "subsidiary program '%s' could not be invoked"
             : status == 127
             ? "subsidiary program '%s' not found"
             : status == INT_MAX
             ? "subsidiary program '%s' failed"
             : "subsidiary program '%s' failed (exit status %d)"),
           diff_program, status);

  return diff_result + total;
}


/* Scan a regular diff line (consisting of > or <, followed by a
   space, followed by text (including nulls) up to a newline.

   This next routine began life as a macro and many parameters in it
   are used as call-by-reference values.  */
static char *
scan_diff_line (char *scan_ptr, char **set_start, size_t *set_length,
                char *limit, char leadingchar)
{
  char *line_ptr;

  if (!(scan_ptr[0] == leadingchar
        && scan_ptr[1] == ' '))
    fatal ("invalid diff format; incorrect leading line chars");

  *set_start = line_ptr = scan_ptr + 2;
  while (*line_ptr++ != '\n')
    continue;

  /* Include newline if the original line ended in a newline,
     or if an edit script is being generated.
     Copy any missing newline message to stderr if an edit script is being
     generated, because edit scripts cannot handle missing newlines.
     Return the beginning of the next line.  */
  *set_length = line_ptr - *set_start;
  if (line_ptr < limit && *line_ptr == '\\')
    {
      if (edscript)
        fprintf (stderr, "%s:", program_name);
      else
        --*set_length;
      line_ptr++;
      do
        {
          if (edscript)
            putc (*line_ptr, stderr);
        }
      while (*line_ptr++ != '\n');
    }

  return line_ptr;
}

/* Output a three way diff passed as a list of diff3_block's.  The
   argument MAPPING is indexed by external file number (in the
   argument list) and contains the internal file number (from the diff
   passed).  This is important because the user expects outputs in
   terms of the argument list number, and the diff passed may have
   been done slightly differently (if the last argument was "-", for
   example).  REV_MAPPING is the inverse of MAPPING.  */

static void
output_diff3 (FILE *outputfile, struct diff3_block *diff,
              int const mapping[3], int const rev_mapping[3])
{
  int i;
  int oddoneout;
  char *cp;
  struct diff3_block *ptr;
  lin line;
  size_t length;
  int dontprint;
  static int skew_increment[3] = { 2, 3, 1 }; /* 0==>2==>1==>3 */
  char const *line_prefix = initial_tab ? "\t" : "  ";

  for (ptr = diff; ptr; ptr = D_NEXT (ptr))
    {
      char x[2];

      switch (ptr->correspond)
        {
        case DIFF_ALL:
          x[0] = 0;
          dontprint = 3;	/* Print them all */
          oddoneout = 3;	/* Nobody's odder than anyone else */
          break;
        case DIFF_1ST:
        case DIFF_2ND:
        case DIFF_3RD:
          oddoneout = rev_mapping[ptr->correspond - DIFF_1ST];

          x[0] = oddoneout + '1';
          x[1] = 0;
          dontprint = oddoneout == 0;
          break;
        default:
          fatal ("internal error: invalid diff type passed to output");
        }
      fprintf (outputfile, "====%s\n", x);

      /* Go 0, 2, 1 if the first and third outputs are equivalent.  */
      for (i = 0; i < 3;
           i = (oddoneout == 1 ? skew_increment[i] : i + 1))
        {
          int realfile = mapping[i];
          lin lowt = D_LOWLINE (ptr, realfile);
          lin hight = D_HIGHLINE (ptr, realfile);

          fprintf (outputfile, "%d:", i + 1);
          switch (lowt - hight)
            {
            case 1:
              fprintf (outputfile, "%"pI"da\n", lowt - 1);
              break;
            case 0:
              fprintf (outputfile, "%"pI"dc\n", lowt);
              break;
            default:
              fprintf (outputfile, "%"pI"d,%"pI"dc\n", lowt, hight);
              break;
            }

          if (i == dontprint) continue;

          if (lowt <= hight)
            {
              line = 0;
              do
                {
                  fputs (line_prefix, outputfile);
                  cp = D_RELNUM (ptr, realfile, line);
                  length = D_RELLEN (ptr, realfile, line);
                  fwrite (cp, sizeof (char), length, outputfile);
                }
              while (++line < hight - lowt + 1);
              if (cp[length - 1] != '\n')
                fprintf (outputfile, "\n\\ %s\n",
                         _("No newline at end of file"));
            }
        }
    }
}


/* Output to OUTPUTFILE the lines of B taken from FILENUM.  Double any
   initial '.'s; yield nonzero if any initial '.'s were doubled.  */

static bool
dotlines (FILE *outputfile, struct diff3_block *b, int filenum)
{
  lin i;
  bool leading_dot = false;

  for (i = 0;
       i < D_NUMLINES (b, filenum);
       i++)
    {
      char *line = D_RELNUM (b, filenum, i);
      if (line[0] == '.')
        {
          leading_dot = true;
          fputc ('.', outputfile);
        }
      fwrite (line, sizeof (char),
              D_RELLEN (b, filenum, i), outputfile);
    }

  return leading_dot;
}

/* Output to OUTPUTFILE a '.' line.  If LEADING_DOT is true, also
   output a command that removes initial '.'s starting with line START
   and continuing for NUM lines.  */

static void
undotlines (FILE *outputfile, bool leading_dot, lin start, lin num)
{
  fputs (".\n", outputfile);
  if (leading_dot)
    {
      if (num == 1)
        fprintf (outputfile, "%"pI"ds/^\\.//\n", start);
      else
        fprintf (outputfile, "%"pI"d,%"pI"ds/^\\.//\n", start, start + num - 1);
    }
}

/* Output a diff3 set of blocks as an ed script.  This script applies
   the changes between file's 2 & 3 to file 1.  Take the precise
   format of the ed script to be output from global variables set
   during options processing.  Reverse the order of
   the set of diff3 blocks in DIFF; this gets
   around the problems involved with changing line numbers in an ed
   script.

   As in 'output_diff3', the variable MAPPING maps from file number
   according to the argument list to file number according to the diff
   passed.  All files listed below are in terms of the argument list.
   REV_MAPPING is the inverse of MAPPING.

   FILE0, FILE1 and FILE2 are the strings to print as the names of the
   three files.  These may be the actual names, or may be the
   arguments specified with -L.

   Return 1 if conflicts were found.  */

static bool
output_diff3_edscript (FILE *outputfile, struct diff3_block *diff,
                       int const mapping[3], int const rev_mapping[3],
                       char const *file0, char const *file1, char const *file2)
{
  bool leading_dot;
  bool conflicts_found = false;
  bool conflict;
  struct diff3_block *b;

  for (b = reverse_diff3_blocklist (diff); b; b = b->next)
    {
      /* Must do mapping correctly.  */
      enum diff_type type
        = (b->correspond == DIFF_ALL
           ? DIFF_ALL
           : DIFF_1ST + rev_mapping[b->correspond - DIFF_1ST]);

      /* If we aren't supposed to do this output block, skip it.  */
      switch (type)
        {
        default: continue;
        case DIFF_2ND: if (!show_2nd) continue; conflict = true; break;
        case DIFF_3RD: if (overlap_only) continue; conflict = false; break;
        case DIFF_ALL: if (simple_only) continue; conflict = flagging; break;
        }

      lin low0 = D_LOWLINE (b, mapping[FILE0]);
      lin high0 = D_HIGHLINE (b, mapping[FILE0]);

      if (conflict)
        {
          conflicts_found = true;


          /* Mark end of conflict.  */

          fprintf (outputfile, "%"pI"da\n", high0);
          leading_dot = false;
          if (type == DIFF_ALL)
            {
              if (show_2nd)
                {
                  /* Append lines from FILE1.  */
                  fprintf (outputfile, "||||||| %s\n", file1);
                  leading_dot = dotlines (outputfile, b, mapping[FILE1]);
                }
              /* Append lines from FILE2.  */
              fputs ("=======\n", outputfile);
              leading_dot |= dotlines (outputfile, b, mapping[FILE2]);
            }
          fprintf (outputfile, ">>>>>>> %s\n", file2);
          undotlines (outputfile, leading_dot, high0 + 2,
                      (D_NUMLINES (b, mapping[FILE1])
                       + D_NUMLINES (b, mapping[FILE2]) + 1));


          /* Mark start of conflict.  */

          fprintf (outputfile, "%"pI"da\n<<<<<<< %s\n", low0 - 1,
                   type == DIFF_ALL ? file0 : file1);
          leading_dot = false;
          if (type == DIFF_2ND)
            {
              /* Prepend lines from FILE1.  */
              leading_dot = dotlines (outputfile, b, mapping[FILE1]);
              fputs ("=======\n", outputfile);
            }
          undotlines (outputfile, leading_dot, low0 + 1,
                      D_NUMLINES (b, mapping[FILE1]));
        }
      else if (D_NUMLINES (b, mapping[FILE2]) == 0)
        /* Write out a delete */
        {
          if (low0 == high0)
            fprintf (outputfile, "%"pI"dd\n", low0);
          else
            fprintf (outputfile, "%"pI"d,%"pI"dd\n", low0, high0);
        }
      else
        /* Write out an add or change */
        {
          switch (high0 - low0)
            {
            case -1:
              fprintf (outputfile, "%"pI"da\n", high0);
              break;
            case 0:
              fprintf (outputfile, "%"pI"dc\n", high0);
              break;
            default:
              fprintf (outputfile, "%"pI"d,%"pI"dc\n", low0, high0);
              break;
            }

          undotlines (outputfile, dotlines (outputfile, b, mapping[FILE2]),
                      low0, D_NUMLINES (b, mapping[FILE2]));
        }
    }
  if (finalwrite)
    fputs ("w\nq\n", outputfile);
  return conflicts_found;
}

/* Read from INFILE and output to OUTPUTFILE a set of diff3_blocks
   DIFF as a merged file.  This acts like 'ed file0
   <[output_diff3_edscript]', except that it works even for binary
   data or incomplete lines.

   As before, MAPPING maps from arg list file number to diff file
   number, REV_MAPPING is its inverse, and FILE0, FILE1, and FILE2 are
   the names of the files.

   Return 1 if conflicts were found.  */

static bool
output_diff3_merge (FILE *infile, FILE *outputfile, struct diff3_block *diff,
                    int const mapping[3], int const rev_mapping[3],
                    char const *file0, char const *file1, char const *file2)
{
  int c;
  lin i;
  bool conflicts_found = false;
  bool conflict;
  struct diff3_block *b;
  lin linesread = 0;

  for (b = diff; b; b = b->next)
    {
      /* Must do mapping correctly.  */
      enum diff_type type
        = ((b->correspond == DIFF_ALL)
           ? DIFF_ALL
           : DIFF_1ST + rev_mapping[b->correspond - DIFF_1ST]);
      char const *format_2nd = "<<<<<<< %s\n";

      /* If we aren't supposed to do this output block, skip it.  */
      switch (type)
        {
        default: continue;
        case DIFF_2ND: if (!show_2nd) continue; conflict = true; break;
        case DIFF_3RD: if (overlap_only) continue; conflict = false; break;
        case DIFF_ALL: if (simple_only) continue; conflict = flagging;
          format_2nd = "||||||| %s\n";
          break;
        }

      /* Copy I lines from file 0.  */
      i = D_LOWLINE (b, FILE0) - linesread - 1;
      linesread += i;
      while (0 <= --i)
        do
          {
            c = getc (infile);
            if (c == EOF)
              {
                if (ferror (infile))
                  perror_with_exit (_("read failed"));
                else if (feof (infile))
                  fatal ("input file shrank");
              }
            putc (c, outputfile);
          }
        while (c != '\n');

      if (conflict)
        {
          conflicts_found = true;

          if (type == DIFF_ALL)
            {
              /* Put in lines from FILE0 with bracket.  */
              fprintf (outputfile, "<<<<<<< %s\n", file0);
              for (i = 0;
                   i < D_NUMLINES (b, mapping[FILE0]);
                   i++)
                fwrite (D_RELNUM (b, mapping[FILE0], i), sizeof (char),
                        D_RELLEN (b, mapping[FILE0], i), outputfile);
            }

          if (show_2nd)
            {
              /* Put in lines from FILE1 with bracket.  */
              fprintf (outputfile, format_2nd, file1);
              for (i = 0;
                   i < D_NUMLINES (b, mapping[FILE1]);
                   i++)
                fwrite (D_RELNUM (b, mapping[FILE1], i), sizeof (char),
                        D_RELLEN (b, mapping[FILE1], i), outputfile);
            }

          fputs ("=======\n", outputfile);
        }

      /* Put in lines from FILE2.  */
      for (i = 0;
           i < D_NUMLINES (b, mapping[FILE2]);
           i++)
        fwrite (D_RELNUM (b, mapping[FILE2], i), sizeof (char),
                D_RELLEN (b, mapping[FILE2], i), outputfile);

      if (conflict)
        fprintf (outputfile, ">>>>>>> %s\n", file2);

      /* Skip I lines in file 0.  */
      i = D_NUMLINES (b, FILE0);
      linesread += i;
      while (0 <= --i)
        while ((c = getc (infile)) != '\n')
          if (c == EOF)
            {
              if (ferror (infile))
                perror_with_exit (_("read failed"));
              else if (feof (infile))
                {
                  if (i || b->next)
                    fatal ("input file shrank");
                  return conflicts_found;
                }
            }
    }
  /* Copy rest of common file.  */
  while ((c = getc (infile)) != EOF || !(ferror (infile) | feof (infile)))
    putc (c, outputfile);
  return conflicts_found;
}

/* Reverse the order of the list of diff3 blocks.  */

static struct diff3_block *
reverse_diff3_blocklist (struct diff3_block *diff)
{
  register struct diff3_block *tmp, *next, *prev;

  for (tmp = diff, prev = 0;  tmp;  tmp = next)
    {
      next = tmp->next;
      tmp->next = prev;
      prev = tmp;
    }

  return prev;
}

static void
fatal (char const *msgid)
{
  die (EXIT_TROUBLE, 0, "%s", _(msgid));
}

static void
perror_with_exit (char const *string)
{
  die (EXIT_TROUBLE, errno, "%s", string);
}
