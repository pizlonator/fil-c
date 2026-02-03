/* This file is part of The New Aspell

   Copyright (C) 2004 Sergey Poznyakoff

   GNU Aspell free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   GNU Aspell is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Aspell; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>

#include "settings.h"
#include "asc_ctype.hpp"
#include "config.hpp"
#include "indiv_filter.hpp"
#include "string_map.hpp"
#include "mutable_container.hpp"
#include "clone_ptr-t.hpp"
#include "filter_char_vector.hpp"

namespace {
  using namespace acommon;

  class NroffFilter : public IndividualFilter
  {
  private:
    enum filter_state {
      initial,
      escape,
      request,
      font_switch,
      size_switch,
      skip_char,
      skip_digit,
      skip_space_before_ident,
      skip_ident,
      skip_cond,
      skip_leading_ws,
      argument,
      register_reference,
      gnu_register_name      
    } state;
    bool newline;      // Newline has just been seen.
    size_t skip_chars; // Number of characters to skip
    char req_name[2];  // Name of the recent nroff request
    int pos;           // position of the next char in req_name
    bool in_request;   // are we within a request?

    // Return true if we should ignore argument to the current request.
    bool inline ignore_request_argument ()
    {
      static char ignore_req_tab[][3] = {
	"ds",
	"de",
	"nr",
	"do",
	"so"
      };
      for (unsigned i = 0; i < sizeof(ignore_req_tab)/sizeof(ignore_req_tab[0]);
	   i++)
	if (memcmp(ignore_req_tab[i], req_name, 2) == 0)
	  return true;
      return false;
    }
    
    bool process_char (FilterChar::Chr c);
      
  public:

    PosibErr<bool> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> NroffFilter::setup(Config * opts) 
  {
    name_ = "nroff-filter";
    order_num_ = 0.2;
    reset();
    return true;
  }

  void NroffFilter::reset() 
  {
    state = initial;
    newline = true;
    in_request = false;
    pos = 0;
    skip_chars = 0;
  }

  bool NroffFilter::process_char(FilterChar::Chr c)
  {
    if (skip_chars)
      {
	skip_chars--;
	return true;
      }

    switch (state)
      {
      case initial:
	switch (c)
	  {
	  case '\\':
	    state = escape;
	    return true;
	    
	  case '\'':
	  case '.':
	    if (newline)
	      {
		state = skip_leading_ws;
		return true;
	      }
	    else if (in_request)
	      {
		state = request;
		pos = 0;
		return true;
	      }
	    break;
	  }
	break;

      case escape:
	switch (c)
	  {
	  case 'f': // font switch
	    state = font_switch;
	    return true;
	    
	  case '*': // string register
	  case 'n': // numeric register
	    state = register_reference;
	    return true;
	    
	  case 's': // change size escape
	    state = size_switch;
	    return true;

	  case '(':
	    // extended char set: \(XX
	    skip_chars = 2;
	    state = initial;
	    return true;

	  case '[':
	    // extended char set: \[comp1 comp2 ... ] (GNU extension)
	    state = gnu_register_name;
	    return true;
	      
	  case '"': // comment
	    state = initial;
	    return true;

	  case '\\': // quoting;
	    return true;

	  case '$':
	    state = argument;
	    return true;

	  default:
	    state = initial;
	  }
	break;
	
      case register_reference:
	// Reference to a string or numeric register. 
	switch (c)
	  {
	  case '(': // Traditional nroff
	    skip_chars = 2;
	    state = initial;
	    return true;
	    
	  case '[': // register long name (GNU extension)
	    state = gnu_register_name;
	    return true;

	  default:
	    state = initial;
	  }
	break;

      case gnu_register_name:
	if (c == ']' || c == '\n')
	  state = initial;
	return true;
	
      case font_switch:
	// font change escape. Either
	//     \f(XY
	// or
	//     \fX
	if (c == '(')
	  skip_chars = 2;
	state = initial;
	return true;

      case size_switch:
	// size chage escape. It can be called in a variety of ways:
	// \sN \s+N \s-N \s(NN \s+(NN \s-(NN \s(+NN \s(-NN  
	if (c == '+' || c == '-')
	  {
	    state = skip_char;
	    return true;
	  }
	/* FALL THROUGH */

      case skip_char:
	state = skip_digit;
	return true;
	
      case skip_digit:
	state = initial;
	if (asc_isdigit (c))
	  return true;
	break;

      case skip_leading_ws:
	if (asc_isspace(c))
	  break;
	state = request;
	pos = 0;
	/* FALL THROUGH */
	    
      case request:
	if (c == '\n' || c == '.') //double-dot
	  {
	    state = initial;
	  }
	else
	  {
	    req_name[pos++] = c;
	    if (pos == 2)
	      {
		if (ignore_request_argument ())
		  state = skip_space_before_ident;
		else if (memcmp (req_name, "if", 2) == 0)
		  state = skip_cond;
		else
		  state = skip_ident;
	      }
	  }
	return true;

      case skip_cond:
	if (asc_isspace(c))
	  return true;

	state = initial;
	in_request = true;
	if (c == 't' || c == 'n') // if t .. if n
	  return true;
	return process_char(c); // tail recursion;
	
      case skip_space_before_ident:
	if (!asc_isspace(c))
	  state = skip_ident;
	return true;

      case skip_ident:
	if (asc_isspace(c))
	  {
	    state = initial;
	    in_request = true;
	  }
	return true;

      case argument:
	if (asc_isdigit(c))
	  return true;
	state = initial;
	return process_char(c); // tail recursion;
      }
    
    return false;
  }

  void NroffFilter::process(FilterChar * & str, FilterChar * & stop)
  {
    FilterChar * cur = str;
    for (; cur != stop; cur++)
      {
	if (process_char (*cur) && *cur != '\n')
	  *cur = ' ';
	newline = *cur == '\n';
	if (newline)
	  in_request = false;
      }
  }

}

C_EXPORT
IndividualFilter * new_aspell_nroff_filter() {
  return new NroffFilter;                                
}

  
