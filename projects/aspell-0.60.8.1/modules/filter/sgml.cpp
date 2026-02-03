// This file is part of The New Aspell
// Copyright (C) 2004 by Tom Snyder
// Copyright (C) 2001-2004 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.
//
// The original filter was written by Kevin Atkinson.
// Tom Snyder rewrote the filter to support skipping SGML tags
//
// This filter enables the spell checking of sgml, html, and xhtml files
// by skipping the <elements> and such.
// The overall strategy is based on http://www.w3.org/Library/SGML.c.
// We don't use that code (nor the sourceforge 'expat' project code) for
// simplicity's sake.  We don't need to fully parse all aspects of HTML -
// we just need to skip and handle a few aspects. The w3.org code had too
// many linkages into their overall SGML/HTML processing engine.
//
// See the comment at the end of this file for examples of what we handle.
// See the config setting docs regarding our config lists: check and skip.

#include <stdio.h> // needed for sprintf

#include "settings.h"

#include "asc_ctype.hpp"
#include "config.hpp"
#include "indiv_filter.hpp"
#include "string_map.hpp"
#include "mutable_container.hpp"
#include "clone_ptr-t.hpp"
#include "filter_char_vector.hpp"

//right now unused option
//  static const KeyInfo sgml_options[] = {
//    {"sgml-extension", KeyInfoList, "html,htm,php,sgml",
//     N_("sgml file extensions")}
//  };

namespace {

  using namespace acommon;

  class ToLowerMap : public StringMap
  {
  public:
    PosibErr<bool> add(ParmStr to_add) {
      String new_key;
      for (const char * i = to_add; *i; ++i) new_key += asc_tolower(*i);
      return StringMap::add(new_key);
    }

    PosibErr<bool> remove(ParmStr to_rem) {
      String new_key;
      for (const char * i = to_rem; *i; ++i) new_key += asc_tolower(*i);
      return StringMap::remove(new_key);
    }
  };

  class SgmlFilter : public IndividualFilter 
  {
    // State enum. These states track where we are in the HTML/tag/element constructs.
    // This diagram shows the main states. The marked number is the state we enter
    // *after* we've read that char. Note that some of the state transitions handle
    // illegal HTML such as <tag=what?>.
    //
    //  real text <tag attrib = this  attrib2='that'> &nbsp; </tag> &#123;
    //   |        |   |  |   ||  |             |      |  |    |      | 
    //   1        2   3  4   56  7             8      10 11   9      12
    enum ScanState {
      S_text,   // 1. raw user text outside of any markup.
      S_tag,    // 2. reading the 'tag' in <tag>
      S_tag_gap,// 3. gap between attributes within an element:
      S_attr,   // 4. Looking at an attrib name
      S_attr_gap,// 5. optional gap after attrib name
      S_equals,  // 6. Attrib equals sign, also space after the equals.
      S_value,   // 7. In attrib value.
      S_quoted,  // 8. In quoted attrib value.
      S_end,     // 9. Same as S_tag, but this is a </zee> type end tag.
      S_ignore_junk, // special case invalid area to ignore.
      S_ero,     // 10. in the &code; special encoding within HTML.
      S_entity,  // 11. in the alpha named &nom; special encoding.. 
      S_cro,     // 12. after the # of a &#nnn; numerical char reference encoding.
  
    // SGML.. etc can have these special "declarations" within them. We skip them
    //  in a more raw manners since they don't abide by the attrib= rules.
    // Most importantly, some of the attrib quoting rules don't apply.
    //  <!ENTITY rtfchar "gg" - - (high, low)>  <!--  fully commented -->
    //   |               |                        ||                   |
    //   20              21                     23  24                 25
    S_md,     // 20. In a declaration (or beginning a comment).
    S_mdq,    // 21. Declaration in quotes - double or single quotes.
    S_com_1,  // 23. perhaps a comment beginning.
    S_com,    // 24. Fully in a comment
    S_com_e,  // 25. Perhaps ending a comment.
    
    //S_literal, // within a tag pair that means all content should be interpreted literally: <PRE>
               // NOT CURRENTLY SUPPORTED FULLY.
               
    //S_esc,S_dollar,S_paren,S_nonasciitext // WOULD BE USED FOR ISO_2022_JP support.
                                          // NOT CURRENTLY SUPPORTED.               
    };
    
    ScanState in_what;
	     // which quote char is quoting this attrib value.	
    FilterChar::Chr  quote_val;   
	    // one char prior to this one. For escape handling and such.
    FilterChar::Chr  lookbehind;   

    String tag_name;    // we accumulate the current tag name here.
    String attrib_name; // we accumulate the current attribute name here.
    
    bool include_attrib;  // are we in one of the attribs that *should* be spell checked (alt=..)
    int  skipall;         // are we in one of the special skip-all content tags? This is treated
			  // as a bool and as a nesting level count.
    String tag_endskip;  // tag name that will end that.
    
    StringMap check_attribs; // list of attribs that we *should* spell check.
    StringMap skip_tags;   // list of tags that start a no-check-at-all zone.

    String which;

    bool process_char(FilterChar::Chr c);
 
  public:

    SgmlFilter(const char * n) : which(n) {}

    PosibErr<bool> setup(Config *);
    void reset();
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> SgmlFilter::setup(Config * opts) 
  {
    name_ = which + "-filter";
    order_num_ = 0.35;
    check_attribs.clear();
    skip_tags.clear();
    opts->retrieve_list("f-" + which + "-skip",  &skip_tags);
    opts->retrieve_list("f-" + which + "-check", &check_attribs);
    reset();
    return true;
  }
  
  void SgmlFilter::reset() 
  {
    in_what = S_text;
    quote_val = lookbehind = '\0';
    skipall = 0;
    include_attrib = false;
  }

  // yes this should be inlines, it is only called once
  
  // RETURNS: TRUE if the caller should skip the passed char and
  //  not do any spell check on it. FALSE if char is a part of the text
  //  of the document.
  bool SgmlFilter::process_char(FilterChar::Chr c) {
  
    bool retval = true;  // DEFAULT RETURN VALUE. All returns are done
    			 // via retval and falling out the bottom. Except for
    			 // one case that must manage the lookbehind char.
    
    // PS: this switch will be fast since S_ABCs are an enum and
    //  any good compiler will build a jump table for it.
    // RE the gotos: Sometimes considered bad practice but that is
    //  how the W3C code (1995) handles it. Could be done also with recursion
    //  but I doubt that will clarify it. The gotos are done in cases where several
    //  state changes occur on a single input char.

    switch( in_what ) {
    
      case S_text:   // 1. raw user text outside of any markup.
	   s_text:
        switch( c ) {
          case '&': in_what = S_ero; 
		    break;
          case '<': in_what = S_tag; tag_name.clear(); 
		    break;
          default:
                retval = skipall;  // ********** RETVAL ASSIGNED
        }			    // **************************
        break;
        
      case S_tag:    // 2. reading the 'tag' in <tag>
      		     //  heads up: <foo/bar will be treated as an end tag. That's what w3c does.
        switch( c ) {
	  case '>': goto all_end_tags;
          case '/': in_what = S_end; 
		    tag_name.clear(); 
		    break;
          case '!': in_what = S_md;  
		    break;
          default: // either more alphanum of the tag, or end of tagname:
            if( asc_isalpha(c) || asc_isdigit(c) ) {
                tag_name += asc_tolower(c);            
            }
            else {  // End of the tag:
                in_what = S_tag_gap;
		goto s_tag_gap;  // handle content in that zone.
	    }
	}
	break;

	// '>'  '>'  '>'  '>' 
      all_end_tags:   // this gets called by several states to handle the
		       // possibility of a '>' ending a whole <tag...> guy.
	if( c != '>' ) break;
	in_what = S_text;

	if( lookbehind == '/' ) {
	    // Wowza: this is how we handle the <script stuff /> XML style self
	    //  terminating tag. By clearing the tagname out tag-skip-all code
	    //  will not be invoked.
	    tag_name.clear();
	}
	    
	// Does this tag cause us to skip all content?
	if( skipall ) {  
	    // already in a skip-all context. See if this is
	    // the same skipall tag:
	    if( !strcmp( tag_name.c_str(), tag_endskip.c_str() ) ) {
		++skipall;  // increment our nesting level count.
	    }
	}
	else {  // Should we begin a skip all range?
	    skipall = (skip_tags.have( tag_name.c_str() ) ? 1 : 0);
	    if( skipall ) {
		tag_endskip = tag_name;  // remember what tag to end on.
	    }
	}
	break;
                        
      case S_tag_gap: // 3. gap between attributes within an element:
	   s_tag_gap:
      	switch( c ) {
      	  case '>': goto all_end_tags;

      	  case '=': in_what = S_attr_gap; 
		    break; // uncommon - no-name attrib value
      	  default:
      	    if( asc_isspace( c ) ) break; // still in gap.
      	    else {
		in_what = S_attr;   // start of attribute name;
		attrib_name.clear();
      	 	attrib_name += asc_tolower( c );
      	    }
      	    break;
      	 }
      	 break;
      	 
      case S_end:     // 9. Same as S_tag, but this is a </zee> type end tag.
      	if( asc_isalpha(c) || asc_isdigit(c) ) {
      	  tag_name += asc_tolower( c );
      	}
      	else {
	  // See if we have left a skipall tag range.
	  if( skipall && !strcmp( tag_name.c_str(), tag_endskip.c_str() ) ) {
	    --skipall; // lessen nesting level count. This usually takes us to zero.
	  }
	  if( c == '>' ) in_what = S_text;  // --don't go to all_end_tags.  Really.
	  else in_what = S_ignore_junk;  // no-mans land: </end whats this??>
      	}
      	break;
      	
      case S_ignore_junk:  // no-mans land state: </end whats this here??>
      	if( c == '>' ) in_what = S_text;
      	break;
      
      case S_attr:   // 4. Looking at an attrib name
      	if( asc_isspace(c) ) in_what = S_attr_gap;
      	else if( c == '=' )  in_what = S_equals;
	else if( c == '>' )  goto all_end_tags;
	else {
	  attrib_name += asc_tolower( c );
	}
	break;
	
      case S_attr_gap: // 5. optional gap after attrib name
      	if( asc_isspace(c) ) break;
      	else if( c == '=' )  in_what = S_equals;
	else if( c == '>' )  goto all_end_tags;
	else { // beginning of a brand new attr
	  attrib_name.clear();
	  attrib_name += asc_tolower( c );
	}
	break;
      
      case S_equals:  // 6. Attrib equals sign, also space after the equals.
      	if( asc_isspace(c) ) break;
      	switch( c ) {
      	  case '>':  goto all_end_tags;

      	  case '\'': 
      	  case '"':  in_what = S_quoted;
      	  	     quote_val = c;
		     break;
      	  default:   in_what = S_value;   
		     break;
      	}
      	// See if this attrib deserves full checking:
      	include_attrib=check_attribs.have( attrib_name.c_str() );
	// Handle the first value char if that is where we are now:
	if( in_what == S_value ) goto s_value;
	break;
      
      case S_value:   // 7. In attrib value.
	   s_value:
      	if( c == '>' ) goto all_end_tags;
      	else if( asc_isspace(c) ) in_what = S_tag_gap; // end of attrib value
      				// *****************************
      				// ********** RETVAL ASSIGNED
      	else if( include_attrib ) retval = false; // spell check this value.
      	break;
      
      case S_quoted: // 8. In quoted attrib value.
      	if( c == quote_val && lookbehind != '\\' ) in_what = S_tag_gap;
      	else if( c == '\\' && lookbehind == '\\' ) {
      		// This is an escape of an backslash. Therefore the backslash
      		// does not escape what follows. Therefore we don't leave it in
      		// the lookbehind. Yikes!
      	  lookbehind = '\0';
      	  return !include_attrib;      // ************* RETURN RETURN RETURN RETURN
      	}
      	else retval = !include_attrib;
	break;
      
      // note: these three cases - S_ero, S_cro, and S_entity which all handle
      //  the &stuff; constructs are broken into 3 states for future upgrades. Someday
      //  you may want to handle the chars these guys represent as individual chars.
      //  I don't have the desire nor the knowledge to do it now.  -Tom, 5/5/04.	
      case S_ero:     // 10. in the &code; special encoding within HTML.
      		// &# is a 'Char Ref Open'
      	if( c == '#' ) {
	  in_what = S_cro;
	  break;
	}
      	// FALLTHROUGH INTENTIONAL
      	
      case S_cro:     // 12. after the # of a &#nnn; numerical char reference encoding.
      case S_entity:  // 11. in the alpha named &nom; special encoding.. 
	if( asc_isalpha(c) || asc_isdigit(c) ) break; // more entity chars.
	in_what = S_text;
      	if( c == ';' ) break;  // end of char code.
	goto s_text; // ran right into text. Handle it.
      
  
      // SGML.. etc can have these special "declarations" within them. We skip them
      //  in a more raw manners since they don't abide by the attrib= rules.
      // Most importantly, some of the quoting rules don't apply.
      //  <!ENTITY rtfchar "gg" 'tt' - - (high, low)>  <!--  fully commented -->
      //   |               |    |                        ||                  ||
      //   20              21   22                     23  24              25  26
      case S_md:     // 20. In a declaration (or comment).
      	switch( c ) {
      	  case '-': if( lookbehind == '!' ) {
			in_what = S_com_1;
		    }
		    break;

      	  case '"':     // fallthrough - yes.
      	  case '\'': in_what = S_mdq; 
		     quote_val=c; 
		     break;
      	  case '>':  in_what = S_text; // note: NOT all_end_tags cause it's not a real tag.
		     break;
      	}
      	break;
    
    case S_mdq: // 22. Declaration in quotes.
    	if( c == quote_val ) in_what = S_md;
    	else if( c == '>' )  in_what = S_text;
    	break;
    
    case S_com_1:  // 23. perhaps a comment beginning.
    	if( c == '-' ) in_what = S_com;
    	else if( c == '>' ) in_what = S_text;
    	else in_what = S_md; // out of possible comment.
    	break;
    	
    case S_com:    // 24. Fully in a comment
    	if( c == '-' && lookbehind == '-' ) in_what = S_com_e;
    	break;
    
    case S_com_e:  // 25. Perhaps ending a comment.
    	if( c == '>' ) in_what = S_text;
    	else if( c != '-' ) in_what = S_com;  // back to basic comment.
    	break;
    }
  
    // update the lookbehind:
    lookbehind = c;
   
    return( retval );
  }
  
  void SgmlFilter::process(FilterChar * & str, FilterChar * & stop)
  {
    FilterChar * cur = str;
    while (cur != stop) {
      if (process_char(*cur))
	*cur = ' ';
      ++cur;
    }
  }
  
  //
  //
  //

  class SgmlDecoder : public IndividualFilter 
  {
    FilterCharVector buf;
    String which;
  public:
    SgmlDecoder(const char * n) : which(n) {}
    PosibErr<bool> setup(Config *);
    void reset() {}
    void process(FilterChar * &, FilterChar * &);
  };

  PosibErr<bool> SgmlDecoder::setup(Config *) 
  {
    name_ = which + "-decoder";
    order_num_ = 0.65;
    return true;
  }

  void SgmlDecoder::process(FilterChar * & start, FilterChar * & stop)
  {
    buf.clear();
    FilterChar * i = start;
    while (i != stop)
    {
      if (*i == '&') {
	FilterChar * i0 = i;
	FilterChar::Chr chr;
	++i;
	if (i != stop && *i == '#') {
	  chr = 0;
	  ++i;
	  while (i != stop && asc_isdigit(*i)) {
	    chr *= 10;
	    chr += *i - '0';
	    ++i;
	  }
	} else {
	  while (i != stop && (asc_isalpha(*i) || asc_isdigit(*i))) {
	    ++i;
	  }
	  chr = '?';
	}
	if (i != stop && *i == ';')
	  ++i;
	buf.append(FilterChar(chr, i0, i));
      } else {
	buf.append(*i);
	++i;
      }
    }
    buf.append('\0');
    start = buf.pbegin();
    stop  = buf.pend() - 1;
  }

  //
  // Sgml Encoder - BROKEN do not use
  //

//   class SgmlEncoder : public IndividualFilter 
//   {
//     FilterCharVector buf;
//     String which;
//   public:
//     SgmlEncoder(const char * n) : which(n) {}
//     PosibErr<bool> setup(Config *);
//     void reset() {}
//     void process(FilterChar * &, FilterChar * &);
//   };

//   PosibErr<bool> SgmlEncoder::setup(Config *) 
//   {
//     name_ = which + "-encoder";
//     order_num_ = 0.99;
//     return true;
//   }

//   void SgmlEncoder::process(FilterChar * & start, FilterChar * & stop)
//   {
//     buf.clear();
//     FilterChar * i = start;
//     while (i != stop)
//     {
//       if (*i > 127) {
// 	buf.append("&#", i->width);
// 	char b[10];
// 	sprintf(b, "%d", i->chr);
// 	buf.append(b, 0);
// 	buf.append(';', 0);
//       } else {
// 	buf.append(*i);
//       }
//       ++i;
//     }
//     buf.append('\0');
//     start = buf.pbegin();
//     stop  = buf.pend() - 1;
//   }
}

C_EXPORT IndividualFilter * new_aspell_sgml_filter() 
{
  return new SgmlFilter("sgml");
}
C_EXPORT IndividualFilter * new_aspell_sgml_decoder() 
{
  return new SgmlDecoder("sgml");
}
// C_EXPORT IndividualFilter * new_aspell_sgml_encoder() 
// {
//   return new SgmlEncoder("sgml");
// }

C_EXPORT IndividualFilter * new_aspell_html_filter() 
{
  return new SgmlFilter("html");
}
C_EXPORT IndividualFilter * new_aspell_html_decoder() 
{
  return new SgmlDecoder("html");
}
// C_EXPORT IndividualFilter * new_aspell_html_encoder() 
// {
//   return new SgmlEncoder("html");
// }


/* Example HTML:

<!-- 
This file contains several constructs that test the parsing and
handling of SGML/HTML/XML in sgml.cpp.

The only spelling errors you should see will be the word 'report this NNNN'.
There will be 22 of these.

run this by executing:
aspell pipe -H < sgmltest.html

WARNING: this is not really valid HTML. Don't display in a browser!
-->

<!-- phase 1 - SGML comments. -->
reportthiszphaseONE
 <!-- ** 1.0 Valid comments... This file is full of them.  -->
 <!-- ** 1.1 invalid open comment: -->
<!- not in a comment>reportthisyes</!->

 <!-- ** 1.2 invalid close comment: -->
<!-- -- > spallwhat DON'T REPORT -> spallwhat DON'T REPORT -->

<!-- phase 1.5 - special entity encodings -->
reportthisphaseONEFIVE
 &nbsp; don't&nbsp;report&nbsp;this
 &#011; do not&#x20;report this.
 do not&gt;report this.
 this &amp; that.

<!-- phase 2 - special skip tags -->
reportthisphaseTWO
<SCRIPT> spallwhat DON'T REPORT </SCRIPT> reportthisyes
<style> spallwhat DON'T REPORT </style> reportthisyes
<STYLE something="yes yes"
      > spallwhat DON'T REPORT </style > reportthisyes
<script/> reportthisyes  <!-- XHTML style terminated tag -->
<script someattrib=value/> reportthisyes  <!-- XHTML style terminated tag -->
<!-- Nested skip tags -->
<script> spallwhatnoreport <script> nonoreport </script><b>hello</b> nonoreport</script>reportthisyes

<!-- phase 3 - special 'include this' attributes -->
reportthisphaseTHREE
<tagname alt="image text reportthisyes" alt2=spallwhat altt="spallwhat don't report">
<tagname ALT="image text reportthisyes" ALT2=spallwhat AL="spallwhat don't report">

<!-- phase 4 - attribute value quoteing and escaping -->
reportthisphaseoneFOUR
<checkthis attribute111=simple/value.value >
<checkagain SOMEattrib   =   "whoa boy, mimimimspelled  ">
<singlequotes gotcha=   'singlypingly quoted///'>
<dblescaped gogogogo="dontcheck \">still in dontcheck\\\" still in dontcheck"> reportthisyes.
<dBLmore TomTomTomTom="so many escapes: \\\\\\\\"> reportthisyes.
<dblescaped gogogogo='dontcheck \'>still in dontcheck\\\' still in dontcheck'> reportthisyes.
<dBLmore TomTomTomTom='so many escapes: \\\\\\\\'> reportthisyes.
<mixnmatch scanhere='">dontcheck \"dontcheck \'dontcheck' alt=reportthisyes>

<!-- phase 5 - questionable (though all too common) constructs -->
reportthisphaseFIVE
<tag=dontreport> reportthisyes <tag hahahahhaha>reportthisyes
<!-- this one is from Yahoo! -->
<td width=1%><img src="http://wellll/thereeee/nowwww" alt="cool stuff">
<td width=1%><img src=http://wellll/thereeee/nowwww alt=real cool stuff>

*/
