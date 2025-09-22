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
%define api.value.automove
%define api.location.file none
%define parse.assert
%locations

%code requires // *.hh
{
#include <memory> // std::unique_ptr
#include <string>
#include <vector>

  using string_uptr = std::unique_ptr<std::string>;
  using string_uptrs = std::vector<string_uptr>;
}

%code // *.cc
{
#include <climits>  // INT_MIN, INT_MAX
#include <iostream>
#include <sstream>

  namespace yy
  {
    // Prototype of the yylex function providing subsequent tokens.
    static parser::symbol_type yylex ();

    // Print a vector of strings.
    std::ostream&
    operator<< (std::ostream& o, const string_uptrs& ss)
    {
      o << '{';
      const char *sep = "";
      for (const auto& s: ss)
        {
          o << sep << *s;
          sep = ", ";
        }
      return o << '}';
    }
  }

  template <typename... Args>
  string_uptr
  make_string_uptr (Args&&... args)
  {
    // std::make_unique is C++14.
    return string_uptr (new std::string{std::forward<Args> (args)...});
  }
}

%token <string_uptr> TEXT;
%token <int> NUMBER;
%printer { yyo << '(' << &$$ << ") " << $$; } <*>;
%printer { yyo << *$$; } <string_uptr>;
%token END_OF_FILE 0;

%type <string_uptr> item;
%type <string_uptrs> list;

%%

result:
  list  { std::cout << $1 << '\n'; }
;

list:
  %empty     { /* Generates an empty string list */ }
| list item  { $$ = $1; $$.emplace_back ($2); }
;

item:
  TEXT
| NUMBER  { $$ = make_string_uptr (std::to_string ($1)); }
;
%%

// The last number return by the scanner is max - 1.
int max = 4;

namespace yy
{
  // The yylex function providing subsequent tokens:
  // TEXT         "I have three numbers for you."
  // NUMBER       1
  // NUMBER       2
  // NUMBER       ...
  // NUMBER       max - 1
  // TEXT         "And that's all!"
  // END_OF_FILE

  static
  parser::symbol_type
  yylex ()
  {
    static int count = 0;
    const int stage = count;
    ++count;
    auto loc = parser::location_type{nullptr, stage + 1, stage + 1};
    if (stage == 0)
      return parser::make_TEXT (make_string_uptr ("I have numbers for you."), std::move (loc));
    else if (stage < max)
      return parser::make_NUMBER (stage, std::move (loc));
    else if (stage == max)
      return parser::make_TEXT (make_string_uptr ("And that's all!"), std::move (loc));
    else
      return parser::make_END_OF_FILE (std::move (loc));
  }

  // Mandatory error function
  void
  parser::error (const parser::location_type& loc, const std::string& msg)
  {
    std::cerr << loc << ": " << msg << '\n';
  }
}

int
main (int argc, const char *argv[])
{
  if (2 <= argc && isdigit (static_cast<unsigned char> (*argv[1])))
    {
      auto maxl = strtol (argv[1], nullptr, 10);
      max = INT_MIN <= maxl && maxl <= INT_MAX ? int(maxl) : 4;
    }
  auto&& p = yy::parser{};
  p.set_debug_level (!!getenv ("YYDEBUG"));
  return p.parse ();
}

// Local Variables:
// mode: C++
// End:
