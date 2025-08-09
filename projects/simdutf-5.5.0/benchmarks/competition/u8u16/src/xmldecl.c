/*  xmldecl.c - Parsing XML and Text Declarations.
    Copyright (c) 2008, Robert D. Cameron.
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "xmldecl.h"
#include "xml_error.h"
#include "multiliteral.h"
#include "bytelex.h"


Entity_Info::Entity_Info() {
	encoding = NULL;
}
Entity_Info::~Entity_Info() {
	delete [] encoding;
}

/* Signature-based character set family detection in accord with
   Appendix F of the XML 1.0 and 1.1 specifications. */

/* These definitions use b2int16 to determine appropriate doublebyte
   values based on endianness of the underlying architecture. */
static const int x0000 = b2int16<0x00, 0x00>::value;
static const int xFEFF = b2int16<0xFE, 0xFF>::value;
static const int xFFFE = b2int16<0xFF, 0xFE>::value;
static const int x003C = b2int16<0x00, 0x3C>::value;
static const int x3C00 = b2int16<0x3C, 0x00>::value;
static const int x4C6F = b2int16<0x4C, 0x6F>::value;
static const int xA794 = b2int16<0xA7, 0x94>::value;
static const int xEFBE = b2int16<0xEF, 0xBE>::value;

void Entity_Info::AnalyzeSignature(unsigned char * signature) {
 	uint16_t * XML_dbl_byte = (uint16_t *) signature;
	switch (XML_dbl_byte[0]) {
		case x0000:
			switch (XML_dbl_byte[1]) {
				case xFEFF: set_charset_family(ASCII, QuadByte, BigEndian, 1);break;
				case xFFFE: set_charset_family(ASCII, QuadByte, Unusual_2143, 1);break;
				case x3C00: set_charset_family(ASCII, QuadByte, Unusual_2143, 0);break;
				default: set_charset_family(ASCII, QuadByte, BigEndian, 0);
			}
			break;
		case xFEFF:
			if (XML_dbl_byte[1] == x0000)
				set_charset_family(ASCII, QuadByte, Unusual_3412, 1);
			else set_charset_family(ASCII, DoubleByte, BigEndian, 1);
			break;
		case xFFFE:
			if (XML_dbl_byte[1] == x0000)
				set_charset_family(ASCII, QuadByte, LittleEndian, 1);
			else set_charset_family(ASCII, DoubleByte, LittleEndian, 1);
			break;
		case x003C:
			if (XML_dbl_byte[1] == x0000)
				set_charset_family(ASCII, QuadByte, Unusual_3412, 0);
			else set_charset_family(ASCII, DoubleByte, BigEndian, 0);
			break;
		case x3C00:
			if (XML_dbl_byte[1] == x0000)
				set_charset_family(ASCII, QuadByte, LittleEndian, 0);
			else set_charset_family(ASCII, DoubleByte, LittleEndian, 0);
			break;
		case x4C6F:
			if (XML_dbl_byte[1] == xA794)
				set_charset_family(EBCDIC, SingleByte, BigEndian, 0);
			else set_charset_family(ASCII, SingleByte, BigEndian, 0);
			break;
		case xEFBE:
			if (signature[2] == 0xBF)
				set_charset_family(ASCII, SingleByte, BigEndian, 3);
			else set_charset_family(ASCII, SingleByte, BigEndian, 0);
			break;
		default:
			set_charset_family(ASCII, SingleByte, BigEndian, 0);
	}
}
void Entity_Info::set_charset_family(CodeUnit_Base C, CodeUnit_Size S, CodeUnit_ByteOrder O, int B){
 		code_unit_base = C;
 		code_unit_size = S;
 		byte_order = O;
 		BOM_units = B;
 }


XML_Decl_Parser::XML_Decl_Parser(unsigned char * bytes){
	buffer_base_pos = 0;
	x8data = bytes;
}

XML_Decl_Parser::~XML_Decl_Parser(){
}

inline void XML_Decl_Parser::DeclError() {
	DeclarationError(AbsPos());
}

inline int XML_Decl_Parser::AbsPos() const {
	return 	buffer_base_pos + buffer_rel_pos;
}

inline unsigned char * XML_Decl_Parser::cur() const {
	return &x8data[buffer_rel_pos];
}

inline void XML_Decl_Parser::Advance(int n) {
	buffer_rel_pos += n;
}

inline void XML_Decl_Parser::Scan_WS() {
	while (at_WhiteSpace_10<ASCII>(cur())) Advance(1);
}

inline void XML_Decl_Parser::ScanToQuote() {
	while (!AtQuote<ASCII>(cur())) buffer_rel_pos+=1;
}

inline void XML_Decl_Parser::ParseVersion(Entity_Info & e) {
	/* Skip "version" */
	Advance(7);
	Scan_WS();
	if (!AtChar<ASCII,'='>(cur())) DeclError();
	Advance(1);
	Scan_WS();
	if (at_1_0<ASCII>(cur())) e.version = XML_1_0;
	else if (at_1_1<ASCII>(cur())) e.version = XML_1_1;
	else DeclError();
	Advance(5);
}

inline void XML_Decl_Parser::ParseEncoding(Entity_Info & e) {
	/* Skip "encoding" */
	Advance(8);
	e.has_encoding_decl = true;
	Scan_WS();
	if (!AtChar<ASCII,'='>(cur())) DeclError();
	Advance(1);
	Scan_WS();
	if (AtQuote<ASCII>(cur())) {
		unsigned char quoteCh = cur()[0];
		Advance(1);
		int start_pos = AbsPos();
		ScanToQuote();
		if (cur()[0] != quoteCh) DeclError();
		int lgth = AbsPos() - start_pos;
		e.encoding = new unsigned char[lgth + 1];
		memcpy(e.encoding, &x8data[start_pos-buffer_base_pos], lgth);
		e.encoding[lgth] = '\0';
	}
	else DeclError();
	Advance(1);
}

inline void XML_Decl_Parser::ParseStandalone(Entity_Info & e) {
	/* Skip "standalone" */
	Advance(10);
	Scan_WS();
	if (!AtChar<ASCII,'='>(cur())) DeclError();
	Advance(1);
	Scan_WS();
	if (at_yes<ASCII>(cur())) {Advance(5); e.standalone = Standalone_yes;}
	else if (at_no<ASCII>(cur())) {Advance(4); e.standalone = Standalone_no;}
	else DeclError();
}

void XML_Decl_Parser::ReadXMLInfo(Entity_Info & e) {
	e.version = no_XML_version_value;
	e.has_encoding_decl = false;
	e.standalone = Standalone_no_value;
	buffer_rel_pos = e.BOM_units;
	// It is possible that there is no XML declaration.
	if (!at_XmlDecl_start<ASCII>(cur())) {
		e.content_start = AbsPos();
		return;
	}
	// Otherwise, the XML declaration exists and must have
	// at least version information.
	Advance(6);
	Scan_WS();
	if (!at_version<ASCII>(cur())) DeclError();
	ParseVersion(e);
	if (at_PI_End<ASCII>(cur())) {
		e.content_start = AbsPos()+2;
		return;
	}
	if (!at_WhiteSpace_10<ASCII>(cur())) DeclError();
	Scan_WS();
	if (at_encoding<ASCII>(cur())) {
		ParseEncoding(e);
		if (at_PI_End<ASCII>(cur())) {
			e.content_start = AbsPos()+2;
			return;
		}
		if (!at_WhiteSpace_10<ASCII>(cur())) DeclError();
		Scan_WS();
	}
	if (at_standalone<ASCII>(cur())) {
		ParseStandalone(e);
		Scan_WS();
	}
	if (!at_PI_End<ASCII>(cur())) DeclError();
	e.content_start = AbsPos()+2;
}

// Similar to reading the XML_declaration of the document entity,
// ReadTextDeclaration reads the text declaration of an external
// parsed entity.

void XML_Decl_Parser::ReadTextDeclaration(Entity_Info & e) {
	e.version = no_XML_version_value;
	e.has_encoding_decl = false;
	e.standalone = Standalone_no_value;
	buffer_rel_pos = e.BOM_units;
	// It is possible that there is no text declaration.
	if (!at_XmlDecl_start<ASCII>(cur())) {
		e.content_start = AbsPos();
		return;
	}
	// Otherwise, the text declaration exists and may have
	// version information.
	Advance(6);
	Scan_WS();
	if (at_version<ASCII>(cur())) {
		ParseVersion(e);
		// Must have whitespace character before encoding declaration.
		if (!at_WhiteSpace_10<ASCII>(cur())) DeclError();
		Scan_WS();
	}
	if (!at_encoding<ASCII>(cur())) DeclError();
	ParseEncoding(e);
	Scan_WS();
	if (!at_PI_End<ASCII>(cur())) DeclError();
	e.content_start = AbsPos()+2;
}

void XML_Decl_Parser::ReadXMLorTextDecl(Entity_Info & e) {
	e.version = no_XML_version_value;
	e.has_encoding_decl = false;
	e.standalone = Standalone_no_value;
	buffer_rel_pos = e.BOM_units;
	// It is possible that there is no XML or text declaration.
	if (!at_XmlDecl_start<ASCII>(cur())) {
		e.content_start = AbsPos();
		return;
	}
	// Otherwise, the XML or text declaration exists and may have
	// version information.
	Advance(6);
	Scan_WS();
	if (at_version<ASCII>(cur())) {
		ParseVersion(e);
		if (at_PI_End<ASCII>(cur())) {
			e.content_start = AbsPos()+2;
			return;
		}
		if (!at_WhiteSpace_10<ASCII>(cur())) DeclError();
		Scan_WS();
		if (at_encoding<ASCII>(cur())) {
			ParseEncoding(e);
			if (at_PI_End<ASCII>(cur())) {
				e.content_start = AbsPos()+2;
				return;
			}
			if (!at_WhiteSpace_10<ASCII>(cur())) DeclError();
			Scan_WS();
		}
		if (at_standalone<ASCII>(cur())) {
			ParseStandalone(e);
			Scan_WS();
		}
	}
	else {	// Without version, we can only have a text declaration,
		// in which case an encoding spec is required.
		if (!at_encoding<ASCII>(cur())) DeclError();
		ParseEncoding(e);
		Scan_WS();
		// No standalone spec is allowed in a text declaration.
	}
	if (!at_PI_End<ASCII>(cur())) DeclError();	
	e.content_start = AbsPos()+2;
}
