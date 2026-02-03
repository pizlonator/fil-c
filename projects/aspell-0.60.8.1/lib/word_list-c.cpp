/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "string_enumeration.hpp"
#include "word_list.hpp"

namespace acommon {

class StringEnumeration;
class WordList;

extern "C" int aspell_word_list_empty(const WordList * ths)
{
  return ths->empty();
}

extern "C" unsigned int aspell_word_list_size(const WordList * ths)
{
  return ths->size();
}

extern "C" StringEnumeration * aspell_word_list_elements(const WordList * ths)
{
  StringEnumeration * els = ths->elements();
  els->from_internal_ = ths->from_internal_;
  return els;
}



}

