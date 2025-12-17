#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "srptool-options.h"
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

struct srptool_options srptool_options;

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
  { "index", required_argument, 0, 'i' },
  { "username", required_argument, 0, 'u' },
  { "passwd", required_argument, 0, 'p' },
  { "salt", required_argument, 0, 's' },
  { "verify", no_argument, 0, CHAR_MAX + 1 },
  { "passwd-conf", required_argument, 0, 'v' },
  { "create-conf", required_argument, 0, CHAR_MAX + 2 },
  { "version", optional_argument, 0, CHAR_MAX + 3 },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

/* Default options.  */
static const char *index_default = "3";

int
process_options (int argc, char **argv)
{
  struct srptool_options *opts = &srptool_options;
  int opt;

  opts->arg.index = index_default;
  opts->value.index = 3;

  while ((opt = getopt_long (argc, argv, "!d:hi:p:s:u:v:",
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
      case 'i':
        opts->present.index = true;
        opts->arg.index = optarg;
        opts->value.index = parse_number(optarg);
        opts->enabled.index = true;
        break;
      case 'u':
        opts->present.username = true;
        opts->arg.username = optarg;
        opts->enabled.username = true;
        break;
      case 'p':
        opts->present.passwd = true;
        opts->arg.passwd = optarg;
        opts->enabled.passwd = true;
        break;
      case 's':
        opts->present.salt = true;
        opts->arg.salt = optarg;
        opts->value.salt = parse_number(optarg);
        opts->enabled.salt = true;
        break;
      case CHAR_MAX + 1: /* --verify */
        opts->present.verify = true;
        opts->enabled.verify = true;
        break;
      case 'v':
        opts->present.passwd_conf = true;
        opts->arg.passwd_conf = optarg;
        opts->enabled.passwd_conf = true;
        break;
      case CHAR_MAX + 2: /* --create-conf */
        opts->present.create_conf = true;
        opts->arg.create_conf = optarg;
        opts->enabled.create_conf = true;
        break;
      case CHAR_MAX + 3: /* --version */
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
            "srptool 3.8.7\n"
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
            "srptool 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "srptool 3.8.7\n"
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
    "srptool - GnuTLS SRP tool\n"
    "Usage:  srptool [ -<flag> [<val>] | --<name>[{=| }<val>] ]... \n"
    "\n"
    "None:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "   -i, --index=num            specify the index of the group parameters in tpasswd.conf to use\n"
    "   -u, --username=str         specify a username\n"
    "   -p, --passwd=str           specify a password file\n"
    "   -s, --salt=num             specify salt size\n"
    "       --verify               just verify the password\n"
    "   -v, --passwd-conf=str      specify a password conf file\n"
    "       --create-conf=str      Generate a password configuration file\n"
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
    "Simple program that emulates the programs in the Stanford SRP (Secure\n"
    "Remote Password) libraries using GnuTLS.  It is intended for use in  places\n"
    "where you don't expect SRP authentication to be the used for system users.\n"
    "\n"
    "In  brief,  to use SRP you need to create two files. These are the password\n"
    "file that holds the users and the verifiers associated with  them  and  the\n"
    "configuration file to hold the group parameters (called tpasswd.conf).\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
