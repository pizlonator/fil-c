// This file is part of The New Aspell
// Copyright (C) 2002,2011 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include <assert.h>

#include "checker_string.hpp"
#include "speller.hpp"
#include "document_checker.hpp"
#include "asc_ctype.hpp"
#include "convert.hpp"

extern Conv dconv;
extern Conv uiconv;

static int get_line(FILE * in, CheckerString::Line & d)
{
  d.clear();
  int i;
  while ((i = getc(in)), i != EOF)
  {
    d.real.push_back(static_cast<char>(i));
    if (i == '\n') break;
  }
  if (dconv.conv) {
    dconv.conv->convert(d.real.str(), d.real.size(), d.buf, dconv.buf0);
    d.disp.str = d.buf.str();
    d.disp.size = d.buf.size();
  } else {
    d.disp.str = d.real.str();
    d.disp.size = d.real.size();
  }
  return d.real.size();
}

CheckerString::CheckerString(AspellSpeller * speller, 
                             FILE * in, FILE * out, 
                             int num_lines)
  : in_(in), out_(out), speller_(speller)
{
  lines_.reserve(num_lines + 1);
  for (; num_lines > 0; --num_lines)
  {
    lines_.resize(lines_.size() + 1);
    int s = get_line(in_, lines_.back());
    if (s == 0) break;
  }
  if (lines_.back().real.size() != 0)
    lines_.resize(lines_.size() + 1);

  end_ = lines_.end() - 1;
  cur_line_ = lines_.begin();
  diff_ = 0;
  has_repl_ = false;

  checker_.reset(new_document_checker(reinterpret_cast<Speller *>(speller)));
  checker_->process(cur_line_->real.data(), cur_line_->real.size());
}

void CheckerString::abort() {
  fclose(in_);
  in_ = 0;
  if (out_) {
    fclose(out_);
    out_ = 0;
  }
}

CheckerString::~CheckerString()
{
  if (!in_ && !out_)
    return;
  if (out_)
    for (cur_line_ = first_line(); !off_end(cur_line_); next_line(cur_line_))
    {
      fwrite(cur_line_->real.data(), cur_line_->real.size(), 1, out_);
      cur_line_->clear();
    }
  if (in_ != stdin)
    fclose(in_);
  if (out_ && out_ != stdout && out_ != stdout)
    fclose(out_);
}

bool CheckerString::read_next_line()
{
  if (feof(in_)) return false;
  Lines::iterator next = end_;
  inc(next);
  if (next == cur_line_) return false;
  int s = get_line(in_, *end_);
  if (s == 0) return false;
  end_ = next;
  if (out_ && end_->real.size() > 0)
    fwrite(end_->real.data(), end_->real.size(), 1, out_);
  end_->clear();
  return true;
}

bool CheckerString::next_misspelling()
{
  if (off_end(cur_line_)) return false;
  if (has_repl_) {
    has_repl_ = false;
    CharVector word;
    bool correct = false;
    // FIXME: This is a hack to avoid trying to check a word with a space
    //        in it.  The correct action is to reparse to string and
    //        check each word individually.  However doing so involves
    //        an API enhancement in Checker.
    for (int i = 0; i != real_word_size_; ++i) {
      if (asc_isspace(*(real_word_begin_ + i)))
        correct = true;
    }
    if (!correct)
      correct = aspell_speller_check(speller_, &*real_word_begin_, real_word_size_);
    diff_ += real_word_size_ - tok_.len;
    tok_.len = real_word_size_;
    if (!correct)
      return true;
  }
  while ((tok_ = checker_->next_misspelling()).len == 0) {
    next_line(cur_line_);
    diff_ = 0;
    if (off_end(cur_line_)) return false;
    checker_->process(cur_line_->real.data(), cur_line_->real.size());
  }
  real_word_begin_ = cur_line_->real.begin() + tok_.offset + diff_;
  real_word_size_  = tok_.len;
  fix_display_str();
  return true;
}

void CheckerString::replace(ParmString repl)
{
  assert(real_word_size_ > 0);
  int offset = real_word_begin_ - cur_line_->real.begin();
  aspell_speller_store_replacement(speller_, &*real_word_begin_, real_word_size_,
                                   repl.str(), repl.size());
  cur_line_->real.replace(real_word_begin_, real_word_begin_ + real_word_size_,
                          repl.str(), repl.str() + repl.size());
  real_word_begin_ = cur_line_->real.begin() + offset;
  real_word_size_ = repl.size();
  fix_display_str();
  has_repl_ = true;
}

void CheckerString::fix_display_str()
{
  if (dconv.conv) {
    cur_line_->buf.clear();
    int s = real_word_begin_ - cur_line_->real.begin();
    if (s > 0) dconv.conv->convert(cur_line_->real.data(), s, 
                                   cur_line_->buf, dconv.buf0);
    int off = cur_line_->buf.size();
    dconv.conv->convert(real_word_begin_, real_word_size_, 
                        cur_line_->buf, dconv.buf0);
    disp_word_size_ = cur_line_->buf.size() - off;
    s = cur_line_->real.end() - (real_word_begin_ + real_word_size_);
    if (s > 0) dconv.conv->convert(cur_line_->real.data() + (cur_line_->real.size() - s), 
                                  s, cur_line_->buf, dconv.buf0);
    cur_line_->disp.str = cur_line_->buf.str();
    cur_line_->disp.size = cur_line_->buf.size();
    disp_word_begin_ = cur_line_->buf.data() + off;
  } else {
    cur_line_->disp.str  = cur_line_->real.str();
    cur_line_->disp.size = cur_line_->real.size();
    disp_word_size_  = real_word_size_;
    disp_word_begin_ = real_word_begin_;
  }
}

