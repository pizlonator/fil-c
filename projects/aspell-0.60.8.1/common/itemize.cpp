// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <string.h>
#include <stdlib.h>

#include "asc_ctype.hpp"
#include "itemize.hpp"
#include "mutable_container.hpp"
#include <stdio.h>
#include <cstdio>

//FIXME: it should be possible to escape ',' '+' '-' '!' so that they can
//       appear in values
//       If '\' is used, then what about when the option file is parsed
//         as it strips away all '\' escapes.

namespace acommon {

  struct ItemizeItem {
    char   action;
    const char * name;
    ItemizeItem() : action('\0'), name(0) {}
  };

  class ItemizeTokenizer {
  private:
    char * list;
    char * i;
  public:
    ItemizeTokenizer(const char * l);
    ~ItemizeTokenizer();
  private:
    ItemizeTokenizer(const ItemizeTokenizer & other) ;
    ItemizeTokenizer & operator=(const ItemizeTokenizer & other);
  public:
    ItemizeItem next();
  };

  ItemizeTokenizer::ItemizeTokenizer(const char * l) 
  {
    size_t size = strlen(l) + 1;
    list = new char[size];
    i = list;
    strncpy(list, l, size);
  }

  ItemizeTokenizer::~ItemizeTokenizer() 
  {
    delete[] list;
  }


  ItemizeItem ItemizeTokenizer::next() 
  {
    ItemizeItem li;
    while (*i != '\0' && (asc_isspace(*i) || *i == ',')) ++i;
    if (*i == '\0') return li;
    li.action = *i;
    if (*i == '+' || *i == '-') {
      ++i;
    } else if (*i == '!') {
      li.name = "";
      ++i;
      return li;
    } else {
      li.action = '+';
    }
    while (*i != '\0' && *i != ',' && asc_isspace(*i)) ++i;
    if (*i == '\0' || *i == ',') return next();
    li.name = i;
    while (*i != '\0' && *i != ',') ++i;
    while (i != list && asc_isspace(*(i-1))) --i;
    if (*i != '\0') {
      *i = '\0';
      ++i;
    }
    return li;
  }


  PosibErr<void> itemize (ParmString s, MutableContainer & d) {
    ItemizeTokenizer els(s);
    ItemizeItem li;
    while (li = els.next(), li.name != 0) {
      switch (li.action) {
      case '+':
	RET_ON_ERR(d.add(li.name));
	break;
      case '-':
	RET_ON_ERR(d.remove(li.name));
	break;
      case '!':
	RET_ON_ERR(d.clear());
	break;
      default:
	abort();
      }
    }
    return no_err;
  }

}
