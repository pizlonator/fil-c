/* asn1Coding.c --- program to generate a DER coding of an ASN1 definition.
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

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include <libtasn1.h>

#include "progname.h"
#include "version-etc.h"

/* This feature is available in gcc versions 2.5 and later.  */
#if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 5)
# define ATTR_NO_RETRUN
#else
# define ATTR_NO_RETRUN __attribute__ ((__noreturn__))
#endif

ATTR_NO_RETRUN static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, "Try `%s --help' for more information.\n", program_name);
  else
    {
      printf ("\
Usage: %s [OPTION] DEFINITIONS ASSIGNMENTS\n", program_name);
      printf ("\
Generates a DER encoding of ASN.1 DEFINITIONS file\n\
and ASSIGNMENTS file with value assignments.\n\
\n");
      printf ("\
Mandatory arguments to long options are mandatory for short options too.\n\
  -c, --check           checks the syntax only\n\
  -o, --output=FILE     output file\n\
  -h, --help            display this help and exit\n\
  -v, --version         output version information and exit\n");
      emit_bug_reporting_address ();
    }
  exit (status);
}

#define ASSIGNMENT_SUCCESS 1
#define ASSIGNMENT_ERROR   2
#define ASSIGNMENT_EOF     3

static int
readAssignment (FILE * file, char *varName, char *value)
{

  int ret;

  ret = fscanf (file, "%1023s", varName);
  if (ret == EOF)
    return ASSIGNMENT_EOF;
  if (!strcmp (varName, "''"))
    varName[0] = 0;

  ret = fscanf (file, "%1023s", value);
  if (ret == EOF)
    return ASSIGNMENT_ERROR;

  return ASSIGNMENT_SUCCESS;
}

static void
createFileName (char *inputFileName, char **outputFileName)
{
  char *char_p, *slash_p, *dot_p;

  /* searching the last '/' and '.' in inputFileAssignmentName */
  char_p = inputFileName;
  slash_p = inputFileName;
  while ((char_p = strchr (char_p, '/')))
    {
      char_p++;
      slash_p = char_p;
    }

  char_p = slash_p;
  dot_p = inputFileName + strlen (inputFileName);

  while ((char_p = strchr (char_p, '.')))
    {
      dot_p = char_p;
      char_p++;
    }

  /* outputFileName= inputFileName + .out */
  *outputFileName = (char *) malloc (dot_p - inputFileName + 1 +
				     strlen (".out"));
  if (*outputFileName == NULL)
    {
      fprintf (stderr, "Memory error\n");
      exit (1);
    }

  memcpy (*outputFileName, inputFileName, dot_p - inputFileName);
  (*outputFileName)[dot_p - inputFileName] = 0;
  strcat (*outputFileName, ".out");
  return;
}

int
main (int argc, char *argv[])
{
  static const struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {"check", no_argument, 0, 'c'},
    {"output", required_argument, 0, 'o'},
    {0, 0, 0, 0}
  };
  int option_index = 0;
  int option_result;
  char *outputFileName = NULL;
  char *inputFileAsnName = NULL;
  char *inputFileAssignmentName = NULL;
  int checkSyntaxOnly = 0;
  asn1_node definitions = NULL;
  asn1_node structure = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  int asn1_result = ASN1_SUCCESS;
  FILE *outputFile;
  FILE *inputFile;
  char varName[1024];
  char value[1024];
  unsigned char *der = NULL;
  int der_len;
  int k;
  int last_ra;

  set_program_name (argv[0]);

  opterr = 0;			/* disable error messages from getopt */

  while (1)
    {

      option_result =
	getopt_long (argc, argv, "hvco:", long_options, &option_index);

      if (option_result == -1)
	break;

      switch (option_result)
	{
	case 'h':		/* HELP */
	  free (outputFileName);
	  usage (EXIT_SUCCESS);
	  break;
	case 'v':		/* VERSION */
	  version_etc (stdout, program_name, PACKAGE, VERSION,
		       "Fabio Fiorina", NULL);
	  free (outputFileName);
	  exit (0);
	  break;
	case 'c':		/* CHECK SYNTAX */
	  checkSyntaxOnly = 1;
	  break;
	case 'o':		/* OUTPUT */
	  assert (optarg != NULL);
	  outputFileName = strdup (optarg);
	  if (outputFileName == NULL)
	    {
	      fprintf (stderr, "Memory error\n");
	      exit (1);
	    }
	  break;
	case '?':		/* UNKNOW OPTION */
	  free (outputFileName);
	  fprintf (stderr,
		   "asn1Coding: option '%s' not recognized or without argument.\n\n",
		   argv[optind - 1]);
	  usage (EXIT_FAILURE);
	  break;
	default:
	  fprintf (stderr,
		   "asn1Coding: ?? getopt returned character code Ox%x ??\n",
		   (unsigned) option_result);
	}
    }

  if (optind == argc || optind == argc - 1)
    {
      free (outputFileName);
      fputs ("asn1Coding: input files missing\n", stderr);
      usage (EXIT_FAILURE);
    }

  inputFileAsnName = strdup (argv[optind]);
  if (inputFileAsnName == NULL)
    {
      fprintf (stderr, "Memory error\n");
      exit (1);
    }

  inputFileAssignmentName = strdup (argv[optind + 1]);
  if (inputFileAssignmentName == NULL)
    {
      fprintf (stderr, "Memory error\n");
      exit (1);
    }

  asn1_result =
    asn1_parser2tree (inputFileAsnName, &definitions, errorDescription);

  switch (asn1_result)
    {
    case ASN1_SUCCESS:
      fputs ("Parse: done.\n", stderr);
      break;
    case ASN1_FILE_NOT_FOUND:
      fprintf (stderr, "asn1Coding: FILE %s NOT FOUND\n", inputFileAsnName);
      break;
    case ASN1_SYNTAX_ERROR:
    case ASN1_IDENTIFIER_NOT_FOUND:
    case ASN1_NAME_TOO_LONG:
      fprintf (stderr, "asn1Coding: %s\n", errorDescription);
      break;
    default:
      fprintf (stderr, "libtasn1 ERROR: %s\n", asn1_strerror (asn1_result));
    }

  if (asn1_result != ASN1_SUCCESS)
    {
      free (inputFileAsnName);
      free (inputFileAssignmentName);
      exit (1);
    }


  inputFile = fopen (inputFileAssignmentName, "r");

  if (inputFile == NULL)
    {
      fprintf (stderr, "asn1Coding: file '%s' not found\n",
	       inputFileAssignmentName);
      free (inputFileAsnName);
      free (inputFileAssignmentName);
      exit (1);
    }


  putc ('\n', stderr);

  while ((last_ra = readAssignment (inputFile, varName, value))
	 == ASSIGNMENT_SUCCESS)
    {
      fprintf (stderr, "var=%s, value=%s\n", varName, value);
      if (structure == NULL)
	{
	  asn1_result = asn1_create_element (definitions, value, &structure);
	}
      else
	{
	  if (strcmp (value, "(NULL)") == 0)
	    asn1_result = asn1_write_value (structure, varName, NULL, 0);
	  else
	    asn1_result = asn1_write_value (structure, varName, value, 0);
	}

      if (asn1_result != ASN1_SUCCESS)
	{
	  fprintf (stderr, "libtasn1 ERROR: %s\n",
		   asn1_strerror (asn1_result));

	  asn1_delete_structure (&definitions);
	  asn1_delete_structure (&structure);

	  free (inputFileAsnName);
	  free (inputFileAssignmentName);

	  fclose (inputFile);
	  exit (1);
	}
    }
  if (last_ra != ASSIGNMENT_EOF)
    {
      fprintf (stderr, "asn1Coding: error reading assignment file\n");
      exit (1);
    }
  fclose (inputFile);

  putc ('\n', stderr);
  asn1_print_structure (stderr, structure, "", ASN1_PRINT_NAME_TYPE_VALUE);

  der_len = 0;
  asn1_result = asn1_der_coding (structure, "", NULL, &der_len,
				 errorDescription);
  if (asn1_result == ASN1_MEM_ERROR)
    {
      der = malloc (der_len);
      if (der == NULL)
	{
	  fprintf (stderr, "Memory error\n");
	  exit (1);
	}
      asn1_result = asn1_der_coding (structure, "", der, &der_len,
				     errorDescription);
    }
  fprintf (stderr, "\nCoding: %s\n\n", asn1_strerror (asn1_result));
  if (asn1_result != ASN1_SUCCESS)
    {
      fprintf (stderr, "asn1Coding: %s\n", errorDescription);

      free (der);

      asn1_delete_structure (&definitions);
      asn1_delete_structure (&structure);

      free (inputFileAsnName);
      free (inputFileAssignmentName);

      exit (1);
    }

  /* Print the 'Certificate1' DER encoding */
  fprintf (stderr, "-----------------\nNumber of bytes=%i\n", der_len);
  for (k = 0; k < der_len; k++)
    fprintf (stderr, "%02x ", der[k]);
  fputs ("\n-----------------\n", stderr);

  asn1_delete_structure (&definitions);
  asn1_delete_structure (&structure);

  if (!checkSyntaxOnly)
    {
      if (outputFileName == NULL)
	createFileName (inputFileAssignmentName, &outputFileName);

      fprintf (stderr, "\nOutputFile=%s\n", outputFileName);

      outputFile = fopen (outputFileName, "w");

      if (outputFile == NULL)
	{
	  fprintf (stderr,
		   "asn1Coding: output file '%s' not available\n",
		   outputFileName);
	  free (der);
	  free (inputFileAsnName);
	  free (inputFileAssignmentName);
	  free (outputFileName);
	  exit (1);
	}

      for (k = 0; k < der_len; k++)
	fprintf (outputFile, "%c", der[k]);
      fclose (outputFile);
      fputs ("\nWriting: done.\n", stderr);
    }

  free (der);

  free (inputFileAsnName);
  free (inputFileAssignmentName);
  free (outputFileName);

  exit (0);
}
