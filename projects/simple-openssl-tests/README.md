# simple-openssl-tests

A suite of small, self-contained C programs — one binary per cipher or
algorithm — that exercise OpenSSL 3's public API with known-answer and
behavior tests.  Each per-cipher binary is a single `.c` file holding only
its own cipher's KAT constants and `main()`; the machinery it shares with
its siblings lives in common modules next to it, and everything builds and
runs identically under Fil-C, under Yolo-C, and under a normal
(unpizlonated) compiler, so each cipher can be watched through all of the
repo's C stacks.  All working buffers (keys, inputs, outputs, IVs, AAD,
tags, salts, ...) are malloc'd through the labeled `ALLOCATE_*` macros with
optional poisoning hooks, so that a real poisoning implementation can be
attached later without touching a single test program.

Design points:

* **One binary per cipher** (AES-128-CBC, AES-256-GCM, HKDF, ECDSA,
  ML-KEM-768, ... — 275 binaries).  A cipher whose implementation has
  size-dependent code paths gets an explicit small and an explicit large
  binary — see
  [below](#why-some-cryptosystems-have-small-and-large-test-programs).
* **Public OpenSSL 3 API only** (`EVP_CIPHER_fetch`, `EVP_MD_fetch`,
  `EVP_MAC_fetch`, `EVP_KDF_fetch`, `EVP_PKEY_*`, `OSSL_PARAM`,
  `OSSL_PROVIDER_load`, ...).  No deprecated low-level APIs, no headers
  beyond the public ones.
* **Shared modules, not monoliths.**  `poisoning.{h,c}` (the
  labeled-allocation/poisoning machinery, unchanged) and
  `test_common.{h,c}` (the global boilerplate: the `ALLOCATE_*` macros,
  `g_checks`/`CHECK`/`test_failed`, `hexval`/`from_hex`, `expect_equal`,
  `test_parse_args`, `test_prog_name`, `test_skip`, `test_load_provider`,
  `test_report_pass`) are linked into every binary.  The machinery shared
  by a family of related binaries lives in a `test_<family>_common.{c,h}`
  module (52 of them — e.g. `test_aes_cbc_common`,
  `test_digest_sha2_common`, `test_ml_kem_common`), which the Makefile
  links only into that family's binaries.  Each per-cipher binary then
  contains only its cipher's KAT constants and `main()`; KAT material
  shared across a family is exported by the family module as
  `extern const char name[]` arrays (address constants, usable in static
  initializers).  Every expected value is a deterministic known-answer
  test (KAT) or a self-consistency check embedded in the sources.
* **Deterministic where the algorithm allows it**: randomized algorithms
  (RSA keygen, DSA params, SM2, ...) either embed fixed keys generated once
  with the authoritative build, or use the public determinism hooks
  (`nonce-type`, `deterministic`, `seed`, fixed private keys via
  `EVP_PKEY_fromdata`).
* **Every malloc'd buffer that is handed to OpenSSL flows through a labeled
  `ALLOCATE_*` macro** (one line over `poison_alloc()`, defined in
  `test_common.h`; see [Poison options](#poison-options)).

Program contract, identical for all 275 programs:

* `#include "test_common.h"` comes first (it includes `poisoning.h`); a
  family member then includes its `test_<family>_common.h`.
* Every buffer handed to OpenSSL comes from an `ALLOCATE_*` macro.
* `main()` starts with `test_parse_args(argc, argv)`, which runs
  `poison_parse_args(argc, argv)` and records the program name printed in
  every report line.
* On success it prints `<program>: all N checks passed` and exits 0.
* On any check failure it prints `FAIL <program>: <message>` and exits 1.
* If a genuinely version-gated feature is unavailable in the OpenSSL being
  tested (its `EVP_*_fetch` fails), it prints `SKIP <program>: ...` and
  exits 0 — never for anything present in all OpenSSL 3.x releases.  Some
  programs print a SKIP line for a gated *subtest* and still pass the rest;
  such a line is always followed by the normal `all N checks passed` line.

## Build and run

The compiler is chosen with `CC` and, if needed, the OpenSSL prefix with
`OPENSSL_HOME`:

```sh
make CC=gcc                                    # system gcc + system OpenSSL
make CC=clang                                  # system clang + system OpenSSL
make CC=<repo>/build/bin/filcc                 # Fil-C + pizfix OpenSSL 4
make CC=gcc OPENSSL_HOME=/some/openssl/prefix  # any compiler + any OpenSSL 3
```

`make` defaults `OPENSSL_HOME` to the repo's `pizfix/` when the compiler
name contains "fil", and to the system OpenSSL otherwise.  The convenience
wrappers do the same:

```sh
./build_yolo.sh      # normal (unpizlonated) compiler, gcc by default
                     # (+ system OpenSSL); CC=.../OPENSSL_HOME=... overridable
./build_filc.sh      # <repo>/build/bin/filcc (+ pizfix); FILCC=... overridable
```

There is no wrapper for a Yolo-C (yolo-glibc) build — drive `make` directly;
see [Yolo-C](#yolo-c) below.

Extra arguments go to `make`, e.g. `./build_filc.sh clean test`.

Run the whole suite with either of (extra arguments are passed to every
test, which is how you add `--poison-*` options):

```sh
make test            # builds, then ./run_all.sh
./run_all.sh --poison-all
```

`run_all.sh` prints `PASS`/`FAIL` per program plus a summary line
(`275 passed, 0 failed`) and exits nonzero if anything failed; on failure the
failing program's output is replayed and the full output of the last run is
in `/tmp/simple-openssl-tests.log`.  To run one program directly, build and
execute it: `make test_aes_128_cbc && ./test_aes_128_cbc --poison-key`.

Exit codes of the test programs themselves: 0 = pass or (version-gated)
skip, 1 = `FAIL`, 2 = bad usage (unknown argument, unknown poison kind) —
e.g. `./test_aes_128_cbc --bogus` exits 2 while `./test_aes_128_cbc --poison-help`
prints the poison option list and exits 0.

## Compiler support matrix (verified)

Two configurations were verified against the current per-cipher tree on
this machine (Ubuntu 24.04, x86_64), each with `make clean`, a full build
(275 binaries, no warnings), and `./run_all.sh`:

| Configuration | Command | Compiler | OpenSSL linked | Result |
|---|---|---|---|---|
| gcc (normal compiler) | `./build_yolo.sh` (= `make CC=gcc`) | gcc 12.4.0 | system 3.0.13 | 275 built; 242 passed, 33 whole-program SKIPs |
| Fil-C (primary) | `./build_filc.sh` | `build/bin/filcc` (Fil-C 0.686, clang 20.1.8) | pizfix 4.0.3 | 275 built; 270 passed, 2 failed, 3 whole-program SKIPs |

The 2 failures under pizfix OpenSSL 4.0.3 are OpenSSL 4.0 behavior changes,
not Fil-C regressions: `test_sm2` (OpenSSL 4 substitutes the GM/T 0009-2012
default distinguishing ID `1234567812345678` where OpenSSL 3 kept an empty
default ID, so a signature made under the explicit GM/T ID also verifies as a
default-ID signature — `crypto/sm2/sm2_sign.c` now substitutes
`default_sm2_id` when no ID is set) and `test_chacha20_poly1305_small`
(OpenSSL 4's AEAD decrypt `Final` fails with `PROV_R_TAG_NOT_SET` when no tag
was set, where OpenSSL 3 skipped verification —
`cipher_chacha20_poly1305_final` now rejects `tag_len == 0` on decrypt).

The Fil-C binaries were additionally swept with `./run_all.sh --poison-all`
and with the zegarmistrz poisoning-variant sweep
`./run_variants.sh --build=zunsafe` (defaults otherwise).  Natively all 275
binaries ran and the `run_all.sh` summary line read `273 passed, 2 failed`;
that summary counts every exit-0 program as passed, the three ETM
whole-program SKIPs included, so by the support matrix's convention the
native `--poison-all` sweep was 270 passed, 3 whole-program SKIPs, with the
same two OpenSSL 4 behavior changes as the only failures.  The control
variant of the zegarmistrz sweep reports 261 PASS, 9 TIMEOUT, 3 SKIP and
2 FAIL out of 275, where the 9 TIMEOUT rows are wall-clock timeouts of the
default 120s per-test emulator timeout, not failures — re-running each of
those tests under the emulator with a 3600s-7200s timeout passes all of
them (`test_x448` needs the full 7200s under emulation; natively every one
of them passes in about a second) — so counting those 9 as passed gives the
support matrix's 270 passed, with the same 3 whole-program SKIPs and the
same 2 failures.
(The pre-split monolith tree had
also been verified with `/opt/fil/bin/filcc` and with Yolo-C — see
[Yolo-C](#yolo-c) for that configuration's build recipe; those two have not
been re-run since the split.)  Fil-C programs are built with `-g -O2` per
repo convention (the Makefile default).

The 33 gcc SKIPs and 3 filc SKIPs are whole-program version gates (exit 0,
`SKIP <program>: <reason>`), inventoried below.  The gcc-vs-filc
check-count differences among the passing binaries are documented
version-gated subtests; each row of the [test list](#full-test-list) shows
both counts.

SKIP inventory (all exit-0; the rest of each program still runs):

* **On the system OpenSSL 3.0.13** 33 programs take the whole-program SKIP
  path because the whole algorithm postdates 3.0 or is missing from that
  build: the three ETM binaries `test_aes_128_cbc_hmac_sha256_etm`,
  `test_aes_192_cbc_hmac_sha256_etm`, `test_aes_256_cbc_hmac_sha256_etm`
  (ARM64-only in 3.5+); `test_aes_128_gcm_siv`, `test_aes_192_gcm_siv`,
  `test_aes_256_gcm_siv` (3.5+); `test_digest_keccak_224`,
  `test_digest_keccak_256`, `test_digest_keccak_384`,
  `test_digest_keccak_512` (the KECCAK-* names, 3.5+);
  `test_digest_sha2_256_192` (SHA2-256/192, 3.5+); `test_digest_shake128`,
  `test_digest_shake256` (`EVP_DigestSqueeze`, 3.5+); `test_digest_mdc2`
  (MDC2 is compiled out of Ubuntu's 3.0.13); `test_kdf_argon2d`,
  `test_kdf_argon2i`, `test_kdf_argon2id` (3.5+); `test_idea_ecb`,
  `test_idea_cbc`, `test_idea_ofb`, `test_idea_cfb` (IDEA is not offered by
  Ubuntu's 3.0.13 at all); `test_sm4_gcm`, `test_sm4_ccm`, `test_sm4_xts`
  (postdate 3.0); and `test_ml_kem_512`, `test_ml_kem_768`,
  `test_ml_kem_1024`, `test_ml_dsa_44`, `test_ml_dsa_65`, `test_ml_dsa_87`,
  `test_slh_dsa_sha2_128f`, `test_slh_dsa_shake_128f`,
  `test_slh_dsa_sha2_192f` (3.5+).  Seven more print a SKIP notice for a
  gated *subtest* and pass the rest: `test_ecdsa` (`nonce-type` is silently
  ignored before 3.2), `test_ed25519` (Ed25519ctx/ph, 3.5+),
  `test_kdf_hkdf_sha256` (the concrete HKDF-SHA256 name, 3.5+),
  `test_kdf_hkdf_sha384` (the concrete HKDF-SHA384 name, 3.5+),
  `test_kdf_hkdf_sha512` (the concrete HKDF-SHA512 name, 3.5+),
  `test_rsa_raw` (RSASVE, 3.2+) and `test_x25519` (X25519-as-KEM, 3.5+).
  Eleven print informational `note` lines about pre-3.1/3.2 behavior
  (`test_kdf_hkdf_sha1`, `test_kdf_hkdf_sha256`, `test_kdf_hkdf_sha384` and
  `test_kdf_hkdf_sha512` more than five info params, `test_kdf_kbkdf_sha1`,
  `test_kdf_kbkdf_sha256` and `test_kdf_kbkdf_sha512` no `"r"` param —
  `test_kdf_kbkdf_sha256` also no KMAC PRF, `test_kdf_krb5kdf_aes_128_cbc`
  explicitly empty constant, `test_kdf_sskdf_sha1` and
  `test_kdf_sskdf_sha256` repeated-info concatenation,
  `test_chacha20_poly1305_small` an AAD
  update after data is accepted).  The two count columns in the test list
  differ for exactly those gated binaries plus three 3.0/3.6 behavior
  differences: `test_aes_128_siv`, `test_aes_192_siv` and
  `test_aes_256_siv` (an empty payload leaves the tag unset on 3.0.x),
  `test_ed448` (the `context-string` parameter, 3.2+) and
  `test_rand_test_rand` (TEST-RAND's `generate` mode, 3.6+).
* **On the pizfix OpenSSL 4.0.3 configuration** the only whole-program SKIPs
  are still the same three ETM binaries — the ETM AES-CBC-HMAC-SHA* ciphers
  are only registered on ARM64 builds (the SKIP set is unchanged from the
  3.6.5-era record above; what did change under OpenSSL 4 is the two
  behavior-change FAILs described in the support matrix).

Additional verified invariants:

* `make clean` removes exactly the 275 built binaries (and the `.o`/`.d`
  intermediates) and leaves only the sources, the scripts, the Makefile and
  this README.
* `make test` (build + `./run_all.sh`) works in every verified
  configuration.
* On the pre-split tree the Yolo-C binaries were checked with
  `ldd`/`readelf` (the link recipe is unchanged): the interpreter is
  `<repo>/pizfix/lib/ld-fil1-x86_64.so`, `libcrypto.so.3` resolves to
  `/tmp/yolo-openssl-install/lib/libcrypto.so.3` (the yolo build), and the
  RUNPATH is
  `/tmp/yolo-openssl-install/lib:.../lib64:<repo>/pizfix/lib` — the yolo
  OpenSSL must precede `pizfix/lib`, which holds the pizlonated
  libcrypto that Yolo-C must not pick up.

## Yolo-C

Yolo-C is unpizlonated C compiled by the host compiler but linked against
this repo's "yolo" glibc in `pizfix/` (`crt1.o`/`crti.o`/`crtn.o` +
`crtbegin.o`/`crtend.o`, `-lyoloc`, and the `ld-fil1-x86_64.so` dynamic
loader; the full libc lives in `libyolocimpl.so`).  A Yolo-C binary cannot
link the pizlonated libcrypto in `pizfix/` (the ABI slice problem), so it
needs its own separately-built, unpizlonated ("yolo") OpenSSL.  (Note the
two senses of "yolo" in this repo: elsewhere — e.g. `./build_yolo.sh` —
"yolo" just means a normal, unpizlonated compiler; "Yolo-C" here is the
flavor that additionally links this repo's yolo glibc.)

There is no wrapper script for this configuration: drive `make` directly
with a CC wrapper that knows the yolo link recipe and with `OPENSSL_HOME`
pointing at the yolo OpenSSL prefix — on this machine,
`make clean && make CC=/tmp/yolocc OPENSSL_HOME=/tmp/yolo-openssl-install`,
where `/tmp/yolocc` is the proven gcc wrapper with this behavior:

* compile-only (`-c`, `-E`, `-S`, `-M`, `-MM`) and info probes (`--version`,
  `-v`, `-print-*`, `-dump*`): plain `gcc`;
* `-shared`: `gcc` + pizfix `crtbegin.o`/`crtend.o` (they supply
  `__dso_handle` for the nonshared atexit stubs) + `-lyoloc -lgcc`, so the
  DSO gets `DT_NEEDED libyolocimpl.so` instead of `libc.so.6`;
* executable links: the same tail **plus** pizfix `crt1.o`/`crti.o`/`crtn.o`
  first and `-Wl,--dynamic-linker=<repo>/pizfix/lib/ld-fil1-x86_64.so`;
  the link is retried once with `-lgcc` if symbols like `__udivti3` are
  missing.

The yolo link tail is **appended after** the make command line, so the
Makefile's own `-L$(OPENSSL_HOME)/lib -Wl,-rpath,$(OPENSSL_HOME)/lib`
resolve first and the RUNPATH ends up as
`[yolo OpenSSL lib, yolo OpenSSL lib64, pizfix/lib]` — the pizlonated
libcrypto that also lives in `pizfix/lib` is therefore shadowed at runtime.

### As-built recipe for the yolo OpenSSL 3.6.5

Source: **pristine** OpenSSL 3.6.5 — either `projects/.openssl-3.6.5.tar.gz.snapshot`
or the extracted tree `projects/zunsafe-openssl3`.  **Do not** Configure out-of-tree
from `projects/openssl-build/extracted-source`: that tree carries ~6000
stale in-tree build artifacts (`Makefile`, `configdata.pm`, `.o`/`.d`
files) from the pizfix build, which break a fresh out-of-tree Configure.
(The Fil-C port's differences vs pristine 3.6.5 — 78 files as of this
writing, all
`__FILC__`-gated assembly disablers and build-system tweaks — are
pizlonation machinery only and irrelevant to a gcc build.)

```sh
R=/home/pizlo/Programs/fil-c-2
mkdir -p /tmp/yolo-openssl-build && cd /tmp/yolo-openssl-build
/path/to/pristine/openssl-3.6.5/Configure linux-x86_64 shared no-tests \
    --prefix=/tmp/yolo-openssl-install --libdir=lib64 CC=/tmp/yolocc
make -j && make install_sw
```

`/tmp/yolocc` is the wrapper described above.  `install_sw` produces
`/tmp/yolo-openssl-install/{include,bin,lib64}`; the `lib -> lib64` symlink
in that prefix exists so the Makefile's plain `-L<prefix>/lib` also works.
The pizfix build reports `OpenSSL 4.0.3 29 Sep 2026`; the yolo build reports
`OpenSSL 3.6.5 29 Sep 2026`.


## Poison options

Every working buffer is allocated by a labeled `ALLOCATE_*` function at the
top of each program (e.g. `ALLOCATE_KEY`, `ALLOCATE_INPUT`, `ALLOCATE_OUTPUT`,
`ALLOCATE_IV`, `ALLOCATE_AAD`, `ALLOCATE_TAG`, `ALLOCATE_SALT`,
`ALLOCATE_OTHER`), and each wrapper funnels into `poison_alloc()`, which is
implemented in `poisoning.c` and declared in `poisoning.h`.  When the user
requests a poison kind on the command line, `poison_alloc()` calls
`poison_range()` on what `malloc()` returned; otherwise it does not.
`poison_range()` is private to `poisoning.c` (tests poison buffers
exclusively through `poison_alloc()`) and forwards to the zegarmistrz
poisoning API (`<zegarmistrz.h>`, header-only) when a *variant* selection
is active (see the next section); without one it is a no-op — the single
hook where a real poisoning implementation (e.g. a Fil-C/GIMSO poisoning
API) can be installed without touching any test program.  `poison_alloc()` also
records every allocation it hands out — poisoned or not, along with
whether the poison was actually applied — in a resizable table private
to `poisoning.c`; that table is what powers `unpoison_all()`,
`repoison_all()`, and `free_poison_allocations()` (see
[Comparisons and freeing](#comparisons-and-freeing)).  Passing a poison
option must never change what a program computes — it only changes which
buffers the hook is called on.

| Option             | Effect                                          |
|--------------------|-------------------------------------------------|
| `--poison-input`   | call the hook on input buffers                  |
| `--poison-output`  | call the hook on output buffers                 |
| `--poison-key`     | call the hook on key buffers                    |
| `--poison-iv`      | call the hook on IV/nonce buffers               |
| `--poison-aad`     | call the hook on AAD buffers                    |
| `--poison-tag`     | call the hook on tag buffers                    |
| `--poison-salt`    | call the hook on salt/seed/info buffers         |
| `--poison-other`   | call the hook on any other working buffer       |
| `--poison-all`     | all of the above                                |
| `--poison-none`    | none of the above (the default)                 |
| `--poison=a,b,...` | the listed kinds (same names as above, plus `nonce` = `iv`, `seed`/`info` = `salt`, `all`) |
| `--poison-variants=s` | taint the poisoned ranges with the listed zegarmistrz variant(s) (see below) |
| `--poison-help`    | print the option list and exit 0                |

Options may be repeated and combine as a bitmask (`--poison-key --poison-iv`
equals `--poison=key,iv`; `--poison-none` resets).  `poison_parse_args()`
runs first in every `main()`, consumes `-h`/`--help` and only these
`--poison-*` options, and rejects anything else with exit code 2 — the
programs take no other input.

### Poisoning variants (zegarmistrz)

Under the zegarmistrz emulator, `poison_range()` (private to
`poisoning.c`) taints every poisoned buffer with the selected *variant* bits:

```sh
POISON_VARIANTS=cannot-branch,cannot-index ./test_aes_128_cbc --poison-all
./test_aes_128_cbc --poison-all --poison-variants=cannot-var-time-op
```

Accepted `--poison-variants=` tokens: `cannot-branch`, `cannot-index`,
`cannot-var-time-op`, plus `all` (the three of them OR'd) and `none`/the
empty spec (which clear the selection).  The spec *replaces* whatever was
selected before, and an unknown token exits 2 like the other poison
options.  Only these three `CANNOT_*` bits are exposed: zegarmistrz's
`CANNOT_LOAD`/`CANNOT_STORE` and `CLEAR_*_ON_STORE` bits are deliberately
not offered — loads and stores must keep working, and clear-on-store would
make the poisoning of long-lived key material evaporate on first use.

The `POISON_VARIANTS` environment variable provides the default
`--poison-variants=` spec, so poison runs need no per-test argument
plumbing; an explicit `--poison-variants=` on the command line overrides
it.  Poisoning only takes effect under the emulator — outside it every
poison option is a no-op, so poison options never change what a program
computes.

### Comparisons and freeing

`memcmp()` branches on the bytes it compares, so a comparison must never
run on poisoned data — under the emulator that fatally faults the program
(and taint spreads downstream: a ciphertext derived from a poisoned key is
tainted too, even though nobody poisoned it directly).  Every comparison
block in every test therefore runs unpoisoned:

```c

unpoison_all();

CHECK(memcmp(got, want, wantlen) == 0, what);

repoison_all();

```

`unpoison_all()` clears the poison on every allocation `poison_alloc()`
has recorded — including the downstream-tainted ones, which is what makes
the comparison safe.  `repoison_all()` restores the poison on exactly
those tracked allocations that were poisoned when they were made; it
deliberately does not poison allocations that were never poisoned, since
re-poisoning clean data would newly taint it.  Finally,
`free_poison_allocations()` unpoisons every tracked allocation, frees
them all, and empties the table.  It is safe to call any number of
times: after it returns, `poison_alloc()` can hand out fresh allocations
(the table keeps its capacity).  Tests call it as early as possible — at
the end of every test function (or, for per-iteration allocation loops,
at the bottom of the loop body) that allocated poison allocations, and
on the early-return/SKIP paths that would otherwise leave allocations
tracked; buffers allocated directly in `main()` and shared across test
functions are freed once, at the end of `main()`.  Note that
`unpoison_all()`
only covers the heap allocations `poison_alloc()` has recorded: taint
that lands in memory it does not track — stack buffers, or buffers
allocated inside libcrypto itself — is invisible to it and still faults
at the comparison.  Such a fault is a genuine finding about where a
computation touched tainted data, not a bug in the tests.

## Poisoning variant sweep

`run_variants.sh` runs every test under the zegarmistrz emulator once per
poisoning variant and writes a TSV of raw results plus a human-readable
report.  By default it runs whatever top-level `test_*` executables are
already built — it does not build anything; each binary is classified by the
libcrypto `ldd` resolves it to.  `--build` opts into building first:

```sh
./run_variants.sh [--build=zunsafe|sarcastic|yolo|both|all] [--jobs=N]
                  [--timeout=S] [--poison-args='--poison-all']
                  [--allow-no-poison] [--emulator=PATH]
                  [--yolo-openssl-prefix=PREFIX] [--output=PREFIX]
```

* (no `--build`, the default) prebuilt mode: sweep the existing `test_*`
  executables, classified via `ldd`: a pizfix libcrypto → `zunsafe`, a
  libcrypto under projects/sarcastic-openssl3 → `sarcastic`, a system
  libcrypto → `yolo`, and anything else → an `other` group keyed by that
  libcrypto's directory (those binaries are run too, and the directory is
  called out in the report).  Every non-empty group gets its own sweep
  section and its own `build: none (prebuilt binaries; classification via
  ldd)` provenance line; the report header records the per-group counts.
  With no test binaries at all, the script aborts (build some first with
  `--build`).
* `--build` (default: none) builds before sweeping: `zunsafe` = Fil-C's
  `build/bin/filcc` + the pizfix OpenSSL, `sarcastic` = filcc + the in-tree
  projects/sarcastic-openssl3, `yolo` = the system compiler (gcc) + the
  system OpenSSL, `both` = `zunsafe sarcastic`, and `all` = `zunsafe
  sarcastic yolo`.  Multiple builds rebuild and clean (exact binary names
  only) in between.  Each group's report section records the exact make
  invocation plus an `ldd` probe of the libcrypto the built tests actually
  bind.
* `--yolo-openssl-prefix=PREFIX` (only with `--build=yolo` or `--build=all`)
  builds the yolo flavor against the OpenSSL installed at PREFIX instead of
  the system one, as `make CC=gcc OPENSSL_HOME=PREFIX PIZFIX=`.  PREFIX
  must contain an `include/` directory and a libcrypto under `lib/` or
  `lib64/`.  The trailing `PIZFIX=` (a command-line make variable)
  neutralizes every pizfix-derived path in the Makefile, so no pizfix
  `-L`/`-rpath` leaks into the yolo binaries.  It is an error without a
  `--build` that includes yolo, since it only affects building.
* `--jobs` (default nproc/2 capped at 16) is the per-variant test
  parallelism; `--timeout` (default 120s) is the per-test wall clock.
* `--poison-args` (default `--poison-all`) selects which buffer kinds the
  tests poison; the sweep itself decides which zegarmistrz variant each
  run taints.  An explicitly empty `--poison-args=''` is refused (loudly,
  unless `--allow-no-poison` is also passed), because with no poisoned
  buffer no variant can ever fault and the sweep could only ever report
  PASS.
* `--output=PREFIX` writes `<PREFIX>.txt` (the report) and `<PREFIX>.tsv`
  (one `flavor  variant  test  exit  class  note` row per run); the
  default prefix is `/tmp/simple-openssl-variants-<timestamp>`.

The 8 variants swept, in report order: `none` — the control, where
poisoning is a no-op and every test must PASS — followed by the seven
non-empty selections over `cannot-branch`, `cannot-index`, and
`cannot-var-time-op`: `cannot-branch`, `cannot-index`,
`cannot-var-time-op`, `cannot-branch,cannot-index`,
`cannot-branch,cannot-var-time-op`, `cannot-index,cannot-var-time-op`, and
`cannot-branch,cannot-index,cannot-var-time-op`.  A test faulting under a
poisoning variant is a *result* (exit ≥ 128 → `FAULT`), not a script
failure: only build errors and usage errors abort.  The report summarizes
per-variant PASS/FAULT/TIMEOUT/FAIL/SKIP counts, lists the non-PASS tests
per variant, prints a fault matrix over the non-empty variants, and ends
with a VERDICT section.

The committed sweep report lives in this directory:
`zegarmistrz-variants-report.txt` (human-readable, with a native-controls
section appended) and `zegarmistrz-variants-report.tsv` (the raw rows).
Its header records the exact invocation, git HEAD, and post-processing
that produced it.  (Both files are force-added with `git add -f` because
this directory's `.gitignore` is a whitelist that does not list them.)
Both reports predate the per-cipher split: they list the old monolith
binary names (e.g. `test_aes_cbc`), which no longer exist —
`run_variants.sh` enumerates whatever `test_*` executables are present
at run time, so a fresh sweep covers all 275 per-cipher binaries; until
one is committed, these files remain the record of the monolith-era
sweep.

## Why some cryptosystems have small and large test programs

Several OpenSSL implementations pick different code paths depending on the
payload size, buffer alignment, and update pattern, so a 16-byte KAT and a
2 MiB streaming test do not exercise the same code.  Where that is the
case, the suite has an explicit small and an explicit
large binary per cipher: the six AES-GCM binaries (`test_aes_128_gcm_small`,
`test_aes_192_gcm_small`, `test_aes_256_gcm_small` and the matching
`_large` siblings), the five AES-CCM binaries (`test_aes_128_ccm_small`,
`test_aes_192_ccm_small`, `test_aes_256_ccm_small`, `test_aes_128_ccm_large`,
`test_aes_192_ccm_large`), the five AES-OCB binaries
(`test_aes_128_ocb_small`, `test_aes_192_ocb_small`, `test_aes_256_ocb_small`,
`test_aes_192_ocb_large`, `test_aes_256_ocb_large`), `test_aes_128_xts`,
`test_aes_256_xts` and `test_aes_256_xts_large`, `test_chacha20_small`,
`test_chacha20_large`, `test_chacha20_poly1305_small`,
`test_chacha20_poly1305_large`, and `test_rc4`, `test_rc4_40`,
`test_rc4_large`; other binaries combine the regimes in one file when
they fit.  The regimes below were
read out of the OpenSSL 3.6.5 sources in
`projects/openssl-build3/extracted-source` and each is exercised by the
corresponding tests:

| Implementation | Size-dependent behavior | Evidence (3.6.5) |
|---|---|---|
| GCM | Four regimes: `< 16` bytes go through the `mres` partial-state path; `16..127` whole blocks through the per-call GHASH; `>= 128` uses the `AES_GCM_ENC_BYTES` interleaved bulk kernel (encrypt and decrypt have separate dispatches); `>= 3072` gets GHASH chunked in `GHASH_CHUNK (3*1024)` pieces | `crypto/modes/gcm128.c:856-928`; `include/crypto/aes_platform.h:83-84` (`AES_GCM_ENC_BYTES/DEC_BYTES = 128`) dispatched at `providers/implementations/ciphers/cipher_aes_gcm_hw.c:69` (enc) and `:106` (dec); `crypto/modes/gcm128.c:350` |
| CCM | The total payload length must be announced (a `NULL` update) and is fixed by the first data update; AAD is length-encoded in regimes (the `L'` formatting in `CRYPTO_ccm128_setiv`/`aad`); defaults are `ivlen = 7` and `taglen = 12` | `providers/implementations/ciphers/ciphercommon_ccm.c:674-676` (length set) and `:705-716` (defaults `ctx->l = 8` ⇒ ivlen 7, `ctx->m = 12`); `crypto/modes/ccm128.c:68-117` |
| OCB | Nonce (IV) length must be `1..15`; partial final blocks are handled only when the caller signals the last call, so mid-stream updates must be multiples of 16 except the tail | `crypto/modes/ocb128.c:224` (nonce length check), `:323-330` (last-call partial handling) |
| SIV | S2V has separate branches for messages `< 16` bytes (pad-and-double) and `>= 16` (xorend); a context performs exactly one crypto operation per init (`crypto_ok` counts down), so a second payload update fails | `crypto/modes/siv128.c:101-140` (S2V branches), `:283-285` (`crypto_ok` countdown) |
| Key wrap | `AES-KW` input must be `>= 16` bytes and a multiple of 8; `AES-KWP` (wrap with padding) accepts any length `>= 1` | `crypto/modes/wrap128.c:55` (KW length rule), `:218-230` (KWP padding path) |
| CBC-CTS | One-shot only: an update shorter than one block fails and a second data update is rejected; CS1/CS2/CS3 differ in how the final two blocks are stolen | `providers/implementations/ciphers/cipher_cts.c:330-342` (one-shot/min-length checks); `crypto/modes/cts128.c:27-86` (general stealing), `:301-326` (final-block stealing) |
| XTS | Each update is its own data unit and the tweak never advances inside one; a data unit larger than 2^20 AES blocks (16 MiB) fails; ciphertext stealing happens in both encrypt and decrypt directions | `providers/implementations/ciphers/cipher_aes_xts.c:188-211` (per-update checks, size limit); `crypto/modes/xts128.c:23+` (stealing) |
| ChaCha20 | A 64-byte partial keystream buffer bridges uneven updates; the counter-wrap path chunks large updates around the 32-bit block counter | `providers/implementations/ciphers/cipher_chacha20_hw.c:37-92` |
| ChaCha20-Poly1305 | TLS-style records (`plen <= 64`) take a different path from bulk data; AAD must be consumed before payload; the final update pads and finishes the Poly1305 state | `providers/implementations/ciphers/cipher_chacha20_poly1305_hw.c:155-210` (TLS path), `:266-397` (AEAD path) |
| Poly1305 (MAC) | Updates shorter than 16 bytes are buffered until a full block or final arrives | `crypto/poly1305/poly1305.c:447-478` |
| MD32-family digests (SHA1/SHA2/MD5/RIPEMD/SM3/WHIRLPOOL) | Partial-block buffering in the shared `HASH_UPDATE` machinery, with a memcpy fast path at block boundaries | `include/crypto/md32_common.h:157-203` |
| SHA3/SHAKE/KECCAK | Rate-boundary buffering on update; `EVP_DigestSqueeze` buffers the leftover sponge state between squeezes | `crypto/sha/sha3.c:47-100` (sponge init/squeeze), `:136-215` (update/final buffering) |
| BLAKE2 | The last block is stashed until Final (the finalization bit flips the block) | `providers/implementations/digests/blake2b_prov.c:257-293` |
| EVP layer (all ciphers) | Fetched-provider vs legacy `EVP_*_ex` paths behave the same, and every update may emit up to `inl + block_size` bytes | `crypto/evp/evp_enc.c:885-973` (`evp_EncryptDecryptUpdate`), `:1015-1017` (the `inl + block_size` outsize contract) |
| Block ciphers with padding | CBC decryption holds back the last block until Final so padding can be verified | `providers/implementations/ciphers/ciphercommon.c:885-905` |

## Full test list

All 275 binaries, grouped into families.  Each family's shared
`test_<family>_common` module is named (binaries with no family module are
marked as such; they link only `test_common.o` and `poisoning.o`).  The two
count columns are the checks each binary prints under gcc + system OpenSSL
3.0.13 and under filcc + pizfix OpenSSL 3.6.5 (as recorded while pizfix held
3.6.5; a fresh per-binary check-count pass against pizfix OpenSSL 4.0.3
reproduced every one of these numbers exactly — the only 4.0.3 differences
are `test_sm2` and `test_chacha20_poly1305_small`, which fail outright on
the OpenSSL 4 behavior changes described in the support matrix);
`skip` marks a whole-program
version-gated SKIP on that toolchain (the reasons are inventoried
[above](#compiler-support-matrix-verified)).  The columns differ only where
a documented version gate exists — lower on 3.0.13 where a gated subtest
is skipped, higher where a 3.0-era behavior adds checks (e.g.
ChaCha20-Poly1305's AAD ordering).

### Exemplars

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cbc` | 92 | 92 | AES-128-CBC: the NIST SP 800-38A F.2.1 KAT row, fetched/static/`EVP_get_cipherbyname` routes, streaming updates, padding and padding-disabled failures, output-buffer contract |
| `test_aes_128_gcm_small` | 217 | 217 | AES-128-GCM with payloads <= 64 bytes: McGrew/Viega KATs with and without AAD, tamper detection, split updates, AAD ordering, IV lengths, short tags, zero-length payloads |
| `test_digest_sha2_256` | 154 | 154 | SHA2-256: FIPS 180 KATs (the one-block "abc", the 56-byte two-block-boundary and the 112-byte messages), routes, chunked-vs-one-shot equality around block boundaries |

### AES classical modes

#### AES-CBC

**`test_aes_cbc_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cbc` | 92 | 92 | AES-128-CBC: the NIST SP 800-38A F.2.1 KAT row, fetched/static/`EVP_get_cipherbyname` routes, streaming updates, padding and padding-disabled failures, output-buffer contract |
| `test_aes_192_cbc` | 92 | 92 | AES-192-CBC: the NIST SP 800-38A F.2.2 KAT row, fetched/static/`EVP_get_cipherbyname` routes, streaming updates, padding and padding-disabled failures, output-buffer contract |
| `test_aes_256_cbc` | 92 | 92 | AES-256-CBC: the NIST SP 800-38A F.2.3 KAT row, fetched/static/`EVP_get_cipherbyname` routes, streaming updates, padding and padding-disabled failures, output-buffer contract |

#### AES-ECB

**`test_aes_ecb_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_ecb` | 102 | 102 | AES-128-ECB: the SP 800-38A F.1.1 KAT rows, in-place operation, partial/streaming updates |
| `test_aes_192_ecb` | 102 | 102 | AES-192-ECB: the SP 800-38A F.1.2 KAT rows, in-place operation, partial/streaming updates |
| `test_aes_256_ecb` | 102 | 102 | AES-256-ECB: the SP 800-38A F.1.3 KAT rows, in-place operation, partial/streaming updates |

#### AES-CTR

**`test_aes_ctr_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_ctr` | 117 | 117 | AES-128-CTR: the SP 800-38A F.5.1 KAT rows, counter carrying across updates, partial final blocks |
| `test_aes_192_ctr` | 117 | 117 | AES-192-CTR: the SP 800-38A F.5.2 KAT rows, counter carrying across updates, partial final blocks |
| `test_aes_256_ctr` | 117 | 117 | AES-256-CTR: the SP 800-38A F.5.3 KAT rows, counter carrying across updates, partial final blocks |

#### AES-OFB

**`test_aes_ofb_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_ofb` | 99 | 99 | AES-128-OFB(128): the SP 800-38A F.4.1 KAT rows, byte-offset streaming across updates |
| `test_aes_192_ofb` | 99 | 99 | AES-192-OFB(128): the SP 800-38A F.4.2 KAT rows, byte-offset streaming across updates |
| `test_aes_256_ofb` | 99 | 99 | AES-256-OFB(128): the SP 800-38A F.4.3 KAT rows, byte-offset streaming across updates |

#### AES-CFB

**`test_aes_cfb_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cfb` | 86 | 86 | AES-128-CFB(128): the SP 800-38A F.3.1 KAT rows, streaming and in-place |
| `test_aes_192_cfb` | 86 | 86 | AES-192-CFB(128): the SP 800-38A F.3.2 KAT rows, streaming and in-place |
| `test_aes_256_cfb` | 86 | 86 | AES-256-CFB(128): the SP 800-38A F.3.3 KAT rows, streaming and in-place |

#### AES-CFB8

**`test_aes_cfb8_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cfb8` | 288 | 288 | AES-128-CFB8: the F.3 shared plaintext one byte at a time, streaming |
| `test_aes_192_cfb8` | 288 | 288 | AES-192-CFB8: the F.3 shared plaintext one byte at a time, streaming |
| `test_aes_256_cfb8` | 288 | 288 | AES-256-CFB8: the F.3 shared plaintext one byte at a time, streaming |

#### AES-CFB1

**`test_aes_cfb1_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cfb1` | 200 | 200 | AES-128-CFB1 in byte mode and bit-length mode (`EVP_CIPH_FLAG_LENGTH_BITS`) |
| `test_aes_192_cfb1` | 200 | 200 | AES-192-CFB1 in byte mode and bit-length mode (`EVP_CIPH_FLAG_LENGTH_BITS`) |
| `test_aes_256_cfb1` | 200 | 200 | AES-256-CFB1 in byte mode and bit-length mode (`EVP_CIPH_FLAG_LENGTH_BITS`) |

#### AES-XTS

**`test_aes_xts_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_xts` | 172 | 172 | AES-128-XTS small data units: IEEE 1619-2007 KATs, data-unit number as IV, key = two concatenated AES-128 keys, small-length and stealing behavior (stealing works in both directions) |
| `test_aes_256_xts` | 118 | 118 | AES-256-XTS small data units: IEEE 1619-2007 KATs, data-unit number as IV, key = two concatenated AES-256 keys, small-length and stealing behavior (stealing works in both directions) |
| `test_aes_256_xts_large` | 82 | 82 | AES-256-XTS at the 2^20-block data-unit boundary: 16 MiB accepted, 16 MiB + 1 rejected |

### AES AEAD / wrap

#### AES-GCM

**`test_aes_gcm_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_gcm_large` | 1890 | 1890 | AES-128-GCM with 2 MiB payloads fed through updates of 16, 17, 64, 100, 128, 129, 3071, 3072, 3073 and 65536 bytes (all four size regimes), self-consistency oracle |
| `test_aes_128_gcm_small` | 217 | 217 | AES-128-GCM with payloads <= 64 bytes: McGrew/Viega KATs with and without AAD, tamper detection, split updates, AAD ordering, IV lengths, short tags, zero-length payloads |
| `test_aes_192_gcm_large` | 1862 | 1862 | AES-192-GCM with 2 MiB payloads fed through updates of 16, 17, 64, 100, 128, 129, 3071, 3072, 3073 and 65536 bytes (all four size regimes), self-consistency oracle (that key size's tuple is scratch-derived; the RFC publishes 128/256 tuples) |
| `test_aes_192_gcm_small` | 193 | 193 | AES-192-GCM with payloads <= 64 bytes: McGrew/Viega KATs with and without AAD (that key size's tuple is scratch-derived; the RFC publishes 128/256 tuples), tamper detection, split updates, AAD ordering, IV lengths, short tags, zero-length payloads |
| `test_aes_256_gcm_large` | 1847 | 1847 | AES-256-GCM with 2 MiB payloads fed through updates of 16, 17, 64, 100, 128, 129, 3071, 3072, 3073 and 65536 bytes (all four size regimes), self-consistency oracle |
| `test_aes_256_gcm_small` | 193 | 193 | AES-256-GCM with payloads <= 64 bytes: McGrew/Viega KATs with and without AAD, tamper detection, split updates, AAD ordering, IV lengths, short tags, zero-length payloads |

#### AES-GCM-SIV

**`test_aes_gcm_siv_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_gcm_siv` | skip | 191 | RFC 8452 AES-128-GCM-SIV KATs (3.5+; whole-program SKIP on 3.0.13) |
| `test_aes_192_gcm_siv` | skip | 155 | RFC 8452 AES-192-GCM-SIV KATs (3.5+; whole-program SKIP on 3.0.13, scratch-derived 192 vectors) |
| `test_aes_256_gcm_siv` | skip | 155 | RFC 8452 AES-256-GCM-SIV KATs (3.5+; whole-program SKIP on 3.0.13) |

#### AES-CCM

**`test_aes_ccm_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_ccm_large` | 150 | 150 | AES-128-CCM with a 1 MiB single-update payload anchored to RFC 3610 #1 (the length is fixed by the first update and a second update is rejected) |
| `test_aes_128_ccm_small` | 4640 | 4640 | AES-128-CCM small payloads: RFC 3610 packet-vector rows (that key size's tuple is scratch-derived; the RFC publishes 128-bit-key vectors), tag lengths 4..16 even, nonce lengths 7..13, AAD ordering rules, the NULL-update length announcement, payload cap 65535 for 13-byte nonces |
| `test_aes_192_ccm_large` | 150 | 150 | AES-192-CCM with a 1 MiB single-update payload anchored to a derived RFC 3610 #1 tuple (RFC 3610 publishes 128-bit-key vectors only) |
| `test_aes_192_ccm_small` | 4606 | 4606 | AES-192-CCM small payloads: RFC 3610 packet-vector rows (that key size's tuple is scratch-derived; the RFC publishes 128-bit-key vectors), tag lengths 4..16 even, nonce lengths 7..13, AAD ordering rules, the NULL-update length announcement, payload cap 65535 for 13-byte nonces |
| `test_aes_256_ccm_small` | 4606 | 4606 | AES-256-CCM small payloads: RFC 3610 packet-vector rows (that key size's tuple is scratch-derived; the RFC publishes 128-bit-key vectors), tag lengths 4..16 even, nonce lengths 7..13, AAD ordering rules, the NULL-update length announcement, payload cap 65535 for 13-byte nonces |

#### AES-OCB

**`test_aes_ocb_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_ocb_small` | 742 | 742 | AES-128-OCB: RFC 7253 Appendix A tuples, nonce lengths 1..15, tag lengths, partial blocks, AAD after data, tamper |
| `test_aes_192_ocb_large` | 251 | 251 | AES-192-OCB with a 1 MiB payload through mixed 16384/16391-byte updates, self-consistency oracle (the 192 tuple is derived; RFC 7253 publishes 128/256 only), tampered final block |
| `test_aes_192_ocb_small` | 580 | 580 | AES-192-OCB: RFC 7253 Appendix A tuples (the 192 tuple is derived; RFC 7253 publishes 128/256 only), nonce lengths 1..15, tag lengths, partial blocks, AAD after data, tamper |
| `test_aes_256_ocb_large` | 251 | 251 | AES-256-OCB with a 1 MiB payload through mixed 16384/16391-byte updates, self-consistency oracle, tampered final block |
| `test_aes_256_ocb_small` | 580 | 580 | AES-256-OCB: RFC 7253 Appendix A tuples, nonce lengths 1..15, tag lengths, partial blocks, AAD after data, tamper |

#### AES-SIV

**`test_aes_siv_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_siv` | 162 | 170 | AES-128-SIV (RFC 5297): A.1/A.2-style KATs (that key size's tuple is scratch-derived; the RFC publishes 128-bit-key samples), S2V with multiple ADs, one crypto operation per init, tag-as-synthetic-IV, empty-payload version difference (3.0.x fails, 3.6.x computes the synthetic IV) |
| `test_aes_192_siv` | 162 | 170 | AES-192-SIV (RFC 5297): A.1/A.2-style KATs (that key size's tuple is scratch-derived; the RFC publishes 128-bit-key samples), S2V with multiple ADs, one crypto operation per init, tag-as-synthetic-IV, empty-payload version difference (3.0.x fails, 3.6.x computes the synthetic IV) |
| `test_aes_256_siv` | 162 | 170 | AES-256-SIV (RFC 5297): A.1/A.2-style KATs (that key size's tuple is scratch-derived; the RFC publishes 128-bit-key samples), S2V with multiple ADs, one crypto operation per init, tag-as-synthetic-IV, empty-payload version difference (3.0.x fails, 3.6.x computes the synthetic IV) |

#### AES key wrap and -WRAP-INV

**`test_aes_wrap_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_wrap` | 73 | 73 | AES-128-WRAP (RFC 3394 / SP 800-38F): the RFC 3394 KAT row in both the wrap and unwrap directions, the multiple-of-8 >= 16 length rules, unwrap error paths |
| `test_aes_128_wrap_inv` | 26 | 26 | AES-128-WRAP-INV: self-consistency roundtrips under the same length rules (the wrap/unwrap primitives exchanged per SP 800-38F 5.1; the RFC has no -INV examples, so no KAT constants) |
| `test_aes_192_wrap` | 73 | 73 | AES-192-WRAP (RFC 3394 / SP 800-38F): the RFC 3394 KAT row in both the wrap and unwrap directions, the multiple-of-8 >= 16 length rules, unwrap error paths |
| `test_aes_192_wrap_inv` | 26 | 26 | AES-192-WRAP-INV: self-consistency roundtrips under the same length rules (the wrap/unwrap primitives exchanged per SP 800-38F 5.1; the RFC has no -INV examples, so no KAT constants) |
| `test_aes_256_wrap` | 73 | 73 | AES-256-WRAP (RFC 3394 / SP 800-38F): the RFC 3394 KAT row in both the wrap and unwrap directions, the multiple-of-8 >= 16 length rules, unwrap error paths |
| `test_aes_256_wrap_inv` | 26 | 26 | AES-256-WRAP-INV: self-consistency roundtrips under the same length rules (the wrap/unwrap primitives exchanged per SP 800-38F 5.1; the RFC has no -INV examples, so no KAT constants) |

#### AES-WRAP-PAD and -WRAP-PAD-INV

**`test_aes_wrap_pad_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_wrap_pad` | 123 | 123 | AES-128-WRAP-PAD (RFC 5649): a derived KAT (the RFC publishes only the 192-bit example), arbitrary lengths >= 1, the A65959A6 header, corrupted-unwrap rejection |
| `test_aes_128_wrap_pad_inv` | 20 | 20 | AES-128-WRAP-PAD-INV: derived -INV KAT roundtrips with identical output-size math (RFC 5649 has no 128-bit -INV example) |
| `test_aes_192_wrap_pad` | 157 | 157 | AES-192-WRAP-PAD (RFC 5649): the published RFC 5649 section 6 KAT (the only published wrap-pad example), arbitrary lengths >= 1, the A65959A6 header, corrupted-unwrap rejection |
| `test_aes_192_wrap_pad_inv` | 35 | 35 | AES-192-WRAP-PAD-INV: derived -INV KAT roundtrips with identical output-size math (RFC 5649 has no 192-bit -INV example) |
| `test_aes_256_wrap_pad` | 140 | 140 | AES-256-WRAP-PAD (RFC 5649): a derived KAT (the RFC publishes only the 192-bit example), arbitrary lengths >= 1, the A65959A6 header, corrupted-unwrap rejection |
| `test_aes_256_wrap_pad_inv` | 35 | 35 | AES-256-WRAP-PAD-INV: derived -INV KAT roundtrips with identical output-size math (RFC 5649 has no 256-bit -INV example) |

#### CBC ciphertext stealing (AES and CAMELLIA)

**`test_cbc_cts_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cbc_cts` | 369 | 369 | AES-128-CBC-CTS: CS1/CS2/CS3 via `cts_mode`, the one-shot rule, the minimum length, the CS1/CS2/CS3 equality relations, roundtrips |
| `test_aes_192_cbc_cts` | 337 | 337 | AES-192-CBC-CTS: CS1/CS2/CS3 via `cts_mode`, the one-shot rule, the minimum length, the CS1/CS2/CS3 equality relations, roundtrips |
| `test_aes_256_cbc_cts` | 337 | 337 | AES-256-CBC-CTS: CS1/CS2/CS3 via `cts_mode`, the one-shot rule, the minimum length, the CS1/CS2/CS3 equality relations, roundtrips |
| `test_camellia_128_cbc_cts` | 348 | 348 | CAMELLIA-128-CBC-CTS: CS1/CS2/CS3 via `cts_mode`, the one-shot rule, the minimum length, the CS1/CS2/CS3 equality relations, roundtrips |
| `test_camellia_192_cbc_cts` | 337 | 337 | CAMELLIA-192-CBC-CTS: CS1/CS2/CS3 via `cts_mode`, the one-shot rule, the minimum length, the CS1/CS2/CS3 equality relations, roundtrips |
| `test_camellia_256_cbc_cts` | 337 | 337 | CAMELLIA-256-CBC-CTS: CS1/CS2/CS3 via `cts_mode`, the one-shot rule, the minimum length, the CS1/CS2/CS3 equality relations, roundtrips |

#### Stitched AES-CBC-HMAC TLS-record ciphers

**`test_cbc_hmac_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aes_128_cbc_hmac_sha1` | 308 | 308 | the stitched AES-128-CBC-HMAC-SHA1 TLS-record cipher: record-format composition (the KAT record is rebuilt from separate AES-CBC + HMAC-SHA1 calls), TLS AAD layout, variable-length records, roundtrips |
| `test_aes_128_cbc_hmac_sha256` | 308 | 308 | the stitched AES-128-CBC-HMAC-SHA256 TLS-record cipher, same record format as the SHA1 family but a 32-byte MAC: composition, TLS AAD layout, variable-length records, roundtrips |
| `test_aes_128_cbc_hmac_sha256_etm` | skip | skip | the encrypt-then-MAC AES-128-CBC-HMAC-SHA256-ETM variant: registered only on ARM64 builds of OpenSSL 3.5+, so the whole program takes the SKIP path on x86 (3.0.13 and 3.6.5) |
| `test_aes_192_cbc_hmac_sha256_etm` | skip | skip | the encrypt-then-MAC AES-192-CBC-HMAC-SHA256-ETM variant: registered only on ARM64 builds of OpenSSL 3.5+, so the whole program takes the SKIP path on x86 (3.0.13 and 3.6.5) |
| `test_aes_256_cbc_hmac_sha1` | 306 | 306 | the stitched AES-256-CBC-HMAC-SHA1 TLS-record cipher: record-format composition (the KAT record is rebuilt from separate AES-CBC + HMAC-SHA1 calls), TLS AAD layout, variable-length records, roundtrips |
| `test_aes_256_cbc_hmac_sha256` | 306 | 306 | the stitched AES-256-CBC-HMAC-SHA256 TLS-record cipher, same record format as the SHA1 family but a 32-byte MAC: composition, TLS AAD layout, variable-length records, roundtrips |
| `test_aes_256_cbc_hmac_sha256_etm` | skip | skip | the encrypt-then-MAC AES-256-CBC-HMAC-SHA256-ETM variant: registered only on ARM64 builds of OpenSSL 3.5+, so the whole program takes the SKIP path on x86 (3.0.13 and 3.6.5) |

### Non-AES symmetric ciphers

#### CAMELLIA

**`test_camellia_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_camellia_128_cbc` | 84 | 84 | CAMELLIA-128-CBC: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming and padding rules |
| `test_camellia_128_cfb` | 58 | 58 | CAMELLIA-128-CFB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_128_cfb1` | 90 | 90 | CAMELLIA-128-CFB1: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming; CFB1 in byte mode and bit-length mode |
| `test_camellia_128_cfb8` | 58 | 58 | CAMELLIA-128-CFB8: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_128_ctr` | 62 | 62 | CAMELLIA-128-CTR: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_128_ecb` | 56 | 56 | CAMELLIA-128-ECB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming and in-place operation |
| `test_camellia_128_ofb` | 58 | 58 | CAMELLIA-128-OFB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_192_cbc` | 84 | 84 | CAMELLIA-192-CBC: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming and padding rules |
| `test_camellia_192_cfb` | 58 | 58 | CAMELLIA-192-CFB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_192_cfb1` | 90 | 90 | CAMELLIA-192-CFB1: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming; CFB1 in byte mode and bit-length mode |
| `test_camellia_192_cfb8` | 58 | 58 | CAMELLIA-192-CFB8: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_192_ctr` | 62 | 62 | CAMELLIA-192-CTR: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_192_ecb` | 56 | 56 | CAMELLIA-192-ECB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming and in-place operation |
| `test_camellia_192_ofb` | 58 | 58 | CAMELLIA-192-OFB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_256_cbc` | 84 | 84 | CAMELLIA-256-CBC: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming and padding rules |
| `test_camellia_256_cfb` | 58 | 58 | CAMELLIA-256-CFB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_256_cfb1` | 90 | 90 | CAMELLIA-256-CFB1: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming; CFB1 in byte mode and bit-length mode |
| `test_camellia_256_cfb8` | 58 | 58 | CAMELLIA-256-CFB8: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_256_ctr` | 62 | 62 | CAMELLIA-256-CTR: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |
| `test_camellia_256_ecb` | 56 | 56 | CAMELLIA-256-ECB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming and in-place operation |
| `test_camellia_256_ofb` | 58 | 58 | CAMELLIA-256-OFB: the RFC 3713 KAT row, CLI-verified mode/key-size constants, streaming |

#### ARIA

**`test_aria_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_aria_128_cbc` | 44 | 44 | ARIA-128-CBC: KATs, streaming and padding rules |
| `test_aria_128_ccm` | 127 | 127 | ARIA-128-CCM: a scratch-derived KAT tuple (ivlen 12, taglen 12), the NULL-update length announcement, AAD ordering |
| `test_aria_128_cfb` | 40 | 40 | ARIA-128-CFB: KATs, streaming |
| `test_aria_128_cfb1` | 40 | 40 | ARIA-128-CFB1: KATs, streaming (byte mode) |
| `test_aria_128_cfb8` | 40 | 40 | ARIA-128-CFB8: KATs, streaming |
| `test_aria_128_ctr` | 44 | 44 | ARIA-128-CTR: KATs, streaming |
| `test_aria_128_ecb` | 38 | 38 | ARIA-128-ECB: KATs, streaming and in-place operation |
| `test_aria_128_gcm` | 76 | 76 | ARIA-128-GCM: a scratch-derived KAT tuple, IV length set before init, AAD-before-data ordering, tamper detection |
| `test_aria_128_ofb` | 40 | 40 | ARIA-128-OFB: KATs, streaming |
| `test_aria_192_cbc` | 44 | 44 | ARIA-192-CBC: KATs, streaming and padding rules |
| `test_aria_192_ccm` | 127 | 127 | ARIA-192-CCM: a scratch-derived KAT tuple (ivlen 12, taglen 12), the NULL-update length announcement, AAD ordering |
| `test_aria_192_cfb` | 40 | 40 | ARIA-192-CFB: KATs, streaming |
| `test_aria_192_cfb1` | 40 | 40 | ARIA-192-CFB1: KATs, streaming (byte mode) |
| `test_aria_192_cfb8` | 40 | 40 | ARIA-192-CFB8: KATs, streaming |
| `test_aria_192_ctr` | 44 | 44 | ARIA-192-CTR: KATs, streaming |
| `test_aria_192_ecb` | 38 | 38 | ARIA-192-ECB: KATs, streaming and in-place operation |
| `test_aria_192_gcm` | 76 | 76 | ARIA-192-GCM: a scratch-derived KAT tuple, IV length set before init, AAD-before-data ordering, tamper detection |
| `test_aria_192_ofb` | 40 | 40 | ARIA-192-OFB: KATs, streaming |
| `test_aria_256_cbc` | 44 | 44 | ARIA-256-CBC: KATs, streaming and padding rules |
| `test_aria_256_ccm` | 127 | 127 | ARIA-256-CCM: a scratch-derived KAT tuple (ivlen 12, taglen 12), the NULL-update length announcement, AAD ordering |
| `test_aria_256_cfb` | 40 | 40 | ARIA-256-CFB: KATs, streaming |
| `test_aria_256_cfb1` | 40 | 40 | ARIA-256-CFB1: KATs, streaming (byte mode) |
| `test_aria_256_cfb8` | 40 | 40 | ARIA-256-CFB8: KATs, streaming |
| `test_aria_256_ctr` | 44 | 44 | ARIA-256-CTR: KATs, streaming |
| `test_aria_256_ecb` | 38 | 38 | ARIA-256-ECB: KATs, streaming and in-place operation |
| `test_aria_256_gcm` | 76 | 76 | ARIA-256-GCM: a scratch-derived KAT tuple, IV length set before init, AAD-before-data ordering, tamper detection |
| `test_aria_256_ofb` | 40 | 40 | ARIA-256-OFB: KATs, streaming |

#### SM4

**`test_sm4_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_sm4_cbc` | 65 | 65 | SM4-CBC: the GB/T 32907-2016 standard vector, CLI-verified mode constants, streaming and padding rules |
| `test_sm4_ccm` | skip | 35 | SM4-CCM (3.1+): derived KAT (ivlen 12, taglen 12), the NULL-update length announcement; whole-program SKIP on 3.0.13 |
| `test_sm4_cfb` | 30 | 30 | SM4-CFB: the GB/T 32907-2016 standard vector, CLI-verified mode constants, streaming |
| `test_sm4_ctr` | 34 | 34 | SM4-CTR: the GB/T 32907-2016 standard vector, CLI-verified mode constants, streaming |
| `test_sm4_ecb` | 59 | 59 | SM4-ECB: the GB/T 32907-2016 standard vector, CLI-verified mode constants, streaming |
| `test_sm4_gcm` | skip | 40 | SM4-GCM (3.1+): derived KAT, IV length before init, AAD ordering, tamper detection; whole-program SKIP on 3.0.13 |
| `test_sm4_ofb` | 30 | 30 | SM4-OFB: the GB/T 32907-2016 standard vector, CLI-verified mode constants, streaming |
| `test_sm4_xts` | skip | 41 | SM4-XTS (3.1+): derived KAT, a data unit shorter than 16 bytes fails in both directions, the 21-byte ciphertext-stealing roundtrip; whole-program SKIP on 3.0.13 |

#### DES, DESX, two-key DES-EDE

**`test_des_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_des_cbc` | 144 | 144 | DES-CBC (legacy provider): KATs, streaming and padding rules |
| `test_des_cfb` | 75 | 75 | DES-CFB (legacy provider): KATs, streaming |
| `test_des_cfb1` | 37 | 37 | DES-CFB1 (legacy provider): KATs, streaming (byte mode only — the provider ignores `EVP_CIPH_FLAG_LENGTH_BITS`) |
| `test_des_cfb8` | 135 | 135 | DES-CFB8 (legacy provider): KATs, streaming |
| `test_des_ecb` | 144 | 144 | DES-ECB (legacy provider): KATs, streaming and in-place operation |
| `test_des_ede` | 144 | 144 | two-key DES-EDE (ECB, default provider): KATs, in-place operation |
| `test_des_ede_cbc` | 144 | 144 | two-key DES-EDE-CBC (default provider): KATs, padding rules |
| `test_des_ede_cfb` | 75 | 75 | two-key DES-EDE-CFB (CFB64, default provider): KATs, the shift register plus num offset carried across partial updates |
| `test_des_ede_ofb` | 75 | 75 | two-key DES-EDE-OFB (default provider): KATs, the shift register carried across updates |
| `test_des_ofb` | 75 | 75 | DES-OFB (legacy provider): KATs, streaming |
| `test_desx_cbc` | 177 | 177 | DESX-CBC (legacy provider): the 24-byte key = key \|\| input whitening \|\| output whitening, KAT rebuilt from per-block DES-ECB calls |

#### DES-EDE3 (three-key) and DES3-WRAP

**`test_des3_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_des_ede3` | 64 | 64 | three-key DES-EDE3-ECB: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des_ede3_cbc` | 62 | 62 | three-key DES-EDE3-CBC: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des_ede3_cfb` | 50 | 50 | three-key DES-EDE3-CFB: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des_ede3_cfb1` | 50 | 50 | three-key DES-EDE3-CFB1: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des_ede3_cfb8` | 50 | 50 | three-key DES-EDE3-CFB8: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des_ede3_ofb` | 50 | 50 | three-key DES-EDE3-OFB: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des3_wrap` | 92 | 92 | DES3-WRAP (RFC 3217 CMS triple-DES key wrap): the modulo-8-only input rule (no >= 16 minimum), the `in + 16` output (random IV \|\| payload \|\| SHA-1 ICV), unwrap >= 24 and ICV rejection |

#### RC4

**`test_rc4_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_rc4` | 1360 | 1360 | RC4 (legacy provider): the classic "Key"/"Wiki"/"Secret" vectors via `set_key_length`, stream state carried across updates, the 8-byte bulk-path boundaries |
| `test_rc4_40` | 1554 | 1554 | RC4-40 (legacy provider): the CLI-derived vector via `set_key_length`, the 40-bit effective key |
| `test_rc4_large` | 661 | 661 | RC4 at >= 64 KiB through odd update grids: chunked output must equal the one-shot keystream exactly (self-consistency oracle) |
| `test_rc4_hmac_md5` | 265 | 265 | the stitched RC4-HMAC-MD5 TLS-record cipher (legacy provider): record layout (13-byte TLS AAD, MAC key via `SET_MAC_KEY`, `L + 16` body in one update, RC4 over payload \|\| MAC), KAT rebuilt from separate HMAC-MD5 + RC4 calls |

#### ChaCha20 and ChaCha20-Poly1305

**No family module** — these binaries are self-contained: shared boilerplate (`test_common.o`, `poisoning.o`) only.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_chacha20_large` | 17000 | 17000 | ChaCha20 at block-boundary and counter-carry sizes: the 32-bit block counter wrapping into the first nonce word (IV counter 0xffffffff), one-shot vs chunk-grid equality |
| `test_chacha20_poly1305_large` | 200 | 200 | ChaCha20-Poly1305 at >= 1 MiB: the AEAD bulk path, arbitrary chunk grids against the one-shot oracle, AAD sizes {0, 1, 15, 16, 17, 32} covering the mod-16 padding rules |
| `test_chacha20_poly1305_small` | 177 | 168 | ChaCha20-Poly1305 small inputs: the RFC 8439 section 2.8.2 KAT, AAD-before-data ordering (version difference pinned), tag-length rules including the no-`SET_TAG` decrypt that verifies nothing |
| `test_chacha20_small` | 215 | 215 | ChaCha20 small inputs: RFC 8439 section 2.4.2 keystream and ciphertext, the 16-byte IV = little-endian block counter \|\| nonce, partial-keystream bridging across uneven updates |

#### Legacy-provider block cipher sweep (CAST5, Blowfish, IDEA, SEED, RC2)

**`test_legacy_block_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_bf_cbc` | 40 | 40 | Blowfish-CBC (legacy provider): one KAT, roundtrip and streaming check |
| `test_bf_cfb` | 48 | 48 | Blowfish-CFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_bf_ecb` | 40 | 40 | Blowfish-ECB (legacy provider): one KAT, roundtrip and streaming check |
| `test_bf_ofb` | 48 | 48 | Blowfish-OFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_cast5_cbc` | 40 | 40 | CAST5-CBC (legacy provider): one KAT, roundtrip and streaming check |
| `test_cast5_cfb` | 48 | 48 | CAST5-CFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_cast5_ecb` | 40 | 40 | CAST5-ECB (legacy provider): one KAT, roundtrip and streaming check |
| `test_cast5_ofb` | 48 | 48 | CAST5-OFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_idea_cbc` | skip | 40 | IDEA-CBC (legacy provider): one KAT, roundtrip and streaming check; whole-program SKIP on builds without IDEA (3.0.13 here) |
| `test_idea_cfb` | skip | 40 | IDEA-CFB (legacy provider): one KAT, roundtrip and streaming check; whole-program SKIP on builds without IDEA (3.0.13 here) |
| `test_idea_ecb` | skip | 40 | IDEA-ECB (legacy provider): one KAT, roundtrip and streaming check; whole-program SKIP on builds without IDEA (3.0.13 here) |
| `test_idea_ofb` | skip | 40 | IDEA-OFB (legacy provider): one KAT, roundtrip and streaming check; whole-program SKIP on builds without IDEA (3.0.13 here) |
| `test_rc2_40_cbc` | 40 | 40 | RC2-40-CBC (legacy provider): the fixed 5-byte key pinned via `EVP_CIPHER_get_key_length`, KAT/roundtrip/streaming |
| `test_rc2_64_cbc` | 40 | 40 | RC2-64-CBC (legacy provider): the fixed 8-byte key (64 bits, not 6) pinned via `EVP_CIPHER_get_key_length`, KAT/roundtrip/streaming |
| `test_rc2_cbc` | 40 | 40 | RC2-CBC (legacy provider): one KAT, roundtrip and streaming check |
| `test_rc2_cfb` | 48 | 48 | RC2-CFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_rc2_ecb` | 40 | 40 | RC2-ECB (legacy provider): one KAT, roundtrip and streaming check |
| `test_rc2_ofb` | 48 | 48 | RC2-OFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_seed_cbc` | 40 | 40 | SEED-CBC (legacy provider): one KAT, roundtrip and streaming check |
| `test_seed_cfb` | 40 | 40 | SEED-CFB (legacy provider): one KAT, roundtrip and streaming check |
| `test_seed_ecb` | 40 | 40 | SEED-ECB (legacy provider): one KAT, roundtrip and streaming check |
| `test_seed_ofb` | 40 | 40 | SEED-OFB (legacy provider): one KAT, roundtrip and streaming check |

### Digests and MACs

#### SHA-1

**No family module** — these binaries are self-contained: shared boilerplate (`test_common.o`, `poisoning.o`) only.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_sha1` | 107 | 107 | SHA-1: KATs (empty message, one block, two-block boundary), routes, chunked-vs-one-shot |

#### SHA-2

**`test_digest_sha2_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_sha2_224` | 142 | 142 | SHA2-224: FIPS 180 KATs (the one-block "abc", the 56-byte two-block-boundary and the 112-byte messages), routes, chunked-vs-one-shot equality around block boundaries |
| `test_digest_sha2_256` | 154 | 154 | SHA2-256: FIPS 180 KATs (the one-block "abc", the 56-byte two-block-boundary and the 112-byte messages), routes, chunked-vs-one-shot equality around block boundaries |
| `test_digest_sha2_256_192` | skip | 12 | the SHA2-256/192 truncation (3.5+; whole-program SKIP on 3.0.13): KATs, chunked-vs-one-shot equality |
| `test_digest_sha2_384` | 142 | 142 | SHA2-384: FIPS 180 KATs (the one-block "abc", the 56-byte two-block-boundary and the 112-byte messages), routes, chunked-vs-one-shot equality around block boundaries |
| `test_digest_sha2_512` | 154 | 154 | SHA2-512: FIPS 180 KATs (the one-block "abc", the 56-byte two-block-boundary and the 112-byte messages), routes, chunked-vs-one-shot equality around block boundaries |
| `test_digest_sha2_512_224` | 142 | 142 | the SHA2-512/224 truncation: KATs, chunked-vs-one-shot equality around block boundaries |
| `test_digest_sha2_512_256` | 142 | 142 | the SHA2-512/256 truncation: KATs, chunked-vs-one-shot equality around block boundaries |

#### SHA-3 / KECCAK

**`test_digest_sha3_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_keccak_224` | skip | 322 | KECCAK-224: KATs, rate-boundary streaming (the 3.5+ KECCAK-* names; whole-program SKIP on 3.0.13) |
| `test_digest_keccak_256` | skip | 306 | KECCAK-256: KATs, rate-boundary streaming (the 3.5+ KECCAK-* names; whole-program SKIP on 3.0.13) |
| `test_digest_keccak_384` | skip | 242 | KECCAK-384: KATs, rate-boundary streaming (the 3.5+ KECCAK-* names; whole-program SKIP on 3.0.13) |
| `test_digest_keccak_512` | skip | 178 | KECCAK-512: KATs, rate-boundary streaming (the 3.5+ KECCAK-* names; whole-program SKIP on 3.0.13) |
| `test_digest_sha3_224` | 322 | 322 | SHA3-224: KATs, rate-boundary streaming |
| `test_digest_sha3_256` | 315 | 315 | SHA3-256: KATs, rate-boundary streaming |
| `test_digest_sha3_384` | 242 | 242 | SHA3-384: KATs, rate-boundary streaming |
| `test_digest_sha3_512` | 178 | 178 | SHA3-512: KATs, rate-boundary streaming |

#### SHAKE / KECCAK-KMAC

**`test_digest_shake_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_keccak_kmac_128` | 28 | 28 | KECCAK-KMAC-128: the digest form of KMAC-128, KATs |
| `test_digest_keccak_kmac_256` | 28 | 28 | KECCAK-KMAC-256: the digest form of KMAC-256, KATs |
| `test_digest_shake128` | skip | 50 | SHAKE128 XOF: KATs, the two output-length routes, `EVP_DigestSqueeze` (3.5+; whole-program SKIP on 3.0.13) |
| `test_digest_shake256` | skip | 50 | SHAKE256 XOF: KATs, the two output-length routes, `EVP_DigestSqueeze` (3.5+; whole-program SKIP on 3.0.13) |

#### MD5 / MD4 / MD5-SHA1

**`test_digest_md5_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_md4` | 91 | 91 | MD4 (legacy provider): KATs |
| `test_digest_md5` | 96 | 96 | MD5: KATs |
| `test_digest_md5_sha1` | 105 | 105 | MD5-SHA1: the two-digest composition (MD5 \|\| SHA1, 36 bytes) behind one EVP_MD name |

#### RIPEMD-160 / SM3

**`test_digest_ripemd_sm3_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_ripemd160` | 120 | 120 | RIPEMD-160: KATs, streaming |
| `test_digest_sm3` | 117 | 117 | SM3: KATs, streaming |

#### MDC2 / WHIRLPOOL

**`test_digest_mdc2_whirlpool_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_mdc2` | skip | 17 | MDC2 (legacy provider): KATs — MDC2 is compiled out of Ubuntu's 3.0.13, so whole-program SKIP there |
| `test_digest_whirlpool` | 28 | 28 | WHIRLPOOL (legacy provider): KATs |

#### BLAKE2 digests

**`test_digest_blake2_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_digest_blake2b` | 573 | 573 | BLAKE2b-512: RFC 7693-style vectors, block-boundary streaming |
| `test_digest_blake2s` | 589 | 589 | BLAKE2s-256: RFC 7693-style vectors, block-boundary streaming |

#### The NULL identity cipher and digest

**`test_null_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_null_cipher` | 38 | 38 | the NULL identity cipher (0-byte key, 0-byte IV, 1-byte block size) via `EVP_CIPHER_fetch` and `EVP_enc_null()`: identity roundtrips, zero lengths, verified behaviorally (it has no OBJ name, so no byname route) |
| `test_null_digest` | 18 | 18 | the NULL identity digest (0-byte output for any input) via `EVP_MD_fetch` and `EVP_md_null()`: zero lengths and input-independence, verified behaviorally |

#### MACs

**`test_mac_cmac_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_mac_cmac_aes_128_cbc` | 41 | 41 | CMAC over AES-128-CBC (RFC 4493): the four example KATs (the empty-message example doubles as the subkey-generation check), the 40-byte example 3 message split as {17, 23} partial updates equal to one-shot, mac size 16 (the AES block size), a wrong cipher (AES-256-CBC, key-length mismatch) rejected at init |
| `test_mac_cmac_aes_192_cbc` | 41 | 41 | CMAC over AES-192-CBC (SP 800-38B): the four example KATs over the RFC 4493 message, partial updates equal one-shot, mac size 16 (the AES block size), a wrong cipher (AES-256-CBC, key-length mismatch) rejected at init |
| `test_mac_cmac_aes_256_cbc` | 41 | 41 | CMAC over AES-256-CBC (SP 800-38B): the four example KATs over the RFC 4493 message, partial updates equal one-shot, mac size 16 (the AES block size), a wrong cipher (AES-128-CBC, key-length mismatch) rejected at init |
| `test_mac_cmac_des_ede3_cbc` | 23 | 23 | CMAC over DES-EDE3-CBC: a CLI-derived KAT (no published vector; cross-checked on 3.0.13 and 3.6.5), partial updates equal one-shot, mac size 8 (the DES block size), a wrong cipher (AES-128-CBC, key-length mismatch) rejected at init |

**`test_mac_hmac_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_mac_hmac_md5` | 101 | 101 | HMAC-MD5 (RFC 2202): mac size = digest size probed after a plain init, the six RFC 2202 rows (TC1-4, 6, 7 — TC6/TC7 use keys longer than the block), one-shot and chunked ({1, 17} updates), the empty key equal to the zero-padded 32- and 64-byte keys, a nonexistent digest name rejected |
| `test_mac_hmac_sha1` | 101 | 101 | HMAC-SHA1 (RFC 2202): mac size = digest size, the six RFC 2202 rows, one-shot and chunked, the empty key equal to the zero-padded 32- and 64-byte keys, a nonexistent digest name rejected |
| `test_mac_hmac_sha256` | 97 | 97 | HMAC-SHA256 (RFC 4231): mac size = digest size, the six RFC 4231 rows (TC5 is a truncation case EVP cannot express), one-shot and chunked, the empty key equal to the zero-padded keys anchored to a CLI-verified empty-key KAT, a nonexistent digest name rejected |
| `test_mac_hmac_sha384` | 97 | 97 | HMAC-SHA384 (RFC 4231): mac size = digest size, the six RFC 4231 rows, one-shot and chunked, the empty key equal to the zero-padded keys, a nonexistent digest name rejected |
| `test_mac_hmac_sha512` | 97 | 97 | HMAC-SHA512 (RFC 4231): mac size = digest size, the six RFC 4231 rows, one-shot and chunked, the empty key equal to the zero-padded keys, a nonexistent digest name rejected |

**No family module** — these binaries are self-contained: shared boilerplate (`test_common.o`, `poisoning.o`) only.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_mac_gmac` | 83 | 83 | GMAC (GCM restricted to AAD-only input): the TC1 KAT, an AAD-only KAT cross-checked against the GCM cipher path |
| `test_mac_poly1305` | 42 | 42 | Poly1305 MAC: RFC 8439 section 2.5.2, 32-byte `r\|\|s` key |
| `test_mac_siphash` | 121 | 121 | SipHash-2-4: KATs, 64-bit and 128-bit tag lengths |

**`test_mac_kmac_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_mac_kmac128` | 74 | 74 | KMAC-128 (SP 800-185): NIST samples, variable output length, `custom` as OCTET_STRING |
| `test_mac_kmac256` | 61 | 61 | KMAC-256 (SP 800-185): NIST samples, variable output length, `custom` as OCTET_STRING |

**`test_mac_blake2_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_mac_blake2b` | 49 | 49 | keyed BLAKE2b via `EVP_MAC` "BLAKE2BMAC": key lengths and output sizes |
| `test_mac_blake2s` | 43 | 43 | keyed BLAKE2s via `EVP_MAC` "BLAKE2SMAC": key lengths and output sizes |

### KDFs

**No family module** — these binaries are self-contained: shared boilerplate (`test_common.o`, `poisoning.o`) only.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_scrypt` | 27 | 27 | scrypt (RFC 7914): the two published 64-byte vectors, N/r validation, `maxmem_bytes` |
| `test_kdf_tls13kdf` | 34 | 34 | TLS 1.3 key schedule: extract-stage secrets, derive-secret-and-extract, HKDF-Expand-Label, error paths |
| `test_kdf_x942kdf` | 29 | 29 | X9.42 KDF (RFC 2631 KEK derivation, ASN.1 OtherInfo) |
| `test_kdf_x963kdf` | 23 | 23 | X9.63 KDF ("concat KDF"): NIST SHA-256 vectors |

**`test_kdf_hkdf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_hkdf_sha1` | 34 | 35 | HKDF (RFC 5869) for SHA-1: the TC1 OKM KAT, EXTRACT_ONLY (mode, digest-size PRK, derive — no verified SHA-1 PRK constant), EXPAND_ONLY from the extracted PRK, multi-chunk info against a single-info reference derive, error paths; notes where more than five info params are accepted (pre-3.1) |
| `test_kdf_hkdf_sha256` | 56 | 72 | HKDF (RFC 5869) for SHA-256, the family's representative digest: TC1-3 KATs, EXTRACT_ONLY PRK KATs, EXPAND_ONLY from TC1's PRK, multi-chunk info, error paths, plus the concrete HKDF-SHA256 fetch (3.5+; it presets its digest — derives with no "digest" param, byte-equal to bare HKDF + the matching digest param, CLI-verified OKM/PRK KATs; per-name SKIP on 3.0.13) |
| `test_kdf_hkdf_sha384` | 33 | 49 | HKDF (RFC 5869) for SHA-384: TC1 OKM/PRK KATs (the concrete HKDF-SHA384 row's verified values, which lived in `test_kdf_hkdf_sha256` while SHA-384 had no per-digest binary — RFC 5869 publishes no SHA-384 vectors), EXPAND_ONLY, multi-chunk info against a reference derive, error paths, plus the concrete HKDF-SHA384 fetch (3.5+; per-name SKIP on 3.0.13) |
| `test_kdf_hkdf_sha512` | 33 | 49 | HKDF (RFC 5869) for SHA-512: TC1 KATs with the monolith's verified TC1 SHA-512 PRK (the concrete HKDF-SHA512 EXTRACT_ONLY value), EXPAND_ONLY, multi-chunk info against a reference derive, error paths, plus the concrete HKDF-SHA512 fetch (3.5+; per-name SKIP on 3.0.13) |

**`test_kdf_kbkdf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_kbkdf_sha1` | 32 | 40 | SP 800-108 KBKDF with the HMAC-SHA1 PRF: counter and feedback modes (switch behavior checked by difference — no verified SHA-1 switch KATs), the CAVS HMAC-SHA1 r=8 counter vector and the r=12 rejection (the 3.2-era `r` param; note on 3.0.13), feedback IV rules (first block = the counter-mode output, a different IV changes the output, a short IV rejected), error paths |
| `test_kdf_kbkdf_sha256` | 45 | 63 | SP 800-108 KBKDF, the family's representative digest: use-l/use-separator KATs for the HMAC-SHA256 PRF, CAVS r=8 rows (CMAC-AES128 and HMAC-SHA256), the digest-independent CMAC-AES128 PRF in counter and feedback modes, the KMAC128 PRF (3.1+), feedback IV rules, error paths; on 3.0.13 the `r`-param and KMAC-PRF sections are skipped with notes |
| `test_kdf_kbkdf_sha512` | 33 | 41 | SP 800-108 KBKDF with the HMAC-SHA512 PRF: counter and feedback modes checked by difference, the monolith's three-chunk info concatenation vector (gated on the 3.2-era `r` param), feedback IV rules, error paths |

**`test_kdf_sskdf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_sskdf_sha1` | 23 | 25 | Single Step KDF (SP 800-56C) for SHA-1: the CAVS digest-mode KAT, the key/secret parameter alias, multi-chunk info checked relatively (five chunks against a single-info reference derive — 3.0.13 notes that it neither concatenates repeated infos nor caps their number; the CMAC mode has no settable "cipher" param in the provider), the missing-key negative |
| `test_kdf_sskdf_sha256` | 30 | 32 | Single Step KDF (SP 800-56C) for SHA-256: the digest-mode 32- and 64-byte outputs, the HMAC-mode KAT row, the key/secret parameter alias, the verified 48-byte-info OKM anchoring the five-chunk multi-info check (3.0.13 notes as above), the missing-key negative |

**`test_kdf_sshkdf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_sshkdf_sha1` | 71 | 71 | SSH RFC 4253 exchange-hash KDF for SHA-1: all six derivation types A..F from the CAVS set 1 table (each derived to 24 bytes, compared at its CAVS length), the digest-sized `xcghhash` rule, type distinctness, the wrong-length `xcghhash` acceptance pinned by a CLI-verified 8-byte output, error paths |
| `test_kdf_sshkdf_sha256` | 57 | 57 | SSH RFC 4253 exchange-hash KDF for SHA-256: the CAVS type A and C rows over the digest-sized SHA-256 `xcghhash` (both rows derive and compare the full 32 bytes), type distinctness, the wrong-length `xcghhash` acceptance checked relatively (two derives must agree), error paths |

**`test_kdf_pbkdf2_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_pbkdf2_sha1` | 40 | 40 | PBKDF2-HMAC-SHA1: the RFC 6070 rows (P="password"/S="salt" at c=1, 2 and 4096, and the 24-byte-password 36-byte-salt dkLen=25 vector), passwords crossing the HMAC block boundary, iter counts, SP 800-132 pkcs5 bounds acceptance-checked (no verified pkcs5=0 value for SHA-1; the pkcs5=1 c=1 derive is compared against this digest's own c=1 KAT prefix) |
| `test_kdf_pbkdf2_sha256` | 42 | 42 | PBKDF2-HMAC-SHA256: the RFC 6070-style rows (c=1, 2, 4096 and the 70-byte-password c=1000 vector that crosses the HMAC block boundary), the SP 800-132 pkcs5 bounds against CLI-verified pkcs5=0 and pkcs5=1 constants |

**`test_kdf_pkcs12kdf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_pkcs12kdf_sha1` | 33 | 33 | PKCS#12 KDF (RFC 7292 B) for SHA-1: the three material ids (key/IV/MAC secret) at iter 2048, id-distinctness, missing-pass/missing-salt negatives |
| `test_kdf_pkcs12kdf_sha256` | 38 | 38 | PKCS#12 KDF (RFC 7292 B) for SHA-256: the three material ids at iter 2048 plus the iteration-count-of-1 id-1 row, id-distinctness, missing-pass/missing-salt negatives |

**`test_kdf_tls1prf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_tls1prf_md5_sha1` | 26 | 26 | the TLS 1.0/1.1 PRF (RFC 2246/4346's MD5-SHA1 combo, which splits the secret into two halves sharing the middle byte): the NIST master-secret KAT with the seed delivered as three chunks (plus the same KAT truncated to its 16-byte prefix), the same KAT with a pre-concatenated seed, the 47-byte odd secret, missing-secret/missing-seed negatives |
| `test_kdf_tls1prf_sha256` | 26 | 26 | the TLS 1.2 PRF (RFC 5246 section 5, P_SHA-256 with the single configured digest): the NIST master-secret KAT with the seed delivered as three chunks (plus the same KAT truncated to its 16-byte prefix), the same KAT with a pre-concatenated seed, the explicitly empty secret, missing-secret/missing-seed negatives |

**`test_kdf_krb5kdf_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_krb5kdf_aes_128_cbc` | 37 | 38 | Kerberos 5 RFC 3961 "DK" KDF for AES-128-CBC (the aes128-cts-hmac-sha1-96 enctype): the three key-usage-constant KATs, the verified full-block-constant KAT (a block-size constant takes the n-fold identity path), the output-length/constant-ceiling/mandatory-input negatives, the explicitly-empty-constant negative (adaptive: 3.0.13 accepts the empty constant and notes it, 3.1+ rejects it) |
| `test_kdf_krb5kdf_aes_256_cbc` | 28 | 28 | Kerberos 5 RFC 3961 "DK" KDF for AES-256-CBC (the aes256-cts-hmac-sha1-96 enctype): the key-usage-constant KAT, the block-size constant verified relatively against the in-test zero-IV encryption-chain reference (no verified vector for this cipher), the output-length/constant-ceiling/mandatory-input negatives |

#### Argon2

**`test_kdf_argon2_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_kdf_argon2d` | skip | 12 | Argon2d (RFC 9106) through the EVP_KDF public API: the section 5 test vector (3.5+; whole-program SKIP on 3.0.13) |
| `test_kdf_argon2i` | skip | 12 | Argon2i (RFC 9106) through the EVP_KDF public API: the section 5 test vector (3.5+; whole-program SKIP on 3.0.13) |
| `test_kdf_argon2id` | skip | 18 | Argon2id (RFC 9106) through the EVP_KDF public API: the section 5 test vector plus this variant's extra draft-vector and version 1.0 KATs (3.5+; whole-program SKIP on 3.0.13) |

### Asymmetric

**No family module** — these binaries are self-contained: shared boilerplate (`test_common.o`, `poisoning.o`) only.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_dsa` | 40 | 40 | DSA sign/verify with SHA-1/SHA-256 over generated 2048/256 parameters, plus a fixed-key KAT |
| `test_ecdh` | 46 | 46 | ECDH over the RFC 5903 P-256/P-384 groups with the fixed RFC 5114 A.6/A.7 key pairs and shared secrets |
| `test_ecdsa` | 88 | 91 | ECDSA over P-256/P-384/P-521 and secp256k1: keygen, one-shot and streaming DigestSign, tamper and wrong-digest rejection, DER bounds, RFC 6979 A.2.5 deterministic KAT |
| `test_rsa_oaep` | 89 | 89 | RSA-2048-OAEP: KATs (SHA-1/SHA-256, MGF1), labels, message-length boundaries, parameter rules |
| `test_rsa_pkcs1` | 134 | 134 | RSA-2048 PKCS#1 v1.5: type 1 signatures and type 2 encryption KATs with a fixed key, message-length boundaries, implicit rejection on tamper |
| `test_rsa_pss` | 121 | 121 | RSA-2048-PSS sign/verify: KATs, salt lengths (incl. digest length and maximized), restrictions |
| `test_rsa_raw` | 27 | 35 | RSA_NO_PADDING ("textbook RSA") full-modulus roundtrips, plus RSASVE (3.2+; SKIP notice on older builds) |
| `test_sm2` | 106 | 106 | SM2 keygen on the SM2 curve, SM2 signatures (SM3, Z-value from the distinguishing ID) and SM2 public-key encryption |

#### X25519 / X448

**`test_x25519_x448_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_x25519` | 10033 | 10038 | X25519 (RFC 7748): the section 6.1 KATs in both derive directions, iterated vectors, low-order-point rejection, X25519-as-KEM (3.5+; SKIP notice on older builds) |
| `test_x448` | 10029 | 10029 | X448 (RFC 7748): the section 6.2 KATs in both derive directions, iterated vectors, low-order-point rejection |

#### Ed25519 / Ed448

**`test_ed25519_ed448_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_ed25519` | 59 | 78 | Ed25519 (PureEdDSA): RFC 8032 section 7 KATs, the Ed25519ctx/ph instances (3.5+; SKIP notice on older builds) |
| `test_ed448` | 23 | 31 | Ed448 (PureEdDSA): the RFC 8032 section 7.4 KAT, the 3.2+ "context-string" parameter |

#### Finite-field DH

**`test_dh_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_dh` | 27 | 27 | finite-field DH over ffdhe2048 (RFC 7919) fetched as "DH", fixed private keys via `EVP_PKEY_fromdata`, shared-secret KAT |
| `test_dhx` | 27 | 27 | the same over the "DHX" fetch name (the flavor that carries the subgroup order q) |

### Post-quantum and random

#### ML-KEM

**`test_ml_kem_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_ml_kem_1024` | skip | 75 | ML-KEM-1024 (FIPS 203): deterministic seed keygen, encapsulation/decapsulation KATs, implicit rejection of tampered ciphertexts, raw key import/export (3.5+; whole-program SKIP on 3.0.13) |
| `test_ml_kem_512` | skip | 75 | ML-KEM-512 (FIPS 203): deterministic seed keygen, encapsulation/decapsulation KATs, implicit rejection of tampered ciphertexts, raw key import/export (3.5+; whole-program SKIP on 3.0.13) |
| `test_ml_kem_768` | skip | 75 | ML-KEM-768 (FIPS 203): deterministic seed keygen, encapsulation/decapsulation KATs, implicit rejection of tampered ciphertexts, raw key import/export (3.5+; whole-program SKIP on 3.0.13) |

#### ML-DSA

**`test_ml_dsa_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_ml_dsa_44` | skip | 114 | ML-DSA-44 (FIPS 204): deterministic seed keygen, one-shot signature KATs, streaming (chunked) signing via the 3.6-era message API, tamper detection, context binding (3.5+; whole-program SKIP on 3.0.13) |
| `test_ml_dsa_65` | skip | 114 | ML-DSA-65 (FIPS 204): deterministic seed keygen, one-shot signature KATs, streaming (chunked) signing via the 3.6-era message API, tamper detection, context binding (3.5+; whole-program SKIP on 3.0.13) |
| `test_ml_dsa_87` | skip | 114 | ML-DSA-87 (FIPS 204): deterministic seed keygen, one-shot signature KATs, streaming (chunked) signing via the 3.6-era message API, tamper detection, context binding (3.5+; whole-program SKIP on 3.0.13) |

#### SLH-DSA

**`test_slh_dsa_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_slh_dsa_sha2_128f` | skip | 103 | SLH-DSA-SHA2-128f (FIPS 205, a "fast" parameter set): deterministic seed keygen, public-key KAT, one-shot signing of small and 256 KiB messages, signature size, context binding (3.5+; whole-program SKIP on 3.0.13) |
| `test_slh_dsa_sha2_192f` | skip | 103 | SLH-DSA-SHA2-192f (FIPS 205, a "fast" parameter set): deterministic seed keygen, public-key KAT, one-shot signing of small and 256 KiB messages, signature size, context binding (3.5+; whole-program SKIP on 3.0.13) |
| `test_slh_dsa_shake_128f` | skip | 103 | SLH-DSA-SHAKE-128f (FIPS 205, a "fast" parameter set): deterministic seed keygen, public-key KAT, one-shot signing of small and 256 KiB messages, signature size, context binding (3.5+; whole-program SKIP on 3.0.13) |

#### EVP_RAND

**`test_rand_drbg_common.{c,h}`** — the family's shared machinery; each binary below carries only its own KAT constants and `main()`.

| Program | 3.0.13 | 3.6.5 | Coverage |
|---|---|---|---|
| `test_rand_ctr_drbg` | 28 | 28 | EVP_RAND CTR-DRBG (AES-256-CTR): instantiation params, state reporting, generate sizes and chunking, reseed knobs, prediction resistance |
| `test_rand_hash_drbg` | 17 | 17 | EVP_RAND HASH-DRBG (SHA2-256): instantiation params, state reporting, generate sizes and chunking, reseed knobs, prediction resistance |
| `test_rand_hmac_drbg` | 12 | 12 | EVP_RAND HMAC-DRBG: "mac" and "digest" in one `EVP_RAND_CTX_set_params` call, instantiation params, reseed knobs, prediction resistance |
| `test_rand_test_rand` | 14 | 24 | TEST-RAND determinism in entropy mode, plus the 3.6+ "generate" mode pinned by an embedded KAT |

## Behavioral notes discovered

Verified while writing the tests (each is asserted by the binary or family
module named; a family module's routines run inside every binary of that
family):

* XTS ciphertext stealing works in **both** the encrypt and the decrypt
  direction, contrary to some doc impressions (`test_aes_xts_common`).
* OCB accepts an AAD update **after** the first data update
  (`test_aes_ocb_common`).
* CCM's `ivlen`/`taglen` must be set **before** the init call that passes
  the key and nonce (`ccm_init` rejects a nonce whose length does not match
  the configured `ivlen`); defaults are `ivlen = 7` and `taglen = 12`
  (`test_aes_ccm_common`).
* A CCM nonce longer than 13 bytes caps the payload at 65535 bytes
  (`test_aes_ccm_common`).
* SIV: on 3.0.x an empty payload leaves the tag unset and Final fails; 3.6.x
  computes the synthetic IV and succeeds (`test_aes_siv_common`).
* RSA PKCS#1 v1.5 type 2 tamper detection defaults to implicit rejection:
  decrypt "succeeds" but returns a deterministic pseudorandom plaintext
  instead of erroring (`test_rsa_pkcs1`).
* The digest-route RSA signature path (`EVP_DigestSign` with an RSA key) has
  no message-size cap — only the raw no-digest route enforces
  `modlen - 11` (`test_rsa_pkcs1`, `test_rsa_raw`).
* The SM2 default distinguishing ID is the **empty string**, not the
  GM/T 0003 `1234567812345678` (the CLI substitutes the latter explicitly)
  (`test_sm2`).
* SM2 encryption of a 0-byte plaintext fails; SM2 ciphertexts are C1C3C2
  ordered only (`test_sm2`).
* ECDSA deterministic mode is requested with the integer `nonce-type` = 1
  param (RFC 6979) and reproduces the RFC A.2.5 vector; 3.0/3.1 silently
  ignore the parameter, which the test detects by read-back
  (`test_ecdsa`).
* EdDSA forbids explicit digests — the SHA-512 prehash for Ed25519ph
  happens inside the provider (`test_ed25519`, `test_ed448`).
* The ETM AES-CBC-HMAC-SHA* variants are ARM64-only in 3.6.5
  (`test_aes_128_cbc_hmac_sha256_etm` and its 192/256 siblings).
* SLH-DSA is one-shot only — the provider exports no streaming
  sign/verify, so 256 KiB messages are signed in a single call
  (`test_slh_dsa_common`).
* ML-DSA streaming uses the 3.6-era `EVP_PKEY_{sign,verify}_message_*` API
  (`test_ml_dsa_common`).
* The Poly1305 MAC key is 32 bytes, the `r||s` pair (`test_mac_poly1305`).
* KMAC's `custom` must be an OCTET_STRING of exactly `strlen(custom)` bytes
  (no trailing NUL); a UTF8_STRING passes on 3.0.x but is rejected by the
  3.6.x type check, and a NUL inside the string silently changes the tag
  (`test_mac_kmac_common`).
* HMAC accepts a zero-length key, but the key pointer must be non-NULL
  (`test_mac_hmac_common`).
* OSSL_PARAM integers are **native-endian** — big-endian key constants must
  be byte-swapped before `EVP_PKEY_fromdata`
  (`test_rsa_oaep`, `test_rsa_pkcs1`, `test_rsa_pss`, `test_rsa_raw`,
  `test_dh`, `test_dhx`, `test_dsa`, `test_ecdh`, `test_ecdsa`).
* Loading the legacy provider explicitly turns off default-provider
  auto-load, so a program that needs both must load both
  (`test_digest_md5_common`, `test_digest_mdc2_whirlpool_common`).
* CBC-CTS has existed since 3.0 (it is not 3.2-gated) and behaves
  identically on 3.0.13 and 3.6.5 (`test_cbc_cts_common`).
* KBKDF's parameter names are shifted: `"salt"` is the SP 800-108 *Label*,
  `"info"` the *Context*, and `"seed"` the feedback-mode IV
  (`test_kdf_kbkdf_common`).
* HMAC-DRBG requires `"mac"` and `"digest"` in a single
  `EVP_RAND_CTX_set_params` call — splitting them across two calls fails
  (`test_rand_hmac_drbg`).
* The raw private-key import of FFC keys (DH/DSA) does not derive the
  public key, so both must be supplied (`test_dh_common`, `test_dsa`).
* DES3-WRAP differs from AES-WRAP: the wrap direction requires only
  `inl % 8 == 0` (no `>= 16` minimum — an 8-byte input wraps fine) and
  the output is `inl + 16` (a random 8-byte IV plus an 8-byte SHA-1 ICV,
  both inside the CBC-encrypted, reversed blob); the unwrap direction
  requires `>= 24` bytes and verifies the ICV (`test_des3_wrap`,
  `cipher_tdes_wrap.c`).
* DES-CFB1 has **no `use_bits` branch** in its provider `hw`:
  `EVP_CIPH_FLAG_LENGTH_BITS` is accepted but ignored, so a bit-count
  update would overread — Fil-C trapped the exact overread during
  development.  `test_des_cfb1` pins byte mode only (`cipher_des_hw.c`).
* ChaCha20-Poly1305: an AAD update **after** the first data update is
  rejected on 3.6.5 but accepted on 3.0.13
  (`cipher_chacha20_poly1305_hw.c:303-309`), and a decrypt without
  `SET_TAG` verifies nothing — the tag length defaults to 0
  (`test_chacha20_poly1305_small`).
* RC2-64-CBC's key is 8 bytes (= 64 bits, not 6); RC2-40-CBC's is 5
  (`EVP_CIPHER_get_key_length`, `test_rc2_64_cbc`, `test_rc2_40_cbc`).
* The Camellia RFC 3713 KAT ciphertext is
  `67673138549669730857065648eabe43` — the commonly recirculated
  `6767313314...` value is wrong (`test_camellia_common`).
* SM4-XTS/GCM/CCM are absent from OpenSSL 3.0.13 and present in 3.6.5;
  `test_sm4_gcm`, `test_sm4_ccm` and `test_sm4_xts` each take their own
  version-gated whole-program SKIP line there.

## Intentionally not covered

* **RC5, MD2, LMS** — compiled out of the pizfix OpenSSL build
  (`OPENSSL_NO_RC5`, `OPENSSL_NO_MD2`, `OPENSSL_NO_LMS` in
  `pizfix/include/openssl/configuration.h`), so they cannot be exercised
  against the authoritative build.
* **PBKDF1, PVKKDF, HMAC-DRBG-KDF** — niche legacy KDFs with no modern
  users; skipped on purpose.
* **SM2 C1C2C3 ciphertext ordering** — OpenSSL 3 provides C1C3C2 only.
* **STREEBOG / GOST** — requires the third-party GOST provider, which is
  not part of any build here.
* **The ETM CBC-HMAC variants on x86** — the encrypt-then-MAC
  AES-CBC-HMAC-SHA* implementations are ARM64-only in 3.6.5 (and postdate
  3.0), so they cannot be exercised on this machine; the ETM binaries detect
  that and print a whole-program SKIP line (`test_aes_128_cbc_hmac_sha256_etm`
  and its 192/256 siblings).

## Adding a test

One binary per cipher; the file is named `test_<cipher>.c` (e.g.
`test_aes_128_cbc.c`) and is picked up by the Makefile automatically — the
`PROGRAMS` wildcard skips `*_common.c`, which are the link-only family
modules.  House rules:

1. `#include "test_common.h"` first (it includes `poisoning.h`), then the
   OpenSSL headers and, for a family member, the family's
   `test_<family>_common.h`.
2. Allocate every buffer handed to OpenSSL through the labeled
   `ALLOCATE_*` macros from `test_common.h` (`ALLOCATE_KEY`,
   `ALLOCATE_INPUT`, `ALLOCATE_OUTPUT`, `ALLOCATE_IV`, `ALLOCATE_AAD`,
   `ALLOCATE_TAG`, `ALLOCATE_SALT`, `ALLOCATE_OTHER` — one-line macros over
   `poison_alloc()`).  Known-answer constants may be `static const` strings
   (hex is convenient), or `extern const char name[]` arrays imported from
   the family module, but must be copied into `ALLOCATE_*` buffers (via
   `from_hex()`) before use.
3. `main()` starts with `test_parse_args(argc, argv);` (which runs
   `poison_parse_args` and records the program name used in every
   PASS/FAIL/SKIP line).
4. Bracket every `memcpy`/`memcmp` comparison block with
   `unpoison_all()` before it and `repoison_all()` after it (see
   [Comparisons and freeing](#comparisons-and-freeing)) — `memcmp` branches
   on the compared bytes and must run on unpoisoned data.  (The
   `expect_equal()` helper in `test_common.c` applies this discipline for
   length-and-content comparisons.)  Call
   `free_poison_allocations()` at the end of every test function (or, for
   per-iteration allocation loops, at the bottom of the loop body) that
   allocated poison allocations, and before any early `return`/`exit`
   path that would otherwise leave allocations tracked — it is safe to
   call any number of times.  Buffers allocated directly in `main()` and
   shared across test functions are freed once, at the end of `main()`,
   just before the success `printf`.
5. Shared code has exactly three homes.  (a) `test_common.{c,h}` — the
   global boilerplate (`g_checks`, `CHECK`/`test_failed`, `hexval`,
   `from_hex`, `expect_equal`, the `ALLOCATE_*` macros, `test_parse_args`,
   `test_prog_name`, `test_skip`, `test_load_provider`,
   `test_report_pass`); every binary links it, so never redefine any of it
   in a test.  (b) `test_<family>_common.{c,h}` — the machinery shared by a
   family (helpers plus parameterized test routines; e.g.
   `test_aes_cbc_common.{c,h}` backs the three AES-CBC binaries); a family
   module carries no `main()`, is linked only into its family's binaries
   through a prerequisite-only family line in the Makefile, and exports
   shared KAT material as `extern const char name[]` arrays — address
   constants, so they compose into the static initializers of the
   per-cipher binaries (never as `const char *`).  (c) the per-cipher
   binary itself — its own KAT constants and `main()`.  Keep the
   "one binary = one cipher" rule: no binary tests two ciphers, and a
   small/large regime split stays one binary per cipher per regime.
   Adding a cipher to an existing family means a new binary plus (only if
   it needs new vectors or a new routine) an extension of the family
   module; a new family means the `test_<family>_common.{c,h}` pair plus
   one family line in the Makefile.  Everything else stays local to the
   binary: no file I/O, no VLAs, no `alloca`, no threads/signals.  C99.
   Public OpenSSL 3 API only (`EVP_CIPHER_fetch`, `EVP_MD_fetch`,
   `EVP_MAC_fetch`, `EVP_KDF_fetch`, `EVP_PKEY_*`, `OSSL_PARAM`,
   `OSSL_PROVIDER_load`, ...); no deprecated low-level APIs.
   `EVP_get_cipherbyname`/`EVP_get_digestbyname` are fine.
6. Failure handling comes from `test_common.h`: `CHECK(cond, msg)` counts
   passing checks in `g_checks` and calls `test_failed(msg)` on failure
   (prints `FAIL <program>: <msg>` to stderr and exit(1));
   `test_report_pass()` prints `<program>: all N checks passed` and
   `test_skip(reason)` prints `SKIP <program>: <reason>`, both exit(0).
   Call `test_skip()` if a genuinely version-gated algorithm is not
   provided by this OpenSSL (its `EVP_*_fetch` fails) — never skip core
   things present in all of OpenSSL 3.x.  A version-dependent *subtest*
   prints a `SKIP` line and keeps going, and a version-dependent behavior
   difference between two builds is asserted as "3.0.x does X, 3.6.x does
   Y" in a comment.
7. Size output buffers per the EVP contract: each Encrypt/Decrypt update can
   emit up to `inl + block_size` bytes and Final up to `block_size` bytes,
   so allocate `ALLOCATE_OUTPUT` accordingly and CHECK the contract.
8. For AEAD ciphers, set `ivlen`/`taglen` **before** the init call that
   passes the key and IV (CCM outright rejects a nonce whose length does
   not match the configured `ivlen`), and remember SIV/CCM-style rules:
   one payload per init (SIV), the length announcement update (CCM).
9. OSSL_PARAM integer parameters are native-endian: byte-swap big-endian
   constants (e.g. fixed RSA/DH keys) before `EVP_PKEY_fromdata`.
10. Known-answer vectors must be verified against the authoritative build
   before being embedded: use `pizfix/bin/openssl` (OpenSSL 4.0.3) in /tmp
   scratch, e.g. `printf 'abc' | pizfix/bin/openssl dgst -sha256` or
   `printf '<hex>' | xxd -r -p | pizfix/bin/openssl enc -aes-128-cbc -nopad
   -K <keyhex> -iv <ivhex>`.  Note that `openssl enc` cannot do AEAD or XTS
   ciphers; for those, verify with a tiny scratch program.  Behavior
   questions (e.g. which operation orderings are legal) are settled by small
   scratch experiments — but compile them with `filcc` (`build/bin/filcc
   probe.c -g -O2 -lcrypto`), **not** with `gcc -I pizfix/include -L
   pizfix/lib`, because that `-L` makes plain `-lc` resolve to pizfix's
   pizlonated `libc.so`, which cannot link into a glibc binary (an
   unpizlonated build cannot link the pizlonated libcrypto at all — that is
   the ABI slice problem that Yolo-C works around above).

Before committing a new program, make sure it builds and passes under
`./build_yolo.sh` and `./build_filc.sh`, and once with `--poison-all`; then
add its row to the test list above (and, for a new family, the Makefile's
family line).
