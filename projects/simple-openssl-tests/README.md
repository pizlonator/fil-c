# simple-openssl-tests

A suite of small, self-contained C programs — one per cryptosystem — that
exercise OpenSSL 3's public API with known-answer and behavior tests.  Every
program is a single `.c` file that builds and runs identically under Fil-C,
under Yolo-C, and under a normal (unpizlonated) compiler, so each
cryptosystem can be watched through all of the repo's C stacks.  All
working buffers (keys, inputs, outputs, IVs, AAD, tags, salts, ...) are
malloc'd through small
labeled `ALLOCATE_*` functions with optional poisoning hooks, so that a real
poisoning implementation can be attached later without touching a single
test program.

Design points:

* **One program per cryptosystem** (AES-CBC, AES-GCM, HKDF, ECDSA, ML-KEM,
  ...).  A program that covers "small" and "large" inputs for the same
  primitive is split into two programs when the implementation has
  size-dependent code paths — see
  [below](#why-some-cryptosystems-have-small-and-large-test-programs).
* **Public OpenSSL 3 API only** (`EVP_CIPHER_fetch`, `EVP_MD_fetch`,
  `EVP_MAC_fetch`, `EVP_KDF_fetch`, `EVP_PKEY_*`, `OSSL_PARAM`,
  `OSSL_PROVIDER_load`, ...).  No deprecated low-level APIs, no headers
  beyond the public ones.
* **Self-contained**: no input files, no network, no threads/signals, no
  VLAs, no `alloca`.  The only shared file is `poisoning.h`.  Every
  expected value is a deterministic known-answer test (KAT) or a
  self-consistency check embedded in the source.
* **Deterministic where the algorithm allows it**: randomized algorithms
  (RSA keygen, DSA params, SM2, ...) either embed fixed keys generated once
  with the authoritative build, or use the public determinism hooks
  (`nonce-type`, `deterministic`, `seed`, fixed private keys via
  `EVP_PKEY_fromdata`).
* **Every malloc'd buffer that is handed to OpenSSL flows through a labeled
  `ALLOCATE_*` wrapper** at the top of the program, which funnels into
  `poison_alloc()` in `poisoning.h` (see [Poison options](#poison-options)).

Program contract, identical for all 80 programs:

* `#include "poisoning.h"` comes first; the labeled `ALLOCATE_*` functions
  come right after the includes.
* `main()` starts with `poison_parse_args(argc, argv)`.
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
make CC=<repo>/build/bin/filcc                 # Fil-C + pizfix OpenSSL 3
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
(`80 passed, 0 failed`) and exits nonzero if anything failed; on failure the
failing program's output is replayed and the full output of the last run is
in `/tmp/simple-openssl-tests.log`.  To run one program directly, build and
execute it: `make test_aes_cbc && ./test_aes_cbc --poison-key`.

Exit codes of the test programs themselves: 0 = pass or (version-gated)
skip, 1 = `FAIL`, 2 = bad usage (unknown argument, unknown poison kind) —
e.g. `./test_aes_cbc --bogus` exits 2 while `./test_aes_cbc --poison-help`
prints the poison option list and exits 0.

## Compiler support matrix (verified)

All four configurations below were verified on this machine (Ubuntu 24.04,
x86_64) with `make clean`, a full build, and then `./run_all.sh`,
`./run_all.sh --poison-all`, and
`./run_all.sh --poison=key,input,output,tag,iv,aad,salt` — **80 passed,
0 failed in every one of the 12 runs**:

| Configuration | Command | Compiler | OpenSSL linked | Result |
|---|---|---|---|---|
| yolo (normal compiler) | `./build_yolo.sh` (= `make CC=gcc`) | gcc 12.4.0 | system 3.0.13 | 80 passed, 0 failed (~1.5 s suite) |
| Fil-C (primary) | `./build_filc.sh` | `build/bin/filcc` (Fil-C 0.686, clang 20.1.8) | pizfix 3.6.5 | 80 passed, 0 failed (~4 s suite) |
| Fil-C (secondary) | `FILCC=/opt/fil/bin/filcc ./build_filc.sh` | `/opt/fil/bin/filcc` | repo pizfix 3.6.5 (the `/opt/fil` install ships no sysroot OpenSSL) | 80 passed, 0 failed |
| Yolo-C (yolo glibc) | `make CC=/tmp/yolocc OPENSSL_HOME=/tmp/yolo-openssl-install` | host gcc 12.4.0 via the `/tmp/yolocc` wrapper | yolo-built 3.6.5 at `/tmp/yolo-openssl-install` | 80 passed, 0 failed |

`make CC=clang` was also smoke-verified (clang 18.1.3 + system OpenSSL
3.0.13, same code path as gcc).  Fil-C programs are built with `-g -O2` per
repo convention (the Makefile default).

SKIP inventory (all exit-0; the rest of each program still runs):

* **On the system OpenSSL 3.0.13** nine programs take the full-program SKIP
  path because the whole algorithm postdates 3.0 or is missing from that
  build: `test_aes_gcm_siv` (3.5+), `test_digest_sha2` (SHA2-256/192,
  3.5+), `test_digest_sha3` (KECCAK-* names, 3.5+), `test_digest_shake`
  (`EVP_DigestSqueeze`, 3.5+), `test_digest_mdc2_whirlpool` (MDC2 is
  compiled out of Ubuntu's 3.0.13), `test_kdf_argon2` (3.5+),
  `test_ml_kem`/`test_ml_dsa`/`test_slh_dsa` (3.5+).  Seven more print a
  SKIP notice for a gated subtest and pass the rest: `test_ecdsa`
  (`nonce-type` is silently ignored before 3.2), `test_ed25519_ed448`
  (Ed25519ctx/ph, 3.5+), `test_rsa_raw` (RSASVE, 3.2+), `test_x25519_x448`
  (X25519-as-KEM, 3.5+), `test_aes_cbc_hmac_sha256` (the ETM variants are
  ARM64-only in 3.5+), `test_sm4` (SM4-XTS/GCM/CCM postdate 3.0) and
  `test_legacy_block_ciphers` (IDEA is not offered by Ubuntu's 3.0.13 at
  all; the IDEA rows SKIP there and the other rows still run).  Five print
  informational `note` lines about pre-3.1/3.2 behavior (`test_kdf_hkdf`
  >5 info params, `test_kdf_kbkdf` no `"r"` param, `test_kdf_krb5kdf`
  explicitly empty constant, `test_kdf_sskdf` repeated-info
  concatenation, `test_chacha20_poly1305_small` an AAD update after data
  is accepted).
* **On both 3.6.5-based configurations** (pizfix via filcc, and Yolo-C) the
  only notice is the same `test_aes_cbc_hmac_sha256` ETM line — the ETM
  AES-CBC-HMAC-SHA* ciphers are only registered on ARM64 builds.  The five
  `note` programs are silent because 3.6.5 has the newer behaviors.

Additional verified invariants:

* `make clean` removes exactly the 80 built binaries and leaves only
  sources/scripts/Makefile/README.
* `make test` (build + `./run_all.sh`) works in every configuration.
* Yolo-C binaries were checked with `ldd`/`readelf`: the interpreter is
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
or the extracted tree `projects/openssl`.  **Do not** Configure out-of-tree
from `projects/openssl-build/extracted-source`: that tree carries ~6000
stale in-tree build artifacts (`Makefile`, `configdata.pm`, `.o`/`.d`
files) from the pizfix build, which break a fresh out-of-tree Configure.
(The Fil-C port's differences vs pristine 3.6.5 — 48 files, all
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
Both the pizfix build and the yolo build report `OpenSSL 3.6.5 29 Sep 2026`.


## Poison options

Every working buffer is allocated by a labeled `ALLOCATE_*` function at the
top of each program (e.g. `ALLOCATE_KEY`, `ALLOCATE_INPUT`, `ALLOCATE_OUTPUT`,
`ALLOCATE_IV`, `ALLOCATE_AAD`, `ALLOCATE_TAG`, `ALLOCATE_SALT`,
`ALLOCATE_OTHER`), and each wrapper funnels into `poison_alloc()` in
`poisoning.h`.  When the user requests a poison kind on the command line, the
`ALLOCATE_*` function calls `poison_range()` on what `malloc()` returned;
otherwise it does not.  `poison_range()` is currently a **no-op**: it is the
single hook where a real poisoning implementation (e.g. a Fil-C/GIMSO
poisoning API) can be installed without touching any test program.  Passing
a poison option must never change what a program computes — it only changes
which buffers the hook is called on.

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
| `--poison-help`    | print the option list and exit 0                |

Options may be repeated and combine as a bitmask (`--poison-key --poison-iv`
equals `--poison=key,iv`; `--poison-none` resets).  `poison_parse_args()`
runs first in every `main()`, consumes `-h`/`--help` and only these
`--poison-*` options, and rejects anything else with exit code 2 — the
programs take no other input.

## Why some cryptosystems have small and large test programs

Several OpenSSL implementations pick different code paths depending on the
payload size, buffer alignment, and update pattern, so a 16-byte KAT and a
2 MiB streaming test do not exercise the same code.  Where that is the
case, the suite has an explicit small and an explicit large program
(`test_aes_gcm_{small,large}`, `test_aes_ccm_{small,large}`,
`test_aes_ocb_{small,large}`, `test_aes_xts_{small,large}`,
`test_chacha20_{small,large}`, `test_chacha20_poly1305_{small,large}`,
`test_rc4_{small,large}`); other programs
combine the regimes in one file when they fit.  The regimes below were
read out of the OpenSSL 3.6.5 sources in
`projects/openssl-build/extracted-source` and each is exercised by the
corresponding test:

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

All 80 programs, in family order.  "Checks" is the count printed by the
3.6.5 (pizfix) build; on 3.0.13 the count can be lower where a gated
subtest is skipped (and higher where an older behavior adds a check,
e.g. ChaCha20-Poly1305's AAD ordering), and full-SKIP programs print no
count.

### Exemplars

| Program | Checks | Coverage |
|---|---|---|
| `test_aes_cbc` | 127 | AES-128/192/256-CBC: NIST SP 800-38A F.2 KATs, fetched/static/`EVP_get_cipherbyname` routes, streaming updates, padding and padding-disabled failures, output-buffer contract |
| `test_aes_gcm_small` | 380 | AES-128/192/256-GCM with payloads <= 64 bytes: McGrew/Viega KATs with and without AAD (the AES-192 KATs are scratch-derived; published vectors exist only for 128/256), tamper detection, split updates, AAD ordering, IV lengths, short tags, zero-length payloads |
| `test_digest_sha2` | 339 | SHA2-224/256/384/512 and the SHA2-512/224 and SHA2-512/256 truncations: FIPS KATs, chunked-vs-one-shot equality around block boundaries, plus the SHA2-256/192 (3.5+) gate/SKIP |

### AES classical modes

| Program | Checks | Coverage |
|---|---|---|
| `test_aes_ecb` | 157 | AES-128/192/256-ECB: SP 800-38A F.1 KATs, in-place operation, partial/streaming updates |
| `test_aes_ctr` | 189 | AES-128/192/256-CTR: SP 800-38A F.5 KATs, counter carrying across updates, partial final blocks |
| `test_aes_ofb` | 133 | AES-128/192/256-OFB(128): SP 800-38A F.4 KATs, byte-offset streaming across updates |
| `test_aes_cfb128` | 120 | AES-128/192/256-CFB(128): SP 800-38A F.3 KATs, streaming and in-place |
| `test_aes_cfb8` | 744 | AES-128/192/256-CFB8: the F.3 shared plaintext one byte at a time, streaming |
| `test_aes_cfb1` | 234 | AES-128/192/256-CFB1 in byte mode and bit-length mode (`EVP_CIPH_FLAG_LENGTH_BITS`) |
| `test_aes_xts_small` | 214 | AES-128/256-XTS small data units: IEEE 1619-2007 KATs, data-unit number as IV, key = two concatenated AES keys, small-length and stealing behavior |
| `test_aes_xts_large` | 86 | AES-256-XTS at the 2^20-block data-unit boundary: 16 MiB accepted, 16 MiB + 1 rejected |

### AES AEAD / wrap

| Program | Checks | Coverage |
|---|---|---|
| `test_aes_gcm_large` | 3161 | AES-128/192/256-GCM with 2 MiB payloads fed through updates of 16, 17, 64, 100, 128, 129, 3071, 3072, 3073 and 65536 bytes (all four size regimes), self-consistency oracle (the AES-192 KAT is scratch-derived; published vectors exist only for 128/256) |
| `test_aes_ccm_small` | 10543 | AES-128/192/256-CCM small payloads: KATs (the AES-192 KATs are scratch-derived; published vectors exist only for 128/256), tag lengths 4..16 even, nonce lengths 7..13, AAD ordering rules, the NULL-update length announcement, payload cap 65535 for 13-byte nonces |
| `test_aes_ccm_large` | 224 | AES-128/192-CCM with a 1 MiB single-update payload (the length is fixed by the first update; the AES-192 vectors are scratch-derived — RFC 3610 publishes 128-bit-key vectors only) |
| `test_aes_ocb_small` | 1041 | AES-128/192/256-OCB: RFC 7253 KATs (the AES-192 tuples are scratch-derived; RFC 7253 publishes 128/256 only), nonce lengths 1..15, tag lengths, AAD after data |
| `test_aes_ocb_large` | 430 | AES-192/256-OCB with a 1 MiB payload through mixed 16384/16391-byte updates, self-consistency oracle (the AES-192 KAT is scratch-derived, see test_aes_ocb_small) |
| `test_aes_siv` | 342 | AES-128/192/256-SIV (RFC 5297): A.1/A.2-style KATs, S2V with multiple ADs, one crypto operation per init, tag-as-synthetic-IV, empty-payload version difference |
| `test_aes_gcm_siv` | 386 | RFC 8452 AES-128/192/256-GCM-SIV KATs — RFC 8452 defines 128/256, the 192 vectors are scratch-derived (OpenSSL 3.5+; SKIP on older) |
| `test_aes_wrap` | 213 | AES-128/192/256 key wrap and -WRAP-INV at all three key sizes (RFC 3394 / SP 800-38F): KATs, multiple-of-8 >= 16 length rules, unwrap error paths, -INV roundtrips |
| `test_aes_wrap_pad` | 520 | AES-128/192/256-WRAP-PAD and -WRAP-PAD-INV (RFC 5649): KATs, arbitrary lengths >= 1, A65959A6 header |
| `test_aes_cbc_cts` | 2037 | CBC ciphertext stealing CS1/CS2/CS3 via `cts_mode` for AES-128/192/256 and CAMELLIA-128/192/256: one-shot rule, minimum length, variant equality relations, roundtrips |
| `test_aes_cbc_hmac_sha1` | 478 | The stitched AES-128/256-CBC-HMAC-SHA1 TLS-record cipher: record-format composition, TLS AAD layout, variable-length records, roundtrips |
| `test_aes_cbc_hmac_sha256` | 478 | The stitched AES-128/256-CBC-HMAC-SHA256 TLS-record cipher, same record format; documents the ARM64-only ETM variants with a SKIP line |

### Non-AES symmetric ciphers

| Program | Checks | Coverage |
|---|---|---|
| `test_camellia` | 866 | CAMELLIA-128/192/256 in ECB/CBC/CTR/OFB/CFB/CFB1/CFB8 (all key sizes in all classical modes; GCM n/a — Camellia has no GCM): the RFC 3713 KAT, CLI-verified mode/key-size constants, CFB1 in byte and bit-length mode, streaming and padding rules |
| `test_aria` | 972 | ARIA-128/192/256 in ECB/CBC/CTR/OFB/CFB/CFB1/CFB8 + GCM + CCM (all key sizes in all modes): KATs and the AEAD ordering rules (IV length before init, AAD first, CCM length announcement) |
| `test_sm4` | 296 | SM4-ECB/CBC/CTR/OFB/CFB (the GB/T 32907-2016 standard vector) plus SM4-XTS/GCM/CCM (absent from 3.0.13; per-algorithm SKIPs there) |
| `test_des` | 581 | The legacy provider's single-DES family: DES-ECB/CBC/OFB/CFB/CFB1/CFB8, DESX-CBC (24-byte key = key \|\| input whitening \|\| output whitening, rebuilt from per-block DES-ECB calls) and the default-provider two-key DES-EDE family (DES-EDE/CBC plus the stream modes DES-EDE-CFB/DES-EDE-OFB) |
| `test_des3` | 188 | Three-key DES-EDE3 in ECB/CBC/OFB/CFB/CFB8/CFB1: KATs, the fixed 24-byte key length (`set_key_length` must refuse), 8-byte block semantics |
| `test_des3_wrap` | 92 | DES3-WRAP (RFC 3217 CMS triple-DES key wrap): the modulo-8-only input rule (no `>= 16` minimum), the `in + 16` output (random IV \|\| payload \|\| SHA-1 ICV), unwrap `>= 24` and ICV rejection |
| `test_chacha20_small` | 215 | ChaCha20 small inputs: RFC 8439 section 2.4.2 keystream and ciphertext, the 16-byte IV = little-endian block counter \|\| nonce, partial-keystream bridging across uneven updates |
| `test_chacha20_large` | 17000 | ChaCha20 at block-boundary and counter-carry sizes: the 32-bit block counter wrapping into the first nonce word (IV counter 0xffffffff), one-shot vs chunk-grid equality |
| `test_chacha20_poly1305_small` | 168 | ChaCha20-Poly1305 small inputs: the RFC 8439 section 2.8.2 KAT, AAD-before-data ordering (version difference pinned), tag-length rules including the no-`SET_TAG` decrypt that verifies nothing |
| `test_chacha20_poly1305_large` | 200 | ChaCha20-Poly1305 at `>= 1 MiB`: the AEAD bulk path, arbitrary chunk grids against the one-shot oracle, AAD sizes {0, 1, 15, 16, 17, 32} covering the mod-16 padding rules |
| `test_rc4_small` | 1384 | RC4 and RC4-40 (legacy provider): the classic "Key"/"Wiki"/"Secret" vectors via `set_key_length`, stream state carried across updates, the 8-byte bulk-path boundaries |
| `test_rc4_large` | 661 | RC4 at `>= 64 KiB` through odd update grids: chunked output must equal the one-shot keystream exactly (self-consistency oracle) |
| `test_rc4_hmac_md5` | 265 | The stitched RC4-HMAC-MD5 TLS-record cipher: record layout (13-byte TLS AAD, MAC key via `SET_MAC_KEY`, `L + 16` body in one update, RC4 over payload \|\| MAC), KAT rebuilt from separate HMAC-MD5 + RC4 calls |
| `test_legacy_block_ciphers` | 864 | Data-driven sweep of the remaining legacy block ciphers — CAST5, Blowfish, IDEA, SEED and RC2 (incl. RC2-40/64-CBC) — one KAT, roundtrip and streaming check per row |
| `test_null_cipher_digest` | 55 | The NULL identity cipher (0-byte key, 0-byte IV, 1-byte block size, identity roundtrips) via `EVP_CIPHER_fetch` and `EVP_enc_null()`, and the NULL digest (0-byte output for any input) via `EVP_MD_fetch` and `EVP_md_null()` |

### Digests and MACs

| Program | Checks | Coverage |
|---|---|---|
| `test_digest_sha1` | 107 | SHA-1: KATs (empty message, one block, two-block boundary), routes, chunked-vs-one-shot |
| `test_digest_sha3` | 2089 | SHA3-224/256/384/512 and KECCAK-224/256/384/512 (3.5+): KATs, rate-boundary streaming |
| `test_digest_shake` | 115 | SHAKE128/256 XOFs and KECCAK-KMAC-128/256: KATs, `EVP_DigestSqueeze` (3.5+) |
| `test_digest_md5` | 128 | MD5, MD5-SHA1 and MD4 (legacy provider): KATs |
| `test_digest_ripemd_sm3` | 237 | RIPEMD-160 and SM3: KATs, streaming |
| `test_digest_mdc2_whirlpool` | 42 | MDC2 and WHIRLPOOL (legacy provider; MDC2 is absent from Ubuntu's 3.0.13 build) |
| `test_digest_blake2` | 1162 | BLAKE2b-512 and BLAKE2s-256: RFC 7693-style vectors, block-boundary streaming |
| `test_mac_hmac` | 329 | HMAC with SHA256/384/512 (RFC 4231) and SHA1/MD5 (RFC 2202), incl. zero-length (non-NULL) keys |
| `test_mac_cmac` | 89 | CMAC with AES-128/192/256-CBC (RFC 4493 / SP 800-38B) and DES-EDE3-CBC |
| `test_mac_gmac` | 83 | GMAC (GCM restricted to AAD-only input): NIST KATs |
| `test_mac_poly1305` | 42 | Poly1305 MAC: RFC 8439 section 2.5.2, 32-byte `r||s` key |
| `test_mac_siphash` | 121 | SipHash-2-4: KATs, 64-bit and 128-bit tag lengths |
| `test_mac_kmac` | 109 | KMAC-128/256 (SP 800-185): NIST samples, variable output length, `custom` as OCTET_STRING |
| `test_mac_blake2` | 85 | Keyed BLAKE2 via `EVP_MAC` "BLAKE2BMAC"/"BLAKE2SMAC" |

### KDFs

| Program | Checks | Coverage |
|---|---|---|
| `test_kdf_hkdf` | 112 | HKDF (RFC 5869): EXTRACT_AND_EXPAND/EXTRACT_ONLY/EXPAND_ONLY, SHA-256/SHA-1/SHA-512, multi-chunk info, error paths, concrete HKDF-SHA256/384/512 fetches (digest preset: derives with no "digest" param, byte-equal to bare HKDF + the matching digest param, CLI-verified OKM/PRK KATs per digest; per-name SKIP on 3.0.13) |
| `test_kdf_pbkdf2` | 62 | PBKDF2-HMAC-SHA1/SHA256: RFC 6070 KATs, passwords crossing the HMAC block boundary, iter counts |
| `test_kdf_pkcs12kdf` | 53 | PKCS#12 KDF (RFC 7292 B): the three material types (key/IV/MAC) with diverse hashes |
| `test_kdf_scrypt` | 27 | scrypt (RFC 7914): the two published 64-byte vectors, N/r validation, `maxmem_bytes` |
| `test_kdf_tls1prf` | 36 | TLS 1.0/1.1 MD5-SHA1 PRF and TLS 1.2 PRF: NIST-style master-secret KATs, seed/label chunking |
| `test_kdf_sskdf` | 37 | Single Step KDF (SP 800-56C): digest mode and HMAC mode (the CMAC mode has no settable "cipher" param in the provider), ASN.1 and free-form OtherInfo |
| `test_kdf_x963kdf` | 23 | X9.63 KDF ("concat KDF"): NIST SHA-256 vectors |
| `test_kdf_kbkdf` | 74 | SP 800-108 KBKDF: counter and feedback modes with CMAC/HMAC PRFs, use-l/use-separator, counter width `r` (3.2+), feedback IV |
| `test_kdf_krb5kdf` | 43 | Kerberos 5 RFC 3961 "DK" KDF: AES KATs, the n-fold expansion, constant forms |
| `test_kdf_sshkdf` | 81 | SSH RFC 4253 exchange-hash KDF: all six derivation types A..F |
| `test_kdf_x942kdf` | 29 | X9.42 KDF (RFC 2631 KEK derivation, ASN.1 OtherInfo) |
| `test_kdf_argon2` | 22 | Argon2d/i/id (RFC 9106): the three section 5 test vectors (3.5+) |
| `test_kdf_tls13kdf` | 34 | TLS 1.3 key schedule: extract-stage secrets, derive-secret-and-extract, HKDF-Expand-Label, error paths |

### Asymmetric

| Program | Checks | Coverage |
|---|---|---|
| `test_rsa_pkcs1` | 134 | RSA-2048 PKCS#1 v1.5: type 1 signatures and type 2 encryption KATs with a fixed key, message-length boundaries, implicit rejection on tamper |
| `test_rsa_oaep` | 89 | RSA-2048-OAEP: KATs (SHA-1/SHA-256, MGF1), labels, message-length boundaries, parameter rules |
| `test_rsa_pss` | 121 | RSA-2048-PSS sign/verify: KATs, salt lengths (incl. digest length and maximized), restrictions |
| `test_rsa_raw` | 35 | RSA_NO_PADDING ("textbook RSA") full-modulus roundtrips, plus RSASVE/RSA-KEM (3.2+) |
| `test_ecdsa` | 91 | ECDSA over P-256/P-384/P-521 and secp256k1: keygen, one-shot and streaming DigestSign, tamper and wrong-digest rejection, DER bounds, RFC 6979 A.2.5 deterministic KAT |
| `test_ecdh` | 46 | ECDH over the RFC 5903 P-256/P-384 groups with the fixed RFC 5114 A.6/A.7 key pairs and shared secrets |
| `test_x25519_x448` | 20067 | X25519/X448: RFC 7748 KATs, iterated vectors, low-order-point rejection, X25519-as-KEM (3.5+) |
| `test_ed25519_ed448` | 109 | Ed25519/Ed448 (PureEdDSA): RFC 8032 section 7 KATs, Ed25519ctx/ph instances (3.5+), Ed448 context |
| `test_dh` | 38 | Finite-field DH over ffdhe2048 (RFC 7919) under both "DH" and "DHX" names, fixed private keys via `EVP_PKEY_fromdata`, shared-secret KAT |
| `test_dsa` | 40 | DSA sign/verify with SHA-1/SHA-256 over generated 2048/256 parameters, plus a fixed-key KAT |
| `test_sm2` | 106 | SM2 keygen on the SM2 curve, SM2 signatures (SM3, Z-value from the distinguishing ID) and SM2 public-key encryption |

### Post-quantum and random

| Program | Checks | Coverage |
|---|---|---|
| `test_ml_kem` | 225 | ML-KEM-512/768/1024 (FIPS 203): deterministic seed keygen, encapsulation/decapsulation KATs, implicit rejection of tampered ciphertexts, raw key import/export |
| `test_ml_dsa` | 342 | ML-DSA-44/65/87 (FIPS 204): deterministic seed keygen, one-shot signature KATs, streaming signing via `EVP_DigestSign`/sign_message (3.6+), tamper detection, context binding |
| `test_slh_dsa` | 309 | SLH-DSA "fast" parameter sets (FIPS 205): deterministic seed keygen, public-key KATs, one-shot signing of small and 256 KiB messages, signature sizes, context binding |
| `test_rand_drbg` | 81 | `EVP_RAND` CTR-DRBG (AES-256-CTR), HASH-DRBG (SHA2-256), HMAC-DRBG and TEST-RAND: instantiation params, generate sizes and chunking, reseed knobs, prediction resistance, state reporting, TEST-RAND determinism |


## Behavioral notes discovered

Verified while writing the tests (each is asserted by the program named):

* XTS ciphertext stealing works in **both** the encrypt and the decrypt
  direction, contrary to some doc impressions (`test_aes_xts_small`).
* OCB accepts an AAD update **after** the first data update (`test_aes_ocb_small`).
* CCM's `ivlen`/`taglen` must be set **before** the init call that passes
  the key and nonce (`ccm_init` rejects a nonce whose length does not match
  the configured `ivlen`); defaults are `ivlen = 7` and `taglen = 12`
  (`test_aes_ccm_small`).
* A CCM nonce longer than 13 bytes caps the payload at 65535 bytes
  (`test_aes_ccm_small`).
* SIV: on 3.0.x an empty payload leaves the tag unset and Final fails; 3.6.x
  computes the synthetic IV and succeeds (`test_aes_siv`).
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
  happens inside the provider (`test_ed25519_ed448`).
* The ETM AES-CBC-HMAC-SHA* variants are ARM64-only in 3.6.5
  (`test_aes_cbc_hmac_sha1`, `test_aes_cbc_hmac_sha256`).
* SLH-DSA is one-shot only — the provider exports no streaming
  sign/verify, so 256 KiB messages are signed in a single call
  (`test_slh_dsa`).
* ML-DSA streaming uses the 3.6-era `EVP_PKEY_{sign,verify}_message_*` API
  (`test_ml_dsa`).
* The Poly1305 MAC key is 32 bytes, the `r||s` pair (`test_mac_poly1305`).
* KMAC's `custom` must be an OCTET_STRING of exactly `strlen(custom)` bytes
  (no trailing NUL); a UTF8_STRING passes on 3.0.x but is rejected by the
  3.6.x type check, and a NUL inside the string silently changes the tag
  (`test_mac_kmac`).
* HMAC accepts a zero-length key, but the key pointer must be non-NULL
  (`test_mac_hmac`).
* OSSL_PARAM integers are **native-endian** — big-endian key constants must
  be byte-swapped before `EVP_PKEY_fromdata`
  (`test_rsa_*`, `test_dh`, `test_dsa`, `test_ecdh`, `test_ecdsa`).
* Loading the legacy provider explicitly turns off default-provider
  auto-load, so a program that needs both must load both
  (`test_digest_md5`, `test_digest_mdc2_whirlpool`).
* CBC-CTS has existed since 3.0 (it is not 3.2-gated) and behaves
  identically on 3.0.13 and 3.6.5 (`test_aes_cbc_cts`).
* KBKDF's parameter names are shifted: `"salt"` is the SP 800-108 *Label*,
  `"info"` the *Context*, and `"seed"` the feedback-mode IV
  (`test_kdf_kbkdf`).
* HMAC-DRBG requires `"mac"` and `"digest"` in a single
  `EVP_RAND_CTX_set_params` call — splitting them across two calls fails
  (`test_rand_drbg`).
* The raw private-key import of FFC keys (DH/DSA) does not derive the
  public key, so both must be supplied (`test_dh`, `test_dsa`).
* DES3-WRAP differs from AES-WRAP: the wrap direction requires only
  `inl % 8 == 0` (no `>= 16` minimum — an 8-byte input wraps fine) and
  the output is `inl + 16` (a random 8-byte IV plus an 8-byte SHA-1 ICV,
  both inside the CBC-encrypted, reversed blob); the unwrap direction
  requires `>= 24` bytes and verifies the ICV (`test_des3_wrap`,
  `cipher_tdes_wrap.c`).
* DES-CFB1 has **no `use_bits` branch** in its provider `hw`:
  `EVP_CIPH_FLAG_LENGTH_BITS` is accepted but ignored, so a bit-count
  update would overread — Fil-C trapped the exact overread during
  development.  `test_des` pins byte mode only (`cipher_des_hw.c`).
* ChaCha20-Poly1305: an AAD update **after** the first data update is
  rejected on 3.6.5 but accepted on 3.0.13
  (`cipher_chacha20_poly1305_hw.c:303-309`), and a decrypt without
  `SET_TAG` verifies nothing — the tag length defaults to 0
  (`test_chacha20_poly1305_small`).
* RC2-64-CBC's key is 8 bytes (= 64 bits, not 6); RC2-40-CBC's is 5
  (`EVP_CIPHER_get_key_length`, `test_legacy_block_ciphers`).
* The Camellia RFC 3713 KAT ciphertext is
  `67673138549669730857065648eabe43` — the commonly recirculated
  `6767313314...` value is wrong (`test_camellia`).
* SM4-XTS/GCM/CCM are absent from OpenSSL 3.0.13 and present in 3.6.5;
  `test_sm4` gates each with its own version-gated SKIP line.

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
  3.0), so they cannot be exercised on this machine; the programs detect
  that and print a SKIP line (`test_aes_cbc_hmac_sha256`).

## Adding a test

One cryptosystem per program; the file is named `test_<thing>.c` and is
picked up by the Makefile automatically.  House rules:

1. `#include "poisoning.h"` first, then the OpenSSL headers.
2. Right after the includes, define the labeled `ALLOCATE_*` functions the
   program needs — one line each, e.g.
   `static void *ALLOCATE_KEY(size_t n) { return poison_alloc(POISON_KEY, n); }`.
   Every buffer handed to OpenSSL must come from one of these.  Known-answer
   constants may be `static const` strings (hex is convenient), but must be
   copied into `ALLOCATE_*` buffers before use.
3. `main()` starts with `poison_parse_args(argc, argv);`.
4. Each program is fully self-contained: only `poisoning.h` is shared; no new
   shared headers, no helper `.c` files, no file I/O, no VLAs, no `alloca`,
   no threads/signals.  C99.  Public OpenSSL 3 API only (`EVP_CIPHER_fetch`,
   `EVP_MD_fetch`, `EVP_MAC_fetch`, `EVP_KDF_fetch`, `EVP_PKEY_*`,
   `OSSL_PARAM`, `OSSL_PROVIDER_load`, ...); no deprecated low-level APIs.
   `EVP_get_cipherbyname`/`EVP_get_digestbyname` are fine.
5. Failure handling: a `static void test_failed(const char *msg)` plus a
   `CHECK(cond, msg)` macro; on failure print `FAIL <program>: <msg>` to
   stderr and exit(1).  Track passing checks in `g_checks` and, on success,
   print `<program>: all N checks passed` and exit(0).  If a genuinely
   version-gated algorithm is not provided by this OpenSSL (its
   `EVP_*_fetch` fails), print `SKIP <program>: <what> is unavailable` and
   exit(0) — never skip core things present in all of OpenSSL 3.x.  A
   version-dependent *subtest* prints a `SKIP` line and keeps going, and a
   version-dependent behavior difference between two builds is asserted as
   "3.0.x does X, 3.6.x does Y" in a comment.
6. Size output buffers per the EVP contract: each Encrypt/Decrypt update can
   emit up to `inl + block_size` bytes and Final up to `block_size` bytes,
   so allocate `ALLOCATE_OUTPUT` accordingly and CHECK the contract.
7. For AEAD ciphers, set `ivlen`/`taglen` **before** the init call that
   passes the key and IV (CCM outright rejects a nonce whose length does
   not match the configured `ivlen`), and remember SIV/CCM-style rules:
   one payload per init (SIV), the length announcement update (CCM).
8. OSSL_PARAM integer parameters are native-endian: byte-swap big-endian
   constants (e.g. fixed RSA/DH keys) before `EVP_PKEY_fromdata`.
9. Known-answer vectors must be verified against the authoritative build
   before being embedded: use `pizfix/bin/openssl` (OpenSSL 3.6.5) in /tmp
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
add its row to the test list above.
