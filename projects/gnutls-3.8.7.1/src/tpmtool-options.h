#include <stdbool.h>
#include <stdio.h>

#ifndef TPMTOOL_OPTIONS_H_
#define TPMTOOL_OPTIONS_H_ 1

struct tpmtool_list
{
  const char **args;
  unsigned int count;
};

struct tpmtool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool infile;
    bool outfile;
    bool generate_rsa;
    bool register_;
    bool signing;
    bool legacy;
    bool user;
    bool system;
    bool pubkey;
    bool list;
    bool delete;
    bool test_sign;
    bool sec_param;
    bool bits;
    bool inder;
    bool outder;
    bool srk_well_known;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *infile;
    const char *outfile;
    const char *pubkey;
    const char *delete;
    const char *test_sign;
    const char *sec_param;
    const char *bits;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int bits;
  } value;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool infile;
    bool outfile;
    bool generate_rsa;
    bool register_;
    bool signing;
    bool legacy;
    bool user;
    bool system;
    bool pubkey;
    bool list;
    bool delete;
    bool test_sign;
    bool sec_param;
    bool bits;
    bool inder;
    bool outder;
    bool srk_well_known;
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

#define HAVE_OPT_DEBUG tpmtool_options.present.debug
#define HAVE_OPT_INFILE tpmtool_options.present.infile
#define HAVE_OPT_OUTFILE tpmtool_options.present.outfile
#define HAVE_OPT_GENERATE_RSA tpmtool_options.present.generate_rsa
#define HAVE_OPT_REGISTER tpmtool_options.present.register_
#define HAVE_OPT_SIGNING tpmtool_options.present.signing
#define HAVE_OPT_LEGACY tpmtool_options.present.legacy
#define HAVE_OPT_USER tpmtool_options.present.user
#define HAVE_OPT_SYSTEM tpmtool_options.present.system
#define HAVE_OPT_PUBKEY tpmtool_options.present.pubkey
#define HAVE_OPT_LIST tpmtool_options.present.list
#define HAVE_OPT_DELETE tpmtool_options.present.delete
#define HAVE_OPT_TEST_SIGN tpmtool_options.present.test_sign
#define HAVE_OPT_SEC_PARAM tpmtool_options.present.sec_param
#define HAVE_OPT_BITS tpmtool_options.present.bits
#define HAVE_OPT_INDER tpmtool_options.present.inder
#define HAVE_OPT_OUTDER tpmtool_options.present.outder
#define HAVE_OPT_SRK_WELL_KNOWN tpmtool_options.present.srk_well_known
#define HAVE_OPT_VERSION tpmtool_options.present.version
#define HAVE_OPT_HELP tpmtool_options.present.help
#define HAVE_OPT_MORE_HELP tpmtool_options.present.more_help

#define OPT_ARG_DEBUG tpmtool_options.arg.debug
#define OPT_ARG_INFILE tpmtool_options.arg.infile
#define OPT_ARG_OUTFILE tpmtool_options.arg.outfile
#define OPT_ARG_GENERATE_RSA tpmtool_options.arg.generate_rsa
#define OPT_ARG_REGISTER tpmtool_options.arg.register_
#define OPT_ARG_SIGNING tpmtool_options.arg.signing
#define OPT_ARG_LEGACY tpmtool_options.arg.legacy
#define OPT_ARG_USER tpmtool_options.arg.user
#define OPT_ARG_SYSTEM tpmtool_options.arg.system
#define OPT_ARG_PUBKEY tpmtool_options.arg.pubkey
#define OPT_ARG_LIST tpmtool_options.arg.list
#define OPT_ARG_DELETE tpmtool_options.arg.delete
#define OPT_ARG_TEST_SIGN tpmtool_options.arg.test_sign
#define OPT_ARG_SEC_PARAM tpmtool_options.arg.sec_param
#define OPT_ARG_BITS tpmtool_options.arg.bits
#define OPT_ARG_INDER tpmtool_options.arg.inder
#define OPT_ARG_OUTDER tpmtool_options.arg.outder
#define OPT_ARG_SRK_WELL_KNOWN tpmtool_options.arg.srk_well_known
#define OPT_ARG_VERSION tpmtool_options.arg.version
#define OPT_ARG_HELP tpmtool_options.arg.help
#define OPT_ARG_MORE_HELP tpmtool_options.arg.more_help

#define OPT_VALUE_DEBUG tpmtool_options.value.debug
#define OPT_VALUE_BITS tpmtool_options.value.bits

#define ENABLED_OPT_DEBUG tpmtool_options.enabled.debug
#define ENABLED_OPT_INFILE tpmtool_options.enabled.infile
#define ENABLED_OPT_OUTFILE tpmtool_options.enabled.outfile
#define ENABLED_OPT_GENERATE_RSA tpmtool_options.enabled.generate_rsa
#define ENABLED_OPT_REGISTER tpmtool_options.enabled.register_
#define ENABLED_OPT_SIGNING tpmtool_options.enabled.signing
#define ENABLED_OPT_LEGACY tpmtool_options.enabled.legacy
#define ENABLED_OPT_USER tpmtool_options.enabled.user
#define ENABLED_OPT_SYSTEM tpmtool_options.enabled.system
#define ENABLED_OPT_PUBKEY tpmtool_options.enabled.pubkey
#define ENABLED_OPT_LIST tpmtool_options.enabled.list
#define ENABLED_OPT_DELETE tpmtool_options.enabled.delete
#define ENABLED_OPT_TEST_SIGN tpmtool_options.enabled.test_sign
#define ENABLED_OPT_SEC_PARAM tpmtool_options.enabled.sec_param
#define ENABLED_OPT_BITS tpmtool_options.enabled.bits
#define ENABLED_OPT_INDER tpmtool_options.enabled.inder
#define ENABLED_OPT_OUTDER tpmtool_options.enabled.outder
#define ENABLED_OPT_SRK_WELL_KNOWN tpmtool_options.enabled.srk_well_known
#define ENABLED_OPT_VERSION tpmtool_options.enabled.version
#define ENABLED_OPT_HELP tpmtool_options.enabled.help
#define ENABLED_OPT_MORE_HELP tpmtool_options.enabled.more_help




extern struct tpmtool_options tpmtool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* TPMTOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
