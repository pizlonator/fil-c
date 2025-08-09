# BZip3 Format Specification

Version 1

## Headers

The File and Frame formats share a similar structure, differing only in whether they include a
block count field.

### File Header

```
+----------------+------------------+--------------------+
| Header         | Chunk 1          | Chunk 2            |
| (9 bytes)      | (variable size)  | (variable size)    |
+----------------+------------------+--------------------+
```

This is created by the CLI tool.

### Frame Header

```
+----------------+------------------+--------------------+
| Header         | Chunk 1          | Chunk 2            |
| (13 bytes)     | (variable size)  | (variable size)    |
+----------------+------------------+--------------------+
```

This is created/read by `bz3_compress` and `bz3_decompress`.

### Header Structure

| Field          | Type   | Description                     | File Header | Frame Header |
| -------------- | ------ | ------------------------------- | ----------- | ------------ |
| Signature      | u8[5]  | Fixed "BZ3v1" ASCII string      | ✓           | ✓            |
| Max Block Size | u32_le | Maximum decompressed block size | ✓           | ✓            |
| Block Count    | u32_le | Number of blocks in the stream  | ✗           | ✓            |

### Validation Rules

1. **Signature**: Must exactly match "BZ3v1"
2. **Max Block Size**:
   - Minimum: 65KiB (66,560 bytes)
   - Maximum: 511MiB (535,822,336 bytes)
3. **Block Count** (Frame Format only):
   - Must match the actual number of blocks in the stream
   - Should be greater than 0

### Example Parser

```c
typedef struct {
    uint32_t max_block_size;
    uint32_t block_count; // Frame Format only
} bzip3_header_t;

bool read_bzip3_header(FILE* fp, bzip3_header_t* header, bool is_frame_format) {
    char signature[6] = {0};
    
    // Read signature
    if (fread(signature, 1, 5, fp) != 5)
        return false;
        
    if (strcmp(signature, "BZ3v1") != 0)
        return false;
        
    // Read max block size
    uint8_t size_bytes[4];
    if (fread(size_bytes, 1, 4, fp) != 4)
        return false;
        
    header->max_block_size = read_neutral_s32(size_bytes);
    
    if (header->max_block_size < 65536 || 
        header->max_block_size > 535822336)
        return false;
    
    // Read block count if Frame Format
    if (is_frame_format) {
        uint8_t count_bytes[4];
        if (fread(count_bytes, 1, 4, fp) != 4)
            return false;
            
        header->block_count = read_neutral_s32(count_bytes);
        
        if (header->block_count == 0)
            return false;
    }
    
    return true;
}
```

The integers in BZip3 are written unaligned, in little endian format.
A portable implementation is below.

```c
// Reading a 32-bit integer
static s32 read_neutral_s32(u8 * data) {
    return ((u32)data[0]) | 
           (((u32)data[1]) << 8) | 
           (((u32)data[2]) << 16) | 
           (((u32)data[3]) << 24);
}

// Writing a 32-bit integer
static void write_neutral_s32(u8 * data, s32 value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}
```

## Block Format

After the header, both File and Frame formats contain a sequence of blocks that follow the Block
Format specification. Each block is encapsulated in a chunk structure that defines its size.

The blocks (***without chunk header***) can be encoded/decoded using the `bz3_encode_block`
and `bz3_decode_block` APIs.

### Chunk Structure

```c
// Main block structure
struct Chunk {
    u32_le compressedSize;   // Size of compressed block
    u32_le origSize;         // Original uncompressed size
        
    if (origSize < 64) {
        SmallBlock block;
    } else {
        Block block;
    }
};
```

### Small Block Format (< 64 bytes)

For blocks smaller than 64 bytes, no compression is attempted. The data is stored with just a checksum:

```c
struct SmallBlock {
    u32_le crc32;           // CRC32 checksum
    u32_le literal;         // Always 0xFFFFFFFF for small blocks. This is basically an invalid `bwtIndex`
    u8 data[parent.compressedSize - 8]; // Uncompressed data
};
```

### Regular Block Format (≥ 64 bytes)

Larger blocks use a more complex format that supports multiple compression features:

```c
struct Block {
    u32_le crc32;            // CRC32 checksum of uncompressed data
    u32_le bwtIndex;         // Burrows-Wheeler transform index
    u8  model;            // Compression model flags
    
    if ((model & 0x02) != 0)     
        u32_le lzpSize;      // Size after LZP compression
    if ((model & 0x04) != 0)     
        u32_le rleSize;      // Size after RLE compression
        
    u8 data[parent.compressedSize - (popcnt(model) * 4 + 9)];
};
```

#### Compression Model

The `model` byte in regular blocks indicates which compression features were used:

- `0x02`: LZP (Lempel Ziv Prediction) filter
- `0x04`: RLE (Run-Length Encoding) filter

## External Resources

- [BZip3 Pattern for ImHex](https://github.com/WerWolv/ImHex-Patterns/pull/329)
