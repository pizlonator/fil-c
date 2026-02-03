/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_DOCUMENT_CHECKER__HPP
#define ASPELL_DOCUMENT_CHECKER__HPP

#include "filter.hpp"
#include "char_vector.hpp"
#include "copy_ptr.hpp"
#include "can_have_error.hpp"
#include "filter_char.hpp"
#include "filter_char_vector.hpp"

namespace acommon {

  class Config;
  class Speller;
  class Tokenizer;
  class Convert;

  struct Token {
    unsigned int offset;
    unsigned int len;
    operator bool () const {return len != 0;}
  };
  
  
  class DocumentChecker : public CanHaveError {
  public:
    // will take ownership of tokenizer and filter (even if there is an error)
    // config only used for this method.
    // speller expected to stick around.
    PosibErr<void> setup(Tokenizer *, Speller *, Filter *);
    void reset();
    void process(const char * str, int size);
    void process_wide(const void * str, int size, int type_width);
    Token next_misspelling();
    
    Filter * filter() {return filter_;}

    void set_status_fun(void (*)(void *, Token, int), void *); 
   
    DocumentChecker();
    ~DocumentChecker();
    
  private:
    CopyPtr<Filter> filter_;
    CopyPtr<Tokenizer> tokenizer_;
    void (* status_fun_)(void *, Token, int);
    void * status_fun_data_;
    Speller * speller_;
    Convert * conv_;
    FilterCharVector proc_str_;
  };

  PosibErr<DocumentChecker *> new_document_checker(Speller *);

}

#endif /* ASPELL_DOCUMENT_CHECKER__HPP */
