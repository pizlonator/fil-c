# BZip3 Format Documentation

BZip3 is a modern compression format designed for high compression ratios while maintaining
reasonable decompression speeds. It is intended to provide similar compression ratio and
performance to LZMA and BZip2; as opposed to faster Lempel-Ziv codecs that usually offer worse
compression ratio like ZStandard or LZ4.

This documentation covers the technical specifications of the BZip3 format.

## Format Characteristics

- Block level compression (no streams)
- Maximum block size ranges from 65KiB to 511MiB
- Memory usage of ~(6 x block size), both compression and decompression
- Little-endian encoding for integers
- Embedded CRC32 checksums for data integrity
- Combines LZP, RLE followed by Burrows-Wheeler transform and arithmetic coding coupled with
  a statistical predictor.

## Format Overview

BZip3 uses two main top-level formats:

1. **File Format**: The standard format used by the command-line tool
2. **Frame Format**: Used by the high-level API functions `bz3_compress` and `bz3_decompress`.

These formats are very similar: the file format is a superset of the frame format and thus also
contains a block count field.

See [bzip3_format.md](./bzip3_format.md) for more details.
