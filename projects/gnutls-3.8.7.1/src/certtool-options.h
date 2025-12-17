#include <stdbool.h>
#include <stdio.h>

#ifndef CERTTOOL_OPTIONS_H_
#define CERTTOOL_OPTIONS_H_ 1

struct certtool_list
{
  const char **args;
  unsigned int count;
};

struct certtool_options
{
  /* Options present in the command line */
  struct
  {
    bool debug;
    bool verbose;
    bool infile;
    bool outfile;
    bool attime;
    bool certificate_info;
    bool pubkey_info;
    bool generate_self_signed;
    bool generate_certificate;
    bool generate_proxy;
    bool update_certificate;
    bool fingerprint;
    bool key_id;
    bool certificate_pubkey;
    bool v1;
    bool sign_params;
    bool crq_info;
    bool generate_request;
    bool no_crq_extensions;
    bool p12_info;
    bool p12_name;
    bool to_p12;
    bool pbmac1;
    bool key_info;
    bool p8_info;
    bool to_rsa;
    bool generate_privkey;
    bool key_type;
    bool bits;
    bool curve;
    bool sec_param;
    bool to_p8;
    bool pkcs8;
    bool provable;
    bool verify_provable_privkey;
    bool seed;
    bool crl_info;
    bool generate_crl;
    bool verify_crl;
    bool verify_chain;
    bool verify;
    bool verify_hostname;
    bool verify_email;
    bool verify_purpose;
    bool verify_allow_broken;
    bool verify_profile;
    bool p7_generate;
    bool p7_sign;
    bool p7_detached_sign;
    bool p7_include_cert;
    bool p7_time;
    bool p7_show_data;
    bool p7_info;
    bool p7_verify;
    bool smime_to_p7;
    bool generate_dh_params;
    bool get_dh_params;
    bool dh_info;
    bool load_privkey;
    bool load_pubkey;
    bool load_request;
    bool load_certificate;
    bool load_ca_privkey;
    bool load_ca_certificate;
    bool load_crl;
    bool load_data;
    bool password;
    bool null_password;
    bool empty_password;
    bool hex_numbers;
    bool cprint;
    bool rsa;
    bool dsa;
    bool ecc;
    bool hash;
    bool salt_size;
    bool label;
    bool inder;
    bool outder;
    bool disable_quick_random;
    bool template;
    bool stdout_info;
    bool ask_pass;
    bool pkcs_cipher;
    bool provider;
    bool text;
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
    const char *attime;
    const char *sign_params;
    const char *p12_name;
    const char *key_type;
    const char *bits;
    const char *curve;
    const char *sec_param;
    const char *seed;
    const char *verify_hostname;
    const char *verify_email;
    const char *verify_purpose;
    const char *verify_profile;
    const char *load_privkey;
    const char *load_pubkey;
    const char *load_request;
    const char *load_certificate;
    const char *load_ca_privkey;
    const char *load_ca_certificate;
    const char *load_crl;
    const char *load_data;
    const char *password;
    const char *hash;
    const char *salt_size;
    const char *label;
    const char *template;
    const char *pkcs_cipher;
    const char *provider;
    const char *version;
  } arg;

  /* Option arguments parsed as integer */
  struct
  {
    int debug;
    int bits;
    int salt_size;
  } value;

  /* Option enablement status */
  struct
  {
    bool debug;
    bool verbose;
    bool infile;
    bool outfile;
    bool attime;
    bool certificate_info;
    bool pubkey_info;
    bool generate_self_signed;
    bool generate_certificate;
    bool generate_proxy;
    bool update_certificate;
    bool fingerprint;
    bool key_id;
    bool certificate_pubkey;
    bool v1;
    bool sign_params;
    bool crq_info;
    bool generate_request;
    bool no_crq_extensions;
    bool p12_info;
    bool p12_name;
    bool to_p12;
    bool pbmac1;
    bool key_info;
    bool p8_info;
    bool to_rsa;
    bool generate_privkey;
    bool key_type;
    bool bits;
    bool curve;
    bool sec_param;
    bool to_p8;
    bool pkcs8;
    bool provable;
    bool verify_provable_privkey;
    bool seed;
    bool crl_info;
    bool generate_crl;
    bool verify_crl;
    bool verify_chain;
    bool verify;
    bool verify_hostname;
    bool verify_email;
    bool verify_purpose;
    bool verify_allow_broken;
    bool verify_profile;
    bool p7_generate;
    bool p7_sign;
    bool p7_detached_sign;
    bool p7_include_cert;
    bool p7_time;
    bool p7_show_data;
    bool p7_info;
    bool p7_verify;
    bool smime_to_p7;
    bool generate_dh_params;
    bool get_dh_params;
    bool dh_info;
    bool load_privkey;
    bool load_pubkey;
    bool load_request;
    bool load_certificate;
    bool load_ca_privkey;
    bool load_ca_certificate;
    bool load_crl;
    bool load_data;
    bool password;
    bool null_password;
    bool empty_password;
    bool hex_numbers;
    bool cprint;
    bool rsa;
    bool dsa;
    bool ecc;
    bool hash;
    bool salt_size;
    bool label;
    bool inder;
    bool outder;
    bool disable_quick_random;
    bool template;
    bool stdout_info;
    bool ask_pass;
    bool pkcs_cipher;
    bool provider;
    bool text;
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

#define HAVE_OPT_DEBUG certtool_options.present.debug
#define HAVE_OPT_VERBOSE certtool_options.present.verbose
#define HAVE_OPT_INFILE certtool_options.present.infile
#define HAVE_OPT_OUTFILE certtool_options.present.outfile
#define HAVE_OPT_ATTIME certtool_options.present.attime
#define HAVE_OPT_CERTIFICATE_INFO certtool_options.present.certificate_info
#define HAVE_OPT_PUBKEY_INFO certtool_options.present.pubkey_info
#define HAVE_OPT_GENERATE_SELF_SIGNED certtool_options.present.generate_self_signed
#define HAVE_OPT_GENERATE_CERTIFICATE certtool_options.present.generate_certificate
#define HAVE_OPT_GENERATE_PROXY certtool_options.present.generate_proxy
#define HAVE_OPT_UPDATE_CERTIFICATE certtool_options.present.update_certificate
#define HAVE_OPT_FINGERPRINT certtool_options.present.fingerprint
#define HAVE_OPT_KEY_ID certtool_options.present.key_id
#define HAVE_OPT_CERTIFICATE_PUBKEY certtool_options.present.certificate_pubkey
#define HAVE_OPT_V1 certtool_options.present.v1
#define HAVE_OPT_SIGN_PARAMS certtool_options.present.sign_params
#define HAVE_OPT_CRQ_INFO certtool_options.present.crq_info
#define HAVE_OPT_GENERATE_REQUEST certtool_options.present.generate_request
#define HAVE_OPT_NO_CRQ_EXTENSIONS certtool_options.present.no_crq_extensions
#define HAVE_OPT_P12_INFO certtool_options.present.p12_info
#define HAVE_OPT_P12_NAME certtool_options.present.p12_name
#define HAVE_OPT_TO_P12 certtool_options.present.to_p12
#define HAVE_OPT_PBMAC1 certtool_options.present.pbmac1
#define HAVE_OPT_KEY_INFO certtool_options.present.key_info
#define HAVE_OPT_P8_INFO certtool_options.present.p8_info
#define HAVE_OPT_TO_RSA certtool_options.present.to_rsa
#define HAVE_OPT_GENERATE_PRIVKEY certtool_options.present.generate_privkey
#define HAVE_OPT_KEY_TYPE certtool_options.present.key_type
#define HAVE_OPT_BITS certtool_options.present.bits
#define HAVE_OPT_CURVE certtool_options.present.curve
#define HAVE_OPT_SEC_PARAM certtool_options.present.sec_param
#define HAVE_OPT_TO_P8 certtool_options.present.to_p8
#define HAVE_OPT_PKCS8 certtool_options.present.pkcs8
#define HAVE_OPT_PROVABLE certtool_options.present.provable
#define HAVE_OPT_VERIFY_PROVABLE_PRIVKEY certtool_options.present.verify_provable_privkey
#define HAVE_OPT_SEED certtool_options.present.seed
#define HAVE_OPT_CRL_INFO certtool_options.present.crl_info
#define HAVE_OPT_GENERATE_CRL certtool_options.present.generate_crl
#define HAVE_OPT_VERIFY_CRL certtool_options.present.verify_crl
#define HAVE_OPT_VERIFY_CHAIN certtool_options.present.verify_chain
#define HAVE_OPT_VERIFY certtool_options.present.verify
#define HAVE_OPT_VERIFY_HOSTNAME certtool_options.present.verify_hostname
#define HAVE_OPT_VERIFY_EMAIL certtool_options.present.verify_email
#define HAVE_OPT_VERIFY_PURPOSE certtool_options.present.verify_purpose
#define HAVE_OPT_VERIFY_ALLOW_BROKEN certtool_options.present.verify_allow_broken
#define HAVE_OPT_VERIFY_PROFILE certtool_options.present.verify_profile
#define HAVE_OPT_P7_GENERATE certtool_options.present.p7_generate
#define HAVE_OPT_P7_SIGN certtool_options.present.p7_sign
#define HAVE_OPT_P7_DETACHED_SIGN certtool_options.present.p7_detached_sign
#define HAVE_OPT_P7_INCLUDE_CERT certtool_options.present.p7_include_cert
#define HAVE_OPT_P7_TIME certtool_options.present.p7_time
#define HAVE_OPT_P7_SHOW_DATA certtool_options.present.p7_show_data
#define HAVE_OPT_P7_INFO certtool_options.present.p7_info
#define HAVE_OPT_P7_VERIFY certtool_options.present.p7_verify
#define HAVE_OPT_SMIME_TO_P7 certtool_options.present.smime_to_p7
#define HAVE_OPT_GENERATE_DH_PARAMS certtool_options.present.generate_dh_params
#define HAVE_OPT_GET_DH_PARAMS certtool_options.present.get_dh_params
#define HAVE_OPT_DH_INFO certtool_options.present.dh_info
#define HAVE_OPT_LOAD_PRIVKEY certtool_options.present.load_privkey
#define HAVE_OPT_LOAD_PUBKEY certtool_options.present.load_pubkey
#define HAVE_OPT_LOAD_REQUEST certtool_options.present.load_request
#define HAVE_OPT_LOAD_CERTIFICATE certtool_options.present.load_certificate
#define HAVE_OPT_LOAD_CA_PRIVKEY certtool_options.present.load_ca_privkey
#define HAVE_OPT_LOAD_CA_CERTIFICATE certtool_options.present.load_ca_certificate
#define HAVE_OPT_LOAD_CRL certtool_options.present.load_crl
#define HAVE_OPT_LOAD_DATA certtool_options.present.load_data
#define HAVE_OPT_PASSWORD certtool_options.present.password
#define HAVE_OPT_NULL_PASSWORD certtool_options.present.null_password
#define HAVE_OPT_EMPTY_PASSWORD certtool_options.present.empty_password
#define HAVE_OPT_HEX_NUMBERS certtool_options.present.hex_numbers
#define HAVE_OPT_CPRINT certtool_options.present.cprint
#define HAVE_OPT_RSA certtool_options.present.rsa
#define HAVE_OPT_DSA certtool_options.present.dsa
#define HAVE_OPT_ECC certtool_options.present.ecc
#define HAVE_OPT_HASH certtool_options.present.hash
#define HAVE_OPT_SALT_SIZE certtool_options.present.salt_size
#define HAVE_OPT_LABEL certtool_options.present.label
#define HAVE_OPT_INDER certtool_options.present.inder
#define HAVE_OPT_OUTDER certtool_options.present.outder
#define HAVE_OPT_DISABLE_QUICK_RANDOM certtool_options.present.disable_quick_random
#define HAVE_OPT_TEMPLATE certtool_options.present.template
#define HAVE_OPT_STDOUT_INFO certtool_options.present.stdout_info
#define HAVE_OPT_ASK_PASS certtool_options.present.ask_pass
#define HAVE_OPT_PKCS_CIPHER certtool_options.present.pkcs_cipher
#define HAVE_OPT_PROVIDER certtool_options.present.provider
#define HAVE_OPT_TEXT certtool_options.present.text
#define HAVE_OPT_VERSION certtool_options.present.version
#define HAVE_OPT_HELP certtool_options.present.help
#define HAVE_OPT_MORE_HELP certtool_options.present.more_help

#define OPT_ARG_DEBUG certtool_options.arg.debug
#define OPT_ARG_VERBOSE certtool_options.arg.verbose
#define OPT_ARG_INFILE certtool_options.arg.infile
#define OPT_ARG_OUTFILE certtool_options.arg.outfile
#define OPT_ARG_ATTIME certtool_options.arg.attime
#define OPT_ARG_CERTIFICATE_INFO certtool_options.arg.certificate_info
#define OPT_ARG_PUBKEY_INFO certtool_options.arg.pubkey_info
#define OPT_ARG_GENERATE_SELF_SIGNED certtool_options.arg.generate_self_signed
#define OPT_ARG_GENERATE_CERTIFICATE certtool_options.arg.generate_certificate
#define OPT_ARG_GENERATE_PROXY certtool_options.arg.generate_proxy
#define OPT_ARG_UPDATE_CERTIFICATE certtool_options.arg.update_certificate
#define OPT_ARG_FINGERPRINT certtool_options.arg.fingerprint
#define OPT_ARG_KEY_ID certtool_options.arg.key_id
#define OPT_ARG_CERTIFICATE_PUBKEY certtool_options.arg.certificate_pubkey
#define OPT_ARG_V1 certtool_options.arg.v1
#define OPT_ARG_SIGN_PARAMS certtool_options.arg.sign_params
#define OPT_ARG_CRQ_INFO certtool_options.arg.crq_info
#define OPT_ARG_GENERATE_REQUEST certtool_options.arg.generate_request
#define OPT_ARG_NO_CRQ_EXTENSIONS certtool_options.arg.no_crq_extensions
#define OPT_ARG_P12_INFO certtool_options.arg.p12_info
#define OPT_ARG_P12_NAME certtool_options.arg.p12_name
#define OPT_ARG_TO_P12 certtool_options.arg.to_p12
#define OPT_ARG_PBMAC1 certtool_options.arg.pbmac1
#define OPT_ARG_KEY_INFO certtool_options.arg.key_info
#define OPT_ARG_P8_INFO certtool_options.arg.p8_info
#define OPT_ARG_TO_RSA certtool_options.arg.to_rsa
#define OPT_ARG_GENERATE_PRIVKEY certtool_options.arg.generate_privkey
#define OPT_ARG_KEY_TYPE certtool_options.arg.key_type
#define OPT_ARG_BITS certtool_options.arg.bits
#define OPT_ARG_CURVE certtool_options.arg.curve
#define OPT_ARG_SEC_PARAM certtool_options.arg.sec_param
#define OPT_ARG_TO_P8 certtool_options.arg.to_p8
#define OPT_ARG_PKCS8 certtool_options.arg.pkcs8
#define OPT_ARG_PROVABLE certtool_options.arg.provable
#define OPT_ARG_VERIFY_PROVABLE_PRIVKEY certtool_options.arg.verify_provable_privkey
#define OPT_ARG_SEED certtool_options.arg.seed
#define OPT_ARG_CRL_INFO certtool_options.arg.crl_info
#define OPT_ARG_GENERATE_CRL certtool_options.arg.generate_crl
#define OPT_ARG_VERIFY_CRL certtool_options.arg.verify_crl
#define OPT_ARG_VERIFY_CHAIN certtool_options.arg.verify_chain
#define OPT_ARG_VERIFY certtool_options.arg.verify
#define OPT_ARG_VERIFY_HOSTNAME certtool_options.arg.verify_hostname
#define OPT_ARG_VERIFY_EMAIL certtool_options.arg.verify_email
#define OPT_ARG_VERIFY_PURPOSE certtool_options.arg.verify_purpose
#define OPT_ARG_VERIFY_ALLOW_BROKEN certtool_options.arg.verify_allow_broken
#define OPT_ARG_VERIFY_PROFILE certtool_options.arg.verify_profile
#define OPT_ARG_P7_GENERATE certtool_options.arg.p7_generate
#define OPT_ARG_P7_SIGN certtool_options.arg.p7_sign
#define OPT_ARG_P7_DETACHED_SIGN certtool_options.arg.p7_detached_sign
#define OPT_ARG_P7_INCLUDE_CERT certtool_options.arg.p7_include_cert
#define OPT_ARG_P7_TIME certtool_options.arg.p7_time
#define OPT_ARG_P7_SHOW_DATA certtool_options.arg.p7_show_data
#define OPT_ARG_P7_INFO certtool_options.arg.p7_info
#define OPT_ARG_P7_VERIFY certtool_options.arg.p7_verify
#define OPT_ARG_SMIME_TO_P7 certtool_options.arg.smime_to_p7
#define OPT_ARG_GENERATE_DH_PARAMS certtool_options.arg.generate_dh_params
#define OPT_ARG_GET_DH_PARAMS certtool_options.arg.get_dh_params
#define OPT_ARG_DH_INFO certtool_options.arg.dh_info
#define OPT_ARG_LOAD_PRIVKEY certtool_options.arg.load_privkey
#define OPT_ARG_LOAD_PUBKEY certtool_options.arg.load_pubkey
#define OPT_ARG_LOAD_REQUEST certtool_options.arg.load_request
#define OPT_ARG_LOAD_CERTIFICATE certtool_options.arg.load_certificate
#define OPT_ARG_LOAD_CA_PRIVKEY certtool_options.arg.load_ca_privkey
#define OPT_ARG_LOAD_CA_CERTIFICATE certtool_options.arg.load_ca_certificate
#define OPT_ARG_LOAD_CRL certtool_options.arg.load_crl
#define OPT_ARG_LOAD_DATA certtool_options.arg.load_data
#define OPT_ARG_PASSWORD certtool_options.arg.password
#define OPT_ARG_NULL_PASSWORD certtool_options.arg.null_password
#define OPT_ARG_EMPTY_PASSWORD certtool_options.arg.empty_password
#define OPT_ARG_HEX_NUMBERS certtool_options.arg.hex_numbers
#define OPT_ARG_CPRINT certtool_options.arg.cprint
#define OPT_ARG_RSA certtool_options.arg.rsa
#define OPT_ARG_DSA certtool_options.arg.dsa
#define OPT_ARG_ECC certtool_options.arg.ecc
#define OPT_ARG_HASH certtool_options.arg.hash
#define OPT_ARG_SALT_SIZE certtool_options.arg.salt_size
#define OPT_ARG_LABEL certtool_options.arg.label
#define OPT_ARG_INDER certtool_options.arg.inder
#define OPT_ARG_OUTDER certtool_options.arg.outder
#define OPT_ARG_DISABLE_QUICK_RANDOM certtool_options.arg.disable_quick_random
#define OPT_ARG_TEMPLATE certtool_options.arg.template
#define OPT_ARG_STDOUT_INFO certtool_options.arg.stdout_info
#define OPT_ARG_ASK_PASS certtool_options.arg.ask_pass
#define OPT_ARG_PKCS_CIPHER certtool_options.arg.pkcs_cipher
#define OPT_ARG_PROVIDER certtool_options.arg.provider
#define OPT_ARG_TEXT certtool_options.arg.text
#define OPT_ARG_VERSION certtool_options.arg.version
#define OPT_ARG_HELP certtool_options.arg.help
#define OPT_ARG_MORE_HELP certtool_options.arg.more_help

#define OPT_VALUE_DEBUG certtool_options.value.debug
#define OPT_VALUE_BITS certtool_options.value.bits
#define OPT_VALUE_SALT_SIZE certtool_options.value.salt_size

#define ENABLED_OPT_DEBUG certtool_options.enabled.debug
#define ENABLED_OPT_VERBOSE certtool_options.enabled.verbose
#define ENABLED_OPT_INFILE certtool_options.enabled.infile
#define ENABLED_OPT_OUTFILE certtool_options.enabled.outfile
#define ENABLED_OPT_ATTIME certtool_options.enabled.attime
#define ENABLED_OPT_CERTIFICATE_INFO certtool_options.enabled.certificate_info
#define ENABLED_OPT_PUBKEY_INFO certtool_options.enabled.pubkey_info
#define ENABLED_OPT_GENERATE_SELF_SIGNED certtool_options.enabled.generate_self_signed
#define ENABLED_OPT_GENERATE_CERTIFICATE certtool_options.enabled.generate_certificate
#define ENABLED_OPT_GENERATE_PROXY certtool_options.enabled.generate_proxy
#define ENABLED_OPT_UPDATE_CERTIFICATE certtool_options.enabled.update_certificate
#define ENABLED_OPT_FINGERPRINT certtool_options.enabled.fingerprint
#define ENABLED_OPT_KEY_ID certtool_options.enabled.key_id
#define ENABLED_OPT_CERTIFICATE_PUBKEY certtool_options.enabled.certificate_pubkey
#define ENABLED_OPT_V1 certtool_options.enabled.v1
#define ENABLED_OPT_SIGN_PARAMS certtool_options.enabled.sign_params
#define ENABLED_OPT_CRQ_INFO certtool_options.enabled.crq_info
#define ENABLED_OPT_GENERATE_REQUEST certtool_options.enabled.generate_request
#define ENABLED_OPT_NO_CRQ_EXTENSIONS certtool_options.enabled.no_crq_extensions
#define ENABLED_OPT_P12_INFO certtool_options.enabled.p12_info
#define ENABLED_OPT_P12_NAME certtool_options.enabled.p12_name
#define ENABLED_OPT_TO_P12 certtool_options.enabled.to_p12
#define ENABLED_OPT_PBMAC1 certtool_options.enabled.pbmac1
#define ENABLED_OPT_KEY_INFO certtool_options.enabled.key_info
#define ENABLED_OPT_P8_INFO certtool_options.enabled.p8_info
#define ENABLED_OPT_TO_RSA certtool_options.enabled.to_rsa
#define ENABLED_OPT_GENERATE_PRIVKEY certtool_options.enabled.generate_privkey
#define ENABLED_OPT_KEY_TYPE certtool_options.enabled.key_type
#define ENABLED_OPT_BITS certtool_options.enabled.bits
#define ENABLED_OPT_CURVE certtool_options.enabled.curve
#define ENABLED_OPT_SEC_PARAM certtool_options.enabled.sec_param
#define ENABLED_OPT_TO_P8 certtool_options.enabled.to_p8
#define ENABLED_OPT_PKCS8 certtool_options.enabled.pkcs8
#define ENABLED_OPT_PROVABLE certtool_options.enabled.provable
#define ENABLED_OPT_VERIFY_PROVABLE_PRIVKEY certtool_options.enabled.verify_provable_privkey
#define ENABLED_OPT_SEED certtool_options.enabled.seed
#define ENABLED_OPT_CRL_INFO certtool_options.enabled.crl_info
#define ENABLED_OPT_GENERATE_CRL certtool_options.enabled.generate_crl
#define ENABLED_OPT_VERIFY_CRL certtool_options.enabled.verify_crl
#define ENABLED_OPT_VERIFY_CHAIN certtool_options.enabled.verify_chain
#define ENABLED_OPT_VERIFY certtool_options.enabled.verify
#define ENABLED_OPT_VERIFY_HOSTNAME certtool_options.enabled.verify_hostname
#define ENABLED_OPT_VERIFY_EMAIL certtool_options.enabled.verify_email
#define ENABLED_OPT_VERIFY_PURPOSE certtool_options.enabled.verify_purpose
#define ENABLED_OPT_VERIFY_ALLOW_BROKEN certtool_options.enabled.verify_allow_broken
#define ENABLED_OPT_VERIFY_PROFILE certtool_options.enabled.verify_profile
#define ENABLED_OPT_P7_GENERATE certtool_options.enabled.p7_generate
#define ENABLED_OPT_P7_SIGN certtool_options.enabled.p7_sign
#define ENABLED_OPT_P7_DETACHED_SIGN certtool_options.enabled.p7_detached_sign
#define ENABLED_OPT_P7_INCLUDE_CERT certtool_options.enabled.p7_include_cert
#define ENABLED_OPT_P7_TIME certtool_options.enabled.p7_time
#define ENABLED_OPT_P7_SHOW_DATA certtool_options.enabled.p7_show_data
#define ENABLED_OPT_P7_INFO certtool_options.enabled.p7_info
#define ENABLED_OPT_P7_VERIFY certtool_options.enabled.p7_verify
#define ENABLED_OPT_SMIME_TO_P7 certtool_options.enabled.smime_to_p7
#define ENABLED_OPT_GENERATE_DH_PARAMS certtool_options.enabled.generate_dh_params
#define ENABLED_OPT_GET_DH_PARAMS certtool_options.enabled.get_dh_params
#define ENABLED_OPT_DH_INFO certtool_options.enabled.dh_info
#define ENABLED_OPT_LOAD_PRIVKEY certtool_options.enabled.load_privkey
#define ENABLED_OPT_LOAD_PUBKEY certtool_options.enabled.load_pubkey
#define ENABLED_OPT_LOAD_REQUEST certtool_options.enabled.load_request
#define ENABLED_OPT_LOAD_CERTIFICATE certtool_options.enabled.load_certificate
#define ENABLED_OPT_LOAD_CA_PRIVKEY certtool_options.enabled.load_ca_privkey
#define ENABLED_OPT_LOAD_CA_CERTIFICATE certtool_options.enabled.load_ca_certificate
#define ENABLED_OPT_LOAD_CRL certtool_options.enabled.load_crl
#define ENABLED_OPT_LOAD_DATA certtool_options.enabled.load_data
#define ENABLED_OPT_PASSWORD certtool_options.enabled.password
#define ENABLED_OPT_NULL_PASSWORD certtool_options.enabled.null_password
#define ENABLED_OPT_EMPTY_PASSWORD certtool_options.enabled.empty_password
#define ENABLED_OPT_HEX_NUMBERS certtool_options.enabled.hex_numbers
#define ENABLED_OPT_CPRINT certtool_options.enabled.cprint
#define ENABLED_OPT_RSA certtool_options.enabled.rsa
#define ENABLED_OPT_DSA certtool_options.enabled.dsa
#define ENABLED_OPT_ECC certtool_options.enabled.ecc
#define ENABLED_OPT_HASH certtool_options.enabled.hash
#define ENABLED_OPT_SALT_SIZE certtool_options.enabled.salt_size
#define ENABLED_OPT_LABEL certtool_options.enabled.label
#define ENABLED_OPT_INDER certtool_options.enabled.inder
#define ENABLED_OPT_OUTDER certtool_options.enabled.outder
#define ENABLED_OPT_DISABLE_QUICK_RANDOM certtool_options.enabled.disable_quick_random
#define ENABLED_OPT_TEMPLATE certtool_options.enabled.template
#define ENABLED_OPT_STDOUT_INFO certtool_options.enabled.stdout_info
#define ENABLED_OPT_ASK_PASS certtool_options.enabled.ask_pass
#define ENABLED_OPT_PKCS_CIPHER certtool_options.enabled.pkcs_cipher
#define ENABLED_OPT_PROVIDER certtool_options.enabled.provider
#define ENABLED_OPT_TEXT certtool_options.enabled.text
#define ENABLED_OPT_VERSION certtool_options.enabled.version
#define ENABLED_OPT_HELP certtool_options.enabled.help
#define ENABLED_OPT_MORE_HELP certtool_options.enabled.more_help




extern struct certtool_options certtool_options;
int process_options (int argc, char **argv);
void usage (FILE *out, int status);

#endif /* CERTTOOL_OPTIONS_H_ */
#define optionProcess(a,b,c) process_options(b,c)
#define STACKCT_OPT OPTS_COUNT
#define STACKLST_OPT OPTS_ARRAY
