/*  bytelex.h - XML lexical recognizers for pseudo-ASCII or
    EBCDIC-family byte streams
    Copyright (c) 2008, Robert D. Cameron.
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.
*/
#ifndef BYTELEX_H
#define BYTELEX_H

#include "xmldecl.h"
#include "multiliteral.h"

template<CodeUnit_Base C, unsigned char c>
inline bool AtChar(unsigned char x8data[]) {
  return x8data[0] == Ord<C, c>::value;
}

template<CodeUnit_Base C>
inline bool AtQuote(unsigned char x8data[]) {
  return (x8data[0] == Ord<C, '"'>::value) | (x8data[0] == Ord<C, '\''>::value);
}

/* In both ASCII and EBCDIC, digits are in a contiguous range 
   from 0 through 9 */
template<CodeUnit_Base C>
inline bool at_Digit(unsigned char x8data[]) {
  return (x8data[0] >= Ord<C, '0'>::value) & (x8data[0] <= Ord<C, '9'>::value);
}

template<CodeUnit_Base C>
inline unsigned int DigitVal(unsigned char d) {
  return d - Ord<C, '0'>::value;
}

/* In both ASCII and EBCDIC, digits are in a contiguous range 
   from 0 through 9.  Similarly the hex characters A through F
   and a through f are also in contiguous ranges that differ in
   only one bit position (Ord<C, 'a'>::value ^ Ord<C, 'A'>::value).*/
template<CodeUnit_Base C>
inline bool at_HexDigit(unsigned char x8data[]) {
  const unsigned char case_bit = Ord<C, 'a'>::value ^ Ord<C, 'A'>::value;
  const unsigned char canon_A =  Ord<C, 'a'>::value | case_bit;
  const unsigned char canon_F =  Ord<C, 'f'>::value | case_bit;
  unsigned char ch = x8data[0];
  unsigned char canon_ch = ch | case_bit;
  return ((ch >= Ord<C, '0'>::value) & (ch <= Ord<C, '9'>::value)) |
         ((canon_ch >= canon_A) & (canon_ch <= canon_F));
}

template<CodeUnit_Base C>
inline unsigned int HexVal(unsigned char ch) {
  const unsigned char case_bit = Ord<C, 'a'>::value ^ Ord<C, 'A'>::value;
  const unsigned char canon_A =  Ord<C, 'a'>::value | case_bit;
  unsigned char canon_ch = ch | case_bit;
  if ((ch >= Ord<C, '0'>::value) & (ch <= Ord<C, '9'>::value)) return ch - Ord<C, '0'>::value;
  else return (ch | case_bit) - canon_A + 10;
}

// Whitespace recognition.  This varies between XML 1.0 and
// XML 1.1, but only the XML 1.0 version is needed.

template<CodeUnit_Base C>
inline bool at_WhiteSpace_10(unsigned char x8data[]) {
  unsigned char ch = x8data[0];
  return (ch == Ord<C, ' '>::value) || 
         (ch == CR<C>::value) || (ch == LF<C>::value) || (ch == HT<C>::value);
}



template<CodeUnit_Base C>
inline bool at_EndTag_Start(unsigned char x8data[]) {
  return s2int16(x8data) == c2int16<C, '<', '/'>::value;
}

template<CodeUnit_Base C>
inline bool at_Comment_Start(unsigned char x8data[]) {
  return s4int32(x8data) == c4int32<C, '<', '!', '-', '-'>::value;
}

template<CodeUnit_Base C>
inline bool at_DoubleHyphen(unsigned char x8data[]) {
  return s2int16(x8data) == c2int16<C, '-', '-'>::value;
}

template<CodeUnit_Base C>
inline bool at_Comment_End(unsigned char x8data[]) {
  return s3int32(x8data) == c3int32<C, '-', '-', '>'>::value;
}

template<CodeUnit_Base C>
inline bool at_CDATA_Start(unsigned char x8data[]) {
  return s8int64(x8data) == 
         c8int64<C, '<', '!', '[', 'C', 'D', 'A', 'T', 'A'>::value;
}

template<CodeUnit_Base C>
inline bool at_CDATA_End(unsigned char x8data[]) {
  return s3int32(x8data) == c3int32<C, ']', ']', '>'>::value;
}

template<CodeUnit_Base C>
inline bool at_PI_Start(unsigned char x8data[]) {
  return s2int16(x8data) == c2int16<C, '<', '?'>::value;
}

template<CodeUnit_Base C>
inline bool at_PI_End(unsigned char x8data[]) {
  return s2int16(x8data) == c2int16<C, '?', '>'>::value;
}

template<CodeUnit_Base C>
inline bool at_CharRef_Start(unsigned char x8data[]) {
  return s2int16(x8data) == c2int16<C, '&', '#'>::value;
}


template<CodeUnit_Base C>
inline bool at_EqualsQuote(unsigned char x8data[]) {
  uint16_t EQ = s2int16(x8data);
  return (EQ == c2int16<C, '=', '"'>::value) | (EQ == c2int16<C, '=', '\''>::value);
}

template<CodeUnit_Base C>
inline bool at_xmlns(unsigned char x8data[]) {
  return s5int64(x8data) == c5int64<C, 'x', 'm', 'l', 'n', 's'>::value; 
}

template<CodeUnit_Base C>
inline bool at_EmptyElementDelim(unsigned char x8data[]) {
  return s2int16(x8data) == c2int16<C, '/', '>'>::value;
}

template<CodeUnit_Base C>
inline bool at_XmlDecl_start(unsigned char x8data[]) {
  return (s5int64(x8data) == c5int64<C, '<', '?', 'x', 'm', 'l'>::value) &&
         at_WhiteSpace_10<C>(&x8data[5]);
}

template<CodeUnit_Base C>
inline bool at_version(unsigned char x8data[]) {
  return s7int64(x8data) == c7int64<C, 'v', 'e', 'r', 's', 'i', 'o', 'n'>::value;
}

template<CodeUnit_Base C>
inline bool at_1_0(unsigned char x8data[]) {
  return (s5int64(x8data) == c5int64<C, '"', '1', '.', '0', '"'>::value) ||
         (s5int64(x8data) == c5int64<C, '\'', '1', '.', '0', '\''>::value);
}

template<CodeUnit_Base C>
inline bool at_1_1(unsigned char x8data[]) {
  return (s5int64(x8data) == c5int64<C, '"', '1', '.', '1', '"'>::value) ||
         (s5int64(x8data) == c5int64<C, '\'', '1', '.', '1', '\''>::value);
}

template<CodeUnit_Base C>
inline bool at_encoding(unsigned char x8data[]) {
  return s8int64(x8data) == c8int64<C, 'e', 'n', 'c', 'o', 'd', 'i', 'n', 'g'>::value;
}

template<CodeUnit_Base C>
inline bool at_standalone(unsigned char x8data[]) {
  return (s8int64(x8data) == c8int64<C, 's', 't', 'a', 'n', 'd', 'a', 'l', 'o'>::value) &
         (s2int16(&x8data[8]) == c2int16<C, 'n', 'e'>::value);
}

template<CodeUnit_Base C>
inline bool at_yes(unsigned char x8data[]) {
  return (s5int64(x8data) == c5int64<C, '"', 'y', 'e', 's', '"'>::value) |
         (s5int64(x8data) == c5int64<C, '\'', 'y', 'e', 's', '\''>::value);
}

template<CodeUnit_Base C>
inline bool at_no(unsigned char x8data[]) {
  return (s4int32(x8data) == c4int32<C, '"', 'n', 'o', '"'>::value) |
         (s4int32(x8data) == c4int32<C, '\'', 'n', 'o', '\''>::value);
}

template<CodeUnit_Base C>
inline bool at_XxMmLll(unsigned char x8data[]) {
  return caseless_comp<C, 'x', 'm', 'l'>(x8data);
}

/* The at_ElementTag_Start recognizer rules out '<!', '<?', '</'
   combinations while returning true for '<' followed by any NameStrt
   character. 
*/
template<CodeUnit_Base C>
inline bool at_ElementTag_Start(unsigned char x8data[]) {
  return (x8data[0] == Ord<C, '<'>::value) & (x8data[1] != Ord<C, '!'>::value) &
         (x8data[1] != Ord<C, '?'>::value) & (x8data[1] != Ord<C, '/'>::value);
}

/* The following ugly hack optimizes for ASCII. */
template<>
inline bool at_ElementTag_Start<ASCII>(unsigned char x8data[]) {
  return (x8data[0] == Ord<ASCII, '<'>::value) &
         ((x8data[1] & 0xE1) != 0x21);
}


inline bool at_UTF_8(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '8'>(x8data);
}

inline bool at_UCS_2(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 'c', 's', '-', '2'>(x8data);
}

inline bool at_UCS_4(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 'c', 's', '-', '4'>(x8data);
}

inline bool at_UCS_2LE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 'c', 's', '-', '2', 'l', 'e'>(x8data);
}

inline bool at_UCS_2BE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 'c', 's', '-', '2', 'b', 'e'>(x8data);
}

inline bool at_UCS_4LE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 'c', 's', '-', '4', 'l', 'e'>(x8data);
}

inline bool at_UCS_4BE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 'c', 's', '-', '4', 'b', 'e'>(x8data);
}

inline bool at_UTF_16(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '1', '6'>(x8data);
}

inline bool at_UTF_32(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '3', '2'>(x8data);
}

inline bool at_UTF_16LE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '1', '6', 'l', 'e'>(x8data);
}

inline bool at_UTF_32LE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '3', '2', 'l', 'e'>(x8data);
}

inline bool at_UTF_16BE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '1', '6', 'b', 'e'>(x8data);
}

inline bool at_UTF_32BE(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'u', 't', 'f', '-', '3', '2', 'b', 'e'>(x8data);
}

inline bool at_ASCII(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'a', 's', 'c', 'i', 'i'>(x8data);
}

inline bool at_Latin1(unsigned char x8data[]) {
  return caseless_comp<ASCII, 'l', 'a', 't', 'i', 'n', '1'>(x8data);
}

inline bool at_EBCDIC(unsigned char x8data[]) {
  return caseless_comp<EBCDIC, 'e', 'b', 'c', 'd', 'i', 'c'>(x8data);
}

template<CodeUnit_Base C>
inline bool at_DOCTYPE_start(unsigned char x8data[]) {
	return s8int64(x8data) == c8int64<C, '<', '!','D', 'O', 'C', 'T', 'Y', 'P'>::value & AtChar<C,'E'>(&x8data[8]);
}

template<CodeUnit_Base C>
inline bool at_SYSTEM(unsigned char x8data[]) {
	return s6int64(x8data) == c6int64<C, 'S', 'Y', 'S', 'T', 'E', 'M'>::value;
}

template<CodeUnit_Base C>
inline bool at_PUBLIC(unsigned char x8data[]) {
	return s6int64(x8data) == c6int64<C, 'P', 'U', 'B', 'L', 'I', 'C'>::value;
}

template<CodeUnit_Base C>
inline bool at_ELEMENT(unsigned char x8data[]) {
	return s7int64(x8data) == c7int64<C, 'E', 'L', 'E', 'M', 'E', 'N', 'T'>::value;
}

template<CodeUnit_Base C>
inline bool at_ATTLIST(unsigned char x8data[]) {
	return s7int64(x8data) == c7int64<C, 'A', 'T', 'T', 'L', 'I', 'S', 'T'>::value;
}

template<CodeUnit_Base C>
inline bool at_ENTITY(unsigned char x8data[]) {
	return s6int64(x8data) == c6int64<C, 'E', 'N', 'T', 'I', 'T', 'Y'>::value;
}

template<CodeUnit_Base C>
inline bool at_NOTATION(unsigned char x8data[]) {
	return s8int64(x8data) == c8int64<C, 'N', 'O', 'T', 'A', 'T', 'I', 'O', 'N'>::value;
}

template<CodeUnit_Base C>
inline bool at_EMPTY(unsigned char x8data[]) {
	return s5int64(x8data) == c5int64<C, 'E', 'M', 'P', 'T', 'Y'>::value;
}

template<CodeUnit_Base C>
inline bool at_PCDATA(unsigned char x8data[]) {
	return s7int64(x8data) == c7int64<C, '#', 'P', 'C', 'D', 'A', 'T', 'A'>::value;
}

template<CodeUnit_Base C>
inline bool at_Para_star(unsigned char x8data[]) {
	return s2int16(x8data) == c2int16<C, ')', '*'>::value;
}

template<CodeUnit_Base C>
inline bool at_CDATA(unsigned char x8data[]) {
	return s5int64(x8data) == c5int64<C, 'C', 'D', 'A', 'T', 'A'>::value;
}

template<CodeUnit_Base C>
inline bool at_ID(unsigned char x8data[]) {
	return s2int16(x8data) == c2int16<C, 'I', 'D'>::value;
}

template<CodeUnit_Base C>
inline bool at_IDREF(unsigned char x8data[]) {
	return s5int64(x8data) == c5int64<C, 'I', 'D', 'R', 'E', 'F'>::value;
}

template<CodeUnit_Base C>
inline bool at_NDATA(unsigned char x8data[]) {
	return s5int64(x8data) == c5int64<C, 'N', 'D', 'A', 'T', 'A'>::value;
}

template<CodeUnit_Base C>
inline bool at_IDREFS(unsigned char x8data[]) {
	return s6int64(x8data) == c6int64<C, 'I', 'D', 'R', 'E', 'F', 'S'>::value;
}

template<CodeUnit_Base C>
inline bool at_ENTITIES(unsigned char x8data[]) {
	return s8int64(x8data) == c8int64<C, 'E', 'N', 'T', 'I', 'T', 'I', 'E', 'S'>::value;
}

template<CodeUnit_Base C>
inline bool at_NMTOKEN(unsigned char x8data[]) {
	return s7int64(x8data) == c7int64<C, 'N', 'M', 'T', 'O', 'K', 'E', 'N'>::value;
}

template<CodeUnit_Base C>
inline bool at_NMTOKENS(unsigned char x8data[]) {
	return s8int64(x8data) == c8int64<C, 'N', 'M', 'T', 'O', 'K', 'E', 'N', 'S'>::value;
}

template<CodeUnit_Base C>
inline bool at_REQUIRED(unsigned char x8data[]) {
	return s8int64(x8data) == c8int64<C, '#', 'R', 'E', 'Q', 'U', 'I', 'R', 'E'>::value
          & AtChar<C,'D'>(&x8data[8]);
}

template<CodeUnit_Base C>
inline bool at_IMPLIED(unsigned char x8data[]) {
	return s8int64(x8data) == c8int64<C, '#', 'I', 'M', 'P', 'L', 'I', 'E', 'D'>::value;
}

template<CodeUnit_Base C>
inline bool at_FIXED(unsigned char x8data[]) {
	return s6int64(x8data) == c6int64<C, '#', 'F', 'I', 'X', 'E', 'D'>::value;
}

template<CodeUnit_Base C>
inline bool at_ANY(unsigned char x8data[]) {
	return s3int32(x8data) == c3int32<C, 'A', 'N', 'Y'>::value;
}

template<CodeUnit_Base C>
inline bool at_INCLUDE(unsigned char x8data[]) {
	return s7int64(x8data) == c7int64<C, 'I', 'N', 'C', 'L', 'U', 'D', 'E'>::value;
}

template<CodeUnit_Base C>
inline bool at_IGNORE(unsigned char x8data[]) {
	return s6int64(x8data) == c6int64<C, 'I', 'G', 'N', 'O', 'R', 'E'>::value;
}

template<CodeUnit_Base C>
inline bool at_condSect_start(unsigned char x8data[]) {
	return s3int32(x8data) == c3int32<C, '<', '!', '['>::value;
}

template<CodeUnit_Base C>
inline bool at_xml(unsigned char x8data[]) { 
  return (s4int32(x8data) == c4int32<C, '?', 'x', 'm', 'l'>::value);
}

template<CodeUnit_Base C>
inline bool at_PubidChar(unsigned char x8data[]) {
	switch (x8data[0]) {
		case Ord<C, '0'>::value: case Ord<C, '1'>::value: 
		case Ord<C, '2'>::value: case Ord<C, '3'>::value: 
		case Ord<C, '4'>::value: case Ord<C, '5'>::value:
		case Ord<C, '6'>::value: case Ord<C, '7'>::value:
		case Ord<C, '8'>::value: case Ord<C, '9'>::value:
		case Ord<C, 'A'>::value: case Ord<C, 'a'>::value:
		case Ord<C, 'B'>::value: case Ord<C, 'b'>::value:
		case Ord<C, 'C'>::value: case Ord<C, 'c'>::value:
		case Ord<C, 'D'>::value: case Ord<C, 'd'>::value:
		case Ord<C, 'E'>::value: case Ord<C, 'e'>::value:
		case Ord<C, 'F'>::value: case Ord<C, 'f'>::value:
		case Ord<C, 'G'>::value: case Ord<C, 'g'>::value:
		case Ord<C, 'H'>::value: case Ord<C, 'h'>::value:
		case Ord<C, 'I'>::value: case Ord<C, 'i'>::value:
		case Ord<C, 'J'>::value: case Ord<C, 'j'>::value:
		case Ord<C, 'K'>::value: case Ord<C, 'k'>::value:
		case Ord<C, 'L'>::value: case Ord<C, 'l'>::value:
		case Ord<C, 'M'>::value: case Ord<C, 'm'>::value:
		case Ord<C, 'N'>::value: case Ord<C, 'n'>::value:
		case Ord<C, 'O'>::value: case Ord<C, 'o'>::value:
		case Ord<C, 'P'>::value: case Ord<C, 'p'>::value:
		case Ord<C, 'Q'>::value: case Ord<C, 'q'>::value:
		case Ord<C, 'R'>::value: case Ord<C, 'r'>::value:
		case Ord<C, 'S'>::value: case Ord<C, 's'>::value:
		case Ord<C, 'T'>::value: case Ord<C, 't'>::value:
		case Ord<C, 'U'>::value: case Ord<C, 'u'>::value:
		case Ord<C, 'V'>::value: case Ord<C, 'v'>::value:
		case Ord<C, 'W'>::value: case Ord<C, 'w'>::value:
		case Ord<C, 'X'>::value: case Ord<C, 'x'>::value:
		case Ord<C, 'Y'>::value: case Ord<C, 'y'>::value:
		case Ord<C, 'Z'>::value: case Ord<C, 'z'>::value:
		case Ord<C, '-'>::value: case Ord<C, '\''>::value:
		case Ord<C, '('>::value: case Ord<C, ')'>::value:
		case Ord<C, '+'>::value: case Ord<C, ','>::value:
		case Ord<C, '.'>::value: case Ord<C, '/'>::value:
		case Ord<C, ':'>::value: case Ord<C, '='>::value:
		case Ord<C, '?'>::value: case Ord<C, ';'>::value:
		case Ord<C, '!'>::value: case Ord<C, '*'>::value:
		case Ord<C, '#'>::value: case Ord<C, '@'>::value:
		case Ord<C, '$'>::value: case Ord<C, '_'>::value:
		case Ord<C, '%'>::value: case Ord<C, ' '>::value:
		case CR<C>::value: case LF<C>::value:
			return true;
		default: return false;
	}
}
#endif
