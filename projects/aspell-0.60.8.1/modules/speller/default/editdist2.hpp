
#include "leditdist.hpp"
#include "editdist.hpp"

#include <cassert>

namespace aspeller {
  inline int edit_distance(ParmString a, ParmString b, 
			   int level, // starting level
			   int limit, // maximum level
			   const EditDistanceWeights & w 
			   = EditDistanceWeights()) 
  {
    int score;
    assert(level > 0  && limit >= level);
    do {
      if (level == 2) {
	score = limit2_edit_distance(a,b,w);
      } else if (level < 5) {
	score = limit_edit_distance(a,b,level,w);
      } else {
	score = edit_distance(a,b,w);
      }
      ++level;
    } while (score >= LARGE_NUM && level <= limit);
    return score;
  }
}
