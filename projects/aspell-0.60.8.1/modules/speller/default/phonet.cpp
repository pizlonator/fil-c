/*  phonetic.c - generic replacement aglogithms for phonetic transformation
    Copyright (C) 2000 Björn Jacke

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation;
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Björn Jacke may be reached by email at bjoern.jacke@gmx.de

    Changelog:

    2000-01-05  Björn Jacke <bjoern.jacke@gmx.de>
                Initial Release insprired by the article about phonetic
                transformations out of c't 25/1999

*/

#include <string.h>
#include <assert.h>

#include <vector>

#include "asc_ctype.hpp"
#include "string.hpp"
#include "phonet.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "language.hpp"
#include "objstack.hpp"
#include "vararray.hpp"

using namespace acommon;

namespace aspeller {

  const char * const PhonetParms::rules_end = "";
  
  static bool to_bool(const String & str) {
    if (str == "1" || str == "true") return true;
    else return false;
  }
#if 0
  void dump_phonet_rules(ostream & out, const PhonetParms & parms) {
    out << "version         " << parms.version << "\n";
    out << "followup        " << parms.followup << "\n";
    out << "collapse_result " << parms.collapse_result << "\n";
    out << "\n";
    ios::fmtflags flags = out.setf(ios::left);
    for (int i = 0; parms.rules[i] != PhonetParms::rules_end; i += 2) {
      out << setw(20) << parms.rules[i] << " " 
	  << (parms.rules[i+1][0] == '\0' ? "_" : parms.rules[i+1])
	  << "\n";
    }
    out.flags(flags);
  }
#endif

  struct PhonetParmsImpl : public PhonetParms {
    void * data;
    ObjStack strings;
    PhonetParmsImpl() : data(0) {}
    ~PhonetParmsImpl() {if (data) free(data);}
  };

  static void init_phonet_hash(PhonetParms & parms);

  // like strcpy but safe if the strings overlap
  //   but only if dest < src
  static inline void strmove(char * dest, char * src) {
    while (*src) 
      *dest++ = *src++;
    *dest = '\0';
  }
  
  PosibErr<PhonetParms *> new_phonet(const String & file, 
                                     Conv & iconv,
                                     const Language * lang) 
  {
    String buf; DataPair dp;

    FStream in;
    RET_ON_ERR(in.open(file, "r"));

    PhonetParmsImpl * parms = new PhonetParmsImpl();

    parms->lang = lang;

    parms->followup        = true;
    parms->collapse_result = false;
    parms->remove_accents  = true;

    int num = 0;
    while (getdata_pair(in, dp, buf)) {
      if (dp.key != "followup" && dp.key != "collapse_result" &&
	  dp.key != "version")
	++num;
    }

    in.restart();

    size_t vsize = sizeof(char *) * (2 * num + 2);
    parms->data = malloc(vsize);

    const char * * r = (const char * *)parms->data;

    char * empty_str = parms->strings.dup("");

    while (true) {
      if (!getdata_pair(in, dp, buf)) break;
      if (dp.key == "followup") {
	parms->followup = to_bool(dp.value);
      } else if (dp.key == "collapse_result") {
	parms->collapse_result = to_bool(dp.value);
      } else if (dp.key == "version") {
	parms->version = dp.value;
      } else if (dp.key == "remove_accents") {
        parms->remove_accents = to_bool(dp.value);
      } else {
	*r = parms->strings.dup(iconv(dp.key));
	++r;
	if (dp.value == "_") {
	  *r = empty_str;
	} else {
	  *r = parms->strings.dup(iconv(dp.value));
	}
	++r;
      }
    }
    if (parms->version.empty()) {
      delete parms;
      return make_err(bad_file_format, file, "You must specify a version string");
    }
    *(r  ) = PhonetParms::rules_end;
    *(r+1) = PhonetParms::rules_end;
    parms->rules = (const char * *)parms->data;


    for (unsigned i = 0; i != 256; ++i) {
      parms->to_clean[i] = (lang->char_type(i) > Language::NonLetter 
                            ? (parms->remove_accents 
                               ? lang->to_upper(lang->de_accent(i)) 
                               : lang->to_upper(i))
                            : 0);
    }

    init_phonet_hash(*parms);

    return parms;
  }

  static void init_phonet_hash(PhonetParms & parms) 
  {
    int i, k;

    for (i = 0; i < parms.hash_size; i++) {
      parms.hash[i] = -1;
    }

    for (i = 0; parms.rules[i] != PhonetParms::rules_end; i += 2) {
      /**  set hash value  **/
      k = (unsigned char) parms.rules[i][0];

      if (parms.hash[k] < 0) {
	parms.hash[k] = i;
      }
    }
  }


#ifdef PHONET_TRACE
  void trace_info(char * text, int n, char * error,
		  const PhonetParms & parms) 
  {
    /**  dump tracing info  **/
    
    printf ("%s %d:  \"%s\"  >  \"%s\" %s", text, ((n/2)+1), parms.rules[n],
	    parms.rules[n+1], error);
  }
#endif

  int phonet (const char * inword, char * target,
              int len,
	      const PhonetParms & parms)
  {
    /**       Do phonetic transformation.       **/
    /**  "len" = length of "inword" incl. '\0'. **/

    /**  result:  >= 0:  length of "target"    **/
    /**            otherwise:  error            **/

    int  i,j,k=0,n,p,z;
    int  k0,n0,p0=-333,z0;
    if (len == -1) len = strlen(inword);
    VARARRAY(char, word, len + 1);
    char c, c0;
    const char * s;

    typedef unsigned char uchar;
    
    /**  to convert string to uppercase and possible remove accents **/
    char * res = word;
    for (const char * str = inword; *str; ++str) {
      char c = parms.to_clean[(uchar)*str];
      if (c) *res++ = c;
    }
    *res = '\0';
    
    /**  check word  **/
    i = j = z = 0;
    while ((c = word[i]) != '\0') {
      #ifdef PHONET_TRACE
         cout << "\nChecking position " << j << ":  word = \""
              << word+i << "\",";
         printf ("  target = \"%.*s\"", j, target);
      #endif
      n = parms.hash[(uchar) c];
      z0 = 0;

      if (n >= 0) {
        /**  check all rules for the same letter  **/
        while (parms.rules[n][0] == c) {
          #ifdef PHONET_TRACE
             trace_info ("\n> Checking rule No.",n,"",parms);
          #endif

          /**  check whole string  **/
          k = 1;   /** number of found letters  **/
          p = 5;   /** default priority  **/
          s = parms.rules[n];
          s++;     /**  important for (see below)  "*(s-1)"  **/
          
          while (*s != '\0'  &&  word[i+k] == *s
                 &&  !asc_isdigit (*s)  &&  strchr ("(-<^$", *s) == NULL) {
            k++;
            s++;
          }
          if (*s == '(') {
            /**  check letters in "(..)"  **/
            if (parms.lang->is_alpha(word[i+k])  // ...could be implied?
                && strchr(s+1, word[i+k]) != NULL) {
              k++;
              while (*s != ')')
                s++;
              s++;
            }
          }
          p0 = (int) *s;
          k0 = k;
          while (*s == '-'  &&  k > 1) {
            k--;
            s++;
          }
          if (*s == '<')
            s++;
          if (asc_isdigit (*s)) {
            /**  determine priority  **/
            p = *s - '0';
            s++;
          }
          if (*s == '^'  &&  *(s+1) == '^')
            s++;

          if (*s == '\0'
              || (*s == '^'  
                  && (i == 0  ||  ! parms.lang->is_alpha(word[i-1]))
                  && (*(s+1) != '$'
                      || (! parms.lang->is_alpha(word[i+k0]) )))
              || (*s == '$'  &&  i > 0  
                  &&  parms.lang->is_alpha(word[i-1])
                  && (! parms.lang->is_alpha(word[i+k0]) ))) 
          {
            /**  search for followup rules, if:     **/
            /**  parms.followup and k > 1  and  NO '-' in searchstring **/
            c0 = word[i+k-1];
            n0 = parms.hash[(uchar) c0];
//
            if (parms.followup  &&  k > 1  &&  n0 >= 0
                &&  p0 != (int) '-'  &&  word[i+k] != '\0') {
              /**  test follow-up rule for "word[i+k]"  **/
              while (parms.rules[n0][0] == c0) {
                #ifdef PHONET_TRACE
                    trace_info ("\n> > follow-up rule No.",n0,"... ",parms);
                #endif

                /**  check whole string  **/
                k0 = k;
                p0 = 5;
                s = parms.rules[n0];
                s++;
                while (*s != '\0'  &&  word[i+k0] == *s
                       && ! asc_isdigit(*s)  &&  strchr("(-<^$",*s) == NULL) {
                  k0++;
                  s++;
                }
                if (*s == '(') {
                  /**  check letters  **/
                  if (parms.lang->is_alpha(word[i+k0])
                      &&  strchr (s+1, word[i+k0]) != NULL) {
                    k0++;
                    while (*s != ')'  &&  *s != '\0')
                      s++;
                    if (*s == ')')
                      s++;
                  }
                }
                while (*s == '-') {
                  /**  "k0" gets NOT reduced   **/
                  /**  because "if (k0 == k)"  **/
                  s++;
                }
                if (*s == '<')
                  s++;
                if (asc_isdigit (*s)) {
                  p0 = *s - '0';
                  s++;
                }

                if (*s == '\0'
                    /**  *s == '^' cuts  **/
                    || (*s == '$'  &&  ! parms.lang->is_alpha(word[i+k0]))) 
                {
                  if (k0 == k) {
                    /**  this is just a piece of the string  **/
                    #ifdef PHONET_TRACE
                        cout << "discarded (too short)";
                    #endif
                    n0 += 2;
                    continue;
                  }

                  if (p0 < p) {
                    /**  priority too low  **/
                    #ifdef PHONET_TRACE
                        cout << "discarded (priority)";
                    #endif
                    n0 += 2;
                    continue;
                  }
                  /**  rule fits; stop search  **/
                  break;
                }
                #ifdef PHONET_TRACE
                    cout << "discarded";
                #endif
                n0 += 2;
              } /**  End of "while (parms.rules[n0][0] == c0)"  **/

              if (p0 >= p  && parms.rules[n0][0] == c0) {
                #ifdef PHONET_TRACE
                    trace_info ("\n> Rule No.", n,"",parms);
                    trace_info ("\n> not used because of follow-up",
                                      n0,"",parms);
                #endif
                n += 2;
                continue;
              }
            } /** end of follow-up stuff **/

            /**  replace string  **/
            #ifdef PHONET_TRACE
                trace_info ("\nUsing rule No.", n,"\n",parms);
            #endif
            s = parms.rules[n+1];
            p0 = (parms.rules[n][0] != '\0'
                 &&  strchr (parms.rules[n]+1,'<') != NULL) ? 1:0;
            if (p0 == 1 &&  z == 0) {
              /**  rule with '<' is used  **/
              if (j > 0  &&  *s != '\0'
                 && (target[j-1] == c  ||  target[j-1] == *s)) {
                j--;
              }
              z0 = 1;
              z = 1;
              k0 = 0;
              while (*s != '\0'  &&  word[i+k0] != '\0') {
                word[i+k0] = *s;
                k0++;
                s++;
              }
              if (k > k0)
                strmove (&word[0]+i+k0, &word[0]+i+k);

              /**  new "actual letter"  **/
              c = word[i];
            }
            else { /** no '<' rule used **/
              i += k - 1;
              z = 0;
              while (*s != '\0'
                     &&  *(s+1) != '\0'  &&  j < len) {
                if (j == 0  ||  target[j-1] != *s) {
                  target[j] = *s;
                  j++;
                }
                s++;
              }
              /**  new "actual letter"  **/
              c = *s;
              if (parms.rules[n][0] != '\0'
                 &&  strstr (parms.rules[n]+1, "^^") != NULL) {
                if (c != '\0') {
                  target[j] = c;
                  j++;
                }
                strmove (&word[0], &word[0]+i+1);
                i = 0;
                z0 = 1;
              }
            }
            break;
          }  /** end of follow-up stuff **/
          n += 2;
        } /**  end of while (parms.rules[n][0] == c)  **/
      } /**  end of if (n >= 0)  **/
      if (z0 == 0) {
        if (k && (assert(p0!=-333),!p0) &&  j < len &&  c != '\0'
           && (!parms.collapse_result  ||  j == 0  ||  target[j-1] != c)){
           /**  condense only double letters  **/
          target[j] = c;
	  ///printf("\n setting \n");
          j++;
        }
        #ifdef PHONET_TRACE
        else if (p0 || !k)
          cout << "\nNo rule found; character \"" << word[i] << "\" skipped\n";
        #endif

        i++;
        z = 0;
	k=0;
      }
    }  /**  end of   while ((c = word[i]) != '\0')  **/

    target[j] = '\0';
    return (j);

  }  /**  end of function "phonet"  **/
}

#if 0

int main (int argc, char *argv[]) {
  using namespace autil;

  if (argc < 3) {
    printf ("Usage:  phonet <data file> <word>\n");
    return(1);
  }

  char phone_word[strlen(argv[2])+1]; /**  max possible length of words  **/

  PhonetParms * parms;
  ifstream f(argv[1]);
  parms = load_phonet_rules(f);

  init_phonet_charinfo(*parms);
  init_phonet_hash(*parms);
  phonet (argv[2],phone_word,*parms);
  printf ("%s\n", phone_word);
  return(0);
}
#endif
