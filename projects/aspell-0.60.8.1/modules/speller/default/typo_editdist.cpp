
#include <cstring>

#include "vararray.hpp"
#include "typo_editdist.hpp"
#include "config.hpp"
#include "language.hpp"
#include "file_data_util.hpp"
#include "getdata.hpp"
#include "cache-t.hpp"
#include "asc_ctype.hpp"

// edit_distance is implemented using a straight forward dynamic
// programming algorithm with out any special tricks.  Its space
// usage AND running time is tightly asymptotically bounded by
// strlen(a)*strlen(b)

typedef unsigned char uchar;

namespace aspeller {

  using namespace std;

  short typo_edit_distance(ParmString word0, 
			   ParmString target0,
			   const TypoEditDistanceInfo & w) 
  {
    int word_size   = word0.size() + 1;
    int target_size = target0.size() + 1;
    const uchar * word   = reinterpret_cast<const uchar *>(word0.str());
    const uchar * target = reinterpret_cast<const uchar *>(target0.str());
    VARARRAY(short, e_d, word_size * target_size);
    ShortMatrix e(word_size,target_size, e_d);
    e(0,0) = 0;
    for (int j = 1; j != target_size; ++j)
      e(0,j) = e(0,j-1) + w.missing;
    --word;
    --target;
    short te;
    for (int i = 1; i != word_size; ++i) {
      e(i,0) = e(i-1,0) + w.extra_dis2;
      for (int j = 1; j != target_size; ++j) {

	if (word[i] == target[j]) {

	  e(i,j) = e(i-1,j-1);

	} else {
	  
	  te = e(i,j) = e(i-1,j-1) + w.repl(word[i],target[j]);
	  
	  if (i != 1) {
	    te =  e(i-1,j ) + w.extra(word[i-1], target[j]);
	    if (te < e(i,j)) e(i,j) = te;
	    te = e(i-2,j-1) + w.extra(word[i-1], target[j]) 
 	                     + w.repl(word[i]  , target[j]);
	    if (te < e(i,j)) e(i,j) = te;
	  } else {
	    te =  e(i-1,j) + w.extra_dis2;
	    if (te < e(i,j)) e(i,j) = te;
	  }

	  te = e(i,j-1) + w.missing;
	  if (te < e(i,j)) e(i,j) = te;

	  //swap
	  if (i != 1 && j != 1) {
	      te = e(i-2,j-2) + w.swap
		+ w.repl(word[i], target[j-1])
		+ w.repl(word[i-1], target[j]);
	      if (te < e(i,j)) e(i,j) = te;
	    }
	}
      } 
    }
    return e(word_size-1,target_size-1);
  }

  static GlobalCache<TypoEditDistanceInfo> typo_edit_dist_info_cache("keyboard");

  PosibErr<void> setup(CachePtr<const TypoEditDistanceInfo> & res,
                       const Config * c, const Language * l, ParmString kb)
  {
    PosibErr<TypoEditDistanceInfo *> pe = get_cache_data(&typo_edit_dist_info_cache, c, l, kb);
    if (pe.has_err()) return pe;
    res.reset(pe.data);
    return no_err;
  }

  struct CharPair {
    char d[2];
    CharPair(char a, char b) {d[0] = a; d[1] = b;}
  };

  void TypoEditDistanceInfo::set_max() {
    if (missing > max) max = missing;
    if (swap    > max) max = swap;
    if (repl_dis1 > max) max = repl_dis1;
    if (repl_dis2 > max) max = repl_dis2;
    if (extra_dis1 > max) max = extra_dis1;
    if (extra_dis2 > max) max = extra_dis2;
  }

  PosibErr<TypoEditDistanceInfo *> 
  TypoEditDistanceInfo::get_new(const char * kb, const Config * cfg, const Language * l)
  {
    ConvEC iconv;
    RET_ON_ERR(iconv.setup(*cfg, "utf-8", l->charmap(), NormFrom));
    
    Vector<CharPair> data;
    
    char to_stripped[256];
    for (int i = 0; i <= 255; ++i)
      to_stripped[i] = l->to_stripped(i);
    
    if (strcmp(kb, "none") != 0) {
      FStream in;
      String file, dir1, dir2;
      fill_data_dir(cfg, dir1, dir2);
      find_file(file, dir1, dir2, kb, ".kbd");
      RET_ON_ERR(in.open(file.c_str(), "r"));
      
      String buf;
      DataPair d;
      while (getdata_pair(in, d, buf)) {
        if (d.key == "key") {
          PosibErr<char *> pe(iconv(d.value.str, d.value.size));
          if (pe.has_err()) 
            return pe.with_file(file, d.line_num);
          char * v = pe.data;
          char base = *v;
        while (*v) {
          to_stripped[(uchar)*v] = base;
          ++v;
          while (asc_isspace(*v)) ++v;
        }
        } else {
          PosibErr<char *> pe(iconv(d.key.str, d.key.size));
          if (pe.has_err()) 
            return pe.with_file(file, d.line_num);
          char * v = pe.data;
          if (strlen(v) != 2) 
            return make_err(invalid_string, d.key.str).with_file(file, d.line_num);
          to_stripped[(uchar)v[0]] = v[0];
          to_stripped[(uchar)v[1]] = v[1];
          data.push_back(CharPair(v[0], v[1]));
        }
      }
    }

    TypoEditDistanceInfo * w = new TypoEditDistanceInfo();
    w->keyboard = kb;

    memset(w->to_normalized_, 0, sizeof(w->to_normalized_));

    int c = 1;
    for (int i = 0; i <= 255; ++i) {
      if (l->is_alpha(i)) {
	if (w->to_normalized_[(uchar)to_stripped[i]] == 0) {
	  w->to_normalized_[i] = c;
	  w->to_normalized_[(uchar)to_stripped[i]] = c;
	  ++c;
	} else {
	  w->to_normalized_[i] = w->to_normalized_[(uchar)to_stripped[i]];
	}
      }
    }
    for (int i = 0; i != 256; ++i) {
      if (w->to_normalized_[i]==0) w->to_normalized_[i] = c;
    }
    w->max_normalized = c;
    
    c = w->max_normalized + 1;
    int cc = c * c;
    w->data = (short *)malloc(cc * 2 * sizeof(short));
    w->repl .init(c, c, w->data);
    w->extra.init(c, c, w->data + cc);
    
    for (int i = 0; i != c; ++i) {
      for (int j = 0; j != c; ++j) {
        w->repl (i,j) = w->repl_dis2;
        w->extra(i,j) = w->extra_dis2;
      }
    }
    
    for (unsigned i = 0; i != data.size(); ++i) {
      const char * d = data[i].d;
      w->repl (w->to_normalized(d[0]),
               w->to_normalized(d[1])) = w->repl_dis1;
      w->repl (w->to_normalized(d[1]),
               w->to_normalized(d[0])) = w->repl_dis1;
      w->extra(w->to_normalized(d[0]),
               w->to_normalized(d[1])) = w->extra_dis1;
      w->extra(w->to_normalized(d[1]),
               w->to_normalized(d[0])) = w->extra_dis1;
    }
    
    for (int i = 0; i != c; ++i) {
      w->repl(i,i) = 0;
      w->extra(i,i) = w->extra_dis1;
    }

    return w;
  }
  

}
