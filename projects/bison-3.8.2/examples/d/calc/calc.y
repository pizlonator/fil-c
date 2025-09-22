/* Parser and scanner for calc in D.   -*- D -*-

   Copyright (C) 2018-2021 Free Software Foundation, Inc.

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

%language "D"

%define api.parser.class {Calc}
%define api.push-pull push
%define api.token.constructor
%define api.value.type union
%define parse.error detailed
%define parse.trace

%locations

/* Bison Declarations */
%token PLUS   "+"
       MINUS  "-"
       STAR   "*"
       SLASH  "/"
       LPAR   "("
       RPAR   ")"
       EOL    "end of line"
%token <int> NUM "number"
%type  <int> exp
%printer { yyo.write($$); } <int>

%left "-" "+"
%left "*" "/"
%precedence UNARY   /* unary operators */

/* Grammar follows */
%%
input:
  line
| input line
;

line:
  EOL
| exp EOL           { writeln ($exp); }
| error EOL         { yyerrok(); }
;

exp:
  NUM                  { $$ = $1; }
| exp "+" exp          { $$ = $1 + $3; }
| exp "-" exp          { $$ = $1 - $3; }
| exp "*" exp          { $$ = $1 * $3; }
| exp "/" exp          { $$ = $1 / $3; }
| "+" exp  %prec UNARY { $$ = $2; }
| "-" exp  %prec UNARY { $$ = -$2; }
| "(" exp ")"          { $$ = $2; }
;

%%
import std.range.primitives;
import std.stdio;

auto calcLexer(R)(R range)
if (isInputRange!R && is(ElementType!R : dchar))
{
  return new CalcLexer!R(range);
}

auto calcLexer(File f)
{
  import std.algorithm : map, joiner;
  import std.utf : byDchar;

  return f.byChunk(1024)        // avoid making a syscall roundtrip per char
          .map!(chunk => cast(char[]) chunk) // because byChunk returns ubyte[]
          .joiner               // combine chunks into a single virtual range of char
          .calcLexer;           // forward to other overload
}

class CalcLexer(R) : Lexer
if (isInputRange!R && is(ElementType!R : dchar))
{
  R input;

  this(R r) { input = r; }

  Location location;

  // Should be a local in main, shared with %parse-param.
  int exit_status = 0;

  void yyerror(const Location loc, string s)
  {
    exit_status = 1;
    stderr.writeln(loc.toString(), ": ", s);
  }

  Symbol yylex()
  {
    import std.uni : isWhite, isNumber;

    // Skip initial spaces
    while (!input.empty && input.front != '\n' && isWhite(input.front))
    {
      location.end.column++;
      input.popFront;
    }
    location.step();

    if (input.empty)
      return Symbol.YYEOF(location);

    // Numbers.
    if (input.front.isNumber)
    {
      import std.compiler : version_minor;
      static if (version_minor >= 95)
      {
        // from Dlang v2.095.0 onwards std.conv.parse reports
        // the number of consumed characters
        import std.typecons : Flag, Yes;
        import std.conv : parse;
        auto parsed = parse!(int, R, Yes.doCount)(input);
        int ival = parsed.data;
        location.end.column += cast(int) parsed.count;
      }
      else
      {
        auto copy = input;
        import std.conv : parse;
        int ival = input.parse!int;
        while (!input.empty && copy.front != input.front)
        {
          location.end.column++;
          copy.popFront;
        }
      }
      return Symbol.NUM(ival, location);
    }

    // Individual characters
    auto ch = input.front;
    input.popFront;
    location.end.column++;
    switch (ch)
    {
      case '+':  return Symbol.PLUS(location);
      case '-':  return Symbol.MINUS(location);
      case '*':  return Symbol.STAR(location);
      case '/':  return Symbol.SLASH(location);
      case '(':  return Symbol.LPAR(location);
      case ')':  return Symbol.RPAR(location);
      case '\n':
      {
        location.end.line++;
        location.end.column = 1;
        return Symbol.EOL(location);
      }
      default: assert(0);
    }
  }
}

int main()
{
  auto l = calcLexer(stdin);
  auto p = new Calc(l);
  import core.stdc.stdlib : getenv;
  if (getenv("YYDEBUG"))
    p.setDebugLevel(1);
  int status;
  do {
    status = p.pushParse(l.yylex());
  } while (status == PUSH_MORE);
  return l.exit_status;
}
