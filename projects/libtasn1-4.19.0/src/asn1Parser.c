/* asn1Parser.c -- program to parse a file with ASN1 definitions
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
Usage: %s [OPTION] FILE\n", program_name);
      printf ("\
Read FILE with ASN.1 definitions and generate\n\
a C array that is used with libtasn1 functions.\n\
\n");
      printf ("\
Mandatory arguments to long options are mandatory for short options too.\n\
  -c, --check           checks the syntax only\n\
  -o, --output=FILE     output file\n\
  -n, --name=NAME       array name\n\
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
    {"version", no_argument, 0, 'v'},
    {"check", no_argument, 0, 'c'},
    {"output", required_argument, 0, 'o'},
    {"name", required_argument, 0, 'n'},
    {0, 0, 0, 0}
  };
  int option_index = 0;
  int option_result;
  char *outputFileName = NULL;
  char *inputFileName = NULL;
  char *vectorName = NULL;
  int checkSyntaxOnly = 0;
  asn1_node pointer = NULL;
  char errorDescription[ASN1_MAX_ERROR_DESCRIPTION_SIZE];
  int parse_result = ASN1_SUCCESS;

  set_program_name (argv[0]);

  opterr = 0;			/* disable error messages from getopt */

  while (1)
    {

      option_result =
	getopt_long (argc, argv, "hvco:n:", long_options, &option_index);

      if (option_result == -1)
	break;

      switch (option_result)
	{
	case 0:
	  fprintf (stderr, "option %s", long_options[option_index].name);
	  if (optarg)
	    fprintf (stderr, " with arg %s", optarg);
	  putc ('\n', stderr);
	  break;
	case 'h':		/* HELP */
	  free (outputFileName);
	  free (vectorName);
	  usage (EXIT_SUCCESS);
	  break;
	case 'v':		/* VERSION */
	  version_etc (stdout, program_name, PACKAGE, VERSION,
		       "Fabio Fiorina", NULL);
	  free (outputFileName);
	  free (vectorName);
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
	case 'n':		/* VECTOR NAME */
	  assert (optarg != NULL);
	  vectorName = strdup (optarg);
	  if (vectorName == NULL)
	    {
	      fprintf (stderr, "Memory error\n");
	      exit (1);
	    }
	  break;
	case '?':		/* UNKNOW OPTION */
	  fprintf (stderr,
		   "asn1Parser: option '%s' not recognized or without argument.\n\n",
		   argv[optind - 1]);
	  free (outputFileName);
	  free (vectorName);
	  usage (EXIT_FAILURE);
	  break;
	default:
	  fprintf (stderr,
		   "asn1Parser: ?? getopt returned character code Ox%x ??\n",
		   (unsigned) option_result);
	}

    }

  if (optind == argc)
    {
      free (outputFileName);
      free (vectorName);
      usage (EXIT_SUCCESS);
    }
  else
    {
      inputFileName = (char *) malloc (strlen (argv[optind]) + 1);
      if (inputFileName == NULL)
	{
	  fprintf (stderr, "Memory error\n");
	  exit (1);
	}
      strcpy (inputFileName, argv[optind]);
    }

  if (checkSyntaxOnly == 1)
    {
      parse_result =
	asn1_parser2tree (inputFileName, &pointer, errorDescription);
      asn1_delete_structure (&pointer);
    }
  else				/* C VECTOR CREATION */
    parse_result = asn1_parser2array (inputFileName,
				      outputFileName, vectorName,
				      errorDescription);

  switch (parse_result)
    {
    case ASN1_SUCCESS:
      fputs ("Done.\n", stderr);
      break;
    case ASN1_FILE_NOT_FOUND:
      fprintf (stderr, "asn1Parser: file %s was not found\n", inputFileName);
      break;
    case ASN1_SYNTAX_ERROR:
    case ASN1_IDENTIFIER_NOT_FOUND:
    case ASN1_NAME_TOO_LONG:
      fprintf (stderr, "asn1Parser: %s\n", errorDescription);
      break;
    default:
      fprintf (stderr, "libtasn1 ERROR: %s\n", asn1_strerror (parse_result));
    }


  free (inputFileName);
  free (outputFileName);
  free (vectorName);

  if (parse_result != ASN1_SUCCESS)
    exit (1);
  exit (0);
}
