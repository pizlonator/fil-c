#include <stdbool.h>
#include <stdio.h>

#ifndef GNUTLS_CLI_DEBUG_OPTIONS_H_
#define GNUTLS_CLI_DEBUG_OPTIONS_H_ 1

struct gnutls_cli_debug_list
{
  const char **args;
  unsigned int count;
};

struct gnutls_cli_debug_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool verbose;
    bool port;
    bool starttls_proto;
    bool attime;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *port;
    const char *starttls_proto;
    const char *attime;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int port;
  } value;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool verbose;
    bool port;
    bool starttls_proto;
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

#define HAVE_OPT_DEBUG gnutls_cli_debug_options.present.debug
#define HAVE_OPT_VERBOSE gnutls_cli_debug_options.present.verbose
#define HAVE_OPT_PORT gnutls_cli_debug_options.present.port
#define HAVE_OPT_STARTTLS_PROTO gnutls_cli_debug_options.present.starttls_proto
#define HAVE_OPT_ATTIME gnutls_cli_debug_options.present.attime
#define HAVE_OPT_VERSION gnutls_cli_debug_options.present.version
#define HAVE_OPT_HELP gnutls_cli_debug_options.present.help
#define HAVE_OPT_MORE_HELP gnutls_cli_debug_options.present.more_help

#define OPT_ARG_DEBUG gnutls_cli_debug_options.arg.debug
#define OPT_ARG_VERBOSE gnutls_cli_debug_options.arg.verbose
#define OPT_ARG_PORT gnutls_cli_debug_options.arg.port
#define OPT_ARG_STARTTLS_PROTO gnutls_cli_debug_options.arg.starttls_proto
#define OPT_ARG_ATTIME gnutls_cli_debug_options.arg.attime
#define OPT_ARG_VERSION gnutls_cli_debug_options.arg.version
#define OPT_ARG_HELP gnutls_cli_debug_options.arg.help
#define OPT_ARG_MORE_HELP gnutls_cli_debug_options.arg.more_help

#define OPT_VALUE_DEBUG gnutls_cli_debug_options.value.debug
#define OPT_VALUE_PORT gnutls_cli_debug_options.value.port

#define ENABLED_OPT_DEBUG gnutls_cli_debug_options.enabled.debug
#define ENABLED_OPT_VERBOSE gnutls_cli_debug_options.enabled.verbose
#define ENABLED_OPT_PORT gnutls_cli_debug_options.enabled.port
#define ENABLED_OPT_STARTTLS_PROTO gnutls_cli_debug_options.enabled.starttls_proto
#define ENABLED_OPT_ATTIME gnutls_cli_debug_options.enabled.attime
#define ENABLED_OPT_VERSION gnutls_cli_debug_options.enabled.version
#define ENABLED_OPT_HELP gnutls_cli_debug_options.enabled.help
#define ENABLED_OPT_MORE_HELP gnutls_cli_debug_options.enabled.more_help




extern struct gnutls_cli_debug_options gnutls_cli_debug_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* GNUTLS_CLI_DEBUG_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
