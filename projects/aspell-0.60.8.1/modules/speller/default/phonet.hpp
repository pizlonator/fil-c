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
*/

#ifndef ASPELLER_PHONET__HPP
#define ASPELLER_PHONET__HPP

#include "string.hpp"
#include "posib_err.hpp"

using namespace acommon;

namespace acommon {struct Conv;}

namespace aspeller {

  class Language;

  struct PhonetParms {
    String version;
    
    bool followup;
    bool collapse_result;

    bool remove_accents;

    static const char * const rules_end;
    const char * * rules;

    const Language * lang;

    char to_clean[256];

    static const int hash_size = 256;
    int hash[hash_size];

    virtual ~PhonetParms() {}
  };

  int phonet (const char * inword, char * target, 
              int len,
	      const PhonetParms & parms);

#if 0
  void dump_phonet_rules(std::ostream & out, const PhonetParms & parms);
  // the istream must be seekable
#endif

  PosibErr<PhonetParms *> new_phonet(const String & file, 
                                     Conv & iconv,
                                     const Language * lang);

}

#endif
