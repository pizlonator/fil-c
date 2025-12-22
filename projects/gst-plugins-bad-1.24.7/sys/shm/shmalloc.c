/* GStreamer
 * Copyright (C) <2009> Collabora Ltd
 *  @author: Olivier Crete <olivier.crete@collabora.co.uk
 * Copyright (C) <2009> Nokia Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "shmalloc.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* This is the allocated space to hold multiple blocks */
struct _ShmAllocSpace
{
  /* The total size of this space */
  size_t size;

  /* chained list of the blocks contained in this space */
  ShmAllocBlock *blocks;
};

/* A single block of data */
struct _ShmAllocBlock
{
  int use_count;

  /* Pointer back to the AllocSpace where this block is */
  ShmAllocSpace *space;

  /* The offset of this block in the alloc space */
  unsigned long offset;
  /* The size of the block */
  unsigned long size;

  /* Pointer to the next block in the chain */
  ShmAllocBlock *next;
};


ShmAllocSpace *
shm_alloc_space_new (size_t size)
{
  ShmAllocSpace *self = spalloc_new (ShmAllocSpace);

  memset (self, 0, sizeof (ShmAllocSpace));

  self->size = size;

  return self;
}

void
shm_alloc_space_free (ShmAllocSpace * self)
{
  assert (self && self->blocks == NULL);
  spalloc_free (ShmAllocSpace, self);
}


ShmAllocBlock *
shm_alloc_space_alloc_block (ShmAllocSpace * self, unsigned long size)
{
  ShmAllocBlock *block;
  ShmAllocBlock *item = NULL;
  ShmAllocBlock *prev_item = NULL;
  unsigned long prev_end_offset = 0;


  for (item = self->blocks; item; item = item->next) {
    unsigned long max_size = 0;

    max_size = item->offset - prev_end_offset;

    if (max_size >= size)
      break;

    prev_end_offset = item->offset + item->size;
    prev_item = item;
  }

  /* Return NULL if there is no big enough space, otherwise, there is space
   * at the end */
  assert (prev_end_offset <= self->size);
  if (!item && self->size - prev_end_offset < size)
    return NULL;

  block = spalloc_new (ShmAllocBlock);
  memset (block, 0, sizeof (ShmAllocBlock));
  block->offset = prev_end_offset;
  block->size = size;
  block->use_count = 1;
  block->space = self;

  if (prev_item)
    prev_item->next = block;
  else
    self->blocks = block;

  block->next = item;

  return block;
}

unsigned long
shm_alloc_space_alloc_block_get_offset (ShmAllocBlock * block)
{
  return block->offset;
}

static void
shm_alloc_space_free_block (ShmAllocBlock * block)
{
  ShmAllocBlock *item = NULL;
  ShmAllocBlock *prev_item = NULL;
  ShmAllocSpace *self = block->space;

  for (item = self->blocks; item; item = item->next) {
    if (item == block) {
      if (prev_item)
        prev_item->next = item->next;
      else
        self->blocks = item->next;
      break;
    }
    prev_item = item;
  }

  spalloc_free (ShmAllocBlock, block);
}

ShmAllocBlock *
shm_alloc_space_block_get (ShmAllocSpace * self, unsigned long offset)
{
  ShmAllocBlock *block = NULL;

  for (block = self->blocks; block; block = block->next) {
    if (block->offset <= offset && (block->offset + block->size) > offset)
      return block;
  }

  return NULL;
}


void
shm_alloc_space_block_inc (ShmAllocBlock * block)
{
  block->use_count++;
}

void
shm_alloc_space_block_dec (ShmAllocBlock * block)
{
  block->use_count--;

  if (block->use_count <= 0)
    shm_alloc_space_free_block (block);
}
