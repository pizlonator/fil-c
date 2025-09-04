/* vi: set sw=4 ts=4: */
/*
 * tiny vi.c: A small 'vi' clone
 * Copyright (C) 2000, 2001 Sterling Huxley <sterling@europa.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//
//Things To Do:
//	./.exrc
//	add magic to search	/foo.*bar
//	add :help command
//	:map macros
//	if mark[] values were line numbers rather than pointers
//	it would be easier to change the mark when add/delete lines
//	More intelligence in refresh()
//	":r !cmd"  and  "!cmd"  to filter text through an external command
//	An "ex" line oriented mode- maybe using "cmdedit"

//config:config VI
//config:	bool "vi (26 kb)"
//config:	default y
//config:	help
//config:	'vi' is a text editor. More specifically, it is the One True
//config:	text editor <grin>. It does, however, have a rather steep
//config:	learning curve. If you are not already comfortable with 'vi'
//config:	you may wish to use something else.
//config:
//config:config FEATURE_VI_MAX_LEN
//config:	int "Maximum screen width"
//config:	range 256 16384
//config:	default 4096
//config:	depends on VI
//config:	help
//config:	Contrary to what you may think, this is not eating much.
//config:	Make it smaller than 4k only if you are very limited on memory.
//config:
//config:config FEATURE_VI_8BIT
//config:	bool "Allow to display 8-bit chars (otherwise shows dots)"
//config:	default n
//config:	depends on VI
//config:	help
//config:	If your terminal can display characters with high bit set,
//config:	you may want to enable this. Note: vi is not Unicode-capable.
//config:	If your terminal combines several 8-bit bytes into one character
//config:	(as in Unicode mode), this will not work properly.
//config:
//config:config FEATURE_VI_COLON
//config:	bool "Enable \":\" colon commands (no \"ex\" mode)"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Enable a limited set of colon commands. This does not
//config:	provide an "ex" mode.
//config:
//config:config FEATURE_VI_COLON_EXPAND
//config:	bool "Expand \"%\" and \"#\" in colon commands"
//config:	default y
//config:	depends on FEATURE_VI_COLON
//config:	help
//config:	Expand the special characters \"%\" (current filename)
//config:	and \"#\" (alternate filename) in colon commands.
//config:
//config:config FEATURE_VI_YANKMARK
//config:	bool "Enable yank/put commands and mark cmds"
//config:	default y
//config:	depends on VI
//config:	help
//config:	This enables you to use yank and put, as well as mark.
//config:
//config:config FEATURE_VI_SEARCH
//config:	bool "Enable search and replace cmds"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Select this if you wish to be able to do search and replace.
//config:
//config:config FEATURE_VI_REGEX_SEARCH
//config:	bool "Enable regex in search and replace"
//config:	default n   # Uses GNU regex, which may be unavailable. FIXME
//config:	depends on FEATURE_VI_SEARCH
//config:	help
//config:	Use extended regex search.
//config:
//config:config FEATURE_VI_USE_SIGNALS
//config:	bool "Catch signals"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Selecting this option will make vi signal aware. This will support
//config:	SIGWINCH to deal with Window Changes, catch ^Z and ^C and alarms.
//config:
//config:config FEATURE_VI_DOT_CMD
//config:	bool "Remember previous cmd and \".\" cmd"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Make vi remember the last command and be able to repeat it.
//config:
//config:config FEATURE_VI_READONLY
//config:	bool "Enable -R option and \"view\" mode"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Enable the read-only command line option, which allows the user to
//config:	open a file in read-only mode.
//config:
//config:config FEATURE_VI_SETOPTS
//config:	bool "Enable settable options, ai ic showmatch"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Enable the editor to set some (ai, ic, showmatch) options.
//config:
//config:config FEATURE_VI_SET
//config:	bool "Support :set"
//config:	default y
//config:	depends on VI
//config:
//config:config FEATURE_VI_WIN_RESIZE
//config:	bool "Handle window resize"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Behave nicely with terminals that get resized.
//config:
//config:config FEATURE_VI_ASK_TERMINAL
//config:	bool "Use 'tell me cursor position' ESC sequence to measure window"
//config:	default y
//config:	depends on VI
//config:	help
//config:	If terminal size can't be retrieved and $LINES/$COLUMNS are not set,
//config:	this option makes vi perform a last-ditch effort to find it:
//config:	position cursor to 999,999 and ask terminal to report real
//config:	cursor position using "ESC [ 6 n" escape sequence, then read stdin.
//config:	This is not clean but helps a lot on serial lines and such.
//config:
//config:config FEATURE_VI_UNDO
//config:	bool "Support undo command \"u\""
//config:	default y
//config:	depends on VI
//config:	help
//config:	Support the 'u' command to undo insertion, deletion, and replacement
//config:	of text.
//config:
//config:config FEATURE_VI_UNDO_QUEUE
//config:	bool "Enable undo operation queuing"
//config:	default y
//config:	depends on FEATURE_VI_UNDO
//config:	help
//config:	The vi undo functions can use an intermediate queue to greatly lower
//config:	malloc() calls and overhead. When the maximum size of this queue is
//config:	reached, the contents of the queue are committed to the undo stack.
//config:	This increases the size of the undo code and allows some undo
//config:	operations (especially un-typing/backspacing) to be far more useful.
//config:
//config:config FEATURE_VI_UNDO_QUEUE_MAX
//config:	int "Maximum undo character queue size"
//config:	default 256
//config:	range 32 65536
//config:	depends on FEATURE_VI_UNDO_QUEUE
//config:	help
//config:	This option sets the number of bytes used at runtime for the queue.
//config:	Smaller values will create more undo objects and reduce the amount
//config:	of typed or backspaced characters that are grouped into one undo
//config:	operation; larger values increase the potential size of each undo
//config:	and will generally malloc() larger objects and less frequently.
//config:	Unless you want more (or less) frequent "undo points" while typing,
//config:	you should probably leave this unchanged.
//config:
//config:config FEATURE_VI_VERBOSE_STATUS
//config:	bool "Enable verbose status reporting"
//config:	default y
//config:	depends on VI
//config:	help
//config:	Enable more verbose reporting of the results of yank, change,
//config:	delete, undo and substitution commands.

//applet:IF_VI(APPLET(vi, BB_DIR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_VI) += vi.o

//usage:#define vi_trivial_usage
//usage:       IF_FEATURE_VI_COLON("[-c CMD] ")IF_FEATURE_VI_READONLY("[-R] ")"[-H] [FILE]..."
//usage:#define vi_full_usage "\n\n"
//usage:       "Edit FILE\n"
//usage:	IF_FEATURE_VI_COLON(
//usage:     "\n	-c CMD	Initial command to run ($EXINIT and ~/.exrc also available)"
//usage:	)
//usage:	IF_FEATURE_VI_READONLY(
//usage:     "\n	-R	Read-only"
//usage:	)
//usage:     "\n	-H	List available features"
// note: non-standard, "vim -H" is Hebrew mode (bidi support)

#include "libbb.h"
// Should be after libbb.h: on some systems regex.h needs sys/types.h:
#if ENABLE_FEATURE_VI_REGEX_SEARCH
# include <regex.h>
#endif

// the CRASHME code is unmaintained, and doesn't currently build
#define ENABLE_FEATURE_VI_CRASHME 0
#define IF_FEATURE_VI_CRASHME(...)


#if ENABLE_LOCALE_SUPPORT

#if ENABLE_FEATURE_VI_8BIT
//FIXME: this does not work properly for Unicode anyway
# define Isprint(c) (isprint)(c)
#else
# define Isprint(c) isprint_asciionly(c)
#endif

#else

// 0x9b is Meta-ESC
#if ENABLE_FEATURE_VI_8BIT
# define Isprint(c) ((unsigned char)(c) >= ' ' && (c) != 0x7f && (unsigned char)(c) != 0x9b)
#else
# define Isprint(c) ((unsigned char)(c) >= ' ' && (unsigned char)(c) < 0x7f)
#endif

#endif

#define isbackspace(c) ((c) == term_orig.c_cc[VERASE] || (c) == 8 || (c) == 127)

enum {
	MAX_TABSTOP = 32, // sanity limit
	// User input len. Need not be extra big.
	// Lines in file being edited *can* be bigger than this.
	MAX_INPUT_LEN = 128,
	// Sanity limits. We have only one buffer of this size.
	MAX_SCR_COLS = CONFIG_FEATURE_VI_MAX_LEN,
	MAX_SCR_ROWS = CONFIG_FEATURE_VI_MAX_LEN,
};

// VT102 ESC sequences.
// See "Xterm Control Sequences"
// http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
#define ESC "\033"
// Inverse/Normal text
#define ESC_BOLD_TEXT ESC"[7m"
#define ESC_NORM_TEXT ESC"[m"
// Bell
#define ESC_BELL "\007"
// Clear-to-end-of-line
#define ESC_CLEAR2EOL ESC"[K"
// Clear-to-end-of-screen.
// (We use default param here.
// Full sequence is "ESC [ <num> J",
// <num> is 0/1/2 = "erase below/above/all".)
#define ESC_CLEAR2EOS          ESC"[J"
// Cursor to given coordinate (1,1: top left)
#define ESC_SET_CURSOR_POS     ESC"[%u;%uH"
#define ESC_SET_CURSOR_TOPLEFT ESC"[H"
//UNUSED
//// Cursor up and down
//#define ESC_CURSOR_UP   ESC"[A"
//#define ESC_CURSOR_DOWN "\n"

#if ENABLE_FEATURE_VI_DOT_CMD
// cmds modifying text[]
static const char modifying_cmds[] ALIGN1 = "aAcCdDiIJoOpPrRs""xX<>~";
#endif

enum {
	YANKONLY = FALSE,
	YANKDEL = TRUE,
	FORWARD = 1,	// code depends on "1"  for array index
	BACK = -1,	// code depends on "-1" for array index
	LIMITED = 0,	// char_search() only current line
	FULL = 1,	// char_search() to the end/beginning of entire text
	PARTIAL = 0,	// buffer contains partial line
	WHOLE = 1,	// buffer contains whole lines
	MULTI = 2,	// buffer may include newlines

	S_BEFORE_WS = 1,	// used in skip_thing() for moving "dot"
	S_TO_WS = 2,		// used in skip_thing() for moving "dot"
	S_OVER_WS = 3,		// used in skip_thing() for moving "dot"
	S_END_PUNCT = 4,	// used in skip_thing() for moving "dot"
	S_END_ALNUM = 5,	// used in skip_thing() for moving "dot"

	C_END = -1,	// cursor is at end of line due to '$' command
};


// vi.c expects chars to be unsigned.
// busybox build system provides that, but it's better
// to audit and fix the source

struct globals {
	// many references - keep near the top of globals
	char *text, *end;       // pointers to the user data in memory
	char *dot;              // where all the action takes place
	int text_size;		// size of the allocated buffer

	// the rest
#if ENABLE_FEATURE_VI_SETOPTS
	smallint vi_setops;     // set by setops()
#define VI_AUTOINDENT (1 << 0)
#define VI_EXPANDTAB  (1 << 1)
#define VI_ERR_METHOD (1 << 2)
#define VI_IGNORECASE (1 << 3)
#define VI_SHOWMATCH  (1 << 4)
#define VI_TABSTOP    (1 << 5)
#define autoindent (vi_setops & VI_AUTOINDENT)
#define expandtab  (vi_setops & VI_EXPANDTAB )
#define err_method (vi_setops & VI_ERR_METHOD) // indicate error with beep or flash
#define ignorecase (vi_setops & VI_IGNORECASE)
#define showmatch  (vi_setops & VI_SHOWMATCH )
// order of constants and strings must match
#define OPTS_STR \
		"ai\0""autoindent\0" \
		"et\0""expandtab\0" \
		"fl\0""flash\0" \
		"ic\0""ignorecase\0" \
		"sm\0""showmatch\0" \
		"ts\0""tabstop\0"
#else
#define autoindent (0)
#define expandtab  (0)
#define err_method (0)
#define ignorecase (0)
#endif

#if ENABLE_FEATURE_VI_READONLY
	smallint readonly_mode;
#define SET_READONLY_FILE(flags)        ((flags) |= 0x01)
#define SET_READONLY_MODE(flags)        ((flags) |= 0x02)
#define UNSET_READONLY_FILE(flags)      ((flags) &= 0xfe)
#else
#define SET_READONLY_FILE(flags)        ((void)0)
#define SET_READONLY_MODE(flags)        ((void)0)
#define UNSET_READONLY_FILE(flags)      ((void)0)
#endif

	smallint editing;        // >0 while we are editing a file
	                         // [code audit says "can be 0, 1 or 2 only"]
	smallint cmd_mode;       // 0=command  1=insert 2=replace
	int modified_count;      // buffer contents changed if !0
	int last_modified_count; // = -1;
	int cmdline_filecnt;     // how many file names on cmd line
	int cmdcnt;              // repetition count
	char *rstart;            // start of text in Replace mode
	unsigned rows, columns;	 // the terminal screen is this size
#if ENABLE_FEATURE_VI_ASK_TERMINAL
	int get_rowcol_error;
#endif
	int crow, ccol;          // cursor is on Crow x Ccol
	int offset;              // chars scrolled off the screen to the left
	int have_status_msg;     // is default edit status needed?
	                         // [don't make smallint!]
	int last_status_cksum;   // hash of current status line
	char *current_filename;
#if ENABLE_FEATURE_VI_COLON_EXPAND
	char *alt_filename;
#endif
	char *screenbegin;       // index into text[], of top line on the screen
	char *screen;            // pointer to the virtual screen buffer
	int screensize;          //            and its size
	int tabstop;
	int last_search_char;    // last char searched for (int because of Unicode)
	smallint last_search_cmd;    // command used to invoke last char search
#if ENABLE_FEATURE_VI_CRASHME
	char last_input_char;    // last char read from user
#endif
#if ENABLE_FEATURE_VI_UNDO_QUEUE
	char undo_queue_state;   // One of UNDO_INS, UNDO_DEL, UNDO_EMPTY
#endif

#if ENABLE_FEATURE_VI_DOT_CMD
	smallint adding2q;	 // are we currently adding user input to q
	int lmc_len;             // length of last_modifying_cmd
	char *ioq, *ioq_start;   // pointer to string for get_one_char to "read"
	int dotcnt;              // number of times to repeat '.' command
#endif
#if ENABLE_FEATURE_VI_SEARCH
	char *last_search_pattern; // last pattern from a '/' or '?' search
#endif
#if ENABLE_FEATURE_VI_SETOPTS
	int char_insert__indentcol;		// column of recent autoindent or 0
	int newindent;		// autoindent value for 'O'/'cc' commands
						// or -1 to use indent from previous line
#endif
	smallint cmd_error;

	// former statics
#if ENABLE_FEATURE_VI_YANKMARK
	char *edit_file__cur_line;
#endif
	int refresh__old_offset;
	int format_edit_status__tot;

	// a few references only
#if ENABLE_FEATURE_VI_YANKMARK
	smalluint YDreg;//,Ureg;// default delete register and orig line for "U"
#define Ureg 27
	char *reg[28];          // named register a-z, "D", and "U" 0-25,26,27
	char regtype[28];       // buffer type: WHOLE, MULTI or PARTIAL
	char *mark[28];         // user marks points somewhere in text[]-  a-z and previous context ''
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
	sigjmp_buf restart;     // int_handler() jumps to location remembered here
#endif
	struct termios term_orig; // remember what the cooked mode was
	int cindex;               // saved character index for up/down motion
	smallint keep_index;      // retain saved character index
#if ENABLE_FEATURE_VI_COLON
	llist_t *initial_cmds;
#endif
	// Should be just enough to hold a key sequence,
	// but CRASHME mode uses it as generated command buffer too
#if ENABLE_FEATURE_VI_CRASHME
	char readbuffer[128];
#else
	char readbuffer[KEYCODE_BUFFER_SIZE];
#endif
#define STATUS_BUFFER_LEN  200
	char status_buffer[STATUS_BUFFER_LEN]; // messages to the user
#if ENABLE_FEATURE_VI_DOT_CMD
	char last_modifying_cmd[MAX_INPUT_LEN];	// last modifying cmd for "."
#endif
	char get_input_line__buf[MAX_INPUT_LEN]; // former static

	char scr_out_buf[MAX_SCR_COLS + MAX_TABSTOP * 2];

#if ENABLE_FEATURE_VI_UNDO
// undo_push() operations
#define UNDO_INS         0
#define UNDO_DEL         1
#define UNDO_INS_CHAIN   2
#define UNDO_DEL_CHAIN   3
# if ENABLE_FEATURE_VI_UNDO_QUEUE
#define UNDO_INS_QUEUED  4
#define UNDO_DEL_QUEUED  5
# endif

// Pass-through flags for functions that can be undone
#define NO_UNDO          0
#define ALLOW_UNDO       1
#define ALLOW_UNDO_CHAIN 2
# if ENABLE_FEATURE_VI_UNDO_QUEUE
#define ALLOW_UNDO_QUEUED 3
# else
// If undo queuing disabled, don't invoke the missing queue logic
#define ALLOW_UNDO_QUEUED ALLOW_UNDO
# endif

	struct undo_object {
		struct undo_object *prev;	// Linking back avoids list traversal (LIFO)
		int start;		// Offset where the data should be restored/deleted
		int length;		// total data size
		uint8_t u_type;		// 0=deleted, 1=inserted, 2=swapped
		char undo_text[1];	// text that was deleted (if deletion)
	} *undo_stack_tail;
# if ENABLE_FEATURE_VI_UNDO_QUEUE
#define UNDO_USE_SPOS   32
#define UNDO_EMPTY      64
	char *undo_queue_spos;	// Start position of queued operation
	int undo_q;
	char undo_queue[CONFIG_FEATURE_VI_UNDO_QUEUE_MAX];
# endif
#endif /* ENABLE_FEATURE_VI_UNDO */
};
#define G (*ptr_to_globals)
#define text           (G.text          )
#define text_size      (G.text_size     )
#define end            (G.end           )
#define dot            (G.dot           )
#define reg            (G.reg           )

#define vi_setops               (G.vi_setops          )
#define editing                 (G.editing            )
#define cmd_mode                (G.cmd_mode           )
#define modified_count          (G.modified_count     )
#define last_modified_count     (G.last_modified_count)
#define cmdline_filecnt         (G.cmdline_filecnt    )
#define cmdcnt                  (G.cmdcnt             )
#define rstart                  (G.rstart             )
#define rows                    (G.rows               )
#define columns                 (G.columns            )
#define crow                    (G.crow               )
#define ccol                    (G.ccol               )
#define offset                  (G.offset             )
#define status_buffer           (G.status_buffer      )
#define have_status_msg         (G.have_status_msg    )
#define last_status_cksum       (G.last_status_cksum  )
#define current_filename        (G.current_filename   )
#define alt_filename            (G.alt_filename       )
#define screen                  (G.screen             )
#define screensize              (G.screensize         )
#define screenbegin             (G.screenbegin        )
#define tabstop                 (G.tabstop            )
#define last_search_char        (G.last_search_char   )
#define last_search_cmd         (G.last_search_cmd    )
#if ENABLE_FEATURE_VI_CRASHME
#define last_input_char         (G.last_input_char    )
#endif
#if ENABLE_FEATURE_VI_READONLY
#define readonly_mode           (G.readonly_mode      )
#else
#define readonly_mode           0
#endif
#define adding2q                (G.adding2q           )
#define lmc_len                 (G.lmc_len            )
#define ioq                     (G.ioq                )
#define ioq_start               (G.ioq_start          )
#define dotcnt                  (G.dotcnt             )
#define last_search_pattern     (G.last_search_pattern)
#define char_insert__indentcol  (G.char_insert__indentcol)
#define newindent               (G.newindent          )
#define cmd_error               (G.cmd_error          )

#define edit_file__cur_line     (G.edit_file__cur_line)
#define refresh__old_offset     (G.refresh__old_offset)
#define format_edit_status__tot (G.format_edit_status__tot)

#define YDreg          (G.YDreg         )
//#define Ureg           (G.Ureg          )
#define regtype        (G.regtype       )
#define mark           (G.mark          )
#define restart        (G.restart       )
#define term_orig      (G.term_orig     )
#define cindex         (G.cindex        )
#define keep_index     (G.keep_index    )
#define initial_cmds   (G.initial_cmds  )
#define readbuffer     (G.readbuffer    )
#define scr_out_buf    (G.scr_out_buf   )
#define last_modifying_cmd  (G.last_modifying_cmd )
#define get_input_line__buf (G.get_input_line__buf)

#if ENABLE_FEATURE_VI_UNDO
#define undo_stack_tail  (G.undo_stack_tail )
# if ENABLE_FEATURE_VI_UNDO_QUEUE
#define undo_queue_state (G.undo_queue_state)
#define undo_q           (G.undo_q          )
#define undo_queue       (G.undo_queue      )
#define undo_queue_spos  (G.undo_queue_spos )
# endif
#endif

#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	last_modified_count--; \
	/* "" but has space for 2 chars: */ \
	IF_FEATURE_VI_SEARCH(last_search_pattern = xzalloc(2);) \
	tabstop = 8; \
	IF_FEATURE_VI_SETOPTS(newindent--;) \
} while (0)

#if ENABLE_FEATURE_VI_CRASHME
static int crashme = 0;
#endif

static void show_status_line(void);	// put a message on the bottom line
static void status_line_bold(const char *, ...);

static void show_help(void)
{
	puts("These features are available:"
#if ENABLE_FEATURE_VI_SEARCH
	"\n\tPattern searches with / and ?"
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
	"\n\tLast command repeat with ."
#endif
#if ENABLE_FEATURE_VI_YANKMARK
	"\n\tLine marking with 'x"
	"\n\tNamed buffers with \"x"
#endif
#if ENABLE_FEATURE_VI_READONLY
	//not implemented: "\n\tReadonly if vi is called as \"view\""
	//redundant: usage text says this too: "\n\tReadonly with -R command line arg"
#endif
#if ENABLE_FEATURE_VI_SET
	"\n\tSome colon mode commands with :"
#endif
#if ENABLE_FEATURE_VI_SETOPTS
	"\n\tSettable options with \":set\""
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
	"\n\tSignal catching- ^C"
	"\n\tJob suspend and resume with ^Z"
#endif
#if ENABLE_FEATURE_VI_WIN_RESIZE
	"\n\tAdapt to window re-sizes"
#endif
	);
}

static void write1(const char *out)
{
	fputs_stdout(out);
}

#if ENABLE_FEATURE_VI_WIN_RESIZE
static int query_screen_dimensions(void)
{
	int err = get_terminal_width_height(STDIN_FILENO, &columns, &rows);
	if (rows > MAX_SCR_ROWS)
		rows = MAX_SCR_ROWS;
	if (columns > MAX_SCR_COLS)
		columns = MAX_SCR_COLS;
	return err;
}
#else
static ALWAYS_INLINE int query_screen_dimensions(void)
{
	return 0;
}
#endif

// sleep for 'h' 1/100 seconds, return 1/0 if stdin is (ready for read)/(not ready)
static int mysleep(int hund)
{
	struct pollfd pfd[1];

	if (hund != 0)
		fflush_all();

	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;
	return safe_poll(pfd, 1, hund*10) > 0;
}

//----- Set terminal attributes --------------------------------
static void rawmode(void)
{
	// no TERMIOS_CLEAR_ISIG: leave ISIG on - allow signals
	set_termios_to_raw(STDIN_FILENO, &term_orig, TERMIOS_RAW_CRNL);
}

static void cookmode(void)
{
	fflush_all();
	tcsetattr_stdin_TCSANOW(&term_orig);
}

//----- Terminal Drawing ---------------------------------------
// The terminal is made up of 'rows' line of 'columns' columns.
// classically this would be 24 x 80.
//  screen coordinates
//  0,0     ...     0,79
//  1,0     ...     1,79
//  .       ...     .
//  .       ...     .
//  22,0    ...     22,79
//  23,0    ...     23,79   <- status line

//----- Move the cursor to row x col (count from 0, not 1) -------
static void place_cursor(int row, int col)
{
	char cm1[sizeof(ESC_SET_CURSOR_POS) + sizeof(int)*3 * 2];

	if (row < 0) row = 0;
	if (row >= rows) row = rows - 1;
	if (col < 0) col = 0;
	if (col >= columns) col = columns - 1;

	sprintf(cm1, ESC_SET_CURSOR_POS, row + 1, col + 1);
	write1(cm1);
}

//----- Erase from cursor to end of line -----------------------
static void clear_to_eol(void)
{
	write1(ESC_CLEAR2EOL);
}

static void go_bottom_and_clear_to_eol(void)
{
	place_cursor(rows - 1, 0);
	clear_to_eol();
}

//----- Start standout mode ------------------------------------
static void standout_start(void)
{
	write1(ESC_BOLD_TEXT);
}

//----- End standout mode --------------------------------------
static void standout_end(void)
{
	write1(ESC_NORM_TEXT);
}

//----- Text Movement Routines ---------------------------------
static char *begin_line(char *p) // return pointer to first char cur line
{
	if (p > text) {
		p = memrchr(text, '\n', p - text);
		if (!p)
			return text;
		return p + 1;
	}
	return p;
}

static char *end_line(char *p) // return pointer to NL of cur line
{
	if (p < end - 1) {
		p = memchr(p, '\n', end - p - 1);
		if (!p)
			return end - 1;
	}
	return p;
}

static char *dollar_line(char *p) // return pointer to just before NL line
{
	p = end_line(p);
	// Try to stay off of the Newline
	if (*p == '\n' && (p - begin_line(p)) > 0)
		p--;
	return p;
}

static char *prev_line(char *p) // return pointer first char prev line
{
	p = begin_line(p);	// goto beginning of cur line
	if (p > text && p[-1] == '\n')
		p--;			// step to prev line
	p = begin_line(p);	// goto beginning of prev line
	return p;
}

static char *next_line(char *p) // return pointer first char next line
{
	p = end_line(p);
	if (p < end - 1 && *p == '\n')
		p++;			// step to next line
	return p;
}

//----- Text Information Routines ------------------------------
static char *end_screen(void)
{
	char *q;
	int cnt;

	// find new bottom line
	q = screenbegin;
	for (cnt = 0; cnt < rows - 2; cnt++)
		q = next_line(q);
	q = end_line(q);
	return q;
}

// count line from start to stop
static int count_lines(char *start, char *stop)
{
	char *q;
	int cnt;

	if (stop < start) { // start and stop are backwards- reverse them
		q = start;
		start = stop;
		stop = q;
	}
	cnt = 0;
	stop = end_line(stop);
	while (start <= stop && start <= end - 1) {
		start = end_line(start);
		if (*start == '\n')
			cnt++;
		start++;
	}
	return cnt;
}

static char *find_line(int li)	// find beginning of line #li
{
	char *q;

	for (q = text; li > 1; li--) {
		q = next_line(q);
	}
	return q;
}

static int next_tabstop(int col)
{
	return col + ((tabstop - 1) - (col % tabstop));
}

static int prev_tabstop(int col)
{
	return col - ((col % tabstop) ?: tabstop);
}

static int next_column(char c, int co)
{
	if (c == '\t')
		co = next_tabstop(co);
	else if ((unsigned char)c < ' ' || c == 0x7f)
		co++; // display as ^X, use 2 columns
	return co + 1;
}

static int get_column(char *p)
{
	const char *r;
	int co = 0;

	for (r = begin_line(p); r < p; r++)
		co = next_column(*r, co);
	return co;
}

//----- Erase the Screen[] memory ------------------------------
static void screen_erase(void)
{
	memset(screen, ' ', screensize);	// clear new screen
}

static void new_screen(int ro, int co)
{
	char *s;

	free(screen);
	screensize = ro * co + 8;
	s = screen = xmalloc(screensize);
	// initialize the new screen. assume this will be a empty file.
	screen_erase();
	// non-existent text[] lines start with a tilde (~).
	//screen[(1 * co) + 0] = '~';
	//screen[(2 * co) + 0] = '~';
	//..
	//screen[((ro-2) * co) + 0] = '~';
	ro -= 2;
	while (--ro >= 0) {
		s += co;
		*s = '~';
	}
}

//----- Synchronize the cursor to Dot --------------------------
static void sync_cursor(char *d, int *row, int *col)
{
	char *beg_cur;	// begin and end of "d" line
	char *tp;
	int cnt, ro, co;

	beg_cur = begin_line(d);	// first char of cur line

	if (beg_cur < screenbegin) {
		// "d" is before top line on screen
		// how many lines do we have to move
		cnt = count_lines(beg_cur, screenbegin);
 sc1:
		screenbegin = beg_cur;
		if (cnt > (rows - 1) / 2) {
			// we moved too many lines. put "dot" in middle of screen
			for (cnt = 0; cnt < (rows - 1) / 2; cnt++) {
				screenbegin = prev_line(screenbegin);
			}
		}
	} else {
		char *end_scr;	// begin and end of screen
		end_scr = end_screen();	// last char of screen
		if (beg_cur > end_scr) {
			// "d" is after bottom line on screen
			// how many lines do we have to move
			cnt = count_lines(end_scr, beg_cur);
			if (cnt > (rows - 1) / 2)
				goto sc1;	// too many lines
			for (ro = 0; ro < cnt - 1; ro++) {
				// move screen begin the same amount
				screenbegin = next_line(screenbegin);
				// now, move the end of screen
				end_scr = next_line(end_scr);
				end_scr = end_line(end_scr);
			}
		}
	}
	// "d" is on screen- find out which row
	tp = screenbegin;
	for (ro = 0; ro < rows - 1; ro++) {	// drive "ro" to correct row
		if (tp == beg_cur)
			break;
		tp = next_line(tp);
	}

	// find out what col "d" is on
	co = 0;
	do { // drive "co" to correct column
		if (*tp == '\n') //vda || *tp == '\0')
			break;
		co = next_column(*tp, co) - 1;
		// inserting text before a tab, don't include its position
		if (cmd_mode && tp == d - 1 && *d == '\t') {
			co++;
			break;
		}
	} while (tp++ < d && ++co);

	// "co" is the column where "dot" is.
	// The screen has "columns" columns.
	// The currently displayed columns are  0+offset -- columns+ofset
	// |-------------------------------------------------------------|
	//               ^ ^                                ^
	//        offset | |------- columns ----------------|
	//
	// If "co" is already in this range then we do not have to adjust offset
	//      but, we do have to subtract the "offset" bias from "co".
	// If "co" is outside this range then we have to change "offset".
	// If the first char of a line is a tab the cursor will try to stay
	//  in column 7, but we have to set offset to 0.

	if (co < 0 + offset) {
		offset = co;
	}
	if (co >= columns + offset) {
		offset = co - columns + 1;
	}
	// if the first char of the line is a tab, and "dot" is sitting on it
	//  force offset to 0.
	if (d == beg_cur && *d == '\t') {
		offset = 0;
	}
	co -= offset;

	*row = ro;
	*col = co;
}

//----- Format a text[] line into a buffer ---------------------
static char* format_line(char *src /*, int li*/)
{
	unsigned char c;
	int co;
	int ofs = offset;
	char *dest = scr_out_buf; // [MAX_SCR_COLS + MAX_TABSTOP * 2]

	c = '~'; // char in col 0 in non-existent lines is '~'
	co = 0;
	while (co < columns + tabstop) {
		// have we gone past the end?
		if (src < end) {
			c = *src++;
			if (c == '\n')
				break;
			if ((c & 0x80) && !Isprint(c)) {
				c = '.';
			}
			if (c < ' ' || c == 0x7f) {
				if (c == '\t') {
					c = ' ';
					//      co %    8     !=     7
					while ((co % tabstop) != (tabstop - 1)) {
						dest[co++] = c;
					}
				} else {
					dest[co++] = '^';
					if (c == 0x7f)
						c = '?';
					else
						c += '@'; // Ctrl-X -> 'X'
				}
			}
		}
		dest[co++] = c;
		// discard scrolled-off-to-the-left portion,
		// in tabstop-sized pieces
		if (ofs >= tabstop && co >= tabstop) {
			memmove(dest, dest + tabstop, co);
			co -= tabstop;
			ofs -= tabstop;
		}
		if (src >= end)
			break;
	}
	// check "short line, gigantic offset" case
	if (co < ofs)
		ofs = co;
	// discard last scrolled off part
	co -= ofs;
	dest += ofs;
	// fill the rest with spaces
	if (co < columns)
		memset(&dest[co], ' ', columns - co);
	return dest;
}

//----- Refresh the changed screen lines -----------------------
// Copy the source line from text[] into the buffer and note
// if the current screenline is different from the new buffer.
// If they differ then that line needs redrawing on the terminal.
//
static void refresh(int full_screen)
{
#define old_offset refresh__old_offset

	int li, changed;
	char *tp, *sp;		// pointer into text[] and screen[]

	if (ENABLE_FEATURE_VI_WIN_RESIZE IF_FEATURE_VI_ASK_TERMINAL(&& !G.get_rowcol_error) ) {
		unsigned c = columns, r = rows;
		query_screen_dimensions();
#if ENABLE_FEATURE_VI_USE_SIGNALS
		full_screen |= (c - columns) | (r - rows);
#else
		if (c != columns || r != rows) {
			full_screen = TRUE;
			// update screen memory since SIGWINCH won't have done it
			new_screen(rows, columns);
		}
#endif
	}
	sync_cursor(dot, &crow, &ccol);	// where cursor will be (on "dot")
	tp = screenbegin;	// index into text[] of top line

	// compare text[] to screen[] and mark screen[] lines that need updating
	for (li = 0; li < rows - 1; li++) {
		int cs, ce;				// column start & end
		char *out_buf;
		// format current text line
		out_buf = format_line(tp /*, li*/);

		// skip to the end of the current text[] line
		if (tp < end) {
			char *t = memchr(tp, '\n', end - tp);
			if (!t) t = end - 1;
			tp = t + 1;
		}

		// see if there are any changes between virtual screen and out_buf
		changed = FALSE;	// assume no change
		cs = 0;
		ce = columns - 1;
		sp = &screen[li * columns];	// start of screen line
		if (full_screen) {
			// force re-draw of every single column from 0 - columns-1
			goto re0;
		}
		// compare newly formatted buffer with virtual screen
		// look forward for first difference between buf and screen
		for (; cs <= ce; cs++) {
			if (out_buf[cs] != sp[cs]) {
				changed = TRUE;	// mark for redraw
				break;
			}
		}

		// look backward for last difference between out_buf and screen
		for (; ce >= cs; ce--) {
			if (out_buf[ce] != sp[ce]) {
				changed = TRUE;	// mark for redraw
				break;
			}
		}
		// now, cs is index of first diff, and ce is index of last diff

		// if horz offset has changed, force a redraw
		if (offset != old_offset) {
 re0:
			changed = TRUE;
		}

		// make a sanity check of columns indexes
		if (cs < 0) cs = 0;
		if (ce > columns - 1) ce = columns - 1;
		if (cs > ce) { cs = 0; ce = columns - 1; }
		// is there a change between virtual screen and out_buf
		if (changed) {
			// copy changed part of buffer to virtual screen
			memcpy(sp+cs, out_buf+cs, ce-cs+1);
			place_cursor(li, cs);
			// write line out to terminal
			fwrite(&sp[cs], ce - cs + 1, 1, stdout);
		}
	}

	place_cursor(crow, ccol);

	if (!keep_index)
		cindex = ccol + offset;

	old_offset = offset;
#undef old_offset
}

//----- Force refresh of all Lines -----------------------------
static void redraw(int full_screen)
{
	// cursor to top,left; clear to the end of screen
	write1(ESC_SET_CURSOR_TOPLEFT ESC_CLEAR2EOS);
	screen_erase();		// erase the internal screen buffer
	last_status_cksum = 0;	// force status update
	refresh(full_screen);	// this will redraw the entire display
	show_status_line();
}

//----- Flash the screen  --------------------------------------
static void flash(int h)
{
	standout_start();
	redraw(TRUE);
	mysleep(h);
	standout_end();
	redraw(TRUE);
}

static void indicate_error(void)
{
#if ENABLE_FEATURE_VI_CRASHME
	if (crashme > 0)
		return;
#endif
	cmd_error = TRUE;
	if (!err_method) {
		write1(ESC_BELL);
	} else {
		flash(10);
	}
}

//----- IO Routines --------------------------------------------
static int readit(void) // read (maybe cursor) key from stdin
{
	int c;

	fflush_all();

	// Wait for input. TIMEOUT = -1 makes read_key wait even
	// on nonblocking stdin.
	// Note: read_key sets errno to 0 on success.
 again:
	c = safe_read_key(STDIN_FILENO, readbuffer, /*timeout:*/ -1);
	if (c == -1) { // EOF/error
		if (errno == EAGAIN) // paranoia
			goto again;
		go_bottom_and_clear_to_eol();
		cookmode(); // terminal to "cooked"
		bb_simple_error_msg_and_die("can't read user input");
	}
	return c;
}

#if ENABLE_FEATURE_VI_DOT_CMD
static int get_one_char(void)
{
	int c;

	if (!adding2q) {
		// we are not adding to the q.
		// but, we may be reading from a saved q.
		// (checking "ioq" for NULL is wrong, it's not reset to NULL
		// when done - "ioq_start" is reset instead).
		if (ioq_start != NULL) {
			// there is a queue to get chars from.
			// careful with correct sign expansion!
			c = (unsigned char)*ioq++;
			if (c != '\0')
				return c;
			// the end of the q
			free(ioq_start);
			ioq_start = NULL;
			// read from STDIN:
		}
		return readit();
	}
	// we are adding STDIN chars to q.
	c = readit();
	if (lmc_len >= ARRAY_SIZE(last_modifying_cmd) - 2) {
		// last_modifying_cmd[] is too small, can't remember the cmd
		// - drop it
		adding2q = 0;
		lmc_len = 0;
	} else {
		last_modifying_cmd[lmc_len++] = c;
	}
	return c;
}
#else
# define get_one_char() readit()
#endif

// Get type of thing to operate on and adjust count
static int get_motion_char(void)
{
	int c, cnt;

	c = get_one_char();
	if (isdigit(c)) {
		if (c != '0') {
			// get any non-zero motion count
			for (cnt = 0; isdigit(c); c = get_one_char())
				cnt = cnt * 10 + (c - '0');
			cmdcnt = (cmdcnt ?: 1) * cnt;
		} else {
			// ensure standalone '0' works
			cmdcnt = 0;
		}
	}

	return c;
}

// Get input line (uses "status line" area)
static char *get_input_line(const char *prompt)
{
	// char [MAX_INPUT_LEN]
#define buf get_input_line__buf

	int c;
	int i;

	strcpy(buf, prompt);
	last_status_cksum = 0;	// force status update
	go_bottom_and_clear_to_eol();
	write1(buf);      // write out the :, /, or ? prompt

	i = strlen(buf);
	while (i < MAX_INPUT_LEN - 1) {
		c = get_one_char();
		if (c == '\n' || c == '\r' || c == 27)
			break;		// this is end of input
		if (isbackspace(c)) {
			// user wants to erase prev char
			buf[--i] = '\0';
			go_bottom_and_clear_to_eol();
			if (i <= 0) // user backs up before b-o-l, exit
				break;
			write1(buf);
		} else if (c > 0 && c < 256) { // exclude Unicode
			// (TODO: need to handle Unicode)
			buf[i] = c;
			buf[++i] = '\0';
			bb_putchar(c);
		}
	}
	refresh(FALSE);
	return buf;
#undef buf
}

static void Hit_Return(void)
{
	int c;

	standout_start();
	write1("[Hit return to continue]");
	standout_end();
	while ((c = get_one_char()) != '\n' && c != '\r')
		continue;
	redraw(TRUE);		// force redraw all
}

//----- Draw the status line at bottom of the screen -------------
// show file status on status line
static int format_edit_status(void)
{
	static const char cmd_mode_indicator[] ALIGN1 = "-IR-";

#define tot format_edit_status__tot

	int cur, percent, ret, trunc_at;

	// modified_count is now a counter rather than a flag.  this
	// helps reduce the amount of line counting we need to do.
	// (this will cause a mis-reporting of modified status
	// once every MAXINT editing operations.)

	// it would be nice to do a similar optimization here -- if
	// we haven't done a motion that could have changed which line
	// we're on, then we shouldn't have to do this count_lines()
	cur = count_lines(text, dot);

	// count_lines() is expensive.
	// Call it only if something was changed since last time
	// we were here:
	if (modified_count != last_modified_count) {
		tot = cur + count_lines(dot, end - 1) - 1;
		last_modified_count = modified_count;
	}

	//    current line         percent
	//   -------------    ~~ ----------
	//    total lines            100
	if (tot > 0) {
		percent = (100 * cur) / tot;
	} else {
		cur = tot = 0;
		percent = 100;
	}

	trunc_at = columns < STATUS_BUFFER_LEN-1 ?
		columns : STATUS_BUFFER_LEN-1;

	ret = snprintf(status_buffer, trunc_at+1,
#if ENABLE_FEATURE_VI_READONLY
		"%c %s%s%s %d/%d %d%%",
#else
		"%c %s%s %d/%d %d%%",
#endif
		cmd_mode_indicator[cmd_mode & 3],
		(current_filename != NULL ? current_filename : "No file"),
#if ENABLE_FEATURE_VI_READONLY
		(readonly_mode ? " [Readonly]" : ""),
#endif
		(modified_count ? " [Modified]" : ""),
		cur, tot, percent);

	if (ret >= 0 && ret < trunc_at)
		return ret;  // it all fit

	return trunc_at;  // had to truncate
#undef tot
}

static int bufsum(char *buf, int count)
{
	int sum = 0;
	char *e = buf + count;
	while (buf < e)
		sum += (unsigned char) *buf++;
	return sum;
}

static void show_status_line(void)
{
	int cnt = 0, cksum = 0;

	// either we already have an error or status message, or we
	// create one.
	if (!have_status_msg) {
		cnt = format_edit_status();
		cksum = bufsum(status_buffer, cnt);
	}
	if (have_status_msg || ((cnt > 0 && last_status_cksum != cksum))) {
		last_status_cksum = cksum;		// remember if we have seen this line
		go_bottom_and_clear_to_eol();
		write1(status_buffer);
		if (have_status_msg) {
			if (((int)strlen(status_buffer) - (have_status_msg - 1)) >
					(columns - 1) ) {
				have_status_msg = 0;
				Hit_Return();
			}
			have_status_msg = 0;
		}
		place_cursor(crow, ccol);  // put cursor back in correct place
	}
	fflush_all();
}

//----- format the status buffer, the bottom line of screen ------
static void status_line(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsnprintf(status_buffer, STATUS_BUFFER_LEN, format, args);
	va_end(args);

	have_status_msg = 1;
}
static void status_line_bold(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	strcpy(status_buffer, ESC_BOLD_TEXT);
	vsnprintf(status_buffer + (sizeof(ESC_BOLD_TEXT)-1),
		STATUS_BUFFER_LEN - sizeof(ESC_BOLD_TEXT) - sizeof(ESC_NORM_TEXT),
		format, args
	);
	strcat(status_buffer, ESC_NORM_TEXT);
	va_end(args);

	have_status_msg = 1 + (sizeof(ESC_BOLD_TEXT)-1) + (sizeof(ESC_NORM_TEXT)-1);
}
static void status_line_bold_errno(const char *fn)
{
	status_line_bold("'%s' "STRERROR_FMT, fn STRERROR_ERRNO);
}

// copy s to buf, convert unprintable
static void print_literal(char *buf, const char *s)
{
	char *d;
	unsigned char c;

	if (!s[0])
		s = "(NULL)";

	d = buf;
	for (; *s; s++) {
		c = *s;
		if ((c & 0x80) && !Isprint(c))
			c = '?';
		if (c < ' ' || c == 0x7f) {
			*d++ = '^';
			c |= '@'; // 0x40
			if (c == 0x7f)
				c = '?';
		}
		*d++ = c;
		*d = '\0';
		if (d - buf > MAX_INPUT_LEN - 10) // paranoia
			break;
	}
}
static void not_implemented(const char *s)
{
	char buf[MAX_INPUT_LEN];
	print_literal(buf, s);
	status_line_bold("'%s' is not implemented", buf);
}

//----- Block insert/delete, undo ops --------------------------
#if ENABLE_FEATURE_VI_YANKMARK
// copy text into a register
static char *text_yank(char *p, char *q, int dest, int buftype)
{
	char *oldreg = reg[dest];
	int cnt = q - p;
	if (cnt < 0) {		// they are backwards- reverse them
		p = q;
		cnt = -cnt;
	}
	// Don't free register yet.  This prevents the memory allocator
	// from reusing the free block so we can detect if it's changed.
	reg[dest] = xstrndup(p, cnt + 1);
	regtype[dest] = buftype;
	free(oldreg);
	return p;
}

static char what_reg(void)
{
	char c;

	c = 'D';			// default to D-reg
	if (YDreg <= 25)
		c = 'a' + (char) YDreg;
	if (YDreg == 26)
		c = 'D';
	if (YDreg == 27)
		c = 'U';
	return c;
}

static void check_context(char cmd)
{
	// Certain movement commands update the context.
	if (strchr(":%{}'GHLMz/?Nn", cmd) != NULL) {
		mark[27] = mark[26];	// move cur to prev
		mark[26] = dot;	// move local to cur
	}
}

static char *swap_context(char *p) // goto new context for '' command make this the current context
{
	char *tmp;

	// the current context is in mark[26]
	// the previous context is in mark[27]
	// only swap context if other context is valid
	if (text <= mark[27] && mark[27] <= end - 1) {
		tmp = mark[27];
		mark[27] = p;
		mark[26] = p = tmp;
	}
	return p;
}

# if ENABLE_FEATURE_VI_VERBOSE_STATUS
static void yank_status(const char *op, const char *p, int cnt)
{
	int lines, chars;

	lines = chars = 0;
	while (*p) {
		++chars;
		if (*p++ == '\n')
			++lines;
	}
	status_line("%s %d lines (%d chars) from [%c]",
				op, lines * cnt, chars * cnt, what_reg());
}
# endif
#endif /* FEATURE_VI_YANKMARK */

#if ENABLE_FEATURE_VI_UNDO
static void undo_push(char *, unsigned, int);
#endif

// open a hole in text[]
// might reallocate text[]! use p += text_hole_make(p, ...),
// and be careful to not use pointers into potentially freed text[]!
static uintptr_t text_hole_make(char *p, int size)	// at "p", make a 'size' byte hole
{
	uintptr_t bias = 0;

	if (size <= 0)
		return bias;
	end += size;		// adjust the new END
	if (end >= (text + text_size)) {
		char *new_text;
		text_size += end - (text + text_size) + 10240;
		new_text = xrealloc(text, text_size);
		bias = (new_text - text);
		screenbegin += bias;
		dot         += bias;
		end         += bias;
		p           += bias;
#if ENABLE_FEATURE_VI_YANKMARK
		{
			int i;
			for (i = 0; i < ARRAY_SIZE(mark); i++)
				if (mark[i])
					mark[i] += bias;
		}
#endif
		text = new_text;
	}
	memmove(p + size, p, end - size - p);
	memset(p, ' ', size);	// clear new hole
	return bias;
}

// close a hole in text[] - delete "p" through "q", inclusive
// "undo" value indicates if this operation should be undo-able
#if !ENABLE_FEATURE_VI_UNDO
#define text_hole_delete(a,b,c) text_hole_delete(a,b)
#endif
static char *text_hole_delete(char *p, char *q, int undo)
{
	char *src, *dest;
	int cnt, hole_size;

	// move forwards, from beginning
	// assume p <= q
	src = q + 1;
	dest = p;
	if (q < p) {		// they are backward- swap them
		src = p + 1;
		dest = q;
	}
	hole_size = q - p + 1;
	cnt = end - src;
#if ENABLE_FEATURE_VI_UNDO
	switch (undo) {
		case NO_UNDO:
			break;
		case ALLOW_UNDO:
			undo_push(p, hole_size, UNDO_DEL);
			break;
		case ALLOW_UNDO_CHAIN:
			undo_push(p, hole_size, UNDO_DEL_CHAIN);
			break;
# if ENABLE_FEATURE_VI_UNDO_QUEUE
		case ALLOW_UNDO_QUEUED:
			undo_push(p, hole_size, UNDO_DEL_QUEUED);
			break;
# endif
	}
	modified_count--;
#endif
	if (src < text || src > end)
		goto thd0;
	if (dest < text || dest >= end)
		goto thd0;
	modified_count++;
	if (src >= end)
		goto thd_atend;	// just delete the end of the buffer
	memmove(dest, src, cnt);
 thd_atend:
	end = end - hole_size;	// adjust the new END
	if (dest >= end)
		dest = end - 1;	// make sure dest in below end-1
	if (end <= text)
		dest = end = text;	// keep pointers valid
 thd0:
	return dest;
}

#if ENABLE_FEATURE_VI_UNDO

# if ENABLE_FEATURE_VI_UNDO_QUEUE
// Flush any queued objects to the undo stack
static void undo_queue_commit(void)
{
	// Pushes the queue object onto the undo stack
	if (undo_q > 0) {
		// Deleted character undo events grow from the end
		undo_push(undo_queue + CONFIG_FEATURE_VI_UNDO_QUEUE_MAX - undo_q,
			undo_q,
			(undo_queue_state | UNDO_USE_SPOS)
		);
		undo_queue_state = UNDO_EMPTY;
		undo_q = 0;
	}
}
# else
#  define undo_queue_commit() ((void)0)
# endif

static void flush_undo_data(void)
{
	struct undo_object *undo_entry;

	while (undo_stack_tail) {
		undo_entry = undo_stack_tail;
		undo_stack_tail = undo_entry->prev;
		free(undo_entry);
	}
}

// Undo functions and hooks added by Jody Bruchon (jody@jodybruchon.com)
// Add to the undo stack
static void undo_push(char *src, unsigned length, int u_type)
{
	struct undo_object *undo_entry;
# if ENABLE_FEATURE_VI_UNDO_QUEUE
	int use_spos = u_type & UNDO_USE_SPOS;
# endif

	// "u_type" values
	// UNDO_INS: insertion, undo will remove from buffer
	// UNDO_DEL: deleted text, undo will restore to buffer
	// UNDO_{INS,DEL}_CHAIN: Same as above but also calls undo_pop() when complete
	// The CHAIN operations are for handling multiple operations that the user
	// performs with a single action, i.e. REPLACE mode or find-and-replace commands
	// UNDO_{INS,DEL}_QUEUED: If queuing feature is enabled, allow use of the queue
	// for the INS/DEL operation.
	// UNDO_{INS,DEL} ORed with UNDO_USE_SPOS: commit the undo queue

# if ENABLE_FEATURE_VI_UNDO_QUEUE
	// This undo queuing functionality groups multiple character typing or backspaces
	// into a single large undo object. This greatly reduces calls to malloc() for
	// single-character operations while typing and has the side benefit of letting
	// an undo operation remove chunks of text rather than a single character.
	switch (u_type) {
	case UNDO_EMPTY:	// Just in case this ever happens...
		return;
	case UNDO_DEL_QUEUED:
		if (length != 1)
			return;	// Only queue single characters
		switch (undo_queue_state) {
		case UNDO_EMPTY:
			undo_queue_state = UNDO_DEL;
		case UNDO_DEL:
			undo_queue_spos = src;
			undo_q++;
			undo_queue[CONFIG_FEATURE_VI_UNDO_QUEUE_MAX - undo_q] = *src;
			// If queue is full, dump it into an object
			if (undo_q == CONFIG_FEATURE_VI_UNDO_QUEUE_MAX)
				undo_queue_commit();
			return;
		case UNDO_INS:
			// Switch from storing inserted text to deleted text
			undo_queue_commit();
			undo_push(src, length, UNDO_DEL_QUEUED);
			return;
		}
		break;
	case UNDO_INS_QUEUED:
		if (length < 1)
			return;
		switch (undo_queue_state) {
		case UNDO_EMPTY:
			undo_queue_state = UNDO_INS;
			undo_queue_spos = src;
		case UNDO_INS:
			while (length--) {
				undo_q++;	// Don't need to save any data for insertions
				if (undo_q == CONFIG_FEATURE_VI_UNDO_QUEUE_MAX)
					undo_queue_commit();
			}
			return;
		case UNDO_DEL:
			// Switch from storing deleted text to inserted text
			undo_queue_commit();
			undo_push(src, length, UNDO_INS_QUEUED);
			return;
		}
		break;
	}
	u_type &= ~UNDO_USE_SPOS;
# endif

	// Allocate a new undo object
	if (u_type == UNDO_DEL || u_type == UNDO_DEL_CHAIN) {
		// For UNDO_DEL objects, save deleted text
		if ((text + length) == end)
			length--;
		// If this deletion empties text[], strip the newline. When the buffer becomes
		// zero-length, a newline is added back, which requires this to compensate.
		undo_entry = xzalloc(offsetof(struct undo_object, undo_text) + length);
		memcpy(undo_entry->undo_text, src, length);
	} else {
		undo_entry = xzalloc(sizeof(*undo_entry));
	}
	undo_entry->length = length;
# if ENABLE_FEATURE_VI_UNDO_QUEUE
	if (use_spos) {
		undo_entry->start = undo_queue_spos - text;	// use start position from queue
	} else {
		undo_entry->start = src - text;	// use offset from start of text buffer
	}
# else
	undo_entry->start = src - text;
# endif
	undo_entry->u_type = u_type;

	// Push it on undo stack
	undo_entry->prev = undo_stack_tail;
	undo_stack_tail = undo_entry;
	modified_count++;
}

static void undo_push_insert(char *p, int len, int undo)
{
	switch (undo) {
	case ALLOW_UNDO:
		undo_push(p, len, UNDO_INS);
		break;
	case ALLOW_UNDO_CHAIN:
		undo_push(p, len, UNDO_INS_CHAIN);
		break;
# if ENABLE_FEATURE_VI_UNDO_QUEUE
	case ALLOW_UNDO_QUEUED:
		undo_push(p, len, UNDO_INS_QUEUED);
		break;
# endif
	}
}

// Undo the last operation
static void undo_pop(void)
{
	int repeat;
	char *u_start, *u_end;
	struct undo_object *undo_entry;

	// Commit pending undo queue before popping (should be unnecessary)
	undo_queue_commit();

	undo_entry = undo_stack_tail;
	// Check for an empty undo stack
	if (!undo_entry) {
		status_line("Already at oldest change");
		return;
	}

	switch (undo_entry->u_type) {
	case UNDO_DEL:
	case UNDO_DEL_CHAIN:
		// make hole and put in text that was deleted; deallocate text
		u_start = text + undo_entry->start;
		text_hole_make(u_start, undo_entry->length);
		memcpy(u_start, undo_entry->undo_text, undo_entry->length);
# if ENABLE_FEATURE_VI_VERBOSE_STATUS
		status_line("Undo [%d] %s %d chars at position %d",
			modified_count, "restored",
			undo_entry->length, undo_entry->start
		);
# endif
		break;
	case UNDO_INS:
	case UNDO_INS_CHAIN:
		// delete what was inserted
		u_start = undo_entry->start + text;
		u_end = u_start - 1 + undo_entry->length;
		text_hole_delete(u_start, u_end, NO_UNDO);
# if ENABLE_FEATURE_VI_VERBOSE_STATUS
		status_line("Undo [%d] %s %d chars at position %d",
			modified_count, "deleted",
			undo_entry->length, undo_entry->start
		);
# endif
		break;
	}
	repeat = 0;
	switch (undo_entry->u_type) {
	// If this is the end of a chain, lower modification count and refresh display
	case UNDO_DEL:
	case UNDO_INS:
		dot = (text + undo_entry->start);
		refresh(FALSE);
		break;
	case UNDO_DEL_CHAIN:
	case UNDO_INS_CHAIN:
		repeat = 1;
		break;
	}
	// Deallocate the undo object we just processed
	undo_stack_tail = undo_entry->prev;
	free(undo_entry);
	modified_count--;
	// For chained operations, continue popping all the way down the chain.
	if (repeat) {
		undo_pop();	// Follow the undo chain if one exists
	}
}

#else
# define flush_undo_data()   ((void)0)
# define undo_queue_commit() ((void)0)
#endif /* ENABLE_FEATURE_VI_UNDO */

//----- Dot Movement Routines ----------------------------------
static void dot_left(void)
{
	undo_queue_commit();
	if (dot > text && dot[-1] != '\n')
		dot--;
}

static void dot_right(void)
{
	undo_queue_commit();
	if (dot < end - 1 && *dot != '\n')
		dot++;
}

static void dot_begin(void)
{
	undo_queue_commit();
	dot = begin_line(dot);	// return pointer to first char cur line
}

static void dot_end(void)
{
	undo_queue_commit();
	dot = end_line(dot);	// return pointer to last char cur line
}

static char *move_to_col(char *p, int l)
{
	int co;

	p = begin_line(p);
	co = 0;
	do {
		if (*p == '\n') //vda || *p == '\0')
			break;
		co = next_column(*p, co);
	} while (co <= l && p++ < end);
	return p;
}

static void dot_next(void)
{
	undo_queue_commit();
	dot = next_line(dot);
}

static void dot_prev(void)
{
	undo_queue_commit();
	dot = prev_line(dot);
}

static void dot_skip_over_ws(void)
{
	// skip WS
	while (isspace(*dot) && *dot != '\n' && dot < end - 1)
		dot++;
}

static void dot_to_char(int cmd)
{
	char *q = dot;
	int dir = islower(cmd) ? FORWARD : BACK;

	if (last_search_char == 0)
		return;

	do {
		do {
			q += dir;
			if ((dir == FORWARD ? q > end - 1 : q < text) || *q == '\n') {
				indicate_error();
				return;
			}
		} while (*q != last_search_char);
	} while (--cmdcnt > 0);

	dot = q;

	// place cursor before/after char as required
	if (cmd == 't')
		dot_left();
	else if (cmd == 'T')
		dot_right();
}

static void dot_scroll(int cnt, int dir)
{
	char *q;

	undo_queue_commit();
	for (; cnt > 0; cnt--) {
		if (dir < 0) {
			// scroll Backwards
			// ctrl-Y scroll up one line
			screenbegin = prev_line(screenbegin);
		} else {
			// scroll Forwards
			// ctrl-E scroll down one line
			screenbegin = next_line(screenbegin);
		}
	}
	// make sure "dot" stays on the screen so we dont scroll off
	if (dot < screenbegin)
		dot = screenbegin;
	q = end_screen();	// find new bottom line
	if (dot > q)
		dot = begin_line(q);	// is dot is below bottom line?
	dot_skip_over_ws();
}

static char *bound_dot(char *p) // make sure  text[0] <= P < "end"
{
	if (p >= end && end > text) {
		p = end - 1;
		indicate_error();
	}
	if (p < text) {
		p = text;
		indicate_error();
	}
	return p;
}

#if ENABLE_FEATURE_VI_DOT_CMD
static void start_new_cmd_q(char c)
{
	// get buffer for new cmd
	dotcnt = cmdcnt ?: 1;
	last_modifying_cmd[0] = c;
	lmc_len = 1;
	adding2q = 1;
}
static void end_cmd_q(void)
{
# if ENABLE_FEATURE_VI_YANKMARK
	YDreg = 26;			// go back to default Yank/Delete reg
# endif
	adding2q = 0;
}
#else
# define end_cmd_q() ((void)0)
#endif /* FEATURE_VI_DOT_CMD */

// copy text into register, then delete text.
//
#if !ENABLE_FEATURE_VI_UNDO
#define yank_delete(a,b,c,d,e) yank_delete(a,b,c,d)
#endif
static char *yank_delete(char *start, char *stop, int buftype, int yf, int undo)
{
	char *p;

	// make sure start <= stop
	if (start > stop) {
		// they are backwards, reverse them
		p = start;
		start = stop;
		stop = p;
	}
	if (buftype == PARTIAL && *start == '\n')
		return start;
	p = start;
#if ENABLE_FEATURE_VI_YANKMARK
	text_yank(start, stop, YDreg, buftype);
#endif
	if (yf == YANKDEL) {
		p = text_hole_delete(start, stop, undo);
	}					// delete lines
	return p;
}

// might reallocate text[]!
static int file_insert(const char *fn, char *p, int initial)
{
	int cnt = -1;
	int fd, size;
	struct stat statbuf;

	if (p < text)
		p = text;
	if (p > end)
		p = end;

	fd = open(fn, O_RDONLY);
	if (fd < 0) {
		if (!initial)
			status_line_bold_errno(fn);
		return cnt;
	}

	// Validate file
	if (fstat(fd, &statbuf) < 0) {
		status_line_bold_errno(fn);
		goto fi;
	}
	if (!S_ISREG(statbuf.st_mode)) {
		status_line_bold("'%s' is not a regular file", fn);
		goto fi;
	}
	size = (statbuf.st_size < INT_MAX ? (int)statbuf.st_size : INT_MAX);
	p += text_hole_make(p, size);
	cnt = full_read(fd, p, size);
	if (cnt < 0) {
		status_line_bold_errno(fn);
		p = text_hole_delete(p, p + size - 1, NO_UNDO);	// un-do buffer insert
	} else if (cnt < size) {
		// There was a partial read, shrink unused space
		p = text_hole_delete(p + cnt, p + size - 1, NO_UNDO);
		status_line_bold("can't read '%s'", fn);
	}
# if ENABLE_FEATURE_VI_UNDO
	else {
		undo_push_insert(p, size, ALLOW_UNDO);
	}
# endif
 fi:
	close(fd);

#if ENABLE_FEATURE_VI_READONLY
	if (initial
	 && ((access(fn, W_OK) < 0) ||
		// root will always have access()
		// so we check fileperms too
		!(statbuf.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))
	    )
	) {
		SET_READONLY_FILE(readonly_mode);
	}
#endif
	return cnt;
}

// find matching char of pair  ()  []  {}
// will crash if c is not one of these
static char *find_pair(char *p, const char c)
{
	const char *braces = "()[]{}";
	char match;
	int dir, level;

	dir = strchr(braces, c) - braces;
	dir ^= 1;
	match = braces[dir];
	dir = ((dir & 1) << 1) - 1; // 1 for ([{, -1 for )\}

	// look for match, count levels of pairs  (( ))
	level = 1;
	for (;;) {
		p += dir;
		if (p < text || p >= end)
			return NULL;
		if (*p == c)
			level++;	// increase pair levels
		if (*p == match) {
			level--;	// reduce pair level
			if (level == 0)
				return p; // found matching pair
		}
	}
}

#if ENABLE_FEATURE_VI_SETOPTS
// show the matching char of a pair,  ()  []  {}
static void showmatching(char *p)
{
	char *q, *save_dot;

	// we found half of a pair
	q = find_pair(p, *p);	// get loc of matching char
	if (q == NULL) {
		indicate_error();	// no matching char
	} else {
		// "q" now points to matching pair
		save_dot = dot;	// remember where we are
		dot = q;		// go to new loc
		refresh(FALSE);	// let the user see it
		mysleep(40);	// give user some time
		dot = save_dot;	// go back to old loc
		refresh(FALSE);
	}
}
#endif /* FEATURE_VI_SETOPTS */

// might reallocate text[]! use p += stupid_insert(p, ...),
// and be careful to not use pointers into potentially freed text[]!
static uintptr_t stupid_insert(char *p, char c) // stupidly insert the char c at 'p'
{
	uintptr_t bias;
	bias = text_hole_make(p, 1);
	p += bias;
	*p = c;
	return bias;
}

// find number of characters in indent, p must be at beginning of line
static size_t indent_len(char *p)
{
	char *r = p;

	while (r < (end - 1) && isblank(*r))
		r++;
	return r - p;
}

#if !ENABLE_FEATURE_VI_UNDO
#define char_insert(a,b,c) char_insert(a,b)
#endif
static char *char_insert(char *p, char c, int undo) // insert the char c at 'p'
{
#if ENABLE_FEATURE_VI_SETOPTS
# define indentcol char_insert__indentcol
	size_t len;
	int col, ntab, nspc;
#endif
	char *bol = begin_line(p);

	if (c == 22) {		// Is this an ctrl-V?
		p += stupid_insert(p, '^');	// use ^ to indicate literal next
		refresh(FALSE);	// show the ^
		c = get_one_char();
		*p = c;
#if ENABLE_FEATURE_VI_UNDO
		undo_push_insert(p, 1, undo);
#else
		modified_count++;
#endif
		p++;
	} else if (c == 27) {	// Is this an ESC?
		cmd_mode = 0;
		undo_queue_commit();
		cmdcnt = 0;
		end_cmd_q();	// stop adding to q
		last_status_cksum = 0;	// force status update
		if ((dot > text) && (p[-1] != '\n')) {
			p--;
		}
#if ENABLE_FEATURE_VI_SETOPTS
		if (autoindent) {
			len = indent_len(bol);
			col = get_column(bol + len);
			if (len && col == indentcol && bol[len] == '\n') {
				// remove autoindent from otherwise empty line
				text_hole_delete(bol, bol + len - 1, undo);
				p = bol;
			}
		}
#endif
	} else if (c == 4) {	// ctrl-D reduces indentation
		char *r = bol + indent_len(bol);
		int prev = prev_tabstop(get_column(r));
		while (r > bol && get_column(r) > prev) {
			if (p > bol)
				p--;
			r--;
			r = text_hole_delete(r, r, ALLOW_UNDO_QUEUED);
		}

#if ENABLE_FEATURE_VI_SETOPTS
		if (autoindent && indentcol && r == end_line(p)) {
			// record changed size of autoindent
			indentcol = get_column(p);
			return p;
		}
#endif
#if ENABLE_FEATURE_VI_SETOPTS
	} else if (c == '\t' && expandtab) {	// expand tab
		col = get_column(p);
		col = next_tabstop(col) - col + 1;
		while (col--) {
# if ENABLE_FEATURE_VI_UNDO
			undo_push_insert(p, 1, undo);
# else
			modified_count++;
# endif
			p += 1 + stupid_insert(p, ' ');
		}
#endif
	} else if (isbackspace(c)) {
		if (cmd_mode == 2) {
			// special treatment for backspace in Replace mode
			if (p > rstart) {
				p--;
#if ENABLE_FEATURE_VI_UNDO
				undo_pop();
#endif
			}
		} else if (p > text) {
			p--;
			p = text_hole_delete(p, p, ALLOW_UNDO_QUEUED);	// shrink buffer 1 char
		}
	} else {
		// insert a char into text[]
		if (c == 13)
			c = '\n';	// translate \r to \n
#if ENABLE_FEATURE_VI_UNDO
# if ENABLE_FEATURE_VI_UNDO_QUEUE
		if (c == '\n')
			undo_queue_commit();
# endif
		undo_push_insert(p, 1, undo);
#else
		modified_count++;
#endif
		p += 1 + stupid_insert(p, c);	// insert the char
#if ENABLE_FEATURE_VI_SETOPTS
		if (showmatch && strchr(")]}", c) != NULL) {
			showmatching(p - 1);
		}
		if (autoindent && c == '\n') {	// auto indent the new line
			if (newindent < 0) {
				// use indent of previous line
				bol = prev_line(p);
				len = indent_len(bol);
				col = get_column(bol + len);

				if (len && col == indentcol) {
					// previous line was empty except for autoindent
					// move the indent to the current line
					memmove(bol + 1, bol, len);
					*bol = '\n';
					return p;
				}
			} else {
				// for 'O'/'cc' commands add indent before newly inserted NL
				if (p != end - 1)	// but not for 'cc' at EOF
					p--;
				col = newindent;
			}

			if (col) {
				// only record indent if in insert/replace mode or for
				// the 'o'/'O'/'cc' commands, which are switched to
				// insert mode early.
				indentcol = cmd_mode != 0 ? col : 0;
				if (expandtab) {
					ntab = 0;
					nspc = col;
				} else {
					ntab = col / tabstop;
					nspc = col % tabstop;
				}
				p += text_hole_make(p, ntab + nspc);
# if ENABLE_FEATURE_VI_UNDO
				undo_push_insert(p, ntab + nspc, undo);
# endif
				memset(p, '\t', ntab);
				p += ntab;
				memset(p, ' ', nspc);
				return p + nspc;
			}
		}
#endif
	}
#if ENABLE_FEATURE_VI_SETOPTS
	indentcol = 0;
# undef indentcol
#endif
	return p;
}

#if ENABLE_FEATURE_VI_COLON_EXPAND
static void init_filename(char *fn)
{
	char *copy = xstrdup(fn);

	if (current_filename == NULL) {
		current_filename = copy;
	} else {
		free(alt_filename);
		alt_filename = copy;
	}
}
#else
# define init_filename(f) ((void)(0))
#endif

static void update_filename(char *fn)
{
#if ENABLE_FEATURE_VI_COLON_EXPAND
	if (fn == NULL)
		return;

	if (current_filename == NULL || strcmp(fn, current_filename) != 0) {
		free(alt_filename);
		alt_filename = current_filename;
		current_filename = xstrdup(fn);
	}
#else
	if (fn != current_filename) {
		free(current_filename);
		current_filename = xstrdup(fn);
	}
#endif
}

// read text from file or create an empty buf
// will also update current_filename
static int init_text_buffer(char *fn)
{
	int rc;

	// allocate/reallocate text buffer
	free(text);
	text_size = 10240;
	screenbegin = dot = end = text = xzalloc(text_size);

	update_filename(fn);
	rc = file_insert(fn, text, 1);
	if (rc <= 0 || *(end - 1) != '\n') {
		// file doesn't exist or doesn't end in a newline.
		// insert a newline to the end
		char_insert(end, '\n', NO_UNDO);
	}

	flush_undo_data();
	modified_count = 0;
	last_modified_count = -1;
#if ENABLE_FEATURE_VI_YANKMARK
	// init the marks
	memset(mark, 0, sizeof(mark));
#endif
	return rc;
}

#if ENABLE_FEATURE_VI_YANKMARK \
 || (ENABLE_FEATURE_VI_COLON && ENABLE_FEATURE_VI_SEARCH) \
 || ENABLE_FEATURE_VI_CRASHME
// might reallocate text[]! use p += string_insert(p, ...),
// and be careful to not use pointers into potentially freed text[]!
# if !ENABLE_FEATURE_VI_UNDO
#  define string_insert(a,b,c) string_insert(a,b)
# endif
static uintptr_t string_insert(char *p, const char *s, int undo) // insert the string at 'p'
{
	uintptr_t bias;
	int i;

	i = strlen(s);
#if ENABLE_FEATURE_VI_UNDO
	undo_push_insert(p, i, undo);
#endif
	bias = text_hole_make(p, i);
	p += bias;
	memcpy(p, s, i);
	return bias;
}
#endif

static int file_write(char *fn, char *first, char *last)
{
	int fd, cnt, charcnt;

	if (fn == 0) {
		status_line_bold("No current filename");
		return -2;
	}
	// By popular request we do not open file with O_TRUNC,
	// but instead ftruncate() it _after_ successful write.
	// Might reduce amount of data lost on power fail etc.
	fd = open(fn, (O_WRONLY | O_CREAT), 0666);
	if (fd < 0)
		return -1;
	cnt = last - first + 1;
	charcnt = full_write(fd, first, cnt);
	ftruncate(fd, charcnt);
	if (charcnt == cnt) {
		// good write
		//modified_count = FALSE;
	} else {
		charcnt = 0;
	}
	close(fd);
	return charcnt;
}

#if ENABLE_FEATURE_VI_SEARCH
# if ENABLE_FEATURE_VI_REGEX_SEARCH
// search for pattern starting at p
static char *char_search(char *p, const char *pat, int dir_and_range)
{
	struct re_pattern_buffer preg;
	const char *err;
	char *q;
	int i, size, range, start;

	re_syntax_options = RE_SYNTAX_POSIX_BASIC & (~RE_DOT_NEWLINE);
	if (ignorecase)
		re_syntax_options |= RE_ICASE;

	memset(&preg, 0, sizeof(preg));
	err = re_compile_pattern(pat, strlen(pat), &preg);
	preg.not_bol = p != text;
	preg.not_eol = p != end - 1;
	if (err != NULL) {
		status_line_bold("bad search pattern '%s': %s", pat, err);
		return p;
	}

	range = (dir_and_range & 1);
	q = end - 1; // if FULL
	if (range == LIMITED)
		q = next_line(p);
	if (dir_and_range < 0) { // BACK?
		q = text;
		if (range == LIMITED)
			q = prev_line(p);
	}

	// RANGE could be negative if we are searching backwards
	range = q - p;
	if (range < 0) {
		size = -range;
		start = size;
	} else {
		size = range;
		start = 0;
	}
	q = p - start;
	if (q < text)
		q = text;
	// search for the compiled pattern, preg, in p[]
	// range < 0, start == size: search backward
	// range > 0, start == 0: search forward
	// re_search() < 0: not found or error
	// re_search() >= 0: index of found pattern
	//           struct pattern   char     int   int    int    struct reg
	// re_search(*pattern_buffer, *string, size, start, range, *regs)
	i = re_search(&preg, q, size, start, range, /*struct re_registers*:*/ NULL);
	regfree(&preg);
	return i < 0 ? NULL : q + i;
}
# else
#  if ENABLE_FEATURE_VI_SETOPTS
static int mycmp(const char *s1, const char *s2, int len)
{
	if (ignorecase) {
		return strncasecmp(s1, s2, len);
	}
	return strncmp(s1, s2, len);
}
#  else
#   define mycmp strncmp
#  endif
static char *char_search(char *p, const char *pat, int dir_and_range)
{
	char *start, *stop;
	int len;
	int range;

	len = strlen(pat);
	range = (dir_and_range & 1);
	if (dir_and_range > 0) { //FORWARD?
		stop = end - 1;	// assume range is p..end-1
		if (range == LIMITED)
			stop = next_line(p);	// range is to next line
		for (start = p; start < stop; start++) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	} else { //BACK
		stop = text;	// assume range is text..p
		if (range == LIMITED)
			stop = prev_line(p);	// range is to prev line
		for (start = p - len; start >= stop; start--) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	}
	// pattern not found
	return NULL;
}
# endif
#endif /* FEATURE_VI_SEARCH */

//----- The Colon commands -------------------------------------
#if ENABLE_FEATURE_VI_COLON
// Evaluate colon address expression.  Returns a pointer to the
// next character or NULL on error.  If 'result' contains a valid
// address 'valid' is TRUE.
static char *get_one_address(char *p, int *result, int *valid)
{
	int num, sign, addr, got_addr;
# if ENABLE_FEATURE_VI_YANKMARK || ENABLE_FEATURE_VI_SEARCH
	char *q, c;
# endif
	IF_FEATURE_VI_SEARCH(int dir;)

	got_addr = FALSE;
	addr = count_lines(text, dot);	// default to current line
	sign = 0;
	for (;;) {
		if (isblank(*p)) {
			if (got_addr) {
				addr += sign;
				sign = 0;
			}
			p++;
		} else if (!got_addr && *p == '.') {	// the current line
			p++;
			//addr = count_lines(text, dot);
			got_addr = TRUE;
		} else if (!got_addr && *p == '$') {	// the last line in file
			p++;
			addr = count_lines(text, end - 1);
			got_addr = TRUE;
		}
# if ENABLE_FEATURE_VI_YANKMARK
		else if (!got_addr && *p == '\'') {	// is this a mark addr
			p++;
			c = tolower(*p);
			p++;
			q = NULL;
			if (c >= 'a' && c <= 'z') {
				// we have a mark
				c = c - 'a';
				q = mark[(unsigned char) c];
			}
			if (q == NULL) {	// is mark valid
				status_line_bold("Mark not set");
				return NULL;
			}
			addr = count_lines(text, q);
			got_addr = TRUE;
		}
# endif
# if ENABLE_FEATURE_VI_SEARCH
		else if (!got_addr && (*p == '/' || *p == '?')) {	// a search pattern
			c = *p;
			q = strchrnul(p + 1, c);
			if (p + 1 != q) {
				// save copy of new pattern
				free(last_search_pattern);
				last_search_pattern = xstrndup(p, q - p);
			}
			p = q;
			if (*p == c)
				p++;
			if (c == '/') {
				q = next_line(dot);
				dir = (FORWARD << 1) | FULL;
			} else {
				q = begin_line(dot);
				dir = ((unsigned)BACK << 1) | FULL;
			}
			q = char_search(q, last_search_pattern + 1, dir);
			if (q == NULL) {
				// no match, continue from other end of file
				q = char_search(dir > 0 ? text : end - 1,
								last_search_pattern + 1, dir);
				if (q == NULL) {
					status_line_bold("Pattern not found");
					return NULL;
				}
			}
			addr = count_lines(text, q);
			got_addr = TRUE;
		}
# endif
		else if (isdigit(*p)) {
			num = 0;
			while (isdigit(*p))
				num = num * 10 + *p++ -'0';
			if (!got_addr) {	// specific line number
				addr = num;
				got_addr = TRUE;
			} else {	// offset from current addr
				addr += sign >= 0 ? num : -num;
			}
			sign = 0;
		} else if (*p == '-' || *p == '+') {
			if (!got_addr) {	// default address is dot
				//addr = count_lines(text, dot);
				got_addr = TRUE;
			} else {
				addr += sign;
			}
			sign = *p++ == '-' ? -1 : 1;
		} else {
			addr += sign;	// consume unused trailing sign
			break;
		}
	}
	*result = addr;
	*valid = got_addr;
	return p;
}

# define GET_ADDRESS   0
# define GET_SEPARATOR 1

// Read line addresses for a colon command.  The user can enter as
// many as they like but only the last two will be used.
static char *get_address(char *p, int *b, int *e, unsigned int *got)
{
	int state = GET_ADDRESS;
	int valid;
	int addr;
	char *save_dot = dot;

	//----- get the address' i.e., 1,3   'a,'b  -----
	for (;;) {
		if (isblank(*p)) {
			p++;
		} else if (state == GET_ADDRESS && *p == '%') {	// alias for 1,$
			p++;
			*b = 1;
			*e = count_lines(text, end-1);
			*got = 3;
			state = GET_SEPARATOR;
		} else if (state == GET_ADDRESS) {
			valid = FALSE;
			p = get_one_address(p, &addr, &valid);
			// Quit on error or if the address is invalid and isn't of
			// the form ',$' or '1,' (in which case it defaults to dot).
			if (p == NULL || !(valid || *p == ',' || *p == ';' || *got & 1))
				break;
			*b = *e;
			*e = addr;
			*got = (*got << 1) | 1;
			state = GET_SEPARATOR;
		} else if (state == GET_SEPARATOR && (*p == ',' || *p == ';')) {
			if (*p == ';')
				dot = find_line(*e);
			p++;
			state = GET_ADDRESS;
		} else {
			break;
		}
	}
	dot = save_dot;
	return p;
}

# if ENABLE_FEATURE_VI_SET && ENABLE_FEATURE_VI_SETOPTS
static void setops(char *args, int flg_no)
{
	char *eq;
	int index;

	eq = strchr(args, '=');
	if (eq) *eq = '\0';
	index = index_in_strings(OPTS_STR, args + flg_no);
	if (eq) *eq = '=';
	if (index < 0) {
 bad:
		status_line_bold("bad option: %s", args);
		return;
	}

	index = 1 << (index >> 1); // convert to VI_bit

	if (index & VI_TABSTOP) {
		int t;
		if (!eq || flg_no) // no "=NNN" or it is "notabstop"?
			goto bad;
		t = bb_strtou(eq + 1, NULL, 10);
		if (t <= 0 || t > MAX_TABSTOP)
			goto bad;
		tabstop = t;
		return;
	}
	if (eq)	goto bad; // boolean option has "="?
	if (flg_no) {
		vi_setops &= ~index;
	} else {
		vi_setops |= index;
	}
}
# endif

# if ENABLE_FEATURE_VI_COLON_EXPAND
static char *expand_args(char *args)
{
	char *s, *t;
	const char *replace;

	args = xstrdup(args);
	for (s = args; *s; s++) {
		if (*s == '%') {
			replace = current_filename;
		} else if (*s == '#') {
			replace = alt_filename;
		} else {
			if (*s == '\\' && s[1] != '\0') {
				for (t = s++; *t; t++)
					*t = t[1];
			}
			continue;
		}

		if (replace == NULL) {
			free(args);
			status_line_bold("No previous filename");
			return NULL;
		}

		*s = '\0';
		t = xasprintf("%s%s%s", args, replace, s+1);
		s = t + (s - args) + strlen(replace);
		free(args);
		args = t;
	}
	return args;
}
# else
#  define expand_args(a) (a)
# endif
#endif /* FEATURE_VI_COLON */

#if ENABLE_FEATURE_VI_REGEX_SEARCH
# define MAX_SUBPATTERN 10	// subpatterns \0 .. \9

// Like strchr() but skipping backslash-escaped characters
static char *strchr_backslash(const char *s, int c)
{
	while (*s) {
		if (*s == c)
			return (char *)s;
		if (*s == '\\')
			if (*++s == '\0')
				break;
		s++;
	}
	return NULL;
}

// If the return value is not NULL the caller should free R
static char *regex_search(char *q, regex_t *preg, const char *Rorig,
				size_t *len_F, size_t *len_R, char **R)
{
	regmatch_t regmatch[MAX_SUBPATTERN], *cur_match;
	char *found = NULL;
	const char *t;
	char *r;

	regmatch[0].rm_so = 0;
	regmatch[0].rm_eo = end_line(q) - q;
	if (regexec(preg, q, MAX_SUBPATTERN, regmatch, REG_STARTEND) != 0)
		return found;

	found = q + regmatch[0].rm_so;
	*len_F = regmatch[0].rm_eo - regmatch[0].rm_so;
	*R = NULL;

 fill_result:
	// first pass calculates len_R, second fills R
	*len_R = 0;
	for (t = Rorig, r = *R; *t; t++) {
		size_t len = 1;	// default is to copy one char from replace pattern
		const char *from = t;
		if (*t == '\\') {
			from = ++t;	// skip backslash
			if (*t >= '0' && *t < '0' + MAX_SUBPATTERN) {
				cur_match = regmatch + (*t - '0');
				if (cur_match->rm_so >= 0) {
					len = cur_match->rm_eo - cur_match->rm_so;
					from = q + cur_match->rm_so;
				}
			}
		}
		*len_R += len;
		if (*R) {
			memcpy(r, from, len);
			r += len;
			/* *r = '\0'; - xzalloc did it */
		}
	}
	if (*R == NULL) {
		*R = xzalloc(*len_R + 1);
		goto fill_result;
	}

	return found;
}
#else /* !ENABLE_FEATURE_VI_REGEX_SEARCH */
# define strchr_backslash(s, c) strchr(s, c)
#endif /* ENABLE_FEATURE_VI_REGEX_SEARCH */

// buf must be no longer than MAX_INPUT_LEN!
static void colon(char *buf)
{
#if !ENABLE_FEATURE_VI_COLON
	// Simple ":cmd" handler with minimal set of commands
	char *p = buf;
	int cnt;

	if (*p == ':')
		p++;
	cnt = strlen(p);
	if (cnt == 0)
		return;
	if (strncmp(p, "quit", cnt) == 0
	 || strcmp(p, "q!") == 0
	) {
		if (modified_count && p[1] != '!') {
			status_line_bold("No write since last change (:%s! overrides)", p);
		} else {
			editing = 0;
		}
		return;
	}
	if (strncmp(p, "write", cnt) == 0
	 || strcmp(p, "wq") == 0
	 || strcmp(p, "wn") == 0
	 || (p[0] == 'x' && !p[1])
	) {
		if (modified_count != 0 || p[0] != 'x') {
			cnt = file_write(current_filename, text, end - 1);
		}
		if (cnt < 0) {
			if (cnt == -1)
				status_line_bold("Write error: "STRERROR_FMT STRERROR_ERRNO);
		} else {
			modified_count = 0;
			last_modified_count = -1;
			status_line("'%s' %uL, %uC",
				current_filename,
				count_lines(text, end - 1), cnt
			);
			if (p[0] == 'x'
			 || p[1] == 'q' || p[1] == 'n'
			) {
				editing = 0;
			}
		}
		return;
	}
	if (strncmp(p, "file", cnt) == 0) {
		last_status_cksum = 0;	// force status update
		return;
	}
	if (sscanf(p, "%d", &cnt) > 0) {
		dot = find_line(cnt);
		dot_skip_over_ws();
		return;
	}
	not_implemented(p);
#else

// check how many addresses we got
# define GOT_ADDRESS (got & 1)
# define GOT_RANGE ((got & 3) == 3)

	char c, *buf1, *q, *r;
	char *fn, cmd[MAX_INPUT_LEN], *cmdend, *args, *exp = NULL;
	int i, l, li, b, e;
	unsigned int got;
	int useforce;

	// :3154	// if (-e line 3154) goto it  else stay put
	// :4,33w! foo	// write a portion of buffer to file "foo"
	// :w		// write all of buffer to current file
	// :q		// quit
	// :q!		// quit- dont care about modified file
	// :'a,'z!sort -u   // filter block through sort
	// :'f		// goto mark "f"
	// :'fl		// list literal the mark "f" line
	// :.r bar	// read file "bar" into buffer before dot
	// :/123/,/abc/d    // delete lines from "123" line to "abc" line
	// :/xyz/	// goto the "xyz" line
	// :s/find/replace/ // substitute pattern "find" with "replace"
	// :!<cmd>	// run <cmd> then return
	//

	while (*buf == ':')
		buf++;			// move past leading colons
	while (isblank(*buf))
		buf++;			// move past leading blanks
	if (!buf[0] || buf[0] == '"')
		goto ret;		// ignore empty lines or those starting with '"'

	li = i = 0;
	b = e = -1;
	got = 0;
	li = count_lines(text, end - 1);
	fn = current_filename;

	// look for optional address(es)  :.  :1  :1,9   :'q,'a   :%
	buf = get_address(buf, &b, &e, &got);
	if (buf == NULL) {
		goto ret;
	}

	// get the COMMAND into cmd[]
	strcpy(cmd, buf);
	buf1 = cmd;
	while (!isspace(*buf1) && *buf1 != '\0') {
		buf1++;
	}
	cmdend = buf1;
	// get any ARGuments
	while (isblank(*buf1))
		buf1++;
	args = buf1;
	*cmdend = '\0';
	useforce = FALSE;
	if (cmdend > cmd && cmdend[-1] == '!') {
		useforce = TRUE;
		cmdend[-1] = '\0';   // get rid of !
	}
	// assume the command will want a range, certain commands
	// (read, substitute) need to adjust these assumptions
	if (!GOT_ADDRESS) {
		q = text;			// no addr, use 1,$ for the range
		r = end - 1;
	} else {
		// at least one addr was given, get its details
		if (e < 0 || e > li) {
			status_line_bold("Invalid range");
			goto ret;
		}
		q = r = find_line(e);
		if (!GOT_RANGE) {
			// if there is only one addr, then it's the line
			// number of the single line the user wants.
			// Reset the end pointer to the end of that line.
			r = end_line(q);
			li = 1;
		} else {
			// we were given two addrs.  change the
			// start pointer to the addr given by user.
			if (b < 0 || b > li || b > e) {
				status_line_bold("Invalid range");
				goto ret;
			}
			q = find_line(b);	// what line is #b
			r = end_line(r);
			li = e - b + 1;
		}
	}
	// ------------ now look for the command ------------
	i = strlen(cmd);
	if (i == 0) {		// :123CR goto line #123
		if (e >= 0) {
			dot = find_line(e);	// what line is #e
			dot_skip_over_ws();
		}
	}
# if ENABLE_FEATURE_ALLOW_EXEC
	else if (cmd[0] == '!') {	// run a cmd
		int retcode;
		// :!ls   run the <cmd>
		exp = expand_args(buf + 1);
		if (exp == NULL)
			goto ret;
		go_bottom_and_clear_to_eol();
		cookmode();
		retcode = system(exp);	// run the cmd
		if (retcode)
			printf("\nshell returned %i\n\n", retcode);
		rawmode();
		Hit_Return();			// let user see results
	}
# endif
	else if (cmd[0] == '=' && !cmd[1]) {	// where is the address
		if (!GOT_ADDRESS) {	// no addr given- use defaults
			e = count_lines(text, dot);
		}
		status_line("%d", e);
	} else if (strncmp(cmd, "delete", i) == 0) {	// delete lines
		if (!GOT_ADDRESS) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		dot = yank_delete(q, r, WHOLE, YANKDEL, ALLOW_UNDO);	// save, then delete lines
		dot_skip_over_ws();
	} else if (strncmp(cmd, "edit", i) == 0) {	// Edit a file
		int size;

		// don't edit, if the current file has been modified
		if (modified_count && !useforce) {
			status_line_bold("No write since last change (:%s! overrides)", cmd);
			goto ret;
		}
		if (args[0]) {
			// the user supplied a file name
			fn = exp = expand_args(args);
			if (exp == NULL)
				goto ret;
		} else if (current_filename == NULL) {
			// no user file name, no current name- punt
			status_line_bold("No current filename");
			goto ret;
		}

		size = init_text_buffer(fn);

# if ENABLE_FEATURE_VI_YANKMARK
		if (Ureg >= 0 && Ureg < 28) {
			free(reg[Ureg]);	//   free orig line reg- for 'U'
			reg[Ureg] = NULL;
		}
		/*if (YDreg < 28) - always true*/ {
			free(reg[YDreg]);	//   free default yank/delete register
			reg[YDreg] = NULL;
		}
# endif
		// how many lines in text[]?
		li = count_lines(text, end - 1);
		status_line("'%s'%s"
			IF_FEATURE_VI_READONLY("%s")
			" %uL, %uC",
			fn,
			(size < 0 ? " [New file]" : ""),
			IF_FEATURE_VI_READONLY(
				((readonly_mode) ? " [Readonly]" : ""),
			)
			li, (int)(end - text)
		);
	} else if (strncmp(cmd, "file", i) == 0) {	// what File is this
		if (e >= 0) {
			status_line_bold("No address allowed on this command");
			goto ret;
		}
		if (args[0]) {
			// user wants a new filename
			exp = expand_args(args);
			if (exp == NULL)
				goto ret;
			update_filename(exp);
		} else {
			// user wants file status info
			last_status_cksum = 0;	// force status update
		}
	} else if (strncmp(cmd, "features", i) == 0) {	// what features are available
		// print out values of all features
		go_bottom_and_clear_to_eol();
		cookmode();
		show_help();
		rawmode();
		Hit_Return();
	} else if (strncmp(cmd, "list", i) == 0) {	// literal print line
		if (!GOT_ADDRESS) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		go_bottom_and_clear_to_eol();
		puts("\r");
		for (; q <= r; q++) {
			int c_is_no_print;

			c = *q;
			c_is_no_print = (c & 0x80) && !Isprint(c);
			if (c_is_no_print) {
				c = '.';
				standout_start();
			}
			if (c == '\n') {
				write1("$\r");
			} else if (c < ' ' || c == 127) {
				bb_putchar('^');
				if (c == 127)
					c = '?';
				else
					c += '@';
			}
			bb_putchar(c);
			if (c_is_no_print)
				standout_end();
		}
		Hit_Return();
	} else if (strncmp(cmd, "quit", i) == 0 // quit
	        || strncmp(cmd, "next", i) == 0 // edit next file
	        || strncmp(cmd, "prev", i) == 0 // edit previous file
	) {
		int n;
		if (useforce) {
			if (*cmd == 'q') {
				// force end of argv list
				optind = cmdline_filecnt;
			}
			editing = 0;
			goto ret;
		}
		// don't exit if the file been modified
		if (modified_count) {
			status_line_bold("No write since last change (:%s! overrides)", cmd);
			goto ret;
		}
		// are there other file to edit
		n = cmdline_filecnt - optind - 1;
		if (*cmd == 'q' && n > 0) {
			status_line_bold("%u more file(s) to edit", n);
			goto ret;
		}
		if (*cmd == 'n' && n <= 0) {
			status_line_bold("No more files to edit");
			goto ret;
		}
		if (*cmd == 'p') {
			// are there previous files to edit
			if (optind < 1) {
				status_line_bold("No previous files to edit");
				goto ret;
			}
			optind -= 2;
		}
		editing = 0;
	} else if (strncmp(cmd, "read", i) == 0) {	// read file into text[]
		int size, num;

		if (args[0]) {
			// the user supplied a file name
			fn = exp = expand_args(args);
			if (exp == NULL)
				goto ret;
			init_filename(fn);
		} else if (current_filename == NULL) {
			// no user file name, no current name- punt
			status_line_bold("No current filename");
			goto ret;
		}
		if (e == 0) {	// user said ":0r foo"
			q = text;
		} else {	// read after given line or current line if none given
			q = next_line(GOT_ADDRESS ? find_line(e) : dot);
			// read after last line
			if (q == end-1)
				++q;
		}
		num = count_lines(text, q);
		if (q == end)
			num++;
		{ // dance around potentially-reallocated text[]
			uintptr_t ofs = q - text;
			size = file_insert(fn, q, 0);
			q = text + ofs;
		}
		if (size < 0)
			goto ret;	// nothing was inserted
		// how many lines in text[]?
		li = count_lines(q, q + size - 1);
		status_line("'%s'"
			IF_FEATURE_VI_READONLY("%s")
			" %uL, %uC",
			fn,
			IF_FEATURE_VI_READONLY((readonly_mode ? " [Readonly]" : ""),)
			li, size
		);
		dot = find_line(num);
	} else if (strncmp(cmd, "rewind", i) == 0) {	// rewind cmd line args
		if (modified_count && !useforce) {
			status_line_bold("No write since last change (:%s! overrides)", cmd);
		} else {
			// reset the filenames to edit
			optind = -1; // start from 0th file
			editing = 0;
		}
# if ENABLE_FEATURE_VI_SET
	} else if (strncmp(cmd, "set", i) == 0) {	// set or clear features
#  if ENABLE_FEATURE_VI_SETOPTS
		char *argp, *argn, oldch;
#  endif
		// only blank is regarded as args delimiter. What about tab '\t'?
		if (!args[0] || strcmp(args, "all") == 0) {
			// print out values of all options
#  if ENABLE_FEATURE_VI_SETOPTS
			status_line_bold(
				"%sautoindent "
				"%sexpandtab "
				"%sflash "
				"%signorecase "
				"%sshowmatch "
				"tabstop=%u",
				autoindent ? "" : "no",
				expandtab ? "" : "no",
				err_method ? "" : "no",
				ignorecase ? "" : "no",
				showmatch ? "" : "no",
				tabstop
			);
#  endif
			goto ret;
		}
#  if ENABLE_FEATURE_VI_SETOPTS
		argp = args;
		while (*argp) {
			i = 0;
			if (argp[0] == 'n' && argp[1] == 'o') // "noXXX"
				i = 2;
			argn = skip_non_whitespace(argp);
			oldch = *argn;
			*argn = '\0';
			setops(argp, i);
			*argn = oldch;
			argp = skip_whitespace(argn);
		}
#  endif /* FEATURE_VI_SETOPTS */
# endif /* FEATURE_VI_SET */

# if ENABLE_FEATURE_VI_SEARCH
	} else if (cmd[0] == 's') {	// substitute a pattern with a replacement pattern
		char *F, *R, *flags;
		size_t len_F, len_R;
		int gflag = 0;		// global replace flag
		int subs = 0;	// number of substitutions
#  if ENABLE_FEATURE_VI_VERBOSE_STATUS
		int last_line = 0, lines = 0;
#  endif
#  if ENABLE_FEATURE_VI_REGEX_SEARCH
		regex_t preg;
		int cflags;
		char *Rorig;
#   if ENABLE_FEATURE_VI_UNDO
		int undo = 0;
#   endif
#  endif

		// F points to the "find" pattern
		// R points to the "replace" pattern
		// replace the cmd line delimiters "/" with NULs
		c = buf[1];	// what is the delimiter
		F = buf + 2;	// start of "find"
		R = strchr_backslash(F, c);	// middle delimiter
		if (!R)
			goto colon_s_fail;
		len_F = R - F;
		*R++ = '\0';	// terminate "find"
		flags = strchr_backslash(R, c);
		if (flags) {
			*flags++ = '\0';	// terminate "replace"
			gflag = *flags;
		}

		if (len_F) {	// save "find" as last search pattern
			free(last_search_pattern);
			last_search_pattern = xstrdup(F - 1);
			last_search_pattern[0] = '/';
		} else if (last_search_pattern[1] == '\0') {
			status_line_bold("No previous search");
			goto ret;
		} else {
			F = last_search_pattern + 1;
			len_F = strlen(F);
		}

		if (!GOT_ADDRESS) {	// no addr given
			q = begin_line(dot);      // start with cur line
			r = end_line(dot);
			b = e = count_lines(text, q); // cur line number
		} else if (!GOT_RANGE) {	// one addr given
			b = e;
		}

#  if ENABLE_FEATURE_VI_REGEX_SEARCH
		Rorig = R;
		cflags = 0;
		if (ignorecase)
			cflags = REG_ICASE;
		memset(&preg, 0, sizeof(preg));
		if (regcomp(&preg, F, cflags) != 0) {
			status_line(":s bad search pattern");
			goto regex_search_end;
		}
#  else
		len_R = strlen(R);
#  endif

		for (i = b; i <= e; i++) {	// so, :20,23 s \0 find \0 replace \0
			char *ls = q;		// orig line start
			char *found;
 vc4:
#  if ENABLE_FEATURE_VI_REGEX_SEARCH
			found = regex_search(q, &preg, Rorig, &len_F, &len_R, &R);
#  else
			found = char_search(q, F, (FORWARD << 1) | LIMITED);	// search cur line only for "find"
#  endif
			if (found) {
				uintptr_t bias;
				// we found the "find" pattern - delete it
				// For undo support, the first item should not be chained
				// This needs to be handled differently depending on
				// whether or not regex support is enabled.
#  if ENABLE_FEATURE_VI_REGEX_SEARCH
#   define TEST_LEN_F len_F	// len_F may be zero
#   define TEST_UNDO1 undo++
#   define TEST_UNDO2 undo++
#  else
#   define TEST_LEN_F 1		// len_F is never zero
#   define TEST_UNDO1 subs
#   define TEST_UNDO2 1
#  endif
				if (TEST_LEN_F)	// match can be empty, no delete needed
					text_hole_delete(found, found + len_F - 1,
								TEST_UNDO1 ? ALLOW_UNDO_CHAIN : ALLOW_UNDO);
				if (len_R != 0) {	// insert the "replace" pattern, if required
					bias = string_insert(found, R,
								TEST_UNDO2 ? ALLOW_UNDO_CHAIN : ALLOW_UNDO);
					found += bias;
					ls += bias;
					//q += bias; - recalculated anyway
				}
#  if ENABLE_FEATURE_VI_REGEX_SEARCH
				free(R);
#  endif
				if (TEST_LEN_F || len_R != 0) {
					dot = ls;
					subs++;
#  if ENABLE_FEATURE_VI_VERBOSE_STATUS
					if (last_line != i) {
						last_line = i;
						++lines;
					}
#  endif
				}
				// check for "global"  :s/foo/bar/g
				if (gflag == 'g') {
					if ((found + len_R) < end_line(ls)) {
						q = found + len_R;
						goto vc4;	// don't let q move past cur line
					}
				}
			}
			q = next_line(ls);
		}
		if (subs == 0) {
			status_line_bold("No match");
		} else {
			dot_skip_over_ws();
#  if ENABLE_FEATURE_VI_VERBOSE_STATUS
			if (subs > 1)
				status_line("%d substitutions on %d lines", subs, lines);
#  endif
		}
#  if ENABLE_FEATURE_VI_REGEX_SEARCH
 regex_search_end:
		regfree(&preg);
#  endif
# endif /* FEATURE_VI_SEARCH */
	} else if (strncmp(cmd, "version", i) == 0) {  // show software version
		status_line(BB_VER);
	} else if (strncmp(cmd, "write", i) == 0  // write text to file
	        || strcmp(cmd, "wq") == 0
	        || strcmp(cmd, "wn") == 0
	        || (cmd[0] == 'x' && !cmd[1])
	) {
		int size;
		//int forced = FALSE;

		// is there a file name to write to?
		if (args[0]) {
			struct stat statbuf;

			exp = expand_args(args);
			if (exp == NULL)
				goto ret;
			if (!useforce && (fn == NULL || strcmp(fn, exp) != 0) &&
					stat(exp, &statbuf) == 0) {
				status_line_bold("File exists (:w! overrides)");
				goto ret;
			}
			fn = exp;
			init_filename(fn);
		}
# if ENABLE_FEATURE_VI_READONLY
		else if (readonly_mode && !useforce && fn) {
			status_line_bold("'%s' is read only", fn);
			goto ret;
		}
# endif
		//if (useforce) {
			// if "fn" is not write-able, chmod u+w
			// sprintf(syscmd, "chmod u+w %s", fn);
			// system(syscmd);
			// forced = TRUE;
		//}
		if (modified_count != 0 || cmd[0] != 'x') {
			size = r - q + 1;
			l = file_write(fn, q, r);
		} else {
			size = 0;
			l = 0;
		}
		//if (useforce && forced) {
			// chmod u-w
			// sprintf(syscmd, "chmod u-w %s", fn);
			// system(syscmd);
			// forced = FALSE;
		//}
		if (l < 0) {
			if (l == -1)
				status_line_bold_errno(fn);
		} else {
			// how many lines written
			li = count_lines(q, q + l - 1);
			status_line("'%s' %uL, %uC", fn, li, l);
			if (l == size) {
				if (q == text && q + l == end) {
					modified_count = 0;
					last_modified_count = -1;
				}
				if (cmd[1] == 'n') {
					editing = 0;
				} else if (cmd[0] == 'x' || cmd[1] == 'q') {
					// are there other files to edit?
					int n = cmdline_filecnt - optind - 1;
					if (n > 0) {
						if (useforce) {
							// force end of argv list
							optind = cmdline_filecnt;
						} else {
							status_line_bold("%u more file(s) to edit", n);
							goto ret;
						}
					}
					editing = 0;
				}
			}
		}
# if ENABLE_FEATURE_VI_YANKMARK
	} else if (strncmp(cmd, "yank", i) == 0) {	// yank lines
		if (!GOT_ADDRESS) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		text_yank(q, r, YDreg, WHOLE);
		li = count_lines(q, r);
		status_line("Yank %d lines (%d chars) into [%c]",
				li, strlen(reg[YDreg]), what_reg());
# endif
	} else {
		// cmd unknown
		not_implemented(cmd);
	}
 ret:
# if ENABLE_FEATURE_VI_COLON_EXPAND
	free(exp);
# endif
	dot = bound_dot(dot);	// make sure "dot" is valid
	return;
# if ENABLE_FEATURE_VI_SEARCH
 colon_s_fail:
	status_line(":s expression missing delimiters");
# endif
#endif /* FEATURE_VI_COLON */
}

//----- Char Routines --------------------------------------------
// Chars that are part of a word-
//    0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
// Chars that are Not part of a word (stoppers)
//    !"#$%&'()*+,-./:;<=>?@[\]^`{|}~
// Chars that are WhiteSpace
//    TAB NEWLINE VT FF RETURN SPACE
// DO NOT COUNT NEWLINE AS WHITESPACE

static int st_test(char *p, int type, int dir, char *tested)
{
	char c, c0, ci;
	int test, inc;

	inc = dir;
	c = c0 = p[0];
	ci = p[inc];
	test = 0;

	if (type == S_BEFORE_WS) {
		c = ci;
		test = (!isspace(c) || c == '\n');
	}
	if (type == S_TO_WS) {
		c = c0;
		test = (!isspace(c) || c == '\n');
	}
	if (type == S_OVER_WS) {
		c = c0;
		test = isspace(c);
	}
	if (type == S_END_PUNCT) {
		c = ci;
		test = ispunct(c);
	}
	if (type == S_END_ALNUM) {
		c = ci;
		test = (isalnum(c) || c == '_');
	}
	*tested = c;
	return test;
}

static char *skip_thing(char *p, int linecnt, int dir, int type)
{
	char c;

	while (st_test(p, type, dir, &c)) {
		// make sure we limit search to correct number of lines
		if (c == '\n' && --linecnt < 1)
			break;
		if (dir >= 0 && p >= end - 1)
			break;
		if (dir < 0 && p <= text)
			break;
		p += dir;		// move to next char
	}
	return p;
}

#if ENABLE_FEATURE_VI_USE_SIGNALS
static void winch_handler(int sig UNUSED_PARAM)
{
	int save_errno = errno;
	// FIXME: do it in main loop!!!
	signal(SIGWINCH, winch_handler);
	query_screen_dimensions();
	new_screen(rows, columns);	// get memory for virtual screen
	redraw(TRUE);		// re-draw the screen
	errno = save_errno;
}
static void tstp_handler(int sig UNUSED_PARAM)
{
	int save_errno = errno;

	// ioctl inside cookmode() was seen to generate SIGTTOU,
	// stopping us too early. Prevent that:
	signal(SIGTTOU, SIG_IGN);

	go_bottom_and_clear_to_eol();
	cookmode(); // terminal to "cooked"

	// stop now
	//signal(SIGTSTP, SIG_DFL);
	//raise(SIGTSTP);
	raise(SIGSTOP); // avoid "dance" with TSTP handler - use SIGSTOP instead
	//signal(SIGTSTP, tstp_handler);

	// we have been "continued" with SIGCONT, restore screen and termios
	rawmode(); // terminal to "raw"
	last_status_cksum = 0; // force status update
	redraw(TRUE); // re-draw the screen

	errno = save_errno;
}
static void int_handler(int sig)
{
	signal(SIGINT, int_handler);
	siglongjmp(restart, sig);
}
#endif /* FEATURE_VI_USE_SIGNALS */

static void do_cmd(int c);

static int at_eof(const char *s)
{
	// does 's' point to end of file, even with no terminating newline?
	return ((s == end - 2 && s[1] == '\n') || s == end - 1);
}

static int find_range(char **start, char **stop, int cmd)
{
	char *p, *q, *t;
	int buftype = -1;
	int c;

	p = q = dot;

#if ENABLE_FEATURE_VI_YANKMARK
	if (cmd == 'Y') {
		c = 'y';
	} else
#endif
	{
		c = get_motion_char();
	}

#if ENABLE_FEATURE_VI_YANKMARK
	if ((cmd == 'Y' || cmd == c) && strchr("cdy><", c)) {
#else
	if (cmd == c && strchr("cd><", c)) {
#endif
		// these cmds operate on whole lines
		buftype = WHOLE;
		if (--cmdcnt > 0) {
			do_cmd('j');
			if (cmd_error)
				buftype = -1;
		}
	} else if (strchr("^%$0bBeEfFtThnN/?|{}\b\177", c)) {
		// Most operate on char positions within a line.  Of those that
		// don't '%' needs no special treatment, search commands are
		// marked as MULTI and  "{}" are handled below.
		buftype = strchr("nN/?", c) ? MULTI : PARTIAL;
		do_cmd(c);		// execute movement cmd
		if (p == dot)	// no movement is an error
			buftype = -1;
	} else if (strchr("wW", c)) {
		buftype = MULTI;
		do_cmd(c);		// execute movement cmd
		// step back one char, but not if we're at end of file,
		// or if we are at EOF and search was for 'w' and we're at
		// the start of a 'W' word.
		if (dot > p && (!at_eof(dot) || (c == 'w' && ispunct(*dot))))
			dot--;
		t = dot;
		// don't include trailing WS as part of word
		while (dot > p && isspace(*dot)) {
			if (*dot-- == '\n')
				t = dot;
		}
		// for non-change operations WS after NL is not part of word
		if (cmd != 'c' && dot != t && *dot != '\n')
			dot = t;
	} else if (strchr("GHL+-gjk'\r\n", c)) {
		// these operate on whole lines
		buftype = WHOLE;
		do_cmd(c);		// execute movement cmd
		if (cmd_error)
			buftype = -1;
	} else if (c == ' ' || c == 'l') {
		// forward motion by character
		int tmpcnt = (cmdcnt ?: 1);
		buftype = PARTIAL;
		do_cmd(c);		// execute movement cmd
		// exclude last char unless range isn't what we expected
		// this indicates we've hit EOL
		if (tmpcnt == dot - p)
			dot--;
	}

	if (buftype == -1) {
		if (c != 27)
			indicate_error();
		return buftype;
	}

	q = dot;
	if (q < p) {
		t = q;
		q = p;
		p = t;
	}

	// movements which don't include end of range
	if (q > p) {
		if (strchr("^0bBFThnN/?|\b\177", c)) {
			q--;
		} else if (strchr("{}", c)) {
			buftype = (p == begin_line(p) && (*q == '\n' || at_eof(q))) ?
							WHOLE : MULTI;
			if (!at_eof(q)) {
				q--;
				if (q > p && p != begin_line(p))
					q--;
			}
		}
	}

	*start = p;
	*stop = q;
	return buftype;
}

//---------------------------------------------------------------------
//----- the Ascii Chart -----------------------------------------------
//  00 nul   01 soh   02 stx   03 etx   04 eot   05 enq   06 ack   07 bel
//  08 bs    09 ht    0a nl    0b vt    0c np    0d cr    0e so    0f si
//  10 dle   11 dc1   12 dc2   13 dc3   14 dc4   15 nak   16 syn   17 etb
//  18 can   19 em    1a sub   1b esc   1c fs    1d gs    1e rs    1f us
//  20 sp    21 !     22 "     23 #     24 $     25 %     26 &     27 '
//  28 (     29 )     2a *     2b +     2c ,     2d -     2e .     2f /
//  30 0     31 1     32 2     33 3     34 4     35 5     36 6     37 7
//  38 8     39 9     3a :     3b ;     3c <     3d =     3e >     3f ?
//  40 @     41 A     42 B     43 C     44 D     45 E     46 F     47 G
//  48 H     49 I     4a J     4b K     4c L     4d M     4e N     4f O
//  50 P     51 Q     52 R     53 S     54 T     55 U     56 V     57 W
//  58 X     59 Y     5a Z     5b [     5c \     5d ]     5e ^     5f _
//  60 `     61 a     62 b     63 c     64 d     65 e     66 f     67 g
//  68 h     69 i     6a j     6b k     6c l     6d m     6e n     6f o
//  70 p     71 q     72 r     73 s     74 t     75 u     76 v     77 w
//  78 x     79 y     7a z     7b {     7c |     7d }     7e ~     7f del
//---------------------------------------------------------------------

//----- Execute a Vi Command -----------------------------------
static void do_cmd(int c)
{
	char *p, *q, *save_dot;
	char buf[12];
	int dir;
	int cnt, i, j;
	int c1;
#if ENABLE_FEATURE_VI_YANKMARK
	char *orig_dot = dot;
#endif
#if ENABLE_FEATURE_VI_UNDO
	int allow_undo = ALLOW_UNDO;
	int undo_del = UNDO_DEL;
#endif

//	c1 = c; // quiet the compiler
//	cnt = yf = 0; // quiet the compiler
//	p = q = save_dot = buf; // quiet the compiler
	memset(buf, '\0', sizeof(buf));
	keep_index = FALSE;
	cmd_error = FALSE;

	show_status_line();

	// if this is a cursor key, skip these checks
	switch (c) {
		case KEYCODE_UP:
		case KEYCODE_DOWN:
		case KEYCODE_LEFT:
		case KEYCODE_RIGHT:
		case KEYCODE_HOME:
		case KEYCODE_END:
		case KEYCODE_PAGEUP:
		case KEYCODE_PAGEDOWN:
		case KEYCODE_DELETE:
			goto key_cmd_mode;
	}

	if (cmd_mode == 2) {
		//  flip-flop Insert/Replace mode
		if (c == KEYCODE_INSERT)
			goto dc_i;
		// we are 'R'eplacing the current *dot with new char
		if (*dot == '\n') {
			// don't Replace past E-o-l
			cmd_mode = 1;	// convert to insert
			undo_queue_commit();
		} else {
			if (1 <= c || Isprint(c)) {
				if (c != 27 && !isbackspace(c))
					dot = yank_delete(dot, dot, PARTIAL, YANKDEL, ALLOW_UNDO);
				dot = char_insert(dot, c, ALLOW_UNDO_CHAIN);
			}
			goto dc1;
		}
	}
	if (cmd_mode == 1) {
		// hitting "Insert" twice means "R" replace mode
		if (c == KEYCODE_INSERT) goto dc5;
		// insert the char c at "dot"
		if (1 <= c || Isprint(c)) {
			dot = char_insert(dot, c, ALLOW_UNDO_QUEUED);
		}
		goto dc1;
	}

 key_cmd_mode:
	switch (c) {
		//case 0x01:	// soh
		//case 0x09:	// ht
		//case 0x0b:	// vt
		//case 0x0e:	// so
		//case 0x0f:	// si
		//case 0x10:	// dle
		//case 0x11:	// dc1
		//case 0x13:	// dc3
#if ENABLE_FEATURE_VI_CRASHME
	case 0x14:			// dc4  ctrl-T
		crashme = (crashme == 0) ? 1 : 0;
		break;
#endif
		//case 0x16:	// syn
		//case 0x17:	// etb
		//case 0x18:	// can
		//case 0x1c:	// fs
		//case 0x1d:	// gs
		//case 0x1e:	// rs
		//case 0x1f:	// us
		//case '!':	// !-
		//case '#':	// #-
		//case '&':	// &-
		//case '(':	// (-
		//case ')':	// )-
		//case '*':	// *-
		//case '=':	// =-
		//case '@':	// @-
		//case 'K':	// K-
		//case 'Q':	// Q-
		//case 'S':	// S-
		//case 'V':	// V-
		//case '[':	// [-
		//case '\\':	// \-
		//case ']':	// ]-
		//case '_':	// _-
		//case '`':	// `-
		//case 'v':	// v-
	default:			// unrecognized command
		buf[0] = c;
		buf[1] = '\0';
		not_implemented(buf);
		end_cmd_q();	// stop adding to q
	case 0x00:			// nul- ignore
		break;
	case 2:			// ctrl-B  scroll up   full screen
	case KEYCODE_PAGEUP:	// Cursor Key Page Up
		dot_scroll(rows - 2, -1);
		break;
	case 4:			// ctrl-D  scroll down half screen
		dot_scroll((rows - 2) / 2, 1);
		break;
	case 5:			// ctrl-E  scroll down one line
		dot_scroll(1, 1);
		break;
	case 6:			// ctrl-F  scroll down full screen
	case KEYCODE_PAGEDOWN:	// Cursor Key Page Down
		dot_scroll(rows - 2, 1);
		break;
	case 7:			// ctrl-G  show current status
		last_status_cksum = 0;	// force status update
		break;
	case 'h':			// h- move left
	case KEYCODE_LEFT:	// cursor key Left
	case 8:		// ctrl-H- move left    (This may be ERASE char)
	case 0x7f:	// DEL- move left   (This may be ERASE char)
		do {
			dot_left();
		} while (--cmdcnt > 0);
		break;
	case 10:			// Newline ^J
	case 'j':			// j- goto next line, same col
	case KEYCODE_DOWN:	// cursor key Down
	case 13:			// Carriage Return ^M
	case '+':			// +- goto next line
		q = dot;
		do {
			p = next_line(q);
			if (p == end_line(q)) {
				indicate_error();
				goto dc1;
			}
			q = p;
		} while (--cmdcnt > 0);
		dot = q;
		if (c == 13 || c == '+') {
			dot_skip_over_ws();
		} else {
			// try to stay in saved column
			dot = cindex == C_END ? end_line(dot) : move_to_col(dot, cindex);
			keep_index = TRUE;
		}
		break;
	case 12:			// ctrl-L  force redraw whole screen
	case 18:			// ctrl-R  force redraw
		redraw(TRUE);	// this will redraw the entire display
		break;
	case 21:			// ctrl-U  scroll up half screen
		dot_scroll((rows - 2) / 2, -1);
		break;
	case 25:			// ctrl-Y  scroll up one line
		dot_scroll(1, -1);
		break;
	case 27:			// esc
		if (cmd_mode == 0)
			indicate_error();
		cmd_mode = 0;	// stop inserting
		undo_queue_commit();
		end_cmd_q();
		last_status_cksum = 0;	// force status update
		break;
	case ' ':			// move right
	case 'l':			// move right
	case KEYCODE_RIGHT:	// Cursor Key Right
		do {
			dot_right();
		} while (--cmdcnt > 0);
		break;
#if ENABLE_FEATURE_VI_YANKMARK
	case '"':			// "- name a register to use for Delete/Yank
		c1 = (get_one_char() | 0x20) - 'a'; // | 0x20 is tolower()
		if ((unsigned)c1 <= 25) { // a-z?
			YDreg = c1;
		} else {
			indicate_error();
		}
		break;
	case '\'':			// '- goto a specific mark
		c1 = (get_one_char() | 0x20);
		if ((unsigned)(c1 - 'a') <= 25) { // a-z?
			c1 = (c1 - 'a');
			// get the b-o-l
			q = mark[c1];
			if (text <= q && q < end) {
				dot = q;
				dot_begin();	// go to B-o-l
				dot_skip_over_ws();
			} else {
				indicate_error();
			}
		} else if (c1 == '\'') {	// goto previous context
			dot = swap_context(dot);	// swap current and previous context
			dot_begin();	// go to B-o-l
			dot_skip_over_ws();
#if ENABLE_FEATURE_VI_YANKMARK
			orig_dot = dot;	// this doesn't update stored contexts
#endif
		} else {
			indicate_error();
		}
		break;
	case 'm':			// m- Mark a line
		// this is really stupid.  If there are any inserts or deletes
		// between text[0] and dot then this mark will not point to the
		// correct location! It could be off by many lines!
		// Well..., at least its quick and dirty.
		c1 = (get_one_char() | 0x20) - 'a';
		if ((unsigned)c1 <= 25) { // a-z?
			// remember the line
			mark[c1] = dot;
		} else {
			indicate_error();
		}
		break;
	case 'P':			// P- Put register before
	case 'p':			// p- put register after
		p = reg[YDreg];
		if (p == NULL) {
			status_line_bold("Nothing in register %c", what_reg());
			break;
		}
		cnt = 0;
		i = cmdcnt ?: 1;
		// are we putting whole lines or strings
		if (regtype[YDreg] == WHOLE) {
			if (c == 'P') {
				dot_begin();	// putting lines- Put above
			}
			else /* if ( c == 'p') */ {
				// are we putting after very last line?
				if (end_line(dot) == (end - 1)) {
					dot = end;	// force dot to end of text[]
				} else {
					dot_next();	// next line, then put before
				}
			}
		} else {
			if (c == 'p')
				dot_right();	// move to right, can move to NL
			// how far to move cursor if register doesn't have a NL
			if (strchr(p, '\n') == NULL)
				cnt = i * strlen(p) - 1;
		}
		do {
			// dot is adjusted if text[] is reallocated so we don't have to
			string_insert(dot, p, allow_undo);	// insert the string
# if ENABLE_FEATURE_VI_UNDO
			allow_undo = ALLOW_UNDO_CHAIN;
# endif
		} while (--cmdcnt > 0);
		dot += cnt;
		dot_skip_over_ws();
# if ENABLE_FEATURE_VI_YANKMARK && ENABLE_FEATURE_VI_VERBOSE_STATUS
		yank_status("Put", p, i);
# endif
		end_cmd_q();	// stop adding to q
		break;
	case 'U':			// U- Undo; replace current line with original version
		if (reg[Ureg] != NULL) {
			p = begin_line(dot);
			q = end_line(dot);
			p = text_hole_delete(p, q, ALLOW_UNDO);	// delete cur line
			p += string_insert(p, reg[Ureg], ALLOW_UNDO_CHAIN);	// insert orig line
			dot = p;
			dot_skip_over_ws();
# if ENABLE_FEATURE_VI_YANKMARK && ENABLE_FEATURE_VI_VERBOSE_STATUS
			yank_status("Undo", reg[Ureg], 1);
# endif
		}
		break;
#endif /* FEATURE_VI_YANKMARK */
#if ENABLE_FEATURE_VI_UNDO
	case 'u':	// u- undo last operation
		undo_pop();
		break;
#endif
	case '$':			// $- goto end of line
	case KEYCODE_END:		// Cursor Key End
		for (;;) {
			dot = end_line(dot);
			if (--cmdcnt <= 0)
				break;
			dot_next();
		}
		cindex = C_END;
		keep_index = TRUE;
		break;
	case '%':			// %- find matching char of pair () [] {}
		for (q = dot; q < end && *q != '\n'; q++) {
			if (strchr("()[]{}", *q) != NULL) {
				// we found half of a pair
				p = find_pair(q, *q);
				if (p == NULL) {
					indicate_error();
				} else {
					dot = p;
				}
				break;
			}
		}
		if (*q == '\n')
			indicate_error();
		break;
	case 'f':			// f- forward to a user specified char
	case 'F':			// F- backward to a user specified char
	case 't':			// t- move to char prior to next x
	case 'T':			// T- move to char after previous x
		last_search_char = get_one_char();	// get the search char
		last_search_cmd = c;
		// fall through
	case ';':			// ;- look at rest of line for last search char
	case ',':           // ,- repeat latest search in opposite direction
		dot_to_char(c != ',' ? last_search_cmd : last_search_cmd ^ 0x20);
		break;
#if ENABLE_FEATURE_VI_DOT_CMD
	case '.':			// .- repeat the last modifying command
		// Stuff the last_modifying_cmd back into stdin
		// and let it be re-executed.
		if (lmc_len != 0) {
			if (cmdcnt)	// update saved count if current count is non-zero
				dotcnt = cmdcnt;
			last_modifying_cmd[lmc_len] = '\0';
			ioq = ioq_start = xasprintf("%u%s", dotcnt, last_modifying_cmd);
		}
		break;
#endif
#if ENABLE_FEATURE_VI_SEARCH
	case 'N':			// N- backward search for last pattern
		dir = last_search_pattern[0] == '/' ? BACK : FORWARD;
		goto dc4;		// now search for pattern
		break;
	case '?':			// ?- backward search for a pattern
	case '/':			// /- forward search for a pattern
		buf[0] = c;
		buf[1] = '\0';
		q = get_input_line(buf);	// get input line- use "status line"
		if (!q[0])	// user changed mind and erased the "/"-  do nothing
			break;
		if (!q[1]) {	// if no pat re-use old pat
			if (last_search_pattern[0])
				last_search_pattern[0] = c;
		} else {	// strlen(q) > 1: new pat- save it and find
			free(last_search_pattern);
			last_search_pattern = xstrdup(q);
		}
		// fall through
	case 'n':			// n- repeat search for last pattern
		// search rest of text[] starting at next char
		// if search fails "dot" is unchanged
		dir = last_search_pattern[0] == '/' ? FORWARD : BACK;
 dc4:
		if (last_search_pattern[1] == '\0') {
			status_line_bold("No previous search");
			break;
		}
		do {
			q = char_search(dot + dir, last_search_pattern + 1,
						(dir << 1) | FULL);
			if (q != NULL) {
				dot = q;	// good search, update "dot"
			} else {
				// no pattern found between "dot" and top/bottom of file
				// continue from other end of file
				const char *msg;
				q = char_search(dir == FORWARD ? text : end - 1,
						last_search_pattern + 1, (dir << 1) | FULL);
				if (q != NULL) {	// found something
					dot = q;	// found new pattern- goto it
					msg = "search hit %s, continuing at %s";
				} else {	// pattern is nowhere in file
					cmdcnt = 0;	// force exit from loop
					msg = "Pattern not found";
				}
				if (dir == FORWARD)
					status_line_bold(msg, "BOTTOM", "TOP");
				else
					status_line_bold(msg, "TOP", "BOTTOM");
			}
		} while (--cmdcnt > 0);
		break;
	case '{':			// {- move backward paragraph
	case '}':			// }- move forward paragraph
		dir = c == '}' ? FORWARD : BACK;
		do {
			int skip = TRUE; // initially skip consecutive empty lines
			while (dir == FORWARD ? dot < end - 1 : dot > text) {
				if (*dot == '\n' && dot[dir] == '\n') {
					if (!skip) {
						if (dir == FORWARD)
							++dot;	// move to next blank line
						goto dc2;
					}
				}
				else {
					skip = FALSE;
				}
				dot += dir;
			}
			goto dc6; // end of file
 dc2:		continue;
		} while (--cmdcnt > 0);
		break;
#endif /* FEATURE_VI_SEARCH */
	case '0':			// 0- goto beginning of line
	case '1':			// 1-
	case '2':			// 2-
	case '3':			// 3-
	case '4':			// 4-
	case '5':			// 5-
	case '6':			// 6-
	case '7':			// 7-
	case '8':			// 8-
	case '9':			// 9-
		if (c == '0' && cmdcnt < 1) {
			dot_begin();	// this was a standalone zero
		} else {
			cmdcnt = cmdcnt * 10 + (c - '0');	// this 0 is part of a number
		}
		break;
	case ':':			// :- the colon mode commands
		p = get_input_line(":");	// get input line- use "status line"
		colon(p);		// execute the command
		break;
	case '<':			// <- Left  shift something
	case '>':			// >- Right shift something
		cnt = count_lines(text, dot);	// remember what line we are on
		if (find_range(&p, &q, c) == -1)
			goto dc6;
		i = count_lines(p, q);	// # of lines we are shifting
		for (p = begin_line(p); i > 0; i--, p = next_line(p)) {
			if (c == '<') {
				// shift left- remove tab or tabstop spaces
				if (*p == '\t') {
					// shrink buffer 1 char
					text_hole_delete(p, p, allow_undo);
				} else if (*p == ' ') {
					// we should be calculating columns, not just SPACE
					for (j = 0; *p == ' ' && j < tabstop; j++) {
						text_hole_delete(p, p, allow_undo);
#if ENABLE_FEATURE_VI_UNDO
						allow_undo = ALLOW_UNDO_CHAIN;
#endif
					}
				}
			} else if (/* c == '>' && */ p != end_line(p)) {
				// shift right -- add tab or tabstop spaces on non-empty lines
				char_insert(p, '\t', allow_undo);
			}
#if ENABLE_FEATURE_VI_UNDO
			allow_undo = ALLOW_UNDO_CHAIN;
#endif
		}
		dot = find_line(cnt);	// what line were we on
		dot_skip_over_ws();
		end_cmd_q();	// stop adding to q
		break;
	case 'A':			// A- append at e-o-l
		dot_end();		// go to e-o-l
		//**** fall through to ... 'a'
	case 'a':			// a- append after current char
		if (*dot != '\n')
			dot++;
		goto dc_i;
		break;
	case 'B':			// B- back a blank-delimited Word
	case 'E':			// E- end of a blank-delimited word
	case 'W':			// W- forward a blank-delimited word
		dir = FORWARD;
		if (c == 'B')
			dir = BACK;
		do {
			if (c == 'W' || isspace(dot[dir])) {
				dot = skip_thing(dot, 1, dir, S_TO_WS);
				dot = skip_thing(dot, 2, dir, S_OVER_WS);
			}
			if (c != 'W')
				dot = skip_thing(dot, 1, dir, S_BEFORE_WS);
		} while (--cmdcnt > 0);
		break;
	case 'C':			// C- Change to e-o-l
	case 'D':			// D- delete to e-o-l
		save_dot = dot;
		dot = dollar_line(dot);	// move to before NL
		// copy text into a register and delete
		dot = yank_delete(save_dot, dot, PARTIAL, YANKDEL, ALLOW_UNDO);	// delete to e-o-l
		if (c == 'C')
			goto dc_i;	// start inserting
#if ENABLE_FEATURE_VI_DOT_CMD
		if (c == 'D')
			end_cmd_q();	// stop adding to q
#endif
		break;
	case 'g': // 'gg' goto a line number (vim) (default: very first line)
		c1 = get_one_char();
		if (c1 != 'g') {
			buf[0] = 'g';
			// c1 < 0 if the key was special. Try "g<up-arrow>"
			// TODO: if Unicode?
			buf[1] = (c1 >= 0 ? c1 : '*');
			buf[2] = '\0';
			not_implemented(buf);
			cmd_error = TRUE;
			break;
		}
		if (cmdcnt == 0)
			cmdcnt = 1;
		// fall through
	case 'G':		// G- goto to a line number (default= E-O-F)
		dot = end - 1;				// assume E-O-F
		if (cmdcnt > 0) {
			dot = find_line(cmdcnt);	// what line is #cmdcnt
		}
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'H':			// H- goto top line on screen
		dot = screenbegin;
		if (cmdcnt > (rows - 1)) {
			cmdcnt = (rows - 1);
		}
		while (--cmdcnt > 0) {
			dot_next();
		}
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'I':			// I- insert before first non-blank
		dot_begin();	// 0
		dot_skip_over_ws();
		//**** fall through to ... 'i'
	case 'i':			// i- insert before current char
	case KEYCODE_INSERT:	// Cursor Key Insert
 dc_i:
#if ENABLE_FEATURE_VI_SETOPTS
		newindent = -1;
#endif
		cmd_mode = 1;	// start inserting
		undo_queue_commit();	// commit queue when cmd_mode changes
		break;
	case 'J':			// J- join current and next lines together
		do {
			dot_end();		// move to NL
			if (dot < end - 1) {	// make sure not last char in text[]
#if ENABLE_FEATURE_VI_UNDO
				undo_push(dot, 1, UNDO_DEL);
				*dot++ = ' ';	// replace NL with space
				undo_push((dot - 1), 1, UNDO_INS_CHAIN);
#else
				*dot++ = ' ';
				modified_count++;
#endif
				while (isblank(*dot)) {	// delete leading WS
					text_hole_delete(dot, dot, ALLOW_UNDO_CHAIN);
				}
			}
		} while (--cmdcnt > 0);
		end_cmd_q();	// stop adding to q
		break;
	case 'L':			// L- goto bottom line on screen
		dot = end_screen();
		if (cmdcnt > (rows - 1)) {
			cmdcnt = (rows - 1);
		}
		while (--cmdcnt > 0) {
			dot_prev();
		}
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'M':			// M- goto middle line on screen
		dot = screenbegin;
		for (cnt = 0; cnt < (rows-1) / 2; cnt++)
			dot = next_line(dot);
		dot_skip_over_ws();
		break;
	case 'O':			// O- open an empty line above
		dot_begin();
#if ENABLE_FEATURE_VI_SETOPTS
		// special case: use indent of current line
		newindent = get_column(dot + indent_len(dot));
#endif
		goto dc3;
	case 'o':			// o- open an empty line below
		dot_end();
 dc3:
#if ENABLE_FEATURE_VI_SETOPTS
		cmd_mode = 1;	// switch to insert mode early
#endif
		dot = char_insert(dot, '\n', ALLOW_UNDO);
		if (c == 'O' && !autoindent) {
			// done in char_insert() for 'O'+autoindent
			dot_prev();
		}
		goto dc_i;
		break;
	case 'R':			// R- continuous Replace char
 dc5:
		cmd_mode = 2;
		undo_queue_commit();
		rstart = dot;
		break;
	case KEYCODE_DELETE:
		if (dot < end - 1)
			dot = yank_delete(dot, dot, PARTIAL, YANKDEL, ALLOW_UNDO);
		break;
	case 'X':			// X- delete char before dot
	case 'x':			// x- delete the current char
	case 's':			// s- substitute the current char
		dir = 0;
		if (c == 'X')
			dir = -1;
		do {
			if (dot[dir] != '\n') {
				if (c == 'X')
					dot--;	// delete prev char
				dot = yank_delete(dot, dot, PARTIAL, YANKDEL, allow_undo);	// delete char
#if ENABLE_FEATURE_VI_UNDO
				allow_undo = ALLOW_UNDO_CHAIN;
#endif
			}
		} while (--cmdcnt > 0);
		end_cmd_q();	// stop adding to q
		if (c == 's')
			goto dc_i;	// start inserting
		break;
	case 'Z':			// Z- if modified, {write}; exit
		c1 = get_one_char();
		// ZQ means to exit without saving
		if (c1 == 'Q') {
			editing = 0;
			optind = cmdline_filecnt;
			break;
		}
		// ZZ means to save file (if necessary), then exit
		if (c1 != 'Z') {
			indicate_error();
			break;
		}
		if (modified_count) {
			if (ENABLE_FEATURE_VI_READONLY && readonly_mode && current_filename) {
				status_line_bold("'%s' is read only", current_filename);
				break;
			}
			cnt = file_write(current_filename, text, end - 1);
			if (cnt < 0) {
				if (cnt == -1)
					status_line_bold("Write error: "STRERROR_FMT STRERROR_ERRNO);
			} else if (cnt == (end - 1 - text + 1)) {
				editing = 0;
			}
		} else {
			editing = 0;
		}
		// are there other files to edit?
		j = cmdline_filecnt - optind - 1;
		if (editing == 0 && j > 0) {
			editing = 1;
			modified_count = 0;
			last_modified_count = -1;
			status_line_bold("%u more file(s) to edit", j);
		}
		break;
	case '^':			// ^- move to first non-blank on line
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'b':			// b- back a word
	case 'e':			// e- end of word
		dir = FORWARD;
		if (c == 'b')
			dir = BACK;
		do {
			if ((dot + dir) < text || (dot + dir) > end - 1)
				break;
			dot += dir;
			if (isspace(*dot)) {
				dot = skip_thing(dot, (c == 'e') ? 2 : 1, dir, S_OVER_WS);
			}
			if (isalnum(*dot) || *dot == '_') {
				dot = skip_thing(dot, 1, dir, S_END_ALNUM);
			} else if (ispunct(*dot)) {
				dot = skip_thing(dot, 1, dir, S_END_PUNCT);
			}
		} while (--cmdcnt > 0);
		break;
	case 'c':			// c- change something
	case 'd':			// d- delete something
#if ENABLE_FEATURE_VI_YANKMARK
	case 'y':			// y- yank   something
	case 'Y':			// Y- Yank a line
#endif
	{
		int yf = YANKDEL;	// assume either "c" or "d"
		int buftype;
#if ENABLE_FEATURE_VI_YANKMARK
# if ENABLE_FEATURE_VI_VERBOSE_STATUS
		char *savereg = reg[YDreg];
# endif
		if (c == 'y' || c == 'Y')
			yf = YANKONLY;
#endif
		// determine range, and whether it spans lines
		buftype = find_range(&p, &q, c);
		if (buftype == -1)	// invalid range
			goto dc6;
		if (buftype == WHOLE) {
			save_dot = p;	// final cursor position is start of range
			p = begin_line(p);
#if ENABLE_FEATURE_VI_SETOPTS
			if (c == 'c')	// special case: use indent of current line
				newindent = get_column(p + indent_len(p));
#endif
			q = end_line(q);
		}
		dot = yank_delete(p, q, buftype, yf, ALLOW_UNDO);	// delete word
		if (buftype == WHOLE) {
			if (c == 'c') {
#if ENABLE_FEATURE_VI_SETOPTS
				cmd_mode = 1;	// switch to insert mode early
#endif
				dot = char_insert(dot, '\n', ALLOW_UNDO_CHAIN);
				// on the last line of file don't move to prev line,
				// handled in char_insert() if autoindent is enabled
				if (dot != (end-1) && !autoindent) {
					dot_prev();
				}
			} else if (c == 'd') {
				dot_begin();
				dot_skip_over_ws();
			} else {
				dot = save_dot;
			}
		}
		// if CHANGING, not deleting, start inserting after the delete
		if (c == 'c') {
			goto dc_i;	// start inserting
		}
#if ENABLE_FEATURE_VI_YANKMARK && ENABLE_FEATURE_VI_VERBOSE_STATUS
		// only update status if a yank has actually happened
		if (reg[YDreg] != savereg)
			yank_status(c == 'd' ? "Delete" : "Yank", reg[YDreg], 1);
#endif
 dc6:
		end_cmd_q();	// stop adding to q
		break;
	}
	case 'k':			// k- goto prev line, same col
	case KEYCODE_UP:		// cursor key Up
	case '-':			// -- goto prev line
		q = dot;
		do {
			p = prev_line(q);
			if (p == begin_line(q)) {
				indicate_error();
				goto dc1;
			}
			q = p;
		} while (--cmdcnt > 0);
		dot = q;
		if (c == '-') {
			dot_skip_over_ws();
		} else {
			// try to stay in saved column
			dot = cindex == C_END ? end_line(dot) : move_to_col(dot, cindex);
			keep_index = TRUE;
		}
		break;
	case 'r':			// r- replace the current char with user input
		c1 = get_one_char();	// get the replacement char
		if (c1 != 27) {
			if (end_line(dot) - dot < (cmdcnt ?: 1)) {
				indicate_error();
				goto dc6;
			}
			do {
				dot = text_hole_delete(dot, dot, allow_undo);
#if ENABLE_FEATURE_VI_UNDO
				allow_undo = ALLOW_UNDO_CHAIN;
#endif
				dot = char_insert(dot, c1, allow_undo);
			} while (--cmdcnt > 0);
			dot_left();
		}
		end_cmd_q();	// stop adding to q
		break;
	case 'w':			// w- forward a word
		do {
			if (isalnum(*dot) || *dot == '_') {	// we are on ALNUM
				dot = skip_thing(dot, 1, FORWARD, S_END_ALNUM);
			} else if (ispunct(*dot)) {	// we are on PUNCT
				dot = skip_thing(dot, 1, FORWARD, S_END_PUNCT);
			}
			if (dot < end - 1)
				dot++;		// move over word
			if (isspace(*dot)) {
				dot = skip_thing(dot, 2, FORWARD, S_OVER_WS);
			}
		} while (--cmdcnt > 0);
		break;
	case 'z':			// z-
		c1 = get_one_char();	// get the replacement char
		cnt = 0;
		if (c1 == '.')
			cnt = (rows - 2) / 2;	// put dot at center
		if (c1 == '-')
			cnt = rows - 2;	// put dot at bottom
		screenbegin = begin_line(dot);	// start dot at top
		dot_scroll(cnt, -1);
		break;
	case '|':			// |- move to column "cmdcnt"
		dot = move_to_col(dot, cmdcnt - 1);	// try to move to column
		break;
	case '~':			// ~- flip the case of letters   a-z -> A-Z
		do {
#if ENABLE_FEATURE_VI_UNDO
			if (isalpha(*dot)) {
				undo_push(dot, 1, undo_del);
				*dot = islower(*dot) ? toupper(*dot) : tolower(*dot);
				undo_push(dot, 1, UNDO_INS_CHAIN);
				undo_del = UNDO_DEL_CHAIN;
			}
#else
			if (islower(*dot)) {
				*dot = toupper(*dot);
				modified_count++;
			} else if (isupper(*dot)) {
				*dot = tolower(*dot);
				modified_count++;
			}
#endif
			dot_right();
		} while (--cmdcnt > 0);
		end_cmd_q();	// stop adding to q
		break;
		//----- The Cursor and Function Keys -----------------------------
	case KEYCODE_HOME:	// Cursor Key Home
		dot_begin();
		break;
		// The Fn keys could point to do_macro which could translate them
#if 0
	case KEYCODE_FUN1:	// Function Key F1
	case KEYCODE_FUN2:	// Function Key F2
	case KEYCODE_FUN3:	// Function Key F3
	case KEYCODE_FUN4:	// Function Key F4
	case KEYCODE_FUN5:	// Function Key F5
	case KEYCODE_FUN6:	// Function Key F6
	case KEYCODE_FUN7:	// Function Key F7
	case KEYCODE_FUN8:	// Function Key F8
	case KEYCODE_FUN9:	// Function Key F9
	case KEYCODE_FUN10:	// Function Key F10
	case KEYCODE_FUN11:	// Function Key F11
	case KEYCODE_FUN12:	// Function Key F12
		break;
#endif
	}

 dc1:
	// if text[] just became empty, add back an empty line
	if (end == text) {
		char_insert(text, '\n', NO_UNDO);	// start empty buf with dummy line
		dot = text;
	}
	// it is OK for dot to exactly equal to end, otherwise check dot validity
	if (dot != end) {
		dot = bound_dot(dot);	// make sure "dot" is valid
	}
#if ENABLE_FEATURE_VI_YANKMARK
	if (dot != orig_dot)
		check_context(c);	// update the current context
#endif

	if (!isdigit(c))
		cmdcnt = 0;		// cmd was not a number, reset cmdcnt
	cnt = dot - begin_line(dot);
	// Try to stay off of the Newline
	if (*dot == '\n' && cnt > 0 && cmd_mode == 0)
		dot--;
}

// NB!  the CRASHME code is unmaintained, and doesn't currently build
#if ENABLE_FEATURE_VI_CRASHME
static int totalcmds = 0;
static int Mp = 85;             // Movement command Probability
static int Np = 90;             // Non-movement command Probability
static int Dp = 96;             // Delete command Probability
static int Ip = 97;             // Insert command Probability
static int Yp = 98;             // Yank command Probability
static int Pp = 99;             // Put command Probability
static int M = 0, N = 0, I = 0, D = 0, Y = 0, P = 0, U = 0;
static const char chars[20] = "\t012345 abcdABCD-=.$";
static const char *const words[20] = {
	"this", "is", "a", "test",
	"broadcast", "the", "emergency", "of",
	"system", "quick", "brown", "fox",
	"jumped", "over", "lazy", "dogs",
	"back", "January", "Febuary", "March"
};
static const char *const lines[20] = {
	"You should have received a copy of the GNU General Public License\n",
	"char c, cm, *cmd, *cmd1;\n",
	"generate a command by percentages\n",
	"Numbers may be typed as a prefix to some commands.\n",
	"Quit, discarding changes!\n",
	"Forced write, if permission originally not valid.\n",
	"In general, any ex or ed command (such as substitute or delete).\n",
	"I have tickets available for the Blazers vs LA Clippers for Monday, Janurary 1 at 1:00pm.\n",
	"Please get w/ me and I will go over it with you.\n",
	"The following is a list of scheduled, committed changes.\n",
	"1.   Launch Norton Antivirus (Start, Programs, Norton Antivirus)\n",
	"Reminder....Town Meeting in Central Perk cafe today at 3:00pm.\n",
	"Any question about transactions please contact Sterling Huxley.\n",
	"I will try to get back to you by Friday, December 31.\n",
	"This Change will be implemented on Friday.\n",
	"Let me know if you have problems accessing this;\n",
	"Sterling Huxley recently added you to the access list.\n",
	"Would you like to go to lunch?\n",
	"The last command will be automatically run.\n",
	"This is too much english for a computer geek.\n",
};
static char *multilines[20] = {
	"You should have received a copy of the GNU General Public License\n",
	"char c, cm, *cmd, *cmd1;\n",
	"generate a command by percentages\n",
	"Numbers may be typed as a prefix to some commands.\n",
	"Quit, discarding changes!\n",
	"Forced write, if permission originally not valid.\n",
	"In general, any ex or ed command (such as substitute or delete).\n",
	"I have tickets available for the Blazers vs LA Clippers for Monday, Janurary 1 at 1:00pm.\n",
	"Please get w/ me and I will go over it with you.\n",
	"The following is a list of scheduled, committed changes.\n",
	"1.   Launch Norton Antivirus (Start, Programs, Norton Antivirus)\n",
	"Reminder....Town Meeting in Central Perk cafe today at 3:00pm.\n",
	"Any question about transactions please contact Sterling Huxley.\n",
	"I will try to get back to you by Friday, December 31.\n",
	"This Change will be implemented on Friday.\n",
	"Let me know if you have problems accessing this;\n",
	"Sterling Huxley recently added you to the access list.\n",
	"Would you like to go to lunch?\n",
	"The last command will be automatically run.\n",
	"This is too much english for a computer geek.\n",
};

// create a random command to execute
static void crash_dummy()
{
	static int sleeptime;   // how long to pause between commands
	char c, cm, *cmd, *cmd1;
	int i, cnt, thing, rbi, startrbi, percent;

	// "dot" movement commands
	cmd1 = " \n\r\002\004\005\006\025\0310^$-+wWeEbBhjklHL";

	// is there already a command running?
	if (readbuffer[0] > 0)
		goto cd1;
 cd0:
	readbuffer[0] = 'X';
	startrbi = rbi = 1;
	sleeptime = 0;          // how long to pause between commands
	memset(readbuffer, '\0', sizeof(readbuffer));
	// generate a command by percentages
	percent = (int) lrand48() % 100;        // get a number from 0-99
	if (percent < Mp) {     //  Movement commands
		// available commands
		cmd = cmd1;
		M++;
	} else if (percent < Np) {      //  non-movement commands
		cmd = "mz<>\'\"";       // available commands
		N++;
	} else if (percent < Dp) {      //  Delete commands
		cmd = "dx";             // available commands
		D++;
	} else if (percent < Ip) {      //  Inset commands
		cmd = "iIaAsrJ";        // available commands
		I++;
	} else if (percent < Yp) {      //  Yank commands
		cmd = "yY";             // available commands
		Y++;
	} else if (percent < Pp) {      //  Put commands
		cmd = "pP";             // available commands
		P++;
	} else {
		// We do not know how to handle this command, try again
		U++;
		goto cd0;
	}
	// randomly pick one of the available cmds from "cmd[]"
	i = (int) lrand48() % strlen(cmd);
	cm = cmd[i];
	if (strchr(":\024", cm))
		goto cd0;               // dont allow colon or ctrl-T commands
	readbuffer[rbi++] = cm; // put cmd into input buffer

	// now we have the command-
	// there are 1, 2, and multi char commands
	// find out which and generate the rest of command as necessary
	if (strchr("dmryz<>\'\"", cm)) {        // 2-char commands
		cmd1 = " \n\r0$^-+wWeEbBhjklHL";
		if (cm == 'm' || cm == '\'' || cm == '\"') {    // pick a reg[]
			cmd1 = "abcdefghijklmnopqrstuvwxyz";
		}
		thing = (int) lrand48() % strlen(cmd1); // pick a movement command
		c = cmd1[thing];
		readbuffer[rbi++] = c;  // add movement to input buffer
	}
	if (strchr("iIaAsc", cm)) {     // multi-char commands
		if (cm == 'c') {
			// change some thing
			thing = (int) lrand48() % strlen(cmd1); // pick a movement command
			c = cmd1[thing];
			readbuffer[rbi++] = c;  // add movement to input buffer
		}
		thing = (int) lrand48() % 4;    // what thing to insert
		cnt = (int) lrand48() % 10;     // how many to insert
		for (i = 0; i < cnt; i++) {
			if (thing == 0) {       // insert chars
				readbuffer[rbi++] = chars[((int) lrand48() % strlen(chars))];
			} else if (thing == 1) {        // insert words
				strcat(readbuffer, words[(int) lrand48() % 20]);
				strcat(readbuffer, " ");
				sleeptime = 0;  // how fast to type
			} else if (thing == 2) {        // insert lines
				strcat(readbuffer, lines[(int) lrand48() % 20]);
				sleeptime = 0;  // how fast to type
			} else {        // insert multi-lines
				strcat(readbuffer, multilines[(int) lrand48() % 20]);
				sleeptime = 0;  // how fast to type
			}
		}
		strcat(readbuffer, ESC);
	}
	readbuffer[0] = strlen(readbuffer + 1);
 cd1:
	totalcmds++;
	if (sleeptime > 0)
		mysleep(sleeptime);      // sleep 1/100 sec
}

// test to see if there are any errors
static void crash_test()
{
	static time_t oldtim;

	time_t tim;
	char d[2], msg[80];

	msg[0] = '\0';
	if (end < text) {
		strcat(msg, "end<text ");
	}
	if (end > textend) {
		strcat(msg, "end>textend ");
	}
	if (dot < text) {
		strcat(msg, "dot<text ");
	}
	if (dot > end) {
		strcat(msg, "dot>end ");
	}
	if (screenbegin < text) {
		strcat(msg, "screenbegin<text ");
	}
	if (screenbegin > end - 1) {
		strcat(msg, "screenbegin>end-1 ");
	}

	if (msg[0]) {
		printf("\n\n%d: \'%c\' %s\n\n\n%s[Hit return to continue]%s",
			totalcmds, last_input_char, msg, ESC_BOLD_TEXT, ESC_NORM_TEXT);
		fflush_all();
		while (safe_read(STDIN_FILENO, d, 1) > 0) {
			if (d[0] == '\n' || d[0] == '\r')
				break;
		}
	}
	tim = time(NULL);
	if (tim >= (oldtim + 3)) {
		sprintf(status_buffer,
				"Tot=%d: M=%d N=%d I=%d D=%d Y=%d P=%d U=%d size=%d",
				totalcmds, M, N, I, D, Y, P, U, end - text + 1);
		oldtim = tim;
	}
}
#endif

#if ENABLE_FEATURE_VI_COLON
static void run_cmds(char *p)
{
	while (p) {
		char *q = p;
		p = strchr(q, '\n');
		if (p)
			while (*p == '\n')
				*p++ = '\0';
		if (strlen(q) < MAX_INPUT_LEN)
			colon(q);
	}
}
#endif

static void edit_file(char *fn)
{
#if ENABLE_FEATURE_VI_YANKMARK
#define cur_line edit_file__cur_line
#endif
	int c;
#if ENABLE_FEATURE_VI_USE_SIGNALS
	int sig;
#endif

	editing = 1;	// 0 = exit, 1 = one file, 2 = multiple files
	rawmode();
	rows = 24;
	columns = 80;
	IF_FEATURE_VI_ASK_TERMINAL(G.get_rowcol_error =) query_screen_dimensions();
#if ENABLE_FEATURE_VI_ASK_TERMINAL
	if (G.get_rowcol_error /* TODO? && no input on stdin */) {
		uint64_t k;
		write1(ESC"[999;999H" ESC"[6n");
		fflush_all();
		k = safe_read_key(STDIN_FILENO, readbuffer, /*timeout_ms:*/ 100);
		if ((int32_t)k == KEYCODE_CURSOR_POS) {
			uint32_t rc = (k >> 32);
			columns = (rc & 0x7fff);
			if (columns > MAX_SCR_COLS)
				columns = MAX_SCR_COLS;
			rows = ((rc >> 16) & 0x7fff);
			if (rows > MAX_SCR_ROWS)
				rows = MAX_SCR_ROWS;
		}
	}
#endif
	new_screen(rows, columns);	// get memory for virtual screen
	init_text_buffer(fn);

#if ENABLE_FEATURE_VI_YANKMARK
	YDreg = 26;			// default Yank/Delete reg
//	Ureg = 27; - const		// hold orig line for "U" cmd
	mark[26] = mark[27] = text;	// init "previous context"
#endif

#if ENABLE_FEATURE_VI_CRASHME
	last_input_char = '\0';
#endif
	crow = 0;
	ccol = 0;

#if ENABLE_FEATURE_VI_USE_SIGNALS
	signal(SIGWINCH, winch_handler);
	signal(SIGTSTP, tstp_handler);
	sig = sigsetjmp(restart, 1);
	if (sig != 0) {
		screenbegin = dot = text;
	}
	// int_handler() can jump to "restart",
	// must install handler *after* initializing "restart"
	signal(SIGINT, int_handler);
#endif

	cmd_mode = 0;		// 0=command  1=insert  2='R'eplace
	cmdcnt = 0;
	offset = 0;			// no horizontal offset
	c = '\0';
#if ENABLE_FEATURE_VI_DOT_CMD
	free(ioq_start);
	ioq_start = NULL;
	adding2q = 0;
#endif

#if ENABLE_FEATURE_VI_COLON
	while (initial_cmds)
		run_cmds((char *)llist_pop(&initial_cmds));
#endif
	redraw(FALSE);			// dont force every col re-draw
	//------This is the main Vi cmd handling loop -----------------------
	while (editing > 0) {
#if ENABLE_FEATURE_VI_CRASHME
		if (crashme > 0) {
			if ((end - text) > 1) {
				crash_dummy();	// generate a random command
			} else {
				crashme = 0;
				string_insert(text, "\n\n#####  Ran out of text to work on.  #####\n\n", NO_UNDO);
				dot = text;
				refresh(FALSE);
			}
		}
#endif
		c = get_one_char();	// get a cmd from user
#if ENABLE_FEATURE_VI_CRASHME
		last_input_char = c;
#endif
#if ENABLE_FEATURE_VI_YANKMARK
		// save a copy of the current line- for the 'U" command
		if (begin_line(dot) != cur_line) {
			cur_line = begin_line(dot);
			text_yank(begin_line(dot), end_line(dot), Ureg, PARTIAL);
		}
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
		// If c is a command that changes text[],
		// (re)start remembering the input for the "." command.
		if (!adding2q
		 && ioq_start == NULL
		 && cmd_mode == 0 // command mode
		 && c > '\0' // exclude NUL and non-ASCII chars
		 && c < 0x7f // (Unicode and such)
		 && strchr(modifying_cmds, c)
		) {
			start_new_cmd_q(c);
		}
#endif
		do_cmd(c);		// execute the user command

		// poll to see if there is input already waiting. if we are
		// not able to display output fast enough to keep up, skip
		// the display update until we catch up with input.
		if (!readbuffer[0] && mysleep(0) == 0) {
			// no input pending - so update output
			refresh(FALSE);
			show_status_line();
		}
#if ENABLE_FEATURE_VI_CRASHME
		if (crashme > 0)
			crash_test();	// test editor variables
#endif
	}
	//-------------------------------------------------------------------

	go_bottom_and_clear_to_eol();
	cookmode();
#undef cur_line
}

#define VI_OPTSTR \
	IF_FEATURE_VI_CRASHME("C") \
	IF_FEATURE_VI_COLON("c:*") \
	"Hh" \
	IF_FEATURE_VI_READONLY("R")

enum {
	IF_FEATURE_VI_CRASHME(OPTBIT_C,)
	IF_FEATURE_VI_COLON(OPTBIT_c,)
	OPTBIT_H,
	OPTBIT_h,
	IF_FEATURE_VI_READONLY(OPTBIT_R,)
	OPT_C = IF_FEATURE_VI_CRASHME(	(1 << OPTBIT_C)) + 0,
	OPT_c = IF_FEATURE_VI_COLON(	(1 << OPTBIT_c)) + 0,
	OPT_H = 1 << OPTBIT_H,
	OPT_h = 1 << OPTBIT_h,
	OPT_R = IF_FEATURE_VI_READONLY(	(1 << OPTBIT_R)) + 0,
};

int vi_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int vi_main(int argc, char **argv)
{
	int opts;

	INIT_G();

#if ENABLE_FEATURE_VI_UNDO
	//undo_stack_tail = NULL; - already is
# if ENABLE_FEATURE_VI_UNDO_QUEUE
	undo_queue_state = UNDO_EMPTY;
	//undo_q = 0; - already is
# endif
#endif

#if ENABLE_FEATURE_VI_CRASHME
	srand((long) getpid());
#endif
#ifdef NO_SUCH_APPLET_YET
	// if we aren't "vi", we are "view"
	if (ENABLE_FEATURE_VI_READONLY && applet_name[2]) {
		SET_READONLY_MODE(readonly_mode);
	}
#endif

	// 0: all of our options are disabled by default in vim
	//vi_setops = 0;
	opts = getopt32(argv, VI_OPTSTR IF_FEATURE_VI_COLON(, &initial_cmds));

#if ENABLE_FEATURE_VI_CRASHME
	if (opts & OPT_C)
		crashme = 1;
#endif
	if (opts & OPT_R)
		SET_READONLY_MODE(readonly_mode);
	if (opts & OPT_H)
		show_help();
	if (opts & (OPT_H | OPT_h)) {
		bb_show_usage();
		return 1;
	}

	argv += optind;
	cmdline_filecnt = argc - optind;

	//  1-  process EXINIT variable from environment
	//  2-  if EXINIT is unset process $HOME/.exrc file
	//  3-  process command line args
#if ENABLE_FEATURE_VI_COLON
	{
		const char *exinit = getenv("EXINIT");
		char *cmds = NULL;

		if (exinit) {
			cmds = xstrdup(exinit);
		} else {
			const char *home = getenv("HOME");

			if (home && *home) {
				char *exrc = concat_path_file(home, ".exrc");
				struct stat st;

				// .exrc must belong to and only be writable by user
				if (stat(exrc, &st) == 0) {
					if ((st.st_mode & (S_IWGRP|S_IWOTH)) == 0
					 && st.st_uid == getuid()
					) {
						cmds = xmalloc_open_read_close(exrc, NULL);
					} else {
						status_line_bold(".exrc: permission denied");
					}
				}
				free(exrc);
			}
		}

		if (cmds) {
			init_text_buffer(NULL);
			run_cmds(cmds);
			free(cmds);
		}
	}
#endif
	// "Save cursor, use alternate screen buffer, clear screen"
	write1(ESC"[?1049h");
	// This is the main file handling loop
	optind = 0;
	while (1) {
		edit_file(argv[optind]); // might be NULL on 1st iteration
		// NB: optind can be changed by ":next" and ":rewind" commands
		optind++;
		if (optind >= cmdline_filecnt)
			break;
	}
	// "Use normal screen buffer, restore cursor"
	write1(ESC"[?1049l");

	return 0;
}
