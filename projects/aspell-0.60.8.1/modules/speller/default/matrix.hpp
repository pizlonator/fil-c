#ifndef __aspeller_matrix_hh__
#define __aspeller_matrix_hh__

#include <vector>

namespace aspeller {

  class ShortMatrix {
    int x_size;
    int y_size;
    short * data;
  public:
    void init(int sx, int sy, short * d) {x_size = sx; y_size = sy; data = d;}
    ShortMatrix() {}
    ShortMatrix(int sx, int sy, short * d) {init(sx,sy,d);}
    short operator() (int x, int y) const {return data[x + y*x_size];}
    short & operator() (int x, int y) {return data[x + y*x_size];}
  };

}

#endif
