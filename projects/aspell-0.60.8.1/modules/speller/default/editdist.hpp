#ifndef __aspeller_edit_distance_hh__
#define __aspeller_edit_distance_hh__

#include "parm_string.hpp"
#include "weights.hpp"

namespace aspeller {

  using acommon::ParmString;

  // edit_distance finds the shortest edit distance.  The edit distance is 
  // (cost of swap)(# of swaps) + (cost of deletion)(# of deletions) 
  //   + (cost of insertion)(# of insertions) 
  //   + (cost of substitutions)(# of substitutions)

  // Preconditions:
  // max(strlen(a), strlen(b))*max(of the edit weights) <= 2^15
  //   if violated than an incorrect result may be returned (which may be negative)
  //   due to overflow of a short integer
  // a,b are not null pointers
  // Returns:
  //   the edit distance between a and b

  // the running time is tightly asymptotically bounded by strlen(a)*strlen(b)

  short edit_distance(ParmString a, ParmString b,
		      const EditDistanceWeights & w = EditDistanceWeights());
}

#endif
