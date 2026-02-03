
// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "tokenizer.hpp"
#include "convert.hpp"
#include "speller.hpp"


namespace acommon {

  class TokenizerBasic : public Tokenizer
  {
  public:
    bool advance();
  };

  bool TokenizerBasic::advance() {
    word_begin = word_end;
    begin_pos = end_pos;
    FilterChar * cur = word_begin;
    unsigned int cur_pos = begin_pos;
    word.clear();

    // skip spaces (non-word characters)
    while (*cur != 0 &&
	   !(is_word(*cur)
	     || (is_begin(*cur) && is_word(cur[1])))) 
    {
      cur_pos += cur->width;
      ++cur;
    }

    if (*cur == 0) return false;

    word_begin = cur;
    begin_pos = cur_pos;

    if (is_begin(*cur) && is_word(cur[1]))
    {
      cur_pos += cur->width;
      ++cur;
    }

    while (is_word(*cur) || 
	   (is_middle(*cur) && 
	    cur > word_begin && is_word(cur[-1]) &&
	    is_word(cur[1]) )) 
    {
      word.append(*cur);
      cur_pos += cur->width;
      ++cur;
    }

    if (is_end(*cur))
    {
      word.append(*cur);
      cur_pos += cur->width;
      ++cur;
    }

    word.append('\0');
    word_end = cur;
    end_pos = cur_pos;

    return true;
  }
#undef increment__

  PosibErr<Tokenizer *> new_tokenizer(Speller * speller)
  {
    Tokenizer * tok = new TokenizerBasic();
    speller->setup_tokenizer(tok);
    return tok;
  }

}
