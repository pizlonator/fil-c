/*
 * Copyright (C) 2002-2022 Free Software Foundation, Inc.
 *
 * This file is part of LIBTASN1.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


/*****************************************************/
/* File: Test_tree.c                                 */
/* Description: Test sequences for these functions:  */
/*     asn1_visit_tree,                              */
/*     asn1_create_element,                          */
/*     asn1_delete_structure2,                       */
/*     asn1_delete_structure,                        */
/*     asn1_write_value,                             */
/*     asn1_read_value,                              */
/*****************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ASN1_INTERNAL_BUILD
#include "libtasn1.h"

#include "Test_tree_asn1_tab.c"

#define ACT_NULL       0
#define ACT_CREATE     1
#define ACT_DELETE     2
#define ACT_WRITE      3
#define ACT_READ       4
#define ACT_VISIT      5
#define ACT_ENCODING   6
#define ACT_DECODING   7
#define ACT_PRINT_DER  8
#define ACT_EXPAND_ANY 9
#define ACT_DECODING_ELEMENT   10
#define ACT_EXPAND_OCTET       11
#define ACT_NUMBER_OF_ELEMENTS 12
#define ACT_DECODING_START_END 13
#define ACT_READ_DEFINITIONS   14
#define ACT_READ_TAG_CLASS     15
#define ACT_OID_2_STRUCTURE    16
#define ACT_READ_LENGTH        17
#define ACT_ENCODING_LENGTH    18
#define ACT_READ_BIT           19
#define ACT_SET_DER            20
#define ACT_DELETE_ELEMENT     21
#define ACT_DELETE2	22


typedef struct
{
  int action;
  const char *par1;
  const char *par2;
  int par3;
  int errorNumber;
  int line;
} test_type;


test_type test_array[] = {

  {ACT_DELETE, "", "", 0, ASN1_ELEMENT_NOT_FOUND, __LINE__},

  /* Test: APPLICATION 30 */
  {ACT_CREATE, "TEST_TREE.KrbError", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "pvno", "5", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 5, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 4, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 5, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.KrbError", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE_ELEMENT, "pvno", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE_ELEMENT, "pvno", "", 0, ASN1_ELEMENT_NOT_FOUND, __LINE__},
  {ACT_DELETE2, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: CHOICE */
  {ACT_CREATE, "TEST_TREE.CertTemplate", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "version", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "validity", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer", "rdnSequence", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "2.5.4.3", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value",
   "\x0c\x18\x71\x75\x61\x73\x61\x72\x2e\x6c\x61\x73\x2e\x69\x63\x2e\x75\x6e\x69\x63\x61\x6d\x70\x2e\x62\x72",
   26, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "2.5.4.7", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value",
   "\x0c\x08\x43\x61\x6d\x70\x69\x6e\x61\x73", 10, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "2.5.4.6", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value",
   "\x13\x06\x42\x72\x61\x73\x69\x6c", 8, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "2.5.4.10", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value", "\x0c\x02\x49\x43", 4,
   ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "2.5.4.11", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value", "\x0c\x03\x4c\x41\x53", 5,
   ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "2.5.4.8", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value",
   "\x0c\x09\x53\x61\x6f\x20\x50\x61\x75\x6c\x6f", 11, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "issuer.rdnSequence", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.type", "1.2.840.113549.1.9.1", 0,
   ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "issuer.rdnSequence.?LAST.value",
   "\x16\x19\x65\x64\x75\x61\x72\x64\x6f\x40\x6c\x61\x73\x2e\x69\x63\x2e\x75\x6e\x69\x63\x61\x6d\x70\x2e\x62\x72",
   27, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 152, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 151, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 152, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.CertTemplate", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE_ELEMENT, "issuer.rdnSequence.?1", "", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_DELETE_ELEMENT, "issuer.rdnSequence.?1", "", 0,
   ASN1_ELEMENT_NOT_FOUND, __LINE__},
  {ACT_DELETE_ELEMENT, "issuer.rdnSequence.?3", "", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_DELETE_ELEMENT, "issuer.rdnSequence.?5", "", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_DELETE_ELEMENT, "issuer.rdnSequence.?7", "", 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: Empty sequnces */
  {ACT_CREATE, "TEST_TREE.sequenceEmpty", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int1", "1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "seq1.int", NULL, 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 11, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 10, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 11, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.sequenceEmpty", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "seq1", "START", 5, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "seq1", "END", 6, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "set1", "START", 7, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "set1", "END", 10, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: Indefinite Length */
  {ACT_CREATE, "TEST_TREE.IndefiniteLengthTest", 0, 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_WRITE, "int1", "1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "seq1.int", "2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set1", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set1.?LAST", "1.2.3.4", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set1", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set1.?LAST", "1.2.5.6", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 255, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_SET_DER,
   "\x30\x18\xa1\x80\x02\x01\x02\x00\x00\x31\x80\x06\x03\x2a\x03\x04\x06\x03\x2a\x05\x06\x00\x00\x02\x01\x01",
   0, 26, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.IndefiniteLengthTest", 0, 0, ASN1_SUCCESS,
   __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "seq1", "START", 2, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "seq1", "END", 8, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "set1", "START", 9, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "set1", "END", 22, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: OID */
  {ACT_CREATE, "TEST_TREE.OidTest", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ_LENGTH, "oid", NULL, 9, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ, "oid", "2.5.29.2", 9, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oid", "1.2.3.4", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oid2", "2.5.29.1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oid3", "1.2.29.2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 7, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 6, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 7, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.OidTest", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "oid", "START", 2, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "oid", "END", 6, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "", "START", 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "", "END", 6, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "oid2", "2.5.29.1", 9, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "oid3", "1.2.29.2", 9, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oid", "1.2.3.4", 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: ENCODING_LENGTH and READ_LEN */
  {ACT_CREATE, "TEST_TREE.OidAndTimeTest", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oid", "1.2.3.4", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "time1", "9801011200Z", 1, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "time2", "2001010112001.12-0700", 1, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oct", "ABCD", 4, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "bol", "TRUE", 1, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "enum", "2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "bit", "1\xC0", 10, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "any", "\x30\x01\x01", 3, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set.?LAST", "10", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set", "NEW", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "set.?LAST", "1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "gen", "Prova", 5, ASN1_SUCCESS, __LINE__},
  {ACT_READ_LENGTH, "oid", NULL, 8, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "time1", NULL, 12, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "time2", NULL, 22, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "oct", NULL, 4, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "bol", NULL, 5, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "enum", NULL, 1, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "any", NULL, 3, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "gen", NULL, 5, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_LENGTH, "bit", NULL, 10, ASN1_MEM_ERROR, __LINE__},
  {ACT_READ_BIT, "bit", "1\xC0", 10, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 79, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 78, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 79, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.OidAndTimeTest", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: OID to STRUCTURE */
  {ACT_OID_2_STRUCTURE, "2.5.29.3", "", 0, ASN1_ELEMENT_NOT_FOUND, __LINE__},
  {ACT_OID_2_STRUCTURE, "1.2.29.2", "", 0, ASN1_ELEMENT_NOT_FOUND, __LINE__},
  {ACT_OID_2_STRUCTURE, "2.5.29.2", "anyTest2", 0, ASN1_SUCCESS, __LINE__},

  /* Test: READ TAG and CLASS */
  {ACT_CREATE, "TEST_TREE.SequenceTestTag", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "int", "", 0, ASN1_ELEMENT_NOT_FOUND, __LINE__},
  {ACT_READ_TAG_CLASS, "int1", "TAG", ASN1_TAG_INTEGER, ASN1_SUCCESS,
   __LINE__},
  {ACT_READ_TAG_CLASS, "int1", "CLASS", ASN1_CLASS_UNIVERSAL, ASN1_SUCCESS,
   __LINE__},
  {ACT_READ_TAG_CLASS, "int2", "TAG", 3, ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "int2", "CLASS", ASN1_CLASS_CONTEXT_SPECIFIC,
   ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "str1", "TAG", 1, ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "str1", "CLASS", ASN1_CLASS_CONTEXT_SPECIFIC,
   ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "str2", "TAG", 28, ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "str2", "CLASS", ASN1_CLASS_UNIVERSAL, ASN1_SUCCESS,
   __LINE__},
  {ACT_READ_TAG_CLASS, "str3", "TAG", 28, ASN1_SUCCESS, __LINE__},
  {ACT_READ_TAG_CLASS, "str3", "CLASS", ASN1_CLASS_UNIVERSAL, ASN1_SUCCESS,
   __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: OBJECT IDENTIFIER  elements */
  {ACT_CREATE, "TEST_TREE.Sequence1", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int1", "v2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "int1", "\x01", 1, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int2", "0", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oct", "\x02\x01\x0a", 3, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "id", "1 2 3 4 5", 0, ASN1_VALUE_NOT_VALID, __LINE__},
  {ACT_WRITE, "id", "2.5.29.2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "id", "2.5.29.2", 9, ASN1_SUCCESS, __LINE__},
  {ACT_READ_LENGTH, "id", NULL, 9, ASN1_MEM_ERROR, __LINE__},
  {ACT_WRITE, "any1", "\x02\x01\x05", 3, ASN1_SUCCESS, __LINE__},
  {ACT_READ_DEFINITIONS, "TEST_TREE.id-anyTest", "2.5.29.1", 9, ASN1_SUCCESS,
   __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 20, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 19, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 200, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.Sequence1", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "id", "START", 7, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "id", "2.5.29.2", 9, ASN1_SUCCESS, __LINE__},
  {ACT_EXPAND_ANY, "", NULL, 0, ASN1_SUCCESS, __LINE__},
  {ACT_EXPAND_OCTET, "oct", "id", 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: CHOICE elements */
  {ACT_CREATE, "TEST_TREE.X520LocalityName", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "", "teletexString", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "teletexString", "PROVA", 5, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 7, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 6, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 7, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.X520LocalityName", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: OPTIONAL elements */
  {ACT_CREATE, "TEST_TREE.DHParameter", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "prime", "1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "base", "2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "privateValueLength", NULL, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 8, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 7, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 8, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.DHParameter", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: Integer */
  {ACT_CREATE, "TEST_TREE.AnyTest2", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "", "int", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "0", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 3, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 2, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 3, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "-1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "2000000000", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "-2000000000", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "-20000000000", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int", "20000000000", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test: elements without names */
  {ACT_CREATE, "TEST_TREE.Sequence1", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "int2", "10", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "seq", "NEW", 1, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "seq.?LAST", "1", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "seq", "NEW", 1, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "seq.?LAST", "2", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "any1", NULL, 0, ASN1_SUCCESS, __LINE__},
  {ACT_NUMBER_OF_ELEMENTS, "seq", "", 2, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "id", "1.2.3.4", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "oct", "\x30\x03\x02\x01\x15", 5, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "int2", 0, 1024, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING, "seq.?2", 0, 2, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "seq.?2", 0, 3, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 25, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 24, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 25, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.Sequence1", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_ELEMENT, "int2", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "int2", "\x0a", 1, ASN1_SUCCESS, __LINE__},
  {ACT_READ_LENGTH, "int2", NULL, 1, ASN1_MEM_ERROR, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.Sequence1", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_START_END, "seq.?2", "START", 10, ASN1_SUCCESS, __LINE__},
  {ACT_EXPAND_OCTET, "oct", "id", 0, ASN1_SUCCESS, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Test GeneralString */
  {ACT_CREATE, "TEST_TREE.Test3", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "a", "1234", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "b", "prova", 5, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 17, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 16, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 17, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.Test3", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_ELEMENT, "b", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "b", "prova", 5, ASN1_SUCCESS, __LINE__},
  {ACT_READ_LENGTH, "b", NULL, 5, ASN1_MEM_ERROR, __LINE__},
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* Bug regression test: Empty write on 64-bit platforms */
  {ACT_CREATE, "TEST_TREE.Checksum", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "checksum", "foo", 0, ASN1_SUCCESS, __LINE__},
  {ACT_WRITE, "checksum", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_ENCODING_LENGTH, "", 0, 4, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 3, ASN1_MEM_ERROR, __LINE__},
  {ACT_ENCODING, "", 0, 4, ASN1_SUCCESS, __LINE__},
  {ACT_PRINT_DER, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_CREATE, "TEST_TREE.Checksum", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING, 0, 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_DECODING_ELEMENT, "checksum", 0, 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ, "checksum", "", 0, ASN1_SUCCESS, __LINE__},
  {ACT_READ_LENGTH, "checksum", NULL, 0, ASN1_SUCCESS, __LINE__},	/* MEM_FAILURE? */
  {ACT_VISIT, "", "", ASN1_PRINT_ALL, ASN1_SUCCESS, __LINE__},
  {ACT_DELETE, "", "", 0, ASN1_SUCCESS, __LINE__},

  /* end */
  {ACT_NULL}

};


#define  error() fprintf(stderr, "Error at line %d\n", __LINE__); exit(1)

int
main (int argc, char *argv[])
{
  int result;
  asn1_node definitions = NULL;
  asn1_node asn1_element = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  FILE *out;
  test_type *test;
  int errorCounter = 0, testCounter = 0, der_len;
  unsigned char value[1024], der[1024];
  int valueLen, tag = 0, class = 0;
  int k;
  int start, end, verbose = 0;
  const char *str_p = NULL;
  const char *treefile = getenv ("ASN1TREE");

  if (argc > 1)
    verbose = 1;

  if (!treefile)
    treefile = "Test_tree.asn";

  if (verbose != 0)
    {
      printf ("\n\n/****************************************/\n");
      printf ("/*     Test sequence : Test_tree        */\n");
      printf ("/****************************************/\n\n");
      printf ("ASN1TREE: %s\n", treefile);
    }

  /* Check version */
  if (asn1_check_version ("0.2.11") == NULL)
    printf ("\nLibrary version check ERROR:\n actual version: %s\n\n",
	    asn1_check_version (NULL));

  if (1)
    result = asn1_parser2tree (treefile, &definitions, errorDescription);
  else
    result =
      asn1_array2tree (Test_tree_asn1_tab, &definitions, errorDescription);

  if (result != ASN1_SUCCESS)
    {
      asn1_perror (result);
      printf ("ErrorDescription = %s\n\n", errorDescription);
      exit (1);
    }

  if (1)
    out = stdout;
  else
    out = fopen ("Test_tree.out", "w");

  if (out == NULL)
    {
      printf ("Output file ERROR\n");
      exit (1);
    }

  if (0)
    {
      asn1_print_structure (out, definitions, "TEST_TREE", ASN1_PRINT_ALL);
      fprintf (out, "\n");
    }

  test = test_array;

  while (test->action != ACT_NULL)
    {
      testCounter++;

      switch (test->action)
	{
	case ACT_CREATE:
	  result =
	    asn1_create_element (definitions, test->par1, &asn1_element);
	  break;
	case ACT_DELETE:
	  result = asn1_delete_structure (&asn1_element);
	  break;
	case ACT_DELETE2:
	  result =
	    asn1_delete_structure2 (&asn1_element, ASN1_DELETE_FLAG_ZEROIZE);
	  break;
	case ACT_DELETE_ELEMENT:
	  result = asn1_delete_element (asn1_element, test->par1);
	  break;
	case ACT_WRITE:
	  if ((test->par2) && (!strcmp ("DER", test->par2)))
	    result =
	      asn1_write_value (asn1_element, test->par1, der, der_len);
	  else
	    result =
	      asn1_write_value (asn1_element, test->par1, test->par2,
				test->par3);
	  break;
	case ACT_READ:
	case ACT_READ_BIT:
	  valueLen = test->par3;
	  result =
	    asn1_read_value (asn1_element, test->par1, value, &valueLen);
	  break;
	case ACT_READ_LENGTH:
	  valueLen = 0;
	  result =
	    asn1_read_value (asn1_element, test->par1, NULL, &valueLen);
	  break;
	case ACT_READ_DEFINITIONS:
	  valueLen = 1024;
	  result =
	    asn1_read_value (definitions, test->par1, value, &valueLen);
	  break;
	case ACT_READ_TAG_CLASS:
	  result = asn1_read_tag (asn1_element, test->par1, &tag, &class);
	  break;
	case ACT_ENCODING:
	  der_len = test->par3;
	  result = asn1_der_coding (asn1_element, test->par1, der, &der_len,
				    errorDescription);
	  break;
	case ACT_ENCODING_LENGTH:
	  der_len = 0;
	  result = asn1_der_coding (asn1_element, test->par1, NULL, &der_len,
				    errorDescription);
	  break;
	case ACT_DECODING:
	  result = asn1_der_decoding (&asn1_element, der, der_len,
				      errorDescription);
	  break;
	case ACT_DECODING_ELEMENT:
	  result =
	    asn1_der_decoding_element (&asn1_element, test->par1, der,
				       der_len, errorDescription);
	  break;
	case ACT_DECODING_START_END:
	  result =
	    asn1_der_decoding_startEnd (asn1_element, der, der_len,
					test->par1, &start, &end);
	  break;
	case ACT_EXPAND_ANY:
	  result = asn1_expand_any_defined_by (definitions, &asn1_element);
	  break;
	case ACT_EXPAND_OCTET:
	  result =
	    asn1_expand_octet_string (definitions, &asn1_element, test->par1,
				      test->par2);
	  break;
	case ACT_OID_2_STRUCTURE:
	  str_p = asn1_find_structure_from_oid (definitions, test->par1);
	  break;
	case ACT_VISIT:
	  if (verbose)
	    {
	      asn1_print_structure (out, asn1_element, test->par1,
				    test->par3);
	      fprintf (out, "\n");
	    }
	  result = ASN1_SUCCESS;
	  break;
	case ACT_PRINT_DER:
	  if (verbose)
	    {
	      printf ("DER encoding len:%i\n", der_len);
	      printf ("DER encoding: ");
	      for (k = 0; k < der_len; k++)
		printf ("%02x ", der[k]);
	      printf ("\n\n");
	    }
	  result = ASN1_SUCCESS;
	  break;
	case ACT_SET_DER:
	  der_len = test->par3;
	  memcpy (der, test->par1, der_len);
	  result = ASN1_SUCCESS;
	  break;
	case ACT_NUMBER_OF_ELEMENTS:
	  result =
	    asn1_number_of_elements (asn1_element, test->par1, &valueLen);
	  break;
	default:
	  break;
	}


      switch (test->action)
	{
	case ACT_CREATE:
	case ACT_DELETE:
	case ACT_DELETE_ELEMENT:
	case ACT_WRITE:
	case ACT_VISIT:
	case ACT_ENCODING:
	case ACT_DECODING:
	case ACT_PRINT_DER:
	case ACT_EXPAND_ANY:
	case ACT_EXPAND_OCTET:
	case ACT_SET_DER:
	  if (result != test->errorNumber)
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s - %s - %d\n", test->action,
		      test->par1, test->par2, test->par3);
	      printf ("  Error expected: %s\n",
		      asn1_strerror (test->errorNumber));
	      printf ("  Error detected: %s\n\n", asn1_strerror (result));
	      error ();
	    }
	  break;
	case ACT_DECODING_ELEMENT:
	  if (result != test->errorNumber)
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s - %s - %d\n", test->action,
		      test->par1, test->par2, test->par3);
	      printf ("  Error expected: %s\n",
		      asn1_strerror (test->errorNumber));
	      printf ("  Error detected: %s\n", asn1_strerror (result));
	      printf ("  Error description : %s\n\n", errorDescription);
	      error ();
	    }
	  break;
	case ACT_NUMBER_OF_ELEMENTS:
	case ACT_READ_LENGTH:
	  if ((result != test->errorNumber) || (valueLen != test->par3))
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s\n", test->action, test->par1);
	      printf ("  Error expected: %s - %d\n",
		      asn1_strerror (test->errorNumber), test->par3);
	      printf ("  Error detected: %s - %d\n\n", asn1_strerror (result),
		      valueLen);
	      error ();
	    }
	  break;
	case ACT_ENCODING_LENGTH:
	  if ((result != test->errorNumber) || (der_len != test->par3))
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s\n", test->action, test->par1);
	      printf ("  Error expected: %s - %d\n",
		      asn1_strerror (test->errorNumber), test->par3);
	      printf ("  Error detected: %s - %d\n\n", asn1_strerror (result),
		      der_len);
	      error ();
	    }
	  break;
	case ACT_OID_2_STRUCTURE:
	  if (((test->errorNumber != ASN1_SUCCESS) && (str_p != NULL)) ||
	      ((test->errorNumber == ASN1_SUCCESS) && (str_p == NULL)) ||
	      ((test->errorNumber == ASN1_SUCCESS)
	       && (strcmp (str_p, test->par2))))
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s\n", test->action, test->par1);
	      printf ("  Error expected: %s - %s\n",
		      asn1_strerror (test->errorNumber), test->par2);
	      printf ("  Value detected: %s\n\n", str_p);
	      error ();
	    }
	  break;
	case ACT_DECODING_START_END:
	  if ((result != test->errorNumber) ||
	      ((!strcmp (test->par2, "START")) && (start != test->par3)) ||
	      ((!strcmp (test->par2, "END")) && (end != test->par3)))
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s - %d\n", test->action, test->par1,
		      test->par3);
	      printf ("  Error expected: %s - %s - %d\n",
		      asn1_strerror (test->errorNumber), test->par2,
		      test->par3);
	      printf ("  Error detected: %s - %d - %d\n\n",
		      asn1_strerror (result), start, end);
	      error ();
	    }
	  break;

	case ACT_READ_TAG_CLASS:
	  if ((result != test->errorNumber) ||
	      ((!strcmp (test->par2, "TAG")) && (tag != test->par3)) ||
	      ((!strcmp (test->par2, "CLASS")) && (class != test->par3)))
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s - %d\n", test->action, test->par1,
		      test->par3);
	      printf ("  Error expected: %s - %s - %d\n",
		      asn1_strerror (test->errorNumber), test->par2,
		      test->par3);
	      printf ("  Error detected: %s - %d - %d\n\n",
		      asn1_strerror (result), tag, class);
	      error ();
	    }

	  break;

	case ACT_READ:
	case ACT_READ_DEFINITIONS:
	case ACT_READ_BIT:
	  if (test->action == ACT_READ_BIT)
	    {
	      if (valueLen % 8 == 0)
		tag = valueLen / 8;
	      else
		tag = (valueLen / 8) + 1;
	      if (test->par3 % 8 == 0)
		class = test->par3 / 8;
	      else
		class = (test->par3 / 8) + 1;
	    }

	  for (k = 0; k < class; k++)
	    if ((unsigned char) test->par2[k] != value[k])
	      {
		k = -1;
		break;
	      }

	  if ((result != test->errorNumber) ||
	      (valueLen != test->par3) || (k == -1))
	    {
	      errorCounter++;
	      printf ("ERROR in %d:\n", test->line);
	      printf ("  Action %d - %s\n", test->action, test->par1);
	      printf ("  Error expected: %s - %d - ",
		      asn1_strerror (test->errorNumber), test->par3);
	      for (k = 0; k < test->par3; k++)
		printf ("%02x", (unsigned int) test->par2[k]);
	      printf ("\n  Error detected: %s - %d - ",
		      asn1_strerror (result), valueLen);
	      for (k = 0; k < valueLen; k++)
		printf ("%02x", value[k]);
	      printf ("\n\n");

	      error ();
	    }

	  break;
	default:
	  break;
	}

      test++;
    }


  if (verbose != 0)
    {
      printf ("Total tests : %d\n", testCounter);
      printf ("Total errors: %d\n", errorCounter);
    }

  /* Clear the definition structures */
  asn1_delete_structure (&definitions);


  if (out != stdout)
    fclose (out);

  if (errorCounter > 0)
    return 1;

  exit (0);
}
