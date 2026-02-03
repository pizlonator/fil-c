// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "string_list.hpp"
//#include "iostream.hpp"

namespace acommon {

  void StringList::copy(const StringList & other)
  {
    StringListNode * * cur = &first;
    StringListNode * other_cur = other.first;
    while (other_cur != 0) {
      *cur = new StringListNode(other_cur->data.c_str());
      cur = &(*cur)->next;
      other_cur = other_cur->next;
    }
    *cur = 0;
  }

  void StringList::destroy()
  {
    while (first != 0) {
      StringListNode * next = first->next;
      delete first;
      first = next;
    }
  }

  bool operator==(const StringList & rhs, 
		  const StringList & lhs)
  {
    StringListNode * rhs_cur = rhs.first;
    StringListNode * lhs_cur = lhs.first;
    while (rhs_cur != 0 && lhs_cur != 0 && rhs_cur->data == lhs_cur->data) {
      rhs_cur = rhs_cur->next;
      lhs_cur = lhs_cur->next;
    }
    return rhs_cur == 0 && lhs_cur == 0;
  }

  StringEnumeration * StringListEnumeration::clone() const
  {
    return new StringListEnumeration(*this);
  }

  void StringListEnumeration::assign(const StringEnumeration * other)
  {
    *this = *(const StringListEnumeration *)other;
  }


  StringList * StringList::clone() const
  {
    return new StringList(*this);
  }

  void StringList::assign(const StringList * other)
  {
    *this = *(const StringList *)other;
  }

  PosibErr<bool> StringList::add(ParmStr str)
  {
    //CERR.printf("ADD %s\n", str.str());
    StringListNode * * cur = &first;
    while (*cur != 0 && strcmp((*cur)->data.c_str(), str) != 0) {
      cur = &(*cur)->next;
    }
    if (*cur == 0) {
      *cur = new StringListNode(str);
      return true;
    } else {
      return false;
    }
  }

  PosibErr<bool> StringList::remove(ParmStr str)
  {
    //CERR.printf("REM %s\n", str.str());
    StringListNode  * * cur  = &first;
    while (*cur != 0 && strcmp((*cur)->data.c_str(), str)!=0 )  {
      cur = &(*cur)->next;
    }
    if (*cur == 0) {
      return false;
    } else {
      StringListNode * tmp = *cur;
      *cur = (*cur)->next;
      delete tmp;
      return true;
    }
  }

  PosibErr<void> StringList::clear()
  {
    //CERR.printf("CLEAR\n");
    StringListNode * temp;
    while (first != 0) {
      temp = first;
      first = temp->next;
      delete temp;
    }
    first = 0;
    return no_err;
  }

  StringEnumeration * StringList::elements() const
  {
    return new StringListEnumeration(first);
  }

  StringList * new_string_list() {
    return new StringList;
  }

}
