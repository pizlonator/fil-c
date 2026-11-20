/* Compare the JPEG shim with SIMDe's reference arithmetic operations. */
#include "arm_neon.h"
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(name, got, want, ...) do { \
  if (memcmp(&(got), &(want), sizeof(got))) { \
    if (failures++ < 10) { \
      printf("FAIL %s ", name); \
      printf(__VA_ARGS__); \
      printf("\n"); \
    } \
  } \
} while (0)

int main(void)
{
  /* Every value against every shift byte, with nonzero high-byte noise.
   * NEON uses the signed low byte, not the full signed 16-bit shift lane.
   */
  for (int s = 0; s < 256; s++) {
    for (int v = 0; v < 65536; v += 8) {
      uint16_t av[8], bv[8];
      for (int i = 0; i < 8; i++) {
        av[i] = (uint16_t)(v + i);
        bv[i] = (uint16_t)(s | ((v * 7 + i * 13) & 0xFF00));
      }
      uint16x8_t a = simde_vld1q_u16(av);
      int16x8_t b = simde_vreinterpretq_s16_u16(simde_vld1q_u16(bv));
      uint16x8_t got = vshlq_u16(a, b), want = simde_vshlq_u16(a, b);
      CHECK("vshlq_u16", got, want, "v=%d s=%d", v, s);
    }
  }

  /* Every signed and unsigned 16-bit leading-zero input, including zero. */
  for (int v = 0; v < 65536; v += 8) {
    int16_t av[8];
    for (int i = 0; i < 8; i++)
      av[i] = (int16_t)(v + i);
    int16x8_t a = simde_vld1q_s16(av);
    int16x8_t got = vclzq_s16(a), want = simde_vclzq_s16(a);
    CHECK("vclzq_s16", got, want, "v=%d", v);
    uint16x8_t ua = simde_vreinterpretq_u16_s16(a);
    uint16x8_t ugot = vclzq_u16(ua), uwant = simde_vclzq_u16(ua);
    CHECK("vclzq_u16", ugot, uwant, "v=%d", v);
  }

  /* Exercise every byte value, then deterministic nonsymmetric vectors.
   * The reduction is modulo 256, not a saturating sum.
   */
  uint32_t x = 12345;
  for (int t = 0; t < 1000000; t++) {
    uint8_t av[8];
    for (int i = 0; i < 8; i++) {
      x = x * 1103515245 + 12345;
      av[i] = t < 256 ? (uint8_t)(t + i) : (uint8_t)(x >> 16);
    }
    uint8x8_t a = simde_vld1_u8(av);
    uint8x8_t got = vcnt_u8(a), want = simde_vcnt_u8(a);
    CHECK("vcnt_u8", got, want, "t=%d", t);
    uint8_t sg = vaddv_u8(a), sw = simde_vaddv_u8(a);
    CHECK("vaddv_u8", sg, sw, "t=%d", t);
  }
  printf("arithmetic: %d failures\n", failures);
  return failures != 0;
}
