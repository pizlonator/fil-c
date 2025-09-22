/* top_nls.c - provide the basis for future nls translations */
/*
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */
/* For contributions to this program, the author wishes to thank:
 *    Craig Small, <csmall@dropbear.xyz>
 *    Sami Kerola, <kerolasa@iki.fi>
 */

#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "nls.h"

#include "top.h"
#include "top_nls.h"

#ifdef NLS_VALIDATE
#include <stdlib.h>
#endif


        /*
         * The provision excluding some strings is intended to be
         * used very sparingly. It exists in case we collide with
         * some translation project person in a position to delay
         * a future release over his or her personal preferences.
         *
         * If it's ever enabled, it will produce a fatal compiler
         * error as our only option since those gettext tools are
         * far too primitive to be influenced with a conditional.
         * They always ignore a '_X()' macro no matter its state. */

#ifndef NLS_INCLUDED
# define _X(str)  (str)
#else
# define _X(str)
# error instead of this #define, restore the true gettext macro(s)
#endif

        // Programmer Note(s):
        //  Preparation ---------------------------------------------
        //    Unless you have *something* following the gettext macro,
        //    xgettext will refuse to see any TRANSLATORS comments.
        //    Thus empty strings have been added for potential future
        //    comment additions.
        //
        //    Also, by omitting the argument for the --add-comments
        //    XGETTEXT_OPTION in po/Makevars, *any* preceding c style
        //    comment will be propagated to the .pot file, providing
        //    that the gettext macro isn't empty as discussed above.
        //    However, this is far too aggressive so we have chosen
        //    the word 'Translation' to denote xgettext comments.
        //
        //    /* Need Not Say 'TRANSLATORS': ...
        //    snprintf(buf, sizeof(buf), "%s", _(      // unseen comment
        //
        //    /* Translation Hint: ...
        //    snprintf(buf, sizeof(buf), "%s", _(""    // now it's seen!
        //
        //  Translation, from po/ directory after make --------------
        //  ( this is the procedure used before any translations were  )
        //  ( available in the po/ directory, which contained only the )
        //  ( procps-ng.pot, this domain's template file.              )
        //
        //  ( below: ll_CC = language/country as in 'zh_CN' or 'en_AU' )
        //
        //    msginit --locale=ll_CC --no-wrap
        //     . creates a ll_CC.po file from the template procps-ng.pot
        //     . may also duplicate msgid as msgstr if languages similar
        //    msgen --no-wrap ll_CC.po --output-file=ll_CC.po
        //     . duplicates every msgid literal as msgstr value
        //     . this is the file that's edited
        //        . replace "Content-Type: ... charset=ASCII\n"
        //                           with "... charset=UTF-8\n"
        //        . translate msgstr values, leaving msgid unchanged
        //    msgfmt ll_CC.po --strict --output-file=procps-ng.mo
        //     . after which ensure, chmod 644
        //     . then copy
        //         to /usr/share/locale-langpack/ll_CC/LC_MESSAGES/
        //         or /usr/share/locale/ll_CC/LC_MESSAGES/
        //  Testing -------------------------------------------------
        //    export LC_ALL= && export LANGUAGE=ll_CC
        //    run some capable program like top
        //

        /*
         * These are our string tables with the following contents:
         *    Head : column headings with varying size limits
         *    Desc : fields descriptions not to exceed 20 screen positions
         *    Norm : regular text possibly also containing c-format specifiers
         *    Uniq : show_special specially formatted strings
         *
         * The latter table presents the greatest translation challenge !
         *
         * We go to the trouble of creating the nls string tables to achieve
         * these objectives:
         *    +  the overhead of repeated runtime calls to gettext()
         *       will be avoided
         *    +  the order of the strings in the template (.pot) file
         *       can be completely controlled
         *    +  none of the important translator only comments will
         *       clutter and obscure the main program
         */
const char *Head_nlstab[EU_MAXPFLGS];
const char *Desc_nlstab[EU_MAXPFLGS];
const char *Norm_nlstab[norm_MAX];
const char *Uniq_nlstab[uniq_MAX];


        /*
         * This routine builds the nls table containing plain text only
         * used as the field descriptions.  Each translated line MUST be
         * kept to a maximum of 20 characters or less! */
static void build_two_nlstabs (void) {

/* Translation Notes ------------------------------------------------
   .  It is strongly recommend that the --no-wrap command line option
   .  be used with all supporting translation tools, when available.
   .
   .  The following line pairs contain only plain text and consist of:
   .     1) a field name/column header - mostly upper case
   .     2) the related description    - both upper and lower case
   .
   .  To avoid truncation on the main top display, each column header
   .  is noted with its maximum size, while a few are 'variable' width.
   .  Names for the latter should probably be 10 or fewer characters.
   .
   .  Those fields shown with a '+' are also eligible for user resizing
   .  using the 'X' command. That means the default width might produce
   .  truncation but need not if widened (see the man page 'X' command).
   .
   .  All headers are subject to a maximum of 8 on the Fields Management
   .  screen where truncation is entirely acceptable.
   .
   .  The associated descriptions are always limited to 20 characters,
   .  and are used only on the Fields Management screen.
   .
   .  In all cases, fewer characters are just fine.
   . */

/* Translation Hint: maximum 'PID' = 5 */
   Head_nlstab[EU_PID] = _("PID");
   Desc_nlstab[EU_PID] = _("Process Id");
/* Translation Hint: maximum 'PPID' = 5 */
   Head_nlstab[EU_PPD] = _("PPID");
   Desc_nlstab[EU_PPD] = _("Parent Process pid");
/* Translation Hint: maximum 'UID' = 5 + */
   Head_nlstab[EU_UED] = _("UID");
   Desc_nlstab[EU_UED] = _("Effective User Id");
/* Translation Hint: maximum 'USER' = 8 + */
   Head_nlstab[EU_UEN] = _("USER");
   Desc_nlstab[EU_UEN] = _("Effective User Name");
/* Translation Hint: maximum 'RUID' = 5 + */
   Head_nlstab[EU_URD] = _("RUID");
   Desc_nlstab[EU_URD] = _("Real User Id");
/* Translation Hint: maximum 'RUSER' = 8 + */
   Head_nlstab[EU_URN] = _("RUSER");
   Desc_nlstab[EU_URN] = _("Real User Name");
/* Translation Hint: maximum 'SUID' = 5 + */
   Head_nlstab[EU_USD] = _("SUID");
   Desc_nlstab[EU_USD] = _("Saved User Id");
/* Translation Hint: maximum 'SUSER' = 8 + */
   Head_nlstab[EU_USN] = _("SUSER");
   Desc_nlstab[EU_USN] = _("Saved User Name");
/* Translation Hint: maximum 'GID' = 5 + */
   Head_nlstab[EU_GID] = _("GID");
   Desc_nlstab[EU_GID] = _("Group Id");
/* Translation Hint: maximum 'GROUP' = 8 + */
   Head_nlstab[EU_GRP] = _("GROUP");
   Desc_nlstab[EU_GRP] = _("Group Name");
/* Translation Hint: maximum 'PGRP' = 5 */
   Head_nlstab[EU_PGD] = _("PGRP");
   Desc_nlstab[EU_PGD] = _("Process Group Id");
/* Translation Hint: maximum 'TTY' = 8 + */
   Head_nlstab[EU_TTY] = _("TTY");
   Desc_nlstab[EU_TTY] = _("Controlling Tty");
/* Translation Hint: maximum 'TPGID' = 5 */
   Head_nlstab[EU_TPG] = _("TPGID");
   Desc_nlstab[EU_TPG] = _("Tty Process Grp Id");
/* Translation Hint: maximum 'SID' = 5 */
   Head_nlstab[EU_SID] = _("SID");
   Desc_nlstab[EU_SID] = _("Session Id");
/* Translation Hint: maximum 'PR' = 3 */
   Head_nlstab[EU_PRI] = _("PR");
   Desc_nlstab[EU_PRI] = _("Priority");
/* Translation Hint: maximum 'NI' = 3 */
   Head_nlstab[EU_NCE] = _("NI");
   Desc_nlstab[EU_NCE] = _("Nice Value");
/* Translation Hint: maximum 'nTH' = 3 */
   Head_nlstab[EU_THD] = _("nTH");
   Desc_nlstab[EU_THD] = _("Number of Threads");
/* Translation Hint: maximum 'P' = 1 */
   Head_nlstab[EU_CPN] = _("P");
   Desc_nlstab[EU_CPN] = _("Last Used Cpu (SMP)");
/* Translation Hint: maximum '%CPU' = 4 */
   Head_nlstab[EU_CPU] = _("%CPU");
   Desc_nlstab[EU_CPU] = _("CPU Usage");
/* Translation Hint: maximum '' = 6 */
   Head_nlstab[EU_TME] = _("TIME");
   Desc_nlstab[EU_TME] = _("CPU Time");
/* Translation Hint: maximum 'TIME+' = 9 */
   Head_nlstab[EU_TM2] = _("TIME+");
   Desc_nlstab[EU_TM2] = _("CPU Time, hundredths");
/* Translation Hint: maximum '%MEM' = 4 */
   Head_nlstab[EU_MEM] = _("%MEM");
   Desc_nlstab[EU_MEM] = _("Memory Usage (RES)");
/* Translation Hint: maximum 'VIRT' = 7 */
   Head_nlstab[EU_VRT] = _("VIRT");
   Desc_nlstab[EU_VRT] = _("Virtual Image (KiB)");
/* Translation Hint: maximum 'SWAP' = 6 */
   Head_nlstab[EU_SWP] = _("SWAP");
   Desc_nlstab[EU_SWP] = _("Swapped Size (KiB)");
/* Translation Hint: maximum 'RES' = 6 */
   Head_nlstab[EU_RES] = _("RES");
   Desc_nlstab[EU_RES] = _("Resident Size (KiB)");
/* Translation Hint: maximum 'CODE' = 4 */
   Head_nlstab[EU_COD] = _("CODE");
   Desc_nlstab[EU_COD] = _("Code Size (KiB)");
/* Translation Hint: maximum 'DATA' = 7 */
   Head_nlstab[EU_DAT] = _("DATA");
   Desc_nlstab[EU_DAT] = _("Data+Stack (KiB)");
/* Translation Hint: maximum 'SHR' = 6 */
   Head_nlstab[EU_SHR] = _("SHR");
   Desc_nlstab[EU_SHR] = _("Shared Memory (KiB)");
/* Translation Hint: maximum 'nMaj' = 4 */
   Head_nlstab[EU_FL1] = _("nMaj");
   Desc_nlstab[EU_FL1] = _("Major Page Faults");
/* Translation Hint: maximum 'nMin' = 4 */
   Head_nlstab[EU_FL2] = _("nMin");
   Desc_nlstab[EU_FL2] = _("Minor Page Faults");
/* Translation Hint: maximum 'nDRT' = 4 */
   Head_nlstab[EU_DRT] = _("nDRT");
   Desc_nlstab[EU_DRT] = _("Dirty Pages Count");
/* Translation Hint: maximum 'S' = 1 */
   Head_nlstab[EU_STA] = _("S");
   Desc_nlstab[EU_STA] = _("Process Status");
/* Translation Hint: maximum 'COMMAND' = variable */
   Head_nlstab[EU_CMD] = _("COMMAND");
   Desc_nlstab[EU_CMD] = _("Command Name/Line");
/* Translation Hint: maximum 'WCHAN' = 10 + */
   Head_nlstab[EU_WCH] = _("WCHAN");
   Desc_nlstab[EU_WCH] = _("Sleeping in Function");
/* Translation Hint: maximum 'Flags' = 8 */
   Head_nlstab[EU_FLG] = _("Flags");
   Desc_nlstab[EU_FLG] = _("Task Flags <sched.h>");
/* Translation Hint: maximum 'CGROUPS' = variable */
   Head_nlstab[EU_CGR] = _("CGROUPS");
   Desc_nlstab[EU_CGR] = _("Control Groups");
/* Translation Hint: maximum 'SUPGIDS' = variable */
   Head_nlstab[EU_SGD] = _("SUPGIDS");
   Desc_nlstab[EU_SGD] = _("Supp Groups IDs");
/* Translation Hint: maximum 'SUPGRPS' = variable */
   Head_nlstab[EU_SGN] = _("SUPGRPS");
   Desc_nlstab[EU_SGN] = _("Supp Groups Names");
/* Translation Hint: maximum 'TGID' = 5 */
   Head_nlstab[EU_TGD] = _("TGID");
   Desc_nlstab[EU_TGD] = _("Thread Group Id");
/* Translation Hint: maximum 'OOMa' = 5 */
   Head_nlstab[EU_OOA] = _("OOMa");
   Desc_nlstab[EU_OOA] = _("OOMEM Adjustment");
/* Translation Hint: maximum 'OOMs' = 4 */
   Head_nlstab[EU_OOM] = _("OOMs");
   Desc_nlstab[EU_OOM] = _("OOMEM Score current");
/* Translation Hint: maximum 'ENVIRON' = variable */
   Head_nlstab[EU_ENV] = _("ENVIRON");
/* Translation Hint: the abbreviation 'vars' below is shorthand for
                     'variables' */
   Desc_nlstab[EU_ENV] = _("Environment vars");
/* Translation Hint: maximum 'vMj' = 3 */
   Head_nlstab[EU_FV1] = _("vMj");
   Desc_nlstab[EU_FV1] = _("Major Faults delta");
/* Translation Hint: maximum 'vMn' = 3 */
   Head_nlstab[EU_FV2] = _("vMn");
   Desc_nlstab[EU_FV2] = _("Minor Faults delta");
/* Translation Hint: maximum 'USED' = 6 */
   Head_nlstab[EU_USE] = _("USED");
   Desc_nlstab[EU_USE] = _("Res+Swap Size (KiB)");
/* Translation Hint: maximum 'nsIPC' = 10 + */
   Head_nlstab[EU_NS1] = _("nsIPC");
   Desc_nlstab[EU_NS1] = _("IPC namespace Inode");
/* Translation Hint: maximum 'nsMNT' = 10 + */
   Head_nlstab[EU_NS2] = _("nsMNT");
   Desc_nlstab[EU_NS2] = _("MNT namespace Inode");
/* Translation Hint: maximum 'nsNET' = 10 + */
   Head_nlstab[EU_NS3] = _("nsNET");
   Desc_nlstab[EU_NS3] = _("NET namespace Inode");
/* Translation Hint: maximum 'nsPID' = 10 + */
   Head_nlstab[EU_NS4] = _("nsPID");
   Desc_nlstab[EU_NS4] = _("PID namespace Inode");
/* Translation Hint: maximum 'nsUSER' = 10 + */
   Head_nlstab[EU_NS5] = _("nsUSER");
   Desc_nlstab[EU_NS5] = _("USER namespace Inode");
/* Translation Hint: maximum 'nsUTS' = 10 + */
   Head_nlstab[EU_NS6] = _("nsUTS");
   Desc_nlstab[EU_NS6] = _("UTS namespace Inode");
/* Translation Hint: maximum 'LXC' = 8 + */
   Head_nlstab[EU_LXC] = _("LXC");
   Desc_nlstab[EU_LXC] = _("LXC container name");
/* Translation Hint: maximum 'RSan' = 6 */
   Head_nlstab[EU_RZA] = _("RSan");
   Desc_nlstab[EU_RZA] = _("RES Anonymous (KiB)");
/* Translation Hint: maximum 'RSfd' = 6 */
   Head_nlstab[EU_RZF] = _("RSfd");
   Desc_nlstab[EU_RZF] = _("RES File-based (KiB)");
/* Translation Hint: maximum 'RSlk' = 6 */
   Head_nlstab[EU_RZL] = _("RSlk");
   Desc_nlstab[EU_RZL] = _("RES Locked (KiB)");
/* Translation Hint: maximum 'RSsh' = 6 */
   Head_nlstab[EU_RZS] = _("RSsh");
   Desc_nlstab[EU_RZS] = _("RES Shared (KiB)");
/* Translation Hint: maximum 'CGNAME' = variable */
   Head_nlstab[EU_CGN] = _("CGNAME");
   Desc_nlstab[EU_CGN] = _("Control Group name");
/* Translation Hint: maximum 'NU' = 2 */
   Head_nlstab[EU_NMA] = _("NU");
   Desc_nlstab[EU_NMA] = _("Last Used NUMA node");
/* Translation Hint: maximum 'LOGID' = 5 + */
   Head_nlstab[EU_LID] = _("LOGID");
   Desc_nlstab[EU_LID] = _("Login User Id");
/* Translation Hint: maximum 'EXE' = variable */
   Head_nlstab[EU_EXE] = _("EXE");
   Desc_nlstab[EU_EXE] = _("Executable Path");
/* Translation Hint: With the next 5 fields, notice how an extra space
                     has been added ahead of one 'KiB' so that they all
                     align. You need not preserve such alignment. */
/* Translation Hint: maximum 'RSS' = 6 */
   Head_nlstab[EU_RSS] = _("RSS");
   Desc_nlstab[EU_RSS] = _("Res Mem (smaps), KiB");
/* Translation Hint: maximum 'PSS' = 6 */
   Head_nlstab[EU_PSS] = _("PSS");
   Desc_nlstab[EU_PSS] = _("Proportion RSS,  KiB");
/* Translation Hint: maximum 'PSan' = 6 */
   Head_nlstab[EU_PZA] = _("PSan");
   Desc_nlstab[EU_PZA] = _("Proportion Anon, KiB");
/* Translation Hint: maximum 'PSfd' = 6 */
   Head_nlstab[EU_PZF] = _("PSfd");
   Desc_nlstab[EU_PZF] = _("Proportion File, KiB");
/* Translation Hint: maximum 'PSsh' = 6 */
   Head_nlstab[EU_PZS] = _("PSsh");
   Desc_nlstab[EU_PZS] = _("Proportion Shrd, KiB");
/* Translation Hint: maximum 'USS' = 6 */
   Head_nlstab[EU_USS] = _("USS");
   Desc_nlstab[EU_USS] = _("Unique RSS, KiB");
/* Translation Hint: maximum 'ioR' = 6 */
   Head_nlstab[EU_IRB] = _("ioR");
   Desc_nlstab[EU_IRB] = _("I/O Bytes Read");
/* Translation Hint: maximum 'ioRop' = 5 */
   Head_nlstab[EU_IRO] = _("ioRop");
   Desc_nlstab[EU_IRO] = _("I/O Read Operations");
/* Translation Hint: maximum 'ioW' = 6 */
   Head_nlstab[EU_IWB] = _("ioW");
   Desc_nlstab[EU_IWB] = _("I/O Bytes Written");
/* Translation Hint: maximum 'ioWop' = 5 */
   Head_nlstab[EU_IWO] = _("ioWop");
   Desc_nlstab[EU_IWO] = _("I/O Write Operations");
/* Translation Hint: maximum 'AGID' = 5 */
   Head_nlstab[EU_AGI] = _("AGID");
   Desc_nlstab[EU_AGI] = _("Autogroup Identifier");
/* Translation Hint: maximum 'AGNI' = 4 */
   Head_nlstab[EU_AGN] = _("AGNI");
   Desc_nlstab[EU_AGN] = _("Autogroup Nice Value");
/* Translation Hint: maximum 'STARTED' = 7 */
   Head_nlstab[EU_TM3] = _("STARTED");
   Desc_nlstab[EU_TM3] = _("Start Time from boot");
/* Translation Hint: maximum 'ELAPSED' = 7 */
   Head_nlstab[EU_TM4] = _("ELAPSED");
   Desc_nlstab[EU_TM4] = _("Elapsed Running Time");
/* Translation Hint: maximum '%CUU' = 6 */
   Head_nlstab[EU_CUU] = _("%CUU");
   Desc_nlstab[EU_CUU] = _("CPU Utilization");
/* Translation Hint: maximum '%CUC' = 7 */
   Head_nlstab[EU_CUC] = _("%CUC");
   Desc_nlstab[EU_CUC] = _("Utilization + child");
/* Translation Hint: maximum 'nsCGROUP' = 10 + */
   Head_nlstab[EU_NS7] = _("nsCGROUP");
   Desc_nlstab[EU_NS7] = _("CGRP namespace Inode");
/* Translation Hint: maximum 'nsTIME' = 10 + */
   Head_nlstab[EU_NS8] = _("nsTIME");
   Desc_nlstab[EU_NS8] = _("TIME namespace Inode");
}


        /*
         * This routine builds the nls table containing both plain text
         * and regular c-format strings. */
static void build_norm_nlstab (void) {

/* Translation Notes ------------------------------------------------
   .  It is strongly recommend that the --no-wrap command line option
   .  be used with all supporting translation tools, when available.
   .
   .  This group of lines contains both plain text and c-format strings.
   .
   .  Some strings reflect switches used to affect the running program
   .  and should not be translated without also making corresponding
   .  c-code logic changes.
   . */

   Norm_nlstab[EXIT_signals_fmt] = _(""
      "\tsignal %d (%s) was caught by %s, please\n"
      "\tsend bug reports to <procps@freelists.org>\n");
   Norm_nlstab[HELP_cmdline_fmt] = _X("\n"
      "Usage:\n"
      " %s [options]\n"
      "\n"
      "Options:\n"
      " -b, --batch-mode                run in non-interactive batch mode\n"
      " -c, --cmdline-toggle            reverse last remembered 'c' state\n"
      " -d, --delay =SECS [.TENTHS]     iterative delay as SECS [.TENTHS]\n"
      " -E, --scale-summary-mem =SCALE  set mem as: k,m,g,t,p,e for SCALE\n"
      " -e, --scale-task-mem =SCALE     set mem with: k,m,g,t,p for SCALE\n"
      " -H, --threads-show              show tasks plus all their threads\n"
      " -i, --idle-toggle               reverse last remembered 'i' state\n"
      " -n, --iterations =NUMBER        exit on maximum iterations NUMBER\n"
      " -O, --list-fields               output all field names, then exit\n"
      " -o, --sort-override =FIELD      force sorting on this named FIELD\n"
      " -p, --pid =PIDLIST              monitor only the tasks in PIDLIST\n"
      " -S, --accum-time-toggle         reverse last remembered 'S' state\n"
      " -s, --secure-mode               run with secure mode restrictions\n"
      " -U, --filter-any-user =USER     show only processes owned by USER\n"
      " -u, --filter-only-euser =USER   show only processes owned by USER\n"
      " -w, --width [=COLUMNS]          change print width [,use COLUMNS]\n"
      " -1, --single-cpu-toggle         reverse last remembered '1' state\n"
      "\n"
      " -h, --help                      display this help text, then exit\n"
      " -V, --version                   output version information & exit\n"
      "\n"
      "For more details see top(1).");
   Norm_nlstab[BAD_delayint_fmt] = _("bad delay interval '%s'");
   Norm_nlstab[BAD_niterate_fmt] = _("bad iterations argument '%s'");
   Norm_nlstab[LIMIT_exceed_fmt] = _("pid limit (%d) exceeded");
   Norm_nlstab[BAD_mon_pids_fmt] = _("bad pid '%s'");
   Norm_nlstab[MISSING_args_fmt] = _("-%c argument missing");
   Norm_nlstab[BAD_widtharg_fmt] = _("bad width arg '%s'");
   Norm_nlstab[UNKNOWN_opts_fmt] = _("unknown option '%s'");
   Norm_nlstab[DELAY_secure_txt] = _("-d disallowed in \"secure\" mode");
   Norm_nlstab[DELAY_badarg_txt] = _("-d requires positive argument");
   Norm_nlstab[ON_word_only_txt] = _("On");
   Norm_nlstab[OFF_one_word_txt] = _("Off");
   Norm_nlstab[VERSION_opts_fmt] = _("%s from %s");
   Norm_nlstab[FOREST_modes_fmt] = _("Forest mode %s");
   Norm_nlstab[FAIL_tty_get_txt] = _("failed tty get");
   Norm_nlstab[FAIL_tty_set_fmt] = _("failed tty set: %s");
   Norm_nlstab[CHOOSE_group_txt] = _("Choose field group (1 - 4)");
   Norm_nlstab[DISABLED_cmd_txt] = _("Command disabled, 'A' mode required");
   Norm_nlstab[DISABLED_win_fmt] = _("Command disabled, activate %s with '-' or '_'");
   Norm_nlstab[COLORS_nomap_txt] = _("No colors to map!");
   Norm_nlstab[FAIL_rc_open_fmt] = _("Failed '%s' open: %s");
   Norm_nlstab[WRITE_rcfile_fmt] = _("Wrote configuration to '%s'");
   Norm_nlstab[DELAY_change_fmt] = _("Change delay from %.1f to");
   Norm_nlstab[THREADS_show_fmt] = _("Show threads %s");
   Norm_nlstab[IRIX_curmode_fmt] = _("Irix mode %s");
   Norm_nlstab[GET_pid2kill_fmt] = _("PID to signal/kill [default pid = %d]");
   Norm_nlstab[GET_sigs_num_fmt] = _("Send pid %d signal [%d/sigterm]");
   Norm_nlstab[FAIL_signals_fmt] = _("Failed signal pid '%d' with '%d': %s");
   Norm_nlstab[BAD_signalid_txt] = _("Invalid signal");
   Norm_nlstab[GET_pid2nice_fmt] = _("PID to renice [default pid = %d]");
   Norm_nlstab[GET_nice_num_fmt] = _("Renice PID %d to value");
   Norm_nlstab[FAIL_re_nice_fmt] = _("Failed renice of PID %d to %d: %s");
   Norm_nlstab[NAME_windows_fmt] = _("Rename window '%s' to (1-3 chars)");
   Norm_nlstab[TIME_accumed_fmt] = _("Cumulative time %s");
   Norm_nlstab[GET_max_task_fmt] = _("Maximum tasks = %d, change to (0 is unlimited)");
   Norm_nlstab[BAD_max_task_txt] = _("Invalid maximum");
   Norm_nlstab[GET_user_ids_txt] = _("Which user (blank for all)");
   Norm_nlstab[UNKNOWN_cmds_txt] = _("Unknown command - try 'h' for help");
   Norm_nlstab[SCROLL_coord_fmt] = _("scroll coordinates: y = %d/%d (tasks), x = %d/%d (fields)");
   Norm_nlstab[FAIL_alloc_c_txt] = _("failed memory allocate");
   Norm_nlstab[FAIL_alloc_r_txt] = _("failed memory re-allocate");
   Norm_nlstab[BAD_numfloat_txt] = _("Unacceptable floating point");
   Norm_nlstab[BAD_username_txt] = _("Invalid user");
   Norm_nlstab[FOREST_views_txt] = _("forest view");
   Norm_nlstab[FAIL_widepid_txt] = _("failed pid maximum size test");
   Norm_nlstab[FAIL_widecpu_txt] = _("failed number of cpus test");
   Norm_nlstab[RC_bad_files_fmt] = _("incompatible rcfile, you should delete '%s'");
   Norm_nlstab[RC_bad_entry_fmt] = _("window entry #%d corrupt, please delete '%s'");
   Norm_nlstab[NOT_onsecure_txt] = _("Unavailable in secure mode");
   Norm_nlstab[NOT_smp_cpus_txt] = _("Only 1 cpu detected");
   Norm_nlstab[BAD_integers_txt] = _("Unacceptable integer");
   Norm_nlstab[SELECT_clash_txt] = _("conflicting process selections (U/p/u)");
/* Translation Hint: This is an abbreviation (limit 3 characters) for:
   .                 kibibytes (1024 bytes) */
   Norm_nlstab[AMT_kilobyte_txt] = _("KiB");
/* Translation Hint: This is an abbreviation (limit 3 characters) for:
   .                 mebibytes (1,048,576 bytes) */
   Norm_nlstab[AMT_megabyte_txt] = _("MiB");
/* Translation Hint: This is an abbreviation (limit 3 characters) for:
   .                 gibibytes (1,073,741,824 bytes) */
   Norm_nlstab[AMT_gigabyte_txt] = _("GiB");
/* Translation Hint: This is an abbreviation (limit 3 characters) for:
   .                 tebibytes (1,099,511,627,776 bytes) */
   Norm_nlstab[AMT_terabyte_txt] = _("TiB");
/* Translation Hint: This is an abbreviation (limit 3 characters) for:
   .                 pebibytes (1,024 tebibytes) */
   Norm_nlstab[AMT_petabyte_txt] = _("PiB");
/* Translation Hint: This is an abbreviation (limit 3 characters) for:
   .                 exbibytes (1,024 pebibytes) */
   Norm_nlstab[AMT_exxabyte_txt] = _("EiB");
   Norm_nlstab[WORD_threads_txt] = _("Threads");
   Norm_nlstab[WORD_process_txt] = _("Tasks");
/* Translation Hint: The following "word" is meant to represent either a single
   .                 cpu or all of the processors in a multi-processor computer
   .                 (should be exactly 6 characters, excluding leading % & colon) */
   Norm_nlstab[WORD_allcpus_txt] = _("%Cpu(s):");
/* Translation Hint: The following "word" is meant to represent a single processor
                     and 'the Cp' prefix will be combined with a  core id: 'p', 'e' or 'u'
   .                 (if 'Cp' is translated, it must be exactly 2 characters long) */
   Norm_nlstab[WORD_eachcpu_fmt] = _("%%Cp%c%-3d:");
/* Translation Hint: The following word "another" must have 1 trailing space */
   Norm_nlstab[WORD_another_txt] = _("another ");
   Norm_nlstab[FIND_no_next_txt] = _("Locate next inactive, use \"L\"");
   Norm_nlstab[GET_find_str_txt] = _("Locate string");
   Norm_nlstab[FIND_no_find_fmt] = _("%s\"%s\" not found");
   Norm_nlstab[XTRA_fixwide_fmt] = _("width incr is %d, change to (0 default, -1 auto)");
   Norm_nlstab[XTRA_warncfg_txt] = _("rcfile has inspect/other-filter error(s), save anyway?");
   Norm_nlstab[XTRA_badflds_fmt] = _("unrecognized field name '%s'");
   Norm_nlstab[XTRA_winsize_txt] = _("even using field names only, window is now too small");
   Norm_nlstab[YINSP_demo01_txt] = _("Open Files");
   Norm_nlstab[YINSP_demo02_txt] = _("NUMA Info");
   Norm_nlstab[YINSP_demo03_txt] = _("Log");
   Norm_nlstab[YINSP_deqfmt_txt] = _("the '=' key will eventually show the actual file read or command(s) executed ...");
   Norm_nlstab[YINSP_deqtyp_txt] = _("demo");
   Norm_nlstab[YINSP_dstory_txt] = _(""
      "This is simulated output representing the contents of some file or the output\n"
      "from some command.  Exactly which commands and/or files are solely up to you.\n"
      "\n"
      "Although this text is for information purposes only, it can still be scrolled\n"
      "and searched like real output will be.  You are encouraged to experiment with\n"
      "those features as explained in the prologue above.\n"
      "\n"
      "To enable real Inspect functionality, entries must be added to the end of the\n"
      "top personal personal configuration file.  You could use your favorite editor\n"
      "to accomplish this, taking care not to disturb existing entries.\n"
      "\n"
      "Another way to add entries is illustrated below, but it risks overwriting the\n"
      "rcfile.  Redirected echoes must not replace (>) but append (>>) to that file.\n"
      "\n"
      "  /bin/echo -e \"pipe\\tOpen Files\\tlsof -P -p %d 2>&1\" >> ~/.toprc\n"
      "  /bin/echo -e \"file\\tNUMA Info\\t/proc/%d/numa_maps\" >> ~/.toprc\n"
      "  /bin/echo -e \"pipe\\tLog\\ttail -n200 /var/log/syslog | sort -Mr\" >> ~/.toprc\n"
      "\n"
      "If you don't know the location or name of the top rcfile, use the 'W' command\n"
      "and note those details.  After backing up the current rcfile, try issuing the\n"
      "above echoes exactly as shown, replacing '.toprc' as appropriate.  The safest\n"
      "approach would be to use copy then paste to avoid any typing mistakes.\n"
      "\n"
      "Finally, restart top to reveal what actual Inspect entries combined with this\n"
      "new command can offer.  The possibilities are endless, especially considering\n"
      "that 'pipe' type entries can include shell scripts too!\n"
      "\n"
      "For additional important information, please consult the top(1) man document.\n"
      "Then, enhance top with your very own customized 'file' and/or 'pipe' entries.\n"
      "\n"
      "Enjoy!\n");
   Norm_nlstab[YINSP_noent1_txt] = _("to enable 'Y' press <Enter> then type 'W' and restart top");
   Norm_nlstab[YINSP_noent2_txt] = _("to enable 'Y' please consult the top man page (press Enter)");
   Norm_nlstab[YINSP_failed_fmt] = _("Selection failed with: %s\n");
   Norm_nlstab[YINSP_pidbad_fmt] = _("unable to inspect, pid %d not found");
   Norm_nlstab[YINSP_pidsee_fmt] = _("inspect at PID [default pid = %d]");
   Norm_nlstab[YINSP_status_fmt] = _("%s: %*d-%-*d lines, %*d-%*d columns, %lu bytes read");
   Norm_nlstab[YINSP_waitin_txt] = _("patience please, working ...");
   Norm_nlstab[YINSP_workin_txt] = _("working, use Ctrl-C to end ...");
/* Translation Hint: Below are 2 abbreviations which can be as long as needed:
   .                 FLD = FIELD, VAL = VALUE */
   Norm_nlstab[OSEL_prompts_fmt] = _("add filter #%d (%s) as: [!]FLD?VAL");
   Norm_nlstab[OSEL_casenot_txt] = _("ignoring case");
   Norm_nlstab[OSEL_caseyes_txt] = _("case sensitive");
   Norm_nlstab[OSEL_errdups_txt] = _("duplicate filter was ignored");
   Norm_nlstab[OSEL_errdelm_fmt] = _("'%s' filter delimiter is missing");
   Norm_nlstab[OSEL_errvalu_fmt] = _("'%s' filter value is missing");
   Norm_nlstab[WORD_include_txt] = _("include");
   Norm_nlstab[WORD_exclude_txt] = _("exclude");
   Norm_nlstab[OSEL_statlin_fmt] = _("<Enter> to resume, filters: %s");
   Norm_nlstab[WORD_noneone_txt] = _("none");
/* Translation Hint: The following word 'Node' should be exactly
                     4 characters, excluding leading %%, fmt chars & colon) */
   Norm_nlstab[NUMA_nodenam_fmt] = _("%%Node%-2d:");
   Norm_nlstab[NUMA_nodeget_fmt] = _("expand which numa node (0-%d)");
   Norm_nlstab[NUMA_nodebad_txt] = _("invalid numa node");
   Norm_nlstab[NUMA_nodenot_txt] = _("sorry, NUMA extensions unavailable");
/* Translation Hint: 'Mem ' is an abbreviation for physical memory/ram
   .                 'Swap' represents the linux swap file --
   .                 please make both translations exactly 4 characters,
   .                 padding with extra spaces as necessary */
   Norm_nlstab[WORD_abv_mem_txt] = _("Mem ");
   Norm_nlstab[WORD_abv_swp_txt] = _("Swap");
   Norm_nlstab[LIB_errormem_fmt] = _("library failed memory statistics, at %d: %s");
   Norm_nlstab[LIB_errorcpu_fmt] = _("library failed cpu statistics, at %d: %s");
   Norm_nlstab[LIB_errorpid_fmt] = _("library failed pids statistics, at %d: %s");
   Norm_nlstab[BAD_memscale_fmt] = _("bad memory scaling arg '%s'");
   Norm_nlstab[XTRA_vforest_fmt] = _("PID to collapse/expand [default pid = %d]");
   Norm_nlstab[XTRA_modebad_txt] = _("wrong mode, command inactive");
   Norm_nlstab[XTRA_warnold_txt] = _("saving prevents older top from reading, save anyway?");
   Norm_nlstab[X_SEMAPHORES_fmt] = _("failed sem_init() at %d: %s");
   Norm_nlstab[X_THREADINGS_fmt] = _("failed pthread_create() at %d: %s");
   Norm_nlstab[X_RESTRICTED_txt] = _("sorry, restricted namespace with reduced functionality");
   Norm_nlstab[AGNI_valueof_fmt] = _("set pid %d AGNI value to");
   Norm_nlstab[AGNI_invalid_txt] = _("valid AGNI range is -20 to +19");
   Norm_nlstab[AGNI_notopen_fmt] = _("autogroup open failed, %s");
   Norm_nlstab[AGNI_nowrite_fmt] = _("autogroup write failed, %s");
   Norm_nlstab[X_BOT_cmdlin_fmt] = _("command line for pid %d, %s");
   Norm_nlstab[X_BOT_ctlgrp_fmt] = _("control groups for pid %d, %s");
   Norm_nlstab[X_BOT_envirn_fmt] = _("environment for pid %d, %s");
   Norm_nlstab[X_BOT_namesp_fmt] = _("namespaces for pid %d, %s");
   Norm_nlstab[X_BOT_nodata_txt] = _("n/a");
   Norm_nlstab[X_BOT_supgrp_fmt] = _("supplementary groups for pid %d, %s");
   Norm_nlstab[X_BOT_msglog_txt] = _("message log, last 10 messages:");
}


        /*
         * This routine builds the nls table containing specially
         * formatted strings designed to fit within an 80x24 terminal. */
static void build_uniq_nlstab (void) {

/* Translation Notes ------------------------------------------------
   .  It is strongly recommend that the --no-wrap command line option
   .  be used with all supporting translation tools, when available.
   .
   .  The next several text groups contain special escape sequences
   .  representing values used to index a table at run-time.
   .
   .  Each such sequence consists of a tilde (~) followed by an ascii
   .  number in the range of '1' - '8'.  Examples are '~2', '~8', etc.
   .  These escape sequences must never themselves be translated but
   .  could be deleted.
   .
   .  If you remove these escape sequences (both tilde and number) it
   .  would make translation easier.  However, the ability to display
   .  colors and bold text at run-time will have been lost.
   .
   .  Additionally, each of these text groups was designed to display
   .  in a 80x24 terminal window.  Hopefully, any translations will
   .  adhere to that goal lest the translated text be truncated.
   .
   .  If you would like additional information regarding these strings,
   .  please see the prologue to the show_special function in the top.c
   .  source file.
   .
   .  Caution:
   .    The next three items represent pages for interacting with a user.
   .    In all cases, the last lines of text must be treated with care.
   .
   .    They must not end with a newline character so that at runtime the
   .    cursor will remain on that final text line.
   .
   .    Also, the special sequences (tilde+number) must not appear on the
   .    last line (the one without the newline). So please avoid any line
   .    wraps that could place them there.
   . */

   Uniq_nlstab[KEYS_helpbas_fmt] = _(""
      "Help for Interactive Commands~2 - %s\n"
      "Window ~1%s~6: ~1Cumulative mode ~3%s~2.  ~1System~6: ~1Delay ~3%.1f secs~2; ~1Secure mode ~3%s~2.\n"
      "\n"
      "  Z~5,~1B~5,E,e   Global: '~1Z~2' colors; '~1B~2' bold; '~1E~2'/'~1e~2' summary/task memory scale\n"
      "  l,t,m,I,0 Toggle: '~1l~2' load avg; '~1t~2' task/cpu; '~1m~2' memory; '~1I~2' Irix; '~10~2' zeros\n"
      "  1,2,3,4,5 Toggle: '~11~2/~12~2/~13~2' cpu/numa views; '~14~2' cpus abreast; '~15~2' P/E-cores\n"
      "  f,X       Fields: '~1f~2' add/remove/order/sort; '~1X~2' increase fixed-width fields\n"
      "\n"
      "  L,&,<,> . Locate: '~1L~2'/'~1&~2' find/again; Move sort column: '~1<~2'/'~1>~2' left/right\n"
      "  R,H,J,C . Toggle: '~1R~2' Sort; '~1H~2' Threads; '~1J~2' Num justify; '~1C~2' Coordinates\n"
      "  c,i,S,j . Toggle: '~1c~2' Cmd name/line; '~1i~2' Idle; '~1S~2' Time; '~1j~2' Str justify\n"
      "  x~5,~1y~5     . Toggle highlights: '~1x~2' sort field; '~1y~2' running tasks\n"
      "  z~5,~1b~5     . Toggle: '~1z~2' color/mono; '~1b~2' bold/reverse (only if 'x' or 'y')\n"
      "  u,U,o,O . Filter by: '~1u~2'/'~1U~2' effective/any user; '~1o~2'/'~1O~2' other criteria\n"
      "  n,#,^O  . Set: '~1n~2'/'~1#~2' max tasks displayed; Show: ~1Ctrl~2+'~1O~2' other filter(s)\n"
      "  V,v,F   . Toggle: '~1V~2' forest view; '~1v~2' hide/show children; '~1F~2' keep focused\n"
      "\n"
      "%s"
      "  ^G,K,N,U  View: ctl groups ~1^G~2; cmdline ~1^K~2; environment ~1^N~2; supp groups ~1^U~2\n"
      "  Y,!,^E,P  Inspect '~1Y~2'; Combine Cpus '~1!~2'; Scale time ~1^E~2; View namespaces ~1^P~2\n"
      "  W,q       Write config file '~1W~2'; Quit '~1q~2'\n"
      "          ( commands shown with '.' require a ~1visible~2 task display ~1window~2 ) \n"
      "Press '~1h~2' or '~1?~2' for help with ~1Windows~2,\n"
      "Type 'q' or <Esc> to continue ");

   Uniq_nlstab[WINDOWS_help_fmt] = _(""
      "Help for Windows / Field Groups~2 - \"Current Window\" = ~1 %s ~6\n"
      "\n"
      ". Use multiple ~1windows~2, each with separate config opts (color,fields,sort,etc)\n"
      ". The 'current' window controls the ~1Summary Area~2 and responds to your ~1Commands~2\n"
      "  . that window's ~1task display~2 can be turned ~1Off~2 & ~1On~2, growing/shrinking others\n"
      "  . with ~1NO~2 task display, some commands will be ~1disabled~2 ('i','R','n','c', etc)\n"
      "    until a ~1different window~2 has been activated, making it the 'current' window\n"
      ". You ~1change~2 the 'current' window by: ~1 1~2) cycling forward/backward;~1 2~2) choosing\n"
      "  a specific field group; or~1 3~2) exiting the color mapping or fields screens\n"
      ". Commands ~1available anytime   -------------~2\n"
      "    A       . Alternate display mode toggle, show ~1Single~2 / ~1Multiple~2 windows\n"
      "    g       . Choose another field group and make it 'current', or change now\n"
      "              by selecting a number from: ~1 1~2 =%s;~1 2~2 =%s;~1 3~2 =%s; or~1 4~2 =%s\n"
      ". Commands ~1requiring~2 '~1A~2' mode~1  -------------~2\n"
      "    G       . Change the ~1Name~5 of the 'current' window/field group\n"
      " ~1*~4  a , w   . Cycle through all four windows:  '~1a~5' Forward; '~1w~5' Backward\n"
      " ~1*~4  - , _   . Show/Hide:  '~1-~5' ~1Current~2 window; '~1_~5' all ~1Visible~2/~1Invisible~2\n"
      "  The screen will be divided evenly between task displays.  But you can make\n"
      "  some ~1larger~2 or ~1smaller~2, using '~1n~2' and '~1i~2' commands.  Then later you could:\n"
      " ~1*~4  = , +   . Rebalance tasks:  '~1=~5' ~1Current~2 window; '~1+~5' ~1Every~2 window\n"
      "              (this also forces the ~1current~2 or ~1every~2 window to become visible)\n"
      "\n"
      "In '~1A~2' mode, '~1*~4' keys are your ~1essential~2 commands.  Please try the '~1a~2' and '~1w~2'\n"
      "commands plus the 'g' sub-commands NOW.  Press <Enter> to make 'Current' ");

/* Translation Notes ------------------------------------------------
   .  The following 'Help for color mapping' simulated screen should
   .  probably NOT be translated.  It is terribly hard to follow in
   .  this form and any translation could produce unpleasing results
   .  that are unlikely to parallel the running top program.
   .
   .  If you decide to proceed with translation, please take care
   .  to not disturb the spaces and the tilde + number delimiters.
   . */
   Uniq_nlstab[COLOR_custom_fmt] = _(""
      "Help for color mapping~2 - \"Current Window\" = ~1 %s ~6\n"
      "\n"
      "   color - 04:25:44 up 8 days, 50 min,  7 users,  load average:\n"
      "   Tasks:~3  64 ~2total,~3   2 ~3running,~3  62 ~2sleeping,~3   0 ~2stopped,~3\n"
      "   %%Cpu(s):~3  76.5 ~2user,~3  11.2 ~2system,~3   0.0 ~2nice,~3  12.3 ~2idle~3\n"
      "   ~1 Nasty Message! ~4  -or-  ~1Input Prompt~5\n"
      "   ~1  PID TTY     PR  NI %%CPU    TIME+   VIRT SWAP S COMMAND    ~6\n"
      "   17284 ~8pts/2  ~7  8   0  0.0   0:00.75  1380    0 S /bin/bash   ~8\n"
      "   ~1 8601 pts/1    7 -10  0.4   0:00.03   916    0 R color -b -z~7\n"
      "   11005 ~8?      ~7  9   0  0.0   0:02.50  2852 1008 S amor -sessi~8\n"
      "   available toggles: ~1B~2 =disable bold globally (~1%s~2),\n"
      "       ~1z~2 =color/mono (~1%s~2), ~1b~2 =tasks \"bold\"/reverse (~1%s~2)\n"
      "\n"
      "1) Select a ~1target~2 as an upper case letter, ~1current target~2 is ~1 %c ~4:\n"
      "   S~2 = Summary Data,~1  M~2 = Messages/Prompts,\n"
      "   H~2 = Column Heads,~1  T~2 = Task Information\n"
      "2) Select a ~1color~2 as a number or use the up/down arrow keys\n"
      "   to raise/lower the %d colors value, ~1current color~2 is ~1 %d ~4:\n"
      "   0~2 = black,~1  1~2 = red,    ~1  2~2 = green,~1  3~2 = yellow,\n"
      "   4~2 = blue, ~1  5~2 = magenta,~1  6~2 = cyan, ~1  7~2 = white\n"
      "\n"
      "3) Then use these keys when finished:\n"
      "   'q' or <Esc> to abort changes to window '~1%s~2'\n"
      "   'a' or 'w' to commit & change another, <Enter> to commit and end ");

/* Translation Hint: As is true for the text above, the "keys" shown to the left and
   .                 also embedded in the translatable text (along with escape seqs)
   .                 should never themselves be translated. */
   Uniq_nlstab[KEYS_helpext_fmt] = _(""
      "  d,k,r,^R '~1d~2' set delay; '~1k~2' kill; '~1r~2' renice; ~1Ctrl~2+'~1R~2' renice autogroup\n");

/* Translation Hint:
   .  This Fields Management header should be 3 lines long so as
   .  to allow 1 blank line before the fields & descriptions.
   .  If absolutely necessary, 4 lines could be used (but never more).
   . */
   Uniq_nlstab[FIELD_header_fmt] = _(""
      "Fields Management~2 for window ~1%s~6, whose current sort field is ~1%s~2\n"
      "   Navigate with Up/Dn, Right selects for move then <Enter> or Left commits,\n"
      "   'd' or <Space> toggles display, 's' sets sort.  Use 'q' or <Esc> to end!\n");

/* Translation Hint:
   .  The next 5 items must each be translated as a single line.
   . */
   Uniq_nlstab[STATE_line_1_fmt] = _("%s:~3"
      " %3u ~2total,~3 %3u ~2running,~3 %3u ~2sleeping,~3 %3u ~2stopped,~3 %3u ~2zombie~3\n");

/* Translation Hint: Only the following abbreviations need be translated
   .                 us = user, sy = system, ni = nice, id = idle, wa = wait,
   .                 hi hardware interrupt, si = software interrupt */
   Uniq_nlstab[STATE_lin2x6_fmt] = _("%s~3"
      " %#5.1f ~2us,~3 %#5.1f ~2sy,~3 %#5.1f ~2ni,~3 %#5.1f ~2id,~3 %#5.1f ~2wa,~3 %#5.1f ~2hi,~3 %#5.1f ~2si~3 ~1");

/* Translation Hint: Only the following abbreviations need be translated
   .                 us = user, sy = system, ni = nice, id = idle, wa = wait,
   .                 hi hardware interrupt, si = software interrupt, st = steal time */
   Uniq_nlstab[STATE_lin2x7_fmt] = _("%s~3"
      "%#5.1f ~2us,~3%#5.1f ~2sy,~3%#5.1f ~2ni,~3%#5.1f ~2id,~3%#5.1f ~2wa,~3%#5.1f ~2hi,~3%#5.1f ~2si,~3%#5.1f ~2st~3 ~1");

/* Translation Hint: next 2 must be treated together, with WORDS above & below aligned */
   Uniq_nlstab[MEMORY_line1_fmt] = _(""
      "%s %s:~3 %9.9s~2total,~3 %9.9s~2free,~3 %9.9s~2used,~3 %9.9s~2buff/cache~3 ~1    ");
   Uniq_nlstab[MEMORY_line2_fmt] = _(""
      "%s %s:~3 %9.9s~2total,~3 %9.9s~2free,~3 %9.9s~2used.~3 %9.9s~2avail %s~3");

/* Translation Hint:
   .  The next 2 headers for 'Inspection' must each be 3 lines or less
   . */
   Uniq_nlstab[YINSP_hdsels_fmt] = _(""
      "Inspection~2 Pause at: pid ~1%d~6, running ~1%s~6\n"
      "Use~2:  left/right then <Enter> to ~1select~5 an option; 'q' or <Esc> to ~1end~5 !\n"
      "Options~2: ~1%s\n");

   Uniq_nlstab[YINSP_hdview_fmt] = _(""
      "Inspection~2 View at pid: ~1%s~3, running ~1%s~3.  Locating: ~1%s~6\n"
      "Use~2:  left/right/up/down/etc to ~1navigate~5 the output; 'L'/'&' to ~1locate~5/~1next~5.\n"
      "Or~2:   <Enter> to ~1select another~5; 'q' or <Esc> to ~1end~5 !\n");
}


        /*
         * This function must be called very early at startup, before
         * any other function call, and especially before any changes
         * have been made to the terminal if NLS_VALIDATE is defined!
         *
         * The gettext documentation suggests that alone among locale
         * variables LANGUAGE=ll_CC may be abbreviated as LANGUAGE=ll
         * to denote the language's main dialect.  Unfortunately this
         * does not appear to be true.  One must specify the complete
         * ll_CC.  Optionally, a '.UTF-8' or '.uft8' suffix, as shown
         * in the following examples, may also be included:
         *    export LANGUAGE=ll_CC          # minimal requirement
         *    export LANGUAGE=ll_CC.UTF-8    # optional convention
         *    export LANGUAGE=ll_CC.utf8     # ok, too
         *
         * Additionally, as suggested in the gettext documentation, a
         * user will also have to export an empty LC_ALL= to actually
         * enable any translations.
         */
void initialize_nls (void) {
#ifdef NLS_VALIDATE
   static const char *nls_err ="\t%s_nlstab[%d] == NULL\n";
   int i;

   setlocale (LC_ALL, "");
   bindtextdomain(PACKAGE, LOCALEDIR);
   textdomain(PACKAGE);

   memset(Head_nlstab, 0, sizeof(Head_nlstab));
   memset(Desc_nlstab, 0, sizeof(Desc_nlstab));
   build_two_nlstabs();
   for (i = 0; i < EU_MAXPFLGS; i++) {
      if (!Head_nlstab[i]) {
         fprintf(stderr, nls_err, "Head", i);
         exit(1);
      }
      if (!Desc_nlstab[i]) {
         fprintf(stderr, nls_err, "Desc", i);
         exit(1);
      }
   }
   memset(Norm_nlstab, 0, sizeof(Norm_nlstab));
   build_norm_nlstab();
   for (i = 0; i < norm_MAX; i++)
      if (!Norm_nlstab[i]) {
         fprintf(stderr, nls_err, "Norm", i);
         exit(1);
      }
   memset(Uniq_nlstab, 0, sizeof(Uniq_nlstab));
   build_uniq_nlstab();
   for (i = 0; i < uniq_MAX; i++)
      if (!Uniq_nlstab[i]) {
         fprintf(stderr, nls_err, "Uniq", i);
         exit(1);
      }
#else
   setlocale (LC_ALL, "");
   bindtextdomain(PACKAGE, LOCALEDIR);
   textdomain(PACKAGE);

   build_two_nlstabs();
   build_norm_nlstab();
   build_uniq_nlstab();
#endif
}
