#include <stdbool.h>
#include <stdio.h>

#ifndef SRPTOOL_OPTIONS_H_
#define SRPTOOL_OPTIONS_H_ 1

struct srptool_list
{
  const char **args;
  unsigned int count;
};

struct srptool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool index;
    bool username;
    bool passwd;
    bool salt;
    bool verify;
    bool passwd_conf;
    bool create_conf;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *debug;
    const char *index;
    const char *username;
    const char *passwd;
    const char *salt;
    const char *passwd_conf;
    const char *create_conf;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int index;
    int salt;
  } value;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool index;
    bool username;
    bool passwd;
    bool salt;
    bool verify;
    bool passwd_conf;
    bool create_conf;
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

#define HAVE_OPT_DEBUG srptool_options.present.debug
#define HAVE_OPT_INDEX srptool_options.present.index
#define HAVE_OPT_USERNAME srptool_options.present.username
#define HAVE_OPT_PASSWD srptool_options.present.passwd
#define HAVE_OPT_SALT srptool_options.present.salt
#define HAVE_OPT_VERIFY srptool_options.present.verify
#define HAVE_OPT_PASSWD_CONF srptool_options.present.passwd_conf
#define HAVE_OPT_CREATE_CONF srptool_options.present.create_conf
#define HAVE_OPT_VERSION srptool_options.present.version
#define HAVE_OPT_HELP srptool_options.present.help
#define HAVE_OPT_MORE_HELP srptool_options.present.more_help

#define OPT_ARG_DEBUG srptool_options.arg.debug
#define OPT_ARG_INDEX srptool_options.arg.index
#define OPT_ARG_USERNAME srptool_options.arg.username
#define OPT_ARG_PASSWD srptool_options.arg.passwd
#define OPT_ARG_SALT srptool_options.arg.salt
#define OPT_ARG_VERIFY srptool_options.arg.verify
#define OPT_ARG_PASSWD_CONF srptool_options.arg.passwd_conf
#define OPT_ARG_CREATE_CONF srptool_options.arg.create_conf
#define OPT_ARG_VERSION srptool_options.arg.version
#define OPT_ARG_HELP srptool_options.arg.help
#define OPT_ARG_MORE_HELP srptool_options.arg.more_help

#define OPT_VALUE_DEBUG srptool_options.value.debug
#define OPT_VALUE_INDEX srptool_options.value.index
#define OPT_VALUE_SALT srptool_options.value.salt

#define ENABLED_OPT_DEBUG srptool_options.enabled.debug
#define ENABLED_OPT_INDEX srptool_options.enabled.index
#define ENABLED_OPT_USERNAME srptool_options.enabled.username
#define ENABLED_OPT_PASSWD srptool_options.enabled.passwd
#define ENABLED_OPT_SALT srptool_options.enabled.salt
#define ENABLED_OPT_VERIFY srptool_options.enabled.verify
#define ENABLED_OPT_PASSWD_CONF srptool_options.enabled.passwd_conf
#define ENABLED_OPT_CREATE_CONF srptool_options.enabled.create_conf
#define ENABLED_OPT_VERSION srptool_options.enabled.version
#define ENABLED_OPT_HELP srptool_options.enabled.help
#define ENABLED_OPT_MORE_HELP srptool_options.enabled.more_help




extern struct srptool_options srptool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* SRPTOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
