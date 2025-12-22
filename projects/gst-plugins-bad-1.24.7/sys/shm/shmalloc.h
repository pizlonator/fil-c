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

#include <stdlib.h>

#ifndef __SHMALLOC_H__
#define __SHMALLOC_H__

#ifdef SHM_PIPE_USE_GLIB
#include <glib.h>

#define spalloc_new(type) g_new (type,1)
#define spalloc_alloc(size) g_malloc (size)

#define spalloc_free(type, buf) g_free (buf)
#define spalloc_free1(size, buf) g_free (buf)

#else

#define spalloc_new(type) malloc (sizeof (type))
#define spalloc_alloc(size) malloc (size)

#define spalloc_free(type, buf) free (buf)
#define spalloc_free1(size, buf) free (buf)

#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ShmAllocSpace ShmAllocSpace;
typedef struct _ShmAllocBlock ShmAllocBlock;

ShmAllocSpace *shm_alloc_space_new (size_t size);
void shm_alloc_space_free (ShmAllocSpace * self);


ShmAllocBlock *shm_alloc_space_alloc_block (ShmAllocSpace * self,
    unsigned long size);
unsigned long shm_alloc_space_alloc_block_get_offset (ShmAllocBlock *block);

void shm_alloc_space_block_inc (ShmAllocBlock * block);
void shm_alloc_space_block_dec (ShmAllocBlock * block);
ShmAllocBlock * shm_alloc_space_block_get (ShmAllocSpace * space,
    unsigned long offset);


#ifdef __cplusplus
}
#endif

#endif /* __SHMALLOC_H__ */
