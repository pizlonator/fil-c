/*
 * environ[] array cleanup code and getenv() wrappers
 *
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#else
#define PR_GET_DUMPABLE 3
#endif
#if (!defined(HAVE_PRCTL) && defined(linux))
#include <sys/syscall.h>
#endif
#include <unistd.h>
#include <sys/types.h>

#include "env.h"
#include "all-io.h"

#ifndef HAVE_ENVIRON_DECL
extern char **environ;
#endif

static char * const forbid[] = {
        "BASH_ENV=",    /* GNU creeping featurism strikes again... */
        "ENV=",
        "HOME=",
        "IFS=",
        "KRB_CONF=",
        "LD_",          /* anything with the LD_ prefix */
        "LIBPATH=",
        "MAIL=",
        "NLSPATH=",
        "PATH=",
        "SHELL=",
        "SHLIB_PATH=",
        (char *) 0
};

/* these are allowed, but with no slashes inside
   (to work around security problems in GNU gettext) */
static char * const noslash[] = {
        "LANG=",
        "LANGUAGE=",
        "LC_",          /* anything with the LC_ prefix */
        (char *) 0
};


struct ul_env_list {
	char *env;
	struct ul_env_list *next;
};

/*
 * Saves @name env.variable to @ls, returns pointer to the new head of the list.
 */
static struct ul_env_list *env_list_add(struct ul_env_list *ls0, const char *str)
{
	struct ul_env_list *ls;
	char *p;
	size_t sz = 0;

	if (!str || !*str)
		return ls0;

	sz = strlen(str) + 1;
	p = malloc(sizeof(struct ul_env_list) + sz);
	if (!p)
		return ls0;

	ls = (struct ul_env_list *) p;
	p += sizeof(struct ul_env_list);
	memcpy(p, str, sz);
	ls->env = p;

	ls->next = ls0;
	return ls;
}

/*
 * Use env_from_fd() to read environment from @fd.
 *
 * @fd must be /proc/<pid>/environ file.
*/
struct ul_env_list *env_from_fd(int fd)
{
	char *buf = NULL, *p;
	ssize_t rc = 0;
	struct ul_env_list *ls = NULL;

	if ((rc = read_all_alloc(fd, &buf)) < 1)
		return NULL;
	buf[rc] = '\0';
	p = buf;

	while (rc > 0) {
		ls = env_list_add(ls, p);
		p += strlen(p) + 1;
		rc -= strlen(p) + 1;
	}

	free(buf);
	return ls;
}

/*
 * Use setenv() for all stuff in @ls.
 *
 * It would be possible to use putenv(), but we want to keep @ls free()-able.
 */
int env_list_setenv(struct ul_env_list *ls)
{
	int rc = 0;

	while (ls && rc == 0) {
		if (ls->env) {
			char *val = strchr(ls->env, '=');
			if (!val)
				continue;
			*val = '\0';
			rc = setenv(ls->env, val + 1, 0);
			*val = '=';
		}
		ls = ls->next;
	}
	return rc;
}

void env_list_free(struct ul_env_list *ls)
{
	while (ls) {
		struct ul_env_list *x = ls;
		ls = ls->next;
		free(x);
	}
}

/*
 * Removes unwanted variables from environ[]. If @org is not NULL than stores
 * unwnated variables to the list.
 */
void __sanitize_env(struct ul_env_list **org)
{
        char **envp = environ;
        char * const *bad;
        char **cur;
        int last = 0;

        for (cur = envp; *cur; cur++)
                last++;

        for (cur = envp; *cur; cur++) {
                for (bad = forbid; *bad; bad++) {
                        if (strncmp(*cur, *bad, strlen(*bad)) == 0) {
				if (org)
					*org = env_list_add(*org, *cur);
                                last = remove_entry(envp, cur - envp, last);
                                cur--;
                                break;
                        }
                }
        }

        for (cur = envp; *cur; cur++) {
                for (bad = noslash; *bad; bad++) {
                        if (strncmp(*cur, *bad, strlen(*bad)) != 0)
                                continue;
                        if (!strchr(*cur, '/'))
                                continue;  /* OK */
			if (org)
				*org = env_list_add(*org, *cur);
                        last = remove_entry(envp, cur - envp, last);
                        cur--;
                        break;
                }
        }
}

void sanitize_env(void)
{
	__sanitize_env(NULL);
}

char *safe_getenv(const char *arg)
{
	if ((getuid() != geteuid()) || (getgid() != getegid()))
		return NULL;
#ifdef HAVE_PRCTL
	if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) == 0)
		return NULL;
#else
#if (defined(linux) && defined(SYS_prctl))
	if (syscall(SYS_prctl, PR_GET_DUMPABLE, 0, 0, 0, 0) == 0)
		return NULL;
#endif
#endif
#ifdef HAVE_SECURE_GETENV
return secure_getenv(arg);
#elif HAVE___SECURE_GETENV
	return __secure_getenv(arg);
#else
	return getenv(arg);
#endif
}

#ifdef TEST_PROGRAM
int main(void)
{
	char *const *bad;
	char copy[32];
	char *p;
	int retval = EXIT_SUCCESS;
	struct ul_env_list *removed = NULL;

	for (bad = forbid; *bad; bad++) {
		strcpy(copy, *bad);
		p = strchr(copy, '=');
		if (p)
			*p = '\0';
		setenv(copy, copy, 1);
	}

	/* removed */
	__sanitize_env(&removed);

	/* check removal */
	for (bad = forbid; *bad; bad++) {
		strcpy(copy, *bad);
		p = strchr(copy, '=');
		if (p)
			*p = '\0';
		p = getenv(copy);
		if (p) {
			warnx("%s was not removed", copy);
			retval = EXIT_FAILURE;
		}
	}

	/* restore removed */
	env_list_setenv(removed);

	/* check restore */
	for (bad = forbid; *bad; bad++) {
		strcpy(copy, *bad);
		p = strchr(copy, '=');
		if (p)
			*p = '\0';
		p = getenv(copy);
		if (!p) {
			warnx("%s was not restored", copy);
			retval = EXIT_FAILURE;
		}
	}

	env_list_free(removed);

	return retval;
}
#endif
