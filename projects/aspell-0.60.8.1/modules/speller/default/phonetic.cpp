// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include "language.hpp"
#include "phonetic.hpp"
#include "phonet.hpp"

#include "file_util.hpp"
#include "file_data_util.hpp"
#include "clone_ptr-t.hpp"

namespace aspeller {
  
  class SimpileSoundslike : public Soundslike {
  private:
    const Language * lang;
    char first[256];
    char rest[256];
  public:
    SimpileSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup(Conv &) {
      memcpy(first, lang->sl_first_, 256);
      memcpy(rest,  lang->sl_rest_, 256);
      return no_err;
    }
    
    String soundslike_chars() const {
      bool chars_set[256] = {0};
      for (int i = 0; i != 256; ++i) 
      {
        char c = first[i];
        if (c) chars_set[static_cast<unsigned char>(c)] = true;
        c = rest[i];
        if (c) chars_set[static_cast<unsigned char>(c)] = true;
      }
      String     chars_list;
      for (int i = 0; i != 256; ++i) 
      {
        if (chars_set[i]) 
          chars_list += static_cast<char>(i);
      }
      return chars_list;
    }
    
    char * to_soundslike(char * res, const char * str, int size) const 
    {
      char prev, cur = '\0';

      const char * i = str;
      while (*i) {
        cur = first[static_cast<unsigned char>(*i++)];
        if (cur) {*res++ = cur; break;}
      }
      prev = cur;
      
      while (*i) {
	cur = rest[static_cast<unsigned char>(*i++)];
	if (cur && cur != prev) *res++ = cur;
	prev = cur;
      }
      *res = '\0';
      return res;
    }

    const char * name () const {
      return "simple";
    }
    const char * version() const {
      return "2.0";
    }
  };

  class NoSoundslike : public Soundslike {
  private:
    const Language * lang;
  public:
    NoSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup(Conv &) {return no_err;}
    
    String soundslike_chars() const {
      return get_clean_chars(*lang);
    }

    char * to_soundslike(char * res, const char * str, int size) const 
    {
      return lang->LangImpl::to_clean(res, str);
    }

    const char * name() const {
      return "none";
    }
    const char * version() const {
      return "1.0";
    }
  };

  class StrippedSoundslike : public Soundslike {
  private:
    const Language * lang;
  public:
    StrippedSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup(Conv &) {return no_err;}
    
    String soundslike_chars() const {
      return get_stripped_chars(*lang);
    }

    char * to_soundslike(char * res, const char * str, int size) const 
    {
      return lang->LangImpl::to_stripped(res, str);
    }

    const char * name() const {
      return "stripped";
    }
    const char * version() const {
      return "1.0";
    }
  };

  class PhonetSoundslike : public Soundslike {

    const Language * lang;
    StackPtr<PhonetParms> phonet_parms;
    
  public:

    PhonetSoundslike(const Language * l) : lang(l) {}

    PosibErr<void> setup(Conv & iconv) {
      String file;
      file += lang->data_dir();
      file += '/';
      file += lang->name();
      file += "_phonet.dat";
      PosibErr<PhonetParms *> pe = new_phonet(file, iconv, lang);
      if (pe.has_err()) return pe;
      phonet_parms.reset(pe);
      return no_err;
    }


    String soundslike_chars() const 
    {
      bool chars_set[256] = {0};
      String     chars_list;
      for (const char * * i = phonet_parms->rules + 1; 
	   *(i-1) != PhonetParms::rules_end;
	   i += 2) 
      {
        for (const char * j = *i; *j; ++j) 
        {
          chars_set[static_cast<unsigned char>(*j)] = true;
        }
      }
      for (int i = 0; i != 256; ++i) 
      {
        if (chars_set[i]) 
          chars_list += static_cast<char>(i);
      }
      return chars_list;
    }
    
    char * to_soundslike(char * res, const char * str, int size) const 
    {
      int new_size = phonet(str, res, size, *phonet_parms);
      return res + new_size;
    }
    
    const char * name() const
    {
      return "phonet";
    }
    const char * version() const 
    {
      return phonet_parms->version.c_str();
    }
  };
  
  
  PosibErr<Soundslike *> new_soundslike(ParmString name, 
                                        Conv & iconv,
                                        const Language * lang)
  {
    Soundslike * sl;
    if (name == "simple" || name == "generic") {
      sl = new SimpileSoundslike(lang);
    } else if (name == "stripped") {
      sl = new StrippedSoundslike(lang);
    } else if (name == "none") {
      sl = new NoSoundslike(lang);
    } else if (name == lang->name()) {
      sl = new PhonetSoundslike(lang);
    } else {
      abort(); // FIXME
    }
    PosibErrBase pe = sl->setup(iconv);
    if (pe.has_err()) {
      delete sl;
      return pe;
    } else {
      return sl;
    }
  }
}

