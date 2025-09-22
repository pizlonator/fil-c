/*                                                       -*- C++ -*-
  Copyright (C) 2020-2021 Free Software Foundation, Inc.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/* Simplified C++ Type and Expression Grammar.
   Written by Paul Hilfinger for Bison's test suite.  */

%require "3.8"
%glr-parser
%skeleton "glr2.cc"
%define parse.assert
%define api.token.constructor
%header
%locations
%debug

// Custom error messages.
%define parse.error custom

%code requires
{
  #include "ast.hh"
}

%define api.value.type variant

%code
{
  #include <cassert>
  #include <cctype>
  #include <fstream>
  #include <cstring>

  // Merge two semantic values.
  static Node
  stmt_merge (const Node& x0, const Node& x1);

  // Fetch a token.
  static yy::parser::symbol_type
  yylex ();
}

%expect-rr 1

%type <Node> TYPENAME ID stmt expr decl declarator
%printer { yyo << $$; } <Node>

%token
  TYPENAME  "typename"
  ID        "identifier"
  SEMICOLON ";"
  EQUAL     "="
  PLUS      "+"
  LPAREN    "("
  RPAREN    ")"

%right "="
%left "+"

%%

prog : %empty
     | prog stmt   { std::cout << @2 << ": " << $2 << '\n'; }
     ;

stmt : expr ";"  %merge <stmt_merge>   { $$ = $1; }
     | decl      %merge <stmt_merge>
     | error ";"                       { $$ = Nterm ("<error>"); }
     ;

expr : ID
     | TYPENAME "(" expr ")"  { $$ = Nterm ("<cast>", $3, $1); }
     | expr "+" expr          { $$ = Nterm ("+", $1, $3); }
     | expr "=" expr          { $$ = Nterm ("=", $1, $3); }
     ;

decl : TYPENAME declarator ";"
                        { $$ = Nterm ("<declare>", $1, $2); }
     | TYPENAME declarator "=" expr ";"
                        { $$ = Nterm ("<init-declare>", $1, $2, $4); }
     ;

declarator
     : ID
     | "(" declarator ")" { $$ = $2; }
     ;

%%
std::istream* input = nullptr;
yy::parser::location_type loc;


/*---------.
| Parser.  |
`---------*/

// Generate a custom error message.
void
yy::parser::report_syntax_error (const context& ctx) const
{
  std::cerr << ctx.location () << ": syntax error";
  if (!ctx.lookahead ().empty ())
    std::cerr << " on token " << ctx.lookahead ().name ();
  {
    enum { TOKENMAX = 10 };
    symbol_kind_type expected[TOKENMAX];
    int n = ctx.expected_tokens (expected, TOKENMAX);
    if (0 < n)
      {
        for (int i = 0; i < n; ++i)
          std::cerr << (i == 0 ? " (expected " : " or ")
                    << symbol_name (expected[i]);
        std::cerr << ')';
      }
  }
  std::cerr << '\n';
}

// Report the error to the user.
void
yy::parser::error (const location_type& l, const std::string& m)
{
  std::cerr << l << ": " << m << '\n';
}

// Fetch the next token.
static yy::parser::symbol_type
yylex ()
{
  while (true)
    {
      loc.step ();
      loc += 1;
      assert (!input->eof ());
      switch (int c = input->get ())
        {
        case EOF:
          return yy::parser::make_YYEOF (loc);
        case '\t':
          loc.end.column = (loc.end.column + 7) & ~7;
          loc.step ();
          break;
        case ' ': case '\f':
          loc.step ();
          break;
        case '\n':
          loc.lines (1);
          loc.end.column = 0;
          loc.step ();
          break;
        case '+':
          return yy::parser::make_PLUS (loc);
        case '=':
          return yy::parser::make_EQUAL (loc);
        case '(':
          return yy::parser::make_LPAREN (loc);
        case ')':
          return yy::parser::make_RPAREN (loc);
        case ';':
          return yy::parser::make_SEMICOLON (loc);
        default:
          if (isalpha (c))
            {
              std::string form;
              do
                {
                  form += static_cast<char> (c);
                  loc += 1;
                  c = input->get ();
                }
              while (isalnum (c) || c == '_');
              input->unget ();
              loc -= 1;
              if (isupper (static_cast <unsigned char> (form[0])))
                return yy::parser::make_TYPENAME (Term (form), loc);
              else
                return yy::parser::make_ID (Term (form), loc);
            }
          else
            {
              auto msg = "invalid character: " + std::string(1, static_cast<char> (c));
              throw yy::parser::syntax_error (loc, msg);
            }
        }
    }
}

// Merge two semantic values as an AST including both alternatives.
static Node
stmt_merge (const Node& x0, const Node& x1)
{
  return Nterm ("<OR>", x0, x1);
}


/*-------.
| Main.  |
`-------*/

// Parse `file` using parser `parse`.
int
process (yy::parser& parse, const std::string& file)
{
  bool is_stdin = file == "-" || file.empty ();
  if (is_stdin)
    input = &std::cin;
  else
    input = new std::ifstream (file.c_str ());
  loc.initialize (nullptr, 1, 0);
  int status = parse ();
  if (!is_stdin)
    delete input;
  return status;
}

int
main (int argc, char **argv)
{
  yy::parser parse;

  if (getenv ("YYDEBUG"))
    parse.set_debug_level (1);

  bool ran = false;
  for (int i = 1; i < argc; ++i)
    // Enable parse traces on option -p.
    if (strcmp (argv[i], "-p") == 0)
      parse.set_debug_level (1);
    else
      {
        int status = process (parse, argv[i]);
        ran = true;
        if (!status)
          return status;
      }

  if (!ran)
    {
      int status = process (parse, "");
      if (!status)
        return status;
    }
  return 0;
}
