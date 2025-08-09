#ifndef ASCII_EBCDIC_H
#define ASCII_EBCDIC_H
/* ASCII_EBCDIC.h
    Copyright (c) 2008, Robert D. Cameron.
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.
    
    This is a generated file using ASCII_EBCDIC.py.  Do not edit.

*/

#ifndef _MSC_VER
#include <stdint.h>
#endif
#ifdef _MSC_VER
#include "../../lib/stdint.h"
#endif
#include "../xmldecl.h"

template<CodeUnit_Base C> struct CR;
template<> struct CR<ASCII> {static unsigned char const value = 0x0D;};
template<> struct CR<EBCDIC> {static unsigned char const value = 0x0D;};

template<CodeUnit_Base C> struct LF;
template<> struct LF<ASCII> {static unsigned char const value = 0x0A;};
template<> struct LF<EBCDIC> {static unsigned char const value = 0x25;};

template<CodeUnit_Base C> struct HT;
template<> struct HT<ASCII> {static unsigned char const value = 0x09;};
template<> struct HT<EBCDIC> {static unsigned char const value = 0x05;};

template<CodeUnit_Base C, unsigned char c> struct Ord;


template<> struct Ord<ASCII,'\0'> {static uint8_t const value = 0x0;};
template<> struct Ord<ASCII,' '> {static uint8_t const value = 0x20;};
template<> struct Ord<ASCII,'!'> {static uint8_t const value = 0x21;};
template<> struct Ord<ASCII,'"'> {static uint8_t const value = 0x22;};
template<> struct Ord<ASCII,'#'> {static uint8_t const value = 0x23;};
template<> struct Ord<ASCII,'$'> {static uint8_t const value = 0x24;};
template<> struct Ord<ASCII,'%'> {static uint8_t const value = 0x25;};
template<> struct Ord<ASCII,'&'> {static uint8_t const value = 0x26;};
template<> struct Ord<ASCII,'\''> {static uint8_t const value = 0x27;};
template<> struct Ord<ASCII,'('> {static uint8_t const value = 0x28;};
template<> struct Ord<ASCII,')'> {static uint8_t const value = 0x29;};
template<> struct Ord<ASCII,'*'> {static uint8_t const value = 0x2a;};
template<> struct Ord<ASCII,'+'> {static uint8_t const value = 0x2b;};
template<> struct Ord<ASCII,','> {static uint8_t const value = 0x2c;};
template<> struct Ord<ASCII,'-'> {static uint8_t const value = 0x2d;};
template<> struct Ord<ASCII,'.'> {static uint8_t const value = 0x2e;};
template<> struct Ord<ASCII,'/'> {static uint8_t const value = 0x2f;};
template<> struct Ord<ASCII,'0'> {static uint8_t const value = 0x30;};
template<> struct Ord<ASCII,'1'> {static uint8_t const value = 0x31;};
template<> struct Ord<ASCII,'2'> {static uint8_t const value = 0x32;};
template<> struct Ord<ASCII,'3'> {static uint8_t const value = 0x33;};
template<> struct Ord<ASCII,'4'> {static uint8_t const value = 0x34;};
template<> struct Ord<ASCII,'5'> {static uint8_t const value = 0x35;};
template<> struct Ord<ASCII,'6'> {static uint8_t const value = 0x36;};
template<> struct Ord<ASCII,'7'> {static uint8_t const value = 0x37;};
template<> struct Ord<ASCII,'8'> {static uint8_t const value = 0x38;};
template<> struct Ord<ASCII,'9'> {static uint8_t const value = 0x39;};
template<> struct Ord<ASCII,':'> {static uint8_t const value = 0x3a;};
template<> struct Ord<ASCII,';'> {static uint8_t const value = 0x3b;};
template<> struct Ord<ASCII,'<'> {static uint8_t const value = 0x3c;};
template<> struct Ord<ASCII,'='> {static uint8_t const value = 0x3d;};
template<> struct Ord<ASCII,'>'> {static uint8_t const value = 0x3e;};
template<> struct Ord<ASCII,'?'> {static uint8_t const value = 0x3f;};
template<> struct Ord<ASCII,'@'> {static uint8_t const value = 0x40;};
template<> struct Ord<ASCII,'A'> {static uint8_t const value = 0x41;};
template<> struct Ord<ASCII,'B'> {static uint8_t const value = 0x42;};
template<> struct Ord<ASCII,'C'> {static uint8_t const value = 0x43;};
template<> struct Ord<ASCII,'D'> {static uint8_t const value = 0x44;};
template<> struct Ord<ASCII,'E'> {static uint8_t const value = 0x45;};
template<> struct Ord<ASCII,'F'> {static uint8_t const value = 0x46;};
template<> struct Ord<ASCII,'G'> {static uint8_t const value = 0x47;};
template<> struct Ord<ASCII,'H'> {static uint8_t const value = 0x48;};
template<> struct Ord<ASCII,'I'> {static uint8_t const value = 0x49;};
template<> struct Ord<ASCII,'J'> {static uint8_t const value = 0x4a;};
template<> struct Ord<ASCII,'K'> {static uint8_t const value = 0x4b;};
template<> struct Ord<ASCII,'L'> {static uint8_t const value = 0x4c;};
template<> struct Ord<ASCII,'M'> {static uint8_t const value = 0x4d;};
template<> struct Ord<ASCII,'N'> {static uint8_t const value = 0x4e;};
template<> struct Ord<ASCII,'O'> {static uint8_t const value = 0x4f;};
template<> struct Ord<ASCII,'P'> {static uint8_t const value = 0x50;};
template<> struct Ord<ASCII,'Q'> {static uint8_t const value = 0x51;};
template<> struct Ord<ASCII,'R'> {static uint8_t const value = 0x52;};
template<> struct Ord<ASCII,'S'> {static uint8_t const value = 0x53;};
template<> struct Ord<ASCII,'T'> {static uint8_t const value = 0x54;};
template<> struct Ord<ASCII,'U'> {static uint8_t const value = 0x55;};
template<> struct Ord<ASCII,'V'> {static uint8_t const value = 0x56;};
template<> struct Ord<ASCII,'W'> {static uint8_t const value = 0x57;};
template<> struct Ord<ASCII,'X'> {static uint8_t const value = 0x58;};
template<> struct Ord<ASCII,'Y'> {static uint8_t const value = 0x59;};
template<> struct Ord<ASCII,'Z'> {static uint8_t const value = 0x5a;};
template<> struct Ord<ASCII,'['> {static uint8_t const value = 0x5b;};
template<> struct Ord<ASCII,'\\'> {static uint8_t const value = 0x5c;};
template<> struct Ord<ASCII,']'> {static uint8_t const value = 0x5d;};
template<> struct Ord<ASCII,'^'> {static uint8_t const value = 0x5e;};
template<> struct Ord<ASCII,'_'> {static uint8_t const value = 0x5f;};
template<> struct Ord<ASCII,'`'> {static uint8_t const value = 0x60;};
template<> struct Ord<ASCII,'a'> {static uint8_t const value = 0x61;};
template<> struct Ord<ASCII,'b'> {static uint8_t const value = 0x62;};
template<> struct Ord<ASCII,'c'> {static uint8_t const value = 0x63;};
template<> struct Ord<ASCII,'d'> {static uint8_t const value = 0x64;};
template<> struct Ord<ASCII,'e'> {static uint8_t const value = 0x65;};
template<> struct Ord<ASCII,'f'> {static uint8_t const value = 0x66;};
template<> struct Ord<ASCII,'g'> {static uint8_t const value = 0x67;};
template<> struct Ord<ASCII,'h'> {static uint8_t const value = 0x68;};
template<> struct Ord<ASCII,'i'> {static uint8_t const value = 0x69;};
template<> struct Ord<ASCII,'j'> {static uint8_t const value = 0x6a;};
template<> struct Ord<ASCII,'k'> {static uint8_t const value = 0x6b;};
template<> struct Ord<ASCII,'l'> {static uint8_t const value = 0x6c;};
template<> struct Ord<ASCII,'m'> {static uint8_t const value = 0x6d;};
template<> struct Ord<ASCII,'n'> {static uint8_t const value = 0x6e;};
template<> struct Ord<ASCII,'o'> {static uint8_t const value = 0x6f;};
template<> struct Ord<ASCII,'p'> {static uint8_t const value = 0x70;};
template<> struct Ord<ASCII,'q'> {static uint8_t const value = 0x71;};
template<> struct Ord<ASCII,'r'> {static uint8_t const value = 0x72;};
template<> struct Ord<ASCII,'s'> {static uint8_t const value = 0x73;};
template<> struct Ord<ASCII,'t'> {static uint8_t const value = 0x74;};
template<> struct Ord<ASCII,'u'> {static uint8_t const value = 0x75;};
template<> struct Ord<ASCII,'v'> {static uint8_t const value = 0x76;};
template<> struct Ord<ASCII,'w'> {static uint8_t const value = 0x77;};
template<> struct Ord<ASCII,'x'> {static uint8_t const value = 0x78;};
template<> struct Ord<ASCII,'y'> {static uint8_t const value = 0x79;};
template<> struct Ord<ASCII,'z'> {static uint8_t const value = 0x7a;};
template<> struct Ord<ASCII,'{'> {static uint8_t const value = 0x7b;};
template<> struct Ord<ASCII,'|'> {static uint8_t const value = 0x7c;};
template<> struct Ord<ASCII,'}'> {static uint8_t const value = 0x7d;};

template<> struct Ord<EBCDIC,'\0'> {static uint8_t const value = 0x0;};
template<> struct Ord<EBCDIC,' '> {static uint8_t const value = 0x40;};
template<> struct Ord<EBCDIC,'!'> {static uint8_t const value = 0x5a;};
template<> struct Ord<EBCDIC,'"'> {static uint8_t const value = 0x7f;};
template<> struct Ord<EBCDIC,'#'> {static uint8_t const value = 0x7b;};
template<> struct Ord<EBCDIC,'$'> {static uint8_t const value = 0x5b;};
template<> struct Ord<EBCDIC,'%'> {static uint8_t const value = 0x6c;};
template<> struct Ord<EBCDIC,'&'> {static uint8_t const value = 0x50;};
template<> struct Ord<EBCDIC,'\''> {static uint8_t const value = 0x7d;};
template<> struct Ord<EBCDIC,'('> {static uint8_t const value = 0x4d;};
template<> struct Ord<EBCDIC,')'> {static uint8_t const value = 0x5d;};
template<> struct Ord<EBCDIC,'*'> {static uint8_t const value = 0x5c;};
template<> struct Ord<EBCDIC,'+'> {static uint8_t const value = 0x4e;};
template<> struct Ord<EBCDIC,','> {static uint8_t const value = 0x6b;};
template<> struct Ord<EBCDIC,'-'> {static uint8_t const value = 0x60;};
template<> struct Ord<EBCDIC,'.'> {static uint8_t const value = 0x4b;};
template<> struct Ord<EBCDIC,'/'> {static uint8_t const value = 0x61;};
template<> struct Ord<EBCDIC,'0'> {static uint8_t const value = 0xf0;};
template<> struct Ord<EBCDIC,'1'> {static uint8_t const value = 0xf1;};
template<> struct Ord<EBCDIC,'2'> {static uint8_t const value = 0xf2;};
template<> struct Ord<EBCDIC,'3'> {static uint8_t const value = 0xf3;};
template<> struct Ord<EBCDIC,'4'> {static uint8_t const value = 0xf4;};
template<> struct Ord<EBCDIC,'5'> {static uint8_t const value = 0xf5;};
template<> struct Ord<EBCDIC,'6'> {static uint8_t const value = 0xf6;};
template<> struct Ord<EBCDIC,'7'> {static uint8_t const value = 0xf7;};
template<> struct Ord<EBCDIC,'8'> {static uint8_t const value = 0xf8;};
template<> struct Ord<EBCDIC,'9'> {static uint8_t const value = 0xf9;};
template<> struct Ord<EBCDIC,':'> {static uint8_t const value = 0x7a;};
template<> struct Ord<EBCDIC,';'> {static uint8_t const value = 0x5e;};
template<> struct Ord<EBCDIC,'<'> {static uint8_t const value = 0x4c;};
template<> struct Ord<EBCDIC,'='> {static uint8_t const value = 0x7e;};
template<> struct Ord<EBCDIC,'>'> {static uint8_t const value = 0x6e;};
template<> struct Ord<EBCDIC,'?'> {static uint8_t const value = 0x6f;};
template<> struct Ord<EBCDIC,'@'> {static uint8_t const value = 0x7c;};
template<> struct Ord<EBCDIC,'A'> {static uint8_t const value = 0xc1;};
template<> struct Ord<EBCDIC,'B'> {static uint8_t const value = 0xc2;};
template<> struct Ord<EBCDIC,'C'> {static uint8_t const value = 0xc3;};
template<> struct Ord<EBCDIC,'D'> {static uint8_t const value = 0xc4;};
template<> struct Ord<EBCDIC,'E'> {static uint8_t const value = 0xc5;};
template<> struct Ord<EBCDIC,'F'> {static uint8_t const value = 0xc6;};
template<> struct Ord<EBCDIC,'G'> {static uint8_t const value = 0xc7;};
template<> struct Ord<EBCDIC,'H'> {static uint8_t const value = 0xc8;};
template<> struct Ord<EBCDIC,'I'> {static uint8_t const value = 0xc9;};
template<> struct Ord<EBCDIC,'J'> {static uint8_t const value = 0xd1;};
template<> struct Ord<EBCDIC,'K'> {static uint8_t const value = 0xd2;};
template<> struct Ord<EBCDIC,'L'> {static uint8_t const value = 0xd3;};
template<> struct Ord<EBCDIC,'M'> {static uint8_t const value = 0xd4;};
template<> struct Ord<EBCDIC,'N'> {static uint8_t const value = 0xd5;};
template<> struct Ord<EBCDIC,'O'> {static uint8_t const value = 0xd6;};
template<> struct Ord<EBCDIC,'P'> {static uint8_t const value = 0xd7;};
template<> struct Ord<EBCDIC,'Q'> {static uint8_t const value = 0xd8;};
template<> struct Ord<EBCDIC,'R'> {static uint8_t const value = 0xd9;};
template<> struct Ord<EBCDIC,'S'> {static uint8_t const value = 0xe2;};
template<> struct Ord<EBCDIC,'T'> {static uint8_t const value = 0xe3;};
template<> struct Ord<EBCDIC,'U'> {static uint8_t const value = 0xe4;};
template<> struct Ord<EBCDIC,'V'> {static uint8_t const value = 0xe5;};
template<> struct Ord<EBCDIC,'W'> {static uint8_t const value = 0xe6;};
template<> struct Ord<EBCDIC,'X'> {static uint8_t const value = 0xe7;};
template<> struct Ord<EBCDIC,'Y'> {static uint8_t const value = 0xe8;};
template<> struct Ord<EBCDIC,'Z'> {static uint8_t const value = 0xe9;};
template<> struct Ord<EBCDIC,'['> {static uint8_t const value = 0xba;};
template<> struct Ord<EBCDIC,'\\'> {static uint8_t const value = 0xe0;};
template<> struct Ord<EBCDIC,']'> {static uint8_t const value = 0xbb;};
template<> struct Ord<EBCDIC,'^'> {static uint8_t const value = 0xb0;};
template<> struct Ord<EBCDIC,'_'> {static uint8_t const value = 0x6d;};
template<> struct Ord<EBCDIC,'`'> {static uint8_t const value = 0x79;};
template<> struct Ord<EBCDIC,'a'> {static uint8_t const value = 0x81;};
template<> struct Ord<EBCDIC,'b'> {static uint8_t const value = 0x82;};
template<> struct Ord<EBCDIC,'c'> {static uint8_t const value = 0x83;};
template<> struct Ord<EBCDIC,'d'> {static uint8_t const value = 0x84;};
template<> struct Ord<EBCDIC,'e'> {static uint8_t const value = 0x85;};
template<> struct Ord<EBCDIC,'f'> {static uint8_t const value = 0x86;};
template<> struct Ord<EBCDIC,'g'> {static uint8_t const value = 0x87;};
template<> struct Ord<EBCDIC,'h'> {static uint8_t const value = 0x88;};
template<> struct Ord<EBCDIC,'i'> {static uint8_t const value = 0x89;};
template<> struct Ord<EBCDIC,'j'> {static uint8_t const value = 0x91;};
template<> struct Ord<EBCDIC,'k'> {static uint8_t const value = 0x92;};
template<> struct Ord<EBCDIC,'l'> {static uint8_t const value = 0x93;};
template<> struct Ord<EBCDIC,'m'> {static uint8_t const value = 0x94;};
template<> struct Ord<EBCDIC,'n'> {static uint8_t const value = 0x95;};
template<> struct Ord<EBCDIC,'o'> {static uint8_t const value = 0x96;};
template<> struct Ord<EBCDIC,'p'> {static uint8_t const value = 0x97;};
template<> struct Ord<EBCDIC,'q'> {static uint8_t const value = 0x98;};
template<> struct Ord<EBCDIC,'r'> {static uint8_t const value = 0x99;};
template<> struct Ord<EBCDIC,'s'> {static uint8_t const value = 0xa2;};
template<> struct Ord<EBCDIC,'t'> {static uint8_t const value = 0xa3;};
template<> struct Ord<EBCDIC,'u'> {static uint8_t const value = 0xa4;};
template<> struct Ord<EBCDIC,'v'> {static uint8_t const value = 0xa5;};
template<> struct Ord<EBCDIC,'w'> {static uint8_t const value = 0xa6;};
template<> struct Ord<EBCDIC,'x'> {static uint8_t const value = 0xa7;};
template<> struct Ord<EBCDIC,'y'> {static uint8_t const value = 0xa8;};
template<> struct Ord<EBCDIC,'z'> {static uint8_t const value = 0xa9;};
template<> struct Ord<EBCDIC,'{'> {static uint8_t const value = 0xc0;};
template<> struct Ord<EBCDIC,'|'> {static uint8_t const value = 0x4f;};
template<> struct Ord<EBCDIC,'}'> {static uint8_t const value = 0xd0;};

template <unsigned char _> struct UC2lc {static unsigned char const value = _;};
template <> struct UC2lc<'A'> {static unsigned char const value = 'a';};
template <> struct UC2lc<'B'> {static unsigned char const value = 'b';};
template <> struct UC2lc<'C'> {static unsigned char const value = 'c';};
template <> struct UC2lc<'D'> {static unsigned char const value = 'd';};
template <> struct UC2lc<'E'> {static unsigned char const value = 'e';};
template <> struct UC2lc<'F'> {static unsigned char const value = 'f';};
template <> struct UC2lc<'G'> {static unsigned char const value = 'g';};
template <> struct UC2lc<'H'> {static unsigned char const value = 'h';};
template <> struct UC2lc<'I'> {static unsigned char const value = 'i';};
template <> struct UC2lc<'J'> {static unsigned char const value = 'j';};
template <> struct UC2lc<'K'> {static unsigned char const value = 'k';};
template <> struct UC2lc<'L'> {static unsigned char const value = 'l';};
template <> struct UC2lc<'M'> {static unsigned char const value = 'm';};
template <> struct UC2lc<'N'> {static unsigned char const value = 'n';};
template <> struct UC2lc<'O'> {static unsigned char const value = 'o';};
template <> struct UC2lc<'P'> {static unsigned char const value = 'p';};
template <> struct UC2lc<'Q'> {static unsigned char const value = 'q';};
template <> struct UC2lc<'R'> {static unsigned char const value = 'r';};
template <> struct UC2lc<'S'> {static unsigned char const value = 's';};
template <> struct UC2lc<'T'> {static unsigned char const value = 't';};
template <> struct UC2lc<'U'> {static unsigned char const value = 'u';};
template <> struct UC2lc<'V'> {static unsigned char const value = 'v';};
template <> struct UC2lc<'W'> {static unsigned char const value = 'w';};
template <> struct UC2lc<'X'> {static unsigned char const value = 'x';};
template <> struct UC2lc<'Y'> {static unsigned char const value = 'y';};
template <> struct UC2lc<'Z'> {static unsigned char const value = 'z';};

template <unsigned char _> struct lc2UC {static unsigned char const value = _;};
template <> struct lc2UC<'a'> {static unsigned char const value = 'A';};
template <> struct lc2UC<'b'> {static unsigned char const value = 'B';};
template <> struct lc2UC<'c'> {static unsigned char const value = 'C';};
template <> struct lc2UC<'d'> {static unsigned char const value = 'D';};
template <> struct lc2UC<'e'> {static unsigned char const value = 'E';};
template <> struct lc2UC<'f'> {static unsigned char const value = 'F';};
template <> struct lc2UC<'g'> {static unsigned char const value = 'G';};
template <> struct lc2UC<'h'> {static unsigned char const value = 'H';};
template <> struct lc2UC<'i'> {static unsigned char const value = 'I';};
template <> struct lc2UC<'j'> {static unsigned char const value = 'J';};
template <> struct lc2UC<'k'> {static unsigned char const value = 'K';};
template <> struct lc2UC<'l'> {static unsigned char const value = 'L';};
template <> struct lc2UC<'m'> {static unsigned char const value = 'M';};
template <> struct lc2UC<'n'> {static unsigned char const value = 'N';};
template <> struct lc2UC<'o'> {static unsigned char const value = 'O';};
template <> struct lc2UC<'p'> {static unsigned char const value = 'P';};
template <> struct lc2UC<'q'> {static unsigned char const value = 'Q';};
template <> struct lc2UC<'r'> {static unsigned char const value = 'R';};
template <> struct lc2UC<'s'> {static unsigned char const value = 'S';};
template <> struct lc2UC<'t'> {static unsigned char const value = 'T';};
template <> struct lc2UC<'u'> {static unsigned char const value = 'U';};
template <> struct lc2UC<'v'> {static unsigned char const value = 'V';};
template <> struct lc2UC<'w'> {static unsigned char const value = 'W';};
template <> struct lc2UC<'x'> {static unsigned char const value = 'X';};
template <> struct lc2UC<'y'> {static unsigned char const value = 'Y';};
template <> struct lc2UC<'z'> {static unsigned char const value = 'Z';};

#endif
