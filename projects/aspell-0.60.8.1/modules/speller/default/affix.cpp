// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// This code is based on the the MySpell affix code:
//
/*
 * Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
 * Contributors.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All modifications to the source code must be clearly marked as
 *    such.  Binary redistributions based on modified source code
 *    must be clearly marked as modified versions in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY KEVIN B. HENDRICKS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * KEVIN B. HENDRICKS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <cstdlib>
#include <cstring>
#include <cstdio>

//#include "iostream.hpp"

#include "affix.hpp"
#include "errors.hpp"
#include "getdata.hpp"
#include "parm_string.hpp"
#include "check_list.hpp"
#include "speller_impl.hpp"
#include "vararray.hpp"
#include "lsort.hpp"
#include "hash-t.hpp"

#include "gettext.h"

using namespace std;

namespace aspeller {

typedef unsigned char byte;
static char EMPTY[1] = {0};

//////////////////////////////////////////////////////////////////////
//
// Entry struct definations
//

struct Conds
{
  char * str;
  unsigned num;
  char conds[SETSIZE];
  char get(byte i) const {return conds[i];}
};

struct AffEntry
{
  const char *   appnd;
  const char *   strip;
  byte           appndl;
  byte           stripl;
  byte           xpflg;
  char           achar;
  const Conds *  conds;
  //unsigned int numconds;
  //char         conds[SETSIZE];
};

// A Prefix Entry
  
struct PfxEntry : public AffEntry
{
  PfxEntry * next;
  PfxEntry * next_eq;
  PfxEntry * next_ne;
  PfxEntry * flag_next;
  PfxEntry() {}

  bool check(const LookupInfo &, const AffixMgr * pmyMgr,
             ParmString, CheckInfo &, GuessInfo *, bool cross = true) const;

  inline bool          allow_cross() const { return ((xpflg & XPRODUCT) != 0); }
  inline byte flag() const { return achar;  }
  inline const char *  key() const  { return appnd;  }
  bool applicable(SimpleString) const;
  SimpleString add(SimpleString, ObjStack & buf) const;
};

// A Suffix Entry

struct SfxEntry : public AffEntry
{
  const char * rappnd; // this is set in AffixMgr::build_sfxlist
  
  SfxEntry *   next;
  SfxEntry *   next_eq;
  SfxEntry *   next_ne;
  SfxEntry *   flag_next;

  SfxEntry() {}

  bool check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *,
             int optflags, AffEntry * ppfx);

  inline bool          allow_cross() const { return ((xpflg & XPRODUCT) != 0); }
  inline byte flag() const { return achar;  }
  inline const char *  key() const  { return rappnd; } 
  bool applicable(SimpleString) const;
  SimpleString add(SimpleString, ObjStack & buf, int limit, SimpleString) const;
};

//////////////////////////////////////////////////////////////////////
//
// Utility functions declarations
//

/* return 1 if s1 is subset of s2 */
static bool isSubset(const char * s1, const char * s2)
{
  while( *s1 && (*s1 == *s2) ) {
    s1++;
    s2++;
  }
  return (*s1 == '\0');
}

// return 1 if s1 (reversed) is a leading subset of end of s2
static bool isRevSubset(const char * s1, const char * end_of_s2, int len)
{
  while( (len > 0) && *s1 && (*s1 == *end_of_s2) ) {
    s1++;
    end_of_s2--;
    len --;
  }
  return (*s1 == '\0');
}

template <class T>
struct AffixLess
{
  bool operator() (T * x, T * y) const {return strcmp(x->key(),y->key()) < 0;}
};

// struct StringLookup {
//   struct Parms {
//     typedef const char * Value;
//     typedef const char * Key;
//     static const bool is_multi = false;
//     hash<const char *> hfun;
//     size_t hash(const char * s) {return hfun(s);}
//     bool equal(const char * x, const char * y) {return strcmp(x,y) == 0;}
//     const char * key(const char * c) {return c;}
//   };
//   typedef HashTable<Parms> Lookup;
//   Lookup lookup;
//   ObjStack * data_buf;
//   StringLookup(ObjStack * b) : data_buf(b) {}
//   const char * dup(const char * orig) {
//     pair<Lookup::iterator, bool> res = lookup.insert(orig);
//     if (res.second) *res.first = data_buf->dup(orig);
//     return *res.first;
//     //return data_buf->dup(orig);
//   }
// };

struct CondsLookupParms {
  typedef const Conds * Value;
  typedef const char * Key;
  static const bool is_multi = false;
  acommon::hash<const char *> hfun;
  size_t hash(const char * s) {return hfun(s);}
  bool equal(const char * x, const char * y) {return strcmp(x,y) == 0;}
  const char * key(const Conds * c) {return c->str;}
};

typedef HashTable<CondsLookupParms> CondsLookup;

// normalizes and checks the cond_str
// returns the length of the new string or -1 if invalid
static int normalize_cond_str(char * str)
{
  char * s = str;
  char * d = str;
  while (*s) {
    if (*s != '[') {
      *d++ = *s++;
    } else if (s[1] == '\0' || s[1] == ']') {
      return -1;
    } else if (s[2] == ']') {
      *d++ = s[1];
      s += 3;
    } else {
      *d++ = *s++;
      if (*s == '^') *d++ = *s++;
      while (*s != ']') {
        if (*s == '\0' || *s == '[') return -1;
        char * min = s;
        for (char * i = s + 1; *i != ']'; ++i) {
          if ((byte)*i < (byte)*min) min = i;}
        char c = *s;
        *d++ = *min;
        *min = c;
        ++s;
      }
      *d++ = *s++;
    }
  }
  *d = '\0';
  return d - str;
}

static void encodeit(CondsLookup &, ObjStack &, 
                     AffEntry * ptr, char * cs);

//////////////////////////////////////////////////////////////////////
//
// Affix Manager
//

PosibErr<void> AffixMgr::setup(ParmString affpath, Conv & iconv)
{
  // register hash manager and load affix data from aff file
  //cpdmin = 3;  // default value
  max_strip_ = 0;
  for (int i=0; i < SETSIZE; i++) {
    pStart[i] = NULL;
    sStart[i] = NULL;
    pFlag[i] = NULL;
    sFlag[i] = NULL;
    max_strip_f[i] = 0;
  }
  return parse_file(affpath, iconv);
}

AffixMgr::AffixMgr(const Language * l) 
  : lang(l), data_buf(1024*16) {}

AffixMgr::~AffixMgr() {}

static inline void max_(int & lhs, int rhs) 
{
  if (lhs < rhs) lhs = rhs;
}

// read in aff file and build up prefix and suffix entry objects 
PosibErr<void> AffixMgr::parse_file(const char * affpath, Conv & iconv)
{
  // io buffers
  String buf; DataPair dp;

  CondsLookup conds_lookup;
 
  // open the affix file
  affix_file = data_buf.dup(affpath);
  FStream afflst;
  RET_ON_ERR(afflst.open(affpath,"r"));

  // step one is to parse the affix file building up the internal
  // affix data structures

  // read in each line ignoring any that do not
  // start with a known line type indicator

  char prev_aff = '\0';

  while (getdata_pair(afflst,dp,buf)) {
    char affix_type = ' ';

    /* parse in the name of the character set used by the .dict and .aff */

    if (dp.key == "SET") {
      String buf;
      encoding = data_buf.dup(fix_encoding_str(dp.value, buf));
      if (strcmp(encoding, lang->data_encoding()) != 0)
        return make_err(incorrect_encoding, affix_file, lang->data_encoding(), encoding);
    }

    /* parse in the flag used by the controlled compound words */
    //else if (d.key == "COMPOUNDFLAG")
    //  compound = data_buf.dup(d.value);

    /* parse in the flag used by the controlled compound words */
    //else if (d.key == "COMPOUNDMIN")
    //  cpdmin = atoi(d.value); // FiXME

    //else if (dp.key == "TRY" || dp.key == "REP");

    else if (dp.key == "PFX" || dp.key == "SFX")
      affix_type = dp.key[0];

    if (affix_type == ' ') continue;

    //
    // parse this affix: P - prefix, S - suffix
    //

    int numents = 0;      // number of affentry structures to parse
    char achar='\0';      // affix char identifier
    short xpflg=0;
    AffEntry * nptr;
    {
      // split affix header line into pieces
      split(dp);
      if (dp.key.empty()) goto error;
      // key is affix char
      const char * astr = iconv(dp.key);
      if (astr[0] == '\0' || astr[1] != '\0') goto error;
      achar = astr[0];
      if (achar == prev_aff) goto error_count;
      prev_aff = achar;

      split(dp);
      if (dp.key.size != 1 || 
          !(dp.key[0] == 'Y' || dp.key[0] == 'N')) goto error;
      // key is cross product indicator 
      if (dp.key[0] == 'Y') xpflg = XPRODUCT;
    
      split(dp);
      if (dp.key.empty()) goto error;
      // key is number of affentries
      
      numents = atoi(dp.key); 
  
      for (int j = 0; j < numents; j++) {
        getdata_pair(afflst, dp, buf);

        if (affix_type == 'P') {
          nptr = (AffEntry *) data_buf.alloc_bottom(sizeof(PfxEntry));
          new (nptr) PfxEntry;
        } else {
          nptr = (AffEntry *) data_buf.alloc_bottom(sizeof(SfxEntry));
          new (nptr) SfxEntry;
        }

        nptr->xpflg = xpflg;

        split(dp);
        if (dp.key.empty()) goto error;
        // key is affix charter
        if (iconv(dp.key)[0] != achar) goto error_count;
        nptr->achar = achar;
 
        split(dp);
        if (dp.key.empty()) goto error;
        // key is strip 
        if (dp.key != "0") {
          ParmString s0(iconv(dp.key));
          max_(max_strip_, s0.size());
          max_(max_strip_f[(byte)achar], s0.size());
          nptr->strip = data_buf.dup(s0);
          nptr->stripl = s0.size();
        } else {
          nptr->strip  = "";
          nptr->stripl = 0;
        }
    
        split(dp);
        if (dp.key.empty()) goto error;
        // key is affix string or 0 for null
        if (dp.key != "0") {
          nptr->appnd  = data_buf.dup(iconv(dp.key));
          nptr->appndl = strlen(nptr->appnd);
        } else {
          nptr->appnd  = "";
          nptr->appndl = 0;
        }
    
        split(dp);
        if (dp.key.empty()) goto error;
        // key is the conditions descriptions
        char * cond = iconv(dp.key);
        int cond_len = normalize_cond_str(cond);
        if (cond_len < 0)
          return (make_err(invalid_cond, MsgConv(lang)(cond))
                  .with_file(affix_file, dp.line_num));
        if (nptr->stripl != 0) {
          char * cc = cond;
          if (affix_type == 'S') cc += cond_len - nptr->stripl;
          if (cond_len < nptr->stripl || 
              memcmp(cc, nptr->strip, nptr->stripl) != 0)
            return (make_err(invalid_cond_strip, 
                             MsgConv(lang)(cond), MsgConv(lang)(nptr->strip))
                    .with_file(affix_file, dp.line_num));
        }
        encodeit(conds_lookup, data_buf, nptr, cond);
    
        // now create SfxEntry or PfxEntry objects and use links to
        // build an ordered (sorted by affix string) list
        if (affix_type == 'P')
          build_pfxlist(static_cast<PfxEntry *>(nptr));
        else
          build_sfxlist(static_cast<SfxEntry *>(nptr)); 
      }
    }
    continue;
  error:
    return make_err(corrupt_affix, MsgConv(lang)(achar)).with_file(affix_file, dp.line_num);
  error_count:
    return make_err(corrupt_affix, MsgConv(lang)(achar), 
                    _("Possibly incorrect count.")).with_file(affix_file, dp.line_num);
  }
  afflst.close();

  // now we can speed up performance greatly taking advantage of the 
  // relationship between the affixes and the idea of "subsets".

  // View each prefix as a potential leading subset of another and view
  // each suffix (reversed) as a potential trailing subset of another.

  // To illustrate this relationship if we know the prefix "ab" is
  // found in the word to examine, only prefixes that "ab" is a
  // leading subset of need be examined.  Furthermore is "ab" is not
  // present then none of the prefixes that "ab" is is a subset need
  // be examined.

  // The same argument goes for suffix string that are reversed.

  // Then to top this off why not examine the first char of the word
  // to quickly limit the set of prefixes to examine (i.e. the
  // prefixes to examine must be leading supersets of the first
  // character of the word (if they exist)
 
  // To take advantage of this "subset" relationship, we need to add
  // two links from entry.  One to take next if the current prefix
  // is found (call it nexteq) and one to take next if the current
  // prefix is not found (call it nextne).

  // Since we have built ordered lists, all that remains is to
  // properly initialize the nextne and nexteq pointers that relate
  // them

  process_pfx_order();
  process_sfx_order();

  //CERR.printf("%u\n", data_buf.calc_size()/1024);

  return no_err;

}


// we want to be able to quickly access prefix information
// both by prefix flag, and sorted by prefix string itself
// so we need to set up two indexes

PosibErr<void> AffixMgr::build_pfxlist(PfxEntry* pfxptr)
{
  PfxEntry * ptr;
  PfxEntry * ep = pfxptr;

  // get the right starting point 
  const char * key = ep->key();
  const byte flg = ep->flag();

  // first index by flag which must exist
  ptr = pFlag[flg];
  ep->flag_next = ptr;
  pFlag[flg] = ep;

  // next insert the affix string, it will be sorted latter

  byte sp = *((const byte *)key);
  ptr = pStart[sp];
  ep->next = ptr;
  pStart[sp] = ep;
  return no_err;
}

// we want to be able to quickly access suffix information
// both by suffix flag, and sorted by the reverse of the
// suffix string itself; so we need to set up two indexes

PosibErr<void> AffixMgr::build_sfxlist(SfxEntry* sfxptr)
{
  SfxEntry * ptr;
  SfxEntry * ep = sfxptr;
  char * tmp = (char *)data_buf.alloc(sfxptr->appndl + 1);
  sfxptr->rappnd = tmp;

  // reverse the string
  char * dest = tmp + sfxptr->appndl;
  *dest-- = 0;
  const char * src = sfxptr->appnd;
  for (; dest >= tmp; --dest, ++src)
    *dest = *src;

  /* get the right starting point */
  const char * key = ep->key();
  const byte flg = ep->flag();

  // first index by flag which must exist
  ptr = sFlag[flg];
  ep->flag_next = ptr;
  sFlag[flg] = ep;

  // next insert the affix string, it will be sorted latter
    
  byte sp = *((const byte *)key);
  ptr = sStart[sp];
  ep->next = ptr;
  sStart[sp] = ep;
  return no_err;
}



// initialize the PfxEntry links NextEQ and NextNE to speed searching
PosibErr<void> AffixMgr::process_pfx_order()
{
  PfxEntry* ptr;

  // loop through each prefix list starting point
  for (int i=1; i < SETSIZE; i++) {

    ptr = pStart[i];

    if (ptr && ptr->next)
      ptr = pStart[i] = sort(ptr, AffixLess<PfxEntry>());

    // look through the remainder of the list
    //  and find next entry with affix that 
    // the current one is not a subset of
    // mark that as destination for NextNE
    // use next in list that you are a subset
    // of as NextEQ

    for (; ptr != NULL; ptr = ptr->next) {

      PfxEntry * nptr = ptr->next;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset( ptr->key() , nptr->key() )) break;
      }
      ptr->next_ne = nptr;
      ptr->next_eq = NULL;
      if ((ptr->next) && isSubset(ptr->key() , 
                                  (ptr->next)->key())) 
        ptr->next_eq = ptr->next;
    }

    // now clean up by adding smart search termination strings
    // if you are already a superset of the previous prefix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = pStart[i];
    for (; ptr != NULL; ptr = ptr->next) {
      PfxEntry * nptr = ptr->next;
      PfxEntry * mptr = NULL;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset(ptr->key(),nptr->key())) break;
        mptr = nptr;
      }
      if (mptr) mptr->next_ne = NULL;
    }
  }
  return no_err;
}



// initialize the SfxEntry links NextEQ and NextNE to speed searching
PosibErr<void> AffixMgr::process_sfx_order()
{
  SfxEntry* ptr;

  // loop through each prefix list starting point
  for (int i=1; i < SETSIZE; i++) {

    ptr = sStart[i];

    if (ptr && ptr->next)
      ptr = sStart[i] = sort(ptr, AffixLess<SfxEntry>());

    // look through the remainder of the list
    //  and find next entry with affix that 
    // the current one is not a subset of
    // mark that as destination for NextNE
    // use next in list that you are a subset
    // of as NextEQ

    for (; ptr != NULL; ptr = ptr->next) {
      SfxEntry * nptr = ptr->next;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset(ptr->key(),nptr->key())) break;
      }
      ptr->next_ne = nptr;
      ptr->next_eq = NULL;
      if ((ptr->next) && isSubset(ptr->key(),(ptr->next)->key())) 
        ptr->next_eq = ptr->next;
    }


    // now clean up by adding smart search termination strings:
    // if you are already a superset of the previous suffix
    // but not a subset of the next, search can end here
    // so set NextNE properly

    ptr = sStart[i];
    for (; ptr != NULL; ptr = ptr->next) {
      SfxEntry * nptr = ptr->next;
      SfxEntry * mptr = NULL;
      for (; nptr != NULL; nptr = nptr->next) {
        if (! isSubset(ptr->key(),nptr->key())) break;
        mptr = nptr;
      }
      if (mptr) mptr->next_ne = NULL;
    }
  }
  return no_err;
}

// takes aff file condition string and creates the
// conds array - please see the appendix at the end of the
// file affentry.cxx which describes what is going on here
// in much more detail

static void encodeit(CondsLookup & l, ObjStack & buf, 
                     AffEntry * ptr, char * cs)
{
  byte c;
  int i, j, k;

  // see if we already have this conds matrix

  CondsLookup::iterator itr = l.find(cs);
  if (!(itr == l.end())) {
    ptr->conds = *itr;
    return;
  }

  Conds * cds = (Conds *)buf.alloc_bottom(sizeof(Conds));
  cds->str = buf.dup(cs);
  l.insert(cds);
  ptr->conds = cds;

  int nc = strlen(cs);
  VARARRAYM(byte, mbr, nc + 1, MAXLNLEN);

  // now clear the conditions array
  memset(cds->conds, 0, sizeof(cds->conds));

  // now parse the string to create the conds array
  
  int neg = 0;   // complement indicator
  int grp = 0;   // group indicator
  int n = 0;     // number of conditions
  int ec = 0;    // end condition indicator
  int nm = 0;    // number of member in group

  // if no condition just return
  if (strcmp(cs,".")==0) {
    cds->num = 0;
    return;
  }

  i = 0;
  while (i < nc) {
    c = *((byte *)(cs + i));

    // start group indicator
    if (c == '[') {
      grp = 1;
      c = 0;
    }

    // complement flag
    if ((grp == 1) && (c == '^')) {
      neg = 1;
      c = 0;
    }

    // end goup indicator
    if (c == ']') {
      ec = 1;
      c = 0;
    }

    // add character of group to list
    if ((grp == 1) && (c != 0)) {
      *(mbr + nm) = c;
      nm++;
      c = 0;
    }

    // end of condition 
    if (c != 0) {
      ec = 1;
    }

    
    if (ec) {
      if (grp == 1) {
        if (neg == 0) {
          // set the proper bits in the condition array vals for those chars
          for (j=0;j<nm;j++) {
            k = (unsigned int) mbr[j];
            cds->conds[k] = cds->conds[k] | (1 << n);
          }
        } else {
          // complement so set all of them and then unset indicated ones
          for (j=0;j<SETSIZE;j++) cds->conds[j] = cds->conds[j] | (1 << n);
          for (j=0;j<nm;j++) {
            k = (unsigned int) mbr[j];
            cds->conds[k] = cds->conds[k] & ~(1 << n);
          }
        }
        neg = 0;
        grp = 0;   
        nm = 0;
      } else {
        // not a group so just set the proper bit for this char
        // but first handle special case of . inside condition
        if (c == '.') {
          // wild card character so set them all
          for (j=0;j<SETSIZE;j++) cds->conds[j] = cds->conds[j] | (1 << n);
        } else {  
          cds->conds[(unsigned int)c] = cds->conds[(unsigned int)c] | (1 << n);
        }
      }
      n++;
      ec = 0;
    }


    i++;
  }
  cds->num = n;
  return;
}


// check word for prefixes
bool AffixMgr::prefix_check (const LookupInfo & linf, ParmString word, 
                             CheckInfo & ci, GuessInfo * gi, bool cross) const
{
  if (word.empty()) return false;
 
  // first handle the special case of 0 length prefixes
  PfxEntry * pe = pStart[0];
  while (pe) {
    if (pe->check(linf,this,word,ci,gi)) return true;
    pe = pe->next;
  }
  
  // now handle the general case
  byte sp = *reinterpret_cast<const byte *>(word.str());
  PfxEntry * pptr = pStart[sp];

  while (pptr) {
    if (isSubset(pptr->key(),word)) {
      if (pptr->check(linf,this,word,ci,gi,cross)) return true;
      pptr = pptr->next_eq;
    } else {
      pptr = pptr->next_ne;
    }
  }
    
  return false;
}


// check word for suffixes
bool AffixMgr::suffix_check (const LookupInfo & linf, ParmString word, 
                             CheckInfo & ci, GuessInfo * gi,
                             int sfxopts, AffEntry * ppfx) const
{
  if (word.empty()) return false;

  // first handle the special case of 0 length suffixes
  SfxEntry * se = sStart[0];
  while (se) {
    if (se->check(linf, word, ci, gi, sfxopts, ppfx)) return true;
    se = se->next;
  }
  
  // now handle the general case
  byte sp = *((const byte *)(word + word.size() - 1));
  SfxEntry * sptr = sStart[sp];

  while (sptr) {
    if (isRevSubset(sptr->key(), word + word.size() - 1, word.size())) {
      if (sptr->check(linf, word, ci, gi, sfxopts, ppfx)) return true;
      sptr = sptr->next_eq;
    } else {
      sptr = sptr->next_ne;
    }
  }
    
  return false;
}

// check if word with affixes is correctly spelled
bool AffixMgr::affix_check(const LookupInfo & linf, ParmString word, 
                           CheckInfo & ci, GuessInfo * gi) const
{
  if (word.empty()) return false;

  // Deal With Case in a semi-intelligent manner
  CasePattern cp = lang->LangImpl::case_pattern(word);
  ParmString pword = word;
  ParmString sword = word;
  CharVector lower;
  if (cp == FirstUpper) {
    lower.append(word, word.size() + 1);
    lower[0] = lang->to_lower(word[0]);
    pword = ParmString(lower.data(), lower.size() - 1);
  } else if (cp == AllUpper) {
    lower.resize(word.size() + 1);
    unsigned int i = 0;
    for (; i != word.size(); ++i)
      lower[i] = lang->to_lower(word[i]);
    lower[i] = '\0';
    pword = ParmString(lower.data(), lower.size() - 1);
    sword = pword;
  }

  // check all prefixes (also crossed with suffixes if allowed) 
  if (prefix_check(linf, pword, ci, gi)) return true;

  // if still not found check all suffixes
  if (suffix_check(linf, sword, ci, gi, 0, NULL)) return true;

  // if still not found check again but with the lower case version
  // which can make a difference if the entire word matches the cond
  // string
  if (cp == FirstUpper) {
    return suffix_check(linf, pword, ci, gi, 0, NULL);
  } else {
    return false;
  }
}

void AffixMgr::munch(ParmString word, GuessInfo * gi, bool cross) const
{
  LookupInfo li(0, LookupInfo::AlwaysTrue);
  CheckInfo ci;
  gi->reset();
  CasePattern cp = lang->LangImpl::case_pattern(word);
  if (cp == AllUpper) return;
  if (cp != FirstUpper)
    prefix_check(li, word, ci, gi, cross);
  suffix_check(li, word, ci, gi, 0, NULL);
}

WordAff * AffixMgr::expand(ParmString word, ParmString aff, 
                           ObjStack & buf, int limit) const
{
  byte * empty = (byte *)buf.alloc(1);
  *empty = 0;

  byte * suf  = (byte *)buf.alloc(aff.size() + 1); 
  byte * suf_e = suf;
  byte * csuf = (byte *)buf.alloc(aff.size() + 1); 
  byte * csuf_e = csuf;

  WordAff * head = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
  WordAff * cur = head;
  cur->word = buf.dup(word);
  cur->aff  = suf;

  for (const byte * c = (const byte *)aff.str(), * end = c + aff.size();
       c != end; 
       ++c) 
  {
    if (sFlag[*c]) *suf_e++ = *c; 
    if (sFlag[*c] && sFlag[*c]->allow_cross()) *csuf_e++ = *c;
    
    for (PfxEntry * p = pFlag[*c]; p; p = p->flag_next) {
      SimpleString newword = p->add(word, buf);
      if (!newword) continue;
      cur->next = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
      cur = cur->next;
      cur->word = newword;
      cur->aff = p->allow_cross() ? csuf : empty;
    }
  }

  *suf_e = 0;
  *csuf_e = 0;
  cur->next = 0;

  if (limit == 0) return head;

  WordAff * * end = &cur->next;
  WordAff * * very_end = end;
  size_t nsuf_s = suf_e - suf + 1;

  for (WordAff * * cur = &head; cur != end; cur = &(*cur)->next) {
    if ((int)(*cur)->word.size - max_strip_ >= limit) continue;
    byte * nsuf = (byte *)buf.alloc(nsuf_s);
    expand_suffix((*cur)->word, (*cur)->aff, buf, limit, nsuf, &very_end, word);
    (*cur)->aff = nsuf;
  }

  return head;
}

WordAff * AffixMgr::expand_suffix(ParmString word, const byte * aff, 
                                  ObjStack & buf, int limit,
                                  byte * new_aff, WordAff * * * l,
                                  ParmString orig_word) const
{
  WordAff * head = 0;
  if (l) head = **l;
  WordAff * * cur = l ? *l : &head;
  bool expanded     = false;
  bool not_expanded = false;
  if (!orig_word) orig_word = word;

  while (*aff) {
    if ((int)word.size() - max_strip_f[*aff] < limit) {
      for (SfxEntry * p = sFlag[*aff]; p; p = p->flag_next) {
        SimpleString newword = p->add(word, buf, limit, orig_word);
        if (!newword) continue;
        if (newword == EMPTY) {not_expanded = true; continue;}
        *cur = (WordAff *)buf.alloc_bottom(sizeof(WordAff));
        (*cur)->word = newword;
        (*cur)->aff  = (const byte *)EMPTY;
        cur = &(*cur)->next;
        expanded = true;
      }
    }
    if (new_aff && (!expanded || not_expanded)) *new_aff++ = *aff;
    ++aff;
  }
  *cur = 0;
  if (new_aff) *new_aff = 0;
  if (l) *l = cur;
  return head;
}

CheckAffixRes AffixMgr::check_affix(ParmString word, char aff) const
{
  CheckAffixRes res = InvalidAffix;
  
  for (PfxEntry * p = pFlag[(unsigned char)aff]; p; p = p->flag_next) {
    res = InapplicableAffix;
    if (p->applicable(word)) return ValidAffix;
  }

  for (SfxEntry * p = sFlag[(unsigned char)aff]; p; p = p->flag_next) {
    if (res == InvalidAffix) res = InapplicableAffix;
    if (p->applicable(word)) return ValidAffix;
  }

  return res;
}



//////////////////////////////////////////////////////////////////////
//
// LookupInfo
//

int LookupInfo::lookup (ParmString word, const SensitiveCompare * c, 
                        char achar, 
                        WordEntry & o, GuessInfo * gi) const
{
  SpellerImpl::WS::const_iterator i = begin;
  const char * g = 0;
  if (mode == Word) {
    do {
      (*i)->lookup(word, c, o);
      for (;!o.at_end(); o.adv()) {
        if (TESTAFF(o.aff, achar))
          return 1;
        else
          g = o.word;
      }
      ++i;
    } while (i != end);
  } else if (mode == Clean) {
    do {
      (*i)->clean_lookup(word, o);
      for (;!o.at_end(); o.adv()) {
        if (TESTAFF(o.aff, achar))
          return 1;
        else
          g = o.word;
      }
      ++i;
    } while (i != end);
  } else if (gi) {
    g = gi->dup(word);
  }
  if (gi && g) {
    CheckInfo * ci = gi->add();
    ci->word = g;
    return -1;
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////
//
// Affix Entry
//

bool PfxEntry::applicable(SimpleString word) const
{
  unsigned int cond;
  /* make sure all conditions match */
  if ((word.size > stripl) && (word.size >= conds->num)) {
    const byte * cp = (const byte *) word.str;
    for (cond = 0;  cond < conds->num;  cond++) {
      if ((conds->get(*cp++) & (1 << cond)) == 0)
        break;
    }
    if (cond >= conds->num) return true;
  }
  return false;
}

// add prefix to this word assuming conditions hold
SimpleString PfxEntry::add(SimpleString word, ObjStack & buf) const
{
  unsigned int cond;
  /* make sure all conditions match */
  if ((word.size > stripl) && (word.size >= conds->num)) {
    const byte * cp = (const byte *) word.str;
    for (cond = 0;  cond < conds->num;  cond++) {
      if ((conds->get(*cp++) & (1 << cond)) == 0)
        break;
    }
    if (cond >= conds->num) {
      /* */
      int alen = word.size - stripl;
      char * newword = (char *)buf.alloc(alen + appndl + 1);
      if (appndl) memcpy(newword, appnd, appndl);
      memcpy(newword + appndl, word + stripl, alen + 1);
      return SimpleString(newword, alen + appndl);
    }
  }
  return SimpleString();
}

// check if this prefix entry matches 
bool PfxEntry::check(const LookupInfo & linf, const AffixMgr * pmyMgr,
                     ParmString word,
                     CheckInfo & ci, GuessInfo * gi, bool cross) const
{
  unsigned int		cond;	// condition number being examined
  unsigned              tmpl;   // length of tmpword
  WordEntry             wordinfo;     // hash entry of root word or NULL
  byte *	cp;		
  VARARRAYM(char, tmpword, word.size()+stripl+1, MAXWORDLEN+1);

  // on entry prefix is 0 length or already matches the beginning of the word.
  // So if the remaining root word has positive length
  // and if there are enough chars in root word and added back strip chars
  // to meet the number of characters conditions, then test it

  tmpl = word.size() - appndl;

  if ((tmpl > 0) &&  (tmpl + stripl >= conds->num)) {

    // generate new root word by removing prefix and adding
    // back any characters that would have been stripped

    if (stripl) strcpy (tmpword, strip);
    strcpy ((tmpword + stripl), (word + appndl));

    // now make sure all of the conditions on characters
    // are met.  Please see the appendix at the end of
    // this file for more info on exactly what is being
    // tested

    cp = (byte *)tmpword;
    for (cond = 0;  cond < conds->num;  cond++) {
      if ((conds->get(*cp++) & (1 << cond)) == 0) break;
    }

    // if all conditions are met then check if resulting
    // root word in the dictionary

    if (cond >= conds->num) {
      CheckInfo * lci = 0;
      CheckInfo * guess = 0;
      tmpl += stripl;

      int res = linf.lookup(tmpword, &linf.sp->s_cmp_end, achar, wordinfo, gi);

      if (res == 1) {

        lci = &ci;
        lci->word = wordinfo.word;
        goto quit;
        
      } else if (res == -1) {

        guess = gi->head;

      }
      
      // prefix matched but no root word was found 
      // if XPRODUCT is allowed, try again but now 
      // cross checked combined with a suffix
      
      if (gi)
        lci = gi->head;
      
      if (cross && xpflg & XPRODUCT) {
        if (pmyMgr->suffix_check(linf, ParmString(tmpword, tmpl), 
                                 ci, gi,
                                 XPRODUCT, (AffEntry *)this)) {
          lci = &ci;
          
        } else if (gi) {
          
          CheckInfo * stop = lci;
          for (lci = gi->head; 
               lci != stop; 
               lci = const_cast<CheckInfo *>(lci->next)) 
          {
            lci->pre_flag = achar;
            lci->pre_strip_len = stripl;
            lci->pre_add_len = appndl;
            lci->pre_add = appnd;
          }
          
        } else {
          
          lci = 0;
          
        }
      }
    
      if (guess)
        lci = guess;
      
    quit:
      if (lci) {
        lci->pre_flag = achar;
        lci->pre_strip_len = stripl;
        lci->pre_add_len = appndl;
        lci->pre_add = appnd;
      }
      if (lci == &ci) return true;
    }
  }
  return false;
}

bool SfxEntry::applicable(SimpleString word) const
{
  int cond;
  /* make sure all conditions match */
  if ((word.size > stripl) && (word.size >= conds->num)) {
    const byte * cp = (const byte *) (word + word.size);
    for (cond = conds->num; --cond >=0; ) {
      if ((conds->get(*--cp) & (1 << cond)) == 0)
        break;
    }
    if (cond < 0) return true;
  }
  return false;
}

// add suffix to this word assuming conditions hold
SimpleString SfxEntry::add(SimpleString word, ObjStack & buf, 
                           int limit, SimpleString orig_word) const
{
  int cond;
  /* make sure all conditions match */
  if ((orig_word.size > stripl) && (orig_word.size >= conds->num)) {
    const byte * cp = (const byte *) (orig_word + orig_word.size);
    for (cond = conds->num; --cond >=0; ) {
      if ((conds->get(*--cp) & (1 << cond)) == 0)
        break;
    }
    if (cond < 0) {
      int alen = word.size - stripl;
      if (alen >= limit) return EMPTY;
      /* we have a match so add suffix */
      char * newword = (char *)buf.alloc(alen + appndl + 1);
      memcpy(newword, word, alen);
      memcpy(newword + alen, appnd, appndl + 1);
      return SimpleString(newword, alen + appndl);
    }
  }
  return SimpleString();
}

// see if this suffix is present in the word 
bool SfxEntry::check(const LookupInfo & linf, ParmString word,
                     CheckInfo & ci, GuessInfo * gi,
                     int optflags, AffEntry* ppfx)
{
  unsigned              tmpl;		 // length of tmpword 
  int			cond;		 // condition beng examined
  WordEntry             wordinfo;        // hash entry pointer
  byte *	cp;
  VARARRAYM(char, tmpword, word.size()+stripl+1, MAXWORDLEN+1);
  PfxEntry* ep = (PfxEntry *) ppfx;

  // if this suffix is being cross checked with a prefix
  // but it does not support cross products skip it

  if ((optflags & XPRODUCT) != 0 &&  (xpflg & XPRODUCT) == 0)
    return false;

  // upon entry suffix is 0 length or already matches the end of the word.
  // So if the remaining root word has positive length
  // and if there are enough chars in root word and added back strip chars
  // to meet the number of characters conditions, then test it

  tmpl = word.size() - appndl;

  if ((tmpl > 0)  &&  (tmpl + stripl >= conds->num)) {

    // generate new root word by removing suffix and adding
    // back any characters that would have been stripped or
    // or null terminating the shorter string

    strcpy (tmpword, word);
    cp = (byte *)(tmpword + tmpl);
    if (stripl) {
      strcpy ((char *)cp, strip);
      tmpl += stripl;
      cp = (byte *)(tmpword + tmpl);
    } else *cp = '\0';

    // now make sure all of the conditions on characters
    // are met.  Please see the appendix at the end of
    // this file for more info on exactly what is being
    // tested

    for (cond = conds->num;  --cond >= 0; ) {
      if ((conds->get(*--cp) & (1 << cond)) == 0) break;
    }

    // if all conditions are met then check if resulting
    // root word in the dictionary

    if (cond < 0) {
      CheckInfo * lci = 0;
      tmpl += stripl;
      const SensitiveCompare * cmp = 
        optflags & XPRODUCT ? &linf.sp->s_cmp_middle : &linf.sp->s_cmp_begin;
      int res = linf.lookup(tmpword, cmp, achar, wordinfo, gi);
      if (res == 1
          && ((optflags & XPRODUCT) == 0 || TESTAFF(wordinfo.aff, ep->achar)))
      {
        lci = &ci;
        lci->word = wordinfo.word;
      } else if (res == 1 && gi) {
        lci = gi->add();
        lci->word = wordinfo.word;
      } else if (res == -1) { // gi must be defined
        lci = gi->head;
      }

      if (lci) {
        lci->suf_flag = achar;
        lci->suf_strip_len = stripl;
        lci->suf_add_len = appndl;
        lci->suf_add = appnd;
      }
      
      if (lci == &ci) return true;
    }
  }
  return false;
}

//////////////////////////////////////////////////////////////////////
//
// new_affix_mgr
//


PosibErr<AffixMgr *> new_affix_mgr(ParmString name, 
                                   Conv & iconv,
                                   const Language * lang)
{
  if (name == "none")
    return 0;
  //CERR << "NEW AFFIX MGR\n";
  String file;
  file += lang->data_dir();
  file += '/';
  file += lang->name();
  file += "_affix.dat";
  AffixMgr * affix;
  affix = new AffixMgr(lang);
  PosibErrBase pe = affix->setup(file, iconv);
  if (pe.has_err()) {
    delete affix;
    return pe;
  } else {
    return affix;
  }
}
}

/**************************************************************************

Appendix:  Understanding Affix Code


An affix is either a  prefix or a suffix attached to root words to make 
other words.

Basically a Prefix or a Suffix is set of AffEntry objects
which store information about the prefix or suffix along 
with supporting routines to check if a word has a particular 
prefix or suffix or a combination.

The structure affentry is defined as follows:

struct AffEntry
{
   unsigned char achar;   // char used to represent the affix
   char * strip;          // string to strip before adding affix
   char * appnd;          // the affix string to add
   short  stripl;         // length of the strip string
   short  appndl;         // length of the affix string
   short  numconds;       // the number of conditions that must be met
   short  xpflg;          // flag: XPRODUCT- combine both prefix and suffix 
   char   conds[SETSIZE]; // array which encodes the conditions to be met
};


Here is a suffix borrowed from the en_US.aff file.  This file 
is whitespace delimited.

SFX D Y 4 
SFX D   0     e          d
SFX D   y     ied        [^aeiou]y
SFX D   0     ed         [^ey]
SFX D   0     ed         [aeiou]y

This information can be interpreted as follows:

In the first line has 4 fields

Field
-----
1     SFX - indicates this is a suffix
2     D   - is the name of the character flag which represents this suffix
3     Y   - indicates it can be combined with prefixes (cross product)
4     4   - indicates that sequence of 4 affentry structures are needed to
               properly store the affix information

The remaining lines describe the unique information for the 4 SfxEntry 
objects that make up this affix.  Each line can be interpreted
as follows: (note fields 1 and 2 are as a check against line 1 info)

Field
-----
1     SFX         - indicates this is a suffix
2     D           - is the name of the character flag for this affix
3     y           - the string of chars to strip off before adding affix
                         (a 0 here indicates the NULL string)
4     ied         - the string of affix characters to add
5     [^aeiou]y   - the conditions which must be met before the affix
                    can be applied

Field 5 is interesting.  Since this is a suffix, field 5 tells us that
there are 2 conditions that must be met.  The first condition is that 
the next to the last character in the word must *NOT* be any of the 
following "a", "e", "i", "o" or "u".  The second condition is that
the last character of the word must end in "y".

So how can we encode this information concisely and be able to 
test for both conditions in a fast manner?  The answer is found
but studying the wonderful ispell code of Geoff Kuenning, et.al. 
(now available under a normal BSD license).

If we set up a conds array of 256 bytes indexed (0 to 255) and access it
using a character (cast to an unsigned char) of a string, we have 8 bits
of information we can store about that character.  Specifically we
could use each bit to say if that character is allowed in any of the 
last (or first for prefixes) 8 characters of the word.

Basically, each character at one end of the word (up to the number 
of conditions) is used to index into the conds array and the resulting 
value found there says whether the that character is valid for a 
specific character position in the word.  

For prefixes, it does this by setting bit 0 if that char is valid 
in the first position, bit 1 if valid in the second position, and so on. 

If a bit is not set, then that char is not valid for that position in the
word.

If working with suffixes bit 0 is used for the character closest 
to the front, bit 1 for the next character towards the end, ..., 
with bit numconds-1 representing the last char at the end of the string. 

Note: since entries in the conds[] are 8 bits, only 8 conditions 
(read that only 8 character positions) can be examined at one
end of a word (the beginning for prefixes and the end for suffixes.

So to make this clearer, lets encode the conds array values for the 
first two affentries for the suffix D described earlier.


  For the first affentry:    
     numconds = 1             (only examine the last character)

     conds['e'] =  (1 << 0)   (the word must end in an E)
     all others are all 0

  For the second affentry:
     numconds = 2             (only examine the last two characters)     

     conds[X] = conds[X] | (1 << 0)     (aeiou are not allowed)
         where X is all characters *but* a, e, i, o, or u
         

     conds['y'] = (1 << 1)     (the last char must be a y)
     all other bits for all other entries in the conds array are zero


**************************************************************************/
