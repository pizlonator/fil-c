/* vi.c - You can't spell "evil" without "vi".
 *
 * Copyright 2015 Rob Landley <rob@landley.net>
 * Copyright 2019 Jarno Mäkipää <jmakip87@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/vi.html

USE_VI(NEWTOY(vi, ">1s:c:", TOYFLAG_USR|TOYFLAG_BIN))

config VI
  bool "vi"
  default n
  help
    usage: vi [-s SCRIPT] FILE

    Visual text editor. Predates keyboards with standardized cursor keys.
    If you don't know how to use it, hit the ESC key, type :q! and press ENTER.

    -s	run SCRIPT as if typed at keyboard (like -c "source SCRIPT")
    -c	run SCRIPT of ex commands

    The editor is usually in one of three modes:

      Hit ESC for "vi mode" where each key is a command.
      Hit : for "ex mode" which runs command lines typed at bottom of screen.
      Hit i (from vi mode) for "insert mode" where typing adds to the file.

    ex mode commands (ESC to exit ex mode):

      q   Quit (exit editor if no unsaved changes)
      q!  Quit discarding unsaved changes
      w   Write changed contents to file (optionally to NAME argument)
      wq  Write to file, then quit

    vi mode single key commands:
      i  switch to insert mode (until next ESC)
      u  undo last change (can be repeated)
      a  append (move one character right, switch to insert mode)
      A  append (jump to end of line, switch to insert mode)

    vi mode commands that prompt for more data on bottom line:
      :  switch to ex mode
      /  search forwards for regex
      ?  search backwards for regex
      .  repeat last command

      [count][cmd][motion]
      cmd: c d y
      motion: 0 b e G H h j k L l M w $ f F

      [count][cmd]
      cmd: D I J O n o p x dd yy

      [cmd]
      cmd: / ? : A a i CTRL_D CTRL_B CTRL_E CTRL_F CTRL_Y \e \b

      [cmd]
      \b \e \n 'set list' 'set nolist' d $ % g v
*/
#define FOR_vi
#include "toys.h"
#define CTL(a) a-'@'

GLOBALS(
  char *c, *s;

  char *filename;
  int vi_mode, tabstop, list, cur_col, cur_row, scr_row, drawn_row, drawn_col,
      count0, count1, vi_mov_flag;
  unsigned screen_height, screen_width;
  char vi_reg, *last_search;
  struct str_line {
    int alloc, len;
    char *data;
  } *il;
  size_t screen, cursor; //offsets
  //yank buffer
  struct yank_buf {
    char reg;
    int alloc;
    char *data;
  } yank;

  size_t filesize;
// mem_block contains RO data that is either original file as mmap
// or heap allocated inserted data
  struct block_list {
    struct block_list *next, *prev;
    struct mem_block {
      size_t size, len;
      enum alloc_flag {
        MMAP,  //can be munmap() before exit()
        HEAP,  //can be free() before exit()
        STACK, //global or stack perhaps toybuf
      } alloc;
      const char *data;
    } *node;
  } *text;

// slices do not contain actual allocated data but slices of data in mem_block
// when file is first opened it has only one slice.
// after inserting data into middle new mem_block is allocated for insert data
// and 3 slices are created, where first and last slice are pointing to original
// mem_block with offsets, and middle slice is pointing to newly allocated block
// When deleting, data is not freed but mem_blocks are sliced more such way that
// deleted data left between 2 slices
  struct slice_list {
    struct slice_list *next, *prev;
    struct slice {
      size_t len;
      const char *data;
    } *node;
  } *slices;
)

static const char *blank = " \n\r\t";
static const char *specials = ",.:;=-+*/(){}<>[]!@#$%^&|\\?\"\'";

//get utf8 length and width at same time
static int utf8_lnw(int *width, char *s, int bytes)
{
  unsigned wc;
  int length = 1;

  if (*s == '\t') *width = TT.tabstop;
  else {
    length = utf8towc(&wc, s, bytes);
    if (length < 1) length = 0, *width = 0;
    else *width = wcwidth(wc);
  }
  return length;
}

static int utf8_dec(char key, char *utf8_scratch, int *sta_p)
{
  int len = 0;
  char *c = utf8_scratch;
  c[*sta_p] = key;
  if (!(*sta_p))  *c = key;
  if (*c < 0x7F) return *sta_p = 1;
  if ((*c & 0xE0) == 0xc0) len = 2;
  else if ((*c & 0xF0) == 0xE0 ) len = 3;
  else if ((*c & 0xF8) == 0xF0 ) len = 4;
  else return *sta_p = 0;

  if (++*sta_p == 1) return 0;
  if ((c[*sta_p-1] & 0xc0) != 0x80) return *sta_p = 0;

  if (*sta_p == len) return !(c[(*sta_p)] = 0);

  return 0;
}

static char* utf8_last(char* str, int size)
{
  char *end = str+size;
  int pos = size, len, width = 0;

  for (;pos >= 0; end--, pos--) {
    len = utf8_lnw(&width, end, size-pos);
    if (len && width) return end;
  }
  return 0;
}

struct double_list *dlist_add_before(struct double_list **head,
  struct double_list **list, char *data)
{
  struct double_list *new = xmalloc(sizeof(struct double_list));
  new->data = data;
  if (*list == *head) *head = new;

  dlist_add_nomalloc(list, new);
  return new;
}

struct double_list *dlist_add_after(struct double_list **head,
  struct double_list **list, char *data)
{
  struct double_list *new = xmalloc(sizeof(struct double_list));
  new->data = data;

  if (*list) {
    new->prev = *list;
    new->next = (*list)->next;
    (*list)->next->prev = new;
    (*list)->next = new;
  } else *head = *list = new->next = new->prev = new;
  return new;
}

// str must be already allocated
// ownership of allocated data is moved
// data, pre allocated data
// offset, offset in whole text
// size, data allocation size of given data
// len, length of the string
// type, define allocation type for cleanup purposes at app exit
static int insert_str(const char *data, size_t offset, size_t size, size_t len,
  enum alloc_flag type)
{
  struct mem_block *b = xmalloc(sizeof(struct mem_block));
  struct slice *next = xmalloc(sizeof(struct slice));
  struct slice_list *s = TT.slices;

  b->size = size;
  b->len = len;
  b->alloc = type;
  b->data = data;
  next->len = len;
  next->data = data;

  //mem blocks can be just added unordered
  TT.text = (struct block_list *)dlist_add((struct double_list **)&TT.text,
    (char *)b);

  if (!s) {
    TT.slices = (struct slice_list *)dlist_add(
      (struct double_list **)&TT.slices,
      (char *)next);
  } else {
    size_t pos = 0;
    //search insertation point for slice
    do {
      if (pos<=offset && pos+s->node->len>offset) break;
      pos += s->node->len;
      s = s->next;
      if (s == TT.slices) return -1; //error out of bounds
    } while (1);
    //need to cut previous slice into 2 since insert is in middle
    if (pos+s->node->len>offset && pos!=offset) {
      struct slice *tail = xmalloc(sizeof(struct slice));
      tail->len = s->node->len-(offset-pos);
      tail->data = s->node->data+(offset-pos);
      s->node->len = offset-pos;
      //pos = offset;
      s = (struct slice_list *)dlist_add_after(
        (struct double_list **)&TT.slices,
        (struct double_list **)&s,
        (char *)tail);

      s = (struct slice_list *)dlist_add_before(
        (struct double_list **)&TT.slices,
        (struct double_list **)&s,
        (char *)next);
    } else if (pos==offset) {
      // insert before
      s = (struct slice_list *)dlist_add_before(
        (struct double_list **)&TT.slices,
        (struct double_list **)&s,
        (char *)next);
    } else {
      // insert after
      s = (void *)dlist_add_after((void *)&TT.slices, (void *)&s, (void *)next);
    }
  }
  return 0;
}

// this will not free any memory
// will only create more slices depending on position
static int cut_str(size_t offset, size_t len)
{
  struct slice_list *e, *s = TT.slices;
  size_t end = offset+len;
  size_t epos, spos = 0;

  if (!s) return -1;

  //find start and end slices
  for (;;) {
    if (spos<=offset && spos+s->node->len>offset) break;
    spos += s->node->len;
    s = s->next;

    if (s == TT.slices) return -1; //error out of bounds
  }

  for (e = s, epos = spos; ; ) {
    if (epos<=end && epos+e->node->len>end) break;
    epos += e->node->len;
    e = e->next;

    if (e == TT.slices) return -1; //error out of bounds
  }

  for (;;) {
    if (spos == offset && ( end >= spos+s->node->len)) {
      //cut full
      spos += s->node->len;
      offset += s->node->len;
      s = dlist_pop(&s);
      if (s == TT.slices) TT.slices = s->next;

    } else if (spos < offset && ( end >= spos+s->node->len)) {
      //cut end
      size_t clip = s->node->len - (offset - spos);
      offset = spos+s->node->len;
      spos += s->node->len;
      s->node->len -= clip;
    } else if (spos == offset && s == e) {
      //cut begin
      size_t clip = end - offset;
      s->node->len -= clip;
      s->node->data += clip;
      break;
    } else {
      //cut middle
      struct slice *tail = xmalloc(sizeof(struct slice));
      size_t clip = end-offset;
      tail->len = s->node->len-(offset-spos)-clip;
      tail->data = s->node->data+(offset-spos)+clip;
      s->node->len = offset-spos; //wrong?
      s = (struct slice_list *)dlist_add_after(
        (struct double_list **)&TT.slices,
        (struct double_list **)&s,
        (char *)tail);
      break;
    }
    if (s == e) break;

    s = s->next;
  }

  return 0;
}
static int modified()
{
  if (TT.text->next !=  TT.text->prev) return 1;
  if (TT.slices->next != TT.slices->prev) return 1;
  if (!TT.text || !TT.slices) return 0;
  if (!TT.text->node || !TT.slices->node) return 0;
  if (TT.text->node->alloc != MMAP) return 1;
  if (TT.text->node->len != TT.slices->node->len) return 1;
  if (!TT.text->node->len) return 1;
  return 0;
}

//find offset position in slices
static struct slice_list *slice_offset(size_t *start, size_t offset)
{
  struct slice_list *s = TT.slices;
  size_t spos = 0;

  //find start
  for ( ;s ; ) {
    if (spos<=offset && spos+s->node->len>offset) break;

    spos += s->node->len;
    s = s->next;

    if (s == TT.slices) s = 0; //error out of bounds
  }
  if (s) *start = spos;
  return s;
}

static size_t text_strchr(size_t offset, char c)
{
  struct slice_list *s = TT.slices;
  size_t epos, spos = 0;
  int i = 0;

  //find start
  if (!(s = slice_offset(&spos, offset))) return SIZE_MAX;

  i = offset-spos;
  epos = spos+i;
  do {
    for (; i < s->node->len; i++, epos++)
      if (s->node->data[i] == c) return epos;
    s = s->next;
    i = 0;
  } while (s != TT.slices);

  return SIZE_MAX;
}

static size_t text_strrchr(size_t offset, char c)
{
  struct slice_list *s = TT.slices;
  size_t epos, spos = 0;
  int i = 0;

  //find start
  if (!(s = slice_offset(&spos, offset))) return SIZE_MAX;

  i = offset-spos;
  epos = spos+i;
  do {
    for (; i >= 0; i--, epos--)
      if (s->node->data[i] == c) return epos;
    s = s->prev;
    i = s->node->len-1;
  } while (s != TT.slices->prev); //tail

  return SIZE_MAX;
}

static size_t text_filesize()
{
  struct slice_list *s = TT.slices;
  size_t pos = 0;

  if (s) do {
    pos += s->node->len;
    s = s->next;
  } while (s != TT.slices);

  return pos;
}

static int text_count(size_t start, size_t end, char c)
{
  struct slice_list *s = TT.slices;
  size_t i, count = 0, spos = 0;
  if (!(s = slice_offset(&spos, start))) return 0;
  i = start-spos;
  if (s) do {
    for (; i < s->node->len && spos+i<end; i++)
      if (s->node->data[i] == c) count++;
    if (spos+i>=end) return count;

    spos += s->node->len;
    i = 0;
    s = s->next;

  } while (s != TT.slices);

  return count;
}

static char text_byte(size_t offset)
{
  struct slice_list *s = TT.slices;
  size_t spos = 0;

  //find start
  if (!(s = slice_offset(&spos, offset))) return 0;
  return s->node->data[offset-spos];
}

//utf-8 codepoint -1 if not valid, 0 if out_of_bounds, len if valid
//copies data to dest if dest is not 0
static int text_codepoint(char *dest, size_t offset)
{
  char scratch[8] = {0};
  int state = 0, finished = 0;

  for (;!(finished = utf8_dec(text_byte(offset), scratch, &state)); offset++)
    if (!state) return -1;

  if (!finished && !state) return -1;
  if (dest) memcpy(dest, scratch, 8);

  return strlen(scratch);
}

static size_t text_sol(size_t offset)
{
  size_t pos;

  if (!TT.filesize || !offset) return 0;
  else if (TT.filesize <= offset) return TT.filesize-1;
  else if ((pos = text_strrchr(offset-1, '\n')) == SIZE_MAX) return 0;
  else if (pos < offset) return pos+1;
  return offset;
}

static size_t text_eol(size_t offset)
{
  if (!TT.filesize) offset = 1;
  else if (TT.filesize <= offset) return TT.filesize-1;
  else if ((offset = text_strchr(offset, '\n')) == SIZE_MAX)
    return TT.filesize-1;
  return offset;
}

static size_t text_nsol(size_t offset)
{
  offset = text_eol(offset);
  if (text_byte(offset) == '\n') offset++;
  if (offset >= TT.filesize) offset--;
  return offset;
}

static size_t text_psol(size_t offset)
{
  offset = text_sol(offset);
  if (offset) offset--;
  if (offset && text_byte(offset-1) != '\n') offset = text_sol(offset-1);
  return offset;
}

static size_t text_getline(char *dest, size_t offset, size_t max_len)
{
  struct slice_list *s = TT.slices;
  size_t end, spos = 0;
  int i, j = 0;

  if (dest) *dest = 0;

  if (!s) return 0;
  if ((end = text_strchr(offset, '\n')) == SIZE_MAX)
    if ((end = TT.filesize)  > offset+max_len) return 0;

  //find start
  if (!(s = slice_offset(&spos, offset))) return 0;

  i = offset-spos;
  j = end-offset+1;
  if (dest) do {
    for (; i < s->node->len && j; i++, j--, dest++)
      *dest = s->node->data[i];
    s = s->next;
    i = 0;
  } while (s != TT.slices && j);

  if (dest) *dest = 0;

  return end-offset;
}

// copying is needed when file has lot of inserts that are
// just few char long, but not always. Advanced search should
// check big slices directly and just copy edge cases.
// Also this is only line based search multiline
// and regexec should be done instead.
static size_t text_strstr(size_t offset, char *str, int dir)
{
  size_t bytes, pos = offset;
  char *s = 0;

  do {
    bytes = text_getline(toybuf, pos, ARRAY_LEN(toybuf));
    if (!bytes) pos += (dir ? 1 : -1); //empty line
    else if ((s = strstr(toybuf, str))) return pos+(s-toybuf);
    else {
      if (!dir) pos -= bytes;
      else pos += bytes;
    }
  } while (pos < (dir ? 0 : TT.filesize));

  return SIZE_MAX;
}

static void block_list_free(void *node)
{
  struct block_list *d = node;

  if (d->node->alloc == HEAP) free((void *)d->node->data);
  else if (d->node->alloc == MMAP) munmap((void *)d->node->data, d->node->size);

  free(d->node);
  free(d);
}

static void show_error(char *fmt, ...)
{
  va_list va;

  printf("\a\e[%dH\e[41m\e[37m\e[K\e[1m", TT.screen_height+1);
  va_start(va, fmt);
  vprintf(fmt, va);
  va_end(va);
  printf("\e[0m");
  fflush(0);
  xferror(stdout);

  // TODO: better integration with status line: keep
  // message until next operation.
  (void)getchar();
}

static void linelist_unload()
{
  llist_traverse((void *)TT.slices, llist_free_double);
  llist_traverse((void *)TT.text, block_list_free);
  TT.slices = 0, TT.text = 0;
}

static void linelist_load(char *filename, int ignore_missing)
{
  int fd;
  long long size;

  if (!filename) filename = TT.filename;
  if (!filename) {
    // `vi` with no arguments creates a new unnamed file.
    insert_str(xstrdup("\n"), 0, 1, 1, HEAP);
    return;
  }

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    if (!ignore_missing)
      show_error("Couldn't open \"%s\" for reading: %s", filename,
          strerror(errno));
    insert_str(xstrdup("\n"), 0, 1, 1, HEAP);
    return;
  }

  size = fdlength(fd);
  if (size > 0) {
    insert_str(xmmap(0,size,PROT_READ,MAP_SHARED,fd,0), 0, size, size, MMAP);
    TT.filesize = text_filesize();
  } else if (!size) insert_str(xstrdup("\n"), 0, 1, 1, HEAP);
  xclose(fd);
}

static int write_file(char *filename)
{
  struct slice_list *s = TT.slices;
  struct stat st;
  int fd = 0;

  if (!modified()) show_error("Not modified");
  if (!filename) filename = TT.filename;
  if (!filename) {
    show_error("No file name");
    return -1;
  }

  if (stat(filename, &st) == -1) st.st_mode = 0644;

  sprintf(toybuf, "%s.swp", filename);

  if ((fd = open(toybuf, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode)) == -1) {
    show_error("Couldn't open \"%s\" for writing: %s", toybuf, strerror(errno));
    return -1;
  }

  if (s) {
    do {
      xwrite(fd, (void *)s->node->data, s->node->len);
      s = s->next;
    } while (s != TT.slices);
  }

  linelist_unload();

  xclose(fd);
  if (!rename(toybuf, filename)) return 1;
  linelist_load(filename, 0);
  return 0;
}

// jump into valid offset index
// and valid utf8 codepoint
static void check_cursor_bounds()
{
  char buf[8] = {0};
  int len, width = 0;

  if (!TT.filesize) TT.cursor = 0;
  for (;;) {
    if (TT.cursor < 1) {
      TT.cursor = 0;
      return;
    } else if (TT.cursor >= TT.filesize-1) {
      TT.cursor = TT.filesize-1;
      return;
    }
    // if we are not in valid data try jump over
    if ((len = text_codepoint(buf, TT.cursor)) < 1) TT.cursor--;
    else if (utf8_lnw(&width, buf, len) && width) break;
    else TT.cursor--; //combine char jump over
  }
}

// TT.vi_mov_flag is used for special cases when certain move
// acts differently depending is there DELETE/YANK or NOP
// Also commands such as G does not default to count0=1
// 0x1 = Command needs argument (f,F,r...)
// 0x2 = Move 1 right on yank/delete/insert (e, $...)
// 0x4 = yank/delete last line fully
// 0x10000000 = redraw after cursor needed
// 0x20000000 = full redraw needed
// 0x40000000 = count0 not given
// 0x80000000 = move was reverse

// TODO rewrite the logic, difficulties counting lines
// and with big files scroll should not rely in knowing
// absoluteline numbers
static void adjust_screen_buffer()
{
  size_t c, s;
  TT.cur_row = 0, TT.scr_row = 0;
  if (!TT.cursor) {
    TT.screen = 0;
    TT.vi_mov_flag = 0x20000000;
    return;
  } else if (TT.screen > (1<<18) || TT.cursor > (1<<18)) {
    //give up, file is big, do full redraw

    TT.screen = text_strrchr(TT.cursor-1, '\n')+1;
    TT.vi_mov_flag = 0x20000000;
    return;
  }

  s = text_count(0, TT.screen, '\n');
  c = text_count(0, TT.cursor, '\n');
  if (s >= c) {
    TT.screen = text_strrchr(TT.cursor-1, '\n')+1;
    s = c;
    TT.vi_mov_flag = 0x20000000; //TODO I disabled scroll
  } else {
    int distance = c-s+1;
    if (distance > (int)TT.screen_height) {
      int n, adj = distance-TT.screen_height;
      TT.vi_mov_flag = 0x20000000; //TODO I disabled scroll
      for (;adj; adj--, s++)
        if ((n = text_strchr(TT.screen, '\n'))+1 > TT.screen)
          TT.screen = n+1;
    }
  }

  TT.scr_row = s;
  TT.cur_row = c;
}

// TODO search yank buffer by register
// TODO yanks could be separate slices so no need to copy data
// now only supports default register
static int vi_yank(char reg, size_t from, int flags)
{
  size_t start = from, end = TT.cursor;
  char *str;

  memset(TT.yank.data, 0, TT.yank.alloc);
  if (TT.vi_mov_flag&0x80000000) start = TT.cursor, end = from;
  else TT.cursor = start; //yank moves cursor to left pos always?

  if (TT.yank.alloc < end-from) {
    size_t new_bounds = (1+end-from)/1024;
    new_bounds += ((1+end-from)%1024) ? 1 : 0;
    new_bounds *= 1024;
    TT.yank.data = xrealloc(TT.yank.data, new_bounds);
    TT.yank.alloc = new_bounds;
  }

  //this is naive copy
  for (str = TT.yank.data ; start<end; start++, str++) *str = text_byte(start);

  *str = 0;

  return 1;
}

static int vi_delete(char reg, size_t from, int flags)
{
  size_t start = from, end = TT.cursor;

  vi_yank(reg, from, flags);

  if (TT.vi_mov_flag&0x80000000) start = TT.cursor, end = from;

  //pre adjust cursor move one right until at next valid rune
  if (TT.vi_mov_flag&2) {
    //TODO
  }
  //do slice cut
  cut_str(start, end-start);

  //cursor is at start at after delete
  TT.cursor = start;
  TT.filesize = text_filesize();
  //find line start by strrchr(/n) ++
  //set cur_col with crunch_n_str maybe?
  TT.vi_mov_flag |= 0x30000000;

  return 1;
}

static int vi_change(char reg, size_t to, int flags)
{
  vi_delete(reg, to, flags);
  TT.vi_mode = 2;
  return 1;
}

static int cur_left(int count0, int count1, char *unused)
{
  int count = count0*count1;

  TT.vi_mov_flag |= 0x80000000;
  for (;count && TT.cursor; count--) {
    TT.cursor--;
    if (text_byte(TT.cursor) == '\n') TT.cursor++;
    check_cursor_bounds();
  }
  return 1;
}

static int cur_right(int count0, int count1, char *unused)
{
  int count = count0*count1, len, width = 0;
  char buf[8] = {0};

  for (;count; count--) {
    len = text_codepoint(buf, TT.cursor);

    if (*buf == '\n') break;
    else if (len > 0) TT.cursor += len;
    else TT.cursor++;

    for (;TT.cursor < TT.filesize;) {
      if ((len = text_codepoint(buf, TT.cursor)) < 1) {
        TT.cursor++; //we are not in valid data try jump over
        continue;
      }

      if (utf8_lnw(&width, buf, len) && width) break;
      else TT.cursor += len;
    }
  }
  check_cursor_bounds();
  return 1;
}

//TODO column shift
static int cur_up(int count0, int count1, char *unused)
{
  int count = count0*count1;

  for (;count--;) TT.cursor = text_psol(TT.cursor);
  TT.vi_mov_flag |= 0x80000000;
  check_cursor_bounds();
  return 1;
}

//TODO column shift
static int cur_down(int count0, int count1, char *unused)
{
  int count = count0*count1;

  for (;count--;) TT.cursor = text_nsol(TT.cursor);
  check_cursor_bounds();
  return 1;
}

static int vi_H(int count0, int count1, char *unused)
{
  TT.cursor = text_sol(TT.screen);
  return 1;
}

static int vi_L(int count0, int count1, char *unused)
{
  TT.cursor = text_sol(TT.screen);
  cur_down(TT.screen_height-1, 1, 0);
  return 1;
}

static int vi_M(int count0, int count1, char *unused)
{
  TT.cursor = text_sol(TT.screen);
  cur_down(TT.screen_height/2, 1, 0);
  return 1;
}

static int search_str(char *s, int direction)
{
  size_t pos = text_strstr(TT.cursor+1, s, direction);

  if (TT.last_search != s) {
    free(TT.last_search);
    TT.last_search = xstrdup(s);
  }

  if (pos != SIZE_MAX) TT.cursor = pos;
  check_cursor_bounds();
  return 0;
}

static int vi_yy(char reg, int count0, int count1)
{
  size_t history = TT.cursor;
  size_t pos = text_sol(TT.cursor); //go left to first char on line
  TT.vi_mov_flag |= 4;

  for (;count0; count0--) TT.cursor = text_nsol(TT.cursor);

  vi_yank(reg, pos, 0);

  TT.cursor = history;
  return 1;
}

static int vi_dd(char reg, int count0, int count1)
{
  size_t pos = text_sol(TT.cursor); //go left to first char on line
  TT.vi_mov_flag |= 0x30000000;

  for (;count0; count0--) TT.cursor = text_nsol(TT.cursor);

  if (pos == TT.cursor && TT.filesize) pos--;
  vi_delete(reg, pos, 0);
  check_cursor_bounds();
  return 1;
}

static int vi_x(char reg, int count0, int count1)
{
  size_t from = TT.cursor;

  if (text_byte(TT.cursor) == '\n') {
    cur_left(count0-1, 1, 0);
  }
  else {
    cur_right(count0-1, 1, 0);
    if (text_byte(TT.cursor) == '\n') TT.vi_mov_flag |= 2;
    else cur_right(1, 1, 0);
  }

  vi_delete(reg, from, 0);
  check_cursor_bounds();
  return 1;
}

static int backspace(char reg, int count0, int count1)
{
  size_t from = 0;
  size_t to = TT.cursor;
  cur_left(1, 1, 0);
  from = TT.cursor;
  if (from != to)
    vi_delete(reg, to, 0);
  check_cursor_bounds();
  return 1;
}

static int vi_movw(int count0, int count1, char *unused)
{
  int count = count0*count1;
  while (count--) {
    char c = text_byte(TT.cursor);
    do {
      if (TT.cursor > TT.filesize-1) break;
      //if at empty jump to non empty
      if (c == '\n') {
        if (++TT.cursor > TT.filesize-1) break;
        if ((c = text_byte(TT.cursor)) == '\n') break;
        continue;
      } else if (strchr(blank, c)) do {
        if (++TT.cursor > TT.filesize-1) break;
        c = text_byte(TT.cursor);
      } while (strchr(blank, c));
      //if at special jump to non special
      else if (strchr(specials, c)) do {
        if (++TT.cursor > TT.filesize-1) break;
        c = text_byte(TT.cursor);
      } while (strchr(specials, c));
      //else jump to empty or spesial
      else do {
        if (++TT.cursor > TT.filesize-1) break;
        c = text_byte(TT.cursor);
      } while (c && !strchr(blank, c) && !strchr(specials, c));

    } while (strchr(blank, c) && c != '\n'); //never stop at empty
  }
  check_cursor_bounds();
  return 1;
}

static int vi_movb(int count0, int count1, char *unused)
{
  int count = count0*count1;
  int type = 0;
  char c;
  while (count--) {
    c = text_byte(TT.cursor);
    do {
      if (!TT.cursor) break;
      //if at empty jump to non empty
      if (strchr(blank, c)) do {
        if (!--TT.cursor) break;
        c = text_byte(TT.cursor);
      } while (strchr(blank, c));
      //if at special jump to non special
      else if (strchr(specials, c)) do {
        if (!--TT.cursor) break;
        type = 0;
        c = text_byte(TT.cursor);
      } while (strchr(specials, c));
      //else jump to empty or spesial
      else do {
        if (!--TT.cursor) break;
        type = 1;
        c = text_byte(TT.cursor);
      } while (!strchr(blank, c) && !strchr(specials, c));

    } while (strchr(blank, c)); //never stop at empty
  }
  //find first
  for (;TT.cursor; TT.cursor--) {
    c = text_byte(TT.cursor-1);
    if (type && !strchr(blank, c) && !strchr(specials, c)) break;
    else if (!type && !strchr(specials, c)) break;
  }

  TT.vi_mov_flag |= 0x80000000;
  check_cursor_bounds();
  return 1;
}

static int vi_move(int count0, int count1, char *unused)
{
  int count = count0*count1;
  int type = 0;
  char c;

  if (count>1) vi_movw(count-1, 1, unused);

  c = text_byte(TT.cursor);
  if (strchr(specials, c)) type = 1;
  TT.cursor++;
  for (;TT.cursor < TT.filesize-1; TT.cursor++) {
    c = text_byte(TT.cursor+1);
    if (!type && (strchr(blank, c) || strchr(specials, c))) break;
    else if (type && !strchr(specials, c)) break;
  }

  TT.vi_mov_flag |= 2;
  check_cursor_bounds();
  return 1;
}


static void i_insert(char *str, int len)
{
  if (!str || !len) return;

  insert_str(xstrdup(str), TT.cursor, len, len, HEAP);
  TT.cursor += len;
  TT.filesize = text_filesize();
  TT.vi_mov_flag |= 0x30000000;
}

static int vi_zero(int count0, int count1, char *unused)
{
  TT.cursor = text_sol(TT.cursor);
  TT.cur_col = 0;
  TT.vi_mov_flag |= 0x80000000;
  return 1;
}

static int vi_dollar(int count0, int count1, char *unused)
{
  size_t new = text_strchr(TT.cursor, '\n');

  if (new != TT.cursor) {
    TT.cursor = new - 1;
    TT.vi_mov_flag |= 2;
    check_cursor_bounds();
  }
  return 1;
}

static void vi_eol()
{
  TT.cursor = text_strchr(TT.cursor, '\n');
  check_cursor_bounds();
}

static void ctrl_b()
{
  int i;

  for (i=0; i<TT.screen_height-2; ++i) {
    TT.screen = text_psol(TT.screen);
    // TODO: retain x offset.
    TT.cursor = text_psol(TT.screen);
  }
}

static void ctrl_d()
{
  int i;

  for (i=0; i<(TT.screen_height-2)/2; ++i) TT.screen = text_nsol(TT.screen);
  // TODO: real vi keeps the x position.
  if (TT.screen > TT.cursor) TT.cursor = TT.screen;
}

static void ctrl_f()
{
  int i;

  for (i=0; i<TT.screen_height-2; ++i) TT.screen = text_nsol(TT.screen);
  // TODO: real vi keeps the x position.
  if (TT.screen > TT.cursor) TT.cursor = TT.screen;
}

static void ctrl_e()
{
  TT.screen = text_nsol(TT.screen);
  // TODO: real vi keeps the x position.
  if (TT.screen > TT.cursor) TT.cursor = TT.screen;
}

static void ctrl_y()
{
  TT.screen = text_psol(TT.screen);
  // TODO: only if we're on the bottom line
  TT.cursor = text_psol(TT.cursor);
  // TODO: real vi keeps the x position.
}

//TODO check register where to push from
static int vi_push(char reg, int count0, int count1)
{
  //if row changes during push original cursor position is kept
  //vi inconsistancy
  //if yank ends with \n push is linemode else push in place+1
  size_t history = TT.cursor;
  char *start = TT.yank.data, *eol = strchr(start, '\n');
  if (strlen(start) == 0) return 1;

  if (start[strlen(start)-1] == '\n') {
    if ((TT.cursor = text_strchr(TT.cursor, '\n')) == SIZE_MAX)
      TT.cursor = TT.filesize;
    else TT.cursor = text_nsol(TT.cursor);
  } else cur_right(1, 1, 0);

  i_insert(start, strlen(start));
  if (eol) {
    TT.vi_mov_flag |= 0x10000000;
    TT.cursor = history;
  }

  return 1;
}

static int vi_find_c(int count0, int count1, char *symbol)
{
////  int count = count0*count1;
  size_t pos = text_strchr(TT.cursor, *symbol);
  if (pos != SIZE_MAX) TT.cursor = pos;
  return 1;
}

static int vi_find_cb(int count0, int count1, char *symbol)
{
  // do backward search
  size_t pos = text_strrchr(TT.cursor, *symbol);
  if (pos != SIZE_MAX) TT.cursor = pos;
  return 1;
}

//if count is not spesified should go to last line
static int vi_go(int count0, int count1, char *symbol)
{
  size_t prev_cursor = TT.cursor;
  int count = count0*count1-1;
  TT.cursor = 0;

  if (TT.vi_mov_flag&0x40000000 && (TT.cursor = TT.filesize) > 0)
    TT.cursor = text_sol(TT.cursor-1);
  else if (count) {
    size_t next = 0;
    for ( ;count && (next = text_strchr(next+1, '\n')) != SIZE_MAX; count--)
      TT.cursor = next;
    TT.cursor++;
  }

  check_cursor_bounds();  //adjusts cursor column
  if (prev_cursor > TT.cursor) TT.vi_mov_flag |= 0x80000000;

  return 1;
}

static int vi_o(char reg, int count0, int count1)
{
  TT.cursor = text_eol(TT.cursor);
  insert_str(xstrdup("\n"), TT.cursor++, 1, 1, HEAP);
  TT.vi_mov_flag |= 0x30000000;
  TT.vi_mode = 2;
  return 1;
}

static int vi_O(char reg, int count0, int count1)
{
  TT.cursor = text_psol(TT.cursor);
  return vi_o(reg, count0, count1);
}

static int vi_D(char reg, int count0, int count1)
{
  size_t pos = TT.cursor;
  if (!count0) return 1;
  vi_eol();
  vi_delete(reg, pos, 0);
  if (--count0) vi_dd(reg, count0, 1);

  check_cursor_bounds();
  return 1;
}

static int vi_I(char reg, int count0, int count1)
{
  TT.cursor = text_sol(TT.cursor);
  TT.vi_mode = 2;
  return 1;
}

static int vi_join(char reg, int count0, int count1)
{
  size_t next;
  while (count0--) {
    //just strchr(/n) and cut_str(pos, 1);
    if ((next = text_strchr(TT.cursor, '\n')) == SIZE_MAX) break;
    TT.cursor = next+1;
    vi_delete(reg, TT.cursor-1, 0);
  }
  return 1;
}

static int vi_find_next(char reg, int count0, int count1)
{
  if (TT.last_search) search_str(TT.last_search, 1);
  return 1;
}

static int vi_find_prev(char reg, int count0, int count1)
{
  if (TT.last_search) search_str(TT.last_search, 0);
  return 1;
}

//NOTES
//vi-mode cmd syntax is
//("[REG])[COUNT0]CMD[COUNT1](MOV)
//where:
//-------------------------------------------------------------
//"[REG] is optional buffer where deleted/yanked text goes REG can be
//  atleast 0-9, a-z or default "
//[COUNT] is optional multiplier for cmd execution if there is 2 COUNT
//  operations they are multiplied together
//CMD is operation to be executed
//(MOV) is movement operation, some CMD does not require MOV and some
//  have special cases such as dd, yy, also movements can work without
//  CMD
//ex commands can be even more complicated than this....

//special cases without MOV and such
struct vi_special_param {
  const char *cmd;
  int (*vi_special)(char, int, int);//REG,COUNT0,COUNT1
} vi_special[] = {
  {"D", &vi_D},
  {"I", &vi_I},
  {"J", &vi_join},
  {"O", &vi_O},
  {"N", &vi_find_prev},
  {"n", &vi_find_next},
  {"o", &vi_o},
  {"p", &vi_push},
  {"x", &vi_x},
  {"dd", &vi_dd},
  {"yy", &vi_yy},
};
//there is around ~47 vi moves, some of them need extra params such as f and '
struct vi_mov_param {
  const char* mov;
  unsigned flags;
  int (*vi_mov)(int, int, char*);//COUNT0,COUNT1,params
} vi_movs[] = {
  {"0", 0, &vi_zero},
  {"b", 0, &vi_movb},
  {"e", 0, &vi_move},
  {"G", 0, &vi_go},
  {"H", 0, &vi_H},
  {"h", 0, &cur_left},
  {"j", 0, &cur_down},
  {"k", 0, &cur_up},
  {"L", 0, &vi_L},
  {"l", 0, &cur_right},
  {"M", 0, &vi_M},
  {"w", 0, &vi_movw},
  {"$", 0, &vi_dollar},
  {"f", 1, &vi_find_c},
  {"F", 1, &vi_find_cb},
};
// change and delete unfortunately behave different depending on move command,
// such as ce cw are same, but dw and de are not...
// also dw stops at w position and cw seem to stop at e pos+1...
// so after movement we need to possibly set up some flags before executing
// command, and command needs to adjust...
struct vi_cmd_param {
  const char* cmd;
  unsigned flags;
  int (*vi_cmd)(char, size_t, int);//REG,from,FLAGS
} vi_cmds[] = {
  {"c", 1, &vi_change},
  {"d", 1, &vi_delete},
  {"y", 1, &vi_yank},
};

static int run_vi_cmd(char *cmd)
{
  int i = 0, val = 0;
  char *cmd_e;
  int (*vi_cmd)(char, size_t, int) = 0, (*vi_mov)(int, int, char*) = 0;

  TT.count0 = 0, TT.count1 = 0, TT.vi_mov_flag = 0;
  TT.vi_reg = '"';

  if (*cmd == '"') {
    cmd++;
    TT.vi_reg = *cmd++; //TODO check validity
  }
  errno = 0;
  val = strtol(cmd, &cmd_e, 10);
  if (errno || val == 0) val = 1, TT.vi_mov_flag |= 0x40000000;
  else cmd = cmd_e;
  TT.count0 = val;

  for (i = 0; i < ARRAY_LEN(vi_special); i++)
    if (strstr(cmd, vi_special[i].cmd))
      return vi_special[i].vi_special(TT.vi_reg, TT.count0, TT.count1);

  for (i = 0; i < ARRAY_LEN(vi_cmds); i++) {
    if (!strncmp(cmd, vi_cmds[i].cmd, strlen(vi_cmds[i].cmd))) {
      vi_cmd = vi_cmds[i].vi_cmd;
      cmd += strlen(vi_cmds[i].cmd);
      break;
    }
  }
  errno = 0;
  val = strtol(cmd, &cmd_e, 10);
  if (errno || val == 0) val = 1;
  else cmd = cmd_e;
  TT.count1 = val;

  for (i = 0; i < ARRAY_LEN(vi_movs); i++) {
    if (!strncmp(cmd, vi_movs[i].mov, strlen(vi_movs[i].mov))) {
      vi_mov = vi_movs[i].vi_mov;
      TT.vi_mov_flag |= vi_movs[i].flags;
      cmd++;
      if (TT.vi_mov_flag&1 && !(*cmd)) return 0;
      break;
    }
  }
  if (vi_mov) {
    int prev_cursor = TT.cursor;
    if (vi_mov(TT.count0, TT.count1, cmd)) {
      if (vi_cmd) return (vi_cmd(TT.vi_reg, prev_cursor, TT.vi_mov_flag));
      else return 1;
    } else return 0; //return some error
  }
  return 0;
}


static void draw_page();

static int get_endline(void)
{
  int cln, rln;

  draw_page();
  cln = TT.cur_row+1;
  run_vi_cmd("G");
  draw_page();
  rln =  TT.cur_row+1;
  run_vi_cmd(xmprintf("%dG", cln));

  return rln+1;
}

// Return non-zero to exit.
static int run_ex_cmd(char *cmd)
{
  int startline = 1, ofst = 0, endline;

  if (*cmd == '/' || *cmd == '\?') search_str(cmd+1, *cmd == '/' ? 0 : 1);
  else if (*cmd == ':') {
    if (cmd[1] == 'q') {
      if (cmd[2] != '!' && modified())
        show_error("Unsaved changes (\"q!\" to ignore)");
      else return 1;
    } else if (!strncmp(cmd+1, "w ", 2)) write_file(&cmd[3]);
    else if (!strncmp(cmd+1, "wq", 2)) {
      if (write_file(0)) return 1;
      show_error("Unsaved changes (\"q!\" to ignore)");
    } else if (!strncmp(cmd+1, "w", 1)) write_file(0);

    else if (!strncmp(cmd+1, "set list", sizeof("set list"))) {
      TT.list = 1;
      TT.vi_mov_flag |= 0x30000000;
    } else if (!strncmp(cmd+1, "set nolist", sizeof("set nolist"))) {
      TT.list = 0;
      TT.vi_mov_flag |= 0x30000000;
    }

    else if (cmd[1] == 'd') {
      run_vi_cmd("dd");
      cur_up(1, 1, 0);
    } else if (cmd[1] == 'j') run_vi_cmd("J");
    else if (cmd[1] == 'g' || cmd[1] == 'v') {
      char *rgx = xmalloc(strlen(cmd));
      int el = get_endline(), ln = 0, vorg = (cmd[1] == 'v' ? REG_NOMATCH : 0);
      if (sscanf(cmd+2, "/%[^/]/%[^\ng]", rgx, cmd+1) == 2) {
        regex_t rgxc;
        if (!regcomp(&rgxc, rgx, 0)) {
          cmd[0] = ':';

          for (; ln < el; ln++) {
            run_vi_cmd("yy");
            if (regexec(&rgxc, TT.yank.data, 0, 0, 0) == vorg) run_ex_cmd(cmd);
            cur_down(1, 1, 0);
          }

          // Reset Frame
          TT.vi_mov_flag |= 0x30000000;
        }
        regfree(&rgxc);
      }
      free(rgx);
    }

    // Line Ranges
    else if (cmd[1] >= '0' && cmd[1] <= '9') {
      if (strstr(cmd, ",")) {
        sscanf(cmd, ":%d,%d%[^\n]", &startline, &endline, cmd+2);
        ofst = 1;
      } else run_vi_cmd(xmprintf("%dG", atoi(cmd+1)));
    } else if (cmd[1] == '$') run_vi_cmd("G");
    else if (cmd[1] == '%') {
      endline = get_endline();
      ofst = 1;
    } else show_error("unknown command '%s'",cmd+1);

    if (ofst) {
      int cline = TT.cur_row+1;

      cmd[ofst] = ':';
      for (; startline <= endline; startline++) {
        run_ex_cmd(cmd+ofst);
        cur_down(1, 1, 0);
      }
      run_vi_cmd(xmprintf("%dG", cline));
      // Screen Reset
      TT.vi_mov_flag |= 0x30000000;
    }
  }
  return 0;
}

static int vi_crunch(FILE *out, int cols, int wc)
{
  int ret = 0;
  if (wc < 32 && TT.list) {
    xputsn("\e[1m");
    ret = crunch_escape(out,cols,wc);
    xputsn("\e[m");
  } else if (wc == '\t') {
    if (out) {
      int i = TT.tabstop;
      for (;i--;) fputs(" ", out);
    }
    ret = TT.tabstop;
  } else if (wc == '\n') return 0;
  return ret;
}

//crunch_str with n bytes restriction for printing substrings or
//non null terminated strings
static int crunch_nstr(char **str, int width, int n, FILE *out, char *escmore,
  int (*escout)(FILE *out, int cols, int wc))
{
  int columns = 0, col, bytes;
  char *start, *end;
  unsigned wc;

  for (end = start = *str; *end && n>0; columns += col, end += bytes, n -= bytes) {
    if ((bytes = utf8towc(&wc, end, 4))>0 && (col = wcwidth(wc))>=0) {
      if (!escmore || wc>255 || !strchr(escmore, wc)) {
        if (width-columns<col) break;
        if (out) fwrite(end, bytes, 1, out);

        continue;
      }
    }

    if (bytes<1) {
      bytes = 1;
      wc = *end;
    }
    col = width-columns;
    if (col<1) break;
    if (escout) {
      if ((col = escout(out, col, wc))<0) break;
    } else if (out) fwrite(end, 1, bytes, out);
  }
  *str = end;

  return columns;
}

static void draw_page()
{
  unsigned y = 0;
  int x = 0, bytes = 0;
  char *line = 0, *end = 0;
  //screen coordinates for cursor
  int cy_scr = 0, cx_scr = 0;
  //variables used only for cursor handling
  int aw = 0, iw = 0, clip = 0, margin = 8, scroll = 0, redraw = 0, SSOL, SOL;

  adjust_screen_buffer();
  //redraw = 3; //force full redraw
  redraw = (TT.vi_mov_flag & 0x30000000)>>28;

  scroll = TT.drawn_row-TT.scr_row;
  if (TT.drawn_row<0 || TT.cur_row<0 || TT.scr_row<0) redraw = 3;
  else if (abs(scroll)>TT.screen_height/2) redraw = 3;

  xputsn("\e[H"); // jump to top left
  if (redraw&2) xputsn("\e[2J\e[H");   //clear screen
  else if (scroll>0) printf("\e[%dL", scroll);  //scroll up
  else if (scroll<0) printf("\e[%dM", -scroll); //scroll down

  SOL = text_sol(TT.cursor);
  bytes = text_getline(toybuf, SOL, ARRAY_LEN(toybuf));
  line = toybuf;

  for (SSOL = TT.screen, y = 0; SSOL < SOL; y++) SSOL = text_nsol(SSOL);

  cy_scr = y;

  // draw cursor row
  /////////////////////////////////////////////////////////////
  // for long lines line starts to scroll when cursor hits margin
  bytes = TT.cursor-SOL; // TT.cur_col;
  end = line;


  printf("\e[%u;0H\e[2K", y+1);
  // find cursor position
  aw = crunch_nstr(&end, INT_MAX, bytes, 0, "\t\n", vi_crunch);

  // if we need to render text that is not inserted to buffer yet
  if (TT.vi_mode == 2 && TT.il->len) {
    char* iend = TT.il->data; //input end
    x = 0;
    // find insert end position
    iw = crunch_str(&iend, INT_MAX, 0, "\t\n", vi_crunch);
    clip = (aw+iw) - TT.screen_width+margin;

    // if clipped area is bigger than text before insert
    if (clip > aw) {
      clip -= aw;
      iend = TT.il->data;

      iw -= crunch_str(&iend, clip, 0, "\t\n", vi_crunch);
      x = crunch_str(&iend, iw, stdout, "\t\n", vi_crunch);
    } else {
      iend = TT.il->data;
      end = line;

      //if clipped area is substring from cursor row start
      aw -= crunch_nstr(&end, clip, bytes, 0, "\t\n", vi_crunch);
      x = crunch_str(&end, aw,  stdout, "\t\n", vi_crunch);
      x += crunch_str(&iend, iw, stdout, "\t\n", vi_crunch);
    }
  }
  // when not inserting but still need to keep cursor inside screen
  // margin area
  else if ( aw+margin > TT.screen_width) {
    clip = aw-TT.screen_width+margin;
    end = line;
    aw -= crunch_nstr(&end, clip, bytes, 0, "\t\n", vi_crunch);
    x = crunch_str(&end, aw,  stdout, "\t\n", vi_crunch);
  }
  else {
    end = line;
    x = crunch_nstr(&end, aw, bytes, stdout, "\t\n", vi_crunch);
  }
  cx_scr = x;
  cy_scr = y;
  x += crunch_str(&end, TT.screen_width-x,  stdout, "\t\n", vi_crunch);

  // start drawing all other rows that needs update
  ///////////////////////////////////////////////////////////////////
  y = 0, SSOL = TT.screen, line = toybuf;
  bytes = text_getline(toybuf, SSOL, ARRAY_LEN(toybuf));

  // if we moved around in long line might need to redraw everything
  if (clip != TT.drawn_col) redraw = 3;

  for (; y < TT.screen_height; y++ ) {
    int draw_line = 0;
    if (SSOL == SOL) {
      line = toybuf;
      SSOL += bytes+1;
      bytes = text_getline(line, SSOL, ARRAY_LEN(toybuf));
      continue;
    } else if (redraw) draw_line++;
    else if (scroll<0 && TT.screen_height-y-1<-scroll)
      scroll++, draw_line++;
    else if (scroll>0) scroll--, draw_line++;

    printf("\e[%u;0H", y+1);
    if (draw_line) {
      printf("\e[2K");
      if (line && strlen(line)) {
        aw = crunch_nstr(&line, clip, bytes, 0, "\t\n", vi_crunch);
        crunch_str(&line, TT.screen_width-1, stdout, "\t\n", vi_crunch);
        if ( *line ) printf("@");
      } else printf("\e[2m~\e[m");
    }
    if (SSOL+bytes < TT.filesize)  {
      line = toybuf;
      SSOL += bytes+1;
      bytes = text_getline(line, SSOL, ARRAY_LEN(toybuf));
    } else line = 0;
  }

  TT.drawn_row = TT.scr_row, TT.drawn_col = clip;

  // Finished updating visual area, show status line.
  printf("\e[%u;0H\e[2K", TT.screen_height+1);
  if (TT.vi_mode == 2) printf("\e[1m-- INSERT --\e[m");
  if (!TT.vi_mode) {
    cx_scr = printf("%s", TT.il->data);
    cy_scr = TT.screen_height;
    *toybuf = 0;
  } else {
    // TODO: the row,col display doesn't show the cursor column
    // TODO: real vi shows the percentage by lines, not bytes
    sprintf(toybuf, "%zu/%zuC  %zu%%  %d,%d", TT.cursor, TT.filesize,
      (100*TT.cursor)/(TT.filesize ? : 1), TT.cur_row+1, TT.cur_col+1);
    if (TT.cur_col != cx_scr) sprintf(toybuf+strlen(toybuf),"-%d", cx_scr+1);
  }
  printf("\e[%u;%uH%s\e[%u;%uH", TT.screen_height+1,
    (int) (1+TT.screen_width-strlen(toybuf)),
    toybuf, cy_scr+1, cx_scr+1);
  fflush(0);
  xferror(stdout);
}

void vi_main(void)
{
  char keybuf[16] = {0}, vi_buf[16] = {0}, utf8_code[8] = {0};
  int utf8_dec_p = 0, vi_buf_pos = 0;
  FILE *script = TT.s ? xfopen(TT.s, "r") : 0;

  TT.il = xzalloc(sizeof(struct str_line));
  TT.il->data = xzalloc(TT.il->alloc = 80);
  TT.yank.data = xzalloc(TT.yank.alloc = 128);

  TT.filename = *toys.optargs;
  linelist_load(0, 1);

  TT.vi_mov_flag = 0x20000000;
  TT.vi_mode = 1, TT.tabstop = 8;

  TT.screen_width = 80, TT.screen_height = 24;
  terminal_size(&TT.screen_width, &TT.screen_height);
  TT.screen_height -= 1;

  xsignal(SIGWINCH, generic_signal);
  set_terminal(0, 1, 0, 0);
  //writes stdout into different xterm buffer so when we exit
  //we dont get scroll log full of junk
  xputsn("\e[?1049h");

  if (TT.c) {
    FILE *cc = xfopen(TT.c, "r");
    char *line;

    while ((line = xgetline(cc))) if (run_ex_cmd(TT.il->data)) goto cleanup_vi;
    fclose(cc);
  }

  for (;;) {
    int key = 0;

    draw_page();
    // TODO script should handle cursor keys
    if (script && EOF==(key = fgetc(script))) {
      fclose(script);
      script = 0;
    }
    if (!script) key = scan_key(keybuf, -1);

    if (key == -1) goto cleanup_vi;
    else if (key == -3) {
      toys.signal = 0;
      terminal_size(&TT.screen_width, &TT.screen_height);
      TT.screen_height -= 1; //TODO this is hack fix visual alignment
      continue;
    }

    // TODO: support cursor keys in ex mode too.
    if (TT.vi_mode && key>=256) {
      key -= 256;
      //if handling arrow keys insert what ever is in input buffer before moving
      if (TT.il->len) {
        i_insert(TT.il->data, TT.il->len);
        TT.il->len = 0;
        memset(TT.il->data, 0, TT.il->alloc);
      }
      if (key==KEY_UP) cur_up(1, 1, 0);
      else if (key==KEY_DOWN) cur_down(1, 1, 0);
      else if (key==KEY_LEFT) cur_left(1, 1, 0);
      else if (key==KEY_RIGHT) cur_right(1, 1, 0);
      else if (key==KEY_HOME) vi_zero(1, 1, 0);
      else if (key==KEY_END) vi_dollar(1, 1, 0);
      else if (key==KEY_PGDN) ctrl_f();
      else if (key==KEY_PGUP) ctrl_b();

      continue;
    }

    if (TT.vi_mode == 1) { //NORMAL
      switch (key) {
        case '/':
        case '?':
        case ':':
          TT.vi_mode = 0;
          TT.il->data[0]=key;
          TT.il->len++;
          break;
        case 'A':
          vi_eol();
          TT.vi_mode = 2;
          break;
        case 'a':
          cur_right(1, 1, 0);
          // FALLTHROUGH
        case 'i':
          TT.vi_mode = 2;
          break;
        case CTL('D'):
          ctrl_d();
          break;
        case CTL('B'):
          ctrl_b();
          break;
        case CTL('E'):
          ctrl_e();
          break;
        case CTL('F'):
          ctrl_f();
          break;
        case CTL('Y'):
          ctrl_y();
          break;
        case '\e':
          vi_buf[0] = 0;
          vi_buf_pos = 0;
          break;
        case 0x7F: //FALLTHROUGH
        case '\b':
          backspace(TT.vi_reg, 1, 1);
          break;
        default:
          if (key > ' ' && key < '{') {
            vi_buf[vi_buf_pos] = key;//TODO handle input better
            vi_buf_pos++;
            if (run_vi_cmd(vi_buf)) {
              memset(vi_buf, 0, 16);
              vi_buf_pos = 0;
            }
            else if (vi_buf_pos == 15) {
              vi_buf_pos = 0;
              memset(vi_buf, 0, 16);
            }

          }

          break;
      }
    } else if (TT.vi_mode == 0) { //EX MODE
      switch (key) {
        case '\x7f':
        case '\b':
          if (TT.il->len > 1) {
            TT.il->data[--TT.il->len] = 0;
            break;
          }
          // FALLTHROUGH
        case '\e':
          TT.vi_mode = 1;
          TT.il->len = 0;
          memset(TT.il->data, 0, TT.il->alloc);
          break;
        case '\n':
        case '\r':
          if (run_ex_cmd(TT.il->data)) goto cleanup_vi;
          TT.vi_mode = 1;
          TT.il->len = 0;
          memset(TT.il->data, 0, TT.il->alloc);
          break;
        default: //add chars to ex command until ENTER
          if (key >= ' ' && key < 0x7F) { //might be utf?
            if (TT.il->len == TT.il->alloc) {
              TT.il->data = realloc(TT.il->data, TT.il->alloc*2);
              TT.il->alloc *= 2;
            }
            TT.il->data[TT.il->len] = key;
            TT.il->len++;
          }
          break;
      }
    } else if (TT.vi_mode == 2) {//INSERT MODE
      switch (key) {
        case '\e':
          i_insert(TT.il->data, TT.il->len);
          cur_left(1, 1, 0);
          TT.vi_mode = 1;
          TT.il->len = 0;
          memset(TT.il->data, 0, TT.il->alloc);
          break;
        case 0x7F:
        case '\b':
          if (TT.il->len) {
            char *last = utf8_last(TT.il->data, TT.il->len);
            int shrink = strlen(last);
            memset(last, 0, shrink);
            TT.il->len -= shrink;
          } else backspace(TT.vi_reg, 1, 1);
          break;
        case '\n':
        case '\r':
          //insert newline
          TT.il->data[TT.il->len++] = '\n';
          i_insert(TT.il->data, TT.il->len);
          TT.il->len = 0;
          memset(TT.il->data, 0, TT.il->alloc);
          break;
        default:
          if ((key >= ' ' || key == '\t') &&
              utf8_dec(key, utf8_code, &utf8_dec_p))
          {
            if (TT.il->len+utf8_dec_p+1 >= TT.il->alloc) {
              TT.il->data = realloc(TT.il->data, TT.il->alloc*2);
              TT.il->alloc *= 2;
            }
            strcpy(TT.il->data+TT.il->len, utf8_code);
            TT.il->len += utf8_dec_p;
            utf8_dec_p = 0;
            *utf8_code = 0;
          }
          break;
      }
    }
  }
cleanup_vi:
  linelist_unload();
  free(TT.il->data), free(TT.il), free(TT.yank.data);
  tty_reset();
  xputsn("\e[?1049l");
}
