/*  xml_error.c - Error reporting for XML parsing/validation.
    Copyright (c) 2008, Robert D. Cameron and Dan Lin.
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.
*/

#include "xml_error.h"
char * XML_Constraint_Strings[] = {
		"wfc-PEinInternalSubset",
		"ExtSubset",
		"PE-between-Decls",
		"GIMatch",
		"uniqattspec",
		"NoExternalRefs",
		"CleanAttrVals",
		"wf-Legalchar",
		"wf-entdeclared",
		"textent",
		"norecursion",
		"indtd",
		"vc-roottype",
		"vc-PEinMarkupDecl",
		"vc-check-rmd",
		"elementvalid",
		"ValueType",
		"EDUnique",
		"vc-PEinGroup",
		"vc-MixedChildrenUnique",
		"id",
		"one-id-per-el",
		"id-default",
		"idref",
		"entname",
		"nmtok",
		"notatn",
		"OneNotationPer",
		"NoNotationEmpty",
		"NoDuplicateTokens",
		"enum",
		"RequiredAttr",
		"defattrvalid",
		"FixedAttr",
		"condsec-nesting",
		"vc-entdeclared",
		"not-declared",
		"UniqueNotationName"};

char * XML_NonTerminal_Names[] = {
		"document  [1]",
		"Char  [2]",
		"S  [3]",
		"NameChar  [4]",
		"Name  [5]",
		"Names  [6]",
		"Nmtoken  [7]",
		"Nmtokens  [8]",
		"EntityValue  [9]",
		"AttValue  [10]",
		"SystemLiteral  [11]",
		"PubidLiteral  [12]",
		"PubidChar  [13]",
		"CharData  [14]",
		"Comment  [15]",
		"PI  [16]",
		"PITarget  [17]",
		"CDSect  [18]",
		"CDStart  [19]",
		"CData  [20]",
		"CDEnd  [21]",
		"prolog  [22]",
		"XMLDecl  [23]",
		"VersionInfo  [24]",
		"Eq  [25]",
		"VersionNum  [26]",
		"Misc  [27]",
		"doctypedecl  [28]",
		"DeclSep  [28a]",
		"intSubset  [28b]",
		"markupdecl  [29]",
		"extSubset  [30]",
		"extSubsetDecl  [31]",
		"SDDecl  [32]",
		"element  [39]",
		"STag  [40]",
		"Attribute  [41]",
		"ETag  [42]",
		"content  [43]",
		"EmptyElemTag  [44]",
		"elementdecl  [45]",
		"contentspec  [46]",
		"children  [47]",
		"cp  [48]",
		"choice  [49]",
		"seq  [50]",
		"Mixed  [51]",
		"AttlistDecl  [52]",
		"AttDef  [53]",
		"AttType  [54]",
		"StringType  [55]",
		"TokenizedType  [56]",
		"EnumeratedType  [57]",
		"NotationType  [58]",
		"Enumeration  [59]",
		"DefaultDecl  [60]",
		"conditionalSect  [61]",
		"includeSect  [62]",
		"ignoreSect  [63]",
		"ignoreSectContents  [64]",
		"Ignore  [65]",
		"CharRef  [66]",
		"Reference  [67]",
		"EntityRef  [68]",
		"PEReference  [69]",
		"EntityDecl  [70]",
		"GEDecl  [71]",
		"PEDecl  [72]",
		"EntityDef  [73]",
		"PEDef  [74]",
		"ExternalID  [75]",
		"NDataDecl  [76]",
		"TextDecl  [77]",
		"extParsedEnt  [78]",
		"EncodingDecl  [80]",
		"EncName  [81]",
		"NotationDecl  [82]",
		"PublicID  [83]",
		"Letter  [84]",
		"BaseChar  [85]",
		"Ideographic  [86]",
		"CombiningChar  [87]",
		"Digit  [88]",
		"Extender  [89]"};


void ShowConstraintError(XML_Constraint errCode) {
	if (errCode < vErr_vc_roottype) {
		fprintf(stderr, "Violation of well-formedness constraint: %s\n", XML_Constraint_Strings[errCode]);
		exit(-1);
	}
	else {
		fprintf(stderr, "Violation of validity constraint: %s\n", XML_Constraint_Strings[errCode]);
		exit(-1);
	}
}

void ShowSyntaxError(XML_NonTerminal errCode) {
	fprintf(stderr, "Syntax error in production: %s\n", XML_NonTerminal_Names[errCode]);
}


void NoEncodingError(char * msg) {
	fprintf(stderr, "Error : %s\n", msg);
	exit(-1);
}

void EncodingError(char * msg, unsigned char * encoding, int lgth) {
	fprintf(stderr, "Error : Illegal/unsupported %s encoding of length %i: \"", msg, lgth);
	for (int i = 0; i < lgth; i++) fprintf(stderr, "%c", encoding[i]);
	fprintf(stderr, "\"\n"); 
	exit(-1);
}

void CharSetValidationError(char * encoding, int err_pos) {
	fprintf(stderr, "Error: Invalid %s character in input stream at position %i\n", encoding, err_pos);
	exit(-1);
}

void XMLCharacterError(int err_pos) {
	fprintf(stderr, "Illegal control character in XML input stream at position %i\n", err_pos);
	exit(-1);
}

void IncompleteCodeUnitError() {
	fprintf(stderr, "Error: Incomplete code unit at end of file.\n");
	exit(-1);
}

void DeclarationError(int pos) {
	fprintf(stderr, "Parsing error at position %i in XML or Text declaration.\n", pos);
	exit(-1);
}

void ImplementationLimitError(char * msg) {
	fprintf(stderr, "Fatal implementation limit - %s\n", msg);
	exit(-1);
}

void ContentModelError() {
	fprintf(stderr, "Error: nondeterminism in content model.\n");
	exit(-1);
}

