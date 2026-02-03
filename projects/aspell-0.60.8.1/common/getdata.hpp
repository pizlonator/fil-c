// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#ifndef ASPELL_GET_DATA__HPP
#define ASPELL_GET_DATA__HPP

#include <stddef.h>
#include <limits.h>
#include "string.hpp"
#include "mutable_string.hpp"

namespace acommon {

  class IStream;
  class String;

  // NOTE: getdata_pair WILL NOT unescape a string

  struct DataPair {
    MutableString key;
    MutableString value;
    size_t line_num;
    DataPair() : line_num(0) {}
  };

  bool getdata_pair(IStream & in, DataPair & d, String & buf);

  char * unescape(char * dest, const char * src);
  static inline char * unescape(char * dest) {return unescape(dest, dest);}
  static inline void unescape(MutableString & d) {
    char * e = unescape(d.str, d.str);
    d.size = e - d.str;
  }
  static inline void unescape(String & d) {
    d.ensure_null_end();
    char * e = unescape(d.data(), d.data());
    d.resize(e - d.data());
  }

  // if limit is not given than dest should have enough space for 
  // twice the number of characters of src
  bool escape(char * dest, const char * src, 
	      size_t limit = INT_MAX, const char * others = 0);

  void to_lower(char *);
  void to_lower(String &, const char *);

  // extract the first whitespace delimited word from d.value and put
  // it in d.key.  d.value is expected not to have any leading
  // whitespace. The string is modified in place so that d.key will be
  // null terminated.  d.value is modified to point to any additional
  // data after key with any leading white space removed.  Returns
  // false when there are no more words to extract
  bool split(DataPair & d);

  // preps a string for split
  void init(ParmString str, DataPair & d, String & buf);

  bool getline(IStream & in, DataPair & d, String & buf);

  char * get_nb_line(IStream & in, String & buf);
  void remove_comments(String & buf);

}
#endif
