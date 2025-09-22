/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Privilege Separation for dhcpcd, Linux driver
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/net.h>
#include <linux/seccomp.h>
#include <linux/sockios.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>	/* For TCGETS */
#include <unistd.h>

#include "common.h"
#include "if.h"
#include "logerr.h"
#include "privsep.h"

/*
 * Set this to debug SECCOMP.
 * Then run dhcpcd with strace -f and strace will even translate
 * the failing syscall into the __NR_name define we need to use below.
 * DO NOT ENABLE THIS FOR PRODUCTION BUILDS!
 */
//#define SECCOMP_FILTER_DEBUG

static ssize_t
ps_root_dosendnetlink(struct dhcpcd_ctx *ctx, int protocol, struct msghdr *msg)
{
	struct priv *priv = (struct priv *)ctx->priv;
	int s;
	unsigned char buf[16 * 1024];
	struct iovec riov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};

	switch(protocol) {
	case NETLINK_GENERIC:
		s = priv->generic_fd;
		break;
	case NETLINK_ROUTE:
		s = priv->route_fd;
		break;
	default:
		errno = EPFNOSUPPORT;
		return -1;
	}

	if (sendmsg(s, msg, 0) == -1)
		return -1;

	return if_getnetlink(NULL, &riov, s, 0, NULL, NULL);
}

ssize_t
ps_root_os(struct dhcpcd_ctx *ctx, struct ps_msghdr *psm, struct msghdr *msg,
    __unused void **rdata, __unused size_t *rlen, __unused bool *free_data)
{

	switch (psm->ps_cmd) {
	case PS_ROUTE:
		return ps_root_dosendnetlink(ctx, (int)psm->ps_flags, msg);
	default:
		errno = ENOTSUP;
		return -1;
	}
}

ssize_t
ps_root_sendnetlink(struct dhcpcd_ctx *ctx, int protocol, struct msghdr *msg)
{

	if (ps_sendmsg(ctx, PS_ROOT_FD(ctx), PS_ROUTE,
	    (unsigned long)protocol, msg) == -1)
		return -1;
	return ps_root_readerror(ctx, NULL, 0);
}

#ifdef DISABLE_SECCOMP
#warning SECCOMP has been disabled
#else

#if (BYTE_ORDER == LITTLE_ENDIAN)
# define SECCOMP_ARG_LO	0
# define SECCOMP_ARG_HI	sizeof(uint32_t)
#elif (BYTE_ORDER == BIG_ENDIAN)
# define SECCOMP_ARG_LO	sizeof(uint32_t)
# define SECCOMP_ARG_HI	0
#else
# error "Uknown endian"
#endif

#define SECCOMP_ALLOW(_nr)						    \
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, (_nr), 0, 1),		    \
	BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW)

#define SECCOMP_ALLOW_ARG(_nr, _arg, _val)				    \
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, (_nr), 0, 6),		    \
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,				    \
	    offsetof(struct seccomp_data, args[(_arg)]) + SECCOMP_ARG_LO),  \
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K,				    \
	    ((_val) & 0xffffffff), 0, 3),				    \
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,				    \
	    offsetof(struct seccomp_data, args[(_arg)]) + SECCOMP_ARG_HI),  \
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K,				    \
	    (((uint32_t)((uint64_t)(_val) >> 32)) & 0xffffffff), 0, 1),	    \
	BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),			\
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,				\
		offsetof(struct seccomp_data, nr))

#ifdef SECCOMP_FILTER_DEBUG
#define SECCOMP_FILTER_FAIL	SECCOMP_RET_TRAP
#else
#define SECCOMP_FILTER_FAIL	SECCOMP_RET_KILL
#endif

/* I personally find this quite nutty.
 * Why can a system header not define a default for this? */
#if defined(__i386__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_I386
#elif defined(__x86_64__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__arc__)
#  if defined(__A7__)
#    if (BYTE_ORDER == LITTLE_ENDIAN)
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARCOMPACT
#    else
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARCOMPACTBE
#    endif
#  elif defined(__HS__)
#    if (BYTE_ORDER == LITTLE_ENDIAN)
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARCV2
#    else
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARCV2BE
#    endif
#  else
#    error "Platform does not support seccomp filter yet"
#  endif
#elif defined(__ARCV3__)
#  if defined(__ARC64__)
#    if (BYTE_ORDER == LITTLE_ENDIAN)
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARCV3
#    else
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARCV3BE
#    endif
#  else
#    error "Platform does not support seccomp filter yet"
#  endif
#elif defined(__arm__)
#  ifndef EM_ARM
#    define EM_ARM 40
#  endif
#  if (BYTE_ORDER == LITTLE_ENDIAN)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARM
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ARMEB
#  endif
#elif defined(__aarch64__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_AARCH64
#elif defined(__alpha__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_ALPHA
#elif defined(__hppa__)
#  if defined(__LP64__)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_PARISC64
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_PARISC
#  endif
#elif defined(__ia64__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_IA64
#elif defined(__loongarch__)
#  if defined(__LP64__)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_LOONGARCH64
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_LOONGARCH32
#  endif
#elif defined(__microblaze__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_MICROBLAZE
#elif defined(__m68k__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_M68K
#elif defined(__mips__)
#  if defined(__MIPSEL__)
#    if defined(__LP64__)
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_MIPSEL64
#    else
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_MIPSEL
#    endif
#  elif defined(__LP64__)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_MIPS64
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_MIPS
#  endif
#elif defined(__nds32__)
#  if (BYTE_ORDER == LITTLE_ENDIAN)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_NDS32
#else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_NDS32BE
#endif
#elif defined(__nios2__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_NIOS2
#elif defined(__or1k__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_OPENRISC
#elif defined(__powerpc64__)
#  if (BYTE_ORDER == LITTLE_ENDIAN)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_PPC64LE
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_PPC64
#  endif
#elif defined(__powerpc__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_PPC
#elif defined(__riscv)
#  if defined(__LP64__)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_RISCV64
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_RISCV32
#  endif
#elif defined(__s390x__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_S390X
#elif defined(__s390__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_S390
#elif defined(__sh__)
#  if defined(__LP64__)
#    if (BYTE_ORDER == LITTLE_ENDIAN)
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_SHEL64
#    else
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_SH64
#    endif
#  else
#    if (BYTE_ORDER == LITTLE_ENDIAN)
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_SHEL
#    else
#      define SECCOMP_AUDIT_ARCH AUDIT_ARCH_SH
#    endif
#  endif
#elif defined(__sparc__)
#  if defined(__arch64__)
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_SPARC64
#  else
#    define SECCOMP_AUDIT_ARCH AUDIT_ARCH_SPARC
#  endif
#elif defined(__xtensa__)
#  define SECCOMP_AUDIT_ARCH AUDIT_ARCH_XTENSA
#else
#  error "Platform does not support seccomp filter yet"
#endif

static struct sock_filter ps_seccomp_filter[] = {
	/* Check syscall arch */
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,
	    offsetof(struct seccomp_data, arch)),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, SECCOMP_AUDIT_ARCH, 1, 0),
	BPF_STMT(BPF_RET + BPF_K, SECCOMP_FILTER_FAIL),
	/* Allow syscalls */
	BPF_STMT(BPF_LD + BPF_W + BPF_ABS,
		offsetof(struct seccomp_data, nr)),
#ifdef __NR_accept
	SECCOMP_ALLOW(__NR_accept),
#endif
#ifdef __NR_brk
	SECCOMP_ALLOW(__NR_brk),
#endif
#ifdef __NR_clock_gettime
	SECCOMP_ALLOW(__NR_clock_gettime),
#endif
#if defined(__x86_64__) && defined(__ILP32__) && defined(__X32_SYSCALL_BIT)
	SECCOMP_ALLOW(__NR_clock_gettime & ~__X32_SYSCALL_BIT),
#endif
#ifdef __NR_clock_gettime32
	SECCOMP_ALLOW(__NR_clock_gettime32),
#endif
#ifdef __NR_clock_gettime64
	SECCOMP_ALLOW(__NR_clock_gettime64),
#endif
#ifdef __NR_close
	SECCOMP_ALLOW(__NR_close),
#endif
#ifdef __NR_dup2
	SECCOMP_ALLOW(__NR_dup2), // daemonising dups stderr to stdin(/dev/null)
#endif
#ifdef __NR_dup3
	SECCOMP_ALLOW(__NR_dup3),
#endif
#ifdef __NR_epoll_ctl
	SECCOMP_ALLOW(__NR_epoll_ctl),
#endif
#ifdef __NR_epoll_wait
	SECCOMP_ALLOW(__NR_epoll_wait),
#endif
#ifdef __NR_epoll_pwait
	SECCOMP_ALLOW(__NR_epoll_pwait),
#endif
#ifdef __NR_exit_group
	SECCOMP_ALLOW(__NR_exit_group),
#endif
#ifdef __NR_fcntl
	SECCOMP_ALLOW(__NR_fcntl),
#endif
#ifdef __NR_fcntl64
	SECCOMP_ALLOW(__NR_fcntl64),
#endif
#ifdef __NR_fstat
	SECCOMP_ALLOW(__NR_fstat),
#endif
#ifdef __NR_fstat64
	SECCOMP_ALLOW(__NR_fstat64),
#endif
#ifdef __NR_gettimeofday
	SECCOMP_ALLOW(__NR_gettimeofday),
#endif
#ifdef __NR_getpid
	SECCOMP_ALLOW(__NR_getpid),
#endif
#ifdef __NR_getrandom
	SECCOMP_ALLOW(__NR_getrandom),
#endif
#ifdef __NR_getsockopt
	/* For route socket overflow */
	SECCOMP_ALLOW_ARG(__NR_getsockopt, 1, SOL_SOCKET),
	SECCOMP_ALLOW_ARG(__NR_getsockopt, 2, SO_RCVBUF),
#endif
#ifdef __NR_ioctl
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, SIOCGIFFLAGS),
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, SIOCGIFHWADDR),
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, SIOCGIFINDEX),
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, SIOCGIFMTU),
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, SIOCGIFVLAN),
	/* printf over serial terminal requires this */
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, TCGETS),
	/* dumping leases on musl requires this */
	SECCOMP_ALLOW_ARG(__NR_ioctl, 1, TIOCGWINSZ),
	/* SECCOMP BPF is newer than nl80211 so we don't need SIOCGIWESSID
	 * which lives in the impossible to include linux/wireless.h header */
#endif
#ifdef __NR_madvise /* needed for musl */
	SECCOMP_ALLOW(__NR_madvise),
#endif
#ifdef __NR_mmap
	SECCOMP_ALLOW(__NR_mmap),
#endif
#ifdef __NR_mmap2
	SECCOMP_ALLOW(__NR_mmap2),
#endif
#ifdef __NR_munmap
	SECCOMP_ALLOW(__NR_munmap),
#endif
#ifdef __NR_newfstatat
	SECCOMP_ALLOW(__NR_newfstatat),
#endif
#ifdef __NR_ppoll
	SECCOMP_ALLOW(__NR_ppoll),
#endif
#ifdef __NR_ppoll_time64
	SECCOMP_ALLOW(__NR_ppoll_time64),
#endif
#ifdef __NR_pselect6
	SECCOMP_ALLOW(__NR_pselect6),
#endif
#ifdef __NR_pselect6_time64
	SECCOMP_ALLOW(__NR_pselect6_time64),
#endif
#ifdef __NR_read
	SECCOMP_ALLOW(__NR_read),
#endif
#ifdef __NR_readv
	SECCOMP_ALLOW(__NR_readv),
#endif
#ifdef __NR_recv
	SECCOMP_ALLOW(__NR_recv),
#endif
#ifdef __NR_recvfrom
	SECCOMP_ALLOW(__NR_recvfrom),
#endif
#ifdef __NR_recvmsg
	SECCOMP_ALLOW(__NR_recvmsg),
#endif
#ifdef __NR_rt_sigreturn
	SECCOMP_ALLOW(__NR_rt_sigreturn),
#endif
#ifdef __NR_send
	SECCOMP_ALLOW(__NR_send),
#endif
#ifdef __NR_sendmsg
	SECCOMP_ALLOW(__NR_sendmsg),
#endif
#ifdef __NR_sendto
	SECCOMP_ALLOW(__NR_sendto),
#endif
#ifdef __NR_socketcall
	/* i386 needs this and demonstrates why SECCOMP
	 * is poor compared to OpenBSD pledge(2) and FreeBSD capsicum(4)
	 * as this is soooo tied to the kernel API which changes per arch
	 * and likely libc as well. */
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_ACCEPT),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_ACCEPT4),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_LISTEN),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_GETSOCKOPT),	/* overflow */
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_RECV),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_RECVFROM),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_RECVMSG),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_SEND),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_SENDMSG),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_SENDTO),
	SECCOMP_ALLOW_ARG(__NR_socketcall, 0, SYS_SHUTDOWN),
#endif
#ifdef __NR_shutdown
	SECCOMP_ALLOW(__NR_shutdown),
#endif
#ifdef __NR_time
	SECCOMP_ALLOW(__NR_time),
#endif
#ifdef __NR_wait4
	SECCOMP_ALLOW(__NR_wait4),
#endif
#ifdef __NR_waitpid
	SECCOMP_ALLOW(__NR_waitpid),
#endif
#ifdef __NR_write
	SECCOMP_ALLOW(__NR_write),
#endif
#ifdef __NR_writev
	SECCOMP_ALLOW(__NR_writev),
#endif
#ifdef __NR_uname
	SECCOMP_ALLOW(__NR_uname),
#endif

	/* Deny everything else */
	BPF_STMT(BPF_RET + BPF_K, SECCOMP_FILTER_FAIL),
};

static struct sock_fprog ps_seccomp_prog = {
	.len = (unsigned short)__arraycount(ps_seccomp_filter),
	.filter = ps_seccomp_filter,
};

#ifdef SECCOMP_FILTER_DEBUG
static void
ps_seccomp_violation(__unused int signum, siginfo_t *si, __unused void *context)
{

	logerrx("%s: unexpected syscall %d (arch=0x%x)",
	    __func__, si->si_syscall, si->si_arch);
	_exit(EXIT_FAILURE);
}

static int
ps_seccomp_debug(void)
{
	struct sigaction sa = {
		.sa_flags = SA_SIGINFO,
		.sa_sigaction = &ps_seccomp_violation,
	};
	sigset_t mask;

	/* Install a signal handler to catch any issues with our filter. */
	sigemptyset(&mask);
	sigaddset(&mask, SIGSYS);
	if (sigaction(SIGSYS, &sa, NULL) == -1 ||
	    sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
		return -1;

	return 0;
}
#endif

int
ps_seccomp_enter(void)
{

#ifdef SECCOMP_FILTER_DEBUG
	ps_seccomp_debug();
#endif

	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1 ||
	    prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &ps_seccomp_prog) == -1)
	{
		if (errno == EINVAL)
			errno = ENOSYS;
		return -1;
	}
	return 0;
}
#endif /* !DISABLE_SECCOMP */
