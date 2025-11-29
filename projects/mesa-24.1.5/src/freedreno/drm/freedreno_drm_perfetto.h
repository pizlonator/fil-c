/*
 * Copyright © 2024 Google, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FREEDRENO_DRM_PERFETTO_H_
#define FREEDRENO_DRM_PERFETTO_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Memory events are logged as transition between allocation categories.
 * Ie. a new allocation from kernel would be NONE -> ACTIVE, while a
 * freed buffer going to the BO cache would be ACTIVE -> CACHE, and then
 * if it is eventually freed from the cache, CACHE -> NONE.
 */
enum fd_alloc_category {
   FD_ALLOC_NONE,       /* freed / not allocated */
   FD_ALLOC_HEAP,       /* unused bo heap memory */
   FD_ALLOC_CACHE,      /* unused bo cache memory */
   FD_ALLOC_ACTIVE,     /* actively used */
};

struct fd_bo;

#ifdef HAVE_PERFETTO
void fd_drm_perfetto_init(void);
void fd_alloc_log(struct fd_bo *bo, enum fd_alloc_category from, enum fd_alloc_category to);
#else
static inline void
fd_drm_perfetto_init(void)
{
}
static inline void
fd_alloc_log(struct fd_bo *bo, enum fd_alloc_category from, enum fd_alloc_category to)
{
}
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif /* FREEDRENO_DRM_PERFETTO_H_ */
