// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.
//
// Copyright 2002 Kevin B. Hendricks, Stratford, Ontario, Canada And
// Contributors.  All rights reserved.
//

#ifndef ASPELL_AFFIX__HPP
#define ASPELL_AFFIX__HPP

#include "posib_err.hpp"
#include "wordinfo.hpp"
#include "fstream.hpp"
#include "parm_string.hpp"
#include "simple_string.hpp"
#include "char_vector.hpp"
#include "objstack.hpp"

#define SETSIZE         256
#define MAXAFFIXES      256
#define MAXWORDLEN      255
#define XPRODUCT        (1 << 0)

#define MAXLNLEN        1024

#define TESTAFF( a , b) strchr(a, b)

namespace acommon {
  class Config;
  struct CheckInfo;
  struct Conv;
}

namespace aspeller {

  using namespace acommon;

  class Language;

  class SpellerImpl;
  using acommon::CheckInfo;
  struct GuessInfo;

  struct LookupInfo;
  struct AffEntry;
  struct PfxEntry;
  struct SfxEntry;

  struct WordAff
  {
    SimpleString word;
    const unsigned char * aff;
    WordAff * next;
  };

  enum CheckAffixRes {InvalidAffix, InapplicableAffix, ValidAffix};

  class AffixMgr
  {
    const Language * lang;

    PfxEntry *          pStart[SETSIZE];
    SfxEntry *          sStart[SETSIZE];
    PfxEntry *          pFlag[SETSIZE];
    SfxEntry *          sFlag[SETSIZE];

    int max_strip_f[SETSIZE];
    int max_strip_;

    const char *        encoding;
    //const char *        compound;
    //int                 cpdmin;

    ObjStack data_buf;

    const char * affix_file;

  public:
 
    AffixMgr(const Language * l);
    ~AffixMgr();

    unsigned int max_strip() const {return max_strip_;}

    PosibErr<void> setup(ParmString affpath, Conv &);

    bool affix_check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *) const;
    bool prefix_check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *, 
                      bool cross = true) const;
    bool suffix_check(const LookupInfo &, ParmString, CheckInfo &, GuessInfo *,
                      int sfxopts, AffEntry* ppfx) const;

    void munch(ParmString word, GuessInfo *, bool cross = true) const;

    // None of the expand methods reset the objstack

    WordAff * expand(ParmString word, ParmString aff, 
                     ObjStack &, int limit = INT_MAX) const;

    CheckAffixRes check_affix(ParmString word, char aff) const;

    WordAff * expand_prefix(ParmString word, ParmString aff, 
                            ObjStack & buf) const 
    {
      return expand(word,aff,buf,0);
    }
    WordAff * expand_suffix(ParmString word, const unsigned char * aff,
                            ObjStack &, int limit = INT_MAX,
                            unsigned char * new_aff = 0, WordAff * * * l = 0,
                            ParmString orig_word = 0) const;
    
  private:
    PosibErr<void> parse_file(const char * affpath, Conv &);

    PosibErr<void> build_pfxlist(PfxEntry* pfxptr);
    PosibErr<void> build_sfxlist(SfxEntry* sfxptr);
    PosibErr<void> process_pfx_order();
    PosibErr<void> process_sfx_order();
  };

  PosibErr<AffixMgr *> new_affix_mgr(ParmString name, 
                                     Conv &,
                                     const Language * lang);
}

#endif

