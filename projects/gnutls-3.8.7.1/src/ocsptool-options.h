#include <stdbool.h>
#include <stdio.h>

#ifndef OCSPTOOL_OPTIONS_H_
#define OCSPTOOL_OPTIONS_H_ 1

struct ocsptool_list
{
  const char **args;
  unsigned int count;
};

struct ocsptool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool verbose;
    bool infile;
    bool outfile;
    bool ask;
    bool verify_response;
    bool request_info;
    bool response_info;
    bool generate_request;
    bool nonce;
    bool load_chain;
    bool load_issuer;
    bool load_cert;
    bool load_trust;
    bool load_signer;
    bool inder;
    bool outder;
    bool outpem;
    bool load_request;
    bool load_response;
    bool ignore_errors;
    bool verify_allow_broken;
    bool attime;
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
    const char *ask;
    const char *load_chain;
    const char *load_issuer;
    const char *load_cert;
    const char *load_trust;
    const char *load_signer;
    const char *load_request;
    const char *load_response;
    const char *attime;
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
    bool infile;
    bool outfile;
    bool ask;
    bool verify_response;
    bool request_info;
    bool response_info;
    bool generate_request;
    bool nonce;
    bool load_chain;
    bool load_issuer;
    bool load_cert;
    bool load_trust;
    bool load_signer;
    bool inder;
    bool outder;
    bool outpem;
    bool load_request;
    bool load_response;
    bool ignore_errors;
    bool verify_allow_broken;
    bool attime;
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

#define HAVE_OPT_DEBUG ocsptool_options.present.debug
#define HAVE_OPT_VERBOSE ocsptool_options.present.verbose
#define HAVE_OPT_INFILE ocsptool_options.present.infile
#define HAVE_OPT_OUTFILE ocsptool_options.present.outfile
#define HAVE_OPT_ASK ocsptool_options.present.ask
#define HAVE_OPT_VERIFY_RESPONSE ocsptool_options.present.verify_response
#define HAVE_OPT_REQUEST_INFO ocsptool_options.present.request_info
#define HAVE_OPT_RESPONSE_INFO ocsptool_options.present.response_info
#define HAVE_OPT_GENERATE_REQUEST ocsptool_options.present.generate_request
#define HAVE_OPT_NONCE ocsptool_options.present.nonce
#define HAVE_OPT_LOAD_CHAIN ocsptool_options.present.load_chain
#define HAVE_OPT_LOAD_ISSUER ocsptool_options.present.load_issuer
#define HAVE_OPT_LOAD_CERT ocsptool_options.present.load_cert
#define HAVE_OPT_LOAD_TRUST ocsptool_options.present.load_trust
#define HAVE_OPT_LOAD_SIGNER ocsptool_options.present.load_signer
#define HAVE_OPT_INDER ocsptool_options.present.inder
#define HAVE_OPT_OUTDER ocsptool_options.present.outder
#define HAVE_OPT_OUTPEM ocsptool_options.present.outpem
#define HAVE_OPT_LOAD_REQUEST ocsptool_options.present.load_request
#define HAVE_OPT_LOAD_RESPONSE ocsptool_options.present.load_response
#define HAVE_OPT_IGNORE_ERRORS ocsptool_options.present.ignore_errors
#define HAVE_OPT_VERIFY_ALLOW_BROKEN ocsptool_options.present.verify_allow_broken
#define HAVE_OPT_ATTIME ocsptool_options.present.attime
#define HAVE_OPT_VERSION ocsptool_options.present.version
#define HAVE_OPT_HELP ocsptool_options.present.help
#define HAVE_OPT_MORE_HELP ocsptool_options.present.more_help

#define OPT_ARG_DEBUG ocsptool_options.arg.debug
#define OPT_ARG_VERBOSE ocsptool_options.arg.verbose
#define OPT_ARG_INFILE ocsptool_options.arg.infile
#define OPT_ARG_OUTFILE ocsptool_options.arg.outfile
#define OPT_ARG_ASK ocsptool_options.arg.ask
#define OPT_ARG_VERIFY_RESPONSE ocsptool_options.arg.verify_response
#define OPT_ARG_REQUEST_INFO ocsptool_options.arg.request_info
#define OPT_ARG_RESPONSE_INFO ocsptool_options.arg.response_info
#define OPT_ARG_GENERATE_REQUEST ocsptool_options.arg.generate_request
#define OPT_ARG_NONCE ocsptool_options.arg.nonce
#define OPT_ARG_LOAD_CHAIN ocsptool_options.arg.load_chain
#define OPT_ARG_LOAD_ISSUER ocsptool_options.arg.load_issuer
#define OPT_ARG_LOAD_CERT ocsptool_options.arg.load_cert
#define OPT_ARG_LOAD_TRUST ocsptool_options.arg.load_trust
#define OPT_ARG_LOAD_SIGNER ocsptool_options.arg.load_signer
#define OPT_ARG_INDER ocsptool_options.arg.inder
#define OPT_ARG_OUTDER ocsptool_options.arg.outder
#define OPT_ARG_OUTPEM ocsptool_options.arg.outpem
#define OPT_ARG_LOAD_REQUEST ocsptool_options.arg.load_request
#define OPT_ARG_LOAD_RESPONSE ocsptool_options.arg.load_response
#define OPT_ARG_IGNORE_ERRORS ocsptool_options.arg.ignore_errors
#define OPT_ARG_VERIFY_ALLOW_BROKEN ocsptool_options.arg.verify_allow_broken
#define OPT_ARG_ATTIME ocsptool_options.arg.attime
#define OPT_ARG_VERSION ocsptool_options.arg.version
#define OPT_ARG_HELP ocsptool_options.arg.help
#define OPT_ARG_MORE_HELP ocsptool_options.arg.more_help

#define OPT_VALUE_DEBUG ocsptool_options.value.debug

#define ENABLED_OPT_DEBUG ocsptool_options.enabled.debug
#define ENABLED_OPT_VERBOSE ocsptool_options.enabled.verbose
#define ENABLED_OPT_INFILE ocsptool_options.enabled.infile
#define ENABLED_OPT_OUTFILE ocsptool_options.enabled.outfile
#define ENABLED_OPT_ASK ocsptool_options.enabled.ask
#define ENABLED_OPT_VERIFY_RESPONSE ocsptool_options.enabled.verify_response
#define ENABLED_OPT_REQUEST_INFO ocsptool_options.enabled.request_info
#define ENABLED_OPT_RESPONSE_INFO ocsptool_options.enabled.response_info
#define ENABLED_OPT_GENERATE_REQUEST ocsptool_options.enabled.generate_request
#define ENABLED_OPT_NONCE ocsptool_options.enabled.nonce
#define ENABLED_OPT_LOAD_CHAIN ocsptool_options.enabled.load_chain
#define ENABLED_OPT_LOAD_ISSUER ocsptool_options.enabled.load_issuer
#define ENABLED_OPT_LOAD_CERT ocsptool_options.enabled.load_cert
#define ENABLED_OPT_LOAD_TRUST ocsptool_options.enabled.load_trust
#define ENABLED_OPT_LOAD_SIGNER ocsptool_options.enabled.load_signer
#define ENABLED_OPT_INDER ocsptool_options.enabled.inder
#define ENABLED_OPT_OUTDER ocsptool_options.enabled.outder
#define ENABLED_OPT_OUTPEM ocsptool_options.enabled.outpem
#define ENABLED_OPT_LOAD_REQUEST ocsptool_options.enabled.load_request
#define ENABLED_OPT_LOAD_RESPONSE ocsptool_options.enabled.load_response
#define ENABLED_OPT_IGNORE_ERRORS ocsptool_options.enabled.ignore_errors
#define ENABLED_OPT_VERIFY_ALLOW_BROKEN ocsptool_options.enabled.verify_allow_broken
#define ENABLED_OPT_ATTIME ocsptool_options.enabled.attime
#define ENABLED_OPT_VERSION ocsptool_options.enabled.version
#define ENABLED_OPT_HELP ocsptool_options.enabled.help
#define ENABLED_OPT_MORE_HELP ocsptool_options.enabled.more_help




extern struct ocsptool_options ocsptool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* OCSPTOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
