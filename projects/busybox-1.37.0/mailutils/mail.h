/* vi: set sw=4 ts=4: */
/*
 * helper routines
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

struct globals {
	pid_t helper_pid;
	unsigned timeout;
	unsigned verbose;
	char *user;
	char *pass;
	FILE *fp0; // initial stdin
	char *opt_charset;
};

#define G (*ptr_to_globals)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	G.opt_charset = (char *)CONFIG_FEATURE_MIME_CHARSET; \
} while (0)

//char FAST_FUNC *parse_url(char *url, char **user, char **pass);

void launch_helper(const char **argv) FAST_FUNC;
void get_cred_or_die(int fd) FAST_FUNC;

void send_r_n(const char *s) FAST_FUNC;
char *send_mail_command(const char *fmt, const char *param) FAST_FUNC;

void printbuf_base64(const char *buf, unsigned len) FAST_FUNC;
void printstr_base64(const char *buf) FAST_FUNC;
void printfile_base64(const char *fname) FAST_FUNC;
