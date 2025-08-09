/* hash.c - Calculate various cryptographic hashes.
 *
 * Copyright 2012, 2021 Rob Landley <rob@landley.net>
 *
 * See http://www.ietf.org/rfc/rfc1321.txt
 * and http://www.ietf.org/rfc/rfc4634.txt
 */

#include "toys.h"

// Use external library of hand-coded assembly implementations?
#if CFG_TOYBOX_LIBCRYPTO
#include <openssl/md5.h>
#include <openssl/sha.h>

// Initialize array tersely
#define HASH_INIT(name, prefix) { name, (void *)prefix##_Init, \
  (void *)prefix##_Update, (void *)prefix##_Final, \
  prefix##_DIGEST_LENGTH, }
#define SHA1_DIGEST_LENGTH SHA_DIGEST_LENGTH

void hash_by_name(int fd, char *name, char *result)
{
  // Largest context
  SHA512_CTX ctx;
  struct hash {
    char *name;
    int (*init)(void *);
    int (*update)(void *, void *, size_t);
    int (*final)(void *, void *);
    int digest_length;
  } algorithms[] = {
    USE_MD5SUM(HASH_INIT("md5sum", MD5),)
    USE_SHA1SUM(HASH_INIT("sha1sum", SHA1),)
    USE_SHA224SUM(HASH_INIT("sha224sum", SHA224),)
    USE_SHA256SUM(HASH_INIT("sha256sum", SHA256),)
    USE_SHA384SUM(HASH_INIT("sha384sum", SHA384),)
    USE_SHA512SUM(HASH_INIT("sha512sum", SHA512),)
  }, * hash;
  int i;

  // This should never NOT match, so no need to check
  for (i = 0; i<ARRAY_LEN(algorithms); i++)
    if (!strcmp(name, algorithms[i].name)) break;
  hash = algorithms+i;

  hash->init(&ctx);
  for (;;) {
      i = read(fd, libbuf, sizeof(libbuf));
      if (i<1) break;
      hash->update(&ctx, libbuf, i);
  }
  hash->final(libbuf+128, &ctx);

  for (i = 0; i<hash->digest_length; i++)
    result += sprintf(result, "%02x", libbuf[i+128]);
}

// Builtin implementations
#else

struct browns {
  unsigned *rconsttable32;
  unsigned long long *rconsttable64; // for sha384,sha512

  // Crypto variables blanked after summing
  unsigned long long count, overflow;
  union {
    char c[128]; // bytes, 1024 bits
    unsigned i32[16]; // 512 bits for md5,sha1,sha224,sha256
    unsigned long long i64[16]; // 1024 bits for sha384,sha512
  } state, buffer;
};

// Round constants. Static table for when we haven't got floating point support
#if ! CFG_TOYBOX_FLOAT
static const unsigned md5nofloat[64] = {
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
  0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
  0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
  0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
  0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
  0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};
#else
#define md5nofloat 0
#endif
static unsigned long long sha512nofloat[80] = {
  // we cannot calculate these 64-bit values using the readily
  // available floating point data types and math functions,
  // so we always use this lookup table (80 * 8 bytes)
  0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f,
  0xe9b5dba58189dbbc, 0x3956c25bf348b538, 0x59f111f1b605d019,
  0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242,
  0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
  0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
  0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3,
  0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65, 0x2de92c6f592b0275,
  0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
  0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f,
  0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
  0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc,
  0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
  0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6,
  0x92722c851482353b, 0xa2bfe8a14cf10364, 0xa81a664bbc423001,
  0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
  0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
  0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99,
  0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb,
  0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc,
  0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
  0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915,
  0xc67178f2e372532b, 0xca273eceea26619c, 0xd186b8c721c0c207,
  0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba,
  0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
  0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
  0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a,
  0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};
// sha1 needs only 4 round constant values, so prefer precomputed
static const unsigned sha1rconsts[] = {
  0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6
};

// bit rotations
#define rol(value, bits) (((value)<<(bits))|((value)>>(sizeof(value)*8-(bits))))
#define ror(value, bits) (((value)>>(bits))|((value)<<(sizeof(value)*8-(bits))))

// Mix next 64 bytes of data into md5 hash

static void md5_transform(struct browns *hash)
{
  unsigned x[4], *b = hash->buffer.i32;
  int i;

  for (i = 0; i<4; i++) x[i] = hash->state.i32[i];
  for (i = 0; i<64; i++) {
    unsigned in, a, rot, temp;

    a = (-i)&3;
    if (i<16) {
      in = i;
      rot = 7+(5*(i&3));
      temp = x[(a+1)&3];
      temp = (temp & x[(a+2)&3]) | ((~temp) & x[(a+3)&3]);
    } else if (i<32) {
      in = (1+(5*i))&15;
      temp = (i&3)+1;
      rot = temp*5;
      if (temp&2) rot--;
      temp = x[(a+3)&3];
      temp = (x[(a+1)&3] & temp) | (x[(a+2)&3] & ~temp);
    } else if (i<48) {
      in = (5+(3*(i&15)))&15;
      rot = i&3;
      rot = 4+(5*rot)+((rot+1)&6);
      temp = x[(a+1)&3] ^ x[(a+2)&3] ^ x[(a+3)&3];
    } else {
      in = (7*(i&15))&15;
      rot = (i&3)+1;
      rot = (5*rot)+(((rot+2)&2)>>1);
      temp = x[(a+2)&3] ^ (x[(a+1)&3] | ~x[(a+3)&3]);
    }
    temp += x[a] + SWAP_LE32(b[in]) + hash->rconsttable32[i];
    x[a] = x[(a+1)&3] + ((temp<<rot) | (temp>>(32-rot)));
  }
  for (i = 0; i<4; i++) hash->state.i32[i] += x[i];
}

// Mix next 64 bytes of data into sha1 hash.

static void sha1_transform(struct browns *hash)
{
  int i, j, k, count;
  unsigned *block = hash->buffer.i32, oldstate[5], *rot[5], *temp, work;

  // Copy context->state.i32[] to working vars
  for (i = 0; i<5; i++) {
    oldstate[i] = hash->state.i32[i];
    rot[i] = hash->state.i32 + i;
  }
  if (IS_BIG_ENDIAN) for (i = 0; i<16; i++) block[i] = SWAP_LE32(block[i]);

  // 4 rounds of 20 operations each.
  for (i = count = 0; i<4; i++) {
    for (j = 0; j<20; j++) {
      work = *rot[2] ^ *rot[3];
      if (!i) work = (work & *rot[1]) ^ *rot[3];
      else {
        if (i==2) work = ((*rot[1]|*rot[2])&*rot[3])|(*rot[1]&*rot[2]);
        else work ^= *rot[1];
      }

      if (!i && j<16)
        work += block[count] = (ror(block[count],8)&0xFF00FF00)
                             | (rol(block[count],8)&0x00FF00FF);
      else
        work += block[count&15] = rol(block[(count+13)&15]
              ^ block[(count+8)&15] ^ block[(count+2)&15] ^ block[count&15], 1);
      *rot[4] += work + rol(*rot[0],5) + sha1rconsts[i];
      *rot[1] = rol(*rot[1],30);

      // Rotate by one for next time.
      temp = rot[4];
      for (k = 4; k; k--) rot[k] = rot[k-1];
      *rot = temp;
      count++;
    }
  }
  // Add the previous values of state.i32[]
  for (i = 0; i<5; i++) hash->state.i32[i] += oldstate[i];
}

static void sha2_32_transform(struct browns *hash)
{
  unsigned block[64], s0, s1, S0, S1, ch, maj, temp1, temp2, rot[8];
  int i;

  for (i = 0; i<16; i++) block[i] = SWAP_BE32(hash->buffer.i32[i]);

  // Extend the message schedule array beyond first 16 words
  for (i = 16; i<64; i++) {
    s0 = ror(block[i-15], 7) ^ ror(block[i-15], 18) ^ (block[i-15] >> 3);
    s1 = ror(block[i-2], 17) ^ ror(block[i-2], 19) ^ (block[i-2] >> 10);
    block[i] = block[i-16] + s0 + block[i-7] + s1;
  }
  // Copy context->state.i32[] to working vars
  for (i = 0; i<8; i++) rot[i] = hash->state.i32[i];
  // 64 rounds
  for (i = 0; i<64; i++) {
    S1 = ror(rot[4],6) ^ ror(rot[4],11) ^ ror(rot[4], 25);
    ch = (rot[4] & rot[5]) ^ ((~ rot[4]) & rot[6]);
    temp1 = rot[7] + S1 + ch + hash->rconsttable32[i] + block[i];
    S0 = ror(rot[0],2) ^ ror(rot[0],13) ^ ror(rot[0], 22);
    maj = (rot[0] & rot[1]) ^ (rot[0] & rot[2]) ^ (rot[1] & rot[2]);
    temp2 = S0 + maj;
    memmove(rot+1, rot, 7*sizeof(*rot));
    rot[4] += temp1;
    rot[0] = temp1 + temp2;
  }

  // Add the previous values of state.i32[]
  for (i = 0; i<8; i++) hash->state.i32[i] += rot[i];
}

static void sha2_64_transform(struct browns *hash)
{
  unsigned long long block[80], s0, s1, S0, S1, ch, maj, temp1, temp2, rot[8];
  int i;

  for (i=0; i<16; i++) block[i] = SWAP_BE64(hash->buffer.i64[i]);

  // Extend the message schedule array beyond first 16 words
  for (i = 16; i<80; i++) {
    s0 = ror(block[i-15], 1) ^ ror(block[i-15], 8) ^ (block[i-15] >> 7);
    s1 = ror(block[i-2], 19) ^ ror(block[i-2], 61) ^ (block[i-2] >> 6);
    block[i] = block[i-16] + s0 + block[i-7] + s1;
  }
  // Copy context->state.i64[] to working vars
  for (i = 0; i<8; i++) rot[i] = hash->state.i64[i];
  // 80 rounds
  for (i = 0; i<80; i++) {
    S1 = ror(rot[4],14) ^ ror(rot[4],18) ^ ror(rot[4], 41);
    ch = (rot[4] & rot[5]) ^ ((~ rot[4]) & rot[6]);
    temp1 = rot[7] + S1 + ch + hash->rconsttable64[i] + block[i];
    S0 = ror(rot[0],28) ^ ror(rot[0],34) ^ ror(rot[0], 39);
    maj = (rot[0] & rot[1]) ^ (rot[0] & rot[2]) ^ (rot[1] & rot[2]);
    temp2 = S0 + maj;
    memmove(rot+1, rot, 7*sizeof(*rot));
    rot[4] += temp1;
    rot[0] = temp1 + temp2;
  }

  // Add the previous values of state.i64[]
  for (i=0; i<8; i++) hash->state.i64[i] += rot[i];
}

// Fill 64/128-byte (512/1024-bit) working buffer, call transform() when full.

static void hash_update(char *data, unsigned int len,
  void (*transform)(struct browns *hash), int chunksize, struct browns *hash)
{
  unsigned int i, j;

  j = hash->count & (chunksize - 1);
  if (hash->count+len<hash->count) hash->overflow++;
  hash->count += len;

  for (;;) {
    // Grab next chunk of data, return if it's not enough to process a frame
    i = chunksize - j;
    if (i>len) i = len;
    memcpy(hash->buffer.c+j, data, i);
    if (j+i != chunksize) break;

    // Process a frame
    transform(hash);
    j=0;
    data += i;
    len -= i;
  }
}

void hash_by_name(int fd, char *name, char *result)
{
  unsigned long long count[2];
  int i, chunksize, digestlen, method;
  volatile unsigned *pp;
  void (*transform)(struct browns *hash);
  struct browns *hash = xzalloc(sizeof(struct browns));
  char buf;

  // md5sum, sha1sum, sha224sum, sha256sum, sha384sum, sha512sum
  method = stridx("us2581", name[4]);

  // Calculate table if we have floating point. Static version should drop
  // out at compile time when we don't need it.
  if (!method) { // MD5
    if (CFG_TOYBOX_FLOAT) {
      hash->rconsttable32 = xmalloc(64*4);
      for (i = 0; i<64; i++) hash->rconsttable32[i] = fabs(sin(i+1))*(1LL<<32);
    } else hash->rconsttable32 = (void *)md5nofloat;
  } else if (name[3] == '2') { // sha224, sha256
    hash->rconsttable32 = xmalloc(64*4);
    for (i=0; i<64; i++) hash->rconsttable32[i] = sha512nofloat[i] >> 32;
  } else hash->rconsttable64 = sha512nofloat; // sha384, sha512

  // select hash type
  transform = (void *[]){md5_transform, sha1_transform, sha2_32_transform,
    sha2_32_transform, sha2_64_transform, sha2_64_transform}[method];
  digestlen = (char []){16, 20, 28, 32, 48, 64}[method];
  chunksize = 64<<(method>=4);
  if (method<=1)
    memcpy(hash->state.i32, (unsigned []){0x67452301, 0xEFCDAB89, 0x98BADCFE,
      0x10325476, 0xC3D2E1F0}, 20);
  else if (method==2)
    memcpy(hash->state.i32, (unsigned []){0xc1059ed8, 0x367cd507, 0x3070dd17,
      0xf70e5939, 0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4}, 32);
  else if (method==3)
    memcpy(hash->state.i32, (unsigned []){0x6a09e667, 0xbb67ae85, 0x3c6ef372,
      0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19}, 32);
  else if (method==4)
    memcpy(hash->state.i64, (unsigned long long []){0xcbbb9d5dc1059ed8,
      0x629a292a367cd507, 0x9159015a3070dd17, 0x152fecd8f70e5939,
      0x67332667ffc00b31, 0x8eb44a8768581511, 0xdb0c2e0d64f98fa7,
      0x47b5481dbefa4fa4}, 64);
  else memcpy(hash->state.i64, (unsigned long long []){0x6a09e667f3bcc908,
      0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
      0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b,
      0x5be0cd19137e2179}, 64);

  hash->count = 0;
  for (;;) {
    i = read(fd, libbuf, sizeof(libbuf));
    if (i<1) break;
    hash_update(libbuf, i, transform, chunksize, hash);
  }

  // End the message by appending a "1" bit to the data, ending with the
  // message size (in bits, big endian), and adding enough zero bits in
  // between to pad to the end of the next frame.
  //
  // Since our input up to now has been in whole bytes, we can deal with
  // bytes here too. sha384 and 512 use 128 bit counter, so track overflow.
  buf = 0x80;
  count[0] = (hash->overflow<<3)+(hash->count>>61);
  count[1] = hash->count<<3; // convert to bits
  for (i = 0; i<2; i++)
    count[i] = !method ? SWAP_LE64(count[i]) : SWAP_BE64(count[i]);
  i = 8<<(method>=4);
  do {
    hash_update(&buf, 1, transform, chunksize, hash);
    buf = 0;
  } while ((hash->count&(chunksize-1)) != chunksize-i);
  hash_update((void *)(count+(method<4)), i, transform, chunksize, hash);

  // write digest to result
  if (method>=4) for (i=0; i<digestlen/8; i++)
    result += sprintf(result, "%016llx", hash->state.i64[i]);
  else for (i=0; i<digestlen/4; i++)
    result += sprintf(result, "%08x",
            !method ? bswap_32(hash->state.i32[i]) : hash->state.i32[i]);
  // Wipe variables. Cryptographer paranoia. Avoid "optimizing" out memset
  // by looping on a volatile pointer.
  for (pp = (void *)hash; pp-(unsigned *)hash<sizeof(*hash)/4; pp++) *pp = 0;
  for (pp = (void *)libbuf; pp-(unsigned *)libbuf<sizeof(libbuf)/4; pp++)
    *pp = 0;
}
#endif
