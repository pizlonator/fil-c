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
/* File: Test_parser.c                               */
/* Description: Test sequences for these functions:  */
/*     asn1_parser_asn1,                             */
/*****************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "libtasn1.h"

#define MAX_ERRS 3

typedef struct
{
  int lineNumber;
  const char *line;
  int errorNumber;
  const char *errorDescription[MAX_ERRS];
} test_type;

const char *fileCorrectName;
char fileErroredName[] = "Test_parser_ERROR.asn";

#define _FILE_ "Test_parser_ERROR.asn"

test_type test_array[] = {
  /* Test DEFINITIONS syntax */
  {5,
   "TEST_PARSER2 { } DEFINITIONS IMPLICIT TAGS ::= BEGIN int1 ::= INTEGER END",
   ASN1_SYNTAX_ERROR,
   {_FILE_ ":6: Error: syntax error, unexpected IDENTIFIER, expecting end of file near 'TEST_PARSER'",	/* bison >= 3.6 */
    _FILE_ ":6: Error: syntax error, unexpected IDENTIFIER, expecting $end near 'TEST_PARSER'"}},	/* bison < 3.6 */
  {6, "TEST_PARSER { }", ASN1_SUCCESS, {""}},

  /* Test ASN1_MAX_NAME_SIZE (128) */
  {12,
   "a123456789012345678901234567890123456789012345678901234567890123 ::= INTEGER",
   ASN1_SUCCESS, {""}},
  {12,
   "a1234567890123456789012345678901234567890123456789012345678901234 ::= INTEGER",
   ASN1_NAME_TOO_LONG,
   {_FILE_ ":12: name too long (more than 64 characters)"}},
  /* Test 'check identifier' function */
  {12, "ident1 ::= ident2   ident2 ::= INTEGER",
   ASN1_SUCCESS, {""}},
  {12, "ident1 ::= ident2",
   ASN1_IDENTIFIER_NOT_FOUND, {_FILE_ ":: identifier 'ident2' not found"}},
  {12, "obj1 OBJECT IDENTIFIER ::= {pkix 0 5 4}    "
   "pkix OBJECT IDENTIFIER ::= {1 2}",
   ASN1_SUCCESS, {""}},
  {12, "obj1 OBJECT IDENTIFIER ::= {pkix 0 5 4}",
   ASN1_IDENTIFIER_NOT_FOUND, {_FILE_ ":: identifier 'pkix' not found"}},

  /* Test INTEGER */
  {14, "int1 INTEGER (-5..5),", ASN1_SUCCESS, {""}},
  {14, "int1 INTEGER OPTIONAL,", ASN1_SUCCESS, {""}},
  {14, "int1 INTEGER DEFAULT 1,", ASN1_SUCCESS, {""}},
  {14, "int1 INTEGER DEFAULT -1,", ASN1_SUCCESS, {""}},
  {14, "int1 INTEGER DEFAULT v1,", ASN1_SUCCESS, {""}},
  {14, "int1 [1] INTEGER,", ASN1_SUCCESS, {""}},
  {14, "int1 [1] EXPLICIT INTEGER,", ASN1_SUCCESS, {""}},
  {14, "int1 [1] IMPLICIT INTEGER,", ASN1_SUCCESS, {""}},
  {12, "Integer ::= [1] EXPLICIT INTEGER {v1(-1), v2(1)}", ASN1_SUCCESS,
   {""}},
  {12, "Integer ::= INTEGER {v1(0), v2}", ASN1_SYNTAX_ERROR,
   {_FILE_
    ":12: Error: syntax error, unexpected '}', expecting '(' near '}'"}},
  {12, "Integer ::= INTEGER {v1(0), 1}",
   ASN1_SYNTAX_ERROR,
   {_FILE_
    ":12: Error: syntax error, unexpected NUM, expecting IDENTIFIER or '(' near '1'"}},
  {12, "const1 INTEGER ::= -1", ASN1_SUCCESS, {""}},
  {12, "const1 INTEGER ::= 1", ASN1_SUCCESS, {""}},
  {12, "const1 INTEGER ::= v1",
   ASN1_SYNTAX_ERROR,
   {_FILE_
    ":12: Error: syntax error, unexpected IDENTIFIER, expecting NUM or '+' or '-' near 'v1'"}},
  {16, " generic generalstring",
   ASN1_IDENTIFIER_NOT_FOUND,
   {_FILE_ ":: identifier 'generalstring' not found"}},

  /* Test: OID */
  {20, "   oid1    OBJECT IDENTIFIER DEFAULT Oid-type",
   ASN1_IDENTIFIER_NOT_FOUND, {_FILE_ ":: identifier 'Oid-type' not found"}},
  {20, "   oid1    OBJECT IDENTIFIER DEFAULT 1",
   ASN1_IDENTIFIER_NOT_FOUND, {_FILE_ ":: identifier '1' not found"}},
  {20, "   oid1    OBJECT IDENTIFIER DEFAULT",
   ASN1_SYNTAX_ERROR,
   {_FILE_ ":21: Error: syntax error, unexpected '}' near '}'"}},
  {20, "   oid1    OBJECT IDENTIFIER DEFAULT Oid-type1",
   ASN1_SUCCESS, {""}},
  {22,
   "KeyUsage ::= BIT STRING { xxx   (0), enring       UTF8String     (SIZE (1..200)) }",
   ASN1_SYNTAX_ERROR,
   {_FILE_
    ":22: Error: syntax error, unexpected UTF8String, expecting '(' near 'UTF8String'"}},

  /* end */
  {0}
};

static int
readLine (FILE * file, char *line)
{
  int c;

  while (((c = fgetc (file)) != EOF) && (c != '\n'))
    {
      *line = c;
      line++;
    }

  *line = 0;

  return c;
}

static void
createFile (int lineNumber, const char *line)
{
  FILE *fileIn, *fileOut;
  char lineRead[1024];
  int fileInLineNumber = 0;

  fileIn = fopen (fileCorrectName, "r");
  fileOut = fopen (fileErroredName, "w");

  while (readLine (fileIn, lineRead) != EOF)
    {
      fileInLineNumber++;
      if (fileInLineNumber == lineNumber)
	fprintf (fileOut, "%s\n", line);
      else
	fprintf (fileOut, "%s\n", lineRead);
    }

  fclose (fileOut);
  fclose (fileIn);
}

static int
errorStrcmp (const char *needle, const char **haystack)
{
  for (; haystack && *haystack; haystack++)
    if (strcmp (needle, *haystack) == 0)
      return 0;
  return 1;
}

int
main (int argc, char *argv[])
{
  int result;
  asn1_node definitions = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  test_type *test;
  int errorCounter = 0, testCounter = 0;
  int verbose = 0;

  if (argc > 1)
    verbose = 1;

  fileCorrectName = getenv ("ASN1PARSER");
  if (!fileCorrectName)
    fileCorrectName = "Test_parser.asn";

  if (verbose != 0)
    {
      printf ("\n\n/****************************************/\n");
      printf ("/*     Test sequence : Test_parser      */\n");
      printf ("/****************************************/\n\n");
      printf ("ASN1PARSER: %s\n", fileCorrectName);
    }

  result = asn1_parser2tree (fileCorrectName, &definitions, errorDescription);

  if (result != ASN1_SUCCESS)
    {
      printf ("File '%s' not correct\n", fileCorrectName);
      asn1_perror (result);
      printf ("ErrorDescription = %s\n\n", errorDescription);
      exit (1);
    }

  /* Only for Test */
  /* asn1_visit_tree(stdout,definitions,"TEST_PARSER",ASN1_PRINT_ALL); */

  /* Clear the definitions structures */
  asn1_delete_structure (&definitions);


  test = test_array;

  while (test->lineNumber != 0)
    {
      testCounter++;

      createFile (test->lineNumber, test->line);

      result =
	asn1_parser2tree (fileErroredName, &definitions, errorDescription);
      asn1_delete_structure (&definitions);

      if ((result != test->errorNumber) ||
	  (errorStrcmp (errorDescription, test->errorDescription)))
	{
	  errorCounter++;
	  printf ("ERROR N. %d:\n", errorCounter);
	  printf ("  Line %d - %s\n", test->lineNumber, test->line);
	  printf ("  Error expected: %s - %s\n",
		  asn1_strerror (test->errorNumber), *test->errorDescription);
	  printf ("  Error detected: %s - %s\n\n", asn1_strerror (result),
		  errorDescription);
	  exit (1);
	}

      test++;
    }


  if (verbose != 0)
    {
      printf ("Total tests : %d\n", testCounter);
      printf ("Total errors: %d\n", errorCounter);
    }

  if (errorCounter > 0)
    return 1;

  exit (0);
}
