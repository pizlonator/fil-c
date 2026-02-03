
#ifndef ACOMMON_OBJSTACK__HPP
#define ACOMMON_OBJSTACK__HPP

#include "parm_string.hpp"
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

namespace acommon {

class ObjStack
{
  typedef unsigned char byte;
  struct Node
  {
    Node * next;
    byte data[1]; // hack for data[]
  };
  size_t chunk_size;
  size_t min_align;
  Node * first;
  Node * first_free;
  Node * reserve;
  byte * top;
  byte * bottom;
  byte * temp_end;
  void setup_chunk();
  void new_chunk();
  bool will_overflow(size_t sz) const {
    return offsetof(Node,data) + sz > chunk_size;
  }
  void check_size(size_t sz) {
    assert(!will_overflow(sz));
  }

  ObjStack(const ObjStack &);
  void operator=(const ObjStack &);

  void align_bottom(size_t align) {
    size_t a = (size_t)bottom % align;
    if (a != 0) bottom += align - a;
  }
  void align_top(size_t align) {
    top -= (size_t)top % align;
  }
public:
  // The alignment here is the guaranteed alignment that memory in
  // new chunks will be aligned to.   It does NOT guarantee that
  // every object is aligned as such unless all objects inserted
  // are a multiple of align.
  ObjStack(size_t chunk_s = 1024, size_t align = sizeof(void *));
  ~ObjStack();

  size_t calc_size();

  void reset();
  void trim();
  
  // This alloc_bottom does NOT check alignment.  However, if you always
  // insert objects with a multiple of min_align than it will always
  // me aligned as such.
  void * alloc_bottom(size_t size)  {
    byte * tmp = bottom;
    bottom += size;
    if (bottom > top) {check_size(size); new_chunk(); tmp = bottom; bottom += size;}
    return tmp;
  }
  // This alloc_bottom will insure that the object is aligned based on the
  // alignment given.
  void * alloc_bottom(size_t size, size_t align) 
  {loop:
    align_bottom(align);
    byte * tmp = bottom;
    bottom += size;
    if (bottom > top) {check_size(size); new_chunk(); goto loop;}
    return tmp;
  }
  char * dup_bottom(ParmString str) {
    return (char *)memcpy(alloc_bottom(str.size() + 1), 
                          str.str(), str.size() + 1);
  }

  // This alloc_bottom does NOT check alignment.  However, if you
  // always insert objects with a multiple of min_align than it will
  // always be aligned as such.
  void * alloc_top(size_t size) {
    top -= size;
    if (top < bottom) {check_size(size); new_chunk(); top -= size;}
    return top;
  }
  // This alloc_top will insure that the object is aligned based on
  // the alignment given.
  void * alloc_top(size_t size, size_t align) 
  {loop:
    top -= size;
    align_top(align);
    if (top < bottom) {check_size(size); new_chunk(); goto loop;}
    return top;
  }
  char * dup_top(ParmString str) {
    return (char *)memcpy(alloc_top(str.size() + 1), 
                          str.str(), str.size() + 1);
  }

  // By default objects are allocated from the top since that is slightly
  // more efficient
  void * alloc(size_t size) {return alloc_top(size);}
  void * alloc(size_t size, size_t align) {return alloc_top(size,align);}
  char * dup(ParmString str) {return dup_top(str);}

  // alloc_temp allocates an object from the bottom which can be
  // resized until it is committed.  If the resizing will involve
  // moving the object than the data will be copied in the same way
  // realloc does.  Any previously allocated objects are aborted when
  // alloc_temp is called.
  void * temp_ptr() {
    if (temp_end) return bottom;
    else return 0;
  }
  unsigned temp_size() {
    return temp_end - bottom;
  }
  void * alloc_temp(size_t size) {
    temp_end = bottom + size;
    if (temp_end > top) {
      check_size(size);
      new_chunk();
      temp_end = bottom + size;
    }
    return bottom;
  }
  // returns a pointer to the new beginning of the temp memory
  void * resize_temp(size_t size) {
    if (temp_end == 0)
      return alloc_temp(size);
    if (bottom + size <= top) {
      temp_end = bottom + size;
    } else {
      size_t s = temp_end - bottom;
      byte * p = bottom;
      check_size(size);
      new_chunk();
      memcpy(bottom, p, s);
      temp_end = bottom + size;
    }
    return bottom;
  }
  // returns a pointer to the beginning of the new memory (in
  // otherwords the END of the temp memory BEFORE the call to grow
  // temp) NOT the beginning if the temp memory
  void * grow_temp(size_t s) {
    if (temp_end == 0)
      return alloc_temp(s);
    unsigned old_size = temp_end - bottom;
    unsigned size = old_size + s;
    if (bottom + size <= top) {
      temp_end = bottom + size;
    } else {
      size_t s = temp_end - bottom;
      byte * p = bottom;
      check_size(size);
      new_chunk();
      memcpy(bottom, p, s);
      temp_end = bottom + size;
    }
    return bottom + old_size;
  }
  void abort_temp() {
    temp_end = 0;}
  void commit_temp() {
    bottom = temp_end;
    temp_end = 0;}

  typedef Node Memory;
  Memory * freeze();
  static void dealloc(Memory *);
};

typedef ObjStack StringBuffer;

}

#endif
