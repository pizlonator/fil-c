#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "danetool-options.h"
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

struct danetool_options danetool_options;

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
  { "outfile", required_argument, 0, CHAR_MAX + 1 },
  { "load-pubkey", required_argument, 0, CHAR_MAX + 2 },
  { "load-certificate", required_argument, 0, CHAR_MAX + 3 },
  { "dlv", required_argument, 0, CHAR_MAX + 4 },
  { "hash", required_argument, 0, CHAR_MAX + 5 },
  { "check", required_argument, 0, CHAR_MAX + 6 },
  { "check-ee", no_argument, 0, CHAR_MAX + 7 },
  { "check-ca", no_argument, 0, CHAR_MAX + 8 },
  { "tlsa-rr", no_argument, 0, CHAR_MAX + 9 },
  { "host", required_argument, 0, CHAR_MAX + 10 },
  { "proto", required_argument, 0, CHAR_MAX + 11 },
  { "port", required_argument, 0, CHAR_MAX + 12 },
  { "starttls-proto", required_argument, 0, CHAR_MAX + 14 },
  { "app-proto", required_argument, 0, CHAR_MAX + 13 },
  { "ca", no_argument, 0, CHAR_MAX + 15 },
  { "x509", no_argument, 0, CHAR_MAX + 16 },
  { "domain", no_argument, 0, CHAR_MAX + 18 },
  { "local", no_argument, 0, CHAR_MAX + 17 },
  { "no-domain", no_argument, 0, CHAR_MAX + 19 },
  { "local-dns", no_argument, 0, CHAR_MAX + 20 },
  { "no-local-dns", no_argument, 0, CHAR_MAX + 21 },
  { "insecure", no_argument, 0, CHAR_MAX + 22 },
  { "inder", no_argument, 0, CHAR_MAX + 23 },
  { "inraw", no_argument, 0, CHAR_MAX + 25 },
  { "no-inder", no_argument, 0, CHAR_MAX + 24 },
  { "print-raw", no_argument, 0, CHAR_MAX + 26 },
  { "no-print-raw", no_argument, 0, CHAR_MAX + 27 },
  { "quiet", no_argument, 0, CHAR_MAX + 28 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct danetool_options *opts = &danetool_options;
  int opt;

  opts->enabled.domain = true;
  while ((opt = getopt_long (argc, argv, "!Vd:hv:",
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
      case CHAR_MAX + 1: /* --outfile */
        opts->present.outfile = true;
        opts->arg.outfile = optarg;
        opts->enabled.outfile = true;
        break;
      case CHAR_MAX + 2: /* --load-pubkey */
        opts->present.load_pubkey = true;
        opts->arg.load_pubkey = optarg;
        opts->enabled.load_pubkey = true;
        break;
      case CHAR_MAX + 3: /* --load-certificate */
        opts->present.load_certificate = true;
        opts->arg.load_certificate = optarg;
        opts->enabled.load_certificate = true;
        break;
      case CHAR_MAX + 4: /* --dlv */
        opts->present.dlv = true;
        opts->arg.dlv = optarg;
        opts->enabled.dlv = true;
        break;
      case CHAR_MAX + 5: /* --hash */
        opts->present.hash = true;
        opts->arg.hash = optarg;
        opts->enabled.hash = true;
        break;
      case CHAR_MAX + 6: /* --check */
        opts->present.check = true;
        opts->arg.check = optarg;
        opts->enabled.check = true;
        break;
      case CHAR_MAX + 7: /* --check-ee */
        opts->present.check_ee = true;
        opts->enabled.check_ee = true;
        break;
      case CHAR_MAX + 8: /* --check-ca */
        opts->present.check_ca = true;
        opts->enabled.check_ca = true;
        break;
      case CHAR_MAX + 9: /* --tlsa-rr */
        opts->present.tlsa_rr = true;
        opts->enabled.tlsa_rr = true;
        break;
      case CHAR_MAX + 10: /* --host */
        opts->present.host = true;
        opts->arg.host = optarg;
        opts->enabled.host = true;
        break;
      case CHAR_MAX + 11: /* --proto */
        opts->present.proto = true;
        opts->arg.proto = optarg;
        opts->enabled.proto = true;
        break;
      case CHAR_MAX + 12: /* --port */
        opts->present.port = true;
        opts->arg.port = optarg;
        opts->enabled.port = true;
        break;
      case CHAR_MAX + 14: /* --starttls-proto */
      case CHAR_MAX + 13: /* --app-proto */
        opts->present.starttls_proto = true;
        opts->arg.starttls_proto = optarg;
        opts->enabled.starttls_proto = true;
        break;
      case CHAR_MAX + 15: /* --ca */
        opts->present.ca = true;
        opts->enabled.ca = true;
        break;
      case CHAR_MAX + 16: /* --x509 */
        opts->present.x509 = true;
        opts->enabled.x509 = true;
        break;
      case CHAR_MAX + 18: /* --domain */
      case CHAR_MAX + 17: /* --local */
        opts->present.domain = true;
        opts->enabled.domain = true;
        break;
      case CHAR_MAX + 19: /* --no-domain */
        opts->present.domain = true;
        opts->enabled.domain = false;
        break;
      case CHAR_MAX + 20: /* --local-dns */
        opts->present.local_dns = true;
        opts->enabled.local_dns = true;
        break;
      case CHAR_MAX + 21: /* --no-local-dns */
        opts->present.local_dns = true;
        opts->enabled.local_dns = false;
        break;
      case CHAR_MAX + 22: /* --insecure */
        opts->present.insecure = true;
        opts->enabled.insecure = true;
        break;
      case CHAR_MAX + 23: /* --inder */
      case CHAR_MAX + 25: /* --inraw */
        opts->present.inder = true;
        opts->enabled.inder = true;
        break;
      case CHAR_MAX + 24: /* --no-inder */
        opts->present.inder = true;
        opts->enabled.inder = false;
        break;
      case CHAR_MAX + 26: /* --print-raw */
        opts->present.print_raw = true;
        opts->enabled.print_raw = true;
        break;
      case CHAR_MAX + 27: /* --no-print-raw */
        opts->present.print_raw = true;
        opts->enabled.print_raw = false;
        break;
      case CHAR_MAX + 28: /* --quiet */
        opts->present.quiet = true;
        opts->enabled.quiet = true;
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
  if (HAVE_OPT(TLSA_RR) && !HAVE_OPT(HOST))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "tlsa-rr", "host");
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
            "danetool 3.8.7\n"
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
            "danetool 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "danetool 3.8.7\n"
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
    "danetool - GnuTLS DANE tool\n"
    "Usage:  danetool [ -<flag> [<val>] | --<name>[{=| }<val>] ]... \n"
    "\n"
    "None:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "   -V, --verbose              More verbose output\n"
    "       --outfile=str          Output file\n"
    "       --load-pubkey=str      Loads a public key file\n"
    "       --load-certificate=str Loads a certificate file\n"
    "       --dlv=str              Sets a DLV file\n"
    "       --hash=str             Hash algorithm to use for signing\n"
    "       --check=str            Check a host's DANE TLSA entry\n"
    "       --check-ee             Check only the end-entity's certificate\n"
    "       --check-ca             Check only the CA's certificate\n"
    "       --tlsa-rr              Print the DANE RR data on a certificate or public key\n"
    "				- requires the option 'host'\n"
    "       --host=str             Specify the hostname to be used in the DANE RR\n"
    "       --proto=str            The protocol set for DANE data (tcp, udp etc.)\n"
    "       --port=str             The port or service to connect to, for DANE data\n"
    "       --app-proto            an alias for the 'starttls-proto' option\n"
    "       --starttls-proto=str   The application protocol to be used to obtain the server's certificate (https, ftp, smtp, imap, ldap, xmpp, lmtp, pop3, nntp, sieve, postgres)\n"
    "       --ca                   Whether the provided certificate or public key is a Certificate Authority\n"
    "       --x509                 Use the hash of the X.509 certificate, rather than the public key\n"
    "       --local                an alias for the 'domain' option\n"
    "       --domain               The provided certificate or public key is issued by the local domain\n"
    "				- enabled by default\n"
    "				- disabled as '--no-domain'\n"
    "       --local-dns            Use the local DNS server for DNSSEC resolving\n"
    "       --insecure             Do not verify any DNSSEC signature\n"
    "       --inder                Use DER format for input certificates and private keys\n"
    "       --inraw                an alias for the 'inder' option\n"
    "       --print-raw            Print the received DANE data in raw format\n"
    "       --quiet                Suppress several informational messages\n"
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
    "Tool to generate and check DNS resource records for the DANE protocol.\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
