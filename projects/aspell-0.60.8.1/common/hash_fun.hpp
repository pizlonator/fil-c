// Copyright (c) 2001
// Kevin Atkinson
//
// Permission to use, copy, modify, distribute and sell this software
// and its documentation for any purpose is hereby granted without
// fee, provided that the above copyright notice appear in all copies
// and that both that copyright notice and this permission notice
// appear in supporting documentation. Kevin Atkinson makes no
// representations about the suitability of this software for any
// purpose.  It is provided "as is" without express or implied
// warranty.

#ifndef acommon_hash_fun__hpp
#define acommon_hash_fun__hpp

namespace acommon {
  
  template <typename K> struct hash {};

  template <> struct hash<char>  {unsigned long operator()(char  v) const {return v;}};
  template <> struct hash<short> {unsigned long operator()(short v) const {return v;}};
  template <> struct hash<int>   {unsigned long operator()(int   v) const {return v;}};
  template <> struct hash<long>  {unsigned long operator()(long  v) const {return v;}};
  template <> struct hash<unsigned char>  {unsigned long operator()(unsigned char  v) const {return v;}};
  template <> struct hash<unsigned short> {unsigned long operator()(unsigned short v) const {return v;}};
  template <> struct hash<unsigned int>   {unsigned long operator()(unsigned int   v) const {return v;}};
  template <> struct hash<unsigned long>  {unsigned long operator()(unsigned long  v) const {return v;}};

  template <> struct hash<const char *> {
    inline unsigned long operator() (const char * s) const {
      unsigned long h = 0;
      for (; *s; ++s)
	h=5*h + *s;
      return h;
    }
  };

  template<class Str>
  struct HashString {
    inline unsigned long operator() (const Str &str) const {
      unsigned long h = 0;
      typename Str::const_iterator end = str.end();
      for (typename Str::const_iterator s = str.begin(); 
	   s != end; 
	   ++s)
	h=5*h + *s;
      return h;
    }
  };
  
}

#endif
