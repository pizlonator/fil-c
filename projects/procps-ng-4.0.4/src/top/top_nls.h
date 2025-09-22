/* top_nls.h - provide the basis for future nls translations */
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
#ifndef _Itop_nls
#define _Itop_nls

        /*
         * These are our string tables with the following contents:
         *    Head : column headings with varying size limits
         *    Desc : field descriptions not to exceed 20 screen positions
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
extern const char *Head_nlstab[];
extern const char *Desc_nlstab[];
extern const char *Norm_nlstab[];
extern const char *Uniq_nlstab[];

        /*
         * Simple optional macros to ease table access.
         * The N_txt and N_fmt macros are interchangeable but
         * highlight the two types of strings found in Norm_nlstable.
         */
#define N_col(e) Head_nlstab[e]
#define N_fld(e) Desc_nlstab[e]
#define N_txt(e) Norm_nlstab[e]
#define N_fmt(e) Norm_nlstab[e]
#define N_unq(e) Uniq_nlstab[e]

        /*
         * These enums are the means to access two of our four tables.
         * The Head_nlstab and Desc_nlstab are accessed with standard
         * top pflag enums.
         *
         * The norm_nls enums carry a suffix distinguishing plain text
         * from any text also containiing c-format specifiers.
         */
enum norm_nls {
   AGNI_invalid_txt, AGNI_notopen_fmt, AGNI_nowrite_fmt, AGNI_valueof_fmt,
   AMT_exxabyte_txt, AMT_gigabyte_txt, AMT_kilobyte_txt, AMT_megabyte_txt,
   AMT_petabyte_txt, AMT_terabyte_txt, BAD_delayint_fmt, BAD_integers_txt,
   BAD_max_task_txt, BAD_memscale_fmt, BAD_mon_pids_fmt, BAD_niterate_fmt,
   BAD_numfloat_txt, BAD_signalid_txt, BAD_username_txt, BAD_widtharg_fmt,
   CHOOSE_group_txt, COLORS_nomap_txt, DELAY_badarg_txt, DELAY_change_fmt,
   DELAY_secure_txt, DISABLED_cmd_txt, DISABLED_win_fmt, EXIT_signals_fmt,
   FAIL_alloc_c_txt, FAIL_alloc_r_txt, FAIL_rc_open_fmt, FAIL_re_nice_fmt,
   FAIL_signals_fmt, FAIL_tty_get_txt, FAIL_tty_set_fmt, FAIL_widecpu_txt,
   FAIL_widepid_txt, FIND_no_find_fmt, FIND_no_next_txt, FOREST_modes_fmt,
   FOREST_views_txt, GET_find_str_txt, GET_max_task_fmt, GET_nice_num_fmt,
   GET_pid2kill_fmt, GET_pid2nice_fmt, GET_sigs_num_fmt, GET_user_ids_txt,
   HELP_cmdline_fmt, IRIX_curmode_fmt, LIB_errorcpu_fmt, LIB_errormem_fmt,
   LIB_errorpid_fmt, LIMIT_exceed_fmt, MISSING_args_fmt, NAME_windows_fmt,
   NOT_onsecure_txt, NOT_smp_cpus_txt, NUMA_nodebad_txt, NUMA_nodeget_fmt,
   NUMA_nodenam_fmt, NUMA_nodenot_txt, OFF_one_word_txt, ON_word_only_txt,
   OSEL_casenot_txt, OSEL_caseyes_txt, OSEL_errdelm_fmt, OSEL_errdups_txt,
   OSEL_errvalu_fmt, OSEL_prompts_fmt, OSEL_statlin_fmt, RC_bad_entry_fmt,
   RC_bad_files_fmt, SCROLL_coord_fmt, SELECT_clash_txt, THREADS_show_fmt,
   TIME_accumed_fmt, UNKNOWN_cmds_txt, UNKNOWN_opts_fmt, VERSION_opts_fmt,
   WORD_abv_mem_txt, WORD_abv_swp_txt, WORD_allcpus_txt, WORD_another_txt,
   WORD_eachcpu_fmt, WORD_exclude_txt, WORD_include_txt, WORD_noneone_txt,
   WORD_process_txt, WORD_threads_txt, WRITE_rcfile_fmt,
   XTRA_badflds_fmt, XTRA_fixwide_fmt, XTRA_modebad_txt, XTRA_vforest_fmt,
   XTRA_warncfg_txt, XTRA_warnold_txt, XTRA_winsize_txt,
   X_BOT_cmdlin_fmt, X_BOT_ctlgrp_fmt, X_BOT_envirn_fmt, X_BOT_msglog_txt,
   X_BOT_namesp_fmt, X_BOT_nodata_txt, X_BOT_supgrp_fmt, X_RESTRICTED_txt,
   X_SEMAPHORES_fmt, X_THREADINGS_fmt,
   YINSP_demo01_txt, YINSP_demo02_txt, YINSP_demo03_txt, YINSP_deqfmt_txt,
   YINSP_deqtyp_txt, YINSP_dstory_txt,
   YINSP_failed_fmt, YINSP_noent1_txt, YINSP_noent2_txt, YINSP_pidbad_fmt,
   YINSP_pidsee_fmt, YINSP_status_fmt, YINSP_waitin_txt, YINSP_workin_txt,
      norm_MAX
};

enum uniq_nls {
   COLOR_custom_fmt, FIELD_header_fmt, KEYS_helpbas_fmt, KEYS_helpext_fmt,
   MEMORY_line1_fmt, MEMORY_line2_fmt, STATE_lin2x6_fmt, STATE_lin2x7_fmt,
   STATE_line_1_fmt, WINDOWS_help_fmt, YINSP_hdsels_fmt, YINSP_hdview_fmt,
      uniq_MAX
};

void initialize_nls (void);

#endif /* _Itop_nls */

