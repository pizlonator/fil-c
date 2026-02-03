// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef autil__block_slist_t_hh
#define autil__block_slist_t_hh

#include <cstddef>
#include <cstdlib>
#include <cassert>

#include "block_slist.hpp"

namespace acommon {

  template <typename T>
  void BlockSList<T>::add_block(unsigned int num) 
  {
    assert (offsetof(Node,next)==0);
    const unsigned int ptr_offset = offsetof(Node, data);
    void * block = malloc( ptr_offset + sizeof(Node) * num );
    *reinterpret_cast<void **>(block) = first_block;
    first_block = block;
    Node * first = reinterpret_cast<Node *>(reinterpret_cast<char *>(block) + ptr_offset);
    Node * i = first;
    Node * last = i + num;
    while (i + 1 != last) {
      i->next = i + 1;
      i = i + 1;
    }
    i->next = 0;
    first_available = first;  
  }

  template <typename T>
  void BlockSList<T>::clear()
  {
    void * i = first_block;
    while (i != 0) {
      void * n = *reinterpret_cast<void **>(i);
      free(i);
      i = n;
    }
    first_block = 0;
    first_available = 0;
  }

}

#endif
