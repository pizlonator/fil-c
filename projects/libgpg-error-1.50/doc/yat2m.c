/* yat2m.c - Yet Another Texi 2 Man converter
 *	Copyright (C) 2005, 2013, 2015, 2016, 2017, 2023 g10 Code GmbH
 *      Copyright (C) 2006, 2008, 2011 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

/*
    This is a simple texinfo to man page converter.  It needs some
    special markup in th e texinfo and tries best to get a create man
    page.  It has been designed for the GnuPG man pages and thus only
    a few texinfo commands are supported.

    To use this you need to add the following macros into your texinfo
    source:

      @macro manpage {a}
      @end macro
      @macro mansect {a}
      @end macro
      @macro manpause
      @end macro
      @macro mancont
      @end macro

    They are used by yat2m to select parts of the Texinfo which should
    go into the man page. These macros need to be used without leading
    left space. Processing starts after a "manpage" macro has been
    seen.  "mansect" identifies the section and yat2m make sure to
    emit the sections in the proper order.  Note that @mansect skips
    the next input line if that line begins with @section, @subsection or
    @chapheading.

    To insert verbatim troff markup, the following texinfo code may be
    used:

      @ifset manverb
      .B whateever you want
      @end ifset

    alternatively a special comment may be used:

      @c man:.B whatever you want

    This is useful in case you need just one line. If you want to
    include parts only in the man page but keep the texinfo
    translation you may use:

      @ifset isman
      stuff to be rendered only on man pages
      @end ifset

    or to exclude stuff from man pages:

      @ifclear isman
      stuff not to be rendered on man pages
      @end ifclear

    the keyword @section is ignored, however @subsection gets rendered
    as ".SS".  @menu is completely skipped. Several man pages may be
    extracted from one file, either using the --store or the --select
    option.

    If you want to indent tables in the source use this style:

      @table foo
        @item
        @item
        @table
          @item
        @end
      @end

    Don't change the indentation within a table and keep the same
    number of white space at the start of the line.  yat2m simply
    detects the number of white spaces in front of an @item and removes
    this number of spaces from all following lines until a new @item
    is found or there are less spaces than for the last @item.

    Note that @* does only work correctly if used at the end of an
    input line.

*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>


#if __GNUC__
# define MY_GCC_VERSION (__GNUC__ * 10000 \
                         + __GNUC_MINOR__ * 100         \
                         + __GNUC_PATCHLEVEL__)
#else
# define MY_GCC_VERSION 0
#endif

#if MY_GCC_VERSION >= 20500
# define ATTR_PRINTF(f, a) __attribute__ ((format(printf,f,a)))
# define ATTR_NR_PRINTF(f, a) __attribute__ ((__noreturn__, format(printf,f,a)))
#else
# define ATTR_PRINTF(f, a)
# define ATTR_NR_PRINTF(f, a)
#endif
#if MY_GCC_VERSION >= 30200
# define ATTR_MALLOC  __attribute__ ((__malloc__))
#else
# define ATTR_MALLOC
#endif
#if __GNUC__ >= 4
# define ATTR_SENTINEL(a) __attribute__ ((sentinel(a)))
#else
# define ATTR_SENTINEL(a)
#endif
#define DIM(v)		     (sizeof(v)/sizeof((v)[0]))
#define DIMof(type,member)   DIM(((type *)0)->member)
#define spacep(p)   (*(p) == ' ' || *(p) == '\t')
#define digitp(p)   (*(p) >= '0' && *(p) <= '9')


#define PGM "yat2m"
#ifdef PACKAGE_VERSION
# define VERSION PACKAGE_VERSION
#else
# define VERSION "1.1"
#endif

/* The maximum length of a line including the linefeed and one extra
   character. */
#define LINESIZE 1024

/* Number of allowed condition nestings.  */
#define MAX_CONDITION_NESTING  10

/* Number of allowed table nestings.  */
#define MAX_TABLE_NESTING  10

static char const default_css[] =
  "<style type=\"text/css\">\n"
  "  .y2m {\n"
  "    font-family: monospace;\n"
  "  }\n"
  "  .y2m u {\n"
  "    text-decoration: underline;\n"
  "  }\n"
  "  .y2m-sc {\n"
  "    font-variant: small-caps;\n"
  "  }\n"
  "  .y2m li {\n"
  "    margin-top: 1em;\n"
  "  }\n"
  "  .y2m-item {\n"
  "     display: block;\n"
  "     font-weight: bold;\n"
  "  }\n"
  "  .y2m-args {\n"
  "     font-weight: normal;\n"
  "  }\n"
  "</style>\n";



/* Option flags. */
static int verbose;
static int quiet;
static int debug;
static int htmlmode;
static int gnupgorgmode;
static const char *opt_source;
static const char *opt_release;
static const char *opt_date;
static const char *opt_select;
static const char *opt_include;
static int opt_store;

/* Flag to keep track whether any error occurred.  */
static int any_error;


/* Memory buffer stuff.  */
struct private_membuf_s
{
  size_t len;
  size_t size;
  char *buf;
  int out_of_core;
};
typedef struct private_membuf_s membuf_t;


/* Object to keep macro definitions.  */
struct macro_s
{
  struct macro_s *next;
  char *value;    /* Malloced value. */
  char name[1];
};
typedef struct macro_s *macro_t;

/* List of all defined macros. */
static macro_t macrolist;

/* List of variables set by @set. */
static macro_t variablelist;

/* List of global macro names.  The value part is not used.  */
static macro_t predefinedmacrolist;

/* Object to keep track of @isset and @ifclear.  */
struct condition_s
{
  int manverb;   /* "manverb" needs special treatment.  */
  int isset;     /* This is an @isset condition.  */
  char name[1];  /* Name of the condition macro.  */
};
typedef struct condition_s *condition_t;

/* The stack used to evaluate conditions.  And the current states. */
static condition_t condition_stack[MAX_CONDITION_NESTING];
static int condition_stack_idx;
static int cond_is_active;     /* State of ifset/ifclear */
static int cond_in_verbatim;   /* State of "manverb".  */

/* The stack used to evaluate table setting for @item and @itemx.
 * 1: argument of @item should be handled \- as minus
 * 0: argument of @item should be handled - as hyphen
 */
static int table_item_stack[MAX_TABLE_NESTING];

/* Variable to parse en-dash and em-dash.
 * -1: not parse until the end of the line
 *  0: not parse
 *  1: parse -- for en-dash, --- for em-dash
 */
static int cond_parse_dash;

/* Variable to generate \- (minus) for roff, which may distinguishes
 * minus and hyphen.
 * > 0 : emit \-
 *   0 : emit -
 */
static int cond_2D_as_minus;

/* Object to store one line of content.  */
struct line_buffer_s
{
  struct line_buffer_s *next;
  int verbatim;  /* True if LINE contains verbatim data.  The default
                    is Texinfo source.  */
  char *line;
};
typedef struct line_buffer_s *line_buffer_t;


/* Object to collect the data of a section.  */
struct section_buffer_s
{
  char *name;           /* Malloced name of the section. This may be
                           NULL to indicate this slot is not used.  */
  line_buffer_t lines;  /* Linked list with the lines of the section.  */
  line_buffer_t *lines_tail; /* Helper for faster appending to the
                                linked list.  */
  line_buffer_t last_line;   /* Points to the last line appended.  */
  unsigned int is_see_also:1; /* This is the SEE ALSO section.  */
  unsigned int in_para:1;     /* Track whether we are in paragraph.  */
  unsigned int in_pre:1;      /* Track whether we are in a html pre.  */
};
typedef struct section_buffer_s *section_buffer_t;

/* Variable to keep info about the current page together.  */
static struct
{
  /* Filename of the current page or NULL if no page is active.  Malloced. */
  char *name;

  /* Number of allocated elements in SECTIONS below.  */
  size_t n_sections;
  /* Array with the data of the sections.  */
  section_buffer_t sections;

} thepage;


/* The list of standard section names.  COMMANDS and ASSUAN are GnuPG
   specific. */
static const char * const standard_sections[] =
  { "NAME",  "SYNOPSIS",  "DESCRIPTION",
    "RETURN VALUE", "EXIT STATUS", "ERROR HANDLING", "ERRORS",
    "COMMANDS", "OPTIONS", "USAGE", "EXAMPLES", "FILES",
    "ENVIRONMENT", "DIAGNOSTICS", "SECURITY", "CONFORMING TO",
    "ASSUAN", "NOTES", "BUGS", "AUTHOR", "SEE ALSO", NULL };


/* Actions to be done at the end of a line.  */
enum eol_actions
  {
   EOL_NOTHING = 0,
   EOL_CLOSE_SUBSECTION
  };


/*-- Local prototypes.  --*/
static void proc_texi_buffer (FILE *fp, const char *line, size_t len,
                              int *table_level, int *eol_action,
                              section_buffer_t sect, int char_2D_is_minus);

static void die (const char *format, ...) ATTR_NR_PRINTF(1,2);
static void err (const char *format, ...) ATTR_PRINTF(1,2);
static void inf (const char *format, ...) ATTR_PRINTF(1,2);
static void *xmalloc (size_t n) ATTR_MALLOC;
static void *xcalloc (size_t n, size_t m) ATTR_MALLOC;
static char *xstrconcat (const char *s1, ...) ATTR_SENTINEL(0);



/*-- Functions --*/

/* Print diagnostic message and exit with failure. */
static void
die (const char *format, ...)
{
  va_list arg_ptr;

  fflush (stdout);
  fprintf (stderr, "%s: ", PGM);

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  putc ('\n', stderr);

  exit (1);
}


/* Print diagnostic message. */
static void
err (const char *format, ...)
{
  va_list arg_ptr;

  fflush (stdout);
  if (strncmp (format, "%s:%d:", 6))
    fprintf (stderr, "%s: ", PGM);

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  putc ('\n', stderr);
  any_error = 1;
}

/* Print diagnostic message. */
static void
inf (const char *format, ...)
{
  va_list arg_ptr;

  fflush (stdout);
  fprintf (stderr, "%s: ", PGM);

  va_start (arg_ptr, format);
  vfprintf (stderr, format, arg_ptr);
  va_end (arg_ptr);
  putc ('\n', stderr);
}


static void *
xmalloc (size_t n)
{
  void *p = malloc (n);
  if (!p)
    die ("out of core: %s", strerror (errno));
  return p;
}

static void *
xcalloc (size_t n, size_t m)
{
  void *p = calloc (n, m);
  if (!p)
    die ("out of core: %s", strerror (errno));
  return p;
}

static void *
xrealloc (void *old, size_t n)
{
  void *p = realloc (old, n);
  if (!p)
    die ("out of core: %s", strerror (errno));
  return p;
}

static char *
xstrdup (const char *string)
{
  void *p = malloc (strlen (string)+1);
  if (!p)
    die ("out of core: %s", strerror (errno));
  strcpy (p, string);
  return p;
}

static char *
mystpcpy (char *a,const char *b)
{
  while( *b )
    *a++ = *b++;
  *a = 0;

  return (char*)a;
}


/* Helper for xstrconcat and strconcat.  */
static char *
do_strconcat (int xmode, const char *s1, va_list arg_ptr)
{
  const char *argv[48];
  size_t argc;
  size_t needed;
  char *buffer, *p;

  argc = 0;
  argv[argc++] = s1;
  needed = strlen (s1);
  while (((argv[argc] = va_arg (arg_ptr, const char *))))
    {
      needed += strlen (argv[argc]);
      if (argc >= DIM (argv)-1)
        die ("too may args for strconcat\n");
      argc++;
    }
  needed++;
  buffer = xmode? xmalloc (needed) : malloc (needed);
  for (p = buffer, argc=0; argv[argc]; argc++)
    p = mystpcpy (p, argv[argc]);

  return buffer;
}


/* Concatenate the string S1 with all the following strings up to a
 * NULL.  Returns a malloced buffer with the new string or die.  */
static char *
xstrconcat (const char *s1, ...)
{
  va_list arg_ptr;
  char *result;

  if (!s1)
    result = xstrdup ("");
  else
    {
      va_start (arg_ptr, s1);
      result = do_strconcat (1, s1, arg_ptr);
      va_end (arg_ptr);
    }
  return result;
}


/* A simple implementation of a dynamic buffer.  Use init_membuf() to
 * create a buffer, put_membuf to append bytes and get_membuf to
 * release and return the buffer.  Allocation errors are detected but
 * only returned at the final get_membuf(), this helps not to clutter
 * the code with out of core checks.  */
static void
init_membuf (membuf_t *mb, int initiallen)
{
  mb->len = 0;
  mb->size = initiallen;
  mb->out_of_core = 0;
  mb->buf = malloc (initiallen);
  if (!mb->buf)
    mb->out_of_core = errno;
}


/* Store (BUF,LEN) in the memory buffer MB.  */
static void
put_membuf (membuf_t *mb, const void *buf, size_t len)
{
  if (mb->out_of_core || !len)
    return;

  if (mb->len + len >= mb->size)
    {
      char *p;

      mb->size += len + 1024;
      p = realloc (mb->buf, mb->size);
      if (!p)
        {
          mb->out_of_core = errno ? errno : ENOMEM;
          return;
        }
      mb->buf = p;
    }
  if (buf)
    memcpy (mb->buf + mb->len, buf, len);
  else
    memset (mb->buf + mb->len, 0, len);
  mb->len += len;
}


/* Store STRING in the memory buffer MB.  */
static void
put_membuf_str (membuf_t *mb, const char *string)
{
  if (!string)
    string= "";
  put_membuf (mb, string, strlen (string));
}


/* Return the value of a memory buffer.  LEN is optional.  */
static void *
get_membuf (membuf_t *mb, size_t *len)
{
  char *p;

  if (mb->out_of_core)
    {
      if (mb->buf)
        {
          free (mb->buf);
          mb->buf = NULL;
        }
      errno = mb->out_of_core;
    }

  p = mb->buf;
  if (len)
    *len = mb->len;
  mb->buf = NULL;
  mb->out_of_core = ENOMEM; /* hack to make sure it won't get reused. */
  return p;
}


/* Return a STRING from the memory buffer MB or die.  */
static char *
xget_membuf (membuf_t *mb)
{
  char *p;
  put_membuf (mb, "", 1);
  p = get_membuf (mb, NULL);
  if (!p)
    die ("out of core in xget_membuf: %s", strerror (errno));
  return p;
}


/* Uppercase the ascii characters in STRING.  */
static char *
ascii_strupr (char *string)
{
  char *p;

  for (p = string; *p; p++)
    if (!(*p & 0x80))
      *p = toupper (*p);
  return string;
}


/* Return the current date as an ISO string.  */
const char *
isodatestring (void)
{
  static char buffer[36];
  struct tm *tp;
  time_t atime;

  if (opt_date && *opt_date)
    atime = strtoul (opt_date, NULL, 10);
  else
    atime = time (NULL);
  if (atime < 0)
    strcpy (buffer, "????" "-??" "-??");
  else
    {
      tp = gmtime (&atime);
      sprintf (buffer,"%04d-%02d-%02d",
               1900+tp->tm_year, tp->tm_mon+1, tp->tm_mday );
    }
  return buffer;
}


/* Add NAME to the list of predefined macros which are global for all
   files.  */
static void
add_predefined_macro (const char *name)
{
  macro_t m;

  for (m=predefinedmacrolist; m; m = m->next)
    if (!strcmp (m->name, name))
      break;
  if (!m)
    {
      m = xcalloc (1, sizeof *m + strlen (name));
      strcpy (m->name, name);
      m->next = predefinedmacrolist;
      predefinedmacrolist = m;
    }
}


/* Create or update a macro with name MACRONAME and set its values TO
   MACROVALUE.  Note that ownership of the macro value is transferred
   to this function.  */
static void
set_macro (const char *macroname, char *macrovalue)
{
  macro_t m;

  for (m=macrolist; m; m = m->next)
    if (!strcmp (m->name, macroname))
      break;
  if (m)
    free (m->value);
  else
    {
      m = xcalloc (1, sizeof *m + strlen (macroname));
      strcpy (m->name, macroname);
      m->next = macrolist;
      macrolist = m;
    }
  m->value = macrovalue;
  macrovalue = NULL;
}


/* Create or update a variable with name and value given in NAMEANDVALUE.  */
static void
set_variable (char *nameandvalue)
{
  macro_t m;
  const char *value;
  char *p;

  for (p = nameandvalue; *p && *p != ' ' && *p != '\t'; p++)
    ;
  if (!*p)
    value = "";
  else
    {
      *p++ = 0;
      while (*p == ' ' || *p == '\t')
        p++;
      value = p;
    }

  for (m=variablelist; m; m = m->next)
    if (!strcmp (m->name, nameandvalue))
      break;
  if (m)
    free (m->value);
  else
    {
      m = xcalloc (1, sizeof *m + strlen (nameandvalue));
      strcpy (m->name, nameandvalue);
      m->next = variablelist;
      variablelist = m;
    }
  m->value = xstrdup (value);
}


/* Return true if the macro or variable NAME is set, i.e. not the
   empty string and not evaluating to 0.  */
static int
macro_set_p (const char *name)
{
  macro_t m;

  for (m = macrolist; m ; m = m->next)
    if (!strcmp (m->name, name))
      break;
  if (!m)
    for (m = variablelist; m ; m = m->next)
      if (!strcmp (m->name, name))
        break;
  if (!m || !m->value || !*m->value)
    return 0;
  if ((*m->value & 0x80) || !isdigit (*m->value))
    return 1; /* Not a digit but some other string.  */
  return !!atoi (m->value);
}


/* Evaluate the current conditions.  */
static void
evaluate_conditions (const char *fname, int lnr)
{
  int i;

  (void)fname;
  (void)lnr;

  /* for (i=0; i < condition_stack_idx; i++) */
  /*   inf ("%s:%d:   stack[%d] %s %s %c", */
  /*        fname, lnr, i, condition_stack[i]->isset? "set":"clr", */
  /*        condition_stack[i]->name, */
  /*        (macro_set_p (condition_stack[i]->name) */
  /*         ^ !condition_stack[i]->isset)? 't':'f'); */

  cond_is_active = 1;
  cond_in_verbatim = 0;
  if (condition_stack_idx)
    {
      for (i=0; i < condition_stack_idx; i++)
        {
          if (condition_stack[i]->manverb)
            cond_in_verbatim = (macro_set_p (condition_stack[i]->name)
                                ^ !condition_stack[i]->isset);
          else if (!(macro_set_p (condition_stack[i]->name)
                     ^ !condition_stack[i]->isset))
            {
              cond_is_active = 0;
              break;
            }
        }
    }

  /* inf ("%s:%d:   active=%d verbatim=%d", */
  /*      fname, lnr, cond_is_active, cond_in_verbatim); */
}


/* Push a condition with condition macro NAME onto the stack.  If
   ISSET is true, a @isset condition is pushed.  */
static void
push_condition (const char *name, int isset, const char *fname, int lnr)
{
  condition_t cond;
  int manverb = 0;

  if (condition_stack_idx >= MAX_CONDITION_NESTING)
    {
      err ("%s:%d: condition nested too deep", fname, lnr);
      return;
    }

  if (!strcmp (name, "manverb"))
    {
      if (!isset)
        {
          err ("%s:%d: using \"@ifclear manverb\" is not allowed", fname, lnr);
          return;
        }
      manverb = 1;
    }

  cond = xcalloc (1, sizeof *cond + strlen (name));
  cond->manverb = manverb;
  cond->isset = isset;
  strcpy (cond->name, name);

  condition_stack[condition_stack_idx++] = cond;
  evaluate_conditions (fname, lnr);
}


/* Remove the last condition from the stack.  ISSET is used for error
   reporting.  */
static void
pop_condition (int isset, const char *fname, int lnr)
{
  if (!condition_stack_idx)
    {
      err ("%s:%d: unbalanced \"@end %s\"",
           fname, lnr, isset?"isset":"isclear");
      return;
    }
  condition_stack_idx--;
  free (condition_stack[condition_stack_idx]);
  condition_stack[condition_stack_idx] = NULL;
  evaluate_conditions (fname, lnr);
}



/* Return a section buffer for the section NAME.  Allocate a new buffer
   if this is a new section.  Keep track of the sections in THEPAGE.
   This function may reallocate the section array in THEPAGE.  */
static section_buffer_t
get_section_buffer (const char *name)
{
  int i;
  section_buffer_t sect;

  /* If there is no section we put everything into the required NAME
     section.  Given that this is the first one listed it is likely
     that error are easily visible.  */
  if (!name)
    name = "NAME";

  for (i=0; i < thepage.n_sections; i++)
    {
      sect = thepage.sections + i;
      if (sect->name && !strcmp (name, sect->name))
        return sect;
    }
  for (i=0; i < thepage.n_sections; i++)
    if (!thepage.sections[i].name)
      break;
  if (thepage.n_sections && i < thepage.n_sections)
    sect = thepage.sections + i;
  else
    {
      /* We need to allocate or reallocate the section array.  */
      size_t old_n = thepage.n_sections;
      size_t new_n = 20;

      if (!old_n)
        thepage.sections = xcalloc (new_n, sizeof *thepage.sections);
      else
        {
          thepage.sections = xrealloc (thepage.sections,
                                       ((old_n + new_n)
                                        * sizeof *thepage.sections));
          memset (thepage.sections + old_n, 0,
                  new_n * sizeof *thepage.sections);
        }
      thepage.n_sections += new_n;

      /* Setup the tail pointers.  */
      for (i=old_n; i < thepage.n_sections; i++)
        {
          sect = thepage.sections + i;
          sect->lines_tail = &sect->lines;
        }
      sect = thepage.sections + old_n;
    }

  /* Store the name.  */
  assert (!sect->name);
  sect->name = xstrdup (name);
  sect->is_see_also = !strcmp (name, "SEE ALSO");
  return sect;
}


/* Return a malloced string alternating between bold and italics (or
 * other) font attributes.  */
static char *
roff_alternate (const char *line, const char *mode)
{
  const char *s;
  membuf_t mb;
  enum {
        x_init,
        x_roman,
        x_bold,
        x_italics
  } state, nextstate[2];
  int toggle;  /* values are 0 and 1 to index nextstate.  */

  init_membuf (&mb, 128);

  state = x_init;
  for (toggle = 0; toggle < 2; toggle++)
    {
      if (mode[toggle] == 'B')
        nextstate[toggle] = x_bold;
      else if (mode[toggle] == 'I')
        nextstate[toggle] = x_italics;
      else
        nextstate[toggle] = x_roman;
    }
  toggle = 0;
  for (s=line; *s; s++)
    {
      if (state == x_init)
        {
          if (!(*s == ' ' || *s == '\t'))
            {
              state = nextstate[toggle%2];
              toggle++;
              if (state == x_bold)
                put_membuf_str (&mb, "<strong>");
              else if (state == x_italics)
                put_membuf_str (&mb, "<em>");
              else
                put_membuf_str (&mb, "<span>");
            }
        }
      else
        {
          if (*s == ' ' || *s == '\t')
            {
              if (state == x_bold)
                put_membuf_str (&mb, "</strong>");
              else if (state == x_italics)
                put_membuf_str (&mb, "</em>");
              else
                put_membuf_str (&mb, "</span>");

              put_membuf (&mb, s, 1);
              state = x_init;
            }
        }

      put_membuf (&mb, s, 1);
    }
  if (state == x_bold)
    put_membuf_str (&mb, "</strong>");
  else if (state == x_italics)
    put_membuf_str (&mb, "</em>");
  else if (state == x_roman)
    put_membuf_str (&mb, "</span>");

  return xget_membuf (&mb);
}


/* Add the content of LINE to the section named SECTNAME.  */
static void
add_content (const char *sectname, char *line, int verbatim)
{
  section_buffer_t sect;
  line_buffer_t lb;
  char *linebuffer = NULL;
  char *src, *dst;
  const char *s;

  if (verbatim && htmlmode)
    {
      /* This is roff rendered - map this to HTML.  */
      if (!strncmp (line, ".B ", 3))
        linebuffer = xstrconcat ("<strong>", line+3, "</strong>", NULL);
      else if (!strncmp (line, ".I ", 3))
        linebuffer = xstrconcat ("<em>", line+3, "</em>", NULL);
      else if (!strncmp (line, ".BI ", 4))
        linebuffer = roff_alternate (line+4, "BI");
      else if (!strncmp (line, ".IB ", 4))
        linebuffer = roff_alternate (line+4, "IB");
      else if (!strncmp (line, ".BR ", 4))
        linebuffer = roff_alternate (line+4, "BR");
      else if (!strncmp (line, ".RB ", 4))
        linebuffer = roff_alternate (line+4, "RB");
      else if (!strncmp (line, ".RI ", 4))
        linebuffer = roff_alternate (line+4, "RI");
      else if (!strncmp (line, ".IR ", 4))
        linebuffer = roff_alternate (line+4, "IR");
      else if (!strncmp (line, ".br", 3))
        linebuffer = xstrdup ("<br/>");
      else if (!strncmp (line, "\\- ", 3))
        linebuffer = xstrconcat (" &mdash; ", line+3, NULL);
      else if (strchr (line, '\\'))  /* We need to remove them later.  */
        linebuffer = xstrdup ("<br/>\n");

      if (linebuffer)
        {
          /* Remove backslash escapes;  */
          for (src=dst=linebuffer; *src; src++)
            {
              if (*src == '\\' && src[1] == '\\')
                *dst++ = '\\';
              else if (*src == '\\')
                ;
              else
                *dst++ = *src;
            }
          *dst = 0;
          line = linebuffer;
        }
    }
  else if (htmlmode && !*line)
    line = linebuffer = xstrdup ("@para{}");
  else if (htmlmode)
    {
      size_t n0, n1;

      for (s=line, n0=n1=0; *s; s++, n0++)
        if (*s == '<' || *s == '>')
          n1 += 3;
        else if (*s == '&')
          n1 += 4;
      if (n1)
        {
          dst = linebuffer = xmalloc (n0 + n1 + 1);
          for (s=line; *s; s++)
            if (*s == '<')
              strcpy (dst, "&lt;"), dst += 4;
            else if (*s == '>')
              strcpy (dst, "&gt;"), dst += 4;
            else if (*s == '&')
              strcpy (dst, "&amp;"), dst += 5;
            else
              *dst++ = *s;
          *dst = 0;
          line = linebuffer;
        }
    }

  sect = get_section_buffer (sectname);
  if (sect->last_line && !sect->last_line->verbatim == !verbatim)
    {
      /* Lets append that line to the last one.  We do this to keep
         all lines of the same kind (i.e.verbatim or not) together in
         one large buffer.  */
      size_t n1, n;

      lb = sect->last_line;
      n1 = strlen (lb->line);
      n = n1 + 1 + strlen (line) + 1;
      lb->line = xrealloc (lb->line, n);
      strcpy (lb->line+n1, "\n");
      strcpy (lb->line+n1+1, line);
    }
  else
    {
      lb = xcalloc (1, sizeof *lb);
      lb->verbatim = verbatim;
      if (htmlmode && sect->lines_tail == &sect->lines)
        lb->line = xstrconcat ("@para{}", line, NULL);
      else
        lb->line = xstrdup (line);
      sect->last_line = lb;
      *sect->lines_tail = lb;
      sect->lines_tail = &lb->next;
    }

  free (linebuffer);
}


/* Prepare for a new man page using the filename NAME. */
static void
start_page (char *name)
{
  if (verbose)
    inf ("starting page '%s'", name);
  assert (!thepage.name);
  thepage.name = xstrdup (name);
  thepage.n_sections = 0;
}


/* Write a character to FP.  */
static void
writechr (int c, FILE *fp)
{
  putc (c, fp);
}


/* Write depending on HTMLMODE either ROFF or HTML to FP.  */
static void
writestr (const char *roff, const char *html, FILE *fp)
{
  const char *s = htmlmode? html : roff;

  if (s)
    fputs (s, fp);
}


/* Write the .TH entry of the current page.  Return -1 if there is a
   problem with the page. */
static int
write_th (FILE *fp)
{
  char *name, *p;

  writestr (".\\\" Created from Texinfo source by yat2m " VERSION "\n",
            "<!-- Created from Texinfo source by yat2m " VERSION " -->\n",
            fp);

  name = ascii_strupr (xstrdup (thepage.name));
  p = strrchr (name, '.');
  if (!p || !p[1])
    {
      err ("no section name in man page '%s'", thepage.name);
      free (name);
      return -1;
    }
  *p++ = 0;

  if (htmlmode)
    {
      if (gnupgorgmode)
        {
          fputs
            ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
             "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n"
             "         \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
             "<html xmlns=\"http://www.w3.org/1999/xhtml\""
             " lang=\"en\" xml:lang=\"en\">\n", fp);
        }
      else
        fputs ("<html>\n", fp);
      fputs ("<head>\n", fp);
      fprintf (fp, " <title>%s(%s)</title>\n", name, p);
      if (gnupgorgmode)
        {
          fputs ("<meta http-equiv=\"Content-Type\""
                 " content=\"text/html;charset=utf-8\" />\n", fp);
          fputs ("<meta name=\"viewport\""
                 " content=\"width=device-width, initial-scale=1\" />\n"
                 "<link rel=\"stylesheet\" href=\"/share/site.css\""
                 " type=\"text/css\" />\n", fp);
        }
      else
        fputs (default_css, fp);
      fputs ("</head>\n"
             "<body>\n", fp);
      if (gnupgorgmode)
        fputs ("<div id=\"wrapper\">\n"
               "<div id=\"content\">\n", fp);
      fputs ("<div class=\"y2m\">\n", fp);
    }

  /* This roff source
   *   .TH GPG 1 2016-12-20 "GnuPG 2.1.17" "GNU Privacy Guard 2.1"
   * is rendered by man like this:
   *   GPG(1)         GNU Privacy Guard 2.1      GPG(1)
   *   [...]
   *   GnuPG 2.1.17        2016-12-20            GPG(1)
   */
  if (htmlmode)
    {
      fprintf (fp, "<p class=\"y2m y2m-top\">"
               "<span class=\"y2m-left\">%s(%s)</span> "
               "<span class=\"y2m-center\">%s</span> "
               "<span class=\"y2m-right\">%s(%s)</span>"
               "</p>\n",
               name, p, opt_source, name, p);
      /* Note that the HTML footer is written by write_bottom().  */

    }
  else
    fprintf (fp, ".TH %s %s %s \"%s\" \"%s\"\n",
             name, p, isodatestring (), opt_release, opt_source);

  free (name);
  return 0;
}


/* In HTML mode we need to render a footer.  */
static int
write_bottom (FILE *fp)
{
  char *name, *p;

  if (!htmlmode)
    return 0;

  name = ascii_strupr (xstrdup (thepage.name));
  p = strrchr (name, '.');
  if (!p || !p[1])
    {
      err ("no section name in man page '%s'", thepage.name);
      free (name);
      return -1;
    }
  *p++ = 0;

  /* This roff source
   *   .TH GPG 1 2016-12-20 "GnuPG 2.1.17" "GNU Privacy Guard 2.1"
   * is rendered by man to this footer:
   *   GnuPG 2.1.17        2016-12-20            GPG(1)
   */
  fprintf (fp, "<p class=\"y2m y2m-footer\">"
           "<span class=\"y2m-left\">%s</span> "
           "<span class=\"y2m-center\">%s</span> "
           "<span class=\"y2m-right\">%s(%s)</span>"
           "</p>\n",
           opt_release, isodatestring (), name, p);
  fputs ("</div><!-- class y2m -->\n", fp);
  if (gnupgorgmode)
    fputs ("</div><!-- end content -->\n"
           "</div><!-- end wrapper -->\n", fp);
  fputs ("</body>\n"
         "</html>\n", fp);

  free (name);
  return 0;
}


/* Write the .SH header.  With NULL passed for NAME just close a
 * section in html mode if there is an open section. */
static void
write_sh (FILE *fp, const char *name)
{
  static int in_section;

  if (htmlmode && in_section)
    fprintf (fp, "</div>\n");
  in_section = 0;

  if (name)
    {
      if (htmlmode)
        fprintf (fp,
                 "<div class=\"y2m-section\">\n"
                 "<h2 class=\"y2m-sh\">%s</h2>\n", name);
      else
        fprintf (fp, ".SH %s\n", name);
      in_section = 1;
    }
}

/* Render a @item line to HTML.  (LINE,LEN) gives the arguments of
 * @item.  Use NULL for LINE to close a possible open <li>.  ITEMX
 * flags a @itemx line.  */
static void
write_html_item (FILE *fp, const char *line, size_t len, int itemx)
{
  static int in_li;
  const char *rest;
  size_t n, n0;
  int eol_action = 0;
  int table_level = 0;

  if (!itemx && in_li)
    {
      fprintf (fp, "</li>\n");
      in_li = 0;
    }

  if (line)
    {
      /* Trim the LF and skip leading spaces. */
      if (len && line[len-1] == '\n')
        len--;
      for (; len && (*line == ' ' || *line == '\t'); len--, line++)
        ;
      if (len)
        {
          rest = line;
          for (n=0; n < len && !(*rest == ' ' || *rest == '\t'); n++, rest++)
            ;
          n0 = n;
          for (; n < len && (*rest == ' ' || *rest == '\t'); n++, rest++)
            ;
          len -= n;
          /* Now the first word is (LINE,N0) and the args are (REST,LEN) */
          fprintf (fp, "%s<span class=\"y2m-item\">%.*s",
                   itemx? "    ":"<li>", (int)n0, line);
          if (len)
            {
              fputs (" <span class=\"y2m-args\">", fp);
              proc_texi_buffer (fp, rest, len, &table_level, &eol_action,
                                NULL, 0);
              fputs ("</span>", fp);
            }
          fputs ("</span>\n", fp);
          in_li = 1;
        }
    }
}


/* Process the texinfo command COMMAND (without the leading @) and
   write output if needed to FP. REST is the remainder of the line
   which should either point to an opening brace or to a white space.
   The function returns the number of characters already processed
   from REST.  LEN is the usable length of REST.  TABLE_LEVEL is used
   to control the indentation of tables.  SECT has info about the
   current section or is NULL.  */
static size_t
proc_texi_cmd (FILE *fp, const char *command, const char *rest, size_t len,
               int *table_level, int *eol_action, section_buffer_t sect)
{
  static struct {
    const char *name;    /* Name of the command.  */
    int what;            /* What to do with this command.  */
    int enable_2D_minus; /* Interpret '-' as minus.  */
    const char *lead_in; /* String to print with a opening brace.  */
    const char *lead_out;/* String to print with the closing brace.  */
    const char *html_in; /* Same as LEAD_IN but for HTML.  */
    const char *html_out;/* Same as LEAD_OUT but for HTML.  */
  } cmdtbl[] = {
    { "command", 9, 1, "\\fB", "\\fP", "<i>", "</i>" },
    { "code",    0, 1, "\\fB", "\\fP", "<samp>", "</samp>" },
    { "url",     0, 1, "\\fB", "\\fP", "<strong>", "</strong>" },
    { "sc",      0, 0, "\\fB", "\\fP", "<span class=\"y2m-sc\">", "</span>" },
    { "var",     0, 0, "\\fI", "\\fP", "<u>", "</u>" },
    { "samp",    0, 1, "\\(oq", "\\(cq"  },
    { "kbd",     0, 1, "\\(oq", "\\(cq"  },
    { "file",    0, 1, "\\(oq\\fI","\\fP\\(cq" },
    { "env",     0, 1, "\\(oq\\fI","\\fP\\(cq" },
    { "acronym", 0, 0 },
    { "dfn",     0, 0 },
    { "option",  0, 1, "\\fB", "\\fP", "<samp>", "</samp>" },
    { "example", 10, 1, ".RS 2\n.nf\n",      NULL, "\n<pre>\n", "\n</pre>\n" },
    { "smallexample", 10, 1, ".RS 2\n.nf\n", NULL, "\n<pre>\n", "\n</pre>\n" },
    { "asis",    7 },
    { "anchor",  7 },
    { "cartouche", 1 },
    { "ref",     0, 0, "[", "]" },
    { "xref",    0, 0, "See: [", "]" },
    { "pxref",   0, 0, "see: [", "]" },
    { "uref",    0, 0, "(\\fB", "\\fP)" },
    { "footnote",0, 0, " ([", "])" },
    { "emph",    0, 0, "\\fI", "\\fP", "<em>", "</em>" },
    { "w",       1 },
    { "c",       5 },
    { "efindex", 1 },
    { "opindex", 1 },
    { "cpindex", 1 },
    { "cindex",  1 },
    { "noindent", 0 },
    { "para",   11, },
    { "section", 1 },
    { "chapter", 1 },
    { "subsection", 6, 0, "\n.SS ", NULL, "<h3>" },
    { "chapheading", 0},
    { "item",    2, 0, ".TP\n.B " },
    { "itemx",   2, 0, ".TQ\n.B " },
    { "table",   3 },
    { "itemize", 3 },
    { "bullet",  0, 0, "* " },
    { "*",       0, 0, "\n.br"},
    { "/",       0 },
    { "end",     4 },
    { "quotation", 1, 0, ".RS\n\\fB" },
    { "value", 8 },
    { "dots", 0, 0, "...", NULL, "&hellip;" },
    { "minus", 0, 0, "\\-", NULL, "&minus;" },
    /* From here, macro for table */
    { "gcctabopt",       0, 1 },
    { "gnupgtabopt",     0, 1 },
    { NULL }
  };
  size_t n;
  int i;
  const char *s;
  const char *lead_out = NULL;
  const char *html_out = NULL;
  int ignore_args = 0;
  int see_also_command = 0;
  int enable_2D_minus = 0;

  for (i=0; cmdtbl[i].name && strcmp (cmdtbl[i].name, command); i++)
    ;
  if (cmdtbl[i].name)
    {
      writestr (cmdtbl[i].lead_in, cmdtbl[i].html_in, fp);
      lead_out = cmdtbl[i].lead_out;
      html_out = cmdtbl[i].html_out;
      enable_2D_minus = cmdtbl[i].enable_2D_minus;
      switch (cmdtbl[i].what)
        {
        case 10:
          sect->in_pre = 1;
          cond_parse_dash = 0;
          cond_2D_as_minus = 1;
          /* Fallthrough */
        case 1: /* Throw away the entire line.  */
          s = memchr (rest, '\n', len);
          return s? (s-rest)+1 : len;
        case 2: /* Handle @item.  */
          cond_parse_dash = -1;
          if (htmlmode)
            {
              s = memchr (rest, '\n', len);
              n = s? (s-rest)+1 : len;
              write_html_item (fp, rest, n, !strcmp(cmdtbl[i].name, "itemx"));
              return n;
            }
          else
            {
              s = memchr (rest, '\n', len);
              if (s)
                {
                  n = (s-rest)+1;
                  proc_texi_buffer (fp, rest, n, table_level, eol_action,
                                    sect, table_item_stack[*table_level]);
                  return n;
                }
            }
          break;
        case 3: /* Handle table.  */
          ++*table_level;
          if (*table_level >= MAX_TABLE_NESTING)
            die ("too many nesting level of table\n");
          if (*table_level > (htmlmode? 0 : 1))
            {
              if (htmlmode)
                write_html_item (fp, NULL, 0, 0);
              writestr (".RS\n", "<ul>\n", fp);
            }

          s = memchr (rest, '\n', len);
          if (s)
            {
              size_t n0 = n = (s-rest)+1;

              for (s=rest; n && (*s == ' ' || *s == '\t'); s++, n--)
                ;
              if (n && *s == '@')
                {
                  char argbuf[256];
                  int argidx;

                  s++;
                  n--;
                  for (argidx = 0; argidx < n; argidx++)
                    if (s[argidx] == ' ' || s[argidx] == '\t'
                        || s[argidx] == '\n')
                      break;
                    else
                      argbuf[argidx] = s[argidx];
                  argbuf[argidx] = 0;
                  for (i=0; cmdtbl[i].name; i++)
                    if (!strcmp (cmdtbl[i].name, argbuf))
                      break;
                  if (cmdtbl[i].name && cmdtbl[i].enable_2D_minus)
                    table_item_stack[*table_level] = 1;
                  else
                    table_item_stack[*table_level] = 0;
                  return n0;
                }
              else
                return len;
            }
          else
            /* Now throw away the entire line. */
            return len;
          break;
        case 4: /* Handle end.  */
          for (s=rest, n=len; n && (*s == ' ' || *s == '\t'); s++, n--)
            ;
          if (n >= 5 && !memcmp (s, "table", 5)
              && (!n || s[5] == ' ' || s[5] == '\t' || s[5] == '\n'))
            {
              if (htmlmode)
                write_html_item (fp, NULL, 0, 0);
              if ((*table_level)-- > 1)
                writestr (".RE\n", "</ul>\n", fp);
              else
                writestr (".P\n", "</ul>\n", fp);
            }
          else if (n >= 7 && !memcmp (s, "example", 7)
              && (!n || s[7] == ' ' || s[7] == '\t' || s[7] == '\n'))
            {
              cond_parse_dash = 1;
              cond_2D_as_minus = 0;
              writestr (".fi\n.RE\n", "</pre>\n", fp);
              sect->in_pre = 0;
            }
          else if (n >= 12 && !memcmp (s, "smallexample", 12)
              && (!n || s[12] == ' ' || s[12] == '\t' || s[12] == '\n'))
            {
              cond_parse_dash = 1;
              cond_2D_as_minus = 0;
              writestr (".fi\n.RE\n", "</pre>\n", fp);
              sect->in_pre = 0;
            }
          else if (n >= 9 && !memcmp (s, "quotation", 9)
              && (!n || s[9] == ' ' || s[9] == '\t' || s[9] == '\n'))
            {
              writestr ("\\fR\n.RE\n", "xx", fp);
            }
          /* Now throw away the entire line. */
          s = memchr (rest, '\n', len);
          return s? (s-rest)+1 : len;
        case 5: /* Handle special comments. */
          for (s=rest, n=len; n && (*s == ' ' || *s == '\t'); s++, n--)
            ;
          if (n >= 4 && !memcmp (s, "man:", 4))
            {
              s += 4;
              n -= 4;
              if (htmlmode)
                {
                  if (!strncmp (s, ".RE\n", 4)
                      || !strncmp (s, ".RS\n", 4))
                    ;
                  else
                    inf ("unknown special comment \"man:\"");
                }
              else
                {
                  for (; n && *s != '\n'; n--, s++)
                    writechr (*s, fp);
                  writechr ('\n', fp);
                }
            }
          /* Now throw away the entire line. */
          s = memchr (rest, '\n', len);
          return s? (s-rest)+1 : len;
        case 6:
          *eol_action = EOL_CLOSE_SUBSECTION;
          break;
        case 7:
          ignore_args = 1;
          break;
        case 8:
          ignore_args = 1;
          if (*rest != '{')
            {
              err ("opening brace for command '%s' missing", command);
              return len;
            }
          else
            {
              /* Find closing brace.  */
              for (s=rest+1, n=1; *s && n < len; s++, n++)
                if (*s == '}')
                  break;
              if (*s != '}')
                {
                  err ("closing brace for command '%s' not found", command);
                  return len;
                }
              else
                {
                  size_t rlen = s - (rest + 1);
                  macro_t m;

                  for (m = variablelist; m; m = m->next)
                    {
                      if (strlen (m->name) == rlen
                          && !strncmp (m->name, rest+1, rlen))
                        break;
                    }
                  if (m)
                    writestr (m->value, m->value, fp);
                  else
                    inf ("texinfo variable '%.*s' is not set",
                         (int)rlen, rest+1);
                }
            }
          break;
        case 9:  /* @command{} */
          if (sect && sect->is_see_also)
            see_also_command = 1;
          break;
        case 11: /* @para{} inserted by htmlmode */
          if (!*table_level && !sect->in_pre)
            {
              if (sect->in_para)
                writestr (NULL, "</p>\n", fp);
              writestr (NULL, "\n<p>", fp);
              sect->in_para = 1;
            }
          break;

        default:
          break;
        }
    }
  else /* macro */
    {
      macro_t m;

      for (m = macrolist; m ; m = m->next)
        if (!strcmp (m->name, command))
            break;
      if (m)
        {
          proc_texi_buffer (fp, m->value, strlen (m->value),
                            table_level, eol_action, NULL, 0);
          ignore_args = 1; /* Parameterized macros are not yet supported. */
        }
      else
        inf ("texinfo command '%s' not supported (%.*s)", command,
             (int)((s = memchr (rest, '\n', len)), (s? (s-rest) : len)), rest);
    }

  if (*rest == '{')
    {
      /* Find matching closing brace.  */
      for (s=rest+1, n=1, i=1; i && *s && n < len; s++, n++)
        if (*s == '{')
          i++;
        else if (*s == '}')
          i--;
      if (i)
        {
          err ("closing brace for command '%s' not found", command);
          return len;
        }


      if (n > 2 && len && !ignore_args)
        {
          char *workstr;  /* Let's work on a copy.  */
          char *p;
          macro_t m;
          const char *cmdname;

          n -= 2;

          workstr = xmalloc (len);
          memcpy (workstr, rest+1, len-1);
          workstr[len-1] = 0;
          if (see_also_command && htmlmode
              && workstr[n] == '}' && workstr[n+1] == '('
              && digitp (workstr+n+2)
              && (p = strchr (workstr + n + 2, ')')))
            {
              /* Seems to be a command with section number.  P points
               * to the closing parentheses.  Render as link.  */

              workstr[n] = 0;
              *p = 0;
              cmdname = workstr;
              if (*workstr == '@' && workstr[1])
                {
                  for (m = macrolist; m ; m = m->next)
                    if (!strcmp (m->name, workstr+1))
                      break;
                  if (m)
                    cmdname = m->value;
                }


              fprintf (fp, "<a href=\"%s.%s.html\">%s</a>(%s)",
                       cmdname, workstr + n + 2,
                       cmdname, workstr + n + 2);

              n = p + 2 - workstr;
            }
          else
            {
              proc_texi_buffer (fp, workstr, n, table_level, eol_action,
                                NULL, enable_2D_minus);
              n += 2;
            }
          free (workstr);
        }
    }
  else
    n = 0;

  writestr (lead_out, html_out, fp);

  return n;
}



/* Process the string LINE with LEN bytes of Texinfo content.  SECT
 * has infos about the current secion or is NULL.  */
static void
proc_texi_buffer (FILE *fp, const char *line, size_t len,
                  int *table_level, int *eol_action, section_buffer_t sect,
                  int char_2D_is_minus)
{
  const char *s;
  char cmdbuf[256];
  int cmdidx = 0;
  int in_cmd = 0;
  size_t n;

  for (s=line; *s && len; s++, len--)
    {
      if (in_cmd)
        {
          if (in_cmd == 1)
            {
              switch (*s)
                {
                case '@': case '{': case '}':
                  writechr (*s, fp); in_cmd = 0;
                  break;
                case ':': /* Not ending a sentence flag.  */
                  in_cmd = 0;
                  break;
                case '.': case '!': case '?': /* Ending a sentence. */
                  writechr (*s, fp); in_cmd = 0;
                  break;
                case ' ': case '\t': case '\n': /* Non collapsing spaces.  */
                  writechr (*s, fp); in_cmd = 0;
                  break;
                default:
                  cmdidx = 0;
                  cmdbuf[cmdidx++] = *s;
                  in_cmd++;
                  break;
                }
            }
          else if (*s == '{' || *s == ' ' || *s == '\t' || *s == '\n')
            {
              cmdbuf[cmdidx] = 0;
              n = proc_texi_cmd (fp, cmdbuf, s, len, table_level, eol_action,
                                 sect);
              assert (n <= len);
              s += n; len -= n;
              s--; len++;
              in_cmd = 0;
            }
          else if (cmdidx < sizeof cmdbuf -1)
            cmdbuf[cmdidx++] = *s;
          else
            {
              err ("texinfo command too long - ignored");
              in_cmd = 0;
            }
        }
      else if (*s == '@')
        in_cmd = 1;
      else if (*s == '\n')
        {
          switch (*eol_action)
            {
            case EOL_CLOSE_SUBSECTION:
              writestr ("\n\\ \n", "</h3>\n", fp);
              break;
            default:
              writechr (*s, fp);
              break;
            }
          *eol_action = 0;
          if (cond_parse_dash == -1)
            cond_parse_dash = 0;
        }
      else if (*s == '\\')
        writestr ("\\[rs]", "&bsol;", fp);
      else if (cond_parse_dash == 1 && sect && *s == '-')
        /* Handle -- and --- when it's _not_ in an argument.  */
        {
          if (len < 2 || s[1] != '-')
            writechr (*s, fp);
          else if (len < 3 || s[2] != '-')
            {
              writestr ("\\[en]", "&ndash;", fp);
              len--;
              s++;
            }
          else
            {
              writestr ("\\[em]", "&mdash;", fp);
              len -= 2;
              s += 2;
            }
        }
      else if (*s == '-' && (cond_2D_as_minus || char_2D_is_minus))
        {
          writestr ("\\-", "-", fp);
        }
      else
        writechr (*s, fp);
    }

  if (in_cmd > 1)
    {
      cmdbuf[cmdidx] = 0;
      n = proc_texi_cmd (fp, cmdbuf, s, len, table_level, eol_action, sect);
      assert (n <= len);
      s += n; len -= n;
      s--; len++;
      /* in_cmd = 0; -- doc only */
    }
}


/* Do something with the Texinfo line LINE.  If SECT is not NULL is
 * has information about the current section.  */
static void
parse_texi_line (FILE *fp, const char *line, int *table_level,
                 section_buffer_t sect)
{
  int eol_action = 0;

  /* A quick test whether there are any texinfo commands.  */
  if (!strchr (line, '@'))
    {
      /* FIXME: In html mode escape HTML stuff. */
      if (htmlmode && *line)
        fputs ("<p>", fp);
      writestr (line, line, fp);
      if (htmlmode && *line)
        fputs ("</p>", fp);
      writechr ('\n', fp);
      return;
    }
  proc_texi_buffer (fp, line, strlen (line), table_level, &eol_action,
                    sect, 0);
  writechr ('\n', fp);
}


/* Write all the lines LINES to FP.  */
static void
write_content (FILE *fp, section_buffer_t sect)
{
  line_buffer_t line;
  int table_level = 0;
  line_buffer_t lines = sect->lines;

  /* inf ("===BEGIN==============================================="); */
  /* for (line = lines; line; line = line->next) */
  /*   inf ("line='%s'", line->line); */
  /* inf ("===END==============================================="); */


  for (line = lines; line; line = line->next)
    {
      if (line->verbatim)
        {
          /* FIXME: IN HTML mode we need to employ a parser for roff
           * markup.  */
          writestr (line->line, line->line, fp);
          writechr ('\n', fp);
        }
      else
        {
          /* fputs ("TEXI---", fp); */
          /* fputs (line->line, fp); */
          /* fputs ("---\n", fp); */
          parse_texi_line (fp, line->line, &table_level, sect);
        }
    }
}



static int
is_standard_section (const char *name)
{
  int i;
  const char *s;

  for (i=0; (s=standard_sections[i]); i++)
    if (!strcmp (s, name))
      return 1;
  return 0;
}


/* Finish a page; that is sort the data and write it out to the file.  */
static void
finish_page (void)
{
  FILE *fp;
  section_buffer_t sect = NULL;
  int idx;
  const char *s;
  int i;

  if (!thepage.name)
    return; /* No page active.  */

  if (verbose)
    inf ("finishing page '%s'", thepage.name);

  if (opt_select)
    {
      if (!strcmp (opt_select, thepage.name))
        {
          inf ("selected '%s'", thepage.name );
          fp = stdout;
        }
      else
        {
          fp = fopen ( "/dev/null", "w" );
          if (!fp)
            die ("failed to open /dev/null: %s\n", strerror (errno));
        }
    }
  else if (opt_store)
    {
      char *fname = xstrconcat (thepage.name, htmlmode? ".html":NULL, NULL);
      if (verbose)
        inf ("writing '%s'", fname);
      fp = fopen (fname, "w" );
      if (!fp)
        die ("failed to create '%s': %s\n", fname, strerror (errno));
      free (fname);
    }
  else
    fp = stdout;

  if (write_th (fp))
    goto leave;

  for (idx=0; (s=standard_sections[idx]); idx++)
    {
      for (i=0; i < thepage.n_sections; i++)
        {
          sect = thepage.sections + i;
          if (sect->name && !strcmp (s, sect->name))
            break;
        }
      if (i == thepage.n_sections)
        sect = NULL;

      if (sect)
        {
          write_sh (fp, sect->name);
          write_content (fp, sect);
          /* Now continue with all non standard sections directly
             following this one. */
          for (i++; i < thepage.n_sections; i++)
            {
              sect = thepage.sections + i;
              if (sect->name && is_standard_section (sect->name))
                break;
              if (sect->name)
                {
                  write_sh (fp, sect->name);
                  write_content (fp, sect);
                }
            }

        }
    }

  write_sh (fp, NULL);
  if (write_bottom (fp))
    goto leave;

 leave:
  if (fp != stdout)
    fclose (fp);
  free (thepage.name);
  thepage.name = NULL;
  /* FIXME: Cleanup the content.  */
}




/* Parse one Texinfo file and create manpages according to the
   embedded instructions.  */
static void
parse_file (const char *fname, FILE *fp, char **section_name, int in_pause)
{
  char *line;
  int lnr = 0;
  /* Fixme: The following state variables don't carry over to include
     files. */
  int skip_to_end = 0;        /* Used to skip over menu entries. */
  int skip_sect_line = 0;     /* Skip after @mansect.  */
  int item_indent = 0;        /* How far is the current @item indented.  */

  /* Helper to define a macro. */
  char *macroname = NULL;
  char *macrovalue = NULL;
  size_t macrovaluesize = 0;
  size_t macrovalueused = 0;

  line = xmalloc (LINESIZE);
  while (fgets (line, LINESIZE, fp))
    {
      size_t n = strlen (line);
      int got_line = 0;
      char *p, *pend;

      lnr++;
      if (!n || line[n-1] != '\n')
        {
          err ("%s:%d: trailing linefeed missing, line too long or "
               "embedded Nul character", fname, lnr);
          break;
        }
      line[--n] = 0;

      /* Kludge to allow indentation of tables.  */
      for (p=line; *p == ' ' || *p == '\t'; p++)
        ;
      if (*p)
        {
          if (*p == '@' && !strncmp (p+1, "item", 4))
            item_indent = p - line;  /* Set a new indent level.  */
          else if (p - line < item_indent)
            item_indent = 0;         /* Switch off indention.  */

          if (item_indent)
            {
              memmove (line, line+item_indent, n - item_indent + 1);
              n -= item_indent;
            }
        }


      if (*line == '@')
        {
          for (p=line+1, n=1; *p && *p != ' ' && *p != '\t'; p++)
            n++;
          while (*p == ' ' || *p == '\t')
            p++;
        }
      else
        p = line;

      /* Take action on macro.  */
      if (macroname)
        {
          if (n == 4 && !memcmp (line, "@end", 4)
              && (line[4]==' '||line[4]=='\t'||!line[4])
              && !strncmp (p, "macro", 5)
              && (p[5]==' '||p[5]=='\t'||!p[5]))
            {
              if (macrovalueused)
                macrovalue[--macrovalueused] = 0; /* Kill the last LF. */
              macrovalue[macrovalueused] = 0;     /* Terminate macro. */
              macrovalue = xrealloc (macrovalue, macrovalueused+1);

              set_macro (macroname, macrovalue);
              macrovalue = NULL;
              free (macroname);
              macroname = NULL;
            }
          else
            {
              if (macrovalueused + strlen (line) + 2 >= macrovaluesize)
                {
                  macrovaluesize += strlen (line) + 256;
                  macrovalue = xrealloc (macrovalue,  macrovaluesize);
                }
              strcpy (macrovalue+macrovalueused, line);
              macrovalueused += strlen (line);
              macrovalue[macrovalueused++] = '\n';
            }
          continue;
        }


      if (n >= 5 && !memcmp (line, "@node", 5)
          && (line[5]==' '||line[5]=='\t'||!line[5]))
        {
          /* Completey ignore @node lines.  */
          continue;
        }


      if (skip_sect_line)
        {
          skip_sect_line = 0;
          if (!strncmp (line, "@section", 8)
              || !strncmp (line, "@subsection", 11)
              || !strncmp (line, "@chapheading", 12))
            continue;
        }

      /* We only parse lines we need and ignore the rest.  There are a
         few macros used to control this as well as one @ifset
         command.  Parts we know about are saved away into containers
         separate for each section. */

      /* First process ifset/ifclear commands. */
      if (*line == '@')
        {
          if (n == 6 && !memcmp (line, "@ifset", 6)
                   && (line[6]==' '||line[6]=='\t'))
            {
              for (p=line+7; *p == ' ' || *p == '\t'; p++)
                ;
              if (!*p)
                {
                  err ("%s:%d: name missing after \"@ifset\"", fname, lnr);
                  continue;
                }
              for (pend=p; *pend && *pend != ' ' && *pend != '\t'; pend++)
                ;
              *pend = 0;  /* Ignore rest of the line.  */
              push_condition (p, 1, fname, lnr);
              continue;
            }
          else if (n == 8 && !memcmp (line, "@ifclear", 8)
                   && (line[8]==' '||line[8]=='\t'))
            {
              for (p=line+9; *p == ' ' || *p == '\t'; p++)
                ;
              if (!*p)
                {
                  err ("%s:%d: name missing after \"@ifsclear\"", fname, lnr);
                  continue;
                }
              for (pend=p; *pend && *pend != ' ' && *pend != '\t'; pend++)
                ;
              *pend = 0;  /* Ignore rest of the line.  */
              push_condition (p, 0, fname, lnr);
              continue;
            }
          else if (n == 4 && !memcmp (line, "@end", 4)
                   && (line[4]==' '||line[4]=='\t')
                   && !strncmp (p, "ifset", 5)
                   && (p[5]==' '||p[5]=='\t'||!p[5]))
            {
              pop_condition (1, fname, lnr);
              continue;
            }
          else if (n == 4 && !memcmp (line, "@end", 4)
                   && (line[4]==' '||line[4]=='\t')
                   && !strncmp (p, "ifclear", 7)
                   && (p[7]==' '||p[7]=='\t'||!p[7]))
            {
              pop_condition (0, fname, lnr);
              continue;
            }
        }

      /* Take action on ifset/ifclear.  */
      if (!cond_is_active)
        continue;

      /* Process commands. */
      if (*line == '@')
        {
          if (skip_to_end
              && n == 4 && !memcmp (line, "@end", 4)
              && (line[4]==' '||line[4]=='\t'||!line[4]))
            {
              skip_to_end = 0;
            }
          else if (cond_in_verbatim)
            {
                got_line = 1;
            }
          else if (n == 6 && !memcmp (line, "@macro", 6))
            {
              macroname = xstrdup (p);
              macrovalue = xmalloc ((macrovaluesize = 1024));
              macrovalueused = 0;
            }
          else if (n == 4 && !memcmp (line, "@set", 4))
            {
              set_variable (p);
            }
          else if (n == 8 && !memcmp (line, "@manpage", 8))
            {
              free (*section_name);
              *section_name = NULL;
              finish_page ();
              start_page (p);
              in_pause = 0;
            }
          else if (n == 8 && !memcmp (line, "@mansect", 8))
            {
              if (!thepage.name)
                err ("%s:%d: section outside of a man page", fname, lnr);
              else
                {
                  free (*section_name);
                  *section_name = ascii_strupr (xstrdup (p));
                  in_pause = 0;
                  skip_sect_line = 1;
                }
            }
          else if (n == 9 && !memcmp (line, "@manpause", 9))
            {
              if (!*section_name)
                err ("%s:%d: pausing outside of a man section", fname, lnr);
              else if (in_pause)
                err ("%s:%d: already pausing", fname, lnr);
              else
                in_pause = 1;
            }
          else if (n == 8 && !memcmp (line, "@mancont", 8))
            {
              if (!*section_name)
                err ("%s:%d: continue outside of a man section", fname, lnr);
              else if (!in_pause)
                err ("%s:%d: continue while not pausing", fname, lnr);
              else
                in_pause = 0;
            }
          else if (n == 5 && !memcmp (line, "@menu", 5)
                   && (line[5]==' '||line[5]=='\t'||!line[5]))
            {
              skip_to_end = 1;
            }
          else if (n == 8 && !memcmp (line, "@include", 8)
                   && (line[8]==' '||line[8]=='\t'||!line[8]))
            {
              char *incname = xstrdup (p);
              FILE *incfp = fopen (incname, "r");

              if (!incfp && opt_include && *opt_include && *p != '/')
                {
                  free (incname);
                  incname = xmalloc (strlen (opt_include) + 1
                                     + strlen (p) + 1);
                  strcpy (incname, opt_include);
                  if ( incname[strlen (incname)-1] != '/' )
                    strcat (incname, "/");
                  strcat (incname, p);
                  incfp = fopen (incname, "r");
                }

              if (!incfp)
                err ("can't open include file '%s': %s",
                     incname, strerror (errno));
              else
                {
                  parse_file (incname, incfp, section_name, in_pause);
                  fclose (incfp);
                }
              free (incname);
            }
          else if (n == 4 && !memcmp (line, "@bye", 4)
                   && (line[4]==' '||line[4]=='\t'||!line[4]))
            {
              break;
            }
          else if (!skip_to_end)
            got_line = 1;
        }
      else if (!skip_to_end)
        got_line = 1;

      if (got_line && cond_in_verbatim)
        add_content (*section_name, line, 1);
      else if (got_line && thepage.name && *section_name && !in_pause)
        add_content (*section_name, line, 0);

    }
  if (ferror (fp))
    err ("%s:%d: read error: %s", fname, lnr, strerror (errno));
  free (macroname);
  free (macrovalue);
  free (line);
}


static void
top_parse_file (const char *fname, FILE *fp)
{
  char *section_name = NULL;  /* Name of the current section or NULL
                                 if not in a section.  */
  macro_t m;

  while (macrolist)
    {
      macro_t next = macrolist->next;
      free (macrolist->value);
      free (macrolist);
      macrolist = next;
    }
  while (variablelist)
    {
      macro_t next = variablelist->next;
      free (variablelist->value);
      free (variablelist);
      variablelist = next;
    }
  for (m=predefinedmacrolist; m; m = m->next)
    set_macro (m->name, xstrdup ("1"));
  cond_is_active = 1;
  cond_in_verbatim = 0;
  cond_parse_dash = 1;

  parse_file (fname, fp, &section_name, 0);
  free (section_name);
  finish_page ();
}


int
main (int argc, char **argv)
{
  int last_argc = -1;
  const char *s;

  opt_source = "GNU";
  opt_release = "";

  /* Define default macros.  The trick is that these macros are not
     defined when using the actual texinfo renderer. */
  add_predefined_macro ("isman");
  add_predefined_macro ("manverb");

  /* Option parsing.  */
  if (argc)
    {
      argc--; argv++;
    }
  while (argc && last_argc != argc )
    {
      last_argc = argc;
      if (!strcmp (*argv, "--"))
        {
          argc--; argv++;
          break;
        }
      else if (!strcmp (*argv, "--help"))
        {
          puts (
                "Usage: " PGM " [OPTION] [FILE]\n"
                "Extract man pages from a Texinfo source.\n\n"
                "  --html           render output as HTML\n"
                "  --source NAME    use NAME as source field\n"
                "  --release STRING use STRING as the release field\n"
                "  --date EPOCH     use EPOCH as publication date\n"
                "  --store          write output using @manpage name\n"
                "  --select NAME    only output pages with @manpage NAME\n"
                "  --gnupgorg       prepare for use at www.gnupg.org\n"
                "  --verbose        enable extra informational output\n"
                "  --debug          enable additional debug output\n"
                "  --help           display this help and exit\n"
                "  -I DIR           also search in include DIR\n"
                "  -D MACRO         define MACRO to 1\n\n"
                "With no FILE, or when FILE is -, read standard input.\n\n"
                "Report bugs to <https://bugs.gnupg.org>.");
          exit (0);
        }
      else if (!strcmp (*argv, "--version"))
        {
          puts (PGM " " VERSION "\n"
               "Copyright (C) 2005, 2017 g10 Code GmbH\n"
               "This program comes with ABSOLUTELY NO WARRANTY.\n"
               "This is free software, and you are welcome to redistribute it\n"
                "under certain conditions. See the file COPYING for details.");
          exit (0);
        }
      else if (!strcmp (*argv, "--html"))
        {
          htmlmode = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--gnupgorg"))
        {
          gnupgorgmode = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--verbose"))
        {
          verbose = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--quiet"))
        {
          quiet = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--debug"))
        {
          verbose = debug = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--source"))
        {
          argc--; argv++;
          if (argc)
            {
              opt_source = *argv;
              argc--; argv++;
            }
        }
      else if (!strcmp (*argv, "--release"))
        {
          argc--; argv++;
          if (argc)
            {
              opt_release = *argv;
              argc--; argv++;
            }
        }
      else if (!strcmp (*argv, "--date"))
        {
          argc--; argv++;
          if (argc)
            {
              opt_date = *argv;
              argc--; argv++;
            }
        }
      else if (!strcmp (*argv, "--store"))
        {
          opt_store = 1;
          argc--; argv++;
        }
      else if (!strcmp (*argv, "--select"))
        {
          argc--; argv++;
          if (argc)
            {
              opt_select = strrchr (*argv, '/');
              if (opt_select)
                opt_select++;
              else
                opt_select = *argv;
              argc--; argv++;
            }
        }
      else if (!strcmp (*argv, "-I"))
        {
          argc--; argv++;
          if (argc)
            {
              opt_include = *argv;
              argc--; argv++;
            }
        }
      else if (!strcmp (*argv, "-D"))
        {
          argc--; argv++;
          if (argc)
            {
              add_predefined_macro (*argv);
              argc--; argv++;
            }
        }
    }

  if (argc > 1)
    die ("usage: " PGM " [OPTION] [FILE] (try --help for more information)\n");

  /* Take care of supplied timestamp for reproducible builds.  See
   * https://reproducible-builds.org/specs/source-date-epoch/  */
  if (!opt_date && (s = getenv ("SOURCE_DATE_EPOCH")) && *s)
    opt_date = s;

  /* Start processing. */
  if (argc && strcmp (*argv, "-"))
    {
      FILE *fp = fopen (*argv, "rb");
      if (!fp)
        die ("%s:0: can't open file: %s", *argv, strerror (errno));
      top_parse_file (*argv, fp);
      fclose (fp);
    }
  else
    top_parse_file ("-", stdin);

  return !!any_error;
}


/*
Local Variables:
compile-command: "gcc -Wall -g -o yat2m yat2m.c"
End:
*/
