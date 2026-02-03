
#ifndef __aspeller_weights_hh__
#define __aspeller_weights_hh__

namespace aspeller {

  struct EditDistanceWeights {
    int del1;    // the cost of deleting a char in the first string
    int del2;    // the cost of inserting a character or deleting a char
                    // in the second string
    int swap;    // the cost of swapping two adjacent letters
    int sub;     // the cost of replacing one letter with another
    int similar; // the cost of a "similar" but not exact match for
                    // two characters
    int min;     // the min of del1, del2, swap and sub.
    int max;     // the max of del1, del2, swap and sub.
    EditDistanceWeights()
      : del1(1), del2(1), swap(1), sub(1), similar(0), min(1), max(1) {}
  };
  
}

#endif
