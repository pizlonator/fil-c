/*****************************************************************
** encoding.h
**
** Copyright 1998 Clark Cooper
** All rights reserved.
**
** This program is free software; you can redistribute it and/or
** modify it under the same terms as Perl itself.
*/

#ifndef ENCODING_H
#define ENCODING_H 1

#define ENCMAP_MAGIC	0xfeebface

typedef struct prefixmap {
  unsigned char	min;
  unsigned char len;			/* 0 => 256 */
  unsigned short bmap_start;
  unsigned char ispfx[32];
  unsigned char ischar[32];
} PrefixMap;

typedef struct encinf
{
  unsigned short	prefixes_size;
  unsigned short	bytemap_size;
  int			firstmap[256];
  PrefixMap		*prefixes;
  unsigned short	*bytemap;
} Encinfo;

typedef struct encmaphdr
{
  unsigned int		magic;
  char			name[40];
  unsigned short	pfsize;
  unsigned short	bmsize;
  int			map[256];
} Encmap_Header;

/*================================================================
** Structure of Encoding map binary encoding
**
** Note that all shorts and ints are in network order,
** so when packing or unpacking with perl, use 'n' and 'N' respectively.
** In C, use the htonl family of functions.
**
** The basic structure is:
**
**	_______________________
**	|Header (including map expat needs for 1st byte)
**	|PrefixMap * pfsize
**	|   This section isn't included for single-byte encodings.
**	|   For multiple byte encodings, when a byte represents a prefix
**	|   then it indexes into this vector instead of mapping to a
**	|   Unicode character. The PrefixMap type is declared above. The
**	|   ispfx and ischar fields are bitvectors indicating whether
**	|   the byte being mapped is a prefix or character respectively.
**	|   If neither is set, then the character is not mapped to Unicode.
**	|
**	|   The min field is the 1st byte mapped for this prefix; the
**	|   len field is the number of bytes mapped; and bmap_start is
**	|   the starting index of the map for this prefix in the overall
**	|   map (next section).
**	|unsigned short * bmsize
**	|   This section also is omitted for single-byte encodings.
**	|   Each short is either a Unicode scalar or an index into the
**	|   PrefixMap vector.
**
** The header for these files is declared above as the Encmap_Header type.
** The magic field is a magic number which should match the ENCMAP_MAGIC
** macro above. The next 40 bytes stores IANA registered name for the
** encoding. The pfsize field holds the number of PrefixMaps, which should
** be zero for single byte encodings. The bmsize field holds the number of
** shorts used for the overall map.
**
** The map field contains either the Unicode scalar encoded by the 1st byte
** or -n where n is the number of bytes that such a 1st byte implies (Expat
** requires that the number of bytes to encode a character is indicated by
** the 1st byte) or -1 if the byte doesn't map to any Unicode character.
**
** If the encoding is a multiple byte encoding, then there will be PrefixMap
** and character map sections. The 1st PrefixMap (index 0), covers a range
** of bytes that includes all 1st byte prefixes.
**
** Look at convert_to_unicode in Expat.xs to see how this data structure
** is used.
*/

#endif  /* ndef ENCODING_H */
