#include "vqueue.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
struct __vqueue_t
{
  cmph_uint32 * values;
  cmph_uint32 beg, end, capacity;
};

vqueue_t * vqueue_new(cmph_uint32 capacity)
{
  size_t capacity_plus_one = capacity + 1;
  vqueue_t *q = (vqueue_t *)malloc(sizeof(vqueue_t));
  assert(q);
  q->values = (cmph_uint32 *)calloc(capacity_plus_one, sizeof(cmph_uint32));
  q->beg = q->end = 0;
  q->capacity = (cmph_uint32) capacity_plus_one;
  return q;
}

cmph_uint8 vqueue_is_empty(vqueue_t * q)
{
  return (cmph_uint8)(q->beg == q->end);
}

void vqueue_insert(vqueue_t * q, cmph_uint32 val)
{
  assert((q->end + 1)%q->capacity != q->beg); // Is queue full?
  q->end = (q->end + 1)%q->capacity;
  q->values[q->end] = val;
}

cmph_uint32 vqueue_remove(vqueue_t * q)
{
  assert(!vqueue_is_empty(q)); // Is queue empty?
  q->beg = (q->beg + 1)%q->capacity;
  return q->values[q->beg];
}

void vqueue_print(vqueue_t * q)
{
  cmph_uint32 i;
  for (i = q->beg; i != q->end; i = (i + 1)%q->capacity)
    fprintf(stderr, "%u\n", q->values[(i + 1)%q->capacity]);
} 

void vqueue_destroy(vqueue_t *q)
{
  free(q->values); q->values = NULL; free(q);
}
