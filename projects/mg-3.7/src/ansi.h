/*
 * ansi display driver backend
 *
 * This file emulates the termcap/terminfo/ncurses backend with simple
 * ANSI escape sequences.  Since terminfo is the de facto default in 
 * all other Mg clones, this file hooks into that and emulates that
 * interfaces to the best of its abilities.
 */

/* This file is in the public domain. */

#ifndef ANSI_H
#define ANSI_H

typedef struct {
	int   t_nrow;		/* Number of rows.           */
	int   t_ncol;		/* Number of columns         */
	int   t_num[10];	/* Terminal constants        */
	char *t_str[100];	/* Terminal control strings  */
	int   t_fd;		/* filedes from setupterm()  */
} TERMINAL;

extern TERMINAL *cur_term;

#define CUR cur_term->

//extern int columns;
//extern int lines;
#define columns              CUR t_ncol
#define lines                CUR t_nrow
#define change_scroll_region NULL
#define magic_cookie_glitch  CUR t_num[4]

#define bell                 CUR t_str[1]
#define key_down             CUR t_str[11]
#define key_eol              CUR t_str[12]
#define key_home             CUR t_str[13]
#define key_left             CUR t_str[15]
#define key_right            CUR t_str[17]
#define key_up               CUR t_str[19]

#define key_ppage            CUR t_str[53]
#define key_npage            CUR t_str[54]
#define key_beg              CUR t_str[49]
#define key_end              CUR t_str[52]
#define key_ic               CUR t_str[50]
#define key_dc               CUR t_str[51]

#define key_f1               CUR t_str[58]
#define key_f2               CUR t_str[59]
#define key_f3               CUR t_str[60]
#define key_f4               CUR t_str[61]
#define key_f5               CUR t_str[62]
#define key_f6               CUR t_str[63]
#define key_f7               CUR t_str[64]
#define key_f8               CUR t_str[65]
#define key_f9               CUR t_str[66]
#define key_f10              CUR t_str[67]
#define key_f11              CUR t_str[68]
#define key_f12              CUR t_str[69]

#define scroll_reverse       CUR t_str[11]
#define scroll_forward       CUR t_str[10]

#define keypad_local         NULL
#define keypad_xmit          NULL

#define parm_delete_line     CUR t_str[46]
#define parm_insert_line     CUR t_str[47]
#define parm_down_cursor     CUR t_str[48]

#define insert_line          CUR t_str[23]
#define delete_line          CUR t_str[22]
#define clr_eol              CUR t_str[6]
#define clr_eos              CUR t_str[7]
#define cursor_up            CUR t_str[19]
#define cursor_address       CUR t_str[10]

#define enter_ca_mode        ""
#define exit_ca_mode         ""

#define enter_standout_mode  CUR t_str[35]
#define exit_standout_mode   CUR t_str[43]

int   setupterm(const char *term, int filedes, int *errret);

char *tgoto(const char *cap, int col, int row);
int   tputs(const char *str, int affcnt, int (*putc)(int));

#endif /* ANSI_H */
