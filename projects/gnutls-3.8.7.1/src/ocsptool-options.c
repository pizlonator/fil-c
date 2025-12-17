#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ocsptool-options.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif /* !_WIN32 */
#include <limits.h>

struct ocsptool_options ocsptool_options;

static long
parse_number (const char *arg)
{
  char *endptr = NULL;
  errno = 0;
  long result;

  if (strncmp (arg, "0x", 2) == 0)
    result = strtol (arg + 2, &endptr, 16);
  else if (strncmp (arg, "0", 1) == 0
           && strspn (arg, "012345678") == strlen (optarg))
    result = strtol (arg + 1, &endptr, 8);
  else
    result = strtol (arg, &endptr, 10);

  if (errno != 0 || (endptr && *endptr != '\0'))
    {
      char buf[80];
      snprintf (buf, sizeof(buf),
                "'%s' is not a recognizable number",
                arg);
      perror (buf);
      exit (EXIT_FAILURE);
    }

  return result;
}

/* Long options.  */
static const struct option long_options[] =
{
  { "debug", required_argument, 0, 'd' },
  { "verbose", no_argument, 0, 'V' },
  { "infile", required_argument, 0, CHAR_MAX + 1 },
  { "outfile", required_argument, 0, CHAR_MAX + 2 },
  { "ask", optional_argument, 0, CHAR_MAX + 3 },
  { "verify-response", no_argument, 0, 'e' },
  { "request-info", no_argument, 0, 'i' },
  { "response-info", no_argument, 0, 'j' },
  { "generate-request", no_argument, 0, 'q' },
  { "nonce", no_argument, 0, CHAR_MAX + 4 },
  { "no-nonce", no_argument, 0, CHAR_MAX + 5 },
  { "load-chain", required_argument, 0, CHAR_MAX + 6 },
  { "load-issuer", required_argument, 0, CHAR_MAX + 7 },
  { "load-cert", required_argument, 0, CHAR_MAX + 8 },
  { "load-trust", required_argument, 0, CHAR_MAX + 9 },
  { "load-signer", required_argument, 0, CHAR_MAX + 10 },
  { "inder", no_argument, 0, CHAR_MAX + 11 },
  { "no-inder", no_argument, 0, CHAR_MAX + 12 },
  { "outder", no_argument, 0, CHAR_MAX + 13 },
  { "outpem", no_argument, 0, CHAR_MAX + 14 },
  { "load-request", required_argument, 0, 'Q' },
  { "load-response", required_argument, 0, 'S' },
  { "ignore-errors", no_argument, 0, CHAR_MAX + 15 },
  { "verify-allow-broken", no_argument, 0, CHAR_MAX + 16 },
  { "attime", required_argument, 0, CHAR_MAX + 17 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct ocsptool_options *opts = &ocsptool_options;
  int opt;


  while ((opt = getopt_long (argc, argv, "!Q:S:Vd:ehijqv:",
                             long_options, NULL)) != EOF)
    switch (opt)
      {
      case '\0': /* Long option.  */
        break;
      case 'd':
        opts->present.debug = true;
        opts->arg.debug = optarg;
        opts->value.debug = parse_number(optarg);
        opts->enabled.debug = true;
        break;
      case 'V':
        opts->present.verbose = true;
        opts->enabled.verbose = true;
        break;
      case CHAR_MAX + 1: /* --infile */
        opts->present.infile = true;
        opts->arg.infile = optarg;
        opts->enabled.infile = true;
        break;
      case CHAR_MAX + 2: /* --outfile */
        opts->present.outfile = true;
        opts->arg.outfile = optarg;
        opts->enabled.outfile = true;
        break;
      case CHAR_MAX + 3: /* --ask */
        opts->present.ask = true;
        opts->arg.ask = optarg;
        opts->enabled.ask = true;
        break;
      case 'e':
        opts->present.verify_response = true;
        opts->enabled.verify_response = true;
        break;
      case 'i':
        opts->present.request_info = true;
        opts->enabled.request_info = true;
        break;
      case 'j':
        opts->present.response_info = true;
        opts->enabled.response_info = true;
        break;
      case 'q':
        opts->present.generate_request = true;
        opts->enabled.generate_request = true;
        break;
      case CHAR_MAX + 4: /* --nonce */
        opts->present.nonce = true;
        opts->enabled.nonce = true;
        break;
      case CHAR_MAX + 5: /* --no-nonce */
        opts->present.nonce = true;
        opts->enabled.nonce = false;
        break;
      case CHAR_MAX + 6: /* --load-chain */
        opts->present.load_chain = true;
        opts->arg.load_chain = optarg;
        opts->enabled.load_chain = true;
        break;
      case CHAR_MAX + 7: /* --load-issuer */
        opts->present.load_issuer = true;
        opts->arg.load_issuer = optarg;
        opts->enabled.load_issuer = true;
        break;
      case CHAR_MAX + 8: /* --load-cert */
        opts->present.load_cert = true;
        opts->arg.load_cert = optarg;
        opts->enabled.load_cert = true;
        break;
      case CHAR_MAX + 9: /* --load-trust */
        opts->present.load_trust = true;
        opts->arg.load_trust = optarg;
        opts->enabled.load_trust = true;
        break;
      case CHAR_MAX + 10: /* --load-signer */
        opts->present.load_signer = true;
        opts->arg.load_signer = optarg;
        opts->enabled.load_signer = true;
        break;
      case CHAR_MAX + 11: /* --inder */
        opts->present.inder = true;
        opts->enabled.inder = true;
        break;
      case CHAR_MAX + 12: /* --no-inder */
        opts->present.inder = true;
        opts->enabled.inder = false;
        break;
      case CHAR_MAX + 13: /* --outder */
        opts->present.outder = true;
        opts->enabled.outder = true;
        break;
      case CHAR_MAX + 14: /* --outpem */
        opts->present.outpem = true;
        opts->enabled.outpem = true;
        break;
      case 'Q':
        opts->present.load_request = true;
        opts->arg.load_request = optarg;
        opts->enabled.load_request = true;
        break;
      case 'S':
        opts->present.load_response = true;
        opts->arg.load_response = optarg;
        opts->enabled.load_response = true;
        break;
      case CHAR_MAX + 15: /* --ignore-errors */
        opts->present.ignore_errors = true;
        opts->enabled.ignore_errors = true;
        break;
      case CHAR_MAX + 16: /* --verify-allow-broken */
        opts->present.verify_allow_broken = true;
        opts->enabled.verify_allow_broken = true;
        break;
      case CHAR_MAX + 17: /* --attime */
        opts->present.attime = true;
        opts->arg.attime = optarg;
        opts->enabled.attime = true;
        break;
      case 'v':
        opts->present.version = true;
        opts->arg.version = optarg;
        opts->enabled.version = true;
        break;
      case 'h':
        opts->present.help = true;
        opts->enabled.help = true;
        break;
      case '!':
        opts->present.more_help = true;
        opts->enabled.more_help = true;
        break;
      default:
        usage (stderr, EXIT_FAILURE);
        break;
      }

  if (HAVE_OPT(DEBUG) && OPT_VALUE_DEBUG < 0)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "debug", opts->value.debug);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(DEBUG) && OPT_VALUE_DEBUG > 9999)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "debug", opts->value.debug);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(LOAD_TRUST) && HAVE_OPT(LOAD_SIGNER))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "load-trust", "load_signer");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(LOAD_SIGNER) && HAVE_OPT(LOAD_TRUST))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "load-signer", "load_trust");
      exit (EXIT_FAILURE);
    }
  if (optind < argc)
    {
      fprintf (stderr, "Command line arguments are not allowed\n");
      exit (EXIT_FAILURE);
    }


  if (HAVE_OPT(HELP))
    {
      USAGE(0);
    }

  if (HAVE_OPT(MORE_HELP))
#ifdef _WIN32
    {
      USAGE(0);
    }
#else /* _WIN32 */
    {
      pid_t pid;
      int pfds[2];

      if (pipe (pfds) < 0)
        {
          perror ("pipe");
          exit (EXIT_FAILURE);
        }

      pid = fork ();
      if (pid < 0)
        {
          perror ("fork");
          exit (EXIT_FAILURE);
        }

      if (pid == 0)
        {
          close (pfds[0]);
          dup2 (pfds[1], STDOUT_FILENO);
          close (pfds[1]);

          usage (stdout, 0);
        }
      else
        {
          const char *args[2];
          const char *envvar;

          close (pfds[1]);
          dup2 (pfds[0], STDIN_FILENO);
          close (pfds[0]);

          envvar = secure_getenv ("PAGER");
          if (!envvar || *envvar == '\0')
            args[0] = "more";
          else
            args[0] = envvar;

          args[1] = NULL;

          execvp (args[0], (char * const *)args);

          exit (EXIT_FAILURE);
        }
    }
#endif /* !_WIN32 */

  if (HAVE_OPT(VERSION))
    {
      if (!OPT_ARG_VERSION || !strcmp (OPT_ARG_VERSION, "c"))
        {
          const char str[] =
            "ocsptool 3.8.7\n"
            "Copyright (C) 2000-2023 Free Software Foundation, and others\n"
            "This is free software. It is licensed for use, modification and\n"
            "redistribution under the terms of the GNU General Public License,\n"
            "version 3 or later <http://gnu.org/licenses/gpl.html>\n"
            "\n"
            "Please send bug reports to:  <bugs@gnutls.org>            \n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "v"))
        {
          const char str[] =
            "ocsptool 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "ocsptool 3.8.7\n"
            "Copyright (C) 2000-2023 Free Software Foundation, and others\n"
            "This is free software. It is licensed for use, modification and\n"
            "redistribution under the terms of the GNU General Public License,\n"
            "version 3 or later <http://gnu.org/licenses/gpl.html>\n"
            "\n"
            "gnutls is free software: you can redistribute it and/or\n"
            "modify it under the terms of the GNU General Public License\n"
            "as published by the Free Software Foundation,\n"
            "either version 3 of the License, or (at your option) any later version.\n"
            "\n"
            "gnutls is distributed in the hope that it will be useful,\n"
            "but WITHOUT ANY WARRANTY; without even the implied warranty\n"
            "of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
            "See the GNU General Public License for more details.\n"
            "\n"
            "You should have received a copy of the GNU General Public License\n"
            "along with this program.  If not, see <http://www.gnu.org/licenses/>.\n"
            "\n"
            "Please send bug reports to:  <bugs@gnutls.org>            \n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else
        {
          fprintf (stderr,
                   "version option argument 'a' invalid.  Use:\n"
                   "	'v' - version only\n"
                   "	'c' - version and copyright\n"
                   "	'n' - version and full copyright notice\n");
          exit (EXIT_FAILURE);
        }
    }

  return optind;
}

void
usage (FILE *out, int status)
{
  const char str[] =
    "ocsptool - GnuTLS OCSP tool\n"
    "Usage:  ocsptool [ -<flag> [<val>] | --<name>[{=| }<val>] ]... \n"
    "\n"
    "None:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "   -V, --verbose              More verbose output\n"
    "       --infile=file          Input file\n"
    "				- file must pre-exist\n"
    "       --outfile=str          Output file\n"
    "       --ask[=str]            Ask an OCSP/HTTP server on a certificate validity\n"
    "   -e, --verify-response      Verify response\n"
    "   -i, --request-info         Print information on a OCSP request\n"
    "   -j, --response-info        Print information on a OCSP response\n"
    "   -q, --generate-request     Generates an OCSP request\n"
    "       --nonce                Use (or not) a nonce to OCSP request\n"
    "       --load-chain=file      Reads a set of certificates forming a chain from file\n"
    "				- file must pre-exist\n"
    "       --load-issuer=file     Reads issuer's certificate from file\n"
    "				- file must pre-exist\n"
    "       --load-cert=file       Reads the certificate to check from file\n"
    "				- file must pre-exist\n"
    "       --load-trust=file      Read OCSP trust anchors from file\n"
    "				- prohibits the option 'load-signer'\n"
    "				- file must pre-exist\n"
    "       --load-signer=file     Reads the OCSP response signer from file\n"
    "				- prohibits the option 'load-trust'\n"
    "				- file must pre-exist\n"
    "       --inder                Use DER format for input certificates and private keys\n"
    "       --outder               Use DER format for output of responses (this is the default)\n"
    "       --outpem               Use PEM format for output of responses\n"
    "   -Q, --load-request=file    Reads the DER encoded OCSP request from file\n"
    "				- file must pre-exist\n"
    "   -S, --load-response=file   Reads the DER encoded OCSP response from file\n"
    "				- file must pre-exist\n"
    "       --ignore-errors        Ignore any verification errors\n"
    "       --verify-allow-broken  Allow broken algorithms, such as MD5 for verification\n"
    "       --attime=str           Perform validation at the timestamp instead of the system time\n"
    "\n"
    "Version, usage and configuration options:\n"
    "\n"
    "   -v, --version[=arg]        output version information and exit\n"
    "   -h, --help                 display extended usage information and exit\n"
    "   -!, --more-help            extended usage information passed thru pager\n"
    "\n"
    "Options are specified by doubled hyphens and their name or by a single\n"
    "hyphen and the flag character.\n"
    "\n"
    "ocsptool is a program that can parse and print information about\n"
    "OCSP requests/responses, generate requests and verify responses. Unlike\n"
    "other GnuTLS applications it outputs DER encoded structures by default\n"
    "unless the '--outpem' option is specified.\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
