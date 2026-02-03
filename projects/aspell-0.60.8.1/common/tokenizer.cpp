// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "tokenizer.hpp"
#include "convert.hpp"

namespace acommon
{

  Tokenizer::Tokenizer() 
    : word_begin(0), word_end(0), end(0), 
      begin_pos(0), end_pos(0),
      conv_(0) 
  {}

  Tokenizer::~Tokenizer()
  {}

  void Tokenizer::reset (FilterChar * start, FilterChar * stop)
  {
    bool can_encode = conv_->encode(start, stop, buf_);
    assert(can_encode);
    end_pos = 0;
    word_end = start;
    end = stop;
  }

}
