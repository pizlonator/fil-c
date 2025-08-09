/*  xmldecl.h - Parsing XML and text declarations.
    Copyright (c) 2008,  Robert D. Cameron.
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.

*/
#ifndef XML_DECL_H
#define XML_DECL_H

enum XML_version {XML_1_0, XML_1_1, no_XML_version_value};
/* Documents may be encoded in accord with either XML 1.0 or XML 1.1,
   or there may be no XML version declared ("no value" in the 
   XML infoset parlance). */

enum CodeUnit_Base {ASCII, EBCDIC};

/* Code units of the underlying character set may be either ASCII-compatible
   or EBCDIC-compatible.
   ASCII-compatibility means that any code units satisfy the following properties.
     (1) Any code unit whose numeric value is in the ASinclude "byteplex.h"CII range (0 to 0x7F)
         is a complete character sequence (single code unit sequence) representing
         that ASCII character.
     (2) Any code units above the ASCII range are non-ASCII code units.
         No code units or code unit sequences containing a non-ASCII code unit
         may represent an ASCII character.  (This property ensures that 
         non-ASCII code units may be ignored in making ASCII-based parsing decisions).
   EBCDIC-compatible, for the purposes of XML, means that the following property
         applies.include "byteplex.h"

     (*) Code units may form all or part of a code unit sequence representing
         a character in the Unicode range 0 to 0x9F if and only if that code
         unit has the same interpretation unde the basic EBCDIC code page cp037.
*/

enum CodeUnit_Size {SingleByte = 1, DoubleByte = 2, QuadByte = 4};
/* ASCII, EBCDIC, ISO-8859-X and UTF-8 have 8-bit code units (singlebytes);
   The UTF-16 and UCS-2 families have 16-bit code units (doublebyte);
   The UTF-32/UCS-4 family has 32-bit code units. */

enum CodeUnit_ByteOrder {BigEndian, LittleEndian, Unusual_3412, Unusual_2143};
/* The byte order of 16-bit or 32-bit code units.  The possibilities are:
   BigEndian:  UTF-16BE, UCS-2BE, UTF-16 or UCS-2 with a BigEndian byte order mark,
               UTF-16 without a byte order mark, 
               UTF-32BE/UCS-4BE, or UTF-32/UCS-4 with a BigEndian byte order mark.
   LittleEndian: UTF-16LE, UCS-2LE, UTF-16 or UCS-2 with a LittleEndian byte order mark.
                 UTF-32LE/UCS-4LE, or UTF-32/UCS-4 with a LittleEndian byte order mark.
   Unusual_3412: Unusual octet order of UTF-32/UCS-4 with byte order mark FE FF 00 00
   Unusual_2143: Unusual octet order of UTF-32/UCS-4 with byte order mark 00 00 FF FE.
*/

enum XML_standalone {Standalone_yes, Standalone_no, Standalone_no_value};
/* Possible values depending on the optional standalone component of an 
   XML declaration. */

class Entity_Info {
	
public:	
	Entity_Info();
	~Entity_Info();

	/*  Information computed by analyzing the 4-byte initial signature
            of an XML document. */
	int BOM_units; /* no of initial code units for a Byte Order Mark */

	CodeUnit_Base code_unit_base;
	CodeUnit_Size code_unit_size;
	CodeUnit_ByteOrder byte_order;	

	void AnalyzeSignature(unsigned char * signature);

	/* Information computed from the XML or text declaration. */
	XML_version version;
	bool has_encoding_decl;
	unsigned char * encoding;
	XML_standalone standalone;
	int content_start;  /* position after BOM and XML/text decl.*/
	
private:
	void set_charset_family(CodeUnit_Base C, CodeUnit_Size S, CodeUnit_ByteOrder O, int B);
};


class XML_Decl_Parser {
public:
	XML_Decl_Parser (unsigned char * bytes);
	~XML_Decl_Parser ();
	
	void ReadXMLInfo(Entity_Info & e);
	void ReadTextDeclaration(Entity_Info & e);
	// Generic version if type of external entity unknown.
	void ReadXMLorTextDecl(Entity_Info & e);
	

protected:

	unsigned char * x8data;
	int buffer_base_pos;
	int buffer_rel_pos;
	int buffer_limit_pos;
	
	void Advance(int n);
	int AbsPos() const;
	unsigned char * cur() const;
	
private:
	/* Bytespace parsing routines for internal use in ReadXMLInfo and
	   ReadTextDeclaration. */
	void DeclError();
	void Scan_WS();
	void ScanToQuote();
	void ParseVersion(Entity_Info & e);
	void ParseEncoding(Entity_Info & e);
	void ParseStandalone(Entity_Info & e);

};
#endif
