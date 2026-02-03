// This file is part of The New Aspell
// Copyright (C) 2002,2011 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

/* Low level terminal interface.  This file will possible use the following
   macros:
   POSIX_TERMIOS
   HAVE_LIBCURSES
   CURSES_INCLUDE_STANDARD
   CURSES_INCLUDE_WORKAROUND_1
   CURSES_ONLY
   HAVE_GETCH
   HAVE_MBLEN
   All these macros need to have a true value and not just be defined
*/

#include "settings.h"

#ifdef DEFINE_XOPEN_SOURCE_EXTENDED
#  define _XOPEN_SOURCE_EXTENDED 1
#endif

#ifdef CURSES_NON_POSIX
#define CURSES_ONLY 1
#endif

#include <signal.h>

#include "asc_ctype.hpp"
#include "check_funs.hpp"
#include "convert.hpp"
#include "config.hpp"

#include "gettext.h"

using namespace acommon;

StackPtr<CheckerString> state;
const char * last_prompt = 0;
StackPtr<Choices> word_choices;
StackPtr<Choices> menu_choices;
Conv null_conv;
extern Conv dconv;
extern Conv uiconv;

#if   POSIX_TERMIOS

// Posix headers
#include <termios.h>
#include <unistd.h>

static termios new_attributes;
static termios saved_attributes;

#elif HAVE_GETCH

extern "C" {int getch();}

#endif

#if HAVE_LIBCURSES

#include CURSES_HEADER

#if CURSES_INCLUDE_STANDARD

#include TERM_HEADER

#elif CURSES_INCLUDE_WORKAROUND_1

// including <term.h> on solaris causes problems
extern "C" {char * tigetstr(char * capname);}

#endif

enum MenuText {StdMenu, ReplMenu};
static MenuText menu_text = StdMenu;

static bool use_curses = true;
static WINDOW * text_w = 0;
static WINDOW * menu_w = 0;
static WINDOW * choice_w = 0;
//static int beg_x = -1;
static int end_x = -1;
static int max_x;
static char * choice_text;
static int choice_text_size;
static int cur_x = 0;
static int cur_y = 0;
static volatile int last_signal = 0;

#ifdef CURSES_ONLY

char * tigetstr(char *) {return "";}

#else

static SCREEN * term;

#endif

#endif // HAVE_LIBCURSES

#ifdef HAVE_MBLEN
#  include <wchar.h>
#else
#  define mblen(x,y) (*(x) ? 1 : 0)
#endif

static void cleanup (void) {
#if   HAVE_LIBCURSES
  if (use_curses) {
    endwin();
  } else
#endif
  {
#if   POSIX_TERMIOS
    tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
#endif
  }
}

#if   HAVE_LIBCURSES

//static void do_nothing(int) {}

static void layout_screen();

#ifdef CURSES_ONLY

inline void handle_last_signal() {}

#else

static void save_state() {
  getyx(choice_w,cur_y,cur_x);
  choice_text_size = COLS;
  choice_text = new char[choice_text_size];
  for (int i = 0; i != choice_text_size; ++i) {
    choice_text[i] = mvwinch(choice_w, 0, i) & A_CHARTEXT;
  }
  endwin();
}

static void restore_state() {
  delscreen(term);
  term = newterm(0,stdout,stdin);
  set_term(term);
  layout_screen();
  display_menu();
  display_misspelled_word();
  wmove(choice_w,0,0);
  if (COLS <= choice_text_size)
    choice_text_size = COLS - 1;
  for (int i=0; i < choice_text_size; ++i)
    waddch(choice_w,choice_text[i]);
  delete[] choice_text;
  max_x = COLS - 1;
  if (cur_x > max_x)
    cur_x = max_x;
  if (end_x > max_x-1)
    end_x = max_x;
  wmove(choice_w,cur_y,cur_x);
  wnoutrefresh(choice_w);
  doupdate();
}


static void suspend_handler(int) {
  last_signal = SIGTSTP;
}
  
static void continue_handler(int) {
  restore_state();
  signal(SIGTSTP, suspend_handler);
  signal(SIGCONT,  continue_handler),
    last_signal = 0;
}

static void resize_handler(int ) {
  last_signal = SIGWINCH;
}
  
static void resize() {
  save_state();
  restore_state();
  last_signal = 0;
}

static void suspend() {
  save_state();
  signal(SIGTSTP, SIG_DFL);
  raise(SIGTSTP);
  last_signal = 0;
}

static inline void handle_last_signal() {
  switch (last_signal) {
  case SIGWINCH:
    resize();
    signal(SIGWINCH, resize_handler);
    break;
  case SIGTSTP:
    suspend();
    break;
  }
}

#endif

static void layout_screen() {
  text_w = 0;
  menu_w = 0;
  choice_w = 0;
  nonl();
  noecho();
  halfdelay(1);
  keypad(stdscr, true);
  clear();
  int height, width;
  getmaxyx(stdscr, height, width);
  int text_height = height - MENU_HEIGHT - 3;
  if (text_height >= 1) {
    text_w = newwin(text_height, width, 0, 0);
    scrollok(text_w,false);
    move(text_height, 0);
    hline((unsigned char)' '|A_REVERSE, width);
    menu_w = newwin(MENU_HEIGHT, width, text_height+1, 0);
    scrollok(menu_w,false);
  }
  if (height >= 2) {
    move(height-2,0);
    hline((unsigned char)' '|A_REVERSE, width);
  }
  choice_w = newwin(1, width, height-1, 0);
  keypad(choice_w,true);
  scrollok(menu_w,true);
  wnoutrefresh(stdscr);
}

#endif

void begin_check() {
  
  //
  //
  //
#if   HAVE_LIBCURSES
#if   CURSES_ONLY
  use_curses=true;
  initscr();
#else
  term = newterm(0,stdout,stdin);
  if (term == 0) {
    use_curses = false;
  } else {
    set_term(term);
    if ((tigetstr(const_cast<char *>("cup") /*move*/) != 0 
         || (tigetstr(const_cast<char *>("cuf1") /*right*/) != 0 
             && tigetstr(const_cast<char *>("cub1") /*left*/)  != 0 
             && tigetstr(const_cast<char *>("cuu1") /*up  */)  != 0 
             && tigetstr(const_cast<char *>("cud1") /*down*/)  != 0))
        && (tigetstr(const_cast<char *>("rev")) != 0))
    {
      use_curses = true;
    } else {
      use_curses = false;
      endwin();
      delscreen(term);
    }
  }
  if (use_curses) {
    signal(SIGWINCH, resize_handler);
    signal(SIGTSTP,  suspend_handler);
    signal(SIGCONT,  continue_handler);
  }
#endif
  if (use_curses) {
    layout_screen();
    atexit(cleanup);
  }
#endif
#if   POSIX_TERMIOS || (HAVE_LIBCURSES && !CURSES_ONLY)
  if (!isatty (STDIN_FILENO)) {
    puts(_("Error: Stdin not a terminal."));
    exit (-1);
  }
  
  //
  // Save the terminal attributes so we can restore them later.
  //
  tcgetattr (STDIN_FILENO, &saved_attributes);
  atexit(cleanup);
  
  //
  // Set up the terminal to read in a line character at a time
  //
  tcgetattr (STDIN_FILENO, &new_attributes);
#if POSIX_TERMIOS
  new_attributes.c_lflag &= ~(ICANON); // Clear ICANON 
  new_attributes.c_cc[VMIN] = 1;
  new_attributes.c_cc[VTIME] = 0;
#endif
  // FIXME correctly test for _POSIX_VDISABLE;
  new_attributes.c_cc[VINTR] = _POSIX_VDISABLE;
  tcsetattr (STDIN_FILENO, TCSAFLUSH, &new_attributes);
#endif
}

#ifdef HAVE_WIDE_CURSES
#  define IS_KEY(k) (res == KEY_CODE_YES && c == KEY_##k)
#  define NOT_KEY (res != KEY_CODE_YES)
#else
#  define IS_KEY(k) (c == KEY_##k)
#  define NOT_KEY (c < 256)
#endif

void get_line(String & line) {
#if   HAVE_LIBCURSES
  if (use_curses) {
    // FIXME: This won't work correctly when combing characters are
    // involved.
    menu_text = ReplMenu;
    display_menu();
    wnoutrefresh(choice_w);
    doupdate();
    line.resize(0);
#ifdef HAVE_WIDE_CURSES
    wint_t c;
#else
    int c;
#endif
    noecho();
    int begin_x;
    {int junk; getyx(choice_w, junk, begin_x);}
    int max_x = COLS - 1;
    int end_x = begin_x;
    while (true) {
      handle_last_signal();
#ifdef HAVE_WIDE_CURSES
      int res = wget_wch(choice_w, &c);
      if (res == ERR) continue;
#else
      c = wgetch(choice_w);
      if (c == ERR) continue;
#endif
      if (c == '\r' || c == '\n' || c == KEY_ENTER) 
        break;
      if (c == control('c') || c == KEY_BREAK) {
        end_x = begin_x;
        break;
      }
      int y,x;
      getyx(choice_w,y,x);
      if ((IS_KEY(LEFT) || c == control('b')) && begin_x < x) {
        wmove(choice_w, y,x-1);
      } else if ((IS_KEY(RIGHT) || c == control('f')) && x < end_x) {
        wmove(choice_w, y,x+1);
      } else if (IS_KEY(HOME) || c == control('a')) {
        wmove(choice_w, y, begin_x);
      } else if (IS_KEY(END)  || c == control('e')) {
        wmove(choice_w, y, end_x);
      } else if ((IS_KEY(BACKSPACE) || c == control('h') || c == '\x7F') 
                 && begin_x < x) {
        wmove(choice_w, y,x-1);
        wdelch(choice_w);
        --end_x;
      } else if (IS_KEY(DC) || c == control('d')) {
        wdelch(choice_w);
        --end_x;
      } else if (IS_KEY(EOL) || c == control('k')) {
        wclrtoeol(choice_w);
        end_x = x;
      } else if (x < max_x && 32 <= c && c != '\x7F' && NOT_KEY /*c < 256*/) {
#ifdef HAVE_WIDE_CURSES
        wchar_t wc = c;
        cchar_t cc;
        setcchar(&cc, &wc, 0, 0, NULL);
        wins_wch(choice_w, &cc);
#else
        winsch(choice_w, c);
#endif
        wmove(choice_w, y, x+1);
        ++end_x;
      }
      wrefresh(choice_w);
    }
#ifdef HAVE_WIDE_CURSES
    Vector<wchar_t> wstr;
    for (int i = begin_x; i < end_x; ++i) {
      cchar_t cc;
      attr_t att;
      short cp;
      mvwin_wch(choice_w, 0, i, &cc);
      size_t s = getcchar(&cc, 0, &att, &cp, 0);
      wstr.resize(s+1); // +1 to allow room for the null character
      getcchar(&cc, wstr.data(), &att, &cp, 0);
      s = wcstombs(0, wstr.data(), 0);
      if (s != (size_t)-1) {
        int pos = line.alloc(s); // this always leaves space for the
                                 // null character
        wcstombs(line.data(pos), wstr.data(), s);
      } else {
        line += '?';
      }
    }
#else
    for (int i = begin_x; i < end_x; ++i) {
      line += mvwinch(choice_w, 0, i) & A_CHARTEXT;
    }
#endif
    menu_text = StdMenu;
    display_menu();
    doupdate();
  } else 
#endif
  {
#if   POSIX_TERMIOS
    tcsetattr (STDIN_FILENO, TCSANOW, &saved_attributes);
#endif
    line.resize(0);
    char c;
    while ((c = getchar()) != '\n')
      line += c;
#if   POSIX_TERMIOS
    tcsetattr (STDIN_FILENO, TCSANOW, &new_attributes);
#endif
  }
  if (uiconv.conv) {
    line = uiconv(line);
  }
}

#undef IS_KEY
#undef NOT_KEY
    
void get_choice(int & c) {
#if   HAVE_LIBCURSES
  if (use_curses) {
    doupdate();
    int c0;
    do {
      handle_last_signal();
      c0 = wgetch(choice_w);
    } while (c0 == ERR);
    if (c0 == KEY_BREAK)
      c0 = control('c');
    if (1 <= c0 && c0 < 128) {
      c = static_cast<char>(c0);
      waddch(choice_w,c);
      wrefresh(choice_w);
    } else {
      c = 0;
    }
  } else
#endif
  {
#if   POSIX_TERMIOS
    int c0 = 0;
    read (STDIN_FILENO, &c0, 1);
    c = c0;
    putchar('\n');
#elif HAVE_GETCH
    c = getch();
    putchar(c);
    putchar('\n');
#else
    c = getchar();
    char d = c;
    while (d != '\n') d = getchar();
    putchar('\n');
#endif
  }
}

#if   HAVE_LIBCURSES
static void new_line(int & l, int y, int height) {
  --l;
  if (y == height - 1) {
    wmove(text_w, 0, 0);
    wdeleteln(text_w);
    wmove(text_w, height-1, 0);
  } else {
    wmove(text_w,y+1,0);
  }
}
static void new_line(int & l, int height) {
  int y,x;
  getyx(text_w,y,x);
  new_line(l,y,height);
}
#endif

void display_misspelled_word() {

  const char * word_begin = state->word_begin();
  const char * word_end   = state->word_end();
  LineIterator cur_line = state->cur_line();

#if   HAVE_LIBCURSES

  if (use_curses && text_w) {
    int height, width;
    werase(text_w);
    getmaxyx(text_w,height,width);
    assert(height > 0 && width > 0);

    LineIterator i = cur_line;
    
    //
    // backup height/3 lines
    //
    int l = height/3;
    while (!i.off_end() && l > 0) {
      --l;
      --i;
    }
    if (i.off_end()) ++i;

    while (l != 0)
      new_line(l,height);

    l = -1;

    const char * j = i->begin();
    while (!i.off_end()) {

      int y,x;
      int y0,x0;
      getyx(text_w, y, x);
      x0 = x;
      y0 = y;

      wattrset(text_w,A_NORMAL);
      int last_space_pos = 0;
      const char * last_space = j;
      int prev_glyph_pos = 0;
      const char * prev_glyph = j;

      // NOTE: Combining characters after a character at the very end of
      //   a line will cause an unnecessary word wrap.  Unfortunately I
      //   do not know how to avoid it as they is no portable way to
      //   find out if the next character will combine with the
      //   previous.

      // We check that:
      // - we haven't reached the end of the text
      // - we haven't reached the end of the line
      // - curse haven't jumped to the next screen line
      // - curse haven't reached the end of the screen
      while (j < i->end()
             && *j != '\n' 
             && (y0 == y && x >= x0) 
             && x0 < width - 1)
      {
        if (asc_isspace(*j)) {
          last_space_pos = x;
          last_space = j;
        }
        if (j == word_begin) {
          wattrset(text_w,A_REVERSE);
          l = height*2/3;
        } else if (j == word_end) {
          wattrset(text_w,A_NORMAL);
        }
        
        int len = mblen(j, MB_CUR_MAX);
        int res = OK;
        if (len > 0) {
          res = waddnstr(text_w, const_cast<char *>(j), len);
        } else {
          waddch(text_w, ' ');
          len = 1;
        }

        j += len;
        x0 = x;
        getyx(text_w, y, x);

        if (x != x0 || res == ERR) {
          prev_glyph_pos = x0;
          prev_glyph = j - len;
        }
        
      }
      y = y0;
      if (j == i->end() || *j == '\n') {
        ++i;
        j = i->begin();
      } else {
        if (width - last_space_pos < width/3) {
          wmove(text_w, y, last_space_pos);
          wclrtoeol(text_w);
          j = last_space + 1;
        } else {
          wmove(text_w, y, prev_glyph_pos);
          wclrtoeol(text_w);
          j = prev_glyph;
        }
        wmove(text_w, y, width-1);
        wattrset(text_w,A_NORMAL);
        waddch(text_w,'\\');
      }

      if (l == 0) break;
      new_line(l,y,height);
    }

    while (l != 0)
      new_line(l,height);
      
    wnoutrefresh(text_w);

  } else if (use_curses && !text_w) {
    // do nothing
  } else
#endif
  {
    int pre  = word_begin - cur_line->begin();
    int post = cur_line->end() - word_end;
    if (pre)
      fwrite(cur_line->begin(), pre, 1, stdout);
    putchar('*');
    fwrite(word_begin, word_end - word_begin, 1, stdout);
    putchar('*');
    if (post)
      fwrite(word_end, post, 1, stdout);
  }
}

static inline void put (FILE * out, char c) {putc(c, out);}
static inline void put (FILE * out, const char * s) {fputs(s, out);}
static inline void put (FILE * out, const char * s, int size) {fwrite(s, size, 1, out);}

static void print_truncate(FILE * out, const char * word, int width) {
  int i;
  int len = 0;
  for (i = 0; i < width-1 && *word; word += len, ++i) {
    len = mblen(word, MB_CUR_MAX);
    if (len > 0) {
      put(out, word, len);
    } else {
      put(out, ' ');
      len = 1;
    }
  }
  if (i == width-1) {
    if (*word == '\0')
      put(out,' ');
    else if (word[len] == '\0')
      put(out, word, len);
    else
      put(out,'$');
    ++i;
  }
  for (;i < width; ++i)
    put(out,' ');
}

static void display_menu(FILE * out, const Choices * choices, int width, 
                         Conv & conv = null_conv) 
{
  if (width <= 11) return;
  Choices::const_iterator i = choices->begin();
  while (i != choices->end()) {
    put(out,i->choice);
    put(out,") ");
    print_truncate(out, conv(i->desc), width/2 - 4);
    put(out,' ');
    ++i;
    if (i != choices->end()) {
      put(out,i->choice);
      put(out,") ");
      print_truncate(out, conv(i->desc), width/2 - 4);
      ++i;
    }
    putc('\n', out);
  }
}

#if   HAVE_LIBCURSES

static inline void put (WINDOW * w, char c) 
{
  waddch(w,static_cast<unsigned char>(c));
}
static inline void put (WINDOW * w, const char * c) 
{
  waddstr(w,const_cast<char *>(c));
}
static inline void put (WINDOW * w, const char * c, int size)
{
  waddnstr(w,const_cast<char *>(c), size);
}

static void print_truncate(WINDOW * out, const char * word, int width) {
  int len = 0;
  int y,x;
  int y0;
  getyx(out, y, x);
  int stop = x + width - 1;
  while (x <= stop && *word) {
    len = mblen(word, MB_CUR_MAX);
    if (len <= 0) len = 1;
    put(out, word, len);
    word += len;
    y0 = y;
    getyx(out, y, x);
    if (y != y0) {y = y0; x = stop + 1; break;}
  }
  for (; x <= stop; ++x)
    put(out, ' ');
  if (x > stop && *word) {
    wmove(out, y, stop);
    put(out,'$');
  }
}

static void display_menu(WINDOW * out, const Choices * choices, int width,
                         Conv & conv = null_conv) 
{
  if (width <= 11) return;
  Choices::const_iterator i = choices->begin();
  int y,x;
  getyx(out, y, x);
  while (i != choices->end()) {
    wclrtoeol(out);
    put(out,i->choice);
    put(out,") ");
    print_truncate(out, conv(i->desc), width/2 - 4);
    ++i;
    if (i != choices->end()) {
      wmove(out, y, width/2);
      put(out,i->choice);
      put(out,") ");
      print_truncate(out, conv(i->desc), width/2 - 4);
      ++i;
    }
    ++y;
    wmove(out, y, 0);
  }
}

#endif

void display_menu() {
#if   HAVE_LIBCURSES
  if (use_curses && menu_w) {
    if (menu_text == StdMenu) {
      scrollok(menu_w,false);
      int height,width;
      getmaxyx(menu_w,height,width);
      werase(menu_w);
      wmove(menu_w,0,0);
      display_menu(menu_w, word_choices.get(), width, dconv);
      wmove(menu_w,5,0);
      display_menu(menu_w, menu_choices.get(), width);
      wnoutrefresh(menu_w);
    } else {
      //ostream str;
      int height,width;
      getmaxyx(menu_w,height,width);
      struct MenuLine {
        const char * capname;
        const char * fun_key;
        const char * control_key;
        const char * desc;
      };
      static MenuLine menu_items[9] = {
        {0, 
         /* TRANSLATORS: This is a literal Key.*/
         N_("Enter"), 
         "", 
         N_("Accept Changes")},
        {0,
         /* TRANSLATORS: This is a literal Key. */
         N_("Backspace"), 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-H"), 
         N_("Delete the previous character")},
        {"kcub1", 
         /* TRANSLATORS: This is a literal Key. */
         N_("Left"), 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-B"), 
         N_("Move Back one space")},
        {"kcuf1", 
         /* TRANSLATORS: This is a literal Key. */
         N_("Right"), 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-F"), 
         N_("Move Forward one space")},
        {"khome", 
         /* TRANSLATORS: This is a literal Key. */
         N_("Home"), 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-A"), 
         N_("Move to the beginning of the line")},
        {"kend" , 
         /* TRANSLATORS: This is a literal Key. */
         N_("End"), 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-E"), 
         N_("Move to the end of the line")},
        {"kdch1", 
         /* TRANSLATORS: This is a literal Key. */
         N_("Delete"), 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-D"), 
         N_("Delete the next character")},
        {0, 
         "", 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-K"), 
         N_("Kill all characters to the EOL")},
        {0, 
         "", 
         /* TRANSLATORS: This is a literal Key. */
         N_("Control-C"), 
         N_("Abort This Operation")},
      };
      scrollok(menu_w,false);
      werase(menu_w);
      int beg = 0,end = 0;
      int y,x,x0;
      for (int i = 0; i != 9; ++i) {
        wmove(menu_w, i, beg);
        if (menu_items[i].capname == 0 
            || tigetstr(const_cast<char *>(menu_items[i].capname)) != 0)
        {
          getyx(menu_w, y, x0);
          put(menu_w, gt_(menu_items[i].fun_key));
          getyx(menu_w, y, x);
          if (x < beg || y != i) end = width;
          else if (x > end) end = x;
        }
      }
      beg = end + 2;
      if (beg < width) for (int i = 0; i != 9; ++i) {
        wmove(menu_w, i, beg);
        put(menu_w, gt_(menu_items[i].control_key));
        getyx(menu_w, y, x);
        if (x < beg || y != i) end = width;
        else if (x > end) end = x;
      }
      beg = end + 2;
      int w = width - beg;
      if (w > 1) for (int i = 0; i != 9; ++i) {
        wmove(menu_w, i, beg);
        print_truncate(menu_w, gt_(menu_items[i].desc), w);
      }
      wnoutrefresh(menu_w);
    }
  } else if (use_curses && !menu_w) {
    // do nothing
  } else 
#endif
  {
    display_menu(stdout, word_choices.get(), 80, dconv);
    display_menu(stdout, menu_choices.get(), 80);
  }
}

void prompt(const char * prompt) {
  last_prompt = prompt;
#if   HAVE_LIBCURSES
  if (use_curses) {
    werase(choice_w);
    waddstr(choice_w, const_cast<char *>(prompt));
    wnoutrefresh(choice_w);
  } else
#endif
  {
    fputs(prompt, stdout);
    fflush(stdout);
  }
}
  
void error(const char * error) {
#if   HAVE_LIBCURSES 
  if (use_curses) {
    werase(choice_w);
    waddstr(choice_w, const_cast<char *>(error));
    wnoutrefresh(choice_w);
  } else 
#endif
  {
    fputs(error, stdout);
    fputs(last_prompt, stdout);
    fflush(stdout);
  }
}


