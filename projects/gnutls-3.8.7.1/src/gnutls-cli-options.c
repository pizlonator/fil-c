#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gnutls-cli-options.h"
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

struct gnutls_cli_options gnutls_cli_options;

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
append_to_list (struct gnutls_cli_list *list,
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
  { "verbose", no_argument, 0, 'V' },
  { "tofu", no_argument, 0, CHAR_MAX + 1 },
  { "no-tofu", no_argument, 0, CHAR_MAX + 2 },
  { "strict-tofu", no_argument, 0, CHAR_MAX + 3 },
  { "no-strict-tofu", no_argument, 0, CHAR_MAX + 4 },
  { "dane", no_argument, 0, CHAR_MAX + 5 },
  { "no-dane", no_argument, 0, CHAR_MAX + 6 },
  { "local-dns", no_argument, 0, CHAR_MAX + 7 },
  { "no-local-dns", no_argument, 0, CHAR_MAX + 8 },
  { "ca-verification", no_argument, 0, CHAR_MAX + 9 },
  { "no-ca-verification", no_argument, 0, CHAR_MAX + 10 },
  { "ocsp", no_argument, 0, CHAR_MAX + 11 },
  { "no-ocsp", no_argument, 0, CHAR_MAX + 12 },
  { "resume", no_argument, 0, 'r' },
  { "earlydata", required_argument, 0, CHAR_MAX + 13 },
  { "rehandshake", no_argument, 0, 'e' },
  { "sni-hostname", required_argument, 0, CHAR_MAX + 14 },
  { "verify-hostname", required_argument, 0, CHAR_MAX + 15 },
  { "starttls", no_argument, 0, 's' },
  { "starttls-proto", required_argument, 0, CHAR_MAX + 17 },
  { "app-proto", required_argument, 0, CHAR_MAX + 16 },
  { "starttls-name", required_argument, 0, CHAR_MAX + 18 },
  { "udp", no_argument, 0, 'u' },
  { "mtu", required_argument, 0, CHAR_MAX + 19 },
  { "crlf", no_argument, 0, CHAR_MAX + 20 },
  { "fastopen", no_argument, 0, CHAR_MAX + 21 },
  { "x509fmtder", no_argument, 0, CHAR_MAX + 22 },
  { "print-cert", no_argument, 0, CHAR_MAX + 23 },
  { "save-cert", required_argument, 0, CHAR_MAX + 24 },
  { "save-ocsp", required_argument, 0, CHAR_MAX + 25 },
  { "save-ocsp-multi", required_argument, 0, CHAR_MAX + 26 },
  { "save-server-trace", required_argument, 0, CHAR_MAX + 27 },
  { "save-client-trace", required_argument, 0, CHAR_MAX + 28 },
  { "dh-bits", required_argument, 0, CHAR_MAX + 29 },
  { "priority", required_argument, 0, CHAR_MAX + 30 },
  { "x509cafile", required_argument, 0, CHAR_MAX + 31 },
  { "x509crlfile", required_argument, 0, CHAR_MAX + 32 },
  { "x509keyfile", required_argument, 0, CHAR_MAX + 33 },
  { "x509certfile", required_argument, 0, CHAR_MAX + 34 },
  { "rawpkkeyfile", required_argument, 0, CHAR_MAX + 35 },
  { "rawpkfile", required_argument, 0, CHAR_MAX + 36 },
  { "srpusername", required_argument, 0, CHAR_MAX + 37 },
  { "srppasswd", required_argument, 0, CHAR_MAX + 38 },
  { "pskusername", required_argument, 0, CHAR_MAX + 39 },
  { "pskkey", required_argument, 0, CHAR_MAX + 40 },
  { "port", required_argument, 0, 'p' },
  { "insecure", no_argument, 0, CHAR_MAX + 41 },
  { "verify-allow-broken", no_argument, 0, CHAR_MAX + 42 },
  { "ranges", no_argument, 0, CHAR_MAX + 43 },
  { "benchmark-ciphers", no_argument, 0, CHAR_MAX + 44 },
  { "benchmark-tls-kx", no_argument, 0, CHAR_MAX + 45 },
  { "benchmark-tls-ciphers", no_argument, 0, CHAR_MAX + 46 },
  { "list", no_argument, 0, 'l' },
  { "priority-list", no_argument, 0, CHAR_MAX + 47 },
  { "noticket", no_argument, 0, CHAR_MAX + 48 },
  { "srtp-profiles", required_argument, 0, CHAR_MAX + 49 },
  { "alpn", required_argument, 0, CHAR_MAX + 50 },
  { "compress-cert", required_argument, 0, CHAR_MAX + 51 },
  { "heartbeat", no_argument, 0, 'b' },
  { "recordsize", required_argument, 0, CHAR_MAX + 52 },
  { "disable-sni", no_argument, 0, CHAR_MAX + 53 },
  { "disable-extensions", no_argument, 0, CHAR_MAX + 54 },
  { "single-key-share", no_argument, 0, CHAR_MAX + 55 },
  { "post-handshake-auth", no_argument, 0, CHAR_MAX + 56 },
  { "inline-commands", no_argument, 0, CHAR_MAX + 57 },
  { "inline-commands-prefix", required_argument, 0, CHAR_MAX + 58 },
  { "provider", required_argument, 0, CHAR_MAX + 59 },
  { "fips140-mode", no_argument, 0, CHAR_MAX + 60 },
  { "list-config", no_argument, 0, CHAR_MAX + 61 },
  { "logfile", required_argument, 0, CHAR_MAX + 62 },
  { "keymatexport", required_argument, 0, CHAR_MAX + 63 },
  { "keymatexportsize", required_argument, 0, CHAR_MAX + 64 },
  { "waitresumption", no_argument, 0, CHAR_MAX + 65 },
  { "ca-auto-retrieve", no_argument, 0, CHAR_MAX + 66 },
  { "no-ca-auto-retrieve", no_argument, 0, CHAR_MAX + 67 },
  { "attime", required_argument, 0, CHAR_MAX + 68 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct gnutls_cli_options *opts = &gnutls_cli_options;
  int opt;

  opts->enabled.ca_verification = true;
  while ((opt = getopt_long (argc, argv, "!Vbd:ehlp:rsuv:",
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
      case CHAR_MAX + 1: /* --tofu */
        opts->present.tofu = true;
        opts->enabled.tofu = true;
        break;
      case CHAR_MAX + 2: /* --no-tofu */
        opts->present.tofu = true;
        opts->enabled.tofu = false;
        break;
      case CHAR_MAX + 3: /* --strict-tofu */
        opts->present.strict_tofu = true;
        opts->enabled.strict_tofu = true;
        break;
      case CHAR_MAX + 4: /* --no-strict-tofu */
        opts->present.strict_tofu = true;
        opts->enabled.strict_tofu = false;
        break;
      case CHAR_MAX + 5: /* --dane */
        opts->present.dane = true;
        opts->enabled.dane = true;
        break;
      case CHAR_MAX + 6: /* --no-dane */
        opts->present.dane = true;
        opts->enabled.dane = false;
        break;
      case CHAR_MAX + 7: /* --local-dns */
        opts->present.local_dns = true;
        opts->enabled.local_dns = true;
        break;
      case CHAR_MAX + 8: /* --no-local-dns */
        opts->present.local_dns = true;
        opts->enabled.local_dns = false;
        break;
      case CHAR_MAX + 9: /* --ca-verification */
        opts->present.ca_verification = true;
        opts->enabled.ca_verification = true;
        break;
      case CHAR_MAX + 10: /* --no-ca-verification */
        opts->present.ca_verification = true;
        opts->enabled.ca_verification = false;
        break;
      case CHAR_MAX + 11: /* --ocsp */
        opts->present.ocsp = true;
        opts->enabled.ocsp = true;
        break;
      case CHAR_MAX + 12: /* --no-ocsp */
        opts->present.ocsp = true;
        opts->enabled.ocsp = false;
        break;
      case 'r':
        opts->present.resume = true;
        opts->enabled.resume = true;
        break;
      case CHAR_MAX + 13: /* --earlydata */
        opts->present.earlydata = true;
        opts->arg.earlydata = optarg;
        opts->enabled.earlydata = true;
        break;
      case 'e':
        opts->present.rehandshake = true;
        opts->enabled.rehandshake = true;
        break;
      case CHAR_MAX + 14: /* --sni-hostname */
        opts->present.sni_hostname = true;
        opts->arg.sni_hostname = optarg;
        opts->enabled.sni_hostname = true;
        break;
      case CHAR_MAX + 15: /* --verify-hostname */
        opts->present.verify_hostname = true;
        opts->arg.verify_hostname = optarg;
        opts->enabled.verify_hostname = true;
        break;
      case 's':
        opts->present.starttls = true;
        opts->enabled.starttls = true;
        break;
      case CHAR_MAX + 17: /* --starttls-proto */
      case CHAR_MAX + 16: /* --app-proto */
        opts->present.starttls_proto = true;
        opts->arg.starttls_proto = optarg;
        opts->enabled.starttls_proto = true;
        break;
      case CHAR_MAX + 18: /* --starttls-name */
        opts->present.starttls_name = true;
        opts->arg.starttls_name = optarg;
        opts->enabled.starttls_name = true;
        break;
      case 'u':
        opts->present.udp = true;
        opts->enabled.udp = true;
        break;
      case CHAR_MAX + 19: /* --mtu */
        opts->present.mtu = true;
        opts->arg.mtu = optarg;
        opts->value.mtu = parse_number(optarg);
        opts->enabled.mtu = true;
        break;
      case CHAR_MAX + 20: /* --crlf */
        opts->present.crlf = true;
        opts->enabled.crlf = true;
        break;
      case CHAR_MAX + 21: /* --fastopen */
        opts->present.fastopen = true;
        opts->enabled.fastopen = true;
        break;
      case CHAR_MAX + 22: /* --x509fmtder */
        opts->present.x509fmtder = true;
        opts->enabled.x509fmtder = true;
        break;
      case CHAR_MAX + 23: /* --print-cert */
        opts->present.print_cert = true;
        opts->enabled.print_cert = true;
        break;
      case CHAR_MAX + 24: /* --save-cert */
        opts->present.save_cert = true;
        opts->arg.save_cert = optarg;
        opts->enabled.save_cert = true;
        break;
      case CHAR_MAX + 25: /* --save-ocsp */
        opts->present.save_ocsp = true;
        opts->arg.save_ocsp = optarg;
        opts->enabled.save_ocsp = true;
        break;
      case CHAR_MAX + 26: /* --save-ocsp-multi */
        opts->present.save_ocsp_multi = true;
        opts->arg.save_ocsp_multi = optarg;
        opts->enabled.save_ocsp_multi = true;
        break;
      case CHAR_MAX + 27: /* --save-server-trace */
        opts->present.save_server_trace = true;
        opts->arg.save_server_trace = optarg;
        opts->enabled.save_server_trace = true;
        break;
      case CHAR_MAX + 28: /* --save-client-trace */
        opts->present.save_client_trace = true;
        opts->arg.save_client_trace = optarg;
        opts->enabled.save_client_trace = true;
        break;
      case CHAR_MAX + 29: /* --dh-bits */
        opts->present.dh_bits = true;
        opts->arg.dh_bits = optarg;
        opts->value.dh_bits = parse_number(optarg);
        opts->enabled.dh_bits = true;
        break;
      case CHAR_MAX + 30: /* --priority */
        opts->present.priority = true;
        opts->arg.priority = optarg;
        opts->enabled.priority = true;
        break;
      case CHAR_MAX + 31: /* --x509cafile */
        opts->present.x509cafile = true;
        opts->arg.x509cafile = optarg;
        opts->enabled.x509cafile = true;
        break;
      case CHAR_MAX + 32: /* --x509crlfile */
        opts->present.x509crlfile = true;
        opts->arg.x509crlfile = optarg;
        opts->enabled.x509crlfile = true;
        break;
      case CHAR_MAX + 33: /* --x509keyfile */
        opts->present.x509keyfile = true;
        opts->arg.x509keyfile = optarg;
        opts->enabled.x509keyfile = true;
        break;
      case CHAR_MAX + 34: /* --x509certfile */
        opts->present.x509certfile = true;
        opts->arg.x509certfile = optarg;
        opts->enabled.x509certfile = true;
        break;
      case CHAR_MAX + 35: /* --rawpkkeyfile */
        opts->present.rawpkkeyfile = true;
        opts->arg.rawpkkeyfile = optarg;
        opts->enabled.rawpkkeyfile = true;
        break;
      case CHAR_MAX + 36: /* --rawpkfile */
        opts->present.rawpkfile = true;
        opts->arg.rawpkfile = optarg;
        opts->enabled.rawpkfile = true;
        break;
      case CHAR_MAX + 37: /* --srpusername */
        opts->present.srpusername = true;
        opts->arg.srpusername = optarg;
        opts->enabled.srpusername = true;
        break;
      case CHAR_MAX + 38: /* --srppasswd */
        opts->present.srppasswd = true;
        opts->arg.srppasswd = optarg;
        opts->enabled.srppasswd = true;
        break;
      case CHAR_MAX + 39: /* --pskusername */
        opts->present.pskusername = true;
        opts->arg.pskusername = optarg;
        opts->enabled.pskusername = true;
        break;
      case CHAR_MAX + 40: /* --pskkey */
        opts->present.pskkey = true;
        opts->arg.pskkey = optarg;
        opts->enabled.pskkey = true;
        break;
      case 'p':
        opts->present.port = true;
        opts->arg.port = optarg;
        opts->enabled.port = true;
        break;
      case CHAR_MAX + 41: /* --insecure */
        opts->present.insecure = true;
        opts->enabled.insecure = true;
        break;
      case CHAR_MAX + 42: /* --verify-allow-broken */
        opts->present.verify_allow_broken = true;
        opts->enabled.verify_allow_broken = true;
        break;
      case CHAR_MAX + 43: /* --ranges */
        opts->present.ranges = true;
        opts->enabled.ranges = true;
        break;
      case CHAR_MAX + 44: /* --benchmark-ciphers */
        opts->present.benchmark_ciphers = true;
        opts->enabled.benchmark_ciphers = true;
        break;
      case CHAR_MAX + 45: /* --benchmark-tls-kx */
        opts->present.benchmark_tls_kx = true;
        opts->enabled.benchmark_tls_kx = true;
        break;
      case CHAR_MAX + 46: /* --benchmark-tls-ciphers */
        opts->present.benchmark_tls_ciphers = true;
        opts->enabled.benchmark_tls_ciphers = true;
        break;
      case 'l':
        opts->present.list = true;
        opts->enabled.list = true;
        break;
      case CHAR_MAX + 47: /* --priority-list */
        opts->present.priority_list = true;
        opts->enabled.priority_list = true;
        break;
      case CHAR_MAX + 48: /* --noticket */
        opts->present.noticket = true;
        opts->enabled.noticket = true;
        break;
      case CHAR_MAX + 49: /* --srtp-profiles */
        opts->present.srtp_profiles = true;
        opts->arg.srtp_profiles = optarg;
        opts->enabled.srtp_profiles = true;
        break;
      case CHAR_MAX + 50: /* --alpn */
        opts->present.alpn = true;
        append_to_list (&opts->list.alpn, "alpn", optarg);
        opts->enabled.alpn = true;
        break;
      case CHAR_MAX + 51: /* --compress-cert */
        opts->present.compress_cert = true;
        append_to_list (&opts->list.compress_cert, "compress-cert", optarg);
        opts->enabled.compress_cert = true;
        break;
      case 'b':
        opts->present.heartbeat = true;
        opts->enabled.heartbeat = true;
        break;
      case CHAR_MAX + 52: /* --recordsize */
        opts->present.recordsize = true;
        opts->arg.recordsize = optarg;
        opts->value.recordsize = parse_number(optarg);
        opts->enabled.recordsize = true;
        break;
      case CHAR_MAX + 53: /* --disable-sni */
        opts->present.disable_sni = true;
        opts->enabled.disable_sni = true;
        break;
      case CHAR_MAX + 54: /* --disable-extensions */
        opts->present.disable_extensions = true;
        opts->enabled.disable_extensions = true;
        break;
      case CHAR_MAX + 55: /* --single-key-share */
        opts->present.single_key_share = true;
        opts->enabled.single_key_share = true;
        break;
      case CHAR_MAX + 56: /* --post-handshake-auth */
        opts->present.post_handshake_auth = true;
        opts->enabled.post_handshake_auth = true;
        break;
      case CHAR_MAX + 57: /* --inline-commands */
        opts->present.inline_commands = true;
        opts->enabled.inline_commands = true;
        break;
      case CHAR_MAX + 58: /* --inline-commands-prefix */
        opts->present.inline_commands_prefix = true;
        opts->arg.inline_commands_prefix = optarg;
        opts->enabled.inline_commands_prefix = true;
        break;
      case CHAR_MAX + 59: /* --provider */
        opts->present.provider = true;
        opts->arg.provider = optarg;
        opts->enabled.provider = true;
        break;
      case CHAR_MAX + 60: /* --fips140-mode */
        opts->present.fips140_mode = true;
        opts->enabled.fips140_mode = true;
        break;
      case CHAR_MAX + 61: /* --list-config */
        opts->present.list_config = true;
        opts->enabled.list_config = true;
        break;
      case CHAR_MAX + 62: /* --logfile */
        opts->present.logfile = true;
        opts->arg.logfile = optarg;
        opts->enabled.logfile = true;
        break;
      case CHAR_MAX + 63: /* --keymatexport */
        opts->present.keymatexport = true;
        opts->arg.keymatexport = optarg;
        opts->enabled.keymatexport = true;
        break;
      case CHAR_MAX + 64: /* --keymatexportsize */
        opts->present.keymatexportsize = true;
        opts->arg.keymatexportsize = optarg;
        opts->value.keymatexportsize = parse_number(optarg);
        opts->enabled.keymatexportsize = true;
        break;
      case CHAR_MAX + 65: /* --waitresumption */
        opts->present.waitresumption = true;
        opts->enabled.waitresumption = true;
        break;
      case CHAR_MAX + 66: /* --ca-auto-retrieve */
        opts->present.ca_auto_retrieve = true;
        opts->enabled.ca_auto_retrieve = true;
        break;
      case CHAR_MAX + 67: /* --no-ca-auto-retrieve */
        opts->present.ca_auto_retrieve = true;
        opts->enabled.ca_auto_retrieve = false;
        break;
      case CHAR_MAX + 68: /* --attime */
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
  if (HAVE_OPT(STARTTLS_PROTO) && HAVE_OPT(STARTTLS))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "starttls-proto", "starttls");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(STARTTLS_NAME) && HAVE_OPT(STARTTLS))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "starttls-name", "starttls");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(STARTTLS_NAME) && !HAVE_OPT(STARTTLS_PROTO))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "starttls-name", "starttls_proto");
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
  if (HAVE_OPT(SAVE_OCSP) && HAVE_OPT(SAVE_OCSP_MULTI))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "save-ocsp", "save_ocsp_multi");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(SAVE_OCSP_MULTI) && HAVE_OPT(SAVE_OCSP))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "save-ocsp-multi", "save_ocsp");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(X509CERTFILE) && !HAVE_OPT(X509KEYFILE))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "x509certfile", "x509keyfile");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(RAWPKFILE) && !HAVE_OPT(RAWPKKEYFILE))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "rawpkfile", "rawpkkeyfile");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(LIST) && HAVE_OPT(PORT))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "list", "port");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(RECORDSIZE) && OPT_VALUE_RECORDSIZE < 0)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "recordsize", opts->value.recordsize);
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(RECORDSIZE) && OPT_VALUE_RECORDSIZE > 4096)
    {
      fprintf (stderr, "%s option value %d is out of range\n",
               "recordsize", opts->value.recordsize);
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
            "gnutls-cli 3.8.7\n"
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
            "gnutls-cli 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "gnutls-cli 3.8.7\n"
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
    "gnutls-cli - GnuTLS client\n"
    "Usage:  gnutls-cli [ -<flag> [<val>] | --<name>[{=| }<val>] ]... [hostname]\n"
    "\n"
    "None:\n"
    "\n"
    "   -d, --debug=num            Enable debugging\n"
    "				- it must be in the range:\n"
    "				  0 to 9999\n"
    "   -V, --verbose              More verbose output\n"
    "       --tofu                 Enable trust on first use authentication\n"
    "       --strict-tofu          Fail to connect if a certificate is unknown or a known certificate has changed\n"
    "       --dane                 Enable DANE certificate verification (DNSSEC)\n"
    "       --local-dns            Use the local DNS server for DNSSEC resolving\n"
    "       --ca-verification      Enable CA certificate verification\n"
    "				- enabled by default\n"
    "				- disabled as '--no-ca-verification'\n"
    "       --ocsp                 Enable OCSP certificate verification\n"
    "   -r, --resume               Establish a session and resume\n"
    "       --earlydata=str        Send early data on resumption from the specified file\n"
    "   -e, --rehandshake          Establish a session and rehandshake\n"
    "       --sni-hostname=str     Server's hostname for server name indication extension\n"
    "       --verify-hostname=str  Server's hostname to use for validation\n"
    "   -s, --starttls             Connect, establish a plain session and start TLS\n"
    "       --app-proto            an alias for the 'starttls-proto' option\n"
    "       --starttls-proto=str   The application protocol to be used to obtain the server's certificate (https, ftp, smtp, imap, ldap, xmpp, lmtp, pop3, nntp, sieve, postgres)\n"
    "				- prohibits the option 'starttls'\n"
    "       --starttls-name=str    The hostname presented to the application protocol for STARTTLS (for smtp, xmpp, lmtp)\n"
    "				- prohibits the option 'starttls'\n"
    "				- requires the option 'starttls-proto'\n"
    "   -u, --udp                  Use DTLS (datagram TLS) over UDP\n"
    "       --mtu=num              Set MTU for datagram TLS\n"
    "				- it must be in the range:\n"
    "				  0 to 17000\n"
    "       --crlf                 Send CR LF instead of LF\n"
    "       --fastopen             Enable TCP Fast Open\n"
    "       --x509fmtder           Use DER format for certificates to read from\n"
    "       --print-cert           Print peer's certificate in PEM format\n"
    "       --save-cert=str        Save the peer's certificate chain in the specified file in PEM format\n"
    "       --save-ocsp=str        Save the peer's OCSP status response in the provided file\n"
    "				- prohibits the option 'save-ocsp-multi'\n"
    "       --save-ocsp-multi=str  Save all OCSP responses provided by the peer in this file\n"
    "				- prohibits the option 'save-ocsp'\n"
    "       --save-server-trace=str Save the server-side TLS message trace in the provided file\n"
    "       --save-client-trace=str Save the client-side TLS message trace in the provided file\n"
    "       --dh-bits=num          The minimum number of bits allowed for DH\n"
    "       --priority=str         Priorities string\n"
    "       --x509cafile=str       Certificate file or PKCS #11 URL to use\n"
    "       --x509crlfile=file     CRL file to use\n"
    "				- file must pre-exist\n"
    "       --x509keyfile=str      X.509 key file or PKCS #11 URL to use\n"
    "       --x509certfile=str     X.509 Certificate file or PKCS #11 URL to use\n"
    "				- requires the option 'x509keyfile'\n"
    "       --rawpkkeyfile=str     Private key file (PKCS #8 or PKCS #12) or PKCS #11 URL to use\n"
    "       --rawpkfile=str        Raw public-key file to use\n"
    "				- requires the option 'rawpkkeyfile'\n"
    "       --srpusername=str      SRP username to use\n"
    "       --srppasswd=str        SRP password to use\n"
    "       --pskusername=str      PSK username to use\n"
    "       --pskkey=str           PSK key (in hex) to use\n"
    "   -p, --port=str             The port or service to connect to\n"
    "       --insecure             Don't abort program if server certificate can't be validated\n"
    "       --verify-allow-broken  Allow broken algorithms, such as MD5 for certificate verification\n"
    "       --benchmark-ciphers    Benchmark individual ciphers\n"
    "       --benchmark-tls-kx     Benchmark TLS key exchange methods\n"
    "       --benchmark-tls-ciphers  Benchmark TLS ciphers\n"
    "   -l, --list                 Print a list of the supported algorithms and modes\n"
    "				- prohibits the option 'port'\n"
    "       --priority-list        Print a list of the supported priority strings\n"
    "       --noticket             Don't allow session tickets\n"
    "       --srtp-profiles=str    Offer SRTP profiles\n"
    "       --alpn=str             Application layer protocol\n"
    "       --compress-cert=str    Compress certificate\n"
    "   -b, --heartbeat            Activate heartbeat support\n"
    "       --recordsize=num       The maximum record size to advertise\n"
    "				- it must be in the range:\n"
    "				  0 to 4096\n"
    "       --disable-sni          Do not send a Server Name Indication (SNI)\n"
    "       --single-key-share     Send a single key share under TLS1.3\n"
    "       --post-handshake-auth  Enable post-handshake authentication under TLS1.3\n"
    "       --inline-commands      Inline commands of the form ^<cmd>^\n"
    "       --inline-commands-prefix=str Change the default delimiter for inline commands\n"
    "       --provider=file        Specify the PKCS #11 provider library\n"
    "				- file must pre-exist\n"
    "       --fips140-mode         Reports the status of the FIPS140-2 mode in gnutls library\n"
    "       --list-config          Reports the configuration of the library\n"
    "       --logfile=str          Redirect informational messages to a specific file\n"
    "       --keymatexport=str     Label used for exporting keying material\n"
    "       --keymatexportsize=num Size of the exported keying material\n"
    "       --waitresumption       Block waiting for the resumption data under TLS1.3\n"
    "       --ca-auto-retrieve     Enable automatic retrieval of missing CA certificates\n"
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
    "Operands and options may be intermixed.  They will be reordered.\n"
    "\n"
    "Simple client program to set up a TLS connection to some other computer. \n"
    "It sets up a TLS connection and forwards data from the standard input to the secured socket and vice versa.\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
