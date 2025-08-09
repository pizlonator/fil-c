/* deflate.c - deflate/inflate code for gzip and friends
 *
 * Copyright 2014 Rob Landley <rob@landley.net>
 *
 * See RFCs 1950 (zlib), 1951 (deflate), and 1952 (gzip)
 * LSB 4.1 has gzip, gunzip, and zcat
 *
 * TODO: zip -d DIR -x LIST -list -quiet -no overwrite -overwrite -p to stdout
 */

#include "toys.h"

struct deflate {
  // Huffman codes: base offset and extra bits tables (length and distance)
  char lenbits[29], distbits[30];
  unsigned short lenbase[29], distbase[30];
  void *fixdisthuff, *fixlithuff;

  // CRC
  void (*crcfunc)(struct deflate *dd, char *data, unsigned len);
  unsigned crctable[256], crc;


  // Tables only used for deflation
  unsigned short *hashhead, *hashchain;

  // Compressed data buffer (extra space malloced at end)
  unsigned pos, len;
  int infd, outfd, outbuflen;
  char *outbuf, data[];
};

// little endian bit buffer
struct bitbuf {
  int fd, bitpos, len, max;
  char *buf, data[];
};

// malloc a struct bitbuf
static struct bitbuf *bitbuf_init(int fd, int size)
{
  struct bitbuf *bb = xzalloc(sizeof(struct bitbuf)+size);

  bb->max = size;
  bb->fd = fd;
  bb->buf = bb->data;

  return bb;
}

// Advance bitpos without the overhead of recording bits
// Loads more data when input buffer empty
// call with 0 to just load data, returns 0 at EOF
static int bitbuf_skip(struct bitbuf *bb, int bits)
{
  int pos = bb->bitpos + bits + (bits<0), len;

  while (pos >= (len = bb->len<<3)) {
    pos -= len;
    if (1 > (bb->len = read(bb->fd, bb->buf, bb->max))) {
      if (!bits) break;
      error_exit("inflate EOF");
    }
  }
  bb->bitpos = pos;

  return pos<len;
}

// Optimized single bit inlined version
static inline int bitbuf_bit(struct bitbuf *bb)
{
  int bufpos = bb->bitpos>>3;

  if (bufpos == bb->len) {
    bitbuf_skip(bb, -1);
    bufpos = 0;
  }

  return (bb->buf[bufpos]>>(bb->bitpos++&7))&1;
}

// Fetch the next X bits from the bitbuf, little endian
static unsigned bitbuf_get(struct bitbuf *bb, int bits)
{
  int result = 0, offset = 0;

  while (bits) {
    int click = bb->bitpos >> 3, blow, blen;

    // Load more data if buffer empty
    if (click == bb->len) {
      bitbuf_skip(bb, -1);
      click = 0;
    }

    // grab bits from next byte
    blow = bb->bitpos & 7;
    blen = 8-blow;
    if (blen > bits) blen = bits;
    result |= ((bb->buf[click] >> blow) & ((1<<blen)-1)) << offset;
    offset += blen;
    bits -= blen;
    bb->bitpos += blen;
  }

  return result;
}

static void bitbuf_flush(struct bitbuf *bb)
{
  if (!bb->bitpos) return;

  xwrite(bb->fd, bb->buf, (bb->bitpos+7)>>3);
  memset(bb->buf, 0, bb->max);
  bb->bitpos = 0;
}

static void bitbuf_put(struct bitbuf *bb, int data, int len)
{
  while (len) {
    int click = bb->bitpos >> 3, blow, blen;

    // Flush buffer if necessary
    if (click == bb->max) {
      bitbuf_flush(bb);
      click = 0;
    }
    blow = bb->bitpos & 7;
    blen = 8-blow;
    if (blen > len) blen = len;
    bb->buf[click] |= data << blow;
    bb->bitpos += blen;
    data >>= blen;
    len -= blen;
  }
}

// Output inflated data
static void inflate_out(struct deflate *dd, int len)
{
  if (!len) return;
  if (dd->outfd!=-1) xwrite(dd->outfd, dd->data, len);
  else if (len>dd->outbuflen) error_exit("inflate too big");
  else {
    memcpy(dd->outbuf, dd->data, len);
    dd->outbuf += len;
    dd->outbuflen -= len;
  }
  if (dd->crcfunc) dd->crcfunc(dd, dd->data, len);
}

static void output_byte(struct deflate *dd, char sym)
{
  int pos = dd->pos++ & 32767;

  dd->data[pos] = sym;
  if (pos == 32767) inflate_out(dd, 32768);
}

// Huffman coding uses bits to traverse a binary tree to a leaf node,
// By placing frequently occurring symbols at shorter paths, frequently
// used symbols may be represented in fewer bits than uncommon symbols.
// (length[0] isn't used but code's clearer if it's there.)

struct huff {
  unsigned short length[16];  // How many symbols have this bit length?
  unsigned short symbol[288]; // sorted by bit length, then ascending order
};

// Create simple huffman tree from array of bit lengths.

// The symbols in the huffman trees are sorted (first by bit length
// of the code to reach them, then by symbol number). This means that given
// the bit length of each symbol, we can construct a unique tree.
static void len2huff(struct huff *huff, char bitlen[], int len)
{
  int offset[16];
  int i;

  // Count number of codes at each bit length
  memset(huff, 0, sizeof(struct huff));
  for (i = 0; i<len; i++) huff->length[bitlen[i]]++;

  // Sort symbols by bit length, then symbol. Get list of starting positions
  // for each group, then write each symbol to next position within its group.
  *huff->length = *offset = 0;
  for (i = 1; i<16; i++) offset[i] = offset[i-1] + huff->length[i-1];
  for (i = 0; i<len; i++) if (bitlen[i]) huff->symbol[offset[bitlen[i]]++] = i;
}

// Fetch and decode next huffman coded symbol from bitbuf.
// This takes advantage of the sorting to navigate the tree as an array:
// each time we fetch a bit we have all the codes at that bit level in
// order with no gaps.
static unsigned huff_and_puff(struct bitbuf *bb, struct huff *huff)
{
  unsigned short *length = huff->length;
  int start = 0, offset = 0;

  // Traverse through the bit lengths until our code is in this range
  for (;;) {
    offset = (offset << 1) | bitbuf_bit(bb);
    start += *++length;
    if ((offset -= *length) < 0) break;
    if ((length - huff->length) & 16) error_exit("bad symbol");
  }

  return huff->symbol[start + offset];
}

// Decompress deflated data from bitbuf to dd->outfd.
static void inflate(struct deflate *dd, struct bitbuf *bb)
{
  dd->crc = ~0;

  // repeat until spanked
  for (;;) {
    int final, type;

    final = bitbuf_get(bb, 1);
    type = bitbuf_get(bb, 2);

    if (type == 3) error_exit("bad type");

    // Uncompressed block?
    if (!type) {
      int len, nlen;

      // Align to byte, read length
      bitbuf_skip(bb, (8-bb->bitpos)&7);
      len = bitbuf_get(bb, 16);
      nlen = bitbuf_get(bb, 16);
      if (len != (0xffff & ~nlen)) error_exit("bad len");

      // Dump literal output data
      while (len) {
        int pos = bb->bitpos >> 3, bblen = bb->len - pos;
        char *p = bb->buf+pos;

        // dump bytes until done or end of current bitbuf contents
        if (bblen > len) bblen = len;
        pos = bblen;
        while (pos--) output_byte(dd, *(p++));
        bitbuf_skip(bb, bblen << 3);
        len -= bblen;
      }

    // Compressed block
    } else {
      struct huff *disthuff, *lithuff;

      // Dynamic huffman codes?
      if (type == 2) {
        struct huff *h2 = ((struct huff *)libbuf)+1;
        int i, litlen, distlen, hufflen;
        char *hufflen_order = "\x10\x11\x12\0\x08\x07\x09\x06\x0a\x05\x0b"
                              "\x04\x0c\x03\x0d\x02\x0e\x01\x0f", *bits;

        // The huffman trees are stored as a series of bit lengths
        litlen = bitbuf_get(bb, 5)+257;  // max 288
        distlen = bitbuf_get(bb, 5)+1;   // max 32
        hufflen = bitbuf_get(bb, 4)+4;   // max 19

        // The literal and distance codes are themselves compressed, in
        // a complicated way: an array of bit lengths (hufflen many
        // entries, each 3 bits) is used to fill out an array of 19 entries
        // in a magic order, leaving the rest 0. Then make a tree out of it:
        memset(bits = libbuf+1, 0, 19);
        for (i=0; i<hufflen; i++) bits[hufflen_order[i]] = bitbuf_get(bb, 3);
        len2huff(h2, bits, 19);

        // Use that tree to read in the literal and distance bit lengths
        for (i = 0; i < litlen + distlen;) {
          int sym = huff_and_puff(bb, h2);

          // 0-15 are literals, 16 = repeat previous code 3-6 times,
          // 17 = 3-10 zeroes (3 bit), 18 = 11-138 zeroes (7 bit)
          if (sym < 16) bits[i++] = sym;
          else {
            int len = sym & 2;

            len = bitbuf_get(bb, sym-14+len+(len>>1)) + 3 + (len<<2);
            memset(bits+i, bits[i-1] * !(sym&3), len);
            i += len;
          }
        }
        if (i > litlen+distlen) error_exit("bad tree");

        len2huff(lithuff = h2, bits, litlen);
        len2huff(disthuff = ((struct huff *)libbuf)+2, bits+litlen, distlen);

      // Static huffman codes
      } else {
        lithuff = dd->fixlithuff;
        disthuff = dd->fixdisthuff;
      }

      // Use huffman tables to decode block of compressed symbols
      for (;;) {
        int sym = huff_and_puff(bb, lithuff);

        // Literal?
        if (sym < 256) output_byte(dd, sym);

        // Copy range?
        else if (sym > 256) {
          int len, dist;

          sym -= 257;
          len = dd->lenbase[sym] + bitbuf_get(bb, dd->lenbits[sym]);
          sym = huff_and_puff(bb, disthuff);
          dist = dd->distbase[sym] + bitbuf_get(bb, dd->distbits[sym]);
          sym = dd->pos & 32767;

          while (len--) output_byte(dd, dd->data[(dd->pos-dist) & 32767]);

        // End of block
        } else break;
      }
    }

    // Was that the last block?
    if (final) break;
  }

  if (dd->pos & 32767) inflate_out(dd, dd->pos&32767);
}

// Deflate from dd->infd to bitbuf
// For deflate, dd->len = input read, dd->pos = input consumed
static void deflate(struct deflate *dd, struct bitbuf *bb)
{
  char *data = dd->data;
  int len, final = 0;

  dd->crc = ~0;

  while (!final) {
    // Read next half-window of data if we haven't hit EOF yet.
    len = readall(dd->infd, data+(dd->len&32768), 32768);
    if (len < 0) perror_exit("read"); // TODO: add filename
    if (len != 32768) final++;
    if (dd->crcfunc) dd->crcfunc(dd, data+(dd->len&32768), len);
    // dd->len += len;  crcfunc advances len TODO

    // store block as literal
// TODO: actually compress!
    bitbuf_put(bb, final, 1);
    bitbuf_put(bb, 0, 1);

    bitbuf_put(bb, 0, (8-bb->bitpos)&7);
    bitbuf_put(bb, len, 16);
    bitbuf_put(bb, 0xffff & ~len, 16);

    // repeat until spanked
    while (dd->pos != dd->len) {
      unsigned pos = dd->pos&65535;

      bitbuf_put(bb, data[pos], 8);

      // need to refill buffer?
      if (!(32767 & ++dd->pos) && !final) break;
    }
  }
  bitbuf_flush(bb);
}

// Allocate memory for deflate/inflate.
static struct deflate *init_deflate(int compress)
{
  int i, n = 1;
  struct deflate *dd = xmalloc(sizeof(struct deflate)+32768*(compress ? 4 : 1));

  memset(dd, 0, sizeof(struct deflate));
  // decompress needs 32k history, compress adds 64k hashhead and 32k hashchain
  if (compress) {
    dd->hashhead = (unsigned short *)(dd->data+65536);
    dd->hashchain = (unsigned short *)(dd->data+65536+32768);
  }

  // Calculate lenbits, lenbase, distbits, distbase
  *dd->lenbase = 3;
  for (i = 0; i<sizeof(dd->lenbits)-1; i++) {
    if (i>4) {
      if (!(i&3)) {
        dd->lenbits[i]++;
        n <<= 1;
      }
      if (i == 27) n--;
      else dd->lenbits[i+1] = dd->lenbits[i];
    }
    dd->lenbase[i+1] = n + dd->lenbase[i];
  }
  n = 0;
  for (i = 0; i<sizeof(dd->distbits); i++) {
    dd->distbase[i] = 1<<n;
    if (i) dd->distbase[i] += dd->distbase[i-1];
    if (i>3 && !(i&1)) n++;
    dd->distbits[i] = n;
  }

// TODO layout and lifetime of this?
  // Init fixed huffman tables
  for (i=0; i<288; i++) libbuf[i] = 8 + (i>143) - ((i>255)<<1) + (i>279);
  len2huff(dd->fixlithuff = ((struct huff *)libbuf)+3, libbuf, 288);
  memset(libbuf, 5, 30);
  len2huff(dd->fixdisthuff = ((struct huff *)libbuf)+4, libbuf, 30);

  return dd;
}

// Return true/false whether we consumed a gzip header.
static int is_gzip(struct bitbuf *bb)
{
  int flags;

  // Confirm signature
  if (bitbuf_get(bb, 24) != 0x088b1f || (flags = bitbuf_get(bb, 8)) > 31)
    return 0;
  bitbuf_skip(bb, 6*8);

  // Skip extra, name, comment, header CRC fields
  if (flags & 4) bitbuf_skip(bb, bitbuf_get(bb, 16) * 8);
  if (flags & 8) while (bitbuf_get(bb, 8));
  if (flags & 16) while (bitbuf_get(bb, 8));
  if (flags & 2) bitbuf_skip(bb, 16);

  return 1;
}

static void gzip_crc(struct deflate *dd, char *data, unsigned len)
{
  int i;
  unsigned crc, *crc_table = dd->crctable;

  crc = dd->crc;
  for (i = 0; i<len; i++) crc = crc_table[(crc^data[i])&0xff] ^ (crc>>8);
  dd->crc = crc;
  dd->len += len;
}

/*
// Start with crc = 1, or pass in last crc to append more data
// Deferred modulus good for paged size inputs (can't overflow for ~5500 bytes)
unsigned adler32(char *buf, unsigned len, unsigned crc)
{
  unsigned aa = crc&((1<<16)-1), bb = crc>>16;

  while (len--) {
    aa += *buf++;
    bb += aa;
  }

  return ((bb%65521)<<16)+aa%65521;
}
*/

long long gzip_fd(int infd, int outfd)
{
  struct bitbuf *bb = bitbuf_init(outfd, 4096);
  struct deflate *dd = init_deflate(1);
  long long rc;

  // Header from RFC 1952 section 2.2:
  // 2 ID bytes (1F, 8b), gzip method byte (8=deflate), FLAG byte (none),
  // 4 byte MTIME (zeroed), Extra Flags (2=maximum compression),
  // Operating System (FF=unknown)

  dd->infd = infd;
  xwrite(bb->fd, "\x1f\x8b\x08\0\0\0\0\0\x02\xff", 10);

  // Little endian crc table
  crc_init(dd->crctable, 1);
  dd->crcfunc = gzip_crc;

  deflate(dd, bb);

  // tail: crc32, len32

  bitbuf_put(bb, 0, (8-bb->bitpos)&7);
  bitbuf_put(bb, ~dd->crc, 32);
  bitbuf_put(bb, dd->len, 32);
  rc = dd->len;

  bitbuf_flush(bb);
  free(bb);
  free(dd);

  return rc;
}

long long gunzip_common(struct bitbuf *bb, struct deflate *dd)
{
  long long rc = 0;

  // Little endian crc table
  crc_init(dd->crctable, 1);
  dd->crcfunc = gzip_crc;

  do {
    if (!is_gzip(bb)) error_exit("not gzip");

    inflate(dd, bb);
    // tail: crc32, len32
    bitbuf_skip(bb, (8-bb->bitpos)&7);
    if (~dd->crc != bitbuf_get(bb, 32) || dd->len != bitbuf_get(bb, 32))
      error_exit("bad crc");
    rc += dd->len;
    bitbuf_skip(bb, (8-bb->bitpos)&7);
    dd->pos = dd->len = 0;
  } while (bitbuf_skip(bb, 0));
  free(bb);
  free(dd);

  return rc;
}

long long gunzip_mem(char *inbuf, int inlen, char *outbuf, int outlen)
{
  struct bitbuf *bb = bitbuf_init(-1, 0);
  struct deflate *dd = init_deflate(0);

  bb->buf = inbuf;
  bb->max = bb->len = inlen;
  dd->outfd = -1;
  dd->outbuf = outbuf;
  dd->outbuflen = outlen;

  return gunzip_common(bb, dd);
}

long long gunzip_fd(int infd, int outfd)
{
  struct bitbuf *bb = bitbuf_init(infd, 4096);
  struct deflate *dd = init_deflate(0);

  dd->outfd = outfd;
  return gunzip_common(bb, dd);
}
