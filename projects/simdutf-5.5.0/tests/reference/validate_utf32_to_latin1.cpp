#include "validate_utf32.h"

namespace simdutf {
namespace tests {
namespace reference {

simdutf_warn_unused bool validate_utf32_to_latin1(const char32_t *buf, size_t len) noexcept {
  const char32_t* curr = buf;
  const char32_t* end = buf + len;

  while (curr != end) {
      const uint32_t word = *curr;

			if (word > 0xFF) {
				return false;
			}

      curr++;
  }

  return true;
}

} // namespace reference
} // namespace tests
} // namespace simdutf