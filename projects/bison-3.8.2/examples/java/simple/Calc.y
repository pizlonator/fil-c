/* Simple parser and scanner in Java.   -*- Java -*-

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

%language "Java"

%define api.parser.class {Calc}
%define api.parser.public

%define parse.error verbose

%code imports {
  import java.io.IOException;
  import java.io.InputStream;
  import java.io.InputStreamReader;
  import java.io.Reader;
  import java.io.StreamTokenizer;
}

%code {
  public static void main(String[] args) throws IOException {
    CalcLexer l = new CalcLexer(System.in);
    Calc p = new Calc(l);
    if (!p.parse())
      System.exit(1);
  }
}

/* Bison Declarations */
%token <Integer> NUM "number"
%type  <Integer> exp

%nonassoc '='       /* comparison            */
%left '-' '+'
%left '*' '/'
%precedence NEG     /* negation--unary minus */
%right '^'          /* exponentiation        */

/* Grammar follows */
%%
input:
  line
| input line
;

line:
  '\n'
| exp '\n'           { System.out.println($exp); }
| error '\n'
;

exp:
  NUM                { $$ = $1; }
| exp '=' exp
  {
    if ($1.intValue() != $3.intValue())
      yyerror("calc: error: " + $1 + " != " + $3);
  }
| exp '+' exp        { $$ = $1 + $3;  }
| exp '-' exp        { $$ = $1 - $3;  }
| exp '*' exp        { $$ = $1 * $3;  }
| exp '/' exp        { $$ = $1 / $3;  }
| '-' exp  %prec NEG { $$ = -$2; }
| exp '^' exp        { $$ = (int) Math.pow($1, $3); }
| '(' exp ')'        { $$ = $2; }
| '(' error ')'      { $$ = 1111; }
| '!'                { $$ = 0; return YYERROR; }
| '-' error          { $$ = 0; return YYERROR; }
;


%%
class CalcLexer implements Calc.Lexer {

  StreamTokenizer st;

  public CalcLexer(InputStream is) {
    st = new StreamTokenizer(new InputStreamReader(is));
    st.resetSyntax();
    st.eolIsSignificant(true);
    st.whitespaceChars('\t', '\t');
    st.whitespaceChars(' ', ' ');
    st.wordChars('0', '9');
  }

  public void yyerror(String s) {
    System.err.println(s);
  }

  Integer yylval;

  public Object getLVal() {
    return yylval;
  }

  public int yylex() throws IOException {
    int ttype = st.nextToken();
    switch (ttype) {
    case StreamTokenizer.TT_EOF:
      return YYEOF;
    case StreamTokenizer.TT_EOL:
      return (int) '\n';
    case StreamTokenizer.TT_WORD:
      yylval = Integer.parseInt(st.sval);
      return NUM;
    default:
      return ttype;
    }
  }
}
