#define _GNU_SOURCE
#include "pthread_impl.h"
#include "libc.h"
#include <sys/mman.h>

int pthread_getattr_np(pthread_t t, pthread_attr_t *a)
{
	*a = (pthread_attr_t){0};
	a->_a_detach = t->detach_state>=DT_DETACHED;
	a->_a_guardsize = t->guard_size;
        a->_a_stackaddr = zthread_stack_top(t->zthread);
        a->_a_stacksize = (char*)a->_a_stackaddr - (char*)zthread_stack_limit(t->zthread);
	return 0;
}
