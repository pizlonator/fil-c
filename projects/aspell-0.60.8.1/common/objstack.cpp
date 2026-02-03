
#include "objstack.hpp"

namespace acommon {

void ObjStack::setup_chunk()
{
  bottom = first_free->data;
  align_bottom(min_align);
  top    = (byte *)first_free + chunk_size;
  align_top(min_align);
}


ObjStack::ObjStack(size_t chunk_s, size_t align)
  : chunk_size(chunk_s), min_align(align), temp_end(0)
{
  first_free = first = (Node *)malloc(chunk_size);
  first->next = 0;
  reserve = 0;
  setup_chunk();
}

ObjStack::~ObjStack()
{
  while (first) {
    Node * tmp = first->next;
    free(first);
    first = tmp;
  }
  trim();
}

size_t ObjStack::calc_size()
{
  size_t size = 0;
  for (Node * p = first; p; p = p->next)
    size += chunk_size;
  return size;
}

void ObjStack::new_chunk()
{
  if (reserve) {
    first_free->next = reserve;
    reserve = reserve->next;
    first_free = first_free->next;
    first_free->next = 0;
  } else {
    first_free->next = (Node *)malloc(chunk_size);
    first_free = first_free->next;
  }
  first_free->next = 0;
  setup_chunk();
}

void ObjStack::reset()
{
  assert(first_free->next == 0);
  if (first->next) {
    first_free->next = reserve;
    reserve = first->next;
    first->next = 0;
  } 
  first_free = first;
  setup_chunk();
}

void ObjStack::trim()
{
  while (reserve) {
    Node * tmp = reserve->next;
    free(reserve);
    reserve = tmp;
  }
}

ObjStack::Memory * ObjStack::freeze() {
  trim();
  Node * tmp = first;
  first = 0;
  first_free = 0;
  top = 0;
  bottom = 0;
  temp_end = 0;
  return tmp;
}
  
void ObjStack::dealloc(Memory * ptr) {
  while (ptr) {
    Node * tmp = ptr->next;
    free(ptr);
    ptr = tmp;
  }
}

}
