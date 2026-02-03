/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "document_checker.hpp"
#include "error.hpp"

namespace acommon {

class CanHaveError;
class DocumentChecker;
struct Error;
class Filter;
class Speller;

extern "C" void delete_aspell_document_checker(DocumentChecker * ths)
{
  delete ths;
}

extern "C" unsigned int aspell_document_checker_error_number(const DocumentChecker * ths)
{
  return ths->err_ == 0 ? 0 : 1;
}

extern "C" const char * aspell_document_checker_error_message(const DocumentChecker * ths)
{
  return ths->err_ ? ths->err_->mesg : "";
}

extern "C" const Error * aspell_document_checker_error(const DocumentChecker * ths)
{
  return ths->err_;
}

extern "C" CanHaveError * new_aspell_document_checker(Speller * speller)
{
  PosibErr<DocumentChecker *> ret = new_document_checker(speller);
  if (ret.has_err()) {
    return new CanHaveError(ret.release_err());
  } else {
    return ret;
  }
}

extern "C" DocumentChecker * to_aspell_document_checker(CanHaveError * obj)
{
  return static_cast<DocumentChecker *>(obj);
}

extern "C" void aspell_document_checker_reset(DocumentChecker * ths)
{
  ths->reset();
}

extern "C" void aspell_document_checker_process(DocumentChecker * ths, const char * str, int str_size)
{
  ths->process(str, str_size);
}

extern "C" void aspell_document_checker_process_wide(DocumentChecker * ths, const void * str, int str_size, int str_type_width)
{
  ths->process_wide(str, str_size, str_type_width);
}

extern "C" Token aspell_document_checker_next_misspelling(DocumentChecker * ths)
{
  return ths->next_misspelling();
}

extern "C" Token aspell_document_checker_next_misspelling_adj(DocumentChecker * ths, int type_width)
{
  Token res = ths->next_misspelling();
  res.offset /= type_width;
  res.len /= type_width;
  return res;
}

extern "C" Filter * aspell_document_checker_filter(DocumentChecker * ths)
{
  return ths->filter();
}



}

