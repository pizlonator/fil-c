#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gnutls-serv-options.h"
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

struct gnutls_serv_options gnutls_serv_options;

/* Copied from xsize.h in Gnulib */

/* Sum of two sizes, with overflow check.  */
static inline size_t
xsum (size_t size1, size_t size2)
{
  size_t sum = size1 + size2;
  return (sum >= size1 ? sum : SIZE_MAX);
}

/* Check for overflow.  */
#define size_overflow_p(SIZE)   ((SIZE) == SIZE_MAX)

static void
append_to_list (struct gnutls_serv_list *list,
                const char *name, const char *arg)
{
  const char **tmp;
  size_t new_count = xsum (list->count, 1);

  if (size_overflow_p (new_count))
    {
      fprintf (stderr, "too many arguments for %s\n",
               name);
      exit (EXIT_FAILURE);
    }

  tmp = reallocarray (list->args, new_count, sizeof (char *));
  if (!tmp)
    {
      fprintf (stderr, "unable to allocate memory for %s\n",
               name);
      exit (EXIT_FAILURE);
    }

  list->args = tmp;
  list->args[list->count] = optarg;
  list->count = new_count;
}

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
  { "sni-hostname", required_argument, 0, CHAR_MAX + 1 },
  { "sni-hostname-fatal", no_argument, 0, CHAR_MAX + 2 },
  { "alpn", required_argument, 0, CHAR_MAX + 3 },
  { "alpn-fatal", no_argument, 0, CHAR_MAX + 4 },
  { "noticket", no_argument, 0, CHAR_MAX + 5 },
  { "earlydata", no_argument, 0, CHAR_MAX + 6 },
  { "maxearlydata", required_argument, 0, CHAR_MAX + 7 },
  { "nocookie", no_argument, 0, CHAR_MAX + 8 },
  { "generate", no_argument, 0, 'g' },
  { "quiet", no_argument, 0, 'q' },
  { "nodb", no_argument, 0, CHAR_MAX + 9 },
  { "http", no_argument, 0, CHAR_MAX + 10 },
  { "echo", no_argument, 0, CHAR_MAX + 11 },
  { "crlf", no_argument, 0, CHAR_MAX + 12 },
  { "udp", no_argument, 0, 'u' },
  { "mtu", required_argument, 0, CHAR_MAX + 13 },
  { "srtp-profiles", required_argument, 0, CHAR_MAX + 14 },
  { "disable-client-cert", no_argument, 0, 'a' },
  { "require-client-cert", no_argument, 0, 'r' },
  { "verify-client-cert", no_argument, 0, CHAR_MAX + 15 },
  { "compress-cert", required_argument, 0, CHAR_MAX + 16 },
  { "heartbeat", no_argument, 0, 'b' },
  { "x509fmtder", no_argument, 0, CHAR_MAX + 17 },
  { "priority", required_argument, 0, CHAR_MAX + 18 },
  { "dhparams", required_argument, 0, CHAR_MAX + 19 },
  { "x509cafile", required_argument, 0, CHAR_MAX + 20 },
  { "x509crlfile", required_argument, 0, CHAR_MAX + 21 },
  { "pgpkeyfile", required_argument, 0, CHAR_MAX + 22 },
  { "x509keyfile", required_argument, 0, CHAR_MAX + 23 },
  { "x509dsakeyfile", required_argument, 0, CHAR_MAX + 25 },
  { "x509ecckeyfile", required_argument, 0, CHAR_MAX + 27 },
  { "x509certfile", required_argument, 0, CHAR_MAX + 24 },
  { "x509dsacertfile", required_argument, 0, CHAR_MAX + 26 },
  { "x509ecccertfile", required_argument, 0, CHAR_MAX + 28 },
  { "rawpkkeyfile", required_argument, 0, CHAR_MAX + 29 },
  { "rawpkfile", required_argument, 0, CHAR_MAX + 30 },
  { "srppasswd", required_argument, 0, CHAR_MAX + 31 },
  { "srppasswdconf", required_argument, 0, CHAR_MAX + 32 },
  { "pskpasswd", required_argument, 0, CHAR_MAX + 33 },
  { "pskhint", required_argument, 0, CHAR_MAX + 34 },
  { "ocsp-response", required_argument, 0, CHAR_MAX + 35 },
  { "ignore-ocsp-response-errors", no_argument, 0, CHAR_MAX + 36 },
  { "port", required_argument, 0, 'p' },
  { "list", no_argument, 0, 'l' },
  { "provider", required_argument, 0, CHAR_MAX + 37 },
  { "keymatexport", required_argument, 0, CHAR_MAX + 38 },
  { "keymatexportsize", required_argument, 0, CHAR_MAX + 39 },
  { "recordsize", required_argument, 0, CHAR_MAX + 40 },
  { "httpdata", required_argument, 0, CHAR_MAX + 41 },
  { "timeout", required_argument, 0, CHAR_MAX + 42 },
  { "attime", required_argument, 0, CHAR_MAX + 43 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct gnutls_serv_options *opts = &gnutls_serv_options;
  int opt;


  while ((opt = getopt_long (argc, argv, "!abd:ghlp:qruv:",
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
      case CHAR_MAX + 1: /* --sni-hostname */
        opts->present.sni_hostname = true;
        opts->arg.sni_hostname = optarg;
        opts->enabled.sni_hostname = true;
        break;
      case CHAR_MAX + 2: /* --sni-hostname-fatal */
        opts->present.sni_hostname_fatal = true;
        opts->enabled.sni_hostname_fatal = true;
        break;
      case CHAR_MAX + 3: /* --alpn */
        opts->present.alpn = true;
        append_to_list (&opts->list.alpn, "alpn", optarg);
        opts->enabled.alpn = true;
        break;
      case CHAR_MAX + 4: /* --alpn-fatal */
        opts->present.alpn_fatal = true;
        opts->enabled.alpn_fatal = true;
        break;
      case CHAR_MAX + 5: /* --noticket */
        opts->present.noticket = true;
        opts->enabled.noticket = true;
        break;
      case CHAR_MAX + 6: /* --earlydata */
        opts->present.earlydata = true;
        opts->enabled.earlydata = true;
        break;
      case CHAR_MAX + 7: /* --maxearlydata */
        opts->present.maxearlydata = true;
        opts->arg.maxearlydata = optarg;
        opts->value.maxearlydata = parse_number(optarg);
        opts->enabled.maxearlydata = true;
        break;
      case CHAR_MAX + 8: /* --nocookie */
        opts->present.nocookie = true;
        opts->enabled.nocookie = true;
        break;
      case 'g':
        opts->present.generate = true;
        opts->enabled.generate = true;
        break;
      case 'q':
        opts->present.quiet = true;
        opts->enabled.quiet = true;
        break;
      case CHAR_MAX + 9: /* --nodb */
        opts->present.nodb = true;
        opts->enabled.nodb = true;
        break;
      case CHAR_MAX + 10: /* --http */
        opts->present.http = true;
        opts->enabled.http = true;
        break;
      case CHAR_MAX + 11: /* --echo */
        opts->present.echo = true;
        opts->enabled.echo = true;
        break;
      case CHAR_MAX + 12: /* --crlf */
        opts->present.crlf = true;
        opts->enabled.crlf = true;
        break;
      case 'u':
        opts->present.udp = true;
        opts->enabled.udp = true;
        break;
      case CHAR_MAX + 13: /* --mtu */
        opts->present.mtu = true;
        opts->arg.mtu = optarg;
        opts->value.mtu = parse_number(optarg);
        opts->enabled.mtu = true;
        break;
      case CHAR_MAX + 14: /* --srtp-profiles */
        opts->present.srtp_profiles = true;
        opts->arg.srtp_profiles = optarg;
        opts->enabled.srtp_profiles = true;
        break;
      case 'a':
        opts->present.disable_client_cert = true;
        opts->enabled.disable_client_cert = true;
        break;
      case 'r':
        opts->present.require_client_cert = true;
        opts->enabled.require_client_cert = true;
        break;
      case CHAR_MAX + 15: /* --verify-client-cert */
        opts->present.verify_client_cert = true;
        opts->enabled.verify_client_cert = true;
        break;
      case CHAR_MAX + 16: /* --compress-cert */
        opts->present.compress_cert = true;
        append_to_list (&opts->list.compress_cert, "compress-cert", optarg);
        opts->enabled.compress_cert = true;
        break;
      case 'b':
        opts->present.heartbeat = true;
        opts->enabled.heartbeat = true;
        break;
      case CHAR_MAX + 17: /* --x509fmtder */
        opts->present.x509fmtder = true;
        opts->enabled.x509fmtder = true;
        break;
      case CHAR_MAX + 18: /* --priority */
        opts->present.priority = true;
        opts->arg.priority = optarg;
        opts->enabled.priority = true;
        break;
      case CHAR_MAX + 19: /* --dhparams */
        opts->present.dhparams = true;
        opts->arg.dhparams = optarg;
        opts->enabled.dhparams = true;
        break;
      case CHAR_MAX + 20: /* --x509cafile */
        opts->present.x509cafile = true;
        opts->arg.x509cafile = optarg;
        opts->enabled.x509cafile = true;
        break;
      case CHAR_MAX + 21: /* --x509crlfile */
        opts->present.x509crlfile = true;
        opts->arg.x509crlfile = optarg;
        opts->enabled.x509crlfile = true;
        break;
      case CHAR_MAX + 22: /* --pgpkeyfile */
        opts->present.pgpkeyfile = true;
        opts->arg.pgpkeyfile = optarg;
        opts->enabled.pgpkeyfile = true;
        break;
      case CHAR_MAX + 23: /* --x509keyfile */
      case CHAR_MAX + 25: /* --x509dsakeyfile */
      case CHAR_MAX + 27: /* --x509ecckeyfile */
        opts->present.x509keyfile = true;
        append_to_list (&opts->list.x509keyfile, "x509keyfile", optarg);
        opts->enabled.x509keyfile = true;
        break;
      case CHAR_MAX + 24: /* --x509certfile */
      case CHAR_MAX + 26: /* --x509dsacertfile */
      case CHAR_MAX + 28: /* --x509ecccertfile */
        opts->present.x509certfile = true;
        append_to_list (&opts->list.x509certfile, "x509certfile", optarg);
        opts->enabled.x509certfile = true;
        break;
      case CHAR_MAX + 29: /* --rawpkkeyfile */
        opts->present.rawpkkeyfile = true;
        append_to_list (&opts->list.rawpkkeyfile, "rawpkkeyfile", optarg);
        opts->enabled.rawpkkeyfile = true;
        break;
      case CHAR_MAX + 30: /* --rawpkfile */
        opts->present.rawpkfile = true;
        append_to_list (&opts->list.rawpkfile, "rawpkfile", optarg);
        opts->enabled.rawpkfile = true;
        break;
      case CHAR_MAX + 31: /* --srppasswd */
        opts->present.srppasswd = true;
        opts->arg.srppasswd = optarg;
        opts->enabled.srppasswd = true;
        break;
      case CHAR_MAX + 32: /* --srppasswdconf */
        opts->present.srppasswdconf = true;
        opts->arg.srppasswdconf = optarg;
        opts->enabled.srppasswdconf = true;
        break;
      case CHAR_MAX + 33: /* --pskpasswd */
        opts->present.pskpasswd = true;
        opts->arg.pskpasswd = optarg;
        opts->enabled.pskpasswd = true;
        break;
      case CHAR_MAX + 34: /* --pskhint */
        opts->present.pskhint = true;
        opts->arg.pskhint = optarg;
        opts->enabled.pskhint = true;
        break;
      case CHAR_MAX + 35: /* --ocsp-response */
        opts->present.ocsp_response = true;
        append_to_list (&opts->list.ocsp_response, "ocsp-response", optarg);
        opts->enabled.ocsp_response = true;
        break;
      case CHAR_MAX + 36: /* --ignore-ocsp-response-errors */
        opts->present.ignore_ocsp_response_errors = true;
        opts->enabled.ignore_ocsp_response_errors = true;
        break;
      case 'p':
        opts->present.port = true;
        opts->arg.port = optarg;
        opts->value.port = parse_number(optarg);
        opts->enabled.port = true;
        break;
      case 'l':
        opts->present.list = true;
        opts->enabled.list = true;
        break;
      case CHAR_MAX + 37: /* --provider */
        opts->present.provider = true;
        opts->arg.provider = optarg;
        opts->enabled.provider = true;
        break;
      case CHAR_MAX + 38: /* --keymatexport */
        opts->present.keymatexport = true;
        opts->arg.keymatexport = optarg;
        opts->enabled.keymatexport = true;
        break;
      case CHAR_MAX + 39: /* --keymatexportsize */
        opts->present.keymatexportsize = true;
        opts->arg.keymatexportsize = optarg;
        opts->value.keymatexportsize = parse_number(optarg);
        opts->enabled.keymatexportsize = true;
        break;
      case CHAR_MAX + 40: /* --recordsize */
        opts->present.recordsize = true;
        opts->arg.recordsize = optarg;
        opts->value.recordsize = parse_number(optarg);
        opts->enabled.recordsize = true;
        break;
      case CHAR_MAX + 41: /* --httpdata */
        opts->present.httpdata = true;
        opts->arg.httpdata = optarg;
        opts->enabled.httpdata = true;
        break;
      case CHAR_MAX + 42: /* --timeout */
        opts->present.timeout = true;
        opts->arg.timeout = optarg;
        opts->value.timeout = parse_number(optarg);
        opts->enabled.timeout = true;
        break;
      case CHAR_MAX + 43: /* --attime */
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
  if (HAVE_OPT(MAXEARLYDATA) && OPT_VALUE_MAXEARLYDATA < 1)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "maxearlydata", opts->value.maxearlydata);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(MAXEARLYDATA) && OPT_VALUE_MAXEARLYDATA > 2147483648)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "maxearlydata", opts->value.maxearlydata);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(MTU) && OPT_VALUE_MTU < 0)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "mtu", opts->value.mtu);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(MTU) && OPT_VALUE_MTU > 17000)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "mtu", opts->value.mtu);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(DISABLE_CLIENT_CERT) && HAVE_OPT(REQUIRE_CLIENT_CERT))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "disable-client-cert", "require_client_cert");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(RAWPKFILE) && !HAVE_OPT(RAWPKKEYFILE))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "rawpkfile", "rawpkkeyfile");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(RECORDSIZE) && OPT_VALUE_RECORDSIZE < 0)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "recordsize", opts->value.recordsize);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(RECORDSIZE) && OPT_VALUE_RECORDSIZE > 16384)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "recordsize", opts->value.recordsize);
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
            "gnutls-serv 3.8.7\n"
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
            "gnutls-serv 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "gnutls-serv 3.8.7\n"
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
    "gnutls-serv - GnuTLS server\n"
    "Usage:  gnutls-serv [ -<flag> [<val>] | --<name>[{=| }<val>] ]... \n"
    "\n"
    "None:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "       --sni-hostname=str     Server's hostname for server name extension\n"
    "       --sni-hostname-fatal   Send fatal alert on sni-hostname mismatch\n"
    "       --alpn=str             Specify ALPN protocol to be enabled by the server\n"
    "       --alpn-fatal           Send fatal alert on non-matching ALPN name\n"
    "       --noticket             Don't accept session tickets\n"
    "       --earlydata            Accept early data\n"
    "       --maxearlydata=num     The maximum early data size to accept\n"
    "				- it must be in the range:\n"
    "				  1 to 2147483648\n"
    "       --nocookie             Don't require cookie on DTLS sessions\n"
    "   -g, --generate             Generate Diffie-Hellman parameters\n"
    "   -q, --quiet                Suppress some messages\n"
    "       --nodb                 Do not use a resumption database\n"
    "       --http                 Act as an HTTP server\n"
    "       --echo                 Act as an Echo server\n"
    "       --crlf                 Do not replace CRLF by LF in Echo server mode\n"
    "   -u, --udp                  Use DTLS (datagram TLS) over UDP\n"
    "       --mtu=num              Set MTU for datagram TLS\n"
    "				- it must be in the range:\n"
    "				  0 to 17000\n"
    "       --srtp-profiles=str    Offer SRTP profiles\n"
    "   -a, --disable-client-cert  Do not request a client certificate\n"
    "				- prohibits the option 'require-client-cert'\n"
    "   -r, --require-client-cert  Require a client certificate\n"
    "       --verify-client-cert   If a client certificate is sent then verify it\n"
    "       --compress-cert=str    Compress certificate\n"
    "   -b, --heartbeat            Activate heartbeat support\n"
    "       --x509fmtder           Use DER format for certificates to read from\n"
    "       --priority=str         Priorities string\n"
    "       --dhparams=file        DH params file to use\n"
    "				- file must pre-exist\n"
    "       --x509cafile=str       Certificate file or PKCS #11 URL to use\n"
    "       --x509crlfile=file     CRL file to use\n"
    "				- file must pre-exist\n"
    "       --x509keyfile=str      X.509 key file or PKCS #11 URL to use\n"
    "       --x509certfile=str     X.509 Certificate file or PKCS #11 URL to use\n"
    "       --rawpkkeyfile=str     Private key file (PKCS #8 or PKCS #12) or PKCS #11 URL to use\n"
    "       --rawpkfile=str        Raw public-key file to use\n"
    "				- requires the option 'rawpkkeyfile'\n"
    "       --srppasswd=file       SRP password file to use\n"
    "				- file must pre-exist\n"
    "       --srppasswdconf=file   SRP password configuration file to use\n"
    "				- file must pre-exist\n"
    "       --pskpasswd=file       PSK password file to use\n"
    "				- file must pre-exist\n"
    "       --pskhint=str          PSK identity hint to use\n"
    "       --ocsp-response=str    The OCSP response to send to client\n"
    "       --ignore-ocsp-response-errors  Ignore any errors when setting the OCSP response\n"
    "   -p, --port=num             The port to connect to\n"
    "   -l, --list                 Print a list of the supported algorithms and modes\n"
    "       --provider=file        Specify the PKCS #11 provider library\n"
    "				- file must pre-exist\n"
    "       --keymatexport=str     Label used for exporting keying material\n"
    "       --keymatexportsize=num Size of the exported keying material\n"
    "       --recordsize=num       The maximum record size to advertise\n"
    "				- it must be in the range:\n"
    "				  0 to 16384\n"
    "       --httpdata=file        The data used as HTTP response\n"
    "				- file must pre-exist\n"
    "       --timeout=num          The timeout period for server\n"
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
    "Server program that listens to incoming TLS connections.\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
