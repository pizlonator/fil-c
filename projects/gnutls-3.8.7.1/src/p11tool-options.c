#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "p11tool-options.h"
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

struct p11tool_options p11tool_options;

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
  { "list-tokens", no_argument, 0, CHAR_MAX + 1 },
  { "list-token-urls", no_argument, 0, CHAR_MAX + 2 },
  { "list-mechanisms", no_argument, 0, CHAR_MAX + 3 },
  { "initialize", no_argument, 0, CHAR_MAX + 4 },
  { "initialize-pin", no_argument, 0, CHAR_MAX + 5 },
  { "initialize-so-pin", no_argument, 0, CHAR_MAX + 6 },
  { "set-pin", required_argument, 0, CHAR_MAX + 7 },
  { "set-so-pin", required_argument, 0, CHAR_MAX + 8 },
  { "list-all", no_argument, 0, CHAR_MAX + 9 },
  { "list-all-certs", no_argument, 0, CHAR_MAX + 10 },
  { "list-certs", no_argument, 0, CHAR_MAX + 11 },
  { "list-all-privkeys", no_argument, 0, CHAR_MAX + 12 },
  { "list-privkeys", no_argument, 0, CHAR_MAX + 13 },
  { "list-keys", no_argument, 0, CHAR_MAX + 14 },
  { "list-all-trusted", no_argument, 0, CHAR_MAX + 15 },
  { "export", no_argument, 0, CHAR_MAX + 16 },
  { "export-stapled", no_argument, 0, CHAR_MAX + 17 },
  { "export-chain", no_argument, 0, CHAR_MAX + 18 },
  { "export-pubkey", no_argument, 0, CHAR_MAX + 19 },
  { "info", no_argument, 0, CHAR_MAX + 20 },
  { "generate-privkey", required_argument, 0, CHAR_MAX + 23 },
  { "generate-rsa", no_argument, 0, CHAR_MAX + 24 },
  { "generate-dsa", no_argument, 0, CHAR_MAX + 25 },
  { "generate-ecc", no_argument, 0, CHAR_MAX + 26 },
  { "bits", required_argument, 0, CHAR_MAX + 27 },
  { "curve", required_argument, 0, CHAR_MAX + 28 },
  { "sec-param", required_argument, 0, CHAR_MAX + 29 },
  { "set-id", required_argument, 0, CHAR_MAX + 30 },
  { "set-label", required_argument, 0, CHAR_MAX + 31 },
  { "write", no_argument, 0, CHAR_MAX + 32 },
  { "delete", no_argument, 0, CHAR_MAX + 33 },
  { "label", required_argument, 0, CHAR_MAX + 34 },
  { "id", required_argument, 0, CHAR_MAX + 35 },
  { "mark-wrap", no_argument, 0, CHAR_MAX + 36 },
  { "no-mark-wrap", no_argument, 0, CHAR_MAX + 37 },
  { "mark-trusted", no_argument, 0, CHAR_MAX + 38 },
  { "trusted", no_argument, 0, CHAR_MAX + 21 },
  { "no-mark-trusted", no_argument, 0, CHAR_MAX + 39 },
  { "mark-distrusted", no_argument, 0, CHAR_MAX + 40 },
  { "distrusted", no_argument, 0, CHAR_MAX + 22 },
  { "mark-decrypt", no_argument, 0, CHAR_MAX + 41 },
  { "no-mark-decrypt", no_argument, 0, CHAR_MAX + 42 },
  { "mark-sign", no_argument, 0, CHAR_MAX + 43 },
  { "no-mark-sign", no_argument, 0, CHAR_MAX + 44 },
  { "mark-ca", no_argument, 0, CHAR_MAX + 45 },
  { "ca", no_argument, 0, CHAR_MAX + 49 },
  { "no-mark-ca", no_argument, 0, CHAR_MAX + 46 },
  { "mark-private", no_argument, 0, CHAR_MAX + 47 },
  { "private", no_argument, 0, CHAR_MAX + 50 },
  { "no-mark-private", no_argument, 0, CHAR_MAX + 48 },
  { "mark-always-authenticate", no_argument, 0, CHAR_MAX + 51 },
  { "no-mark-always-authenticate", no_argument, 0, CHAR_MAX + 52 },
  { "secret-key", required_argument, 0, CHAR_MAX + 53 },
  { "load-privkey", required_argument, 0, CHAR_MAX + 54 },
  { "load-pubkey", required_argument, 0, CHAR_MAX + 55 },
  { "load-certificate", required_argument, 0, CHAR_MAX + 56 },
  { "debug", required_argument, 0, 'd' },
  { "outfile", required_argument, 0, CHAR_MAX + 57 },
  { "login", no_argument, 0, CHAR_MAX + 58 },
  { "no-login", no_argument, 0, CHAR_MAX + 59 },
  { "so-login", no_argument, 0, CHAR_MAX + 60 },
  { "admin-login", no_argument, 0, CHAR_MAX + 62 },
  { "no-so-login", no_argument, 0, CHAR_MAX + 61 },
  { "test-sign", no_argument, 0, CHAR_MAX + 63 },
  { "sign-params", required_argument, 0, CHAR_MAX + 64 },
  { "hash", required_argument, 0, CHAR_MAX + 65 },
  { "generate-random", required_argument, 0, CHAR_MAX + 66 },
  { "pkcs8", no_argument, 0, '8' },
  { "inder", no_argument, 0, CHAR_MAX + 67 },
  { "inraw", no_argument, 0, CHAR_MAX + 69 },
  { "no-inder", no_argument, 0, CHAR_MAX + 68 },
  { "outder", no_argument, 0, CHAR_MAX + 70 },
  { "outraw", no_argument, 0, CHAR_MAX + 72 },
  { "no-outder", no_argument, 0, CHAR_MAX + 71 },
  { "provider", required_argument, 0, CHAR_MAX + 73 },
  { "provider-opts", required_argument, 0, CHAR_MAX + 74 },
  { "detailed-url", no_argument, 0, CHAR_MAX + 75 },
  { "no-detailed-url", no_argument, 0, CHAR_MAX + 76 },
  { "only-urls", no_argument, 0, CHAR_MAX + 77 },
  { "batch", no_argument, 0, CHAR_MAX + 78 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct p11tool_options *opts = &p11tool_options;
  int opt;


  while ((opt = getopt_long (argc, argv, "!8d:hv:",
                             long_options, NULL)) != EOF)
    switch (opt)
      {
      case '\0': /* Long option.  */
        break;
      case CHAR_MAX + 1: /* --list-tokens */
        opts->present.list_tokens = true;
        opts->enabled.list_tokens = true;
        break;
      case CHAR_MAX + 2: /* --list-token-urls */
        opts->present.list_token_urls = true;
        opts->enabled.list_token_urls = true;
        break;
      case CHAR_MAX + 3: /* --list-mechanisms */
        opts->present.list_mechanisms = true;
        opts->enabled.list_mechanisms = true;
        break;
      case CHAR_MAX + 4: /* --initialize */
        opts->present.initialize = true;
        opts->enabled.initialize = true;
        break;
      case CHAR_MAX + 5: /* --initialize-pin */
        opts->present.initialize_pin = true;
        opts->enabled.initialize_pin = true;
        break;
      case CHAR_MAX + 6: /* --initialize-so-pin */
        opts->present.initialize_so_pin = true;
        opts->enabled.initialize_so_pin = true;
        break;
      case CHAR_MAX + 7: /* --set-pin */
        opts->present.set_pin = true;
        opts->arg.set_pin = optarg;
        opts->enabled.set_pin = true;
        break;
      case CHAR_MAX + 8: /* --set-so-pin */
        opts->present.set_so_pin = true;
        opts->arg.set_so_pin = optarg;
        opts->enabled.set_so_pin = true;
        break;
      case CHAR_MAX + 9: /* --list-all */
        opts->present.list_all = true;
        opts->enabled.list_all = true;
        break;
      case CHAR_MAX + 10: /* --list-all-certs */
        opts->present.list_all_certs = true;
        opts->enabled.list_all_certs = true;
        break;
      case CHAR_MAX + 11: /* --list-certs */
        opts->present.list_certs = true;
        opts->enabled.list_certs = true;
        break;
      case CHAR_MAX + 12: /* --list-all-privkeys */
      case CHAR_MAX + 13: /* --list-privkeys */
      case CHAR_MAX + 14: /* --list-keys */
        opts->present.list_all_privkeys = true;
        opts->enabled.list_all_privkeys = true;
        break;
      case CHAR_MAX + 15: /* --list-all-trusted */
        opts->present.list_all_trusted = true;
        opts->enabled.list_all_trusted = true;
        break;
      case CHAR_MAX + 16: /* --export */
        opts->present.export = true;
        opts->enabled.export = true;
        break;
      case CHAR_MAX + 17: /* --export-stapled */
        opts->present.export_stapled = true;
        opts->enabled.export_stapled = true;
        break;
      case CHAR_MAX + 18: /* --export-chain */
        opts->present.export_chain = true;
        opts->enabled.export_chain = true;
        break;
      case CHAR_MAX + 19: /* --export-pubkey */
        opts->present.export_pubkey = true;
        opts->enabled.export_pubkey = true;
        break;
      case CHAR_MAX + 20: /* --info */
        opts->present.info = true;
        opts->enabled.info = true;
        break;
      case CHAR_MAX + 23: /* --generate-privkey */
        opts->present.generate_privkey = true;
        opts->arg.generate_privkey = optarg;
        opts->enabled.generate_privkey = true;
        break;
      case CHAR_MAX + 24: /* --generate-rsa */
        opts->present.generate_rsa = true;
        opts->enabled.generate_rsa = true;
        break;
      case CHAR_MAX + 25: /* --generate-dsa */
        opts->present.generate_dsa = true;
        opts->enabled.generate_dsa = true;
        break;
      case CHAR_MAX + 26: /* --generate-ecc */
        opts->present.generate_ecc = true;
        opts->enabled.generate_ecc = true;
        break;
      case CHAR_MAX + 27: /* --bits */
        opts->present.bits = true;
        opts->arg.bits = optarg;
        opts->value.bits = parse_number(optarg);
        opts->enabled.bits = true;
        break;
      case CHAR_MAX + 28: /* --curve */
        opts->present.curve = true;
        opts->arg.curve = optarg;
        opts->enabled.curve = true;
        break;
      case CHAR_MAX + 29: /* --sec-param */
        opts->present.sec_param = true;
        opts->arg.sec_param = optarg;
        opts->enabled.sec_param = true;
        break;
      case CHAR_MAX + 30: /* --set-id */
        opts->present.set_id = true;
        opts->arg.set_id = optarg;
        opts->enabled.set_id = true;
        break;
      case CHAR_MAX + 31: /* --set-label */
        opts->present.set_label = true;
        opts->arg.set_label = optarg;
        opts->enabled.set_label = true;
        break;
      case CHAR_MAX + 32: /* --write */
        opts->present.write = true;
        opts->enabled.write = true;
        break;
      case CHAR_MAX + 33: /* --delete */
        opts->present.delete = true;
        opts->enabled.delete = true;
        break;
      case CHAR_MAX + 34: /* --label */
        opts->present.label = true;
        opts->arg.label = optarg;
        opts->enabled.label = true;
        break;
      case CHAR_MAX + 35: /* --id */
        opts->present.id = true;
        opts->arg.id = optarg;
        opts->enabled.id = true;
        break;
      case CHAR_MAX + 36: /* --mark-wrap */
        opts->present.mark_wrap = true;
        opts->enabled.mark_wrap = true;
        break;
      case CHAR_MAX + 37: /* --no-mark-wrap */
        opts->present.mark_wrap = true;
        opts->enabled.mark_wrap = false;
        break;
      case CHAR_MAX + 38: /* --mark-trusted */
      case CHAR_MAX + 21: /* --trusted */
        opts->present.mark_trusted = true;
        opts->enabled.mark_trusted = true;
        break;
      case CHAR_MAX + 39: /* --no-mark-trusted */
        opts->present.mark_trusted = true;
        opts->enabled.mark_trusted = false;
        break;
      case CHAR_MAX + 40: /* --mark-distrusted */
      case CHAR_MAX + 22: /* --distrusted */
        opts->present.mark_distrusted = true;
        opts->enabled.mark_distrusted = true;
        break;
      case CHAR_MAX + 41: /* --mark-decrypt */
        opts->present.mark_decrypt = true;
        opts->enabled.mark_decrypt = true;
        break;
      case CHAR_MAX + 42: /* --no-mark-decrypt */
        opts->present.mark_decrypt = true;
        opts->enabled.mark_decrypt = false;
        break;
      case CHAR_MAX + 43: /* --mark-sign */
        opts->present.mark_sign = true;
        opts->enabled.mark_sign = true;
        break;
      case CHAR_MAX + 44: /* --no-mark-sign */
        opts->present.mark_sign = true;
        opts->enabled.mark_sign = false;
        break;
      case CHAR_MAX + 45: /* --mark-ca */
      case CHAR_MAX + 49: /* --ca */
        opts->present.mark_ca = true;
        opts->enabled.mark_ca = true;
        break;
      case CHAR_MAX + 46: /* --no-mark-ca */
        opts->present.mark_ca = true;
        opts->enabled.mark_ca = false;
        break;
      case CHAR_MAX + 47: /* --mark-private */
      case CHAR_MAX + 50: /* --private */
        opts->present.mark_private = true;
        opts->enabled.mark_private = true;
        break;
      case CHAR_MAX + 48: /* --no-mark-private */
        opts->present.mark_private = true;
        opts->enabled.mark_private = false;
        break;
      case CHAR_MAX + 51: /* --mark-always-authenticate */
        opts->present.mark_always_authenticate = true;
        opts->enabled.mark_always_authenticate = true;
        break;
      case CHAR_MAX + 52: /* --no-mark-always-authenticate */
        opts->present.mark_always_authenticate = true;
        opts->enabled.mark_always_authenticate = false;
        break;
      case CHAR_MAX + 53: /* --secret-key */
        opts->present.secret_key = true;
        opts->arg.secret_key = optarg;
        opts->enabled.secret_key = true;
        break;
      case CHAR_MAX + 54: /* --load-privkey */
        opts->present.load_privkey = true;
        opts->arg.load_privkey = optarg;
        opts->enabled.load_privkey = true;
        break;
      case CHAR_MAX + 55: /* --load-pubkey */
        opts->present.load_pubkey = true;
        opts->arg.load_pubkey = optarg;
        opts->enabled.load_pubkey = true;
        break;
      case CHAR_MAX + 56: /* --load-certificate */
        opts->present.load_certificate = true;
        opts->arg.load_certificate = optarg;
        opts->enabled.load_certificate = true;
        break;
      case 'd':
        opts->present.debug = true;
        opts->arg.debug = optarg;
        opts->value.debug = parse_number(optarg);
        opts->enabled.debug = true;
        break;
      case CHAR_MAX + 57: /* --outfile */
        opts->present.outfile = true;
        opts->arg.outfile = optarg;
        opts->enabled.outfile = true;
        break;
      case CHAR_MAX + 58: /* --login */
        opts->present.login = true;
        opts->enabled.login = true;
        break;
      case CHAR_MAX + 59: /* --no-login */
        opts->present.login = true;
        opts->enabled.login = false;
        break;
      case CHAR_MAX + 60: /* --so-login */
      case CHAR_MAX + 62: /* --admin-login */
        opts->present.so_login = true;
        opts->enabled.so_login = true;
        break;
      case CHAR_MAX + 61: /* --no-so-login */
        opts->present.so_login = true;
        opts->enabled.so_login = false;
        break;
      case CHAR_MAX + 63: /* --test-sign */
        opts->present.test_sign = true;
        opts->enabled.test_sign = true;
        break;
      case CHAR_MAX + 64: /* --sign-params */
        opts->present.sign_params = true;
        opts->arg.sign_params = optarg;
        opts->enabled.sign_params = true;
        break;
      case CHAR_MAX + 65: /* --hash */
        opts->present.hash = true;
        opts->arg.hash = optarg;
        opts->enabled.hash = true;
        break;
      case CHAR_MAX + 66: /* --generate-random */
        opts->present.generate_random = true;
        opts->arg.generate_random = optarg;
        opts->value.generate_random = parse_number(optarg);
        opts->enabled.generate_random = true;
        break;
      case '8':
        opts->present.pkcs8 = true;
        opts->enabled.pkcs8 = true;
        break;
      case CHAR_MAX + 67: /* --inder */
      case CHAR_MAX + 69: /* --inraw */
        opts->present.inder = true;
        opts->enabled.inder = true;
        break;
      case CHAR_MAX + 68: /* --no-inder */
        opts->present.inder = true;
        opts->enabled.inder = false;
        break;
      case CHAR_MAX + 70: /* --outder */
      case CHAR_MAX + 72: /* --outraw */
        opts->present.outder = true;
        opts->enabled.outder = true;
        break;
      case CHAR_MAX + 71: /* --no-outder */
        opts->present.outder = true;
        opts->enabled.outder = false;
        break;
      case CHAR_MAX + 73: /* --provider */
        opts->present.provider = true;
        opts->arg.provider = optarg;
        opts->enabled.provider = true;
        break;
      case CHAR_MAX + 74: /* --provider-opts */
        opts->present.provider_opts = true;
        opts->arg.provider_opts = optarg;
        opts->enabled.provider_opts = true;
        break;
      case CHAR_MAX + 75: /* --detailed-url */
        opts->present.detailed_url = true;
        opts->enabled.detailed_url = true;
        break;
      case CHAR_MAX + 76: /* --no-detailed-url */
        opts->present.detailed_url = true;
        opts->enabled.detailed_url = false;
        break;
      case CHAR_MAX + 77: /* --only-urls */
        opts->present.only_urls = true;
        opts->enabled.only_urls = true;
        break;
      case CHAR_MAX + 78: /* --batch */
        opts->present.batch = true;
        opts->enabled.batch = true;
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

  if (HAVE_OPT(EXPORT) && HAVE_OPT(EXPORT_STAPLED))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export", "export_stapled");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT) && HAVE_OPT(EXPORT_CHAIN))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export", "export_chain");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT) && HAVE_OPT(EXPORT_PUBKEY))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export", "export_pubkey");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_STAPLED) && HAVE_OPT(EXPORT))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-stapled", "export");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_STAPLED) && HAVE_OPT(EXPORT_CHAIN))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-stapled", "export_chain");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_STAPLED) && HAVE_OPT(EXPORT_PUBKEY))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-stapled", "export_pubkey");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_CHAIN) && HAVE_OPT(EXPORT_STAPLED))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-chain", "export_stapled");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_CHAIN) && HAVE_OPT(EXPORT))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-chain", "export");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_CHAIN) && HAVE_OPT(EXPORT_PUBKEY))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-chain", "export_pubkey");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_PUBKEY) && HAVE_OPT(EXPORT_STAPLED))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-pubkey", "export_stapled");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_PUBKEY) && HAVE_OPT(EXPORT))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-pubkey", "export");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(EXPORT_PUBKEY) && HAVE_OPT(EXPORT_CHAIN))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "export-pubkey", "export_chain");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SET_ID) && HAVE_OPT(WRITE))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "set-id", "write");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SET_LABEL) && HAVE_OPT(WRITE))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "set-label", "write");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SET_LABEL) && HAVE_OPT(SET_ID))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "set-label", "set_id");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(MARK_TRUSTED) && HAVE_OPT(MARK_DISTRUSTED))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "mark-trusted", "mark_distrusted");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(MARK_DISTRUSTED) && HAVE_OPT(MARK_TRUSTED))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "mark-distrusted", "mark_trusted");
      exit (EXIT_FAILURE);
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
            "p11tool 3.8.7\n"
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
            "p11tool 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "p11tool 3.8.7\n"
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
    "p11tool - GnuTLS PKCS #11 tool\n"
    "Usage:  p11tool [ -<flag> [<val>] | --<name>[{=| }<val>] ]... [url]\n"
    "\n"
    "None:\n"
    "\n"
    "\n"
    "Tokens:\n"
    "\n"
    "       --list-tokens          List all available tokens\n"
    "       --list-token-urls      List the URLs available tokens\n"
    "       --list-mechanisms      List all available mechanisms in a token\n"
    "       --initialize           Initializes a PKCS #11 token\n"
    "       --initialize-pin       Initializes/Resets a PKCS #11 token user PIN\n"
    "       --initialize-so-pin    Initializes/Resets a PKCS #11 token security officer PIN\n"
    "       --set-pin=str          Specify the PIN to use on token operations\n"
    "       --set-so-pin=str       Specify the Security Officer's PIN to use on token initialization\n"
    "\n"
    "Object listing:\n"
    "\n"
    "       --list-all             List all available objects in a token\n"
    "       --list-all-certs       List all available certificates in a token\n"
    "       --list-certs           List all certificates that have an associated private key\n"
    "       --list-all-privkeys    List all available private keys in a token\n"
    "       --list-privkeys        an alias for the 'list-all-privkeys' option\n"
    "       --list-keys            an alias for the 'list-all-privkeys' option\n"
    "       --list-all-trusted     List all available certificates marked as trusted\n"
    "       --export               Export the object specified by the URL\n"
    "				- prohibits these options:\n"
    "				export-stapled\n"
    "				export-chain\n"
    "				export-pubkey\n"
    "       --export-stapled       Export the certificate object specified by the URL\n"
    "				- prohibits these options:\n"
    "				export\n"
    "				export-chain\n"
    "				export-pubkey\n"
    "       --export-chain         Export the certificate specified by the URL and its chain of trust\n"
    "				- prohibits these options:\n"
    "				export-stapled\n"
    "				export\n"
    "				export-pubkey\n"
    "       --export-pubkey        Export the public key for a private key\n"
    "				- prohibits these options:\n"
    "				export-stapled\n"
    "				export\n"
    "				export-chain\n"
    "       --info                 List information on an available object in a token\n"
    "       --trusted              an alias for the 'mark-trusted' option\n"
    "       --distrusted           an alias for the 'mark-distrusted' option\n"
    "\n"
    "Key generation:\n"
    "\n"
    "       --generate-privkey=str Generate private-public key pair of given type\n"
    "       --bits=num             Specify the number of bits for the key generate\n"
    "       --curve=str            Specify the curve used for EC key generation\n"
    "       --sec-param=str        Specify the security level\n"
    "\n"
    "Writing objects:\n"
    "\n"
    "       --set-id=str           Set the CKA_ID (in hex) for the specified by the URL object\n"
    "				- prohibits the option 'write'\n"
    "       --set-label=str        Set the CKA_LABEL for the specified by the URL object\n"
    "				- prohibits these options:\n"
    "				write\n"
    "				set-id\n"
    "       --write                Writes the loaded objects to a PKCS #11 token\n"
    "       --delete               Deletes the objects matching the given PKCS #11 URL\n"
    "       --label=str            Sets a label for the write operation\n"
    "       --id=str               Sets an ID for the write operation\n"
    "       --mark-wrap            Marks the generated key to be a wrapping key\n"
    "       --mark-trusted         Marks the object to be written as trusted\n"
    "				- prohibits the option 'mark-distrusted'\n"
    "       --mark-distrusted      When retrieving objects, it requires the objects to be distrusted\n"
    "				- prohibits the option 'mark-trusted'\n"
    "       --mark-decrypt         Marks the object to be written for decryption\n"
    "       --mark-sign            Marks the object to be written for signature generation\n"
    "       --mark-ca              Marks the object to be written as a CA\n"
    "       --mark-private         Marks the object to be written as private\n"
    "       --ca                   an alias for the 'mark-ca' option\n"
    "       --private              an alias for the 'mark-private' option\n"
    "       --mark-always-authenticate  Marks the object to be written as always authenticate\n"
    "       --secret-key=str       Provide a hex encoded secret key\n"
    "       --load-privkey=file    Private key file to use\n"
    "				- file must pre-exist\n"
    "       --load-pubkey=file     Public key file to use\n"
    "				- file must pre-exist\n"
    "       --load-certificate=file Certificate file to use\n"
    "				- file must pre-exist\n"
    "\n"
    "Other options:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "       --outfile=str          Output file\n"
    "       --login                Force (user) login to token\n"
    "       --so-login             Force security officer login to token\n"
    "       --admin-login          an alias for the 'so-login' option\n"
    "       --test-sign            Tests the signature operation of the provided object\n"
    "       --sign-params=str      Sign with a specific signature algorithm\n"
    "       --hash=str             Hash algorithm to use for signing\n"
    "       --generate-random=num  Generate random data\n"
    "   -8, --pkcs8                Use PKCS #8 format for private keys\n"
    "       --inder                Use DER/RAW format for input\n"
    "       --inraw                an alias for the 'inder' option\n"
    "       --outder               Use DER format for output certificates, private keys, and DH parameters\n"
    "       --outraw               an alias for the 'outder' option\n"
    "       --provider=file        Specify the PKCS #11 provider library\n"
    "       --detailed-url         Print detailed URLs\n"
    "       --only-urls            Print a compact listing using only the URLs\n"
    "       --batch                Disable all interaction with the tool\n"
    "\n"
    "Version, usage and configuration options:\n"
    "\n"
    "   -v, --version[=arg]        output version information and exit\n"
    "   -h, --help                 display extended usage information and exit\n"
    "   -!, --more-help            extended usage information passed thru pager\n"
    "\n"
    "Options are specified by doubled hyphens and their name or by a single\n"
    "hyphen and the flag character.\n"
    "Operands and options may be intermixed.  They will be reordered.\n"
    "\n"
    "Program that allows operations on PKCS #11 smart cards\n"
    "and security modules. \n"
    "\n"
    "To use PKCS #11 tokens with GnuTLS the p11-kit configuration files need to be setup.\n"
    "That is create a .module file in /etc/pkcs11/modules with the contents 'module: /path/to/pkcs11.so'.\n"
    "Alternatively the configuration file /etc/gnutls/pkcs11.conf has to exist and contain a number\n"
    "of lines of the form 'load=/usr/lib/opensc-pkcs11.so'.\n"
    "\n"
    "You can provide the PIN to be used for the PKCS #11 operations with the environment variables\n"
    "GNUTLS_PIN and GNUTLS_SO_PIN.\n"
    "\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
