/* top.h - Header file:         show Linux processes */
/*
 * Copyright Â© 2002-2023 Jim Warner <james.warner@comcast.net
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
 *    Albert D. Cahalan, <albert@users.sf.net>
 *    Sami Kerola, <kerolasa@iki.fi>
 */
#ifndef _Itop
#define _Itop

        /* Defines represented in configure.ac ----------------------------- */
//#define BOOST_MEMORY            /* enable extra precision in memory fields */
//#define BOOST_PERCNT            /* enable extra precision for two % fields */
//#define ORIG_TOPDEFS            /* with no rcfile retain original defaults */
//#define SIGNALS_LESS            /* favor reduced signal load over response */

        /* Development/Debugging defines ----------------------------------- */
//#define ATEOJ_RPTSTD            /* report on some miscellany at end-of-job */
//#define BOT_DEAD_ZAP            /* zap Ctrl bottom window when target dies */
//#define BOT_STRV_OFF            /* don't emphasize strv w/ focus if spaces */
//#define CASEUP_HEXES            /* show all those hex values in upper case */
//#define CASEUP_SUFIX            /* show time/mem/cnts suffix in upper case */
//#define CORE_TYPE_LO            /* show the type of cpu core in lower case */
//#define CORE_TYPE_NO            /* don't distinguish the types of cpu core */
//#define EQUCOLHDRYES            /* yes, equalize the column header lengths */
//#define FOCUS_HARD_Y            /* 'F' will avoid topmost task distortions */
//#define FOCUS_TREE_X            /* 'F' resets forest view indentation to 0 */
//#define FOCUS_VIZOFF            /* 'F' doesn't provide the visual clue '|' */
//#define GETOPTFIX_NO            /* do not address getopt_long deficiencies */
//#define INSP_JUSTNOT            /* do not smooth unprintable right margins */
//#define INSP_OFFDEMO            /* disable demo screens, issue msg instead */
//#define INSP_SAVEBUF            /* preserve 'Insp_buf' contents via a file */
//#define INSP_SLIDE_1            /* when scrolling left/right, don't move 8 */
//#define MEMGRAPH_OLD            /* don't use 'available' when graphing Mem */
//#define NLS_INCLUDED            /* provides for excluding from translation */
//#define NLS_VALIDATE            /* ensure the integrity of four nls tables */
//#define OFF_SCROLLBK            /* disable tty emulators scrollback buffer */
//#define OFF_STDERROR            /* disable our stderr buffering (redirect) */
//#define OFF_STDIOLBF            /* disable our own stdout 'IOFBF' override */
//#define OFF_XTRAWIDE            /* disable our extra wide multi-byte logic */
//#define OVERTYPE_SEE            /* display a visual hint for overtype mode */
//#define PRETEND0NUMA            /* pretend that there ain't any numa nodes */
//#define PRETEND48CPU            /* pretend we're smp with 48 ticsers (sic) */
//#define PRETENDECORE            /* pretend we've got some e-core type cpus */
//#define PRETENDNOCAP            /* pretend terminal missing essential caps */
//#define RCFILE_NOERR            /* rcfile errs silently default, vs. fatal */
//#define RECALL_FIXED            /* don't reorder saved strings if recalled */
//#define RMAN_IGNORED            /* don't consider auto right margin glitch */
//#define SCALE_FORMER            /* scale_tics() guy shouldn't mimic uptime */
//#define SCALE_POSTFX            /* scale_tics() try without a 'h,d' suffix */
//#define SCROLLVAR_NO            /* disable intra-column horizontal scrolls */
//#define SCROLLV_BY_1            /* when scrolling left/right do not move 8 */
//#define STRINGCASENO            /* case insensitive compare/locate version */
//#define TERMIOS_ONLY            /* use native input only (just limp along) */
//#define THREADED_CPU            /* separate background thread for cpu updt */
//#define THREADED_MEM            /* separate background thread for mem updt */
//#define THREADED_TSK            /* separate background thread for tsk updt */
//#define TOG4_MEM_1UP            /* don't show two abreast memory statistic */
//#define TOG4_MEM_FIX            /* no variable mem graphs, thus misaligned */
//#define TOG4_SEP_OFF            /* don't show two abreast visual separator */
//#define TOG4_SEP_STD            /* normal mem sep if 2 abreast & no graphs */
//#define TREE_NORESET            /* sort keys should not force 'V' view off */
//#define TREE_SCANALL            /* rescan array w/ forest view, avoid sort */
//#define TREE_VALTMRK            /* use an indented '+' with collapsed pids */
//#define TREE_VCPUOFF            /* a collapsed parent excludes child's cpu */
//#define TREE_VPROMPT            /* pid collapse/expand prompt, vs. top row */
//#define TREE_VWINALL            /* pid collapse/expand impacts all windows */
//#define USE_X_COLHDR            /* emphasize header vs. whole col, for 'x' */
//#define WIDEN_COLUMN            /* base column widths on translated header */


/*######  Notes, etc.  ###################################################*/

        /* For introducing inaugural cgroup support, thanks to:
              Jan Gorig <jgorig@redhat.com> - April, 2011 */

        /* For the motivation and path to nls support, thanks to:
              Sami Kerola, <kerolasa@iki.fi> - December, 2011 */

        /* There are still some short strings that may yet be candidates
           for nls support inclusion.  They're identified with:
              // nls_maybe */

        /* For the impetus and NUMA/Node prototype design, thanks to:
              Lance Shelton <LShelton@fusionio.com> - April, 2013 */

        /* For prompting & helping with top's utf-8 support, thanks to:
              GÃ¶ran Uddeborg <goeran@uddeborg.se> - September, 2017 */

   // pretend as if #define _GNU_SOURCE
char *strcasestr(const char *haystack, const char *needle);

#ifdef STRINGCASENO
#define STRSTR  strcasestr
#define STRCMP  strcasecmp
#else
#define STRSTR  strstr
#define STRCMP  strcmp
#endif


/*######  Some Miscellaneous constants  ##################################*/

        /* The default delay twix updates */
#ifdef ORIG_TOPDEFS
#define DEF_DELAY  3.0
#else
#define DEF_DELAY  1.5
#endif

        /* Length of time a message is displayed and the duration
           of a 'priming' wait during library startup (in microseconds) */
#define MSG_USLEEP  1250000
#define LIB_USLEEP  100000

        /* Specific process id monitoring support (command line only) */
#define MONPIDMAX  20

        /* Output override minimums (the -w switch and/or env vars) */
#define W_MIN_COL  3
#define W_MIN_ROW  3

        /* Miscellaneous buffers with liberal values and some other defines
           -- mostly just to pinpoint source code usage/dependencies */
#define SCREENMAX   512
   /* the above might seem pretty stingy, until you consider that with every
      field displayed the column header would be approximately 250 bytes
      -- so SCREENMAX provides for all fields plus a 250+ byte command line */
#define TNYBUFSIZ    16
#define CAPBUFSIZ    32
#define CLRBUFSIZ    64
#define PFLAGSSIZ   128
#define SMLBUFSIZ   128
#define MEDBUFSIZ   256
#define LRGBUFSIZ   512
#define OURPATHSZ  1024
#define BIGBUFSIZ  2048
#define BOTBUFSIZ 16384
        // next is same as library's max buffer size
#define MAXBUFSIZ (1024*64*2)
   /* in addition to the actual display data, our row might have to accommodate
      many termcap/color transitions - these definitions ensure we have room */
#define ROWMINSIZ  ( SCREENMAX +  8 * (CAPBUFSIZ + CLRBUFSIZ) )
#define ROWMAXSIZ  ( SCREENMAX + 16 * (CAPBUFSIZ + CLRBUFSIZ) )
   // minimum size guarantee for dynamically acquired 'readfile' buffer
#define READMINSZ  2048
   // size of preallocated search string buffers, same as ioline()
#define FNDBUFSIZ  MEDBUFSIZ


   // space between task fields/columns
#define COLPADSTR   " "
#define COLPADSIZ   ( sizeof(COLPADSTR) - 1 )
   // continuation ch when field/column truncated
#define COLPLUSCH   '+'

   // support for keyboard stuff (cursor motion keystrokes, mostly)
#define kbd_ESC    '\033'
#define kbd_SPACE  ' '
#define kbd_ENTER  '\n'
#define kbd_UP     129
#define kbd_DOWN   130
#define kbd_LEFT   131
#define kbd_RIGHT  132
#define kbd_PGUP   133
#define kbd_PGDN   134
#define kbd_HOME   135
#define kbd_END    136
#define kbd_BKSP   137
#define kbd_INS    138
#define kbd_DEL    139
#define kbd_BTAB   140
#define kbd_CtrlE  '\005'
#define kbd_CtrlG  '\007'
#define kbd_CtrlI  '\011'
#define kbd_CtrlK  '\013'
#define kbd_CtrlL  '\014'
#define kbd_CtrlN  '\016'
#define kbd_CtrlO  '\017'
#define kbd_CtrlP  '\020'
#define kbd_CtrlR  '\022'
#define kbd_CtrlU  '\025'

        /* Special value in Pseudo_row to force an additional procs refresh
           -- used at startup and for task/thread mode transitions */
#define PROC_XTRA  -1


/* #####  Enum's and Typedef's  ############################################ */

        /* Flags for each possible field (and then some) --
           these MUST be kept in sync with the Fieldstab[] array !! */
enum pflag {
   EU_PID = 0, EU_PPD,
   EU_UED, EU_UEN, EU_URD, EU_URN, EU_USD, EU_USN,
   EU_GID, EU_GRP, EU_PGD, EU_TTY, EU_TPG, EU_SID,
   EU_PRI, EU_NCE, EU_THD,
   EU_CPN, EU_CPU, EU_TME, EU_TM2,
   EU_MEM, EU_VRT, EU_SWP, EU_RES, EU_COD, EU_DAT, EU_SHR,
   EU_FL1, EU_FL2, EU_DRT,
   EU_STA, EU_CMD, EU_WCH, EU_FLG, EU_CGR,
   EU_SGD, EU_SGN, EU_TGD,
   EU_OOA, EU_OOM,
   EU_ENV,
   EU_FV1, EU_FV2,
   EU_USE,
   EU_NS1, EU_NS2, EU_NS3, EU_NS4, EU_NS5, EU_NS6,
   EU_LXC,
   EU_RZA, EU_RZF, EU_RZL, EU_RZS,
   EU_CGN,
   EU_NMA,
   EU_LID,
   EU_EXE,
   EU_RSS, EU_PSS, EU_PZA, EU_PZF, EU_PZS, EU_USS,
   EU_IRB, EU_IRO, EU_IWB, EU_IWO,
   EU_AGI, EU_AGN,
   EU_TM3, EU_TM4, EU_CUU, EU_CUC,
   EU_NS7, EU_NS8,
#ifdef USE_X_COLHDR
   // not really pflags, used with tbl indexing
   EU_MAXPFLGS
#else
   // not really pflags, used with tbl indexing & col highlighting
   EU_MAXPFLGS, EU_XON, EU_XOF
#endif
};

        /* The scaling 'target' used with memory fields */
enum scale_enum {
   SK_Kb, SK_Mb, SK_Gb, SK_Tb, SK_Pb, SK_Eb
};

        /* Used to manipulate (and document) the Frames_signal states */
enum resize_states {
   BREAK_off = 0, BREAK_kbd, BREAK_sig, BREAK_autox, BREAK_screen
};

        /* This typedef just ensures consistent 'process flags' handling */
typedef int FLG_t;

        /* These typedefs attempt to ensure consistent 'ticks' handling */
typedef unsigned long long TIC_t;
typedef          long long SIC_t;


        /* /////////////////////////////////////////////////////////////// */
        /* Special Section: multiple windows/field groups  --------------- */
        /* ( kind of a header within a header: constants, types & macros ) */

#define CAPTABMAX  9             /* max entries in each win's caps table   */
#define GROUPSMAX  4             /* the max number of simultaneous windows */
#define WINNAMSIZ  4             /* size of RCW_t winname buf (incl '\0')  */
#define GRPNAMSIZ  WINNAMSIZ+2   /* window's name + number as in: '#:...'  */

        /* The Persistent 'Mode' flags!
           These are preserved in the rc file, as a single integer and the
           letter shown is the corresponding 'command' toggle */
        // 'View_' flags affect the summary (minimum), taken from 'Curwin'
#define View_CPUSUM  0x008000     // '1' - show combined cpu stats (vs. each)
#define View_CPUNOD  0x400000     // '2' - show numa node cpu stats ('3' also)
#define View_LOADAV  0x004000     // 'l' - display load avg and uptime summary
#define View_STATES  0x002000     // 't' - display task/cpu(s) states summary
#define View_MEMORY  0x001000     // 'm' - display memory summary
#define View_NOBOLD  0x000008     // 'B' - disable 'bold' attribute globally
#define View_SCROLL  0x080000     // 'C' - enable coordinates msg w/ scrolling
        // 'Show_' & 'Qsrt_' flags are for task display in a visible window
#define Show_COLORS  0x000800     // 'z' - show in color (vs. mono)
#define Show_HIBOLD  0x000400     // 'b' - rows and/or cols bold (vs. reverse)
#define Show_HICOLS  0x000200     // 'x' - show sort column emphasized
#define Show_HIROWS  0x000100     // 'y' - show running tasks highlighted
#define Show_CMDLIN  0x000080     // 'c' - show cmdline vs. name
#define Show_CTIMES  0x000040     // 'S' - show times as cumulative
#define Show_IDLEPS  0x000020     // 'i' - show idle processes (all tasks)
#define Show_TASKON  0x000010     // '-' - tasks showable when Mode_altscr
#define Show_FOREST  0x000002     // 'V' - show cmd/cmdlines with ascii art
#define Qsrt_NORMAL  0x000004     // 'R' - reversed column sort (high to low)
#define Show_JRSTRS  0x040000     // 'j' - right justify "string" data cols
#define Show_JRNUMS  0x020000     // 'J' - right justify "numeric" data cols
        // these flag(s) have no command as such - they're for internal use
#define NOPRINT_xxx  0x010000     // build task rows only (not for display)
#define EQUWINS_xxx  0x000001     // rebalance all wins & tasks (off i,n,u/U)

        // Default flags if there's no rcfile to provide user customizations
#ifdef ORIG_TOPDEFS
#define DEF_WINFLGS ( View_LOADAV | View_STATES | View_CPUSUM | View_MEMORY \
   | Show_HIBOLD | Show_HIROWS | Show_IDLEPS | Show_TASKON | Show_JRNUMS \
   | Qsrt_NORMAL )
#define DEF_GRAPHS2  0, 0
#define DEF_SCALES2  SK_Mb, SK_Kb
#define ALT_WINFLGS  DEF_WINFLGS
#define ALT_GRAPHS2  0, 0
#else
#define DEF_WINFLGS ( View_LOADAV | View_STATES | View_MEMORY | Show_CMDLIN \
   | Show_COLORS | Show_FOREST | Show_HIROWS | Show_IDLEPS | Show_JRNUMS | Show_TASKON \
   | Qsrt_NORMAL )
#define DEF_GRAPHS2  1, 2
#define DEF_SCALES2  SK_Gb, SK_Mb
#define ALT_WINFLGS (DEF_WINFLGS | Show_HIBOLD) & ~Show_FOREST
#define ALT_GRAPHS2  2, 0
#endif

        /* These are used to direct wins_reflag */
enum reflag_enum {
   Flags_TOG, Flags_SET, Flags_OFF
};

        /* These are used to direct win_warn */
enum warn_enum {
   Warn_ALT, Warn_VIZ
};

        /* This type helps support both a window AND the rcfile */
typedef struct RCW_t {  // the 'window' portion of an rcfile
   int    sortindx,               // sort field (represented as procflag)
          winflags,               // 'view', 'show' and 'sort' mode flags
          maxtasks,               // user requested maximum, 0 equals all
          graph_cpus,             // 't' - View_STATES supplementary vals
          graph_mems,             // 'm' - View_MEMORY supplememtary vals
          double_up,              // '4' - show multiple cpus on one line
          combine_cpus,           // '!' - keep combining additional cpus
          core_types,             // '5' - show/filter P-core/E-core cpus
          summclr,                // a colors 'number' used for summ info
          msgsclr,                //             "           in msgs/pmts
          headclr,                //             "           in cols head
          taskclr;                //             "           in task rows
   char   winname [WINNAMSIZ];    // name for the window, user changeable
   FLG_t  fieldscur [PFLAGSSIZ];  // the fields for display & their order
} RCW_t;

        /* This represents the complete rcfile */
typedef struct RCF_t {
   char   id;                   // rcfile version id
   int    mode_altscr;          // 'A' - Alt display mode (multi task windows)
   int    mode_irixps;          // 'I' - Irix vs. Solaris mode (SMP-only)
   float  delay_time;           // 'd'/'s' - How long to sleep twixt updates
   int    win_index;            // Curwin, as index
   RCW_t  win [GROUPSMAX];      // a 'WIN_t.rc' for each window
   int    fixed_widest;         // 'X' - wider non-scalable col addition
   int    summ_mscale;          // 'E' - scaling of summary memory values
   int    task_mscale;          // 'e' - scaling of process memory values
   int    zero_suppress;        // '0' - suppress scaled zeros toggle
   int    tics_scaled;          // ^E  - scale TIME and/or TIME+ columns
} RCF_t;

        /* This structure stores configurable information for each window.
           By expending a little effort in its creation and user requested
           maintenance, the only real additional per frame cost of having
           windows is an extra sort -- but that's just on pointers! */
typedef struct WIN_t {
   FLG_t  pflgsall [PFLAGSSIZ],        // all 'active/on' fieldscur, as enum
          procflgs [PFLAGSSIZ];        // fieldscur subset, as enum
   RCW_t  rc;                          // stuff that gets saved in the rcfile
   int    winnum,          // a window's number (array pos + 1)
          winlines,        // current task window's rows (volatile)
          maxpflgs,        // number of displayed procflgs ("on" in fieldscur)
          totpflgs,        // total of displayable procflgs in pflgsall array
          begpflg,         // scrolled beginning pos into pflgsall array
          endpflg,         // scrolled ending pos into pflgsall array
          begtask,         // scrolled beginning pos into total tasks
          begnext,         // new scrolled delta for next frame's begtask
#ifndef SCROLLVAR_NO
          varcolbeg,       // scrolled position within variable width col
#endif
          varcolsz,        // max length of variable width column(s)
          usrseluid,       // validated uid for 'u/U' user selection
          usrseltyp,       // the basis for matching above uid
          usrselflg,       // flag denoting include/exclude matches
          hdrcaplen;       // column header xtra caps len, if any
   char   capclr_sum [CLRBUFSIZ],      // terminfo strings built from
          capclr_msg [CLRBUFSIZ],      //   RCW_t colors (& rebuilt too),
          capclr_pmt [CLRBUFSIZ],      //   but NO recurring costs !
          capclr_hdr [CLRBUFSIZ],      //   note: sum, msg and pmt strs
          capclr_rowhigh [SMLBUFSIZ],  //         are only used when this
          capclr_rownorm [CLRBUFSIZ],  //         window is the 'Curwin'!
          cap_bold [CAPBUFSIZ],        // support for View_NOBOLD toggle
          grpname [GRPNAMSIZ],         // window number:name, printable
#ifdef USE_X_COLHDR
          columnhdr [ROWMINSIZ],       // column headings for procflgs
#else
          columnhdr [SCREENMAX],       // column headings for procflgs
#endif
         *captab [CAPTABMAX];          // captab needed by show_special()
   struct osel_s *osel_1st;            // other selection criteria anchor
   int    osel_tot;                    // total of other selection criteria
   char  *findstr;                     // window's current/active search string
   int    findlen;                     // above's strlen, without call overhead
   int    focus_pid;                   // target pid when 'F' toggle is active
   int    focus_beg;                   // ppt index where 'F' toggle has begun
   int    focus_end;                   // ppt index where 'F' toggle has ended
#ifdef FOCUS_TREE_X
   int    focus_lvl;                   // the indentation level of parent task
#endif
   struct pids_stack **ppt;            // this window's stacks ptr array
   struct WIN_t *next,                 // next window in window stack
                *prev;                 // prior window in window stack
} WIN_t;

        // Used to test/manipulate the window flags
#define CHKw(q,f)    (int)((q)->rc.winflags & (f))
#define TOGw(q,f)    (q)->rc.winflags ^=  (f)
#define SETw(q,f)    (q)->rc.winflags |=  (f)
#define OFFw(q,f)    (q)->rc.winflags &= ~(f)
#define ALTCHKw      (Rc.mode_altscr ? 1 : win_warn(Warn_ALT))
#define VIZISw(q)    (!Rc.mode_altscr || CHKw(q,Show_TASKON))
#define VIZCHKw(q)   (VIZISw(q)) ? 1 : win_warn(Warn_VIZ)
#define VIZTOGw(q,f) (VIZISw(q)) ? TOGw(q,(f)) : win_warn(Warn_VIZ)

        // Used to test/manipulte fieldscur values
#define FLDon        0x01
#define FLDoff       0x00
#define FLDget(q,i)  ( (((q)->rc.fieldscur[i]) >> 1) - FLD_OFFSET  )
#define FLDtog(q,i)  ( (q)->rc.fieldscur[i] ^= FLDon )
#define FLDviz(q,i)  ( (q)->rc.fieldscur[i] &  FLDon )
#define ENUviz(w,E)  ( NULL != msch((w)->procflgs, E, w->maxpflgs) )
#define ENUpos(w,E)  ( (int)(msch((w)->pflgsall, E, (w)->totpflgs) - (w)->pflgsall) )
#define ENUcvt(E,x)  ( (int)((E + FLD_OFFSET) << 1) | x )

        // Support for variable width columns (and potentially scrolling too)
#define VARcol(E)    (-1 == Fieldstab[E].width)
#ifndef SCROLLVAR_NO
#ifdef USE_X_COLHDR
#define VARright(w)  (1 == w->maxpflgs && VARcol(w->procflgs[0]))
#else
#define VARright(w) ((1 == w->maxpflgs && VARcol(w->procflgs[0])) || \
                     (3 == w->maxpflgs && EU_XON == w->procflgs[0] && VARcol(w->procflgs[1])))
#endif
#define VARleft(w)   (w->varcolbeg && VARright(w))
#ifdef SCROLLV_BY_1
#define SCROLLAMT    1
#else
#define SCROLLAMT    8
#endif
#endif

        // Support for a proper (visible) row #1 whenever Curwin changes
        // ( or a key which might affect vertical scrolling was struck )
#define mkVIZyes     ( Curwin->begnext != 0 )
#define mkVIZrow1    { Curwin->begnext = +1; Curwin->begtask -= 1; }
#define mkVIZrowX(n) { Curwin->begnext = (n); }
#define mkVIZoff(w)  { w->begnext = 0; }

        /* Special Section: end ------------------------------------------ */
        /* /////////////////////////////////////////////////////////////// */


/*######  Some Miscellaneous Macro definitions  ##########################*/

        /* Yield table size as 'int' */
#define MAXTBL(t)  (int)(sizeof(t) / sizeof(t[0]))

        /* A null-terminating strncpy, assuming strlcpy is not available.
           ( and assuming callers don't need the string length returned ) */
#define STRLCPY(dst,src) { memccpy(dst, src, '\0', sizeof(dst)); dst[sizeof(dst) - 1] = '\0'; }

        /* Used to clear all or part of our Pseudo_screen */
#define PSU_CLREOS(y) memset(&Pseudo_screen[ROWMAXSIZ*y], '\0', Pseudo_size-(ROWMAXSIZ*y))

/*
 * The following three macros are used to 'inline' those portions of the
 * display process involved in formatting, while protecting against any
 * potential embedded 'millesecond delay' escape sequences.
 */
        /**  PUTT - Put to Tty (used in many places)
               . for temporary, possibly interactive, 'replacement' output
               . may contain ANY valid terminfo escape sequences
               . need NOT represent an entire screen row */
#define PUTT(fmt,arg...) do { \
      char _str[ROWMAXSIZ]; \
      snprintf(_str, sizeof(_str), fmt, ## arg); \
      putp(_str); \
   } while (0)

        /**  PUFF - Put for Frame (used in only 3 places)
               . for more permanent frame-oriented 'update' output
               . may NOT contain cursor motion terminfo escapes
               . assumed to represent a complete screen ROW
               . subject to optimization, thus MAY be discarded */
#define PUFF(fmt,arg...) do { \
      char _str[ROWMAXSIZ]; \
      const int _len = snprintf(_str, sizeof(_str), fmt, ## arg); \
      if (Batch) { \
         char *_eol = _str + (_len < 0 ? 0 : (size_t)_len >= sizeof(_str) ? sizeof(_str)-1 : (size_t)_len); \
         while (_eol > _str && _eol[-1] == ' ') _eol--; \
         *_eol = '\0'; putp(_str); } \
      else if (Pseudo_row >= 0 && Pseudo_row < Screen_rows) { \
         char *_ptr = &Pseudo_screen[Pseudo_row++ * ROWMAXSIZ]; \
         if (!strcmp(_ptr, _str)) putp("\n"); \
         else { \
            strcpy(_ptr, _str); \
            putp(_ptr); } } \
   } while (0)

        /**  POOF - Pulled Out of Frame (used in only 1 place)
               . for output that is/was sent directly to the terminal
                 but would otherwise have been counted as a Pseudo_row */
#define POOF(str,cap) do { \
      putp(str); putp(cap); \
      Pseudo_screen[Pseudo_row * ROWMAXSIZ] = '\0'; \
      if (Pseudo_row + 1 < Screen_rows) ++Pseudo_row; \
   } while (0)

        /* Orderly end, with any sort of message - see fmtmk */
#define debug_END(s) { \
           void error_exit (const char *); \
           fputs(Cap_clr_scr, stdout); \
           error_exit(s); \
        }

        /* A poor man's breakpoint, if he's too lazy to learn gdb */
#define its_YOUR_fault { *((char *)0) = '!'; }


/*######  Some Display Support *Data*  ###################################*/
/*      ( see module top_nls.c for the nls translatable data ) */

        /* Configuration files support */
#define SYS_RCRESTRICT  "/etc/toprc"
#define SYS_RCDEFAULTS  "/etc/topdefaultrc"
#define RCF_EYECATCHER  "Config File (Linux processes with windows)\n"
#define RCF_PLUS_H      "\\]^_`abcdefghij"
#define RCF_PLUS_J      "klmnopqrstuvwxyz"
        // this next guy must never, ever change
        // ( transitioned from 'char' to 'int' )
#define RCF_XFORMED_ID  'k'
        // this next guy is incremented when columns change
        // ( to prevent older top versions from accessing )
#define RCF_VERSION_ID  'k'

#define FLD_OFFSET  ( (int)'%' )
#define FLD_ROWMAX  20

        /* The default fields displayed and their order,
           if nothing is specified by the loser, oops user. */
#ifdef ORIG_TOPDEFS
#define DEF_FORMER  "¥¨³´»½ÀÄ·º¹Å&')*+,-./012568<>?ABCFGHIJKLMNOPQRSTUVWXYZ[" RCF_PLUS_H RCF_PLUS_J
#else
#define DEF_FORMER  "¥&K¨³´»½@·º¹56ÄFÅ')*+,-./0128<>?ABCGHIJLMNOPQRSTUVWXYZ[" RCF_PLUS_H RCF_PLUS_J
#endif
        /* Pre-configured windows/field groups */
#define JOB_FORMER  "¥¦¹·º(³´Ä»½@<§Å)*+,-./012568>?ABCFGHIJKLMNOPQRSTUVWXYZ[" RCF_PLUS_H RCF_PLUS_J
#define MEM_FORMER  "¥º»<½¾¿ÀÁMBNÃD34·Å&'()*+,-./0125689FGHIJKLOPQRSTUVWXYZ[" RCF_PLUS_H RCF_PLUS_J
#define USR_FORMER  "¥¦§¨ª°¹·ºÄÅ)+,-./1234568;<=>?@ABCFGHIJKLMNOPQRSTUVWXYZ[" RCF_PLUS_H RCF_PLUS_J
        // old top fields ( 'a'-'z' ) in positions 0-25
        // other suse old top fields ( '{|' ) in positions 26-27
#define CVT_FORMER  "%&*'(-0346789:;<=>?@ACDEFGML)+,./125BHIJKNOPQRSTUVWXYZ[" RCF_PLUS_H RCF_PLUS_J
#define CVT_FLDMAX  28

#ifdef ORIG_TOPDEFS
#define DEF_FIELDS { \
     75,  81, 103, 105, 119, 123, 129, 137, 111, 117, 115, 139,  76,  78,  82,  84,  86,  88,  90,  92, \
     94,  96,  98, 100, 106, 108, 112, 120, 124, 126, 130, 132, 134, 140, 142, 144, 146, 148, 150, 152, \
    154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, \
    194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, \
    234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272  }
#else
#define DEF_FIELDS { \
     75,  76, 150,  81, 103, 105, 119, 123, 128, 111, 117, 115, 106, 108, 137, 140, 139,  78,  82,  84, \
     86,  88,  90,  92,  94,  96,  98, 100, 112, 120, 124, 126, 130, 132, 134, 142, 144, 146, 148, 152, \
    154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, \
    194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, \
    234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272  }
#endif
#define JOB_FIELDS { \
     75,  77, 115, 111, 117,  80, 103, 105, 137, 119, 123, 128, 120,  79, 139,  82,  84,  86,  88,  90, \
     92,  94,  96,  98, 100, 106, 108, 112, 124, 126, 130, 132, 134, 140, 142, 144, 146, 148, 150, 152, \
    154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, \
    194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, \
    234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272  }
#define MEM_FIELDS { \
     75, 117, 119, 120, 123, 125, 127, 129, 131, 154, 132, 156, 135, 136, 102, 104, 111, 139,  76,  78, \
     80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 106, 108, 112, 114, 140, 142, 144, 146, 148, \
    150, 152, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, \
    194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, \
    234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272  }
#define USR_FIELDS { \
     75,  77,  79,  81,  85,  97, 115, 111, 117, 137, 139,  82,  86,  88,  90,  92,  94,  98, 100, 102, \
    104, 106, 108, 112, 118, 120, 122, 124, 126, 128, 130, 132, 134, 140, 142, 144, 146, 148, 150, 152, \
    154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, \
    194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, \
    234, 236, 238, 240, 242, 244, 246, 248, 250, 252, 254, 256, 258, 260, 262, 264, 266, 268, 270, 272  }

        /* The default values for the local config file */
#define DEF_RCFILE { \
   RCF_VERSION_ID, 0, 1, DEF_DELAY, 0, { \
   { EU_CPU, DEF_WINFLGS, 0, DEF_GRAPHS2, 1, 0, 0, \
      COLOR_RED, COLOR_RED, COLOR_YELLOW, COLOR_RED, \
      "Def", DEF_FIELDS }, \
   { EU_PID, ALT_WINFLGS, 0, ALT_GRAPHS2, 0, 0, 0, \
      COLOR_CYAN, COLOR_CYAN, COLOR_WHITE, COLOR_CYAN, \
      "Job", JOB_FIELDS }, \
   { EU_MEM, ALT_WINFLGS, 0, ALT_GRAPHS2, 0, 0, 0, \
      COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLUE, COLOR_MAGENTA, \
      "Mem", MEM_FIELDS }, \
   { EU_UEN, ALT_WINFLGS, 0, ALT_GRAPHS2, 0, 0, 0, \
      COLOR_YELLOW, COLOR_YELLOW, COLOR_GREEN, COLOR_YELLOW, \
      "Usr", USR_FIELDS } \
   }, 0, DEF_SCALES2, 0, 0 }

        /* Summary Lines specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define LOADAV_line  "%s -%s\n"
#define LOADAV_line_alt  "%s~6 -%s\n"

/*######  For Piece of mind  #############################################*/

        /* just sanity check(s)... */
#if defined(RECALL_FIXED) && defined(TERMIOS_ONLY)
# error 'RECALL_FIXED' conflicts with 'TERMIOS_ONLY'
#endif
#if (LRGBUFSIZ < SCREENMAX)
# error 'LRGBUFSIZ' must NOT be less than 'SCREENMAX'
#endif
#if defined(TERMIOS_ONLY)
# warning 'TERMIOS_ONLY' disables input recall and makes man doc incorrect
#endif
#if defined(MEMGRAPH_OLD)
# warning 'MEMGRAPH_OLD' will make the man document Section 2c. misleading
#endif
#if defined(SCALE_FORMER) && defined(SCALE_POSTFX)
# warning 'SCALE_POSTFX' is ignored when 'SCALE_FORMER' is active
#endif
#if defined(USE_X_COLHDR)
# warning 'USE_X_COLHDR' makes parts of man page misleading (4e, 5d & 5e)
#endif
#if defined(TOG4_SEP_STD) && defined(TOG4_SEP_OFF)
# warning 'TOG4_SEP_STD' has no effect when 'TOG4_SEP_OFF' is active
#endif

/*######  Some Prototypes (ha!)  #########################################*/

   /* These 'prototypes' are here exclusively for documentation purposes. */
   /* ( see the find_string routine for the one true required prototype ) */
/*------  Tiny useful routine(s)  ----------------------------------------*/
//atic const char   *fmtmk (const char *fmts, ...);
//atic inline int    mlen (const int *mem);
//atic inline int   *msch (const int *mem, int obj, int max);
//atic inline char  *scat (char *dst, const char *src);
//atic const char   *tg2 (int x, int y);
/*------  Exit/Interrupt routines  ---------------------------------------*/
//atic void          at_eoj (void);
//atic void          bye_bye (const char *str);
//atic void          error_exit (const char *str);
//atic void          sig_abexit (int sig);
//atic void          sig_endpgm (int dont_care_sig);
//atic void          sig_paused (int dont_care_sig);
//atic void          sig_resize (int dont_care_sig);
/*------  Special UTF-8 Multi-Byte support  ------------------------------*/
/*atic char          UTF8_tab[] = { ... }                                 */
//atic inline int    utf8_cols (const unsigned char *p, int n);
//atic int           utf8_delta (const char *str);
//atic int           utf8_embody (const char *str, int width);
//atic const char   *utf8_justify (const char *str, int width, int justr);
//atic int           utf8_proper_col (const char *str, int col, int tophysical);
/*------  Misc Color/Display support  ------------------------------------*/
//atic void          capsmk (WIN_t *q);
//atic void          show_msg (const char *str);
//atic int           show_pmt (const char *str);
//atic void          show_scroll (void);
//atic void          show_special (int interact, const char *glob);
/*------  Low Level Memory/Keyboard/File I/O support  --------------------*/
//atic void         *alloc_c (size_t num);
//atic void         *alloc_r (void *ptr, size_t num);
//atic char         *alloc_s (const char *str);
//atic inline int    ioa (struct timespec *ts);
//atic int           ioch (int ech, char *buf, unsigned cnt);
//atic int           iokey (int action);
//atic char         *ioline (const char *prompt);
//atic int           mkfloat (const char *str, float *num, int whole);
//atic int           readfile (FILE *fp, char **baddr, size_t *bsize, size_t *bread);
/*------  Small Utility routines  ----------------------------------------*/
//atic float         get_float (const char *prompt);
//atic int           get_int (const char *prompt);
//atic inline const char *hex_make (long num, int noz);
//atic const char   *user_certify (WIN_t *q, const char *str, char typ);
/*------  Basic Formatting support  --------------------------------------*/
//atic inline const char *justify_pad (const char *str, int width, int justr);
//atic inline const char *make_chr (const char ch, int width, int justr);
//atic inline const char *make_num (long num, int width, int justr, int col, int noz);
//atic inline const char *make_str (const char *str, int width, int justr, int col);
//atic inline const char *make_str_utf8 (const char *str, int width, int justr, int col);
//atic const char   *scale_mem (int target, float num, int width, int justr);
//atic const char   *scale_num (float num, int width, int justr);
//atic const char   *scale_pcnt (float num, int width, int justr, int xtra);
//atic const char   *scale_tics (TIC_t tics, int width, int justr, int target);
/*------  Fields Management support  -------------------------------------*/
/*atic struct        Fieldstab[] = { ... }                                */
//atic void          adj_geometry (void);
//atic void          build_headers (void);
//atic void          calibrate_fields (void);
//atic void          display_fields (int focus, int extend);
//atic void          fields_utility (void);
//atic inline void   widths_resize (void);
//atic void          zap_fieldstab (void);
/*------  Library Interface (as separate threads)  -----------------------*/
//atic void         *cpus_refresh (void *unused);
//atic void         *memory_refresh (void *unused);
//atic void         *tasks_refresh (void *unused);
/*------  Inspect Other Output  ------------------------------------------*/
//atic void          insp_cnt_nl (void);
#ifndef INSP_OFFDEMO
//atic void          insp_do_demo (char *fmts, int pid);
#endif
//atic void          insp_do_file (char *fmts, int pid);
//atic void          insp_do_pipe (char *fmts, int pid);
//atic inline int    insp_find_ofs (int col, int row);
//atic void          insp_find_str (int ch, int *col, int *row);
//atic void          insp_mkrow_raw (int col, int row);
//atic void          insp_mkrow_utf8 (int col, int row);
//atic void          insp_show_pgs (int col, int row, int max);
//atic int           insp_view_choice (struct pids_stack *p);
//atic void          inspection_utility (int pid);
/*------  Other Filtering ------------------------------------------------*/
//atic const char   *osel_add (WIN_t *q, int ch, char *glob, int push);
//atic void          osel_clear (WIN_t *q);
//atic inline int    osel_matched (const WIN_t *q, FLG_t enu, const char *str);
/*------  Startup routines  ----------------------------------------------*/
//atic void          before (char *me);
//atic int           cfg_xform (WIN_t *q, char *flds, const char *defs);
//atic int           config_insp (FILE *fp, char *buf, size_t size);
//atic int           config_osel (FILE *fp, char *buf, size_t size);
//atic const char   *configs_file (FILE *fp, const char *name, float *delay);
//atic int           configs_path (const char *const fmts, ...);
//atic void          configs_reads (void);
//atic void          parse_args (int argc, char **argv);
//atic void          signals_set (void);
//atic void          whack_terminal (void);
/*------  Windows/Field Groups support  ----------------------------------*/
//atic void          win_names (WIN_t *q, const char *name);
//atic void          win_reset (WIN_t *q);
//atic WIN_t        *win_select (int ch);
//atic int           win_warn (int what);
//atic void          wins_clrhlp (WIN_t *q, int save);
//atic void          wins_colors (void);
//atic void          wins_reflag (int what, int flg);
//atic void          wins_stage_1 (void);
//atic void          wins_stage_2 (void);
//atic inline int    wins_usrselect (const WIN_t *q, int idx);
/*------  Forest View support  -------------------------------------------*/
//atic void          forest_adds (const int self, int level);
//atic void          forest_begin (WIN_t *q);
//atic void          forest_config (WIN_t *q);
//atic inline const char *forest_display (const WIN_t *q, int idx);
/*------  Special Separate Bottom Window support  ------------------------*/
//atic void          bot_do (const char *str, int focus);
//atic int           bot_focus_str (const char *hdr, const char *str);
//atic int           bot_focus_strv (const char *hdr, const char **strv);
//atic void         *bot_item_hlp (struct pids_stack *p);
//atic void          bot_item_show (void);
//atic void          bot_item_toggle (int what, const char *head, char sep);
/*------  Interactive Input Tertiary support  ----------------------------*/
//atic inline int    find_ofs (const WIN_t *q, const char *buf);
//atic void          find_string (int ch);
//atic void          help_view (void);
//atic void          other_filters (int ch);
//atic void          write_rcfile (void);
/*------  Interactive Input Secondary support (do_key helpers)  ----------*/
//atic void          keys_global (int ch);
//atic void          keys_summary (int ch);
//atic void          keys_task (int ch);
//atic void          keys_window (int ch);
//atic void          keys_xtra (int ch);
/*------  Tertiary summary display support (summary_show helpers)  -------*/
//atic struct rx_st *sum_rx (struct graph_parms *these);
//atic inline int    sum_see (const char *str, int nobuf);
//atic int           sum_tics (struct stat_stack *this, const char *pfx, int nobuf);
//atic int           sum_unify (struct stat_stack *this, int nobuf);
/*------  Secondary summary display support (summary_show helpers)  ------*/
//atic void          do_cpus (void);
//atic void          do_memory (void);
/*------  Main Screen routines  ------------------------------------------*/
//atic void          do_key (int ch);
//atic void          summary_show (void);
//atic const char   *task_show (const WIN_t *q, int idx);
//atic void          window_hlp (void);
//atic int           window_show (WIN_t *q, int wmax);
/*------  Entry point plus two  ------------------------------------------*/
//atic void          frame_hlp (int wix, int max);
//atic void          frame_make (void);
//     int           main (int argc, char *argv[]);

#endif /* _Itop */

