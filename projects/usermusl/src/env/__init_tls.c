#define SYSCALL_NO_TLS 1
#include <elf.h>
#include <limits.h>
#include <sys/mman.h>
#include <string.h>
#include <stddef.h>
#include "pthread_impl.h"
#include "libc.h"
#include "atomic.h"
#include "syscall.h"

volatile int __thread_list_lock;

int __init_tp(void *p)
{
	pthread_t td = p;
	td->self = td;
        zthread_set_self_cookie(p);
	libc.can_do_threads = 1;
	td->detach_state = DT_JOINABLE;
	td->tid = zthread_self_id();
        td->zthread = zthread_self();
	ZASSERT(td->tid);
	td->locale = &libc.global_locale;
	td->robust_list.head = &td->robust_list.head;
	td->sysinfo = __sysinfo;
	td->next = td->prev = td;
	return 0;
}

struct pthread builtin_pt;

#if ULONG_MAX == 0xffffffff
typedef Elf32_Phdr Phdr;
#else
typedef Elf64_Phdr Phdr;
#endif

extern weak hidden const size_t _DYNAMIC[];

static void static_init_tls(size_t *aux)
{
	unsigned char *p;
	size_t n;
	Phdr *phdr, *tls_phdr=0;
	size_t base = 0;
	void *mem;

	if (__init_tp(&builtin_pt) < 0)
		a_crash();
}

weak_alias(static_init_tls, __init_tls);
