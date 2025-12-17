#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "certtool-options.h"
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

struct certtool_options certtool_options;

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
  { "attime", required_argument, 0, CHAR_MAX + 3 },
  { "certificate-info", no_argument, 0, 'i' },
  { "pubkey-info", no_argument, 0, CHAR_MAX + 4 },
  { "generate-self-signed", no_argument, 0, 's' },
  { "generate-certificate", no_argument, 0, 'c' },
  { "generate-proxy", no_argument, 0, CHAR_MAX + 5 },
  { "update-certificate", no_argument, 0, 'u' },
  { "fingerprint", no_argument, 0, CHAR_MAX + 6 },
  { "key-id", no_argument, 0, CHAR_MAX + 7 },
  { "certificate-pubkey", no_argument, 0, CHAR_MAX + 8 },
  { "v1", no_argument, 0, CHAR_MAX + 9 },
  { "sign-params", required_argument, 0, CHAR_MAX + 10 },
  { "crq-info", no_argument, 0, CHAR_MAX + 11 },
  { "generate-request", no_argument, 0, 'q' },
  { "no-crq-extensions", no_argument, 0, CHAR_MAX + 12 },
  { "p12-info", no_argument, 0, CHAR_MAX + 13 },
  { "p12-name", required_argument, 0, CHAR_MAX + 14 },
  { "to-p12", no_argument, 0, CHAR_MAX + 15 },
  { "pbmac1", no_argument, 0, CHAR_MAX + 16 },
  { "key-info", no_argument, 0, 'k' },
  { "p8-info", no_argument, 0, CHAR_MAX + 17 },
  { "to-rsa", no_argument, 0, CHAR_MAX + 18 },
  { "generate-privkey", no_argument, 0, 'p' },
  { "key-type", required_argument, 0, CHAR_MAX + 19 },
  { "bits", required_argument, 0, CHAR_MAX + 20 },
  { "curve", required_argument, 0, CHAR_MAX + 21 },
  { "sec-param", required_argument, 0, CHAR_MAX + 22 },
  { "to-p8", no_argument, 0, CHAR_MAX + 23 },
  { "pkcs8", no_argument, 0, '8' },
  { "provable", no_argument, 0, CHAR_MAX + 24 },
  { "verify-provable-privkey", no_argument, 0, CHAR_MAX + 25 },
  { "seed", required_argument, 0, CHAR_MAX + 26 },
  { "crl-info", no_argument, 0, 'l' },
  { "generate-crl", no_argument, 0, CHAR_MAX + 27 },
  { "verify-crl", no_argument, 0, CHAR_MAX + 28 },
  { "verify-chain", no_argument, 0, 'e' },
  { "verify", no_argument, 0, CHAR_MAX + 29 },
  { "verify-hostname", required_argument, 0, CHAR_MAX + 30 },
  { "verify-email", required_argument, 0, CHAR_MAX + 31 },
  { "verify-purpose", required_argument, 0, CHAR_MAX + 32 },
  { "verify-allow-broken", no_argument, 0, CHAR_MAX + 33 },
  { "verify-profile", required_argument, 0, CHAR_MAX + 34 },
  { "p7-generate", no_argument, 0, CHAR_MAX + 35 },
  { "p7-sign", no_argument, 0, CHAR_MAX + 36 },
  { "p7-detached-sign", no_argument, 0, CHAR_MAX + 37 },
  { "p7-include-cert", no_argument, 0, CHAR_MAX + 38 },
  { "no-p7-include-cert", no_argument, 0, CHAR_MAX + 39 },
  { "p7-time", no_argument, 0, CHAR_MAX + 40 },
  { "no-p7-time", no_argument, 0, CHAR_MAX + 41 },
  { "p7-show-data", no_argument, 0, CHAR_MAX + 42 },
  { "no-p7-show-data", no_argument, 0, CHAR_MAX + 43 },
  { "p7-info", no_argument, 0, CHAR_MAX + 44 },
  { "p7-verify", no_argument, 0, CHAR_MAX + 45 },
  { "smime-to-p7", no_argument, 0, CHAR_MAX + 46 },
  { "generate-dh-params", no_argument, 0, CHAR_MAX + 47 },
  { "get-dh-params", no_argument, 0, CHAR_MAX + 48 },
  { "dh-info", no_argument, 0, CHAR_MAX + 49 },
  { "load-privkey", required_argument, 0, CHAR_MAX + 50 },
  { "load-pubkey", required_argument, 0, CHAR_MAX + 51 },
  { "load-request", required_argument, 0, CHAR_MAX + 52 },
  { "load-certificate", required_argument, 0, CHAR_MAX + 53 },
  { "load-ca-privkey", required_argument, 0, CHAR_MAX + 54 },
  { "load-ca-certificate", required_argument, 0, CHAR_MAX + 55 },
  { "load-crl", required_argument, 0, CHAR_MAX + 56 },
  { "load-data", required_argument, 0, CHAR_MAX + 57 },
  { "password", required_argument, 0, CHAR_MAX + 58 },
  { "null-password", no_argument, 0, CHAR_MAX + 59 },
  { "empty-password", no_argument, 0, CHAR_MAX + 60 },
  { "hex-numbers", no_argument, 0, CHAR_MAX + 61 },
  { "cprint", no_argument, 0, CHAR_MAX + 62 },
  { "rsa", no_argument, 0, CHAR_MAX + 63 },
  { "dsa", no_argument, 0, CHAR_MAX + 64 },
  { "ecc", no_argument, 0, CHAR_MAX + 65 },
  { "ecdsa", no_argument, 0, CHAR_MAX + 66 },
  { "hash", required_argument, 0, CHAR_MAX + 67 },
  { "salt-size", required_argument, 0, CHAR_MAX + 68 },
  { "label", required_argument, 0, CHAR_MAX + 69 },
  { "inder", no_argument, 0, CHAR_MAX + 70 },
  { "inraw", no_argument, 0, CHAR_MAX + 72 },
  { "no-inder", no_argument, 0, CHAR_MAX + 71 },
  { "outder", no_argument, 0, CHAR_MAX + 73 },
  { "outraw", no_argument, 0, CHAR_MAX + 75 },
  { "no-outder", no_argument, 0, CHAR_MAX + 74 },
  { "disable-quick-random", no_argument, 0, CHAR_MAX + 76 },
  { "template", required_argument, 0, CHAR_MAX + 77 },
  { "stdout-info", no_argument, 0, CHAR_MAX + 78 },
  { "ask-pass", no_argument, 0, CHAR_MAX + 79 },
  { "pkcs-cipher", required_argument, 0, CHAR_MAX + 80 },
  { "provider", required_argument, 0, CHAR_MAX + 81 },
  { "text", no_argument, 0, CHAR_MAX + 82 },
  { "no-text", no_argument, 0, CHAR_MAX + 83 },
  { "version", optional_argument, 0, 'v' },
  { "help", no_argument, 0, 'h' },
  { "more-help", no_argument, 0, '!' },
  { 0, 0, 0, 0 }

};

int
process_options (int argc, char **argv)
{
  struct certtool_options *opts = &certtool_options;
  int opt;

  opts->enabled.p7_include_cert = true;
  opts->enabled.text = true;
  while ((opt = getopt_long (argc, argv, "!8Vcd:ehiklpqsuv:",
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
      case CHAR_MAX + 3: /* --attime */
        opts->present.attime = true;
        opts->arg.attime = optarg;
        opts->enabled.attime = true;
        break;
      case 'i':
        opts->present.certificate_info = true;
        opts->enabled.certificate_info = true;
        break;
      case CHAR_MAX + 4: /* --pubkey-info */
        opts->present.pubkey_info = true;
        opts->enabled.pubkey_info = true;
        break;
      case 's':
        opts->present.generate_self_signed = true;
        opts->enabled.generate_self_signed = true;
        break;
      case 'c':
        opts->present.generate_certificate = true;
        opts->enabled.generate_certificate = true;
        break;
      case CHAR_MAX + 5: /* --generate-proxy */
        opts->present.generate_proxy = true;
        opts->enabled.generate_proxy = true;
        break;
      case 'u':
        opts->present.update_certificate = true;
        opts->enabled.update_certificate = true;
        break;
      case CHAR_MAX + 6: /* --fingerprint */
        opts->present.fingerprint = true;
        opts->enabled.fingerprint = true;
        break;
      case CHAR_MAX + 7: /* --key-id */
        opts->present.key_id = true;
        opts->enabled.key_id = true;
        break;
      case CHAR_MAX + 8: /* --certificate-pubkey */
        opts->present.certificate_pubkey = true;
        opts->enabled.certificate_pubkey = true;
        break;
      case CHAR_MAX + 9: /* --v1 */
        opts->present.v1 = true;
        opts->enabled.v1 = true;
        break;
      case CHAR_MAX + 10: /* --sign-params */
        opts->present.sign_params = true;
        opts->arg.sign_params = optarg;
        opts->enabled.sign_params = true;
        break;
      case CHAR_MAX + 11: /* --crq-info */
        opts->present.crq_info = true;
        opts->enabled.crq_info = true;
        break;
      case 'q':
        opts->present.generate_request = true;
        opts->enabled.generate_request = true;
        break;
      case CHAR_MAX + 12: /* --no-crq-extensions */
        opts->present.no_crq_extensions = true;
        opts->enabled.no_crq_extensions = true;
        break;
      case CHAR_MAX + 13: /* --p12-info */
        opts->present.p12_info = true;
        opts->enabled.p12_info = true;
        break;
      case CHAR_MAX + 14: /* --p12-name */
        opts->present.p12_name = true;
        opts->arg.p12_name = optarg;
        opts->enabled.p12_name = true;
        break;
      case CHAR_MAX + 15: /* --to-p12 */
        opts->present.to_p12 = true;
        opts->enabled.to_p12 = true;
        break;
      case CHAR_MAX + 16: /* --pbmac1 */
        opts->present.pbmac1 = true;
        opts->enabled.pbmac1 = true;
        break;
      case 'k':
        opts->present.key_info = true;
        opts->enabled.key_info = true;
        break;
      case CHAR_MAX + 17: /* --p8-info */
        opts->present.p8_info = true;
        opts->enabled.p8_info = true;
        break;
      case CHAR_MAX + 18: /* --to-rsa */
        opts->present.to_rsa = true;
        opts->enabled.to_rsa = true;
        break;
      case 'p':
        opts->present.generate_privkey = true;
        opts->enabled.generate_privkey = true;
        break;
      case CHAR_MAX + 19: /* --key-type */
        opts->present.key_type = true;
        opts->arg.key_type = optarg;
        opts->enabled.key_type = true;
        break;
      case CHAR_MAX + 20: /* --bits */
        opts->present.bits = true;
        opts->arg.bits = optarg;
        opts->value.bits = parse_number(optarg);
        opts->enabled.bits = true;
        break;
      case CHAR_MAX + 21: /* --curve */
        opts->present.curve = true;
        opts->arg.curve = optarg;
        opts->enabled.curve = true;
        break;
      case CHAR_MAX + 22: /* --sec-param */
        opts->present.sec_param = true;
        opts->arg.sec_param = optarg;
        opts->enabled.sec_param = true;
        break;
      case CHAR_MAX + 23: /* --to-p8 */
        opts->present.to_p8 = true;
        opts->enabled.to_p8 = true;
        break;
      case '8':
        opts->present.pkcs8 = true;
        opts->enabled.pkcs8 = true;
        break;
      case CHAR_MAX + 24: /* --provable */
        opts->present.provable = true;
        opts->enabled.provable = true;
        break;
      case CHAR_MAX + 25: /* --verify-provable-privkey */
        opts->present.verify_provable_privkey = true;
        opts->enabled.verify_provable_privkey = true;
        break;
      case CHAR_MAX + 26: /* --seed */
        opts->present.seed = true;
        opts->arg.seed = optarg;
        opts->enabled.seed = true;
        break;
      case 'l':
        opts->present.crl_info = true;
        opts->enabled.crl_info = true;
        break;
      case CHAR_MAX + 27: /* --generate-crl */
        opts->present.generate_crl = true;
        opts->enabled.generate_crl = true;
        break;
      case CHAR_MAX + 28: /* --verify-crl */
        opts->present.verify_crl = true;
        opts->enabled.verify_crl = true;
        break;
      case 'e':
        opts->present.verify_chain = true;
        opts->enabled.verify_chain = true;
        break;
      case CHAR_MAX + 29: /* --verify */
        opts->present.verify = true;
        opts->enabled.verify = true;
        break;
      case CHAR_MAX + 30: /* --verify-hostname */
        opts->present.verify_hostname = true;
        opts->arg.verify_hostname = optarg;
        opts->enabled.verify_hostname = true;
        break;
      case CHAR_MAX + 31: /* --verify-email */
        opts->present.verify_email = true;
        opts->arg.verify_email = optarg;
        opts->enabled.verify_email = true;
        break;
      case CHAR_MAX + 32: /* --verify-purpose */
        opts->present.verify_purpose = true;
        opts->arg.verify_purpose = optarg;
        opts->enabled.verify_purpose = true;
        break;
      case CHAR_MAX + 33: /* --verify-allow-broken */
        opts->present.verify_allow_broken = true;
        opts->enabled.verify_allow_broken = true;
        break;
      case CHAR_MAX + 34: /* --verify-profile */
        opts->present.verify_profile = true;
        opts->arg.verify_profile = optarg;
        opts->enabled.verify_profile = true;
        break;
      case CHAR_MAX + 35: /* --p7-generate */
        opts->present.p7_generate = true;
        opts->enabled.p7_generate = true;
        break;
      case CHAR_MAX + 36: /* --p7-sign */
        opts->present.p7_sign = true;
        opts->enabled.p7_sign = true;
        break;
      case CHAR_MAX + 37: /* --p7-detached-sign */
        opts->present.p7_detached_sign = true;
        opts->enabled.p7_detached_sign = true;
        break;
      case CHAR_MAX + 38: /* --p7-include-cert */
        opts->present.p7_include_cert = true;
        opts->enabled.p7_include_cert = true;
        break;
      case CHAR_MAX + 39: /* --no-p7-include-cert */
        opts->present.p7_include_cert = true;
        opts->enabled.p7_include_cert = false;
        break;
      case CHAR_MAX + 40: /* --p7-time */
        opts->present.p7_time = true;
        opts->enabled.p7_time = true;
        break;
      case CHAR_MAX + 41: /* --no-p7-time */
        opts->present.p7_time = true;
        opts->enabled.p7_time = false;
        break;
      case CHAR_MAX + 42: /* --p7-show-data */
        opts->present.p7_show_data = true;
        opts->enabled.p7_show_data = true;
        break;
      case CHAR_MAX + 43: /* --no-p7-show-data */
        opts->present.p7_show_data = true;
        opts->enabled.p7_show_data = false;
        break;
      case CHAR_MAX + 44: /* --p7-info */
        opts->present.p7_info = true;
        opts->enabled.p7_info = true;
        break;
      case CHAR_MAX + 45: /* --p7-verify */
        opts->present.p7_verify = true;
        opts->enabled.p7_verify = true;
        break;
      case CHAR_MAX + 46: /* --smime-to-p7 */
        opts->present.smime_to_p7 = true;
        opts->enabled.smime_to_p7 = true;
        break;
      case CHAR_MAX + 47: /* --generate-dh-params */
        opts->present.generate_dh_params = true;
        opts->enabled.generate_dh_params = true;
        break;
      case CHAR_MAX + 48: /* --get-dh-params */
        opts->present.get_dh_params = true;
        opts->enabled.get_dh_params = true;
        break;
      case CHAR_MAX + 49: /* --dh-info */
        opts->present.dh_info = true;
        opts->enabled.dh_info = true;
        break;
      case CHAR_MAX + 50: /* --load-privkey */
        opts->present.load_privkey = true;
        opts->arg.load_privkey = optarg;
        opts->enabled.load_privkey = true;
        break;
      case CHAR_MAX + 51: /* --load-pubkey */
        opts->present.load_pubkey = true;
        opts->arg.load_pubkey = optarg;
        opts->enabled.load_pubkey = true;
        break;
      case CHAR_MAX + 52: /* --load-request */
        opts->present.load_request = true;
        opts->arg.load_request = optarg;
        opts->enabled.load_request = true;
        break;
      case CHAR_MAX + 53: /* --load-certificate */
        opts->present.load_certificate = true;
        opts->arg.load_certificate = optarg;
        opts->enabled.load_certificate = true;
        break;
      case CHAR_MAX + 54: /* --load-ca-privkey */
        opts->present.load_ca_privkey = true;
        opts->arg.load_ca_privkey = optarg;
        opts->enabled.load_ca_privkey = true;
        break;
      case CHAR_MAX + 55: /* --load-ca-certificate */
        opts->present.load_ca_certificate = true;
        opts->arg.load_ca_certificate = optarg;
        opts->enabled.load_ca_certificate = true;
        break;
      case CHAR_MAX + 56: /* --load-crl */
        opts->present.load_crl = true;
        opts->arg.load_crl = optarg;
        opts->enabled.load_crl = true;
        break;
      case CHAR_MAX + 57: /* --load-data */
        opts->present.load_data = true;
        opts->arg.load_data = optarg;
        opts->enabled.load_data = true;
        break;
      case CHAR_MAX + 58: /* --password */
        opts->present.password = true;
        opts->arg.password = optarg;
        opts->enabled.password = true;
        break;
      case CHAR_MAX + 59: /* --null-password */
        opts->present.null_password = true;
        opts->enabled.null_password = true;
        break;
      case CHAR_MAX + 60: /* --empty-password */
        opts->present.empty_password = true;
        opts->enabled.empty_password = true;
        break;
      case CHAR_MAX + 61: /* --hex-numbers */
        opts->present.hex_numbers = true;
        opts->enabled.hex_numbers = true;
        break;
      case CHAR_MAX + 62: /* --cprint */
        opts->present.cprint = true;
        opts->enabled.cprint = true;
        break;
      case CHAR_MAX + 63: /* --rsa */
        opts->present.rsa = true;
        opts->enabled.rsa = true;
        break;
      case CHAR_MAX + 64: /* --dsa */
        opts->present.dsa = true;
        opts->enabled.dsa = true;
        break;
      case CHAR_MAX + 65: /* --ecc */
      case CHAR_MAX + 66: /* --ecdsa */
        opts->present.ecc = true;
        opts->enabled.ecc = true;
        break;
      case CHAR_MAX + 67: /* --hash */
        opts->present.hash = true;
        opts->arg.hash = optarg;
        opts->enabled.hash = true;
        break;
      case CHAR_MAX + 68: /* --salt-size */
        opts->present.salt_size = true;
        opts->arg.salt_size = optarg;
        opts->value.salt_size = parse_number(optarg);
        opts->enabled.salt_size = true;
        break;
      case CHAR_MAX + 69: /* --label */
        opts->present.label = true;
        opts->arg.label = optarg;
        opts->enabled.label = true;
        break;
      case CHAR_MAX + 70: /* --inder */
      case CHAR_MAX + 72: /* --inraw */
        opts->present.inder = true;
        opts->enabled.inder = true;
        break;
      case CHAR_MAX + 71: /* --no-inder */
        opts->present.inder = true;
        opts->enabled.inder = false;
        break;
      case CHAR_MAX + 73: /* --outder */
      case CHAR_MAX + 75: /* --outraw */
        opts->present.outder = true;
        opts->enabled.outder = true;
        break;
      case CHAR_MAX + 74: /* --no-outder */
        opts->present.outder = true;
        opts->enabled.outder = false;
        break;
      case CHAR_MAX + 76: /* --disable-quick-random */
        opts->present.disable_quick_random = true;
        opts->enabled.disable_quick_random = true;
        break;
      case CHAR_MAX + 77: /* --template */
        opts->present.template = true;
        opts->arg.template = optarg;
        opts->enabled.template = true;
        break;
      case CHAR_MAX + 78: /* --stdout-info */
        opts->present.stdout_info = true;
        opts->enabled.stdout_info = true;
        break;
      case CHAR_MAX + 79: /* --ask-pass */
        opts->present.ask_pass = true;
        opts->enabled.ask_pass = true;
        break;
      case CHAR_MAX + 80: /* --pkcs-cipher */
        opts->present.pkcs_cipher = true;
        opts->arg.pkcs_cipher = optarg;
        opts->enabled.pkcs_cipher = true;
        break;
      case CHAR_MAX + 81: /* --provider */
        opts->present.provider = true;
        opts->arg.provider = optarg;
        opts->enabled.provider = true;
        break;
      case CHAR_MAX + 82: /* --text */
        opts->present.text = true;
        opts->enabled.text = true;
        break;
      case CHAR_MAX + 83: /* --no-text */
        opts->present.text = true;
        opts->enabled.text = false;
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
  if (HAVE_OPT(GENERATE_REQUEST) && HAVE_OPT(INFILE))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "generate-request", "infile");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(VERIFY_CRL) && !HAVE_OPT(LOAD_CA_CERTIFICATE))
    {
      fprintf (stderr, "%s option requires the %s options\n",
               "verify-crl", "load_ca_certificate");
      exit (EXIT_FAILURE);
    }
  if (HAVE_OPT(VERIFY_EMAIL) && HAVE_OPT(VERIFY_HOSTNAME))
    {
      fprintf (stderr, "the '%s' and '%s' options conflict\n",
               "verify-email", "verify_hostname");
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
            "certtool 3.8.7\n"
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
            "certtool 3.8.7\n";
          fprintf (stdout, "%s", str);
          exit(0);
        }
      else if (!strcmp (OPT_ARG_VERSION, "n"))
        {
          const char str[] =
            "certtool 3.8.7\n"
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
    "certtool - GnuTLS certificate tool\n"
    "Usage:  certtool [ -<flag> [<val>] | --<name>[{=| }<val>] ]... \n"
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
    "       --attime=str           Perform validation at the timestamp instead of the system time\n"
    "\n"
    "Certificate related options:\n"
    "\n"
    "   -i, --certificate-info     Print information on the given certificate\n"
    "       --pubkey-info          Print information on a public key\n"
    "   -s, --generate-self-signed Generate a self-signed certificate\n"
    "   -c, --generate-certificate Generate a signed certificate\n"
    "       --generate-proxy       Generates a proxy certificate\n"
    "   -u, --update-certificate   Update a signed certificate\n"
    "       --fingerprint          Print the fingerprint of the given certificate\n"
    "       --key-id               Print the key ID of the given certificate\n"
    "       --v1                   Generate an X.509 version 1 certificate (with no extensions)\n"
    "       --sign-params=str      Sign a certificate with a specific signature algorithm\n"
    "\n"
    "Certificate request related options:\n"
    "\n"
    "       --crq-info             Print information on the given certificate request\n"
    "   -q, --generate-request     Generate a PKCS #10 certificate request\n"
    "				- prohibits the option 'infile'\n"
    "       --no-crq-extensions    Do not use extensions in certificate requests\n"
    "\n"
    "PKCS#12 file related options:\n"
    "\n"
    "       --p12-info             Print information on a PKCS #12 structure\n"
    "       --p12-name=str         The PKCS #12 friendly name to use\n"
    "       --to-p12               Generate a PKCS #12 structure\n"
    "       --pbmac1               Use PBMAC1 in a PKCS #12 structure\n"
    "\n"
    "Private key related options:\n"
    "\n"
    "   -k, --key-info             Print information on a private key\n"
    "       --p8-info              Print information on a PKCS #8 structure\n"
    "       --to-rsa               Convert an RSA-PSS key to raw RSA format\n"
    "   -p, --generate-privkey     Generate a private key\n"
    "       --key-type=str         Specify the key type to use on key generation\n"
    "       --bits=num             Specify the number of bits for key generation\n"
    "       --curve=str            Specify the curve used for EC key generation\n"
    "       --sec-param=str        Specify the security level [low, legacy, medium, high, ultra]\n"
    "       --to-p8                Convert a given key to a PKCS #8 structure\n"
    "   -8, --pkcs8                Use PKCS #8 format for private keys\n"
    "       --provable             Generate a private key or parameters from a seed using a provable method\n"
    "       --verify-provable-privkey  Verify a private key generated from a seed using a provable method\n"
    "       --seed=str             When generating a private key use the given hex-encoded seed\n"
    "\n"
    "CRL related options:\n"
    "\n"
    "   -l, --crl-info             Print information on the given CRL structure\n"
    "       --generate-crl         Generate a CRL\n"
    "       --verify-crl           Verify a Certificate Revocation List using a trusted list\n"
    "				- requires the option 'load-ca-certificate'\n"
    "\n"
    "Certificate verification related options:\n"
    "\n"
    "   -e, --verify-chain         Verify a PEM encoded certificate chain\n"
    "       --verify               Verify a PEM encoded certificate (chain) against a trusted set\n"
    "       --verify-hostname=str  Specify a hostname to be used for certificate chain verification\n"
    "       --verify-email=str     Specify a email to be used for certificate chain verification\n"
    "				- prohibits the option 'verify-hostname'\n"
    "       --verify-purpose=str   Specify a purpose OID to be used for certificate chain verification\n"
    "       --verify-allow-broken  Allow broken algorithms, such as MD5 for verification\n"
    "       --verify-profile=str   Specify a security level profile to be used for verification\n"
    "\n"
    "PKCS#7 structure options:\n"
    "\n"
    "       --p7-generate          Generate a PKCS #7 structure\n"
    "       --p7-sign              Signs using a PKCS #7 structure\n"
    "       --p7-detached-sign     Signs using a detached PKCS #7 structure\n"
    "       --p7-include-cert      The signer's certificate will be included in the cert list\n"
    "				- enabled by default\n"
    "				- disabled as '--no-p7-include-cert'\n"
    "       --p7-time              Will include a timestamp in the PKCS #7 structure\n"
    "       --p7-show-data         Will show the embedded data in the PKCS #7 structure\n"
    "       --p7-info              Print information on a PKCS #7 structure\n"
    "       --p7-verify            Verify the provided PKCS #7 structure\n"
    "       --smime-to-p7          Convert S/MIME to PKCS #7 structure\n"
    "\n"
    "Other options:\n"
    "\n"
    "       --get-dh-params        List the included PKCS #3 encoded Diffie-Hellman parameters\n"
    "       --dh-info              Print information PKCS #3 encoded Diffie-Hellman parameters\n"
    "       --load-privkey=str     Loads a private key file\n"
    "       --load-pubkey=str      Loads a public key file\n"
    "       --load-request=str     Loads a certificate request file\n"
    "       --load-certificate=str Loads a certificate file\n"
    "       --load-ca-privkey=str  Loads the certificate authority's private key file\n"
    "       --load-ca-certificate=str Loads the certificate authority's certificate file\n"
    "       --load-crl=str         Loads the provided CRL\n"
    "       --load-data=str        Loads auxiliary data\n"
    "       --password=str         Password to use\n"
    "       --null-password        Enforce a NULL password\n"
    "       --empty-password       Enforce an empty password\n"
    "       --hex-numbers          Print big number in an easier format to parse\n"
    "       --cprint               In certain operations it prints the information in C-friendly format\n"
    "       --hash=str             Hash algorithm to use for signing\n"
    "       --salt-size=num        Specify the RSA-PSS key default salt size\n"
    "       --label=str            Specify the RSA-OAEP label, encoded in hexadecimal\n"
    "       --inder                Use DER format for input certificates, private keys, and DH parameters \n"
    "       --inraw                an alias for the 'inder' option\n"
    "       --outder               Use DER format for output certificates, private keys, and DH parameters\n"
    "       --outraw               an alias for the 'outder' option\n"
    "       --template=str         Template file to use for non-interactive operation\n"
    "       --stdout-info          Print information to stdout instead of stderr\n"
    "       --ask-pass             Enable interaction for entering password when in batch mode\n"
    "       --pkcs-cipher=str      Cipher to use for PKCS #8 and #12 operations\n"
    "       --provider=str         Specify the PKCS #11 provider library\n"
    "       --text                 Output textual information before PEM-encoded certificates, private keys, etc\n"
    "				- enabled by default\n"
    "				- disabled as '--no-text'\n"
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
    "Tool to parse and generate X.509 certificates, requests and private keys.\n"
    "It can be used interactively or non interactively by\n"
    "specifying the template command line option.\n"
    "\n"
    "The tool accepts files or supported URIs via the --infile option. In case PIN\n"
    "is required for URI access you can provide it using the environment variables GNUTLS_PIN \n"
    "and GNUTLS_SO_PIN.\n"
    "\n"
    "\n"
    "Please send bug reports to:  <bugs@gnutls.org>\n"
    "\n";
  fprintf (out, "%s", str);
  exit (status);
}
