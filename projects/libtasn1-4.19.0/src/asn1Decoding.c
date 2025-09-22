/* asn1Decoding.c ---  program to generate an ASN1 type from a DER coding.
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
#include <sys/types.h>
#include <sys/stat.h>

#include <libtasn1.h>

#include "benchmark.h"

#include "progname.h"
#include "read-file.h"
#include "version-etc.h"

static int decode (asn1_node definitions, const char *typeName, void *der,
		   int der_len, int benchmark, int strict);

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
Usage: %s [OPTION] DEFINITIONS ENCODED ASN1TYPE\n", program_name);
      printf ("\
Decodes DER data in ENCODED file, for the ASN1TYPE element\n\
described in ASN.1 DEFINITIONS file, and print decoded structures.\n\
\n");
      printf ("\
  -b, --benchmark       perform a benchmark on decoding\n\
  -s, --strict          use strict DER decoding\n\
  -t, --no-time-strict  use strict DER decoding but not in time fields\n\
  -h, --help            display this help and exit\n\
  -v, --version         output version information and exit\n");
      emit_bug_reporting_address ();
    }
  exit (status);
}

int
main (int argc, char *argv[])
{
  static const struct option long_options[] = {
    {"help", no_argument, 0, 'h'},
    {"strict", no_argument, 0, 's'},
    {"no-time-strict", no_argument, 0, 't'},
    {"debug", no_argument, 0, 'd'},
    {"benchmark", no_argument, 0, 'b'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };
  int option_index = 0;
  int option_result;
  char *inputFileAsnName = NULL;
  char *inputFileDerName = NULL;
  char *typeName = NULL;
  asn1_node definitions = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  int asn1_result = ASN1_SUCCESS;
  unsigned char *der;
  int der_len = 0, benchmark = 0;
  int flags = 0, debug = 0;
  /* FILE *outputFile; */

  set_program_name (argv[0]);

  opterr = 0;			/* disable error messages from getopt */

  while (1)
    {

      option_result =
	getopt_long (argc, argv, "hbdsvtc", long_options, &option_index);

      if (option_result == -1)
	break;

      switch (option_result)
	{
	case 'h':		/* HELP */
	  usage (EXIT_SUCCESS);
	  break;
	case 'b':
	  benchmark = 1;
	  break;
	case 'd':
	  debug = 1;
	  break;
	case 's':
	case 't':
	  flags |= ASN1_DECODE_FLAG_STRICT_DER;
	  if (option_result == 't')
	    flags |= ASN1_DECODE_FLAG_ALLOW_INCORRECT_TIME;
	  break;
	case 'v':		/* VERSION */
	  version_etc (stdout, program_name, PACKAGE, VERSION,
		       "Fabio Fiorina", NULL);
	  exit (0);
	  break;
	case '?':		/* UNKNOW OPTION */
	  fprintf (stderr,
		   "asn1Decoding: option '%s' not recognized or without argument.\n\n",
		   argv[optind - 1]);
	  usage (EXIT_FAILURE);
	  break;
	default:
	  fprintf (stderr,
		   "asn1Decoding: ?? getopt returned character code Ox%x ??\n",
		   (unsigned) option_result);
	}
    }

  if (optind == argc || optind == argc - 1 || optind == argc - 2)
    {
      fprintf (stderr, "asn1Decoding: input files or ASN.1 type "
	       "name missing\n");
      usage (EXIT_FAILURE);
    }

  inputFileAsnName = strdup (argv[optind]);
  inputFileDerName = strdup (argv[optind + 1]);
  typeName = strdup (argv[optind + 2]);

  if (!(inputFileAsnName && inputFileDerName && typeName))
    {
      fprintf (stderr, "allocation failed\n");
      free (inputFileAsnName);
      free (inputFileDerName);
      free (typeName);
      exit (1);
    }

  asn1_result =
    asn1_parser2tree (inputFileAsnName, &definitions, errorDescription);

  switch (asn1_result)
    {
    case ASN1_SUCCESS:
      fprintf (stderr, "Parse: done.\n");
      break;
    case ASN1_FILE_NOT_FOUND:
      fprintf (stderr, "asn1Decoding: FILE %s NOT FOUND\n", inputFileAsnName);
      break;
    case ASN1_SYNTAX_ERROR:
    case ASN1_IDENTIFIER_NOT_FOUND:
    case ASN1_NAME_TOO_LONG:
      fprintf (stderr, "asn1Decoding: %s\n", errorDescription);
      break;
    default:
      fprintf (stderr, "libtasn1 ERROR: %s\n", asn1_strerror (asn1_result));
    }

  if (asn1_result != ASN1_SUCCESS)
    {
      free (inputFileAsnName);
      free (inputFileDerName);
      free (typeName);
      exit (1);
    }


  {
    size_t tmplen;
    der = (unsigned char *) read_file (inputFileDerName, RF_BINARY, &tmplen);
    der_len = tmplen;
  }

  /* read_binary_file() returns a buffer with more data than required,
   * with this reallocation we ensure that memory accesses outside the
   * boundaries are detected */
  if (der != NULL && debug != 0)
    der = realloc (der, der_len);

  if (der == NULL)
    {
      fprintf (stderr, "asn1Decoding: could not read '%s'\n",
	       inputFileDerName);
      asn1_delete_structure (&definitions);

      free (inputFileAsnName);
      free (inputFileDerName);
      free (typeName);
      exit (1);
    }

 /*****************************************/
  /* ONLY FOR TEST                         */
 /*****************************************/
  /*
     der_len=0;
     outputFile=fopen("data.p12","w");
     while(fscanf(inputFile,"%c",der+der_len) != EOF)
     {
     if((der_len>=0x11) && (der_len<=(0xe70)))
     fprintf(outputFile,"%c",der[der_len]);
     der_len++;
     }
     fclose(outputFile);
     fclose(inputFile);
   */

  if (decode (definitions, typeName, der, der_len, benchmark, flags) !=
      ASN1_SUCCESS)
    {
      asn1_delete_structure (&definitions);
      free (inputFileAsnName);
      free (inputFileDerName);
      free (typeName);
      free (der);
      exit (1);
    }

  asn1_delete_structure (&definitions);

  free (der);

  free (inputFileAsnName);
  free (inputFileDerName);
  free (typeName);

  if (asn1_result != ASN1_SUCCESS)
    exit (1);

  exit (0);
}

static int
simple_decode (asn1_node definitions, const char *typeName, void *der,
	       int der_len, int benchmark, int flags)
{

  int asn1_result;
  asn1_node structure = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];

  asn1_result = asn1_create_element (definitions, typeName, &structure);

  /* asn1_print_structure(stdout,structure,"",ASN1_PRINT_ALL); */


  if (asn1_result != ASN1_SUCCESS)
    {
      fprintf (stderr, "Structure creation: %s\n",
	       asn1_strerror (asn1_result));
      asn1_delete_structure (&structure);
      return asn1_result;
    }

  if (flags != 0)
    asn1_result =
      asn1_der_decoding2 (&structure, der, &der_len, flags, errorDescription);
  else
    asn1_result =
      asn1_der_decoding (&structure, der, der_len, errorDescription);

  if (!benchmark)
    fprintf (stderr, "\nDecoding: %s\n", asn1_strerror (asn1_result));
  if (asn1_result != ASN1_SUCCESS)
    {
      fprintf (stderr, "asn1Decoding: %s\n", errorDescription);
      asn1_delete_structure (&structure);
      return asn1_result;
    }

  if (!benchmark)
    {
      fprintf (stderr, "\nDECODING RESULT:\n");
      asn1_print_structure (stdout, structure, "",
			    ASN1_PRINT_NAME_TYPE_VALUE);
    }
  asn1_delete_structure (&structure);
  return ASN1_SUCCESS;
}

static int
decode (asn1_node definitions, const char *typeName, void *der, int der_len,
	int benchmark, int flags)
{
  struct benchmark_st st;

  if (benchmark == 0)
    return simple_decode (definitions, typeName, der, der_len, benchmark,
			  flags);
  else
    {
      start_benchmark (&st);

      do
	{
	  simple_decode (definitions, typeName, der, der_len, benchmark,
			 flags);
	  st.size++;
	}
      while (benchmark_must_finish == 0);

      stop_benchmark (&st, "structures");
      fprintf (stdout, "\n");

    }
  return ASN1_SUCCESS;
}
