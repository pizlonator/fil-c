/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_WORD_LIST__HPP
#define ASPELL_WORD_LIST__HPP


namespace acommon {

class StringEnumeration;

class WordList {
 public:
  class Convert * from_internal_;
  virtual bool empty() const = 0;
  virtual unsigned int size() const = 0;
  virtual StringEnumeration * elements() const = 0;
  WordList() : from_internal_(0) {}
  virtual ~WordList() {}
};


}

#endif /* ASPELL_WORD_LIST__HPP */
