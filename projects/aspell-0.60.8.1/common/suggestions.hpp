/* This file is part of The New Aspell
 * Copyright (C) 2001-2002 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#ifndef ASPELL_SUGGESTIONS__HPP
#define ASPELL_SUGGESTIONS__HPP

#include "vector.hpp"
#include "char_vector.hpp"
#include "convert.hpp"

namespace acommon {

  class SuggestionsData {
  public:
    virtual void get_words(Convert *, Vector<CharVector> &) = 0;
    virtual void get_normalized_scores(Vector<double> &) = 0;
    virtual void get_distances(Vector<double> &) = 0;
    virtual ~SuggestionsData() {}
  };

  class Suggestions {
  public:
    SuggestionsData * sugs_;
    Convert * from_internal_;
    Vector<CharVector> words_buffer_;
    Vector<const char *> words_;
    Vector<double> normalized_scores_;
    Vector<double> distances_;
    void reset() {
      words_buffer_.clear();
      words_.clear();
      normalized_scores_.clear();
      distances_.clear();
    }
    const char * * words(unsigned * len) {
      if (words_.empty()) {
        sugs_->get_words(from_internal_, words_buffer_);
        words_.reserve(words_buffer_.size());
        for (Vector<CharVector>::iterator i = words_buffer_.begin(), e = words_buffer_.end(); i != e; ++i)
          words_.push_back(i->data());
      }
      if (len) *len = words_.size();
      return words_.data();
    }
    double * normalized_scores(unsigned * len) {
      if (normalized_scores_.empty()) {
        sugs_->get_normalized_scores(normalized_scores_);
      }
      if (len) *len = normalized_scores_.size();
      return normalized_scores_.data();
    }
    double * distances(unsigned * len) {
      if (distances_.empty()) {
        sugs_->get_distances(distances_);
      }
      if (len) *len = distances_.size();
      return distances_.data();
    }
  };
  
}

#endif /* ASPELL_SUGGESTIONS__HPP */
