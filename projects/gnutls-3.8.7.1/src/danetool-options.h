#include <stdbool.h>
#include <stdio.h>

#ifndef DANETOOL_OPTIONS_H_
#define DANETOOL_OPTIONS_H_ 1

struct danetool_list
{
  const char **args;
  unsigned int count;
};

struct danetool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool verbose;
    bool outfile;
    bool load_pubkey;
    bool load_certificate;
    bool dlv;
    bool hash;
    bool check;
    bool check_ee;
    bool check_ca;
    bool tlsa_rr;
    bool host;
    bool proto;
    bool port;
    bool starttls_proto;
    bool ca;
    bool x509;
    bool domain;
    bool local_dns;
    bool insecure;
    bool inder;
    bool print_raw;
    bool quiet;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *outfile;
    const char *load_pubkey;
    const char *load_certificate;
    const char *dlv;
    const char *hash;
    const char *check;
    const char *host;
    const char *proto;
    const char *port;
    const char *starttls_proto;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
  } value;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool verbose;
    bool outfile;
    bool load_pubkey;
    bool load_certificate;
    bool dlv;
    bool hash;
    bool check;
    bool check_ee;
    bool check_ca;
    bool tlsa_rr;
    bool host;
    bool proto;
    bool port;
    bool starttls_proto;
    bool ca;
    bool x509;
    bool domain;
    bool local_dns;
    bool insecure;
    bool inder;
    bool print_raw;
    bool quiet;
    bool version;
    bool help;
    bool more_help;
  } enabled;
};

#define HAVE_OPT(name) HAVE_OPT_ ## name
#define OPT_ARG(name) OPT_ARG_ ## name
#define ENABLED_OPT(name) ENABLED_OPT_ ## name
#define OPTS_COUNT(name) OPTS_COUNT_ ## name
#define OPTS_ARRAY(name) OPTS_ARRAY_ ## name
#define USAGE(status) usage (stdout, (status))

#define HAVE_OPT_DEBUG danetool_options.present.debug
#define HAVE_OPT_VERBOSE danetool_options.present.verbose
#define HAVE_OPT_OUTFILE danetool_options.present.outfile
#define HAVE_OPT_LOAD_PUBKEY danetool_options.present.load_pubkey
#define HAVE_OPT_LOAD_CERTIFICATE danetool_options.present.load_certificate
#define HAVE_OPT_DLV danetool_options.present.dlv
#define HAVE_OPT_HASH danetool_options.present.hash
#define HAVE_OPT_CHECK danetool_options.present.check
#define HAVE_OPT_CHECK_EE danetool_options.present.check_ee
#define HAVE_OPT_CHECK_CA danetool_options.present.check_ca
#define HAVE_OPT_TLSA_RR danetool_options.present.tlsa_rr
#define HAVE_OPT_HOST danetool_options.present.host
#define HAVE_OPT_PROTO danetool_options.present.proto
#define HAVE_OPT_PORT danetool_options.present.port
#define HAVE_OPT_STARTTLS_PROTO danetool_options.present.starttls_proto
#define HAVE_OPT_CA danetool_options.present.ca
#define HAVE_OPT_X509 danetool_options.present.x509
#define HAVE_OPT_DOMAIN danetool_options.present.domain
#define HAVE_OPT_LOCAL_DNS danetool_options.present.local_dns
#define HAVE_OPT_INSECURE danetool_options.present.insecure
#define HAVE_OPT_INDER danetool_options.present.inder
#define HAVE_OPT_PRINT_RAW danetool_options.present.print_raw
#define HAVE_OPT_QUIET danetool_options.present.quiet
#define HAVE_OPT_VERSION danetool_options.present.version
#define HAVE_OPT_HELP danetool_options.present.help
#define HAVE_OPT_MORE_HELP danetool_options.present.more_help

#define OPT_ARG_DEBUG danetool_options.arg.debug
#define OPT_ARG_VERBOSE danetool_options.arg.verbose
#define OPT_ARG_OUTFILE danetool_options.arg.outfile
#define OPT_ARG_LOAD_PUBKEY danetool_options.arg.load_pubkey
#define OPT_ARG_LOAD_CERTIFICATE danetool_options.arg.load_certificate
#define OPT_ARG_DLV danetool_options.arg.dlv
#define OPT_ARG_HASH danetool_options.arg.hash
#define OPT_ARG_CHECK danetool_options.arg.check
#define OPT_ARG_CHECK_EE danetool_options.arg.check_ee
#define OPT_ARG_CHECK_CA danetool_options.arg.check_ca
#define OPT_ARG_TLSA_RR danetool_options.arg.tlsa_rr
#define OPT_ARG_HOST danetool_options.arg.host
#define OPT_ARG_PROTO danetool_options.arg.proto
#define OPT_ARG_PORT danetool_options.arg.port
#define OPT_ARG_STARTTLS_PROTO danetool_options.arg.starttls_proto
#define OPT_ARG_CA danetool_options.arg.ca
#define OPT_ARG_X509 danetool_options.arg.x509
#define OPT_ARG_DOMAIN danetool_options.arg.domain
#define OPT_ARG_LOCAL_DNS danetool_options.arg.local_dns
#define OPT_ARG_INSECURE danetool_options.arg.insecure
#define OPT_ARG_INDER danetool_options.arg.inder
#define OPT_ARG_PRINT_RAW danetool_options.arg.print_raw
#define OPT_ARG_QUIET danetool_options.arg.quiet
#define OPT_ARG_VERSION danetool_options.arg.version
#define OPT_ARG_HELP danetool_options.arg.help
#define OPT_ARG_MORE_HELP danetool_options.arg.more_help

#define OPT_VALUE_DEBUG danetool_options.value.debug

#define ENABLED_OPT_DEBUG danetool_options.enabled.debug
#define ENABLED_OPT_VERBOSE danetool_options.enabled.verbose
#define ENABLED_OPT_OUTFILE danetool_options.enabled.outfile
#define ENABLED_OPT_LOAD_PUBKEY danetool_options.enabled.load_pubkey
#define ENABLED_OPT_LOAD_CERTIFICATE danetool_options.enabled.load_certificate
#define ENABLED_OPT_DLV danetool_options.enabled.dlv
#define ENABLED_OPT_HASH danetool_options.enabled.hash
#define ENABLED_OPT_CHECK danetool_options.enabled.check
#define ENABLED_OPT_CHECK_EE danetool_options.enabled.check_ee
#define ENABLED_OPT_CHECK_CA danetool_options.enabled.check_ca
#define ENABLED_OPT_TLSA_RR danetool_options.enabled.tlsa_rr
#define ENABLED_OPT_HOST danetool_options.enabled.host
#define ENABLED_OPT_PROTO danetool_options.enabled.proto
#define ENABLED_OPT_PORT danetool_options.enabled.port
#define ENABLED_OPT_STARTTLS_PROTO danetool_options.enabled.starttls_proto
#define ENABLED_OPT_CA danetool_options.enabled.ca
#define ENABLED_OPT_X509 danetool_options.enabled.x509
#define ENABLED_OPT_DOMAIN danetool_options.enabled.domain
#define ENABLED_OPT_LOCAL_DNS danetool_options.enabled.local_dns
#define ENABLED_OPT_INSECURE danetool_options.enabled.insecure
#define ENABLED_OPT_INDER danetool_options.enabled.inder
#define ENABLED_OPT_PRINT_RAW danetool_options.enabled.print_raw
#define ENABLED_OPT_QUIET danetool_options.enabled.quiet
#define ENABLED_OPT_VERSION danetool_options.enabled.version
#define ENABLED_OPT_HELP danetool_options.enabled.help
#define ENABLED_OPT_MORE_HELP danetool_options.enabled.more_help




extern struct danetool_options danetool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* DANETOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
