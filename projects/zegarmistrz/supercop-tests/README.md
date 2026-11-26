# supercop-tests: the supercop checksum differential suite

Differential checksum tests that run real, unmodified supercop crypto code
as zegarmistrz guests: every implementation is built into a small
deterministic checksum driver, the driver is executed natively (hardware
oracle) and under `../zegarmistrz`, and the test passes iff both runs
print the same checksum output byte-for-byte. This is the permanent,
auto-collected form of the scratch campaign that pinned the emulator bugs
djb reported as supercop checksum differences (`crypto_core/keccakf160064bits/reference1600`
with gcc -O3 giving different output under zegarmistrz than on bare metal,
and friends): those divergences were real emulator bugs -- SHUFPD operand
roles, SHLD/SHRD count regimes, ADC/SBB OF, and the VBLENDV mask operand --
and this suite keeps all 51 affected cores under continuous test so the
fixes cannot regress.

## What is here

One self-contained directory per core/implementation, 61 total, mirroring
the x86sem-tests pattern (one unit per directory, auto-collected by the
sub-runner):

- `core_mult3sntrup{653,761,857,953,1013,1277}` and
  `core_multsntrup{653,761,857,953,1013,1277}`: the NTRU-Prime
  `crypto_core` multiplication kernels (the `multsntrup` dirs vendor the
  `crypto_{decode,encode}_*xint16` helpers their `params.h` needs).
- `core_invhrss701`: the HRSS-SNYDER `crypto_core` inversion kernel --
  the suite's slowest emulated leg by an order of magnitude (~10 min;
  see `core_invhrss701/build.flags`).
- `core_keccakf160064bits_{reference1600,compact1600,AsmX86-64,AsmX86-64shld,lcufullshld}`:
  five Keccak-f[1600] implementations, three of them hand-written
  assembly.
- `encode_*`: 36 `crypto_encode` implementations (int16/freeze3/x3
  codecs, mod-q codecs, and their `round` variants; the `round` dirs
  vendor the base codec's `wrapper.c`/`Encode.c`/`Encode.h` and rename
  it via `build.flags`).
- `onetimeauth_poly1305_{ref,53,amd64,avx,moon_sse2,moon_avx,moon_avx2}`:
  seven Poly1305 one-time authenticator implementations, four of them
  SIMD assembly.

Each directory contains the vendored supercop sources and headers plus:

- `driver.c`: the suite-local checksum driver (see below).
- `build.flags`: per-dir build directives read by the sub-runner
  (`src <file>: <flags>` renames a vendored source via -D, `link: <flags>`
  adds link flags, `emu-timeout: <seconds>` raises the emulated-leg
  bound). Dirs with no directives still carry the file, documenting that
  nothing extra is applied.

`include/` holds the shared headers: the cryptoint headers
(`crypto_int8.h` ... `crypto_uintN.h`) and `brg_endian.h`, byte-identical
copies from the supercop tarball, plus suite-local declaration shims
(`crypto_core.h`, `crypto_encode.h`, `crypto_decode.h`,
`crypto_onetimeauth.h`, `crypto_verify*.h`, and the per-primitive
`crypto_{encode,decode}_*.h` headers). supercop's build system generates
those per-primitive headers from each implementation's `api.h` during its
own build; the tarball only ships `api.h`, so the suite ships equivalent
two-line shims instead. Nothing vendored was modified to accommodate them.

## Provenance and licensing

All vendored sources come from the pristine
[supercop-20261018](https://bench.cr.yp.to/supercop/supercop-20261018.tar.xz)
release (`crypto_core/`, `crypto_encode/`, `crypto_decode/`,
`crypto_onetimeauth/poly1305/`, `crypto_verify/16/ref/`, `cryptoint/`,
and `include/brg_endian.h`). Every vendored file is byte-identical to the
tarball except one, documented below.

supercop's sources are public domain. The files carry that status in one
of three ways: djb's files are marked "Public domain." in their headers
(e.g. `poly1305/ref/auth.c`) or are supercop work products that the
project distributes as public domain (`mult*sntrup`, `invhrss701`,
`crypto_encode`, `cryptoint`, the moon poly1305 sources, the keccak
assembly); the Keccak Team files carry an explicit CC0 dedication
("To the extent possible under law, the implementer has waived all
copyright and related or neighboring rights to the source code in this
file", `http://creativecommons.org/publicdomain/zero/1.0/`); and the
Brumley AVX poly1305 file carries an explicit unlicense-style public
domain dedication. Suite-local files (the `driver.c` files, the shim
headers in `include/`, `build.flags` files, `run-supercop-tests`) are
Fil-C project code, written for this suite.

## Methodology

`run-supercop-tests` auto-collects every `*/driver.c`, generates
`testmakefile` (regenerated per run, the filc/run-tests idiom; untracked),
and runs `make -k -j $JOBS`. Per test, all under `test-output/<dir>/`:

1. Build: `gcc -O3` over the vendored sources plus the dir's `driver.c`,
   applying the per-source renames and link flags from `build.flags`.
   Suite-wide, `-DCRYPTO_NAMESPACETOP=crypto_onetimeauth` renames the
   poly1305 entry point to the driver-visible symbol and
   `-DCRYPTO_NAMESPACE(x)=x` neutralizes the keccak `namespace.h` and
   moon `x86.inc` renaming -- the same choices djb's checksum campaign
   made (supercop's build would set these per-impl from `api.h`; the
   tarball does not, so the suite fixes them globally like the campaign
   did).
2. Native leg (hardware oracle): run `./<dir>`, capture stdout to
   `<dir>.native`, stderr to `<dir>.native.err`, bound 120s, require
   exit 0.
3. Emulated leg: run `../../../zegarmistrz ./<dir>` from the output dir
   (so any core dumps land there), capture stdout to `<dir>.emu`, stderr
   to `<dir>.emu.err`, bound 600s by default (`emu-timeout:` in
   `build.flags` raises it; only `core_invhrss701` needs that), require
   exit 0. The target depends on `../zegarmistrz`, so the emulated leg
   re-runs whenever the emulator changes.
4. Verdict (`<dir>.cmp` stamp): `cmp` the two stdout captures, require
   exactly the expected number of `FNVCHECK` lines (3 for core and
   poly1305 dirs, 1 for encode dirs), require the verbatim `<dir>-ok`
   line, and require empty stderr on both legs -- an emulator diagnostic
   is a failure signal here, matching the campaign, which merged stderr
   into the compared stream. PASS lines echo from phony wrappers so
   cached runs still count every sub-test.

The drivers (`driver-core.c`, `driver-encode.c`, `driver-ota.c` logic,
one copy per dir) fold each invocation's outputs into an FNV-1a checksum
and print `FNVCHECK <16hex>` per seed plus a final `<dir>-ok` line. The
input scheme and all numerics are the ones djb's checksum campaign used
per family, so the suite's native-leg checksums reproduce the campaign's
recorded table bit-for-bit (verified 61/61 at landing):

- `crypto_core` dirs: 3 seeds x 8 iterations; LCG-filled `in`/`k`/`c`
  buffers at the `api.h` sizes; FNV over `CRYPTO_OUTPUTBYTES` output
  bytes.
- `crypto_encode` dirs: 1 seed x 16 iterations; LCG-filled
  `CRYPTO_ITEMS*CRYPTO_ITEMBYTES` input; FNV over `CRYPTO_STRBYTES`
  output bytes.
- `crypto_onetimeauth/poly1305` dirs: 3 seeds x 4 iterations x a
  message-length sweep (0, 1, 2, 15, 16, 17, 31, 32, 63, 64, 127, 131,
  135, 255, 256, 511, 1000, 1024 -- partial blocks through multi-block);
  FNV over the 16-byte tags.

The LCG is `s = s*6364136223846793005 + 1442695040888963407` seeded with
`seed*2862933555777941757 + 3037000493` for seed in {1,2,3}; the FNV-1a
basis is 14695981039346656037. Everything is deterministic: the same
binary prints the same bytes natively and under the emulator (no ASLR or
hash variance), which is what makes the byte-compare meaningful.

## Deviations from pristine supercop

Exactly one vendored byte differs from the tarball:
`onetimeauth_poly1305_avx/auth.S` inserts a single `.text` line before
the entry-point symbols. Upstream assembles the entire file's code into
`.rodata` (the file opens with `.section .rodata` and never opens a
`.text` after the constants), so with modern binutils' default
`-z separate-code` that segment is mapped read-only and the guest
segfaults natively before reaching main. The inserted line is a
build-compatibility fix with no semantic change: same instructions,
same constants, same order, merely in `.text` instead of `.rodata`. The
emulator maps segments executable regardless, so the fix only restores a
meaningful native leg. (The campaign used the same patched copy.)

Two build-level deviations, both no-ops on the vendored bytes:

- The per-source renames in `build.flags` (`-Dcrypto_decode=...` and
  friends) reproduce what supercop's build system does; they exist
  because the suite compiles the vendored sources directly.
- The asm-bearing poly1305 dirs link `-no-pie` (the campaign linked
  every target that way; those `.S` files predate toolchain-default PIE
  and, depending on the toolchain, produce absolute text relocations),
  and `core_keccakf160064bits_AsmX86-64shld` links `-no-pie` because its
  `keccakp.S` takes absolute addresses into its own `.text`
  (`R_X86_64_32S`), which cannot link as PIE at all. Everything else
  links as a normal PIE -- which the campaign's scratch drivers never
  tried, so this suite exercises slightly more of the loader than the
  campaign did.

## Running it

```bash
cd projects/zegarmistrz
make -j$(nproc)                  # the emulator must exist first
cd supercop-tests
./run-supercop-tests             # full suite; ~10-12 min cold
JOBS=16 ./run-supercop-tests     # tune parallelism
SUPERCOP_QUIET=1 ./run-supercop-tests   # verdict lines + tally only
```

It also runs as the `supercop-suite` case inside `../run-tests`, which
passes `SUPERCOP_QUIET=1` and a generous timeout because
`core_invhrss701`'s emulated leg alone takes on the order of ten minutes.

Runtime profile (cold, 32 jobs, reference machine): the whole suite runs
about 11 minutes wall, dominated by `core_invhrss701` (~10 min emulated)
and `core_multsntrup1277` (~4.5 min emulated); the slowest five
emulated legs are invhrss701 ~590s, multsntrup1277 ~270s,
mult3sntrup1277 ~105s, multsntrup1013 ~70s, mult3sntrup1013 ~60s, and
everything else is seconds. Native legs are all sub-second. A no-change
invocation re-runs nothing (make caching: emu outputs depend on the
guest binary and `../zegarmistrz`; native outputs on the guest binary;
cmp stamps on both outputs) -- measured at ~0.1s.

## Adding a test

Add a directory `foo/` containing the vendored sources, a `driver.c`
printing `FNVCHECK <hex>` lines plus a final `foo-ok` line, and a
`build.flags` documenting any non-default build directives. The
sub-runner picks it up automatically on the next run; `include/` may need
a shim header if the implementation's sources include one.
