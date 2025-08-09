#include <cstddef>
#include <cstdint>
#include <array>

#include "helpers/common.h"
#include "simdutf.h"

constexpr bool allow_invalid_16bit_input = false;

constexpr std::array options = {
    simdutf::base64_default,          simdutf::base64_url,
    simdutf::base64_reverse_padding,  simdutf::base64_default_no_padding,
    simdutf::base64_url_with_padding,
};

struct decoderesult {
  std::size_t maxbinarylength{};
  simdutf::result convertresult{};
  auto operator<=>(const decoderesult&) const = default;
};

template <typename FromChar>
void decode(std::span<const FromChar> base64_, const auto selected_option) {
  // force to ascii to follow the restriction on the base64_to_binary input.
  // however, not doing so uncovered a real bug, see
  // https://github.com/simdutf/simdutf/issues/503#issuecomment-2287154397
  std::vector<FromChar> base64(begin(base64_), end(base64_));
  if (!allow_invalid_16bit_input) {
    for (auto& x : base64) {
      x &= 0xFF;
    }
  }
  const auto implementations = get_supported_implementations();
  std::vector<decoderesult> results;
  results.reserve(implementations.size());
  for (auto impl : implementations) {
    auto& r = results.emplace_back();
    r.maxbinarylength =
        impl->maximal_binary_length_from_base64(base64.data(), base64.size());
    std::vector<char> output(r.maxbinarylength);
    r.convertresult = impl->base64_to_binary(base64.data(), base64.size(),
                                             output.data(), selected_option);
  }
  auto neq = [](const auto& a, const auto& b) { return a != b; };
  if (std::ranges::adjacent_find(results, neq) != results.end()) {
    std::cerr << "output differs between implementations for decode\n";
    const auto implementations = get_supported_implementations();
    std::size_t i = 0;
    for (const auto& r : results) {
      std::cerr << "impl " << implementations[i]->name()
                << " got maxbinarylength=" << r.maxbinarylength
                << " convertresult=" << r.convertresult << "\n";
      ++i;
    }
    std::cerr << "option: " << selected_option << '\n';
    std::cerr << "data: "
              << (std::is_same_v<FromChar, char> ? "char" : "char16_t") << "{";
    for (int v : base64) {
      std::cerr << v << ", ";
    }
    std::cerr << "}\n";
    std::abort();
  }
}

template <typename FromChar>
void decode_safe(std::span<const FromChar> base64_, const auto selected_option,
                 const std::size_t decode_buf_size) {
  // force to ascii to follow the restriction on the base64_to_binary input.
  // however, not doing so uncovered a real bug, see
  // https://github.com/simdutf/simdutf/issues/503#issuecomment-2287154397
  std::vector<FromChar> base64(begin(base64_), end(base64_));
  if (!allow_invalid_16bit_input) {
    for (auto& x : base64) {
      x &= 0xFF;
    }
  }
  std::vector<char> output(decode_buf_size);
  std::size_t outlen = decode_buf_size;
  const auto convertresult = simdutf::base64_to_binary_safe(
      base64.data(), base64.size(), output.data(), outlen, selected_option);

  // the number of written bytes must always be less than the supplied buffer
  assert(outlen <= decode_buf_size);

  switch (convertresult.error) {
  case simdutf::error_code::OUTPUT_BUFFER_TOO_SMALL: {
    if (!(convertresult.count <= base64.size())) {
      std::cerr << " decode_buf_size=" << decode_buf_size
                << " outlen=" << outlen << " and result=" << convertresult
                << '\n';
      std::abort();
    }
  } break;
  case simdutf::error_code::INVALID_BASE64_CHARACTER: {
    assert(convertresult.count < base64.size());
  } break;
  case simdutf::error_code::BASE64_INPUT_REMAINDER: {
    if (!(convertresult.count <= base64.size())) {
      std::cerr << "on input with size=" << base64.size()
                << ": got BASE64_INPUT_REMAINDER decode_buf_size="
                << decode_buf_size << " outlen=" << outlen
                << " and result=" << convertresult << '\n';
      std::abort();
    }
  } break;
  case simdutf::error_code::SUCCESS: {
    // possibility to compare with the normal function
  } break;
  default:;
  }
}

struct roundtripresult {
  std::size_t length{};
  std::size_t maxbinarylength{};
  std::string outputhash;
  std::size_t written{};
  simdutf::result convertbackresult{};
  auto operator<=>(const roundtripresult&) const = default;
};

void roundtrip(std::span<const char> binary, const auto selected_option) {

  const auto inputhash = FNV1A_hash::as_str(binary);
  const auto implementations = get_supported_implementations();
  std::vector<roundtripresult> results;
  results.reserve(implementations.size());
  for (auto impl : implementations) {
    auto& r = results.emplace_back();
    r.length = impl->base64_length_from_binary(binary.size(), selected_option);
    std::vector<char> output(r.length);
    r.written = impl->binary_to_base64(binary.data(), binary.size(),
                                       output.data(), selected_option);
    if (r.length != r.written) {
      std::abort();
    }
    r.outputhash = FNV1A_hash::as_str(output);
    // convert back to binary
    r.maxbinarylength =
        impl->maximal_binary_length_from_base64(output.data(), output.size());
    std::vector<char> restored(r.maxbinarylength);
    r.convertbackresult = impl->base64_to_binary(
        output.data(), output.size(), restored.data(), selected_option);

    if (const auto restoredhash = FNV1A_hash::as_str(restored);
        inputhash != restoredhash) {
      std::abort();
    }
    if (restored.size() != binary.size()) {
      std::abort();
    }
  }

  auto neq = [](const auto& a, const auto& b) { return a != b; };
  if (std::ranges::adjacent_find(results, neq) != results.end()) {
    std::cerr << "output differs between implementations\n";
    for (const auto& r : results) {
      std::cout << "written=" << r.written << " maxlength=" << r.maxbinarylength
                << " length=" << r.length << '\n';
    }
    std::abort();
  }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // pick one of the function pointers, based on the fuzz data
  // the first byte is which action to take. step forward
  // several bytes so the input is aligned.
  if (size < 4) {
    return 0;
  }
  constexpr auto Ncases = 5u;
  constexpr auto actionmask = std::bit_ceil(Ncases) - 1;
  const auto action = data[0] & actionmask;

  // pick a random option
  const auto selected_option = [](auto index) {
    if (index >= options.size())
      return options[0];
    else {
      return options[index];
    }
  }(data[1] & (std::bit_ceil(options.size()) - 1));

  // decode buffer size
  const std::size_t decode_buffer_size = (data[3] << 8) + data[2];

  data += 4;
  size -= 4;

  switch (action) {
  case 0: {
    const std::span<const char> chardata{(const char*)data, size};
    roundtrip(chardata, selected_option);
  } break;
  case 1: {
    const std::span<const char> chardata{(const char*)data, size};
    decode(chardata, selected_option);
  } break;
  case 2: {
    const std::span<const char16_t> chardata{(const char16_t*)data, size / 2};
    decode(chardata, selected_option);
  } break;
  case 3: {
    const std::span<const char> chardata{(const char*)data, size};
    decode_safe(chardata, selected_option, decode_buffer_size);
  } break;
  case 4: {
    const std::span<const char16_t> chardata{(const char16_t*)data, size / 2};
    decode_safe(chardata, selected_option, decode_buffer_size);
  } break;
  }

  return 0;
}
