
#include "leditdist.hpp"

// The basic algorithm is as follows:
//
// Let A[n] represent the nth character of string n
//     A[n..] represent the substring of A starting at n
//            if n > length of A then it is considered an empty string
//
// edit_distance(A,B,limit) = ed(A,B,0)
//   where ed(A,B,d) = d                              if A & B is empty.
//                   = infinity                       if d > limit
//                   = ed(A[2..],B[2..], d)           if A[1] == B[1] 
//                   = min ( ed(A[2..],B[2..], d+1),  
//                           ed(A,     B[2..], d+1),
//                           ed(A[2..],B,      d+1) ) otherwise
//
// However, the code below:
// 1) Also allows for swaps
// 2) Allow weights to be attached to each edit
// 3) Is not recursive, it uses a loop when it is tail recursion
//    and a small stack otherwise.  The stack will NEVER be larger
//    then 2 * limit.
// 4) Is extremely optimized


#define check_rest(a,b,s)   \
  a0 = a; b0 = b;           \
  while (*a0 == *b0) {      \
    if (*a0 == '\0') {      \
      if (s < min) min = s; \
      break;                \
    }                       \
    ++a0; ++b0;             \
  }

namespace aspeller {

  int limit_edit_distance(const char * a, const char * b, 
			  int limit, const EditDistanceWeights & w)
  {
    limit = limit*w.max;
    static const int size = 10;
    struct Edit {
      const char * a;
      const char * b;
      int score;
    };
    Edit begin[size];
    Edit * i = begin;
    const char * a0;
    const char * b0;
    int score = 0;
    int min = LARGE_NUM;
    
    while (true) {
      
      while (*a == *b) {
	if (*a == '\0') { 
	  if (score < min) min = score;
	  goto FINISH;
	} 
	++a; ++b;
      }

      if (*a == '\0') {

	do {
	  score += w.del2;
	  if (score >= min) goto FINISH;
	  ++b;
	} while (*b != '\0');
	min = score;
	
      } else if (*b == '\0') {
	
	do {
	  score += w.del1;
	  if (score >= min) goto FINISH;
	  ++a;
	} while (*a != '\0');
	min = score;
	
      } else {

	if (score + w.max <= limit) {
	  if (limit*w.min <= w.max*(w.min+score)) {
	    // if floor(score/max)=limit/max-1 then this edit is only good
	    // if it makes the rest of the string match.  So check if
	    // the rest of the string matches to avoid the overhead of
	    // pushing it on then off the stack
	    
	    // delete a character from a
	    check_rest(a+1,b,score + w.del1);
	    
	    // delete a character from b
	    check_rest(a,b+1,score + w.del2);
	    
	    if (*a == *(b+1) && *b == *(a+1)) {

	      // swap two characters
	      check_rest(a+2,b+2, score + w.swap);

	    } else {

	      // substitute one character for another which is the same
	      // thing as deleting a character from both a & b
	      check_rest(a+1,b+1, score + w.sub);
	    
	    }
	  
	  } else {
	  
	    // delete a character from a
	    i->a = a + 1;
	    i->b = b;
	    i->score = score + w.del1;
	    ++i;
	  
	    // delete a character from b
	    i->a = a;
	    i->b = b + 1;
	    i->score = score + w.del2;
	    ++i;
	    
	    // If two characters can be swapped and make a match 
	    // then the substitution is pointless.
	    // Also, there is no need to push this on the stack as
	    // it is going to be imminently removed.
	    if (*a == *(b+1) && *b == *(a+1)) {
	      
	      // swap two characters
	      a = a + 2;
	      b = b + 2;
	      score += w.swap;
	      continue;
	      
	    } else {
	      
	      // substitute one character for another which is the same
	      // thing as deleting a character from both a & b
	      a = a + 1;
	      b = b + 1;
	      score += w.sub;
	      continue;
	    
	    }
	  }
	}
      }
    FINISH:
      if (i == begin) return min;
      --i;
      a = i->a;
      b = i->b;
      score = i->score;
    }
  }

#undef check_rest
#define check_rest(a,b,w)               \
          a0 = a; b0 = b;               \
          while(*a0 == *b0) {           \
	    if (*a0 == '\0') {          \
	      if (w < min) min = w;     \
	      break;                    \
	    }                           \
	    ++a0;                       \
	    ++b0;                       \
          }                             \
          if (amax < a0) amax = a0;

#define check2(a,b,w)                                             \
  aa = a; bb = b;                                                 \
  while(*aa == *bb) {                                             \
    if (*aa == '\0')  {                                           \
      if (amax < aa) amax = aa;                                   \
      if (w < min) min = w;                                       \
      break;                                                      \
    }                                                             \
    ++aa; ++bb;                                                   \
  }                                                               \
  if (*aa == '\0') {                                              \
    if (amax < aa) amax = aa;                                     \
    if (*bb == '\0') {}                                           \
    else if (*(bb+1) == '\0' && w+ws.del2 < min) min = w+ws.del2; \
  } else if (*bb == '\0') {                                       \
    ++aa;                                                         \
    if (amax < aa) amax = aa;                                     \
    if (*aa == '\0' && w+ws.del1 < min) min = w+ws.del1;          \
  } else {                                                        \
    check_rest(aa+1,bb,w+ws.del1);                                \
    check_rest(aa,bb+1,w+ws.del2);                                \
    if (*aa == *(bb+1) && *bb == *(aa+1)) {                       \
      check_rest(aa+2,bb+2,w+ws.swap);                            \
    } else {                                                      \
      check_rest(aa+1,bb+1,w+ws.sub);                             \
    }                                                             \
  }

  EditDist limit0_edit_distance(const char * a, const char * b,
				const EditDistanceWeights & ws)
  {
    while(*a == *b) {
      if (*a == '\0')
	return EditDist(0, a);
      ++a; ++b;
    }

    return EditDist(LARGE_NUM, a);
  }

  EditDist limit1_edit_distance(const char * a, const char * b,
				const EditDistanceWeights & ws)
  {
    int min = LARGE_NUM;
    const char * a0;
    const char * b0;
    const char * amax = a;
    
    while(*a == *b) { 
      if (*a == '\0') 
	return EditDist(0, a);
      ++a; ++b;
    }

    if (*a == '\0') {
      
      ++b;
      if (*b == '\0') return EditDist(ws.del2, a);
      return EditDist(LARGE_NUM, a);
      
    } else if (*b == '\0') {

      ++a;
      if (*a == '\0') return EditDist(ws.del1, a);
      return EditDist(LARGE_NUM, a);
      
    } else {
      
      // delete a character from a
      check_rest(a+1,b,ws.del1);
      
      // delete a character from b
      check_rest(a,b+1,ws.del2);

      if (*a == *(b+1) && *b == *(a+1)) {
	
	// swap two characters
	check_rest(a+2,b+2,ws.swap);

      } else {
	
	// substitute one character for another which is the same
	// thing as deleting a character from both a & b
	check_rest(a+1,b+1,ws.sub);
	
      }
    }
    return EditDist(min, amax);
  }

  EditDist limit2_edit_distance(const char * a, const char * b,
				const EditDistanceWeights & ws)
  {
    int min = LARGE_NUM;
    const char * a0;
    const char * b0;
    const char * aa;
    const char * bb;
    const char * amax = a;
    
    while(*a == *b) { 
      if (*a == '\0') 
	return EditDist(0, a);
      ++a; ++b;
    }

    if (*a == '\0') {
      
      ++b;
      if (*b == '\0') return EditDist(ws.del2, a);
      ++b;
      if (*b == '\0') return EditDist(2*ws.del2, a);
      return EditDist(LARGE_NUM, a);
      
    } else if (*b == '\0') {

      ++a;
      if (*a == '\0') return EditDist(ws.del1, a);
      ++a;
      if (*a == '\0') return EditDist(2*ws.del1, a);
      return EditDist(LARGE_NUM, a);
      
    } else {
      
      // delete a character from a
      check2(a+1,b,ws.del1);
      
      // delete a character from b
      check2(a,b+1,ws.del2);

      if (*a == *(b+1) && *b == *(a+1)) {
	
	// swap two characters
	check2(a+2,b+2,ws.swap);

      } else {
	
	// substitute one character for another which is the same
	// thing as deleting a character from both a & b
	check2(a+1,b+1,ws.sub);
	
      }
    }
    return EditDist(min, amax);
  }
}


