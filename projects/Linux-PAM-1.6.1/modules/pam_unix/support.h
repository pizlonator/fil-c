/*
 * $Id$
 */

#ifndef _PAM_UNIX_SUPPORT_H
#define _PAM_UNIX_SUPPORT_H

#include <pwd.h>

/*
 * File to read value of ENCRYPT_METHOD from.
 */
#define LOGIN_DEFS "/etc/login.defs"


/*
 * here is the string to inform the user that the new passwords they
 * typed were not the same.
 */

/* type definition for the control options */

typedef struct {
	const char *token;
	unsigned long long mask;	/* shall assume 64 bits of flags */
	unsigned long long flag;
        unsigned int is_hash_algo;
} UNIX_Ctrls;

/*
 * macro to determine if a given flag is on
 */

#define on(x,ctrl)  (unix_args[x].flag & (ctrl))

/*
 * macro to determine that a given flag is NOT on
 */

#define off(x,ctrl) (!on(x,ctrl))

/*
 * macro to turn on/off a ctrl flag manually
 */

#define set(x,ctrl)   ((ctrl) = ((ctrl)&unix_args[x].mask)|unix_args[x].flag)
#define unset(x,ctrl) ((ctrl) &= ~(unix_args[x].flag))

/* the generic mask */

#define _ALL_ON_  (~0ULL)

/* end of macro definitions definitions for the control flags */

/* ****************************************************************** *
 * ctrl flags proper..
 */

/*
 * here are the various options recognized by the unix module. They
 * are enumerated here and then defined below. Internal arguments are
 * given NULL tokens.
 */

#define UNIX__OLD_PASSWD          0	/* internal */
#define UNIX__VERIFY_PASSWD       1	/* internal */
#define UNIX__IAMROOT             2	/* internal */

#define UNIX_AUDIT                3	/* print more things than debug..
					   some information may be sensitive */
#define UNIX_USE_FIRST_PASS       4
#define UNIX_TRY_FIRST_PASS       5
#define UNIX_AUTHTOK_TYPE         6	/* TYPE for pam_get_authtok() */

#define UNIX__PRELIM              7	/* internal */
#define UNIX__UPDATE              8	/* internal */
#define UNIX__NONULL              9	/* internal */
#define UNIX__QUIET              10	/* internal */
#define UNIX_USE_AUTHTOK         11	/* insist on reading PAM_AUTHTOK */
#define UNIX_SHADOW              12	/* signal shadow on */
#define UNIX_MD5_PASS            13	/* force the use of MD5 passwords */
#define UNIX__NULLOK             14	/* Null token ok */
#define UNIX_DEBUG               15	/* send more info to syslog(3) */
#define UNIX_NODELAY             16	/* admin does not want a fail-delay */
#define UNIX_NIS                 17	/* wish to use NIS for pwd */
#define UNIX_BIGCRYPT            18	/* use DEC-C2 crypt()^x function */
#define UNIX_LIKE_AUTH           19	/* need to auth for setcred to work */
#define UNIX_REMEMBER_PASSWD     20	/* Remember N previous passwords */
#define UNIX_NOREAP              21     /* don't reap child process */
#define UNIX_BROKEN_SHADOW       22     /* ignore errors reading password aging
					 * information during acct management */
#define UNIX_SHA256_PASS         23	/* new password hashes will use SHA256 */
#define UNIX_SHA512_PASS         24	/* new password hashes will use SHA512 */
#define UNIX_ALGO_ROUNDS         25	/* optional number of rounds for new
					   password hash algorithms */
#define UNIX_BLOWFISH_PASS       26	/* new password hashes will use blowfish */
#define UNIX_MIN_PASS_LEN        27	/* min length for password */
#define UNIX_QUIET		 28	/* Don't print informational messages */
#define UNIX_NO_PASS_EXPIRY      29     /* Don't check for password expiration if not used for authentication */
#define UNIX_DES                 30     /* DES, default */
#define UNIX_GOST_YESCRYPT_PASS  31     /* new password hashes will use gost-yescrypt */
#define UNIX_YESCRYPT_PASS       32     /* new password hashes will use yescrypt */
#define UNIX_NULLRESETOK         33     /* allow empty password if password reset is enforced */
/* -------------- */
#define UNIX_CTRLS_              34	/* number of ctrl arguments defined */

#define UNIX_DES_CRYPT(ctrl)	(off(UNIX_MD5_PASS,ctrl)&&off(UNIX_BIGCRYPT,ctrl)&&off(UNIX_SHA256_PASS,ctrl)&&off(UNIX_SHA512_PASS,ctrl)&&off(UNIX_BLOWFISH_PASS,ctrl)&&off(UNIX_GOST_YESCRYPT_PASS,ctrl)&&off(UNIX_YESCRYPT_PASS,ctrl))

static const UNIX_Ctrls unix_args[UNIX_CTRLS_] =
{
/* symbol                      token name          ctrl mask                  ctrl             *
 * --------------------------- -------------------- ------------------------- ---------------- */

/* UNIX__OLD_PASSWD */         {NULL,               _ALL_ON_,                              01, 0},
/* UNIX__VERIFY_PASSWD */      {NULL,               _ALL_ON_,                              02, 0},
/* UNIX__IAMROOT */            {NULL,               _ALL_ON_,                              04, 0},
/* UNIX_AUDIT */               {"audit",            _ALL_ON_,                             010, 0},
/* UNIX_USE_FIRST_PASS */      {"use_first_pass",   _ALL_ON_^(060ULL),                    020, 0},
/* UNIX_TRY_FIRST_PASS */      {"try_first_pass",   _ALL_ON_^(060ULL),                    040, 0},
/* UNIX_AUTHTOK_TYPE */        {"authtok_type=",    _ALL_ON_,                            0100, 0},
/* UNIX__PRELIM */             {NULL,               _ALL_ON_^(0600ULL),                  0200, 0},
/* UNIX__UPDATE */             {NULL,               _ALL_ON_^(0600ULL),                  0400, 0},
/* UNIX__NONULL */             {NULL,               _ALL_ON_,                           01000, 0},
/* UNIX__QUIET */              {NULL,               _ALL_ON_,                           02000, 0},
/* UNIX_USE_AUTHTOK */         {"use_authtok",      _ALL_ON_,                           04000, 0},
/* UNIX_SHADOW */              {"shadow",           _ALL_ON_,                          010000, 0},
/* UNIX_MD5_PASS */            {"md5",              _ALL_ON_^(015660420000ULL),        020000, 1},
/* UNIX__NULLOK */             {"nullok",           _ALL_ON_^(01000ULL),                    0, 0},
/* UNIX_DEBUG */               {"debug",            _ALL_ON_,                          040000, 0},
/* UNIX_NODELAY */             {"nodelay",          _ALL_ON_,                         0100000, 0},
/* UNIX_NIS */                 {"nis",              _ALL_ON_,                         0200000, 0},
/* UNIX_BIGCRYPT */            {"bigcrypt",         _ALL_ON_^(015660420000ULL),       0400000, 1},
/* UNIX_LIKE_AUTH */           {"likeauth",         _ALL_ON_,                        01000000, 0},
/* UNIX_REMEMBER_PASSWD */     {"remember=",        _ALL_ON_,                        02000000, 0},
/* UNIX_NOREAP */              {"noreap",           _ALL_ON_,                        04000000, 0},
/* UNIX_BROKEN_SHADOW */       {"broken_shadow",    _ALL_ON_,                       010000000, 0},
/* UNIX_SHA256_PASS */         {"sha256",           _ALL_ON_^(015660420000ULL),     020000000, 1},
/* UNIX_SHA512_PASS */         {"sha512",           _ALL_ON_^(015660420000ULL),     040000000, 1},
/* UNIX_ALGO_ROUNDS */         {"rounds=",          _ALL_ON_,                      0100000000, 0},
/* UNIX_BLOWFISH_PASS */       {"blowfish",         _ALL_ON_^(015660420000ULL),    0200000000, 1},
/* UNIX_MIN_PASS_LEN */        {"minlen=",          _ALL_ON_,                      0400000000, 0},
/* UNIX_QUIET */               {"quiet",            _ALL_ON_,                     01000000000, 0},
/* UNIX_NO_PASS_EXPIRY */      {"no_pass_expiry",   _ALL_ON_,                     02000000000, 0},
/* UNIX_DES */                 {"des",              _ALL_ON_^(015660420000ULL),             0, 1},
/* UNIX_GOST_YESCRYPT_PASS */  {"gost_yescrypt",    _ALL_ON_^(015660420000ULL),   04000000000, 1},
/* UNIX_YESCRYPT_PASS */       {"yescrypt",         _ALL_ON_^(015660420000ULL),  010000000000, 1},
/* UNIX_NULLRESETOK */         {"nullresetok",      _ALL_ON_,                    020000000000, 0},
};

#define UNIX_DEFAULTS  (unix_args[UNIX__NONULL].flag)

/* use this to free strings. ESPECIALLY password strings */

#define _pam_delete(xx)			\
{					\
	pam_overwrite_string(xx);	\
	_pam_drop(xx);			\
}

extern int _make_remark(pam_handle_t * pamh, unsigned long long ctrl,
		        int type, const char *text);
extern unsigned long long _set_ctrl(pam_handle_t * pamh, int flags,
				    int *remember, int *rounds,
				    int *pass_min_len,
				    int argc, const char **argv);
extern int _unix_getpwnam (pam_handle_t *pamh,
			   const char *name, int files, int nis,
			   struct passwd **ret);
extern int _unix_comesfromsource (pam_handle_t *pamh,
				  const char *name, int files, int nis);
extern int _unix_blankpasswd(pam_handle_t *pamh, unsigned long long ctrl,
			     const char *name);
extern int _unix_verify_password(pam_handle_t * pamh, const char *name,
				 const char *p, unsigned long long ctrl);

extern int _unix_verify_user(pam_handle_t *pamh, unsigned long long ctrl,
                             const char *name, int *daysleft);

extern int _unix_run_verify_binary(pam_handle_t *pamh,
				   unsigned long long ctrl,
				   const char *user, int *daysleft);
#endif /* _PAM_UNIX_SUPPORT_H */
