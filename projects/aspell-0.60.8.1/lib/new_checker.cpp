// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include "speller.hpp"
#include "document_checker.hpp"
#include "stack_ptr.hpp"
#include "convert.hpp"
#include "tokenizer.hpp"

namespace acommon {

  PosibErr<DocumentChecker *> 
  new_document_checker(Speller * speller)
  {
    StackPtr<DocumentChecker> checker(new DocumentChecker());
    StackPtr<Tokenizer> tokenizer(new_tokenizer(speller));
    StackPtr<Filter> filter(new Filter);
    RET_ON_ERR(setup_filter(*filter, speller->config(), true, true, false));
    RET_ON_ERR(checker->setup(tokenizer.release(), speller, filter.release()));
    return checker.release();
  }

}
