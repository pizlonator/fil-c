#include <stdbool.h>
#include <stdio.h>

#ifndef SYSTEMKEY_TOOL_OPTIONS_H_
#define SYSTEMKEY_TOOL_OPTIONS_H_ 1

struct systemkey_tool_list
{
  const char **args;
  unsigned int count;
};

struct systemkey_tool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool outfile;
    bool list;
    bool delete;
    bool outder;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *outfile;
    const char *delete;
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
    bool outfile;
    bool list;
    bool delete;
    bool outder;
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

#define HAVE_OPT_DEBUG systemkey_tool_options.present.debug
#define HAVE_OPT_OUTFILE systemkey_tool_options.present.outfile
#define HAVE_OPT_LIST systemkey_tool_options.present.list
#define HAVE_OPT_DELETE systemkey_tool_options.present.delete
#define HAVE_OPT_OUTDER systemkey_tool_options.present.outder
#define HAVE_OPT_VERSION systemkey_tool_options.present.version
#define HAVE_OPT_HELP systemkey_tool_options.present.help
#define HAVE_OPT_MORE_HELP systemkey_tool_options.present.more_help

#define OPT_ARG_DEBUG systemkey_tool_options.arg.debug
#define OPT_ARG_OUTFILE systemkey_tool_options.arg.outfile
#define OPT_ARG_LIST systemkey_tool_options.arg.list
#define OPT_ARG_DELETE systemkey_tool_options.arg.delete
#define OPT_ARG_OUTDER systemkey_tool_options.arg.outder
#define OPT_ARG_VERSION systemkey_tool_options.arg.version
#define OPT_ARG_HELP systemkey_tool_options.arg.help
#define OPT_ARG_MORE_HELP systemkey_tool_options.arg.more_help

#define OPT_VALUE_DEBUG systemkey_tool_options.value.debug

#define ENABLED_OPT_DEBUG systemkey_tool_options.enabled.debug
#define ENABLED_OPT_OUTFILE systemkey_tool_options.enabled.outfile
#define ENABLED_OPT_LIST systemkey_tool_options.enabled.list
#define ENABLED_OPT_DELETE systemkey_tool_options.enabled.delete
#define ENABLED_OPT_OUTDER systemkey_tool_options.enabled.outder
#define ENABLED_OPT_VERSION systemkey_tool_options.enabled.version
#define ENABLED_OPT_HELP systemkey_tool_options.enabled.help
#define ENABLED_OPT_MORE_HELP systemkey_tool_options.enabled.more_help




extern struct systemkey_tool_options systemkey_tool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* SYSTEMKEY_TOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
