
#include <cstring>

#include "editdist.hpp"
#include "matrix.hpp"
#include "vararray.hpp"

// edit_distance is implemented using a straight forward dynamic
// programming algorithm with out any special tricks.  Its space
// usage AND running time is tightly asymptotically bounded by
// strlen(a)*strlen(b)

namespace aspeller {

  short edit_distance(ParmString a0, ParmString b0,
		      const EditDistanceWeights & w) 
  {
    int a_size = a0.size() + 1;
    int b_size = b0.size() + 1;
    VARARRAY(short, e_d, a_size * b_size);
    ShortMatrix e(a_size,b_size,e_d);
    e(0, 0) = 0;
    for (int j = 1; j != b_size; ++j)
      e(0, j) = e(0, j-1) + w.del1;
    const char * a = a0.str() - 1;
    const char * b = b0.str() - 1;
    short te;
    for (int i = 1; i != a_size; ++i) {
      e(i, 0) = e(i-1, 0) + w.del2;
      for (int j = 1; j != b_size; ++j) {
	if (a[i] == b[j]) {

	  e(i, j) = e(i-1, j-1);

	} else {

	  e(i, j) = w.sub + e(i-1, j-1);

	  if (i != 1 && j != 1 && 
	      a[i] == b[j-1] && a[i-1] == b[j]) 
	    {
	      te = w.swap + e(i-2, j-2);
	      if (te < e(i, j)) e(i, j) = te;
	    }
	  
	  te = w.del1 + e(i-1, j);
	  if (te < e(i, j)) e(i, j) = te;
	  te = w.del2 + e(i, j-1);
	  if (te < e(i, j)) e(i, j) = te;

	}
      } 
    }
    return e(a_size-1, b_size-1);
  }
}
