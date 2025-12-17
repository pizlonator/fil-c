#include <stdbool.h>
#include <stdio.h>

#ifndef P11TOOL_OPTIONS_H_
#define P11TOOL_OPTIONS_H_ 1

struct p11tool_list
{
  const char **args;
  unsigned int count;
};

struct p11tool_options
{
  /* Options present in the command line */
  struct
  {
    bool list_tokens;
    bool list_token_urls;
    bool list_mechanisms;
    bool initialize;
    bool initialize_pin;
    bool initialize_so_pin;
    bool set_pin;
    bool set_so_pin;
    bool list_all;
    bool list_all_certs;
    bool list_certs;
    bool list_all_privkeys;
    bool list_all_trusted;
    bool export;
    bool export_stapled;
    bool export_chain;
    bool export_pubkey;
    bool info;
    bool generate_privkey;
    bool generate_rsa;
    bool generate_dsa;
    bool generate_ecc;
    bool bits;
    bool curve;
    bool sec_param;
    bool set_id;
    bool set_label;
    bool write;
    bool delete;
    bool label;
    bool id;
    bool mark_wrap;
    bool mark_trusted;
    bool mark_distrusted;
    bool mark_decrypt;
    bool mark_sign;
    bool mark_ca;
    bool mark_private;
    bool mark_always_authenticate;
    bool secret_key;
    bool load_privkey;
    bool load_pubkey;
    bool load_certificate;
    bool debug;
    bool outfile;
    bool login;
    bool so_login;
    bool test_sign;
    bool sign_params;
    bool hash;
    bool generate_random;
    bool pkcs8;
    bool inder;
    bool outder;
    bool provider;
    bool provider_opts;
    bool detailed_url;
    bool only_urls;
    bool batch;
    bool version;
    bool help;
    bool more_help;
  } present;

  /* Option arguments in raw string form */
  struct
  {
    const char *set_pin;
    const char *set_so_pin;
    const char *generate_privkey;
    const char *bits;
    const char *curve;
    const char *sec_param;
    const char *set_id;
    const char *set_label;
    const char *label;
    const char *id;
    const char *secret_key;
    const char *load_privkey;
    const char *load_pubkey;
    const char *load_certificate;
    const char *debug;
    const char *outfile;
    const char *sign_params;
    const char *hash;
    const char *generate_random;
    const char *provider;
    const char *provider_opts;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int bits;
    int debug;
    int generate_random;
  } value;

  /* Option enablement status */
  struct
  {
    bool list_tokens;
    bool list_token_urls;
    bool list_mechanisms;
    bool initialize;
    bool initialize_pin;
    bool initialize_so_pin;
    bool set_pin;
    bool set_so_pin;
    bool list_all;
    bool list_all_certs;
    bool list_certs;
    bool list_all_privkeys;
    bool list_all_trusted;
    bool export;
    bool export_stapled;
    bool export_chain;
    bool export_pubkey;
    bool info;
    bool generate_privkey;
    bool generate_rsa;
    bool generate_dsa;
    bool generate_ecc;
    bool bits;
    bool curve;
    bool sec_param;
    bool set_id;
    bool set_label;
    bool write;
    bool delete;
    bool label;
    bool id;
    bool mark_wrap;
    bool mark_trusted;
    bool mark_distrusted;
    bool mark_decrypt;
    bool mark_sign;
    bool mark_ca;
    bool mark_private;
    bool mark_always_authenticate;
    bool secret_key;
    bool load_privkey;
    bool load_pubkey;
    bool load_certificate;
    bool debug;
    bool outfile;
    bool login;
    bool so_login;
    bool test_sign;
    bool sign_params;
    bool hash;
    bool generate_random;
    bool pkcs8;
    bool inder;
    bool outder;
    bool provider;
    bool provider_opts;
    bool detailed_url;
    bool only_urls;
    bool batch;
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

#define HAVE_OPT_LIST_TOKENS p11tool_options.present.list_tokens
#define HAVE_OPT_LIST_TOKEN_URLS p11tool_options.present.list_token_urls
#define HAVE_OPT_LIST_MECHANISMS p11tool_options.present.list_mechanisms
#define HAVE_OPT_INITIALIZE p11tool_options.present.initialize
#define HAVE_OPT_INITIALIZE_PIN p11tool_options.present.initialize_pin
#define HAVE_OPT_INITIALIZE_SO_PIN p11tool_options.present.initialize_so_pin
#define HAVE_OPT_SET_PIN p11tool_options.present.set_pin
#define HAVE_OPT_SET_SO_PIN p11tool_options.present.set_so_pin
#define HAVE_OPT_LIST_ALL p11tool_options.present.list_all
#define HAVE_OPT_LIST_ALL_CERTS p11tool_options.present.list_all_certs
#define HAVE_OPT_LIST_CERTS p11tool_options.present.list_certs
#define HAVE_OPT_LIST_ALL_PRIVKEYS p11tool_options.present.list_all_privkeys
#define HAVE_OPT_LIST_ALL_TRUSTED p11tool_options.present.list_all_trusted
#define HAVE_OPT_EXPORT p11tool_options.present.export
#define HAVE_OPT_EXPORT_STAPLED p11tool_options.present.export_stapled
#define HAVE_OPT_EXPORT_CHAIN p11tool_options.present.export_chain
#define HAVE_OPT_EXPORT_PUBKEY p11tool_options.present.export_pubkey
#define HAVE_OPT_INFO p11tool_options.present.info
#define HAVE_OPT_GENERATE_PRIVKEY p11tool_options.present.generate_privkey
#define HAVE_OPT_GENERATE_RSA p11tool_options.present.generate_rsa
#define HAVE_OPT_GENERATE_DSA p11tool_options.present.generate_dsa
#define HAVE_OPT_GENERATE_ECC p11tool_options.present.generate_ecc
#define HAVE_OPT_BITS p11tool_options.present.bits
#define HAVE_OPT_CURVE p11tool_options.present.curve
#define HAVE_OPT_SEC_PARAM p11tool_options.present.sec_param
#define HAVE_OPT_SET_ID p11tool_options.present.set_id
#define HAVE_OPT_SET_LABEL p11tool_options.present.set_label
#define HAVE_OPT_WRITE p11tool_options.present.write
#define HAVE_OPT_DELETE p11tool_options.present.delete
#define HAVE_OPT_LABEL p11tool_options.present.label
#define HAVE_OPT_ID p11tool_options.present.id
#define HAVE_OPT_MARK_WRAP p11tool_options.present.mark_wrap
#define HAVE_OPT_MARK_TRUSTED p11tool_options.present.mark_trusted
#define HAVE_OPT_MARK_DISTRUSTED p11tool_options.present.mark_distrusted
#define HAVE_OPT_MARK_DECRYPT p11tool_options.present.mark_decrypt
#define HAVE_OPT_MARK_SIGN p11tool_options.present.mark_sign
#define HAVE_OPT_MARK_CA p11tool_options.present.mark_ca
#define HAVE_OPT_MARK_PRIVATE p11tool_options.present.mark_private
#define HAVE_OPT_MARK_ALWAYS_AUTHENTICATE p11tool_options.present.mark_always_authenticate
#define HAVE_OPT_SECRET_KEY p11tool_options.present.secret_key
#define HAVE_OPT_LOAD_PRIVKEY p11tool_options.present.load_privkey
#define HAVE_OPT_LOAD_PUBKEY p11tool_options.present.load_pubkey
#define HAVE_OPT_LOAD_CERTIFICATE p11tool_options.present.load_certificate
#define HAVE_OPT_DEBUG p11tool_options.present.debug
#define HAVE_OPT_OUTFILE p11tool_options.present.outfile
#define HAVE_OPT_LOGIN p11tool_options.present.login
#define HAVE_OPT_SO_LOGIN p11tool_options.present.so_login
#define HAVE_OPT_TEST_SIGN p11tool_options.present.test_sign
#define HAVE_OPT_SIGN_PARAMS p11tool_options.present.sign_params
#define HAVE_OPT_HASH p11tool_options.present.hash
#define HAVE_OPT_GENERATE_RANDOM p11tool_options.present.generate_random
#define HAVE_OPT_PKCS8 p11tool_options.present.pkcs8
#define HAVE_OPT_INDER p11tool_options.present.inder
#define HAVE_OPT_OUTDER p11tool_options.present.outder
#define HAVE_OPT_PROVIDER p11tool_options.present.provider
#define HAVE_OPT_PROVIDER_OPTS p11tool_options.present.provider_opts
#define HAVE_OPT_DETAILED_URL p11tool_options.present.detailed_url
#define HAVE_OPT_ONLY_URLS p11tool_options.present.only_urls
#define HAVE_OPT_BATCH p11tool_options.present.batch
#define HAVE_OPT_VERSION p11tool_options.present.version
#define HAVE_OPT_HELP p11tool_options.present.help
#define HAVE_OPT_MORE_HELP p11tool_options.present.more_help

#define OPT_ARG_LIST_TOKENS p11tool_options.arg.list_tokens
#define OPT_ARG_LIST_TOKEN_URLS p11tool_options.arg.list_token_urls
#define OPT_ARG_LIST_MECHANISMS p11tool_options.arg.list_mechanisms
#define OPT_ARG_INITIALIZE p11tool_options.arg.initialize
#define OPT_ARG_INITIALIZE_PIN p11tool_options.arg.initialize_pin
#define OPT_ARG_INITIALIZE_SO_PIN p11tool_options.arg.initialize_so_pin
#define OPT_ARG_SET_PIN p11tool_options.arg.set_pin
#define OPT_ARG_SET_SO_PIN p11tool_options.arg.set_so_pin
#define OPT_ARG_LIST_ALL p11tool_options.arg.list_all
#define OPT_ARG_LIST_ALL_CERTS p11tool_options.arg.list_all_certs
#define OPT_ARG_LIST_CERTS p11tool_options.arg.list_certs
#define OPT_ARG_LIST_ALL_PRIVKEYS p11tool_options.arg.list_all_privkeys
#define OPT_ARG_LIST_ALL_TRUSTED p11tool_options.arg.list_all_trusted
#define OPT_ARG_EXPORT p11tool_options.arg.export
#define OPT_ARG_EXPORT_STAPLED p11tool_options.arg.export_stapled
#define OPT_ARG_EXPORT_CHAIN p11tool_options.arg.export_chain
#define OPT_ARG_EXPORT_PUBKEY p11tool_options.arg.export_pubkey
#define OPT_ARG_INFO p11tool_options.arg.info
#define OPT_ARG_GENERATE_PRIVKEY p11tool_options.arg.generate_privkey
#define OPT_ARG_GENERATE_RSA p11tool_options.arg.generate_rsa
#define OPT_ARG_GENERATE_DSA p11tool_options.arg.generate_dsa
#define OPT_ARG_GENERATE_ECC p11tool_options.arg.generate_ecc
#define OPT_ARG_BITS p11tool_options.arg.bits
#define OPT_ARG_CURVE p11tool_options.arg.curve
#define OPT_ARG_SEC_PARAM p11tool_options.arg.sec_param
#define OPT_ARG_SET_ID p11tool_options.arg.set_id
#define OPT_ARG_SET_LABEL p11tool_options.arg.set_label
#define OPT_ARG_WRITE p11tool_options.arg.write
#define OPT_ARG_DELETE p11tool_options.arg.delete
#define OPT_ARG_LABEL p11tool_options.arg.label
#define OPT_ARG_ID p11tool_options.arg.id
#define OPT_ARG_MARK_WRAP p11tool_options.arg.mark_wrap
#define OPT_ARG_MARK_TRUSTED p11tool_options.arg.mark_trusted
#define OPT_ARG_MARK_DISTRUSTED p11tool_options.arg.mark_distrusted
#define OPT_ARG_MARK_DECRYPT p11tool_options.arg.mark_decrypt
#define OPT_ARG_MARK_SIGN p11tool_options.arg.mark_sign
#define OPT_ARG_MARK_CA p11tool_options.arg.mark_ca
#define OPT_ARG_MARK_PRIVATE p11tool_options.arg.mark_private
#define OPT_ARG_MARK_ALWAYS_AUTHENTICATE p11tool_options.arg.mark_always_authenticate
#define OPT_ARG_SECRET_KEY p11tool_options.arg.secret_key
#define OPT_ARG_LOAD_PRIVKEY p11tool_options.arg.load_privkey
#define OPT_ARG_LOAD_PUBKEY p11tool_options.arg.load_pubkey
#define OPT_ARG_LOAD_CERTIFICATE p11tool_options.arg.load_certificate
#define OPT_ARG_DEBUG p11tool_options.arg.debug
#define OPT_ARG_OUTFILE p11tool_options.arg.outfile
#define OPT_ARG_LOGIN p11tool_options.arg.login
#define OPT_ARG_SO_LOGIN p11tool_options.arg.so_login
#define OPT_ARG_TEST_SIGN p11tool_options.arg.test_sign
#define OPT_ARG_SIGN_PARAMS p11tool_options.arg.sign_params
#define OPT_ARG_HASH p11tool_options.arg.hash
#define OPT_ARG_GENERATE_RANDOM p11tool_options.arg.generate_random
#define OPT_ARG_PKCS8 p11tool_options.arg.pkcs8
#define OPT_ARG_INDER p11tool_options.arg.inder
#define OPT_ARG_OUTDER p11tool_options.arg.outder
#define OPT_ARG_PROVIDER p11tool_options.arg.provider
#define OPT_ARG_PROVIDER_OPTS p11tool_options.arg.provider_opts
#define OPT_ARG_DETAILED_URL p11tool_options.arg.detailed_url
#define OPT_ARG_ONLY_URLS p11tool_options.arg.only_urls
#define OPT_ARG_BATCH p11tool_options.arg.batch
#define OPT_ARG_VERSION p11tool_options.arg.version
#define OPT_ARG_HELP p11tool_options.arg.help
#define OPT_ARG_MORE_HELP p11tool_options.arg.more_help

#define OPT_VALUE_BITS p11tool_options.value.bits
#define OPT_VALUE_DEBUG p11tool_options.value.debug
#define OPT_VALUE_GENERATE_RANDOM p11tool_options.value.generate_random

#define ENABLED_OPT_LIST_TOKENS p11tool_options.enabled.list_tokens
#define ENABLED_OPT_LIST_TOKEN_URLS p11tool_options.enabled.list_token_urls
#define ENABLED_OPT_LIST_MECHANISMS p11tool_options.enabled.list_mechanisms
#define ENABLED_OPT_INITIALIZE p11tool_options.enabled.initialize
#define ENABLED_OPT_INITIALIZE_PIN p11tool_options.enabled.initialize_pin
#define ENABLED_OPT_INITIALIZE_SO_PIN p11tool_options.enabled.initialize_so_pin
#define ENABLED_OPT_SET_PIN p11tool_options.enabled.set_pin
#define ENABLED_OPT_SET_SO_PIN p11tool_options.enabled.set_so_pin
#define ENABLED_OPT_LIST_ALL p11tool_options.enabled.list_all
#define ENABLED_OPT_LIST_ALL_CERTS p11tool_options.enabled.list_all_certs
#define ENABLED_OPT_LIST_CERTS p11tool_options.enabled.list_certs
#define ENABLED_OPT_LIST_ALL_PRIVKEYS p11tool_options.enabled.list_all_privkeys
#define ENABLED_OPT_LIST_ALL_TRUSTED p11tool_options.enabled.list_all_trusted
#define ENABLED_OPT_EXPORT p11tool_options.enabled.export
#define ENABLED_OPT_EXPORT_STAPLED p11tool_options.enabled.export_stapled
#define ENABLED_OPT_EXPORT_CHAIN p11tool_options.enabled.export_chain
#define ENABLED_OPT_EXPORT_PUBKEY p11tool_options.enabled.export_pubkey
#define ENABLED_OPT_INFO p11tool_options.enabled.info
#define ENABLED_OPT_GENERATE_PRIVKEY p11tool_options.enabled.generate_privkey
#define ENABLED_OPT_GENERATE_RSA p11tool_options.enabled.generate_rsa
#define ENABLED_OPT_GENERATE_DSA p11tool_options.enabled.generate_dsa
#define ENABLED_OPT_GENERATE_ECC p11tool_options.enabled.generate_ecc
#define ENABLED_OPT_BITS p11tool_options.enabled.bits
#define ENABLED_OPT_CURVE p11tool_options.enabled.curve
#define ENABLED_OPT_SEC_PARAM p11tool_options.enabled.sec_param
#define ENABLED_OPT_SET_ID p11tool_options.enabled.set_id
#define ENABLED_OPT_SET_LABEL p11tool_options.enabled.set_label
#define ENABLED_OPT_WRITE p11tool_options.enabled.write
#define ENABLED_OPT_DELETE p11tool_options.enabled.delete
#define ENABLED_OPT_LABEL p11tool_options.enabled.label
#define ENABLED_OPT_ID p11tool_options.enabled.id
#define ENABLED_OPT_MARK_WRAP p11tool_options.enabled.mark_wrap
#define ENABLED_OPT_MARK_TRUSTED p11tool_options.enabled.mark_trusted
#define ENABLED_OPT_MARK_DISTRUSTED p11tool_options.enabled.mark_distrusted
#define ENABLED_OPT_MARK_DECRYPT p11tool_options.enabled.mark_decrypt
#define ENABLED_OPT_MARK_SIGN p11tool_options.enabled.mark_sign
#define ENABLED_OPT_MARK_CA p11tool_options.enabled.mark_ca
#define ENABLED_OPT_MARK_PRIVATE p11tool_options.enabled.mark_private
#define ENABLED_OPT_MARK_ALWAYS_AUTHENTICATE p11tool_options.enabled.mark_always_authenticate
#define ENABLED_OPT_SECRET_KEY p11tool_options.enabled.secret_key
#define ENABLED_OPT_LOAD_PRIVKEY p11tool_options.enabled.load_privkey
#define ENABLED_OPT_LOAD_PUBKEY p11tool_options.enabled.load_pubkey
#define ENABLED_OPT_LOAD_CERTIFICATE p11tool_options.enabled.load_certificate
#define ENABLED_OPT_DEBUG p11tool_options.enabled.debug
#define ENABLED_OPT_OUTFILE p11tool_options.enabled.outfile
#define ENABLED_OPT_LOGIN p11tool_options.enabled.login
#define ENABLED_OPT_SO_LOGIN p11tool_options.enabled.so_login
#define ENABLED_OPT_TEST_SIGN p11tool_options.enabled.test_sign
#define ENABLED_OPT_SIGN_PARAMS p11tool_options.enabled.sign_params
#define ENABLED_OPT_HASH p11tool_options.enabled.hash
#define ENABLED_OPT_GENERATE_RANDOM p11tool_options.enabled.generate_random
#define ENABLED_OPT_PKCS8 p11tool_options.enabled.pkcs8
#define ENABLED_OPT_INDER p11tool_options.enabled.inder
#define ENABLED_OPT_OUTDER p11tool_options.enabled.outder
#define ENABLED_OPT_PROVIDER p11tool_options.enabled.provider
#define ENABLED_OPT_PROVIDER_OPTS p11tool_options.enabled.provider_opts
#define ENABLED_OPT_DETAILED_URL p11tool_options.enabled.detailed_url
#define ENABLED_OPT_ONLY_URLS p11tool_options.enabled.only_urls
#define ENABLED_OPT_BATCH p11tool_options.enabled.batch
#define ENABLED_OPT_VERSION p11tool_options.enabled.version
#define ENABLED_OPT_HELP p11tool_options.enabled.help
#define ENABLED_OPT_MORE_HELP p11tool_options.enabled.more_help




extern struct p11tool_options p11tool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* P11TOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
