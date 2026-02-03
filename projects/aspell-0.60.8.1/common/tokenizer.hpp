// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ACOMMON_TOKENIZER__HPP
#define ACOMMON_TOKENIZER__HPP

#include "char_vector.hpp"
#include "filter_char.hpp"
#include "filter_char_vector.hpp"

namespace acommon {

  class Convert;
  class Speller;
  class Config;

  class Tokenizer {

  public:
    Tokenizer();
    virtual ~Tokenizer();

    FilterChar * word_begin;
    FilterChar * word_end;
    FilterChar * end;
    
    CharVector word; // this word is in the final encoded form
    unsigned int begin_pos; // pointers back to the original word
    unsigned int end_pos;
    
    // The string passed in _must_ have a null character
    // at stop - 1. (ie stop must be one past the end)
    void reset (FilterChar * in, FilterChar * stop);
    bool at_end() const {return word_begin == word_end;}
    
    virtual bool advance() = 0; // returns false if there is nothing left

    bool is_begin(unsigned char c) const
      {return char_type_[c].begin;}
    bool is_middle(unsigned char c) const
      {return char_type_[c].middle;}
    bool is_end(unsigned char c) const
      {return char_type_[c].end;}
    bool is_word(unsigned char c) const
      {return char_type_[c].word;}

  public: // but don't use
    // The speller class is expected to fill these members in
    struct CharType {
      bool begin;
      bool middle;
      bool end;
      bool word;
      CharType() : begin(false), middle(false), end(false), word(false) {}
    };
    
    CharType char_type_[256];
    Convert * conv_;
    FilterCharVector buf_;
  };

  // returns a new tokenizer and sets it up with the given speller
  // class

  PosibErr<Tokenizer *> new_tokenizer(Speller *);

}

#endif
