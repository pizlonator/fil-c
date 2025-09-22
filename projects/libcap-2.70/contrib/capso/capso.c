/*
 * Worked example for a shared object with a file capability on it
 * leveraging itself for preprogrammed functionality.
 *
 * This example implements a shared library that can bind to
 * the privileged port. ":80".
 *
 * The shared library needs to be installed with
 * cap_net_bind_service=p. As a shared library, it provides the
 * function bind80().
 */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "capso.h"

extern char **environ;

/*
 * fake_exploit is some dedicated code to simulate a shell escape type
 * exploit. This is obviously not something serious to include in code
 * that has actually been audited for security, but we use it to
 * demonstrate an aspect of file capabilities vs. setuid root for
 * granting privilege.
 */
static void fake_exploit(void) {
#ifdef ALLOW_EXPLOIT
    const char *exploit = getenv("TRIGGER_EXPLOIT");
    if (exploit == NULL) {
	return;
    }

    switch (*exploit) {
    case '^':
    case '%':
	exploit++;
	cap_value_t caps = CAP_NET_BIND_SERVICE;
	cap_t c = cap_get_proc();
	cap_set_flag(c, CAP_INHERITABLE, 1, &caps, CAP_SET);
	if (cap_set_proc(c)) {
	    perror("Failed to raise inheritable capability");
	    exit(1);
	}
	if (*(exploit-1) == '%') {
	    break;
	}
	cap_free(c);
	if (cap_set_ambient(caps, CAP_SET) != 0) {
	    perror("Unable to raise ambient capability");
	    exit(1);
	}
	break;
    }

    char *ts = strdup(exploit);
    if (ts == NULL) {
	perror("Failed to duplicate exploit string");
	exit(1);
    }

    int i, j, n = 1;
    for (i = 0; ts[i]; i++) {
	switch (ts[i]) {
	case ' ':
	case '\t':
	    n++;
	    ts[i] = '\0';
	}
    }
    char **argv = calloc(n, sizeof(char *));
    for (i = 0, j = 0; j < n; j++) {
	char *s = ts+i;
	argv[j] = s;
	i += 1 + strlen(s);
	printf("execv argv[%d] = \"%s\"\n", j, s);
    }

    execv(argv[0], argv);
    perror("Execv failed");
    exit(1);
#endif /* def ALLOW_EXPLOIT */
}

/*
 * where_am_i determines the full path for the shared libary that
 * contains this function. It allocates the path in strdup()d memory
 * that should be free()d by the caller. If it can't find itself, it
 * returns NULL.
 */
static char *where_am_i(void)
{
    Dl_info info;
    if (dladdr(where_am_i, &info) == 0) {
	return NULL;
    }
    return strdup(info.dli_fname);
}

/*
 * try_bind80 attempts to reuseably bind to port 80 with the given
 * hostname. It returns a bound filedescriptor or -1 on error.
 */
static int try_bind80(const char *hostname)
{
    struct addrinfo *conf, *detail = NULL;
    int err, ret = -1, one = 1;

    conf = calloc(1, sizeof(*conf));
    if (conf == NULL) {
      return -1;
    }

    conf->ai_family = PF_UNSPEC;
    conf->ai_socktype = SOCK_STREAM;
    conf->ai_protocol = 0;
    conf->ai_flags = AI_PASSIVE | AI_ADDRCONFIG;

    err = getaddrinfo(hostname, "80", conf, &detail);
    if (err != 0) {
	goto done;
    }

    ret = socket(detail->ai_family, detail->ai_socktype, detail->ai_protocol);
    if (ret == -1) {
	goto done;
    }

    if (setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
	close(ret);
	ret = -1;
	goto done;
    }

    if (bind(ret, detail->ai_addr, detail->ai_addrlen)) {
	close(ret);
	ret = -1;
	goto done;
    }

 done:
    if (detail != NULL) {
	freeaddrinfo(detail);
    }
    free(conf);

    return ret;
}

/*
 * set_fd3 forces file descriptor 3 to be associated with a unix
 * socket that can be used to send a file descriptor back to the
 * parent program.
 */
static int set_fd3(void *detail)
{
    int *sp = detail;

    close(sp[0]);
    if (dup2(sp[1], 3) != 3) {
	return -1;
    }
    close(sp[1]);

    return 0;
}

/*
 * bind80 returns a socket filedescriptor that is bound to port 80 of
 * the provided service address.
 *
 * Example:
 *
 *   int fd = bind80("localhost");
 *
 * fd < 0 in the case of error.
 */
int bind80(const char *hostname)
{
    cap_launch_t helper;
    pid_t child;
    char const *args[3];
    char *path;
    int fd, ignored;
    int sp[2];
    char junk[1];
    const int rec_buf_len = CMSG_SPACE(sizeof(int));
    char *rec_buf[CMSG_SPACE(sizeof(int))];
    struct iovec *iov;
    struct msghdr *msg;

    fd = try_bind80(hostname);
    if (fd >= 0) {
	return fd;
    }

#ifdef CAPSO_DEBUG
    printf("application bind80(%s) attempt failed\n", hostname);
    sleep(30);
#endif

    iov = calloc(1, sizeof(struct iovec));
    if (iov == NULL) {
      return -1;
    }
    msg = calloc(1, sizeof(struct msghdr));
    if (msg == NULL) {
      free(iov);
      return -1;
    }

    /*
     * Initial attempt didn't work, so try launching the shared
     * library as an executable and getting it to yield a bound
     * filedescriptor for us via a unix socket pair.
     */
    path = where_am_i();
    if (path == NULL) {
	perror("Unable to find self");
	goto drop_alloc;
    }

    args[0] = "bind80-helper";
    args[1] = hostname;
    args[2] = NULL;

    helper = cap_new_launcher(path, args, (void *) environ);
    if (helper == NULL) {
	goto drop_path;
    }

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sp)) {
	goto drop_helper;
    }

    cap_launcher_callback(helper, set_fd3);
    child = cap_launch(helper, sp);
    close(sp[1]);

    if (child <= 0) {
	goto drop_sp;
    }

    iov[0].iov_base = junk;
    iov[0].iov_len = 1;

    msg->msg_name = NULL;
    msg->msg_namelen = 0;
    msg->msg_iov = iov;
    msg->msg_iovlen = 1;
    msg->msg_control = rec_buf;
    msg->msg_controllen = rec_buf_len;

    if (recvmsg(sp[0], msg, 0) != -1) {
	fd = * (int *) CMSG_DATA(CMSG_FIRSTHDR(msg));
    }
    waitpid(child, &ignored, 0);

 drop_sp:
    close(sp[0]);

 drop_helper:
    cap_free(helper);

 drop_path:
    free(path);

 drop_alloc:
    free(msg);
    free(iov);

    return fd;
}

#include "../../libcap/execable.h"
//#define SO_MAIN int main

SO_MAIN(int argc, char **argv)
{
    const char *cmd = "<capso.so>";
    const cap_value_t cap_net_bind_service = CAP_NET_BIND_SERVICE;
    cap_t working;
    int fd;
    struct msghdr msg;
    struct cmsghdr *ctrl;
    struct iovec payload;
    char data[CMSG_SPACE(sizeof(fd))];
    char junk[1];

#ifdef CAPSO_DEBUG
    printf("invoking %s standalone\n", argv[0]);
    sleep(30);
#endif

    if (argv != NULL) {
	cmd = argv[0];
    }

    if (argc != 2 || argv[1] == NULL || !strcmp(argv[1], "--help")) {
	fprintf(stderr, "usage: %s <hostname>\n", cmd);
	exit(1);
    }

    working = cap_get_proc();
    if (working == NULL) {
	perror("Unable to read capabilities");
	exit(1);
    }

    if (cap_set_flag(working, CAP_EFFECTIVE, 1,
		     &cap_net_bind_service, CAP_SET) != 0) {
	perror("Unable to raise CAP_NET_BIND_SERVICE");
	exit(1);
    }

    if (cap_set_proc(working) != 0) {
	perror("Problem with cap_set_proc");
	fprintf(stderr, "Try: sudo setcap cap_net_bind_service=p %s\n",
		argv[0]);
	exit(1);
    }

    fd = try_bind80(argv[1]);

    memset(data, 0, sizeof(data));
    memset(&payload, 0, sizeof(payload));

    payload.iov_base = junk;
    payload.iov_len = 1;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &payload;
    msg.msg_iovlen = 1;
    msg.msg_control = data;
    msg.msg_controllen = sizeof(data);

    ctrl = CMSG_FIRSTHDR(&msg);
    ctrl->cmsg_level = SOL_SOCKET;
    ctrl->cmsg_type = SCM_RIGHTS;
    ctrl->cmsg_len = CMSG_LEN(sizeof(fd));

    *((int *) CMSG_DATA(ctrl)) = fd;

    if (sendmsg(3, &msg, 0) < 0) {
	perror("Failed to write fd");
    }

    fake_exploit();

#ifdef CAPSO_DEBUG
    printf("exiting standalone %s\n", argv[0]);
    sleep(30);
#endif

    exit(0);
}
