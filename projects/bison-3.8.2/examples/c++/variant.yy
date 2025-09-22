/*
  Copyright (C) 2008-2015, 2018-2021 Free Software Foundation, Inc.

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

%require "3.2"
%debug
%language "c++"
%define api.token.constructor
%define api.value.type variant
%define api.location.file none
%define parse.assert
%locations

%code requires // *.hh
{
#include <string>
#include <vector>
typedef std::vector<std::string> strings_type;
}

%code // *.cc
{
#include <iostream>
#include <sstream>

  namespace yy
  {
    // Prototype of the yylex function providing subsequent tokens.
    static parser::symbol_type yylex ();

    // Print a vector of strings.
    std::ostream&
    operator<< (std::ostream& o, const strings_type& ss)
    {
      o << '{';
      const char *sep = "";
      for (strings_type::const_iterator i = ss.begin (), end = ss.end ();
           i != end; ++i)
        {
          o << sep << *i;
          sep = ", ";
        }
      return o << '}';
    }
  }

  // Convert to string.
  template <typename T>
    std::string
    to_string (const T& t)
  {
    std::ostringstream o;
    o << t;
    return o.str ();
  }
}

%token <::std::string> TEXT;
%token <int> NUMBER;
%printer { yyo << '(' << &$$ << ") " << $$; } <*>;
%token END_OF_FILE 0;

%type <::std::string> item;
%type <::std::vector<std::string>> list;

%%

result:
  list  { std::cout << $1 << '\n'; }
;

list:
  %empty     { /* Generates an empty string list */ }
| list item  { std::swap ($$, $1); $$.push_back ($2); }
;

item:
  TEXT
| NUMBER  { $$ = to_string ($1); }
;
%%

namespace yy
{
  // Use nullptr with pre-C++11.
#if !defined __cplusplus || __cplusplus < 201103L
# define NULLPTR 0
#else
# define NULLPTR nullptr
#endif

  // The yylex function providing subsequent tokens:
  // TEXT         "I have three numbers for you."
  // NUMBER       1
  // NUMBER       2
  // NUMBER       3
  // TEXT         "And that's all!"
  // END_OF_FILE

  static
  parser::symbol_type
  yylex ()
  {
    static int count = 0;
    const int stage = count;
    ++count;
    parser::location_type loc (NULLPTR, stage + 1, stage + 1);
    switch (stage)
      {
      case 0:
        return parser::make_TEXT ("I have three numbers for you.", loc);
      case 1:
      case 2:
      case 3:
        return parser::make_NUMBER (stage, loc);
      case 4:
        return parser::make_TEXT ("And that's all!", loc);
      default:
        return parser::make_END_OF_FILE (loc);
      }
  }

  // Mandatory error function
  void
  parser::error (const parser::location_type& loc, const std::string& msg)
  {
    std::cerr << loc << ": " << msg << '\n';
  }
}

int
main ()
{
  yy::parser p;
  p.set_debug_level (!!getenv ("YYDEBUG"));
  return p.parse ();
}

// Local Variables:
// mode: C++
// End:
