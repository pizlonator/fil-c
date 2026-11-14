/* Compare the JPEG shim's layouts and saturation with SIMDe's references. */
#include "arm_neon.h"
#include <stdio.h>
#include <string.h>

static uint32_t state = 1;
static int failures;

static uint8_t random_byte(void)
{
  state = state * 1103515245 + 12345;
  return (uint8_t)(state >> 16);
}

#define CHECK(name, got, want) do { \
  if (memcmp(&(got), &(want), sizeof(got))) { \
    if (failures++ < 10) \
      printf("FAIL %s\n", name); \
  } \
} while (0)

/* Check every plain load/store override, including the bytes around stores.
 * Use an offset so a mistaken aligned load is not silently accepted.
 */
#define MEMORY(suffix, scalar, vector) do { \
  const scalar *p = (const scalar *)(in + 4); \
  vector got = vld1##suffix(p), want = simde_vld1##suffix(p); \
  CHECK("vld1" #suffix, got, want); \
  memset(out, 0x55, sizeof(out)); \
  memset(ref, 0x55, sizeof(ref)); \
  vst1##suffix((scalar *)(out + 4), want); \
  simde_vst1##suffix((scalar *)(ref + 4), want); \
  CHECK("vst1" #suffix, out, ref); \
} while (0)

#define STORE(name, dst, value) do { \
  memset(out, 0x55, sizeof(out)); \
  memset(ref, 0x55, sizeof(ref)); \
  name(dst out, value); \
  simde_##name(dst ref, value); \
  CHECK(#name, out, ref); \
} while (0)

int main(void)
{
  for (int it = 0; it < 2000; it++) {
    uint8_t in[80], out[80], ref[80];
    for (int i = 0; i < 80; i++)
      in[i] = random_byte();
    int16_t *i16 = (int16_t *)in;

    MEMORY(q_s8, int8_t, int8x16_t);
    MEMORY(q_u8, uint8_t, uint8x16_t);
    MEMORY(q_s16, int16_t, int16x8_t);
    MEMORY(q_u16, uint16_t, uint16x8_t);
    MEMORY(q_s32, int32_t, int32x4_t);
    MEMORY(q_u32, uint32_t, uint32x4_t);
    MEMORY(_s8, int8_t, int8x8_t);
    MEMORY(_u8, uint8_t, uint8x8_t);
    MEMORY(_s16, int16_t, int16x4_t);
    MEMORY(_u16, uint16_t, uint16x4_t);
    MEMORY(_s32, int32_t, int32x2_t);
    MEMORY(_u32, uint32_t, uint32x2_t);

    uint8x16x3_t q3 = vld3q_u8(in), q3r = simde_vld3q_u8(in);
    CHECK("vld3q_u8", q3, q3r);
    uint8x8x3_t d3 = vld3_u8(in), d3r = simde_vld3_u8(in);
    CHECK("vld3_u8", d3, d3r);
    uint8x16x4_t q4 = vld4q_u8(in), q4r = simde_vld4q_u8(in);
    CHECK("vld4q_u8", q4, q4r);
    uint8x8x4_t d4 = vld4_u8(in), d4r = simde_vld4_u8(in);
    CHECK("vld4_u8", d4, d4r);
    uint8x8x2_t d2 = vld2_u8(in), d2r = simde_vld2_u8(in);
    CHECK("vld2_u8", d2, d2r);
    int16x8x4_t s4 = vld4q_s16(i16), s4r = simde_vld4q_s16(i16);
    CHECK("vld4q_s16", s4, s4r);
    int16x4x3_t x3 = vld1_s16_x3(i16), x3r = simde_vld1_s16_x3(i16);
    CHECK("vld1_s16_x3", x3, x3r);
    uint8x16x4_t l4 = vld1q_u8_x4(in), l4r = simde_vld1q_u8_x4(in);
    CHECK("vld1q_u8_x4", l4, l4r);
    int8x16x4_t sl4 = vld1q_s8_x4((int8_t *)in);
    int8x16x4_t sl4r = simde_vld1q_s8_x4((int8_t *)in);
    CHECK("vld1q_s8_x4", sl4, sl4r);

    STORE(vst3q_u8, , q3r);
    STORE(vst3_u8, , d3r);
    STORE(vst4q_u8, , q4r);
    STORE(vst4_u8, , d4r);
    uint8x16x2_t q2 = {{q4r.val[0], q4r.val[1]}};
    STORE(vst2q_u8, , q2);
    int16x4x4_t h4 = {{simde_vld1_s16(i16), simde_vld1_s16(i16 + 4),
                      simde_vld1_s16(i16 + 8), simde_vld1_s16(i16 + 12)}};
    STORE(vst4_s16, (int16_t *), h4);
    uint16x4x4_t u4 = {{simde_vld1_u16((uint16_t *)in),
                       simde_vld1_u16((uint16_t *)in + 4),
                       simde_vld1_u16((uint16_t *)in + 8),
                       simde_vld1_u16((uint16_t *)in + 12)}};
    STORE(vst4_u16, (uint16_t *), u4);
    memset(out, 0x55, sizeof(out));
    memset(ref, 0x55, sizeof(ref));
    vst4_lane_u16((uint16_t *)out, u4, 3);
    simde_vst4_lane_u16((uint16_t *)ref, u4, 3);
    CHECK("vst4_lane_u16", out, ref);
    uint16x4x2_t u2 = {{u4.val[0], u4.val[1]}};
    memset(out, 0x55, sizeof(out));
    memset(ref, 0x55, sizeof(ref));
    vst2_lane_u16((uint16_t *)out, u2, 2);
    simde_vst2_lane_u16((uint16_t *)ref, u2, 2);
    CHECK("vst2_lane_u16", out, ref);

    int32x4_t w = vshll_n_s16(h4.val[0], 13);
    int32x4_t wr = simde_vshll_n_s16(h4.val[0], 13);
    CHECK("vshll_n_s16/13", w, wr);
    w = vshll_n_s16(h4.val[1], 1);
    wr = simde_vshll_n_s16(h4.val[1], 1);
    CHECK("vshll_n_s16/1", w, wr);
    uint8x16_t a8 = simde_vld1q_u8(in), b8 = simde_vld1q_u8(in + 16);
    uint8x16x2_t t8 = vtrnq_u8(a8, b8), t8r = simde_vtrnq_u8(a8, b8);
    CHECK("vtrnq_u8", t8, t8r);
    int16x8_t a16 = simde_vld1q_s16(i16), b16 = simde_vld1q_s16(i16 + 8);
    int16x8x2_t t16 = vtrnq_s16(a16, b16);
    int16x8x2_t t16r = simde_vtrnq_s16(a16, b16);
    CHECK("vtrnq_s16", t16, t16r);
    uint16x8_t ua = simde_vreinterpretq_u16_s16(a16);
    uint16x8_t ub = simde_vreinterpretq_u16_s16(b16);
    uint16x8x2_t tu = vtrnq_u16(ua, ub), tur = simde_vtrnq_u16(ua, ub);
    CHECK("vtrnq_u16", tu, tur);
    int32x4_t a32 = simde_vreinterpretq_s32_s16(a16);
    int32x4_t b32 = simde_vreinterpretq_s32_s16(b16);
    int32x4x2_t t32 = vtrnq_s32(a32, b32);
    int32x4x2_t t32r = simde_vtrnq_s32(a32, b32);
    CHECK("vtrnq_s32", t32, t32r);

    /* Only -32768 * -32768 overflows the doubling multiply. */
    int16_t edge[8] = {-32768, -32768, 32767, -1, 1, 0, -32768, 12345};
    int16x8_t a = (it & 1) ? simde_vld1q_s16(edge) : a16;
    int16x4_t lanes = (it & 2) ? simde_vld1_s16(edge) : h4.val[2];
    int16x8_t m = vqdmulhq_lane_s16(a, lanes, 0);
    int16x8_t mr = simde_vqdmulhq_lane_s16(a, lanes, 0);
    CHECK("vqdmulhq_lane_s16/0", m, mr);
    m = vqdmulhq_lane_s16(a, lanes, 3);
    mr = simde_vqdmulhq_lane_s16(a, lanes, 3);
    CHECK("vqdmulhq_lane_s16/3", m, mr);
    int16x4_t low = simde_vget_low_s16(a);
    int16x4_t n = vqdmulh_lane_s16(low, lanes, 1);
    int16x4_t nr = simde_vqdmulh_lane_s16(low, lanes, 1);
    CHECK("vqdmulh_lane_s16/1", n, nr);
  }
  printf("layout: %d failures\n", failures);
  return failures != 0;
}
