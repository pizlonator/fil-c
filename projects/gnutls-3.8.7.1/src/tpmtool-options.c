#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tpmtool-options.h"
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

struct tpmtool_options tpmtool_options;

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
  { "infile", required_argument, 0, CHAR_MAX + 1 },
  { "outfile", required_argument, 0, CHAR_MAX + 2 },
  { "generate-rsa", no_argument, 0, CHAR_MAX + 3 },
  { "register", no_argument, 0, CHAR_MAX + 4 },
  { "signing", no_argument, 0, CHAR_MAX + 5 },
  { "legacy", no_argument, 0, CHAR_MAX + 6 },
  { "user", no_argument, 0, CHAR_MAX + 7 },
  { "system", no_argument, 0, CHAR_MAX + 8 },
  { "pubkey", required_argument, 0, CHAR_MAX + 9 },
  { "list", no_argument, 0, CHAR_MAX + 10 },
  { "delete", required_argument, 0, CHAR_MAX + 11 },
  { "test-sign", required_argument, 0, CHAR_MAX + 12 },
  { "sec-param", required_argument, 0, CHAR_MAX + 13 },
  { "bits", required_argument, 0, CHAR_MAX + 14 },
  { "inder", no_argument, 0, CHAR_MAX + 15 },
  { "no-inder", no_argument, 0, CHAR_MAX + 16 },
  { "outder", no_argument, 0, CHAR_MAX + 17 },
  { "no-outder", no_argument, 0, CHAR_MAX + 18 },
  { "srk-well-known", no_argument, 0, CHAR_MAX + 19 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct tpmtool_options *opts = &tpmtool_options;
  int opt;


  while ((opt = getopt_long (argc, argv, "!d:hv:",
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
      case CHAR_MAX + 3: /* --generate-rsa */
        opts->present.generate_rsa = true;
        opts->enabled.generate_rsa = true;
        break;
      case CHAR_MAX + 4: /* --register */
        opts->present.register_ = true;
        opts->enabled.register_ = true;
        break;
      case CHAR_MAX + 5: /* --signing */
        opts->present.signing = true;
        opts->enabled.signing = true;
        break;
      case CHAR_MAX + 6: /* --legacy */
        opts->present.legacy = true;
        opts->enabled.legacy = true;
        break;
      case CHAR_MAX + 7: /* --user */
        opts->present.user = true;
        opts->enabled.user = true;
        break;
      case CHAR_MAX + 8: /* --system */
        opts->present.system = true;
        opts->enabled.system = true;
        break;
      case CHAR_MAX + 9: /* --pubkey */
        opts->present.pubkey = true;
        opts->arg.pubkey = optarg;
        opts->enabled.pubkey = true;
        break;
      case CHAR_MAX + 10: /* --list */
        opts->present.list = true;
        opts->enabled.list = true;
        break;
      case CHAR_MAX + 11: /* --delete */
        opts->present.delete = true;
        opts->arg.delete = optarg;
        opts->enabled.delete = true;
        break;
      case CHAR_MAX + 12: /* --test-sign */
        opts->present.test_sign = true;
        opts->arg.test_sign = optarg;
        opts->enabled.test_sign = true;
        break;
      case CHAR_MAX + 13: /* --sec-param */
        opts->present.sec_param = true;
        opts->arg.sec_param = optarg;
        opts->enabled.sec_param = true;
        break;
      case CHAR_MAX + 14: /* --bits */
        opts->present.bits = true;
        opts->arg.bits = optarg;
        opts->value.bits = parse_number(optarg);
        opts->enabled.bits = true;
        break;
      case CHAR_MAX + 15: /* --inder */
        opts->present.inder = true;
        opts->enabled.inder = true;
        break;
      case CHAR_MAX + 16: /* --no-inder */
        opts->present.inder = true;
        opts->enabled.inder = false;
        break;
      case CHAR_MAX + 17: /* --outder */
        opts->present.outder = true;
        opts->enabled.outder = true;
        break;
      case CHAR_MAX + 18: /* --no-outder */
        opts->present.outder = true;
        opts->enabled.outder = false;
        break;
      case CHAR_MAX + 19: /* --srk-well-known */
        opts->present.srk_well_known = true;
        opts->enabled.srk_well_known = true;
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
  if (HAVE_OPT(REGISTER) && !HAVE_OPT(GENERATE_RSA))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "register", "generate_rsa");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SIGNING) && HAVE_OPT(LEGACY))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "signing", "legacy");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SIGNING) && !HAVE_OPT(GENERATE_RSA))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "signing", "generate_rsa");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(LEGACY) && HAVE_OPT(SIGNING))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "legacy", "signing");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(LEGACY) && !HAVE_OPT(GENERATE_RSA))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "legacy", "generate_rsa");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(USER) && HAVE_OPT(SYSTEM))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "user", "system");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(USER) && !HAVE_OPT(REGISTER))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "user", "register");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SYSTEM) && HAVE_OPT(USER))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "system", "user");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SYSTEM) && !HAVE_OPT(REGISTER))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "system", "register");
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
            "tpmtool 3.8.7\n"
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
            "tpmtool 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "tpmtool 3.8.7\n"
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
    "tpmtool - GnuTLS TPM tool\n"
    "Usage:  tpmtool [ -<flag> [<val>] | --<name>[{=| }<val>] ]... \n"
    "\n"
    "None:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "       --infile=file          Input file\n"
    "				- file must pre-exist\n"
    "       --outfile=str          Output file\n"
    "       --generate-rsa         Generate an RSA private-public key pair\n"
    "       --register             Any generated key will be registered in the TPM\n"
    "				- requires the option 'generate-rsa'\n"
    "       --signing              Any generated key will be a signing key\n"
    "				- prohibits the option 'legacy'\n"
    "				- requires the option 'generate-rsa'\n"
    "       --legacy               Any generated key will be a legacy key\n"
    "				- prohibits the option 'signing'\n"
    "				- requires the option 'generate-rsa'\n"
    "       --user                 Any registered key will be a user key\n"
    "				- prohibits the option 'system'\n"
    "				- requires the option 'register'\n"
    "       --system               Any registered key will be a system key\n"
    "				- prohibits the option 'user'\n"
    "				- requires the option 'register'\n"
    "       --pubkey=str           Prints the public key of the provided key\n"
    "       --list                 Lists all stored keys in the TPM\n"
    "       --delete=str           Delete the key identified by the given URL (UUID)\n"
    "       --test-sign=str        Tests the signature operation of the provided object\n"
    "       --sec-param=str        Specify the security level [low, legacy, medium, high, ultra]\n"
    "       --bits=num             Specify the number of bits for key generate\n"
    "       --inder                Use the DER format for keys\n"
    "       --outder               Use DER format for output keys\n"
    "       --srk-well-known       SRK has well known password (20 bytes of zeros)\n"
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
    "Program that allows handling cryptographic data from the TPM chip.\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
