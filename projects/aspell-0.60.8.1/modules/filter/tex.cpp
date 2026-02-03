// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "asc_ctype.hpp"
#include "config.hpp"
#include "string.hpp"
#include "indiv_filter.hpp"
#include "mutable_container.hpp"
#include "string_map.hpp"
#include "clone_ptr-t.hpp"
#include "vector.hpp"
#include "errors.hpp"
#include "convert.hpp"

//#define FILTER_PROGRESS_CONTROL "tex-filter-debug.log"
//#include "filter_debug.hpp"
//#include <stdio.h>
//#include <cstdio>
#include "filter_char_vector.hpp"
#include "string_list.hpp"
#include "string_enumeration.hpp"

#include "gettext.h"

namespace {

  using namespace acommon;

  class TexFilter : public IndividualFilter 
  {
  private:
    enum InWhat {Name, Opt, Parm, Other, Swallow};
    struct Command {
      InWhat in_what;
      String name;
      const char * do_check;
      Command() {}
      Command(InWhat w) : in_what(w), do_check("P") {}
    };

    bool in_comment;
    bool prev_backslash;
    Vector<Command> stack;

    class Commands : public StringMap {
    public:
      PosibErr<bool> add(ParmStr to_add);
      PosibErr<bool> remove(ParmStr to_rem);
    };
    
    Commands commands;
    bool check_comments;
    
    inline void push_command(InWhat);
    inline void pop_command();

    bool end_option(char u, char l);

    inline bool process_char(FilterChar::Chr c);
    
  public:
    PosibErr<bool> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  //
  //
  //

  inline void TexFilter::push_command(InWhat w) {
    stack.push_back(Command(w));
  }

  inline void TexFilter::pop_command() {
    stack.pop_back();
    if (stack.empty())
      push_command(Parm);
  }

  //
  //
  //

  PosibErr<bool> TexFilter::setup(Config * opts) 
  {
    name_ = "tex-filter";
    order_num_ = 0.35;
    //fprintf(stderr,"name %s \n",name_);

    commands.clear();
    opts->retrieve_list("f-tex-command", &commands);
    
    check_comments = opts->retrieve_bool("f-tex-check-comments");

    reset();
    return true;
  }
  
  void TexFilter::reset() 
  {
    in_comment = false;
    prev_backslash = false;
    stack.resize(0);
    push_command(Parm);
  }

#  define top stack.back()

  // yes this should be inlined, it is only called once
  inline bool TexFilter::process_char(FilterChar::Chr c) 
  {
    // deal with comments
    if (c == '%' && !prev_backslash) in_comment = true;
    if (in_comment && c == '\n')     in_comment = false;

    prev_backslash = false;

    if (in_comment)                  return !check_comments;

    if (top.in_what == Name) {
      if (asc_isalpha(c)) {

	top.name += c;
	return true;

      } else {

	if (top.name.empty() && (c == '@')) {
	  top.name += c;
	  return true;
	}
	  
	top.in_what = Other;

	if (top.name.empty()) {
	  top.name.clear();
	  top.name += c;
	  top.do_check = commands.lookup(top.name.c_str());
	  if (top.do_check == 0) top.do_check = "";
	  return !asc_isspace(c);
	}

	top.do_check = commands.lookup(top.name.c_str());
	if (top.do_check == 0) top.do_check = "";

	if (asc_isspace(c)) { // swallow extra spaces
	  top.in_what = Swallow;
	  return true;
	} else if (c == '*') { // ignore * at end of commands
	  return true;
	}
	
	// continue o...
      }

    } else if (top.in_what == Swallow) {

      if (asc_isspace(c))
	return true;
      else
	top.in_what = Other;
    }

    if (c == '{')
      while (*top.do_check == 'O' || *top.do_check == 'o') 
	++top.do_check;

    if (*top.do_check == '\0')
      pop_command();

    if (c == '{') {

      if (top.in_what == Parm || top.in_what == Opt || *top.do_check == '\0')
	push_command(Parm);

      top.in_what = Parm;
      return true;
    }

    if (top.in_what == Other) {

      if (c == '[') {

	top.in_what = Opt;
	return true;

      } else if (asc_isspace(c)) {

	return true;

      } else {
	
	pop_command();

      }

    } 

    if (c == '\\') {
      prev_backslash = true;
      push_command(Name);
      return true;
    }

    if (top.in_what == Parm) {

      if (c == '}')
	return end_option('P','p');
      else
	return *top.do_check == 'p';

    } else if (top.in_what == Opt) {

      if (c == ']')
	return end_option('O', 'o');
      else
	return *top.do_check == 'o';

    }

    return false;
  }

  void TexFilter::process(FilterChar * & str, FilterChar * & stop)
  {
    FilterChar * cur = str;

    while (cur != stop) {
      if (process_char(*cur))
	*cur = ' ';
      ++cur;
    }
  }

  bool TexFilter::end_option(char u, char l) {
    top.in_what = Other;
    if (*top.do_check == u || *top.do_check == l)
      ++top.do_check;
    return true;
  }

  //
  // TexFilter::Commands
  //

  PosibErr<bool> TexFilter::Commands::add(ParmStr value) {
    int p1 = 0;
    while (!asc_isspace(value[p1])) {
      if (value[p1] == '\0') 
	return make_err(bad_value, value,"",
                        _("a string of 'o','O','p',or 'P'"));
      ++p1;
    }
    int p2 = p1 + 1;
    while (asc_isspace(value[p2])) {
      if (value[p2] == '\0') 
	return make_err(bad_value, value,"",
                        _("a string of 'o','O','p',or 'P'"));
      ++p2;
    }
    String t1; t1.assign(value,p1);
    String t2; t2.assign(value+p2);
    return StringMap::replace(t1, t2);
  }
  
  PosibErr<bool> TexFilter::Commands::remove(ParmStr value) {
    int p1 = 0;
    while (!asc_isspace(value[p1]) && value[p1] != '\0') ++p1;
    String temp; temp.assign(value,p1);
    return StringMap::remove(temp);
  }
#if 0  
  //
  //
  class Recode {
    Vector<char *> code;
    int minlength;
  public:
    Recode();
    Recode(const Recode & recode) {*this=recode;}
    void reset();
    bool setExpansion(const char * expander);
    virtual int encode(FilterChar * & begin,FilterChar * & end,
                       FilterCharVector * external);
    virtual int decode(FilterChar * & begin,FilterChar * & end,
                       FilterCharVector * internal);
    bool operator == (char test) ;
    bool operator != (char test) ;
    Recode & operator = (const Recode & rec);
    virtual ~Recode();
  };

  Recode::Recode()
    : code(0)
  {  
    minlength=0;
    reset();
  }

  Recode & Recode::operator = (const Recode & rec) {
    reset();
    code.resize(rec.code.size());
    
    int countrec=0;
  
    for (countrec=0;
         countrec < code.size();
         countrec++) {
      code[countrec]=NULL;
      if (!rec.code[countrec]) {
        continue;
      }
      int length=strlen(rec.code[countrec]);

      code[countrec]= new char[length+1];
      strncpy(code[countrec],rec.code[countrec],length);
      code[countrec][length]='\0';
    }
    return *this;
  }
      

  bool Recode::operator == (char test) {
    return (code.size() && (code[0][0] == test));
  }

  bool Recode::operator != (char test) {
    return (!code.size() || (code[0][0] != test));
  }

  void Recode::reset() {

    unsigned int countcode=0;

    for (countcode=0;
         countcode < code.size();
         countcode++) {
      if (code[countcode]) {
        delete[] code[countcode];
      }
      code[countcode] = NULL;
    }
    code.resize(0);
    minlength=0;
  }

  bool Recode::setExpansion(const char * expander) {

    char * begin=(char *)expander;
    char * end=begin;
    char * start=begin;
    int keylen=0;


    if (code.size() > 0) {
      keylen=strlen(code[0]);
    }
    while (begin && end && *begin) {
      while (end && *end && (*end != ',')) {
        end++;
      }
      if (end  && (begin != end)) {
        
        char hold = *end;
  
        *end='\0';

        int length=strlen(begin);

        if ((begin == start) && (keylen || (length != 1))) {
          if ((length != 1) || (keylen != length) ||
              strncmp(code[0],begin,length)) {
            *end=hold;
            return false;
          }
          *end=hold;
          if (*end) {
            end++;
          }
          begin=end;
          continue;
        }
        else if (minlength < length) {
          minlength=0;
        }
  
        code.resize(code.size()+1);
        code[code.size()-1] = new char[length+1];
        strncpy(code[code.size()-1],begin,length);
        code[code.size()-1][length]='\0';
        *end=hold;
        if (*end) {
          end++;
        }
        begin=end;
      }
    }
    return true;
  }

  int Recode::encode(FilterChar * & begin,FilterChar * & end,
                     FilterCharVector * external) {
    if ((code.size() < 2) || 
        (begin == NULL) || 
        (begin == end) || 
        (begin->width < (unsigned int) minlength )) {
      return 0;
    }
    
    if (code[0][0] != (char)*begin) {
      return 0;
    }
    external->append(code[1],1);
    return strlen(code[0]);
  }

  int Recode::decode(FilterChar * & begin,FilterChar * & end,
                     FilterCharVector * internal) {
  
    FilterChar * i = begin;
    char * codepointer = NULL;
    char * codestart   = NULL; 
  
    if (code.size() < 2) {
      return 0;
    } 
    while((i != end) && 
          ((codepointer == NULL) || (*codepointer))) {
      if (codepointer == NULL) {

        int countcodes=0;

        for (countcodes=1;
             countcodes < (int) code.size();
             countcodes++) {
          if (*i == code[countcodes][0]) {
            codestart=codepointer=code[countcodes];
            break;
          }
        }
        if (codepointer == NULL) {
          return 0;
        }
      }
      if (*codepointer != *i) {
        return 0;
      }
      i++;
      codepointer++;
    }
    if ((codepointer == NULL) || (*codepointer)) {
      return 0;
    }

    int length=strlen(codestart);
    internal->append(code[0],length);
    return length;
  }
        
    
  Recode::~Recode() {
    reset();
  }

  class TexEncoder : public IndividualFilter {
    Vector<Recode> multibytes;
    FilterCharVector buf;
     
  public:
    TexEncoder();
    virtual PosibErr<bool> setup(Config * config); 
    virtual void process(FilterChar * & start, FilterChar * & stop);
    virtual void reset() ;
    virtual ~TexEncoder();
  };

  TexEncoder::TexEncoder()
    : multibytes(0),
      buf()
  {
  }

  PosibErr<bool> TexEncoder::setup(Config * config) {
    name_ = "tex-encoder";
    order_num_ = 0.40;

    StringList multibytechars;

    config->retrieve_list("f-tex-multi-byte", &multibytechars);

    Conv conv; // this a quick and dirty fix witch will only work for
               // iso-8859-1.  Full unicode support needs to be
               // implemented
    conv.setup(*config, "utf-8", "iso-8859-1", NormNone);

    StringEnumeration * multibytelist=multibytechars.elements();
    const char * multibyte0=NULL;
    const char * multibyte=NULL;

    while ((multibyte0=multibytelist->next())) {

      multibyte = conv(multibyte0);

      if (strlen(multibyte) < 3) {
        fprintf(stderr,"Filter: %s ignoring multi byte encoding `%s'\n",
                name_,multibyte);
        continue;
      }

      int countmulti=0;

      while ((countmulti < multibytes.size()) &&
             !multibytes[countmulti].setExpansion(multibyte)) {
        countmulti++;
      }
      if (countmulti >= multibytes.size()) {
        multibytes.resize(multibytes.size()+1);
        if (!multibytes[multibytes.size()-1].setExpansion(multibyte)) {
          fprintf(stderr,"Filter: %s ignoring multi byte encoding `%s'\n",name_,multibyte);
          continue;
        }
      }
    }
    return true;
  }

  void TexEncoder::process(FilterChar * & start, FilterChar * & stop) {
    buf.clear();

    FilterChar * i=start;

    while (i && (i != stop)) {

      FilterChar * old = i;
      int count=0;

      for (count=0;
           count < multibytes.size();
           count++) { 


        i+=multibytes[count].encode(i,stop,&buf);
      }
      if (i == old) {
        buf.append(*i);
        i++;
      }
    }
    buf.append('\0');
    start = buf.pbegin();
    stop  = buf.pend() - 1;
  }
    

  void TexEncoder::reset() {
    multibytes.resize(0);
    buf.clear();
  }

  TexEncoder::~TexEncoder(){
  }
        
    
  class TexDecoder : public IndividualFilter {
    Vector<Recode> multibytes;
    FilterCharVector buf;
    Vector<char *>hyphens;
     
  public:
    TexDecoder();
    virtual PosibErr<bool> setup(Config * config); 
    virtual void process(FilterChar * & start, FilterChar * & stop);
    virtual void reset() ;
    virtual ~TexDecoder();
  };

  TexDecoder::TexDecoder()
    : multibytes(0),
      buf(),
      hyphens()
  {
    FDEBUGNOTOPEN;
  }

  PosibErr<bool> TexDecoder::setup(Config * config) {
    name_ = "tex-decoder";
    order_num_ = 0.3;

    StringList multibytechars;
    
    config->retrieve_list("f-tex-multi-byte", &multibytechars);

    Conv conv; // this a quick and dirty fix witch will only work for
               // iso-8859-1.  Full unicode support needs to be
               // implemented
    conv.setup(*config, "utf-8", "iso-8859-1", NormNone);

    StringEnumeration * multibytelist=multibytechars.elements();
    const char * multibyte0=NULL;
    const char * multibyte=NULL;

    multibytes.resize(0);
    
    while ((multibyte0=multibytelist->next())) {

      multibyte = conv(multibyte0);
      
      if (strlen(multibyte) < 3) {
        fprintf(stderr,"Filter: %s ignoring multi byte encoding `%s'\n",
                name_,multibyte);
        continue;
      }
      FDEBUGPRINTF("next multibyte chars `");
      FDEBUGPRINTF(multibyte);
      FDEBUGPRINTF("'\n");

      int countmulti=0;

      while ((countmulti < multibytes.size()) &&
             !multibytes[countmulti].setExpansion(multibyte)) {
        countmulti++;
      }
      FDEBUGPRINTF("next multibyte chars `");
      FDEBUGPRINTF(multibyte);
      FDEBUGPRINTF("'\n");
      if (countmulti >= multibytes.size()) {
        multibytes.resize(multibytes.size()+1);
        if (!multibytes[multibytes.size()-1].setExpansion(multibyte)) {
          fprintf(stderr,"Filter: %s ignoring multi byte encoding `%s'\n",name_,multibyte);
          continue;
        }
      }
      FDEBUGPRINTF("next multibyte chars `");
      FDEBUGPRINTF(multibyte);
      FDEBUGPRINTF("'\n");
    }
    StringList hyphenChars;
    
    config->retrieve_list("f-tex-hyphen", &hyphenChars);

    StringEnumeration * hyphenList=hyphenChars.elements();
    const char * hyphen=NULL;

    hyphens.resize(0);
    while ((hyphen=hyphenList->next())) {
      FDEBUGPRINTF("next hyphen char `");
      FDEBUGPRINTF(hyphen);
      FDEBUGPRINTF("'\n");
      hyphens.push_back(strdup(hyphen));
    }
    return true;
  }

  void TexDecoder::process(FilterChar * & start, FilterChar * & stop) {
    buf.clear();

    FilterChar * i=start;

    FDEBUGPRINTF("filtin `");
    while (i && (i != stop)) {
      
      FilterChar * old = i;
      int count=0;
      
      for (count=0;
           count < multibytes.size();
           count++) { 
        
        FilterChar * j = i;
        
        i+=multibytes[count].decode(i,stop,&buf);

        while (j != i) {
          char jp[2]={'\0','\0'};

          jp[0]=*j;
          FDEBUGPRINTF(jp);
          j++;
        }
      }
      for(count=0;
          count < hyphens.size();
          count++) {
        if (!hyphens[count]) {
          continue;
        }
        char * hp = &hyphens[count][0];
        char * hpo = hp;
        FilterChar * j = i;
        while (*hp && (j != stop) &&
               (*hp == *j)) {
          hp++;
          j++;
        }
        if (!*hp) {
          FDEBUGPRINTF("{");
          FilterChar * k = i;
          while (k != j) {
            char kp[2]={'\0','\0'};

            kp[0]=*k;
            FDEBUGPRINTF(kp);
            k++;
          } 
          FDEBUGPRINTF("}");
          if (buf.size()) {
            buf[buf.size()-1].width+=strlen(hpo);
//          buf.append(*i,strlen(hpo)+1);
          }
          else {
//FIXME better solution for illegal hyphenation at begin of line
//      illegal as new line chars are whitespace for latex
            buf.append(*i,strlen(hpo));
            char ip[2]={'\0','\0'};
            ip[0]=*i;
            FDEBUGPRINTF(ip);
          }
          i=j;
        }
      }
      if (i == old) {
        char ip[2]={'\0','\0'};
        ip[0]=*i;
        FDEBUGPRINTF(ip);
        buf.append(*i);
        i++;
      }
    }
    buf.append('\0');
    start = buf.pbegin();
    stop  = buf.pend() - 1;
    FDEBUGPRINTF("'\nfiltout `");
    i = start;
    while (i != stop) {
      char ip[2]={'\0','\0'};
      ip[0]=*i;
      FDEBUGPRINTF(ip);
      i++;
    }
    FDEBUGPRINTF("'\n");
  }


  void TexDecoder::reset() {
    multibytes.resize(0);
    buf.clear();
  }

  TexDecoder::~TexDecoder(){
    FDEBUGCLOSE;
  }
#endif
}

C_EXPORT 
IndividualFilter * new_aspell_tex_filter() {return new TexFilter;}

#if 0
C_EXPORT 
IndividualFilter * new_aspell_tex_decoder() {return new TexDecoder;}
C_EXPORT 
IndividualFilter * new_aspell_tex_encoder() {return new TexEncoder;}
#endif
