/* GStreamer
 * Copyright (C) <2016> Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

static inline void
inner_product_gint16_full_1_neon (gint16 * o, const gint16 * a,
    const gint16 * b, gint len, const gint16 * icoeff, gint bstride)
{
    uint32_t remainder = len % 16;
    len = len - remainder;

    asm volatile ("      vmov.s32 q0, #0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vmov.s32 q1, #0\n"
                  "1:"
                  "      vld1.16 {d16, d17, d18, d19}, [%[b]]!\n"
                  "      vld1.16 {d20, d21, d22, d23}, [%[a]]!\n"
                  "      subs %[len], %[len], #16\n"
                  "      vmlal.s16 q0, d16, d20\n"
                  "      vmlal.s16 q1, d17, d21\n"
                  "      vmlal.s16 q0, d18, d22\n"
                  "      vmlal.s16 q1, d19, d23\n"
                  "      bne 1b\n"
                  "      vadd.s32 q0, q0, q1\n"
                  "2:"
                  "      cmp %[remainder], #0\n"
                  "      beq 4f\n"
                  "3:"
                  "      vld1.16 {d16}, [%[b]]!\n"
                  "      vld1.16 {d20}, [%[a]]!\n"
                  "      subs %[remainder], %[remainder], #4\n"
                  "      vmlal.s16 q0, d16, d20\n"
                  "      bgt 3b\n"
                  "4:"
                  "      vadd.s32 d0, d0, d1\n"
                  "      vpadd.s32 d0, d0, d0\n"
                  "      vqrshrn.s32 d0, q0, #15\n"
                  "      vst1.16 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [b] "+r" (b),
                    [len] "+r" (len), [remainder] "+r" (remainder)
                  : [o] "r" (o)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19",
                    "d20", "d21", "d22", "d23");
}

static inline void
inner_product_gint16_linear_1_neon (gint16 * o, const gint16 * a,
    const gint16 * b, gint len, const gint16 * icoeff, gint bstride)
{
    uint32_t remainder = len % 16;
    const gint16 *c[2] = {(gint16*)((gint8*)b + 0*bstride),
                          (gint16*)((gint8*)b + 1*bstride)};
    len = len - remainder;

    asm volatile ("      vmov.s16 q0, #0\n"
                  "      vmov.s16 q1, #0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "1:"
                  "      vld1.16 {d16, d17, d18, d19}, [%[c0]]!\n"
                  "      vld1.16 {d20, d21, d22, d23}, [%[c1]]!\n"
                  "      vld1.16 {d24, d25, d26, d27}, [%[a]]!\n"
                  "      subs %[len], %[len], #16\n"
                  "      vmlal.s16 q0, d16, d24\n"
                  "      vmlal.s16 q1, d20, d24\n"
                  "      vmlal.s16 q0, d17, d25\n"
                  "      vmlal.s16 q1, d21, d25\n"
                  "      vmlal.s16 q0, d18, d26\n"
                  "      vmlal.s16 q1, d22, d26\n"
                  "      vmlal.s16 q0, d19, d27\n"
                  "      vmlal.s16 q1, d23, d27\n"
                  "      bne 1b\n"
                  "2:"
                  "      cmp %[remainder], #0\n"
                  "      beq 4f\n"
                  "3:"
                  "      vld1.16 {d16}, [%[c0]]!\n"
                  "      vld1.16 {d20}, [%[c1]]!\n"
                  "      vld1.16 {d24}, [%[a]]!\n"
                  "      subs %[remainder], %[remainder], #4\n"
                  "      vmlal.s16 q0, d16, d24\n"
                  "      vmlal.s16 q1, d20, d24\n"
                  "      bgt 3b\n"
                  "4:"
                  "      vld2.16 {d20[], d21[]}, [%[ic]]\n"
                  "      vshrn.s32 d0, q0, #15\n"
                  "      vshrn.s32 d2, q1, #15\n"
                  "      vmull.s16 q0, d0, d20\n"
                  "      vmlal.s16 q0, d2, d21\n"
                  "      vadd.s32 d0, d0, d1\n"
                  "      vpadd.s32 d0, d0, d0\n"
                  "      vqrshrn.s32 d0, q0, #15\n"
                  "      vst1.16 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [len] "+r" (len), [remainder] "+r" (remainder)
                  : [o] "r" (o), [ic] "r" (icoeff)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19",
                    "d20", "d21", "d22", "d23",
                    "d24", "d25", "d26", "d27", "memory");
}

static inline void
inner_product_gint16_cubic_1_neon (gint16 * o, const gint16 * a,
    const gint16 * b, gint len, const gint16 * icoeff, gint bstride)
{
    const gint16 *c[4] = {(gint16*)((gint8*)b + 0*bstride),
                          (gint16*)((gint8*)b + 1*bstride),
                          (gint16*)((gint8*)b + 2*bstride),
                          (gint16*)((gint8*)b + 3*bstride)};

    asm volatile ("      vmov.s32 q0, #0\n"
                  "      vmov.s32 q1, #0\n"
                  "      vmov.s32 q2, #0\n"
                  "      vmov.s32 q3, #0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "1:"
                  "      vld1.16 {d16, d17}, [%[c0]]!\n"
                  "      vld1.16 {d18, d19}, [%[c1]]!\n"
                  "      vld1.16 {d20, d21}, [%[c2]]!\n"
                  "      vld1.16 {d22, d23}, [%[c3]]!\n"
                  "      vld1.16 {d24, d25}, [%[a]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmlal.s16 q0, d16, d24\n"
                  "      vmlal.s16 q1, d18, d24\n"
                  "      vmlal.s16 q2, d20, d24\n"
                  "      vmlal.s16 q3, d22, d24\n"
                  "      vmlal.s16 q0, d17, d25\n"
                  "      vmlal.s16 q1, d19, d25\n"
                  "      vmlal.s16 q2, d21, d25\n"
                  "      vmlal.s16 q3, d23, d25\n"
                  "      bne 1b\n"
                  "2:"
                  "      vld4.16 {d20[], d21[], d22[], d23[]}, [%[ic]]\n"
                  "      vshrn.s32 d0, q0, #15\n"
                  "      vshrn.s32 d2, q1, #15\n"
                  "      vshrn.s32 d4, q2, #15\n"
                  "      vshrn.s32 d6, q3, #15\n"
                  "      vmull.s16 q0, d0, d20\n"
                  "      vmlal.s16 q0, d2, d21\n"
                  "      vmlal.s16 q0, d4, d22\n"
                  "      vmlal.s16 q0, d6, d23\n"
                  "      vadd.s32 d0, d0, d1\n"
                  "      vpadd.s32 d0, d0, d0\n"
                  "      vqrshrn.s32 d0, q0, #15\n"
                  "      vst1.16 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [c2] "+r" (c[2]), [c3] "+r" (c[3]), [len] "+r" (len)
                  : [o] "r" (o), [ic] "r" (icoeff)
                  : "cc", "q0", "q1", "q2", "q3",
                    "d16", "d17", "d18", "d19",
                    "d20", "d21", "d22", "d23",
                    "d24", "d25", "memory");
}

static inline void
interpolate_gint16_linear_neon (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
    gint16 *o = op, *a = ap, *ic = icp;
    const gint16 *c[2] = {(gint16*)((gint8*)a + 0*astride),
                          (gint16*)((gint8*)a + 1*astride)};

    asm volatile ("      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vld2.16 {d20[], d21[]}, [%[ic]]\n"
                  "1:"
                  "      vld1.16 {d16, d17}, [%[c0]]!\n"
                  "      vld1.16 {d18, d19}, [%[c1]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmull.s16 q0, d16, d20\n"
                  "      vmull.s16 q1, d17, d20\n"
                  "      vmlal.s16 q0, d18, d21\n"
                  "      vmlal.s16 q1, d19, d21\n"
                  "      vqrshrn.s32 d0, q0, #15\n"
                  "      vqrshrn.s32 d1, q1, #15\n"
                  "      vst1.16 {d0, d1}, [%[o]]!\n"
                  "      bne 1b\n"
                  "2:"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (ic)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19", "d20", "d21", "memory");
}

static inline void
interpolate_gint16_cubic_neon (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
    gint16 *o = op, *a = ap, *ic = icp;
    const gint16 *c[4] = {(gint16*)((gint8*)a + 0*astride),
                          (gint16*)((gint8*)a + 1*astride),
                          (gint16*)((gint8*)a + 2*astride),
                          (gint16*)((gint8*)a + 3*astride)};

    asm volatile ("      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vld4.16 {d24[], d25[], d26[], d27[]}, [%[ic]]\n"
                  "1:"
                  "      vld1.16 {d16, d17}, [%[c0]]!\n"
                  "      vld1.16 {d18, d19}, [%[c1]]!\n"
                  "      vld1.16 {d20, d21}, [%[c2]]!\n"
                  "      vld1.16 {d22, d23}, [%[c3]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmull.s16 q0, d16, d24\n"
                  "      vmull.s16 q1, d17, d24\n"
                  "      vmlal.s16 q0, d18, d25\n"
                  "      vmlal.s16 q1, d19, d25\n"
                  "      vmlal.s16 q0, d20, d26\n"
                  "      vmlal.s16 q1, d21, d26\n"
                  "      vmlal.s16 q0, d22, d27\n"
                  "      vmlal.s16 q1, d23, d27\n"
                  "      vqrshrn.s32 d0, q0, #15\n"
                  "      vqrshrn.s32 d1, q1, #15\n"
                  "      vst1.16 {d0, d1}, [%[o]]!\n"
                  "      bne 1b\n"
                  "2:"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]), [c2] "+r" (c[2]), [c3] "+r" (c[3]),
                    [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (ic)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19", "d20", "d21", "d22",
                    "d23", "d24", "d25", "d26", "d27", "memory");
}

static inline void
inner_product_gint32_full_1_neon (gint32 * o, const gint32 * a,
    const gint32 * b, gint len, const gint32 * icoeff, gint bstride)
{
    uint32_t remainder = len % 8;
    len = len - remainder;

    asm volatile ("      vmov.s64 q0, #0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vmov.s64 q1, #0\n"
                  "1:"
                  "      vld1.32 {d16, d17, d18, d19}, [%[b]]!\n"
                  "      vld1.32 {d20, d21, d22, d23}, [%[a]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmlal.s32 q0, d16, d20\n"
                  "      vmlal.s32 q1, d17, d21\n"
                  "      vmlal.s32 q0, d18, d22\n"
                  "      vmlal.s32 q1, d19, d23\n"
                  "      bne 1b\n"
                  "      vadd.s64 q0, q0, q1\n"
                  "2:"
                  "      cmp %[remainder], #0\n"
                  "      beq 4f\n"
                  "3:"
                  "      vld1.32 {d16, d17}, [%[b]]!\n"
                  "      vld1.32 {d20, d21}, [%[a]]!\n"
                  "      subs %[remainder], %[remainder], #4\n"
                  "      vmlal.s32 q0, d16, d20\n"
                  "      vmlal.s32 q0, d17, d21\n"
                  "      bgt 3b\n"
                  "4:"
                  "      vadd.s64 d0, d0, d1\n"
                  "      vqrshrn.s64 d0, q0, #31\n"
                  "      vst1.32 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [b] "+r" (b),
                    [len] "+r" (len), [remainder] "+r" (remainder)
                  : [o] "r" (o)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19",
                    "d20", "d21", "d22", "d23");
}

static inline void
inner_product_gint32_linear_1_neon (gint32 * o, const gint32 * a,
    const gint32 * b, gint len, const gint32 * icoeff, gint bstride)
{
    const gint32 *c[2] = {(gint32*)((gint8*)b + 0*bstride),
                          (gint32*)((gint8*)b + 1*bstride)};

    asm volatile ("      vmov.s64 q0, #0\n"
                  "      vmov.s64 q1, #0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "1:"
                  "      vld1.32 {d16, d17, d18, d19}, [%[c0]]!\n"
                  "      vld1.32 {d20, d21, d22, d23}, [%[c1]]!\n"
                  "      vld1.32 {d24, d25, d26, d27}, [%[a]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmlal.s32 q0, d16, d24\n"
                  "      vmlal.s32 q1, d20, d24\n"
                  "      vmlal.s32 q0, d17, d25\n"
                  "      vmlal.s32 q1, d21, d25\n"
                  "      vmlal.s32 q0, d18, d26\n"
                  "      vmlal.s32 q1, d22, d26\n"
                  "      vmlal.s32 q0, d19, d27\n"
                  "      vmlal.s32 q1, d23, d27\n"
                  "      bne 1b\n"
                  "2:"
                  "      vld2.32 {d20[], d21[]}, [%[ic]]\n"
                  "      vshrn.s64 d0, q0, #31\n"
                  "      vshrn.s64 d2, q1, #31\n"
                  "      vmull.s32 q0, d0, d20\n"
                  "      vmlal.s32 q0, d2, d21\n"
                  "      vadd.s64 d0, d0, d1\n"
                  "      vqrshrn.s64 d0, q0, #31\n"
                  "      vst1.32 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [len] "+r" (len)
                  : [o] "r" (o), [ic] "r" (icoeff)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19",
                    "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "memory");
}

static inline void
inner_product_gint32_cubic_1_neon (gint32 * o, const gint32 * a,
    const gint32 * b, gint len, const gint32 * icoeff, gint bstride)
{
    const gint32 *c[4] = {(gint32*)((gint8*)b + 0*bstride),
                          (gint32*)((gint8*)b + 1*bstride),
                          (gint32*)((gint8*)b + 2*bstride),
                          (gint32*)((gint8*)b + 3*bstride)};

    asm volatile ("      vmov.s64 q0, #0\n"
                  "      vmov.s64 q1, #0\n"
                  "      vmov.s64 q2, #0\n"
                  "      vmov.s64 q3, #0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "1:"
                  "      vld1.32 {d16, d17}, [%[c0]]!\n"
                  "      vld1.32 {d18, d19}, [%[c1]]!\n"
                  "      vld1.32 {d20, d21}, [%[c2]]!\n"
                  "      vld1.32 {d22, d23}, [%[c3]]!\n"
                  "      vld1.32 {d24, d25}, [%[a]]!\n"
                  "      subs %[len], %[len], #4\n"
                  "      vmlal.s32 q0, d16, d24\n"
                  "      vmlal.s32 q1, d18, d24\n"
                  "      vmlal.s32 q2, d20, d24\n"
                  "      vmlal.s32 q3, d22, d24\n"
                  "      vmlal.s32 q0, d17, d25\n"
                  "      vmlal.s32 q1, d19, d25\n"
                  "      vmlal.s32 q2, d21, d25\n"
                  "      vmlal.s32 q3, d23, d25\n"
                  "      bne 1b\n"
                  "2:"
                  "      vld4.32 {d20[], d21[], d22[], d23[]}, [%[ic]]\n"
                  "      vshrn.s64 d0, q0, #31\n"
                  "      vshrn.s64 d2, q1, #31\n"
                  "      vshrn.s64 d4, q2, #31\n"
                  "      vshrn.s64 d6, q3, #31\n"
                  "      vmull.s32 q0, d0, d20\n"
                  "      vmlal.s32 q0, d2, d21\n"
                  "      vmlal.s32 q0, d4, d22\n"
                  "      vmlal.s32 q0, d6, d23\n"
                  "      vadd.s64 d0, d0, d1\n"
                  "      vqrshrn.s64 d0, q0, #31\n"
                  "      vst1.32 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [c2] "+r" (c[2]), [c3] "+r" (c[3]), [len] "+r" (len)
                  : [o] "r" (o), [ic] "r" (icoeff)
                  : "cc", "q0", "q1", "q2", "q3",
                    "d16", "d17", "d18", "d19",
                    "d20", "d21", "d22", "d23", "d24", "d25", "memory");
}

static inline void
interpolate_gint32_linear_neon (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
    gint32 *o = op, *a = ap, *ic = icp;
    const gint32 *c[2] = {(gint32*)((gint8*)a + 0*astride),
                          (gint32*)((gint8*)a + 1*astride)};

    asm volatile ("      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vld2.32 {d24[], d25[]}, [%[ic]]!\n"
                  "1:"
                  "      vld1.32 {d16, d17, d18, d19}, [%[c0]]!\n"
                  "      vld1.32 {d20, d21, d22, d23}, [%[c1]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmull.s32 q0, d16, d24\n"
                  "      vmull.s32 q1, d17, d24\n"
                  "      vmull.s32 q2, d18, d24\n"
                  "      vmull.s32 q3, d19, d24\n"
                  "      vmlal.s32 q0, d20, d25\n"
                  "      vmlal.s32 q1, d21, d25\n"
                  "      vmlal.s32 q2, d22, d25\n"
                  "      vmlal.s32 q3, d23, d25\n"
                  "      vqrshrn.s64 d0, q0, #31\n"
                  "      vqrshrn.s64 d1, q1, #31\n"
                  "      vqrshrn.s64 d2, q2, #31\n"
                  "      vqrshrn.s64 d3, q3, #31\n"
                  "      vst1.32 {d0, d1, d2, d3}, [%[o]]!\n"
                  "      bne 1b\n"
                  "2:"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (ic)
                  : "cc", "q0", "q1", "q2", "q3",
                    "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "memory");
}

static inline void
interpolate_gint32_cubic_neon (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
    gint32 *o = op, *a = ap, *ic = icp;
    const gint32 *c[4] = {(gint32*)((gint8*)a + 0*astride),
                          (gint32*)((gint8*)a + 1*astride),
                          (gint32*)((gint8*)a + 2*astride),
                          (gint32*)((gint8*)a + 3*astride)};

    asm volatile ("      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vld4.32 {d24[], d25[], d26[], d27[]}, [%[ic]]!\n"
                  "1:"
                  "      vld1.32 {d16, d17}, [%[c0]]!\n"
                  "      vld1.32 {d18, d19}, [%[c1]]!\n"
                  "      vld1.32 {d20, d21}, [%[c2]]!\n"
                  "      vld1.32 {d22, d23}, [%[c3]]!\n"
                  "      subs %[len], %[len], #4\n"
                  "      vmull.s32 q0, d16, d24\n"
                  "      vmull.s32 q1, d17, d24\n"
                  "      vmlal.s32 q0, d18, d25\n"
                  "      vmlal.s32 q1, d19, d25\n"
                  "      vmlal.s32 q0, d20, d26\n"
                  "      vmlal.s32 q1, d21, d26\n"
                  "      vmlal.s32 q0, d22, d27\n"
                  "      vmlal.s32 q1, d23, d27\n"
                  "      vqrshrn.s64 d0, q0, #31\n"
                  "      vqrshrn.s64 d1, q1, #31\n"
                  "      vst1.32 {d0, d1}, [%[o]]!\n"
                  "      bne 1b\n"
                  "2:"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [c2] "+r" (c[2]), [c3] "+r" (c[3]), [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (ic)
                  : "cc", "q0", "q1",
                    "d16", "d17", "d18", "d19", "d20",
                    "d21", "d22", "d23", "d24", "d25", "d26", "d27", "memory");
}

static inline void
inner_product_gfloat_full_1_neon (gfloat * o, const gfloat * a,
    const gfloat * b, gint len, const gfloat * icoeff, gint bstride)
{
    uint32_t remainder = len % 16;
    len = len - remainder;

    asm volatile ("      vmov.f32 q0, #0.0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vmov.f32 q1, #0.0\n"
                  "1:"
                  "      vld1.32 {q4, q5}, [%[b]]!\n"
                  "      vld1.32 {q8, q9}, [%[a]]!\n"
                  "      vld1.32 {q6, q7}, [%[b]]!\n"
                  "      vld1.32 {q10, q11}, [%[a]]!\n"
                  "      subs %[len], %[len], #16\n"
                  "      vmla.f32 q0, q4, q8\n"
                  "      vmla.f32 q1, q5, q9\n"
                  "      vmla.f32 q0, q6, q10\n"
                  "      vmla.f32 q1, q7, q11\n"
                  "      bne 1b\n"
                  "      vadd.f32 q0, q0, q1\n"
                  "2:"
                  "      cmp %[remainder], #0\n"
                  "      beq 4f\n"
                  "3:"
                  "      vld1.32 {q6}, [%[b]]!\n"
                  "      vld1.32 {q10}, [%[a]]!\n"
                  "      subs %[remainder], %[remainder], #4\n"
                  "      vmla.f32 q0, q6, q10\n"
                  "      bgt 3b\n"
                  "4:"
                  "      vadd.f32 d0, d0, d1\n"
                  "      vpadd.f32 d0, d0, d0\n"
                  "      vst1.32 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [b] "+r" (b),
                    [len] "+r" (len), [remainder] "+r" (remainder)
                  : [o] "r" (o)
                  : "cc", "q0", "q1", "q4", "q5", "q6", "q7", "q8",
                    "q9", "q10", "q11");
}

static inline void
inner_product_gfloat_linear_1_neon (gfloat * o, const gfloat * a,
    const gfloat * b, gint len, const gfloat * icoeff, gint bstride)
{
    const gfloat *c[2] = {(gfloat*)((gint8*)b + 0*bstride),
                          (gfloat*)((gint8*)b + 1*bstride)};

    asm volatile ("      vmov.f32 q0, #0.0\n"
                  "      vmov.f32 q1, #0.0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "1:"
                  "      vld1.32 {q8, q9}, [%[c0]]!\n"
                  "      vld1.32 {q10, q11}, [%[c1]]!\n"
                  "      vld1.32 {q12, q13}, [%[a]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmla.f32 q0, q8, q12\n"
                  "      vmla.f32 q1, q10, q12\n"
                  "      vmla.f32 q0, q9, q13\n"
                  "      vmla.f32 q1, q11, q13\n"
                  "      bne 1b\n"
                  "2:"
                  "      vld2.32 {d20[], d21[]}, [%[ic]]\n"
                  "      vmul.f32 d0, d0, d20\n"
                  "      vmla.f32 d0, d1, d20\n"
                  "      vmla.f32 d0, d2, d21\n"
                  "      vmla.f32 d0, d3, d21\n"
                  "      vpadd.f32 d0, d0, d0\n"
                  "      vst1.32 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [len] "+r" (len)
                  : [o] "r" (o), [ic] "r" (icoeff)
                  : "cc", "q0", "q1",
                    "q8", "q9", "q10", "q11", "q12", "q13", "memory");
}

static inline void
inner_product_gfloat_cubic_1_neon (gfloat * o, const gfloat * a,
    const gfloat * b, gint len, const gfloat * icoeff, gint bstride)
{
    const gfloat *c[4] = {(gfloat*)((gint8*)b + 0*bstride),
                          (gfloat*)((gint8*)b + 1*bstride),
                          (gfloat*)((gint8*)b + 2*bstride),
                          (gfloat*)((gint8*)b + 3*bstride)};

    asm volatile ("      vmov.f32 q0, #0.0\n"
                  "      vmov.f32 q1, #0.0\n"
                  "      vmov.f32 q2, #0.0\n"
                  "      vmov.f32 q3, #0.0\n"
                  "      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "1:"
                  "      vld1.32 {q8}, [%[c0]]!\n"
                  "      vld1.32 {q9}, [%[c1]]!\n"
                  "      vld1.32 {q10}, [%[c2]]!\n"
                  "      vld1.32 {q11}, [%[c3]]!\n"
                  "      vld1.32 {q12}, [%[a]]!\n"
                  "      subs %[len], %[len], #4\n"
                  "      vmla.f32 q0, q8, q12\n"
                  "      vmla.f32 q1, q9, q12\n"
                  "      vmla.f32 q2, q10, q12\n"
                  "      vmla.f32 q3, q11, q12\n"
                  "      bne 1b\n"
                  "2:"
                  "      vld4.32 {d20[], d21[], d22[], d23[]}, [%[ic]]\n"
                  "      vmul.f32 d0, d0, d20\n"
                  "      vmla.f32 d0, d1, d20\n"
                  "      vmla.f32 d0, d2, d21\n"
                  "      vmla.f32 d0, d3, d21\n"
                  "      vmla.f32 d0, d4, d22\n"
                  "      vmla.f32 d0, d5, d22\n"
                  "      vmla.f32 d0, d6, d23\n"
                  "      vmla.f32 d0, d7, d23\n"
                  "      vpadd.f32 d0, d0, d0\n"
                  "      vst1.32 d0[0], [%[o]]\n"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [c2] "+r" (c[2]), [c3] "+r" (c[3]), [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (icoeff)
                  : "cc", "q0", "q1", "q2", "q3",
                    "q8", "q9", "q10", "q11", "q12", "memory");
}

static inline void
interpolate_gfloat_linear_neon (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
    gfloat *o = op, *a = ap, *ic = icp;
    const gfloat *c[2] = {(gfloat*)((gint8*)a + 0*astride),
                          (gfloat*)((gint8*)a + 1*astride)};

    asm volatile ("      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vld2.32 {d24[], d26[]}, [%[ic]]!\n"
                  "      vmov.32 d25, d24\n"
                  "      vmov.32 d27, d26\n"
                  "1:"
                  "      vld1.32 {q8, q9}, [%[c0]]!\n"
                  "      vld1.32 {q10, q11}, [%[c1]]!\n"
                  "      subs %[len], %[len], #8\n"
                  "      vmul.f32 q0, q8, q12\n"
                  "      vmul.f32 q1, q9, q12\n"
                  "      vmla.f32 q0, q10, q13\n"
                  "      vmla.f32 q1, q11, q13\n"
                  "      vst1.32 {q0, q1}, [%[o]]!\n"
                  "      bne 1b\n"
                  "2:"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (ic)
                  : "cc", "q0", "q1", "q8", "q9",
                    "q10", "q11", "q12", "q13", "memory");
}

static inline void
interpolate_gfloat_cubic_neon (gpointer op, const gpointer ap,
    gint len, const gpointer icp, gint astride)
{
    gfloat *o = op, *a = ap, *ic = icp;
    const gfloat *c[4] = {(gfloat*)((gint8*)a + 0*astride),
                          (gfloat*)((gint8*)a + 1*astride),
                          (gfloat*)((gint8*)a + 2*astride),
                          (gfloat*)((gint8*)a + 3*astride)};

    asm volatile ("      cmp %[len], #0\n"
                  "      beq 2f\n"
                  "      vld4.32 {d24[], d26[], d28[], d30[]}, [%[ic]]!\n"
                  "      vmov.32 d25, d24\n"
                  "      vmov.32 d27, d26\n"
                  "      vmov.32 d29, d28\n"
                  "      vmov.32 d31, d30\n"
                  "1:"
                  "      vld1.32 {q8}, [%[c0]]!\n"
                  "      vld1.32 {q9}, [%[c1]]!\n"
                  "      vld1.32 {q10}, [%[c2]]!\n"
                  "      vld1.32 {q11}, [%[c3]]!\n"
                  "      subs %[len], %[len], #4\n"
                  "      vmul.f32 q0, q8, q12\n"
                  "      vmla.f32 q0, q9, q13\n"
                  "      vmla.f32 q0, q10, q14\n"
                  "      vmla.f32 q0, q11, q15\n"
                  "      vst1.32 {q0}, [%[o]]!\n"
                  "      bne 1b\n"
                  "2:"
                  : [a] "+r" (a), [c0] "+r" (c[0]), [c1] "+r" (c[1]),
                    [c2] "+r" (c[2]), [c3] "+r" (c[3]),
                    [len] "+r" (len), [o] "+r" (o)
                  : [ic] "r" (ic)
                  : "cc", "q0", "q8", "q9",
                    "q10", "q11", "q12", "q13", "q14", "q15", "memory");
}

MAKE_RESAMPLE_FUNC_STATIC (gint16, full, 1, neon);
MAKE_RESAMPLE_FUNC_STATIC (gint16, linear, 1, neon);
MAKE_RESAMPLE_FUNC_STATIC (gint16, cubic, 1, neon);

MAKE_RESAMPLE_FUNC_STATIC (gint32, full, 1, neon);
MAKE_RESAMPLE_FUNC_STATIC (gint32, linear, 1, neon);
MAKE_RESAMPLE_FUNC_STATIC (gint32, cubic, 1, neon);

MAKE_RESAMPLE_FUNC_STATIC (gfloat, full, 1, neon);
MAKE_RESAMPLE_FUNC_STATIC (gfloat, linear, 1, neon);
MAKE_RESAMPLE_FUNC_STATIC (gfloat, cubic, 1, neon);

static void
audio_resampler_check_neon (const gchar *option)
{
  if (!strcmp (option, "neon")) {
    GST_DEBUG ("enable NEON optimisations");
    resample_gint16_full_1 = resample_gint16_full_1_neon;
    resample_gint16_linear_1 = resample_gint16_linear_1_neon;
    resample_gint16_cubic_1 = resample_gint16_cubic_1_neon;

    interpolate_gint16_linear = interpolate_gint16_linear_neon;
    interpolate_gint16_cubic = interpolate_gint16_cubic_neon;

    resample_gint32_full_1 = resample_gint32_full_1_neon;
    resample_gint32_linear_1 = resample_gint32_linear_1_neon;
    resample_gint32_cubic_1 = resample_gint32_cubic_1_neon;

    interpolate_gint32_linear = interpolate_gint32_linear_neon;
    interpolate_gint32_cubic = interpolate_gint32_cubic_neon;

    resample_gfloat_full_1 = resample_gfloat_full_1_neon;
    resample_gfloat_linear_1 = resample_gfloat_linear_1_neon;
    resample_gfloat_cubic_1 = resample_gfloat_cubic_1_neon;

    interpolate_gfloat_linear = interpolate_gfloat_linear_neon;
    interpolate_gfloat_cubic = interpolate_gfloat_cubic_neon;
  }
}
