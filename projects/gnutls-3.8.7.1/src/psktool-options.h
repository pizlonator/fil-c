#include <stdbool.h>
#include <stdio.h>

#ifndef PSKTOOL_OPTIONS_H_
#define PSKTOOL_OPTIONS_H_ 1

struct psktool_list
{
  const char **args;
  unsigned int count;
};

struct psktool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool keysize;
    bool username;
    bool pskfile;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *keysize;
    const char *username;
    const char *pskfile;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int keysize;
  } value;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool keysize;
    bool username;
    bool pskfile;
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

#define HAVE_OPT_DEBUG psktool_options.present.debug
#define HAVE_OPT_KEYSIZE psktool_options.present.keysize
#define HAVE_OPT_USERNAME psktool_options.present.username
#define HAVE_OPT_PSKFILE psktool_options.present.pskfile
#define HAVE_OPT_VERSION psktool_options.present.version
#define HAVE_OPT_HELP psktool_options.present.help
#define HAVE_OPT_MORE_HELP psktool_options.present.more_help

#define OPT_ARG_DEBUG psktool_options.arg.debug
#define OPT_ARG_KEYSIZE psktool_options.arg.keysize
#define OPT_ARG_USERNAME psktool_options.arg.username
#define OPT_ARG_PSKFILE psktool_options.arg.pskfile
#define OPT_ARG_VERSION psktool_options.arg.version
#define OPT_ARG_HELP psktool_options.arg.help
#define OPT_ARG_MORE_HELP psktool_options.arg.more_help

#define OPT_VALUE_DEBUG psktool_options.value.debug
#define OPT_VALUE_KEYSIZE psktool_options.value.keysize

#define ENABLED_OPT_DEBUG psktool_options.enabled.debug
#define ENABLED_OPT_KEYSIZE psktool_options.enabled.keysize
#define ENABLED_OPT_USERNAME psktool_options.enabled.username
#define ENABLED_OPT_PSKFILE psktool_options.enabled.pskfile
#define ENABLED_OPT_VERSION psktool_options.enabled.version
#define ENABLED_OPT_HELP psktool_options.enabled.help
#define ENABLED_OPT_MORE_HELP psktool_options.enabled.more_help




extern struct psktool_options psktool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* PSKTOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
