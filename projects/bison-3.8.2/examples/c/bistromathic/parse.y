/* Parser and scanner for bistromathic.   -*- C -*-

   Copyright (C) 2019-2021 Free Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

%require "3.7"

// Emitted on top of the implementation file.
%code top {
  #include <ctype.h>  // isdigit
  #include <locale.h> // LC_ALL
  #include <math.h>   // cos, sin, etc.
  #include <stdarg.h> // va_start
  #include <stdio.h>  // printf
  #include <stdlib.h> // calloc
  #include <string.h> // strcmp

  #include <readline/readline.h>
  #include <readline/history.h>

  #if defined ENABLE_NLS && ENABLE_NLS
  // Unable the translation of Bison's generated messages.
  # define YYENABLE_NLS 1
  # include <libintl.h>
  // Unless specified otherwise, we expect bistromathic's own
  // catalog to be installed in the same tree as Bison's catalog.
  # ifndef LOCALEDIR
  #  define LOCALEDIR BISON_LOCALEDIR
  # endif
  #endif
}

// Emitted in the header file, before the definition of YYSTYPE.
%code requires {
  // Function type.
  typedef double (func_t) (double);

  // Data type for links in the chain of symbols.
  typedef struct symrec symrec;
  struct symrec
  {
    char *name;  // name of symbol
    int type;    // type of symbol: either VAR or FUN
    union
    {
      double var;    // value of a VAR
      func_t *fun;   // value of a FUN
    } value;
    symrec *next;  // link field
  };

  symrec *putsym (char const *name, int sym_type);
  symrec *getsym (char const *name);

  // Exchanging information with the parser.
  typedef struct
  {
    // Whether to not emit error messages.
    int silent;
    // The current input line.
    const char *line;
  } user_context;
}

// Emitted in the header file, after the definition of YYSTYPE.
%code provides {
# ifndef __attribute__
#  ifndef __GNUC__
#   define __attribute__(Spec) /* empty */
#  endif
# endif

  yytoken_kind_t
  yylex (const char **line, YYSTYPE *yylval, YYLTYPE *yylloc,
         const user_context *uctx);
  void yyerror (const YYLTYPE *loc, const user_context *uctx,
                char const *format, ...)
    __attribute__ ((__format__ (__printf__, 3, 4)));
}

// Emitted in the implementation file.
%code {
  // Print *LOC on OUT.
  static void location_print (FILE *out, YYLTYPE const * const loc);
  #define YYLOCATION_PRINT location_print

  #if defined ENABLE_NLS && ENABLE_NLS
  # define _(Msgid)  gettext (Msgid)
  #else
  # define _(Msgid)  (Msgid)
  #endif

  // Whether to quit.
  int done = 0;
}

// Include the header in the implementation rather than duplicating it.
%define api.header.include {"parse.h"}

// Don't share global variables between the scanner and the parser.
%define api.pure full

// Generate a push parser.
%define api.push-pull push

// To avoid name clashes (e.g., with C's EOF) prefix token definitions
// with TOK_ (e.g., TOK_EOF).
%define api.token.prefix {TOK_}

// Customized syntax error messages (see yyreport_syntax_error)...
%define parse.error custom

// ... with locations...
%locations

// ... and accurate list of expected tokens.
%define parse.lac full

// Generate the parser description file (calc.output).
%verbose

// User information exchanged with the parser and scanner.
%param {const user_context *uctx}

// Generate YYSTYPE from the types assigned to symbols.
%define api.value.type union
%token
    PLUS   "+"
    MINUS  "-"
    STAR   "*"
    SLASH  "/"
    CARET  "^"
    LPAREN "("
    RPAREN ")"
    EQUAL  "="
    EXIT   "exit"
  <double>
    NUM _("number")
  <symrec*>
    FUN _("function")
    VAR _("variable")

%nterm <double>  exp

// Enable run-time traces (yydebug).
%define parse.trace

// Formatting semantic values in debug traces.
%printer { fprintf (yyo, "%s", $$->name); } VAR;
%printer { fprintf (yyo, "%s()", $$->name); } FUN;
%printer { fprintf (yyo, "%g", $$); } <double>;


// Precedence (from lowest to highest) and associativity.
%precedence "="
%left "+" "-"
%left "*" "/"
%precedence NEG // negation--unary minus
%right "^"      // exponentiation

%% // The grammar follows.
input:
  %empty
| exp     { printf ("%.10g\n", $exp); }
| "exit"  { done = 1; }
;

exp:
  NUM
| VAR               { $$ = $VAR->value.var; }
| VAR "=" exp       { $$ = $3; $VAR->value.var = $3; }
| FUN "(" exp ")"   { $$ = $FUN->value.fun ($3); }
| exp[l] "+" exp[r] { $$ = $l + $r; }
| exp[l] "-" exp[r] { $$ = $l - $r; }
| exp[l] "*" exp[r] { $$ = $l * $r; }
| exp[l] "/" exp[r]
  {
    if ($r == 0)
      {
        yyerror (&@$, uctx, _("error: division by zero"));
        YYERROR;
      }
    else
      $$ = $l / $r;
  }
| "-" exp  %prec NEG { $$ = -$2; }
| exp[l] "^" exp[r]  { $$ = pow ($l, $r); }
| "(" exp ")"        { $$ = $2; }
| "(" error ")"      { $$ = 666; }
;

// End of grammar.
%%

/*------------.
| Functions.  |
`------------*/

struct init
{
  char const *name;
  func_t *fun;
};

static struct init const funs[] =
{
  { "atan", atan },
  { "cos",  cos  },
  { "exp",  exp  },
  { "ln",   log  },
  { "sin",  sin  },
  { "sqrt", sqrt },
  { 0, 0 },
};

// The symbol table: a chain of 'struct symrec'.
static symrec *sym_table;

// Put functions in table.
static void
init_table (void)
{
  for (int i = 0; funs[i].name; i++)
    {
      symrec *ptr = putsym (funs[i].name, TOK_FUN);
      ptr->value.fun = funs[i].fun;
    }
}

symrec *
putsym (char const *name, int sym_type)
{
  symrec *res = (symrec *) malloc (sizeof (symrec));
  res->name = strdup (name);
  res->type = sym_type;
  res->value.var = 0; // Set value to 0 even if fun.
  res->next = sym_table;
  sym_table = res;
  return res;
}

symrec *
getsym (char const *name)
{
  for (symrec *p = sym_table; p; p = p->next)
    if (strcmp (p->name, name) == 0)
      return p;
  return NULL;
}

// How many symbols are registered.
static int
symbol_count (void)
{
  int res = 0;
  for (symrec *p = sym_table; p; p = p->next)
    ++res;
  return res;
}



/*------------.
| Locations.  |
`------------*/

// Print *LOC on OUT.  Do it in a compact way, that avoids redundancy.

static void
location_print (FILE *out, YYLTYPE const * const loc)
{
  fprintf (out, "%d.%d", loc->first_line, loc->first_column);

  int end_col = 0 != loc->last_column ? loc->last_column - 1 : 0;
  if (loc->first_line < loc->last_line)
    fprintf (out, "-%d.%d", loc->last_line, end_col);
  else if (loc->first_column < end_col)
    fprintf (out, "-%d", end_col);
}


/*----------.
| Scanner.  |
`----------*/

yytoken_kind_t
yylex (const char **line, YYSTYPE *yylval, YYLTYPE *yylloc,
       const user_context *uctx)
{
  int c;

  // Ignore white space, get first nonwhite character.
  do {
    // Move the first position onto the last.
    yylloc->first_line = yylloc->last_line;
    yylloc->first_column = yylloc->last_column;

    yylloc->last_column += 1;
    c = *((*line)++);
  } while (c == ' ' || c == '\t');

  switch (c)
    {
    case '+': return TOK_PLUS;
    case '-': return TOK_MINUS;
    case '*': return TOK_STAR;
    case '/': return TOK_SLASH;
    case '^': return TOK_CARET;
    case '=': return TOK_EQUAL;
    case '(': return TOK_LPAREN;
    case ')': return TOK_RPAREN;

    case '!': return TOK_YYUNDEF;

    case '\0': return TOK_YYEOF;

      // Numbers.
    case '.':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      {
        int nchars = 0;
        if (sscanf (*line - 1, "%lf%n", &yylval->TOK_NUM, &nchars) != 1)
          abort ();
        *line += nchars - 1;
        yylloc->last_column += nchars - 1;
        return TOK_NUM;
      }

      // Identifiers.
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y':
    case 'z':
      {
        int nchars = 0;
        char buf[100];
        if (sscanf (*line - 1, "%99[a-z]%n", buf, &nchars) != 1)
          abort ();
        *line += nchars - 1;
        yylloc->last_column += nchars - 1;
        if (strcmp (buf, "exit") == 0)
          return TOK_EXIT;
        else
          {
            symrec *s = getsym (buf);
            if (!s)
              s = putsym (buf, TOK_VAR);
            yylval->TOK_VAR = s;
            return s->type;
          }
      }

      // Stray characters.
    default:
      yyerror (yylloc, uctx, _("syntax error: invalid character: %c"), c);
      return TOK_YYerror;
    }
}


/*---------.
| Parser.  |
`---------*/


static const char *
error_format_string (int argc)
{
  switch (argc)
    {
    default: // Avoid compiler warnings.
    case 0: return _("%@: syntax error");
    case 1: return _("%@: syntax error: unexpected %u");
      // TRANSLATORS: '%@' is a location in a file, '%u' is an
      // "unexpected token", and '%0e', '%1e'... are expected tokens
      // at this point.
      //
      // For instance on the expression "1 + * 2", you'd get
      //
      // 1.5: syntax error: expected - or ( or number or function or variable before *
    case 2: return _("%@: syntax error: expected %0e before %u");
    case 3: return _("%@: syntax error: expected %0e or %1e before %u");
    case 4: return _("%@: syntax error: expected %0e or %1e or %2e before %u");
    case 5: return _("%@: syntax error: expected %0e or %1e or %2e or %3e before %u");
    case 6: return _("%@: syntax error: expected %0e or %1e or %2e or %3e or %4e before %u");
    case 7: return _("%@: syntax error: expected %0e or %1e or %2e or %3e or %4e or %5e before %u");
    case 8: return _("%@: syntax error: expected %0e or %1e or %2e or %3e or %4e or %5e etc., before %u");
    }
}


int
yyreport_syntax_error (const yypcontext_t *ctx, const user_context *uctx)
{
  if (uctx->silent)
    return 0;

  enum { ARGS_MAX = 6 };
  yysymbol_kind_t arg[ARGS_MAX];
  int argsize = yypcontext_expected_tokens (ctx, arg, ARGS_MAX);
  if (argsize < 0)
    return argsize;
  const int too_many_expected_tokens = argsize == 0 && arg[0] != YYSYMBOL_YYEMPTY;
  if (too_many_expected_tokens)
    argsize = ARGS_MAX;
  const char *format = error_format_string (1 + argsize + too_many_expected_tokens);

  const YYLTYPE *loc = yypcontext_location (ctx);
  while (*format)
    // %@: location.
    if (format[0] == '%' && format[1] == '@')
      {
        YYLOCATION_PRINT (stderr, loc);
        format += 2;
      }
    // %u: unexpected token.
    else if (format[0] == '%' && format[1] == 'u')
      {
        fputs (yysymbol_name (yypcontext_token (ctx)), stderr);
        format += 2;
      }
    // %0e, %1e...: expected token.
    else if (format[0] == '%'
             && isdigit ((unsigned char) format[1])
             && format[2] == 'e'
             && (format[1] - '0') < argsize)
      {
        int i = format[1] - '0';
        fputs (yysymbol_name (arg[i]), stderr);
        format += 3;
      }
    else
      {
        fputc (*format, stderr);
        ++format;
      }
  fputc ('\n', stderr);

  // Quote the source line.
  {
    fprintf (stderr, "%5d | %s\n", loc->first_line, uctx->line);
    fprintf (stderr, "%5s | %*s", "", loc->first_column, "^");
    for (int i = loc->last_column - loc->first_column - 1; 0 < i; --i)
      putc ('~', stderr);
    putc ('\n', stderr);
  }
  return 0;
}


// Called by yyparse on errors to report the error to the user.
void
yyerror (const YYLTYPE *loc, const user_context *uctx, char const *format, ...)
{
  if (uctx->silent)
    return;

  YYLOCATION_PRINT (stderr, loc);
  fputs (": ", stderr);
  va_list args;
  va_start (args, format);
  vfprintf (stderr, format, args);
  va_end (args);
  putc ('\n', stderr);
}


// Return a newly allocated copy of at most N bytes of STRING.  In
// other words, return a copy of the initial segment of length N of
// STRING.
static char *
xstrndup (const char *string, size_t n)
{
  // len = strnlen (string, n), portably.
  const char *end = memchr (string, '\0', n);
  size_t len = end ? (size_t) (end - string) : n;
  char *new = malloc (len + 1);
  if (!new)
    abort ();
  new[len] = '\0';
  return memcpy (new, string, len);
}


/*-----------.
| Readline.  |
`-----------*/

// Parse (and execute) this line.
static int
process_line (YYLTYPE *lloc, const char *line)
{
  user_context uctx = {0, line};
  yypstate *ps = yypstate_new ();
  int status = 0;
  do {
    YYSTYPE lval;
    yytoken_kind_t token = yylex (&line, &lval, lloc, &uctx);
    status = yypush_parse (ps, token, &lval, lloc, &uctx);
  } while (status == YYPUSH_MORE);
  yypstate_delete (ps);
  lloc->last_line++;
  lloc->last_column = 1;
  return status;
}

// Get the list of possible tokens after INPUT was read.
// Returns a nonnegative.
static int
expected_tokens (const char *input,
                 int *tokens, int ntokens)
{
  YYDPRINTF ((stderr, "expected_tokens (\"%s\")", input));
  user_context uctx = {1, input};

  // Parse the current state of the line.
  yypstate *ps = yypstate_new ();
  int status = 0;
  YYLTYPE lloc = { 1, 1, 1, 1 };
  do {
    YYSTYPE lval;
    yytoken_kind_t token = yylex (&input, &lval, &lloc, &uctx);
    // Don't let the parse know when we reach the end of input.
    if (token == TOK_YYEOF)
      break;
    status = yypush_parse (ps, token, &lval, &lloc, &uctx);
  } while (status == YYPUSH_MORE);

  int res = 0;
  // If there were parse errors, don't propose completions.
  if (!ps->yynerrs)
    {
      // Then query for the accepted tokens at this point.
      res = yypstate_expected_tokens (ps, tokens, ntokens);
      if (res < 0)
        abort ();
    }
  yypstate_delete (ps);
  return res;
}

// Attempt to complete on the contents of TEXT.  START and END bound
// the region of rl_line_buffer that contains the word to complete.
// TEXT is the word to complete.  We can use the entire contents of
// rl_line_buffer in case we want to do some simple parsing.  Return
// the array of matches, or NULL if there aren't any.
static char **
completion (const char *text, int start, int end)
{
  YYDPRINTF ((stderr, "completion (\"%.*s[%.*s]%s\")\n",
              start, rl_line_buffer,
              end - start, rl_line_buffer + start,
              rl_line_buffer + end));

  // Get list of token numbers.
  int tokens[YYNTOKENS];
  char *line = xstrndup (rl_line_buffer, (size_t) start);
  int ntokens = expected_tokens (line, tokens, YYNTOKENS);
  free (line);

  // Build MATCHES, the list of possible completions.
  const size_t len = strlen (text);
  // Need initial prefix and final NULL.
  char **matches
    = calloc ((size_t) ntokens + (size_t) symbol_count () + 2, sizeof *matches);
  if (!matches)
    abort ();
  int match = 1;
  for (int i = 0; i < ntokens; ++i)
    switch (tokens[i])
      {
      case YYSYMBOL_FUN:
        for (symrec *s = sym_table; s; s = s->next)
          if (s->type == TOK_FUN && strncmp (text, s->name, len) == 0)
            matches[match++] = strdup (s->name);
        break;
      case YYSYMBOL_VAR:
        for (symrec *s = sym_table; s; s = s->next)
          if (s->type == TOK_VAR && strncmp (text, s->name, len) == 0)
            matches[match++] = strdup (s->name);
        break;
      default:
        {
          const char* token = yysymbol_name (tokens[i]);
          if (strncmp (text, token, len) == 0)
            matches[match++] = strdup (token);
          break;
        }
      }

  // Find the longest common prefix, and install it in matches[0], as
  // required by readline.
  if (match == 1)
    matches[0] = strdup (text);
  else
    {
      size_t lcplen = strlen (matches[1]);
      for (int i = 2; i < match && lcplen; ++i)
        for (size_t j = 0; j < lcplen; ++j)
          if (matches[1][j] != matches[i][j])
            lcplen = j;
      matches[0] = xstrndup (matches[1], lcplen);
    }

  if (yydebug)
    {
      fprintf (stderr, "completion (\"%.*s[%.*s]%s\") = ",
               start, rl_line_buffer,
               end - start, rl_line_buffer + start,
               rl_line_buffer + end);
      for (int i = 1; matches[i]; ++i)
        fprintf (stderr, "%s%s",
                 i == 1 ? "{" : ", ",
                 matches[i]);
      fprintf (stderr, "}\n");
    }

  // Don't fall back to proposing file names.
  rl_attempted_completion_over = 1;
  return matches;
}

static void
init_readline (void)
{
  // Allow conditional parsing of the ~/.inputrc file.
  rl_readline_name = "bistromathic";

  // Tell the completer that we want a crack first.
  rl_attempted_completion_function = completion;

  // The basic list of characters that signal a break between words
  // for the completer routine.
  rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(+-*/^)";
}



/*-------.
| Main.  |
`-------*/

int
main (int argc, char const* argv[])
{
#if defined ENABLE_NLS && ENABLE_NLS
  // Set up internationalization.
  setlocale (LC_ALL, "");
  // Use Bison's standard translation catalog for error messages
  // (the generated messages).
  bindtextdomain ("bison-runtime", BISON_LOCALEDIR);
  // The translation catalog of bistromathic is actually included in
  // Bison's.  In your own project, use the name of your project.
  bindtextdomain ("bison", LOCALEDIR);
  textdomain ("bison");
#endif

  // Enable parse traces on option -p.
  if (1 < argc && strcmp (argv[1], "-p") == 0)
    yydebug = 1;
  init_table ();
  init_readline ();
  YYLTYPE lloc = {1, 1, 1, 1};
  while (!done)
    {
      char *line = readline ("> ");
      if (!line)
        {
          // Finish the line started by the prompt.
          putchar ('\n');
          break;
        }
      if (*line)
        add_history (line);
      process_line (&lloc, line);
      free (line);
    }
}
