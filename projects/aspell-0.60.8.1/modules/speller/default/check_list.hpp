// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#ifndef __aspeller_check_list__
#define __aspeller_check_list__

#include "objstack.hpp"
#include "speller.hpp"

namespace aspeller {

  using acommon::CheckInfo;

  static inline void clear_check_info(CheckInfo & ci)
  {
    memset(static_cast<void *>(&ci), 0, sizeof(ci));
  }

  struct GuessInfo
  {
    int num;
    CheckInfo * head;
    GuessInfo() : num(0), head(0) {}
    void reset() { buf.reset(); num = 0; head = 0; }
    CheckInfo * add() {
      num++;
      CheckInfo * tmp = (CheckInfo *)buf.alloc_top(sizeof(CheckInfo), 
                                                   sizeof(void*));
      clear_check_info(*tmp);
      tmp->next = head;
      head = tmp;
      head->guess = true;
      return head;
    }
    void * alloc(unsigned s) {return buf.alloc_bottom(s);}
    char * dup(ParmString str) {return buf.dup(str);}
  private:
    ObjStack buf;
  };


}

#endif
