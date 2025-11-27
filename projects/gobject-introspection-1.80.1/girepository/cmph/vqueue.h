#ifndef __CMPH_VQUEUE_H__
#define __CMPH_VQUEUE_H__

#include "cmph_types.h"
typedef struct __vqueue_t vqueue_t;

vqueue_t * vqueue_new(cmph_uint32 capacity);

cmph_uint8 vqueue_is_empty(vqueue_t * q);

void vqueue_insert(vqueue_t * q, cmph_uint32 val);

cmph_uint32 vqueue_remove(vqueue_t * q);

void vqueue_print(vqueue_t * q);

void vqueue_destroy(vqueue_t * q);
#endif
