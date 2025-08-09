/*  xml_error.h - Error codes for XML parsing/validation.
    Copyright (c) 2008, Robert D. Cameron and Dan Lin.
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters, Inc., under the Academic
    Free License 3.0.

   The following enum provides codes for the well-formedness (wfErr)
   and validity (vErr) constraints of XML, based on the ids for those
   constraints in the REC-xml-20060816.xml document.  The ids may be
   recovered by stripping wfErr_ or vc_Err and substituting "-" for "_".
*/
#ifndef XML_ERROR_H
#define XML_ERROR_H

enum XML_Constraint {
		wfErr_wfc_PEinInternalSubset,
		wfErr_ExtSubset,
		wfErr_PE_between_Decls,
		wfErr_GIMatch,
		wfErr_uniqattspec,
		wfErr_NoExternalRefs,
		wfErr_CleanAttrVals,
		wfErr_wf_Legalchar,
		wfErr_wf_entdeclared,
		wfErr_textent,
		wfErr_norecursion,
		wfErr_indtd,
		vErr_vc_roottype,
		vErr_vc_PEinMarkupDecl,
		vErr_vc_check_rmd,
		vErr_elementvalid,
		vErr_ValueType,
		vErr_EDUnique,
		vErr_vc_PEinGroup,
		vErr_vc_MixedChildrenUnique,
		vErr_id,
		vErr_one_id_per_el,
		vErr_id_default,
		vErr_idref,
		vErr_entname,
		vErr_nmtok,
		vErr_notatn,
		vErr_OneNotationPer,
		vErr_NoNotationEmpty,
		vErr_NoDuplicateTokens,
		vErr_enum,
		vErr_RequiredAttr,
		vErr_defattrvalid,
		vErr_FixedAttr,
		vErr_condsec_nesting,
		vErr_vc_entdeclared,
		vErr_not_declared,
		vErr_UniqueNotationName};


/* The following enum provides codes for XML nonterminals using
   codes in the REC-xml-20060816.xml document.  The ids may be
   recovered by substituting "-" for "_".
*/

enum XML_NonTerminal {
		NT_document,
		NT_Char,
		NT_S,
		NT_NameChar,
		NT_Name,
		NT_Names,
		NT_Nmtoken,
		NT_Nmtokens,
		NT_EntityValue,
		NT_AttValue,
		NT_SystemLiteral,
		NT_PubidLiteral,
		NT_PubidChar,
		NT_CharData,
		NT_Comment,
		NT_PI,
		NT_PITarget,
		NT_CDSect,
		NT_CDStart,
		NT_CData,
		NT_CDEnd,
		NT_prolog,
		NT_XMLDecl,
		NT_VersionInfo,
		NT_Eq,
		NT_VersionNum,
		NT_Misc,
		NT_doctypedecl,
		NT_DeclSep,
		NT_intSubset,
		NT_markupdecl,
		NT_extSubset,
		NT_extSubsetDecl,
		NT_SDDecl,
		NT_element,
		NT_STag,
		NT_Attribute,
		NT_ETag,
		NT_content,
		NT_EmptyElemTag,
		NT_elementdecl,
		NT_contentspec,
		NT_children,
		NT_cp,
		NT_choice,
		NT_seq,
		NT_Mixed,
		NT_AttlistDecl,
		NT_AttDef,
		NT_AttType,
		NT_StringType,
		NT_TokenizedType,
		NT_EnumeratedType,
		NT_NotationType,
		NT_Enumeration,
		NT_DefaultDecl,
		NT_conditionalSect,
		NT_includeSect,
		NT_ignoreSect,
		NT_ignoreSectContents,
		NT_Ignore,
		NT_CharRef,
		NT_Reference,
		NT_EntityRef,
		NT_PEReference,
		NT_EntityDecl,
		NT_GEDecl,
		NT_PEDecl,
		NT_EntityDef,
		NT_PEDef,
		NT_ExternalID,
		NT_NDataDecl,
		NT_TextDecl,
		NT_extParsedEnt,
		NT_EncodingDecl,
		NT_EncName,
		NT_NotationDecl,
		NT_PublicID,
		NT_Letter,
		NT_BaseChar,
		NT_Ideographic,
		NT_CombiningChar,
		NT_Digit,
		NT_Extender};


void ShowConstraintError(XML_Constraint errCode);

void ShowSyntaxError(XML_NonTerminal errCode);

void NoEncodingError(char * msg);

void EncodingError(char * msg, unsigned char * encoding, int lgth);

void CharSetValidationError(char * encoding, int err_pos);

void XMLCharacterError(int err_pos);

void IncompleteCodeUnitError();

void DeclarationError(int pos);

void ImplementationLimitError(char * msg);

void ContentModelError();
#endif
