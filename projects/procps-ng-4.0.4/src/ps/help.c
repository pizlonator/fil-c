/*
 * help.c - ps help output
 *
 * Copyright © 2012-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2023 Craig Small <csmall@dropbear.xyz>
 * Copyright © 2012-2014 Jaromir Capik <jcapik@redhat.com
 * Copyright © 1998-2004 Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"


enum {
  HELP_SMP, HELP_LST, HELP_OUT,
  HELP_THD, HELP_MSC, HELP_ALL,
  HELP_default
};

static struct {
  const char *word;
  const char *abrv;
} help_tab[HELP_default];


static int parse_help_opt (const char *opt) {
/* Translation Notes for ps Help #1 ---------------------------------
   .  This next group of lines represents 6 pairs of words + abbreviations
   .  which are the basis of the 'ps' program help text.
   .
   .  The words and abbreviations you provide will alter program behavior.
   .  They will also appear in the help usage summary associated with the
   .  "Notes for ps Help #2" below.
   .
   .  In their English form, help text would look like this:
   .      Try 'ps --help <simple|list|output|threads|misc|all>'
   .       or 'ps --help <s|l|o|t|m|a>'
   .      for additional help text.
   .
   .  When translating these 6 pairs you may choose any appropriate
   .  language equivalents and the only requirement is the abbreviated
   .  representations must be unique.
   .
   .  By default, those abbreviations are single characters.  However,
   .  they are not limited to only one character after translation.
   . */

/* Translation Hint, Pair #1 */
  help_tab[HELP_SMP].word = _("simple"); help_tab[HELP_SMP].abrv = _("s");
/* Translation Hint, Pair #2 */
  help_tab[HELP_LST].word  = _("list"); help_tab[HELP_LST].abrv = _("l");
/* Translation Hint, Pair #3 */
  help_tab[HELP_OUT].word = _("output"); help_tab[HELP_OUT].abrv = _("o");
/* Translation Hint, Pair #4 */
  help_tab[HELP_THD].word = _("threads"); help_tab[HELP_THD].abrv = _("t");
/* Translation Hint, Pair #5 */
  help_tab[HELP_MSC].word = _("misc"); help_tab[HELP_MSC].abrv = _("m");
/* Translation Hint, Pair #6 */
  help_tab[HELP_ALL].word = _("all"); help_tab[HELP_ALL].abrv = _("a");
/*
 * the above are doubled on each line so they carry the same .pot
 * line # reference and thus appear more like true "pairs" even
 * though xgettext will produce separate msgid/msgstr groups */

  if(opt) {
    int i;
    for (i = HELP_SMP; i < HELP_default; i++)
      if (!strcmp(opt, help_tab[i].word) || !strcmp(opt, help_tab[i].abrv))
        return i;
  }
  return HELP_default;
}


void do_help (const char *opt, int rc);
void do_help (const char *opt, int rc) {
  FILE *out = (rc == EXIT_SUCCESS) ? stdout : stderr;
  int section = parse_help_opt(opt);

  fprintf(out, _("\n"
    "Usage:\n"
    " %s [options]\n"), myname);

  if (section == HELP_SMP || section == HELP_ALL) {
    fputs(_("\nBasic options:\n"), out);
    fputs(_(" -A, -e               all processes\n"), out);
    fputs(_(" -a                   all with tty, except session leaders\n"), out);
    fputs(_("  a                   all with tty, including other users\n"), out);
    fputs(_(" -d                   all except session leaders\n"), out);
    fputs(_(" -N, --deselect       negate selection\n"), out);
    fputs(_("  r                   only running processes\n"), out);
    fputs(_("  T                   all processes on this terminal\n"), out);
    fputs(_("  x                   processes without controlling ttys\n"), out);
  }
  if (section == HELP_LST || section == HELP_ALL) {
    fputs(_("\nSelection by list:\n"), out);
    fputs(_(" -C <command>         command name\n"), out);
    fputs(_(" -G, --Group <GID>    real group id or name\n"), out);
    fputs(_(" -g, --group <group>  session or effective group name\n"), out);
    fputs(_(" -p, p, --pid <PID>   process id\n"), out);
    fputs(_("        --ppid <PID>  parent process id\n"), out);
    fputs(_(" -q, q, --quick-pid <PID>\n"
            "                      process id (quick mode)\n"), out);
    fputs(_(" -s, --sid <session>  session id\n"), out);
    fputs(_(" -t, t, --tty <tty>   terminal\n"), out);
    fputs(_(" -u, U, --user <UID>  effective user id or name\n"), out);
    fputs(_(" -U, --User <UID>     real user id or name\n"), out);
    fputs(_("\n"
      "  The selection options take as their argument either:\n"
      "    a comma-separated list e.g. '-u root,nobody' or\n"
      "    a blank-separated list e.g. '-p 123 4567'\n"), out);
  }
  if (section == HELP_OUT || section == HELP_ALL) {
    fputs(_("\nOutput formats:\n"), out);
    fputs(_(" -D <format>          date format for lstart\n"), out);
    fputs(_(" -F                   extra full\n"), out);
    fputs(_(" -f                   full-format, including command lines\n"), out);
    fputs(_("  f, --forest         ascii art process tree\n"), out);
    fputs(_(" -H                   show process hierarchy\n"), out);
    fputs(_(" -j                   jobs format\n"), out);
    fputs(_("  j                   BSD job control format\n"), out);
    fputs(_(" -l                   long format\n"), out);
    fputs(_("  l                   BSD long format\n"), out);
    fputs(_(" -M, Z                add security data (for SELinux)\n"), out);
    fputs(_(" -O <format>          preloaded with default columns\n"), out);
    fputs(_("  O <format>          as -O, with BSD personality\n"), out);
    fputs(_(" -o, o, --format <format>\n"
            "                      user-defined format\n"), out);
    fputs(_("  -P                  add psr column\n"), out);
    fputs(_("  s                   signal format\n"), out);
    fputs(_("  u                   user-oriented format\n"), out);
    fputs(_("  v                   virtual memory format\n"), out);
    fputs(_("  X                   register format\n"), out);
    fputs(_(" -y                   do not show flags, show rss vs. addr (used with -l)\n"), out);
    fputs(_("     --context        display security context (for SELinux)\n"), out);
    fputs(_("     --headers        repeat header lines, one per page\n"), out);
    fputs(_("     --no-headers     do not print header at all\n"), out);
    fputs(_("     --cols, --columns, --width <num>\n"
      "                      set screen width\n"), out);
    fputs(_("     --rows, --lines <num>\n"
      "                      set screen height\n"), out);
    fputs(_("     --signames       display signal masks using signal names\n"), out);
  }
  if (section == HELP_THD || section == HELP_ALL) {
    fputs(_("\nShow threads:\n"), out);
    fputs(_("  H                   as if they were processes\n"), out);
    fputs(_(" -L                   possibly with LWP and NLWP columns\n"), out);
    fputs(_(" -m, m                after processes\n"), out);
    fputs(_(" -T                   possibly with SPID column\n"), out);
  }
  if (section == HELP_MSC || section == HELP_ALL) {
    fputs(_("\nMiscellaneous options:\n"), out);
    fputs(_(" -c                   show scheduling class with -l option\n"), out);
    fputs(_("  c                   show true command name\n"), out);
    fputs(_("  e                   show the environment after command\n"), out);
    fputs(_("  k,    --sort        specify sort order as: [+|-]key[,[+|-]key[,...]]\n"), out);
    fputs(_("  L                   show format specifiers\n"), out);
    fputs(_("  n                   display numeric uid and wchan\n"), out);
    fputs(_("  S,    --cumulative  include some dead child process data\n"), out);
    fputs(_(" -y                   do not show flags, show rss (only with -l)\n"), out);
    fputs(_(" -V, V, --version     display version information and exit\n"), out);
    fputs(_(" -w, w                unlimited output width\n"), out);
    fprintf(out, _("\n"
      "        --%s <%s|%s|%s|%s|%s|%s>\n"
      "                      display help and exit\n")
        , the_word_help
        , help_tab[HELP_SMP].word, help_tab[HELP_LST].word
        , help_tab[HELP_OUT].word, help_tab[HELP_THD].word
        , help_tab[HELP_MSC].word, help_tab[HELP_ALL].word);
  }
  if (section == HELP_default) {
/* Translation Notes for ps Help #2 ---------------------------------
   .  Most of the following c-format string is derived from the 6
   .  pairs of words + chars mentioned above in "Notes for ps Help #1".
   .
   .  In its full English form, help text would look like this:
   .      Try 'ps --help <simple|list|output|threads|misc|all>'
   .       or 'ps --help <s|l|o|t|m|a>'
   .      for additional help text.
   .
   .  The word for "help" will be translated elsewhere.  Thus, the only
   .  translations below will be: "Try", "or" and "for additional...".
   . */
    fprintf(out, _("\n"
      " Try '%s --%s <%s|%s|%s|%s|%s|%s>'\n"
      "  or '%s --%s <%s|%s|%s|%s|%s|%s>'\n"
      " for additional help text.\n")
        , myname, the_word_help
        , help_tab[HELP_SMP].word, help_tab[HELP_LST].word
        , help_tab[HELP_OUT].word, help_tab[HELP_THD].word
        , help_tab[HELP_MSC].word, help_tab[HELP_ALL].word
        , myname, the_word_help
        , help_tab[HELP_SMP].abrv, help_tab[HELP_LST].abrv
        , help_tab[HELP_OUT].abrv, help_tab[HELP_THD].abrv
        , help_tab[HELP_MSC].abrv, help_tab[HELP_ALL].abrv);
  }
  fprintf(out, _("\nFor more details see ps(1).\n"));
  exit(rc);
}

/* Missing:
 *
 * -P e k
 *
 */
