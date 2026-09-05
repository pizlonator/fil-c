/*
 * Copyright (c) 2024-2026 Epic Games, Inc. All Rights Reserved.
 * Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/* This is the memory safe part of the Fil-C runtime. Most of the runtime is in filc_runtime.c, but
   that's compiled with Yolo-C, and so it isn't memory safe. Anytime we have an opportunity to
   implement runtime functionality using Fil-C itself, we put that code here. */

#include <stdfil.h>
#include <pizlonated_syscalls.h>
#include <pizlonated_runtime.h>
#include <linux/futex.h>
#include <linux/time.h>
#include "syscall.h"

unsigned zversion(void)
{
    return FILC_VERSION;
}

struct lock {
    int word;
};

struct fd_backer {
    /* FIXME: What we really want is an fd_table! The whole idea of epoll is that we can register
       zero or one pointers with each fd in each epoll fd.
    
       It would probably be fine to have a table that is protected by a lock, but then we'd have to
       make sure that we lock the lock for fork. And that's hella annoying to get right, because
       multiple fd's could point to the same backer.
    
       And, curiously, this exact_ptrtable will do the job fine. The only risk is that we get a leak
       because the pointers used in the epoll events aren't ever freed. That would also require the
       user to be repeatedly MODing their epoll entries, each time with a pointer they don't free.
    
       Seems hella unlikely. Therefore, using the exact_ptrtable is expedient for now. */
    zexact_ptrtable* epoll_table;
};

struct fd_holder {
    struct lock lock;
    struct fd_backer* backer;
};

static struct fd_holder* _Atomic fd_table;

#define LOCK_NOT_HELD 0
#define LOCK_HELD 1
#define LOCK_HELD_WAITING 2

static void lock_init(struct lock* lock)
{
    lock->word = LOCK_NOT_HELD;
}

static int int_cas(int* ptr, int expected, int new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic int*)ptr, &expected, new_value,
                                         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}

static void lock_lock(struct lock* lock)
{
    zincrement_signal_deferral_depth();
    
    if (int_cas(&lock->word, LOCK_NOT_HELD, LOCK_HELD) == LOCK_NOT_HELD)
        return;

    unsigned count = 40;
    while (count--) {
        int old_state = int_cas(&lock->word, LOCK_NOT_HELD, LOCK_HELD);
        if (old_state == LOCK_NOT_HELD)
            return;
        if (old_state == LOCK_HELD_WAITING)
            break;
        zsys_sched_yield();
    }

    /* The trick is that if we ever choose to wait, then we will acquire the lock in the waiting
       state. This ensures that the lock never forgets that there are threads waiting. It is also
       slightly conservative: if there's a queue of threads waiting, then the last thread in the
       queue will acquire in waiting mode and then do a wake when unlocking, even though it doesn't
       strictly have to since it's the last one. */
    int locked_state = LOCK_HELD;
    for (;;) {
        int old_state = lock->word;

        if (old_state == LOCK_NOT_HELD) {
            if (int_cas(&lock->word, LOCK_NOT_HELD, locked_state) == LOCK_NOT_HELD)
                return;
            continue;
        }

        if (old_state == LOCK_HELD) {
            if (int_cas(&lock->word, LOCK_HELD, LOCK_HELD_WAITING) != LOCK_HELD)
                continue;
        } else
            ZASSERT(old_state == LOCK_HELD_WAITING);
        locked_state = LOCK_HELD_WAITING;

        zsys_futex_wait((volatile int*)&lock->word, LOCK_HELD_WAITING, 0);
    }
}

static void lock_unlock(struct lock* lock)
{
    for (;;) {
        if (int_cas(&lock->word, LOCK_HELD, LOCK_NOT_HELD) == LOCK_HELD)
            break;

        int old_state = lock->word;
        ZASSERT(old_state == LOCK_HELD || old_state == LOCK_HELD_WAITING);

        if (int_cas(&lock->word, LOCK_HELD_WAITING, LOCK_NOT_HELD) == LOCK_HELD_WAITING) {
            zsys_futex_wake((volatile int*)&lock->word, 1, 0);
            break;
        }
    }

    zdecrement_signal_deferral_depth();
}

/* Consider this race:
   
   - One thread has created an fd, but hasn't placed it into the table.
   
   - Another thread closes that fd.

   If I intercepted all fd creation operations, then I could handle this with a negative ref_count,
   maybe. But I don't.
   
   But what is the worst case here? I can just ignore close operations on fds that I don't yet know
   about. Then, in the case of this race, I'll have a data structure describing an epoll handle that
   has been closed, and I'll keep it around until *another* close operation.

   That's harmelss, since it means that if a program has this race then I'll just think that the fd
   needs epoll tracking even though it doesn't, and that epoll tracking won't have anything in it
   unless the user attempts epoll operations on the fd (and those operations will fail anyway). */

enum fd_nonexistence_mode {
    fd_nonexistent_return_null,
    fd_nonexistent_create
};
static struct fd_holder* get_locked_fd_holder_impl(int fd,
                                                   enum fd_nonexistence_mode nonexistence_mode)
{
    ZASSERT(fd >= 0);

    for (;;) {
        struct fd_holder* table = fd_table;
        
        if (!table || (__SIZE_TYPE__)fd >= zlength(table)) {
            if (nonexistence_mode == fd_nonexistent_return_null)
                return 0;
            struct fd_holder* old_table = table;
            __SIZE_TYPE__ new_length = ((__SIZE_TYPE__)fd + 1) * 2;
            struct fd_holder* new_table = zgc_alloc(sizeof(struct fd_holder) * new_length);
            ZASSERT(zlength(new_table) >= new_length);

            __SIZE_TYPE__ index;
            for (index = 0; index < zlength(old_table); ++index)
                lock_lock(&old_table[index].lock);

            _Bool did_resize = 0;
            if (old_table == fd_table) {
                for (index = zlength(new_table); index--;) {
                    struct fd_holder* new_holder = new_table + index;
                    lock_init(&new_holder->lock);
                    if (index < zlength(old_table)) {
                        struct fd_holder* old_holder = old_table + index;
                        new_holder->backer = old_holder->backer;
                    }
                }
                /* This CAS is necessary in case we're the first ones creating the table. */
                struct fd_holder* expected_table = old_table;
                __c11_atomic_compare_exchange_strong(&fd_table, &expected_table, new_table,
                                                     __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
                if (expected_table == old_table) {
                    fd_table = new_table;
                    table = new_table;
                    did_resize = 1;
                }
            }

            for (index = zlength(old_table); index--;)
                lock_unlock(&old_table[index].lock);

            if (!did_resize)
                continue;

            ZASSERT((__SIZE_TYPE__)fd < zlength(table));
        }

        struct fd_holder* holder = table + fd;
        lock_lock(&holder->lock);
        if (fd_table == table)
            return holder;
        lock_unlock(&holder->lock);
    }
}

static struct fd_holder* get_locked_fd_holder(int fd)
{
    return get_locked_fd_holder_impl(fd, fd_nonexistent_create);
}

static struct fd_holder* get_locked_existing_fd_holder(int fd)
{
    return get_locked_fd_holder_impl(fd, fd_nonexistent_return_null);
}

static void lock_table(void)
{
    /* The way that this is written means that we might lock the new table while some thread is still
       holding the lock on the old table. But that's fine. Any new thread that comes along to do
       anything with the table will be dealing with the new table that we have locked. */
    
    /* Make sure there is a table at all. */
    if (!fd_table)
        lock_unlock(&get_locked_fd_holder(0)->lock);

    for (;;) {
        struct fd_holder* table = fd_table;
        
        __SIZE_TYPE__ index;
        for (index = 0; index < zlength(table); ++index)
            lock_lock(&table[index].lock);

        if (table == fd_table)
            return;

        for (index = zlength(table); index--;)
            lock_unlock(&table[index].lock);
    }
}

static void unlock_table(void)
{
    ZASSERT(fd_table);

    struct fd_holder* table = fd_table;
    __SIZE_TYPE__ index;
    for (index = zlength(table); index--;)
        lock_unlock(&table[index].lock);
}

static struct fd_backer* fd_backer_create(void)
{
    struct fd_backer* result = zgc_alloc(sizeof(struct fd_backer));
    result->epoll_table = zexact_ptrtable_new_weak();
    return result;
}

static struct fd_backer* get_fd_backer(int fd)
{
    struct fd_holder* holder = get_locked_fd_holder(fd);
    struct fd_backer* backer = holder->backer;
    lock_unlock(&holder->lock);
    return backer;
}

static void set_fd_backer(int fd, struct fd_backer* backer)
{
    struct fd_holder* holder = get_locked_fd_holder(fd);
    holder->backer = backer;
    lock_unlock(&holder->lock);
}

int zsys_close(int fd)
{
    /* It's possible for the close to fail with EINTR, so we have to make sure that we only null
       the backer if the close succeeded. */
    struct fd_holder* holder;
    if (fd >= 0)
        holder = get_locked_existing_fd_holder(fd);
    else
        holder = 0;
    int result = zsys_close_impl(fd);
    if (holder) {
        if (!result)
            holder->backer = 0;
        lock_unlock(&holder->lock);
    }
    return result;
}

int zsys_fcntl(int fd, int cmd, ...)
{
    if (fd < 0) {
        zset_errno(9); /* EBADF */
        return -1;
    }
    switch (cmd) {
    case 0: /* F_DUPFD */
    case 1030: { /* F_DUPFD_CLOEXEC */
        struct fd_backer* backer = get_fd_backer(fd);
        int result = *(int*)zcall(zsys_fcntl_impl, zargs());
        if (result >= 0)
            set_fd_backer(result, backer);
        return result;
    }

    default:
        return *(int*)zcall(zsys_fcntl_impl, zargs());
    }
}

int zsys_dup(int fd)
{
    if (fd < 0) {
        zset_errno(9); /* EBADF */
        return -1;
    }
    struct fd_backer* backer = get_fd_backer(fd);
    int result = zsys_dup_impl(fd);
    if (result >= 0)
        set_fd_backer(result, backer);
    return result;
}

int zsys_dup2(int oldfd, int newfd)
{
    if (oldfd < 0) {
        zset_errno(9); /* EBADF */
        return -1;
    }
    struct fd_backer* backer = get_fd_backer(oldfd);
    int result = zsys_dup2_impl(oldfd, newfd);
    if (result >= 0)
        set_fd_backer(result, backer);
    return result;
}

int zsys_fork(void)
{
    lock_table();
    int result = zsys_fork_impl();
    unlock_table();
    return result;
}

int zsys_epoll_create1(int flags)
{
    int result = zsys_epoll_create1_impl(flags);
    if (result >= 0)
        set_fd_backer(result, fd_backer_create());
    return result;
}

typedef union epoll_data {
	void *ptr;
	int fd;
	unsigned u32;
	unsigned long long u64;
} epoll_data_t;

struct epoll_event {
	unsigned events;
	epoll_data_t data;
};

int zsys_epoll_ctl(int epfd, int op, int fd, void* raw_event)
{
    struct epoll_event* event = raw_event;
    if (event) {
        struct fd_backer* backer = get_fd_backer(epfd);
        if (backer)
            zexact_ptrtable_encode(backer->epoll_table, event->data.ptr);
    }
    return zsys_epoll_ctl_impl(epfd, op, fd, event);
}

static void fix_events(int epfd, void* raw_events, int result)
{
    struct fd_backer* backer = get_fd_backer(epfd);
    if (!backer)
        return;
    struct epoll_event* events = raw_events;
    int index;
    for (index = 0; index < result; ++index) {
        events[index].data.ptr = zexact_ptrtable_decode(
            backer->epoll_table, (__SIZE_TYPE__)events[index].data.ptr);
    }
}

int zsys_epoll_wait(int epfd, void* events, int maxevents, int timeout)
{
    int result = zsys_epoll_wait_impl(epfd, events, maxevents, timeout);
    fix_events(epfd, events, result);
    return result;
}

int zsys_epoll_pwait(int epfd, void* events, int maxevents, int timeout, const void* sigmask)
{
    int result = zsys_epoll_pwait_impl(epfd, events, maxevents, timeout, sigmask);
    fix_events(epfd, events, result);
    return result;
}

int zsys_epoll_pwait2(int epfd, void* events, int maxevents, const void* timeout, const void* sigmask)
{
    int result = zsys_epoll_pwait2_impl(epfd, events, maxevents, timeout, sigmask);
    fix_events(epfd, events, result);
    return result;
}

int zsys_close_range(unsigned first, unsigned last, int flags)
{
    if (flags) {
        /* The flags can be CLOSE_RANGE_CLOEXEC or CLOSE_RANGE_UNSHARE.
           
           In the case of CLOSE_RANGE_CLOEXEC, we're not actually closing anything, so we don't have
           to deal with our fd table.
           
           In the case of CLOSE_RANGE_UNSHARE, we're unsharing the file descriptors from other
           threads, so we cannot remove them from the table. */
        return zsys_close_range_impl(first, last, flags);
    }

    /* FIXME: This could be made so much more efficient! We only have to lock the parts of the table
       that we're going to access. */
    lock_table();
    int result = zsys_close_range_impl(first, last, flags);
    if (!result) {
        __SIZE_TYPE__ index;
        for (index = first; index <= last && index < zlength(fd_table); index++) {
            struct fd_holder* holder = fd_table + index;
            holder->backer = 0;
        }
    }
    unlock_table();
    return result;
}

int zsys_dup3(int oldfd, int newfd, int flags)
{
    struct fd_backer* backer = get_fd_backer(oldfd);
    int result = zsys_dup3_impl(oldfd, newfd, flags);
    if (result >= 0)
        set_fd_backer(result, backer);
    return result;
}

struct futex_args {
	volatile void* uaddr;
	long futex_op;
	unsigned long val;
	const struct timespec* timeout;
	volatile void* uaddr2;
	unsigned long val3;
};

long zsys_syscall(long n, ...)
{
    /* The goal is to have this code support all syscalls, but it doesn't do that, yet. So,
       it traps on syscalls it doesn't know about.
	
       That might be OK since the primary use case of syscall(2) is to make syscalls that libc
       doesn't expose as a function. So, it's unlikely we'll ever see a legitimate call to this
       function asking for something like SYS_write, for example.
	
       But if we find such a case, then we'll have to support it because the C programmer is
       always right! */
	
    void* syscall_args = (char*)zargs() + 8;
    void* callee;
    switch (n) {
    case SYS_futex: {
        struct futex_args* args = (struct futex_args*)syscall_args;
        switch (args->futex_op) {
        case FUTEX_WAIT:
        case FUTEX_WAIT | FUTEX_PRIVATE_FLAG:
        case FUTEX_WAIT | FUTEX_CLOCK_REALTIME:
        case FUTEX_WAIT | FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME:
        case FUTEX_WAIT_BITSET:
        case FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG:
        case FUTEX_WAIT_BITSET | FUTEX_CLOCK_REALTIME:
        case FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME: {
            if (args->futex_op & FUTEX_WAIT_BITSET)
                ZASSERT(args->val3 == FUTEX_BITSET_MATCH_ANY);
            int clock_id = (args->futex_op & FUTEX_CLOCK_REALTIME) ? CLOCK_REALTIME : CLOCK_MONOTONIC;
            struct timespec timeout = {};
            if (args->timeout) {
                if (!(args->futex_op & FUTEX_WAIT_BITSET))
                    zsys_clock_gettime(clock_id, &timeout);
                timeout.tv_sec += args->timeout->tv_sec;
                timeout.tv_nsec += args->timeout->tv_nsec;
                if (timeout.tv_nsec >= 1000000000) {
                    timeout.tv_sec++;
                    timeout.tv_nsec -= 1000000000;
                }
            }
            int err = zsys_futex_timedwait(args->uaddr, args->val, clock_id,
                args->timeout ? &timeout : 0, args->futex_op & FUTEX_PRIVATE_FLAG);
            if (err > 0) {
                zset_errno(err);
                return -1;
            }
            return 0;
        }
        case FUTEX_WAKE:
        case FUTEX_WAKE | FUTEX_PRIVATE_FLAG:
            zsys_futex_wake(args->uaddr, args->val, args->futex_op & FUTEX_PRIVATE_FLAG);
            return 0;
        default:
            zerrorf("unsupported futex op: %d.", args->futex_op);
            return -1;
        }
    }

    case SYS_getdents64:
        callee = zsys_getdents;
        break;

    case SYS_set_mempolicy:
        callee = zsys_set_mempolicy;
        break;

    case SYS_get_mempolicy:
        callee = zsys_get_mempolicy;
        break;

    case SYS_landlock_create_ruleset:
        callee = zsys_landlock_create_ruleset;
        break;

    case SYS_landlock_add_rule:
        callee = zsys_landlock_add_rule;
        break;

    case SYS_landlock_restrict_self:
        callee = zsys_landlock_restrict_self;
        break;

    case SYS_perf_event_open:
        callee = zsys_perf_event_open;
        break;

    case SYS_gettid:
        callee = zthread_self_id;
        break;

    case SYS_getrandom:
        callee = zsys_getrandom;
        break;

    case SYS_getpid:
        callee = zsys_getpid;
        break;

    case SYS_fchmodat2:
        /* The pizlonated fchmodat syscall is really an interface to fchmodat2 on modern kernels, and
           on older kernels, it's an interface to a reasonably faithful emulation of fchmodat2.
        
           In particular, zsys_fchmodat does take a flags argument, and correctly handles the case
           where it's nonzero. */
        callee = zsys_fchmodat;
        break;

    case SYS_settimeofday:
        callee = zsys_settimeofday;
        break;

    case SYS_close:
        /* NOTE: Folks do this because they want a "nocancel" version of close(2). */
        callee = zsys_close;
        break;

    case SYS_statx:
        callee = zsys_statx;
        break;

    case SYS_copy_file_range:
        callee = zsys_copy_file_range;
        break;

    case SYS_renameat2:
        callee = zsys_renameat2;
        break;

    case SYS_pidfd_open:
        callee = zsys_pidfd_open;
        break;

    case SYS_setreuid:
        callee = zsys_setreuid;
        break;

    case SYS_setregid:
        callee = zsys_setregid;
        break;

    case SYS_setresuid:
        callee = zsys_setresuid;
        break;

    case SYS_keyctl:
        callee = zsys_keyctl;
        break;

    case SYS_sched_setaffinity:
        callee = zsys_sched_setaffinity;
        break;

    case SYS_sched_getaffinity:
        callee = zsys_raw_sched_getaffinity;
        break;

    case SYS_add_key:
        callee = zsys_add_key;
        break;

    case SYS_request_key:
        callee = zsys_request_key;
        break;

    case SYS_memfd_create:
        callee = zsys_memfd_create;
        break;

    case SYS_write:
        callee = zsys_write;
        break;

    case SYS_openat2:
        callee = zsys_openat2;
        break;

    case SYS_inotify_init1:
        callee = zsys_inotify_init1;
        break;

    case SYS_inotify_add_watch:
        callee = zsys_inotify_add_watch;
        break;

    case SYS_inotify_rm_watch:
        callee = zsys_inotify_rm_watch;
        break;

    case SYS_sigaltstack:
        callee = zsys_sigaltstack;
        break;

	/* FIXME: Implement more syscalls! */

    default:
        zerrorf("unsupported syscall: %ld.", n);
        return -1;
    }
	
    return *(long*)zcall(callee, syscall_args);
}

void* zthread_self_cookie(void)
{
    return zthread_get_cookie(zthread_self());
}

void* zthread_create(void* (*callback)(void* arg), void* arg)
{
    void* result = 0;
    zthread_create2(callback, arg, &result, 0);
    return result;
}

void* zstack_limit(void)
{
    return zthread_stack_limit(zthread_self());
}

void* zstack_top(void)
{
    return zthread_stack_top(zthread_self());
}

int zsys_gettid(void)
{
    return zthread_self_id();
}

int zsys_tkill(int tid, int sig)
{
    (void)tid;
    (void)sig;
    zerror("tkill not supported.");
    return -1;
}

int zsys_tgkill(int tgid, int tid, int sig)
{
    (void)tgid;
    (void)tid;
    (void)sig;
    zerror("tgkill not supported.");
    return -1;
}

void* zsys_create_module(const char* name, __SIZE_TYPE__ size)
{
    /* We don't support this because this syscall has been removed. */
    (void)name;
    (void)size;
    zerror("create_module not supported.");
    return 0;
}

int zsys_query_module(const char* name, int which, void* buf, __SIZE_TYPE__ bufsize,
    __SIZE_TYPE__* ret)
{
    (void)name;
    (void)which;
    (void)buf;
    (void)bufsize;
    (void)ret;
    zerror("query_module not supported.");
    return -1;
}

int zsys_get_kernel_syms(void* table)
{
    /* We don't support this because this syscall has been removed. */
    (void)table;
    zerror("get_kernel_syms not supported.");
    return -1;
}

long zsys_nfsservctl(int cmd, void* argp, void* resp)
{
    /* We don't support this because this syscall has been removed. */
    (void)cmd;
    (void)argp;
    (void)resp;
    zerror("nfsservctl not supported.");
    return -1;
}

int zsys_uselib(const char* library)
{
    /* We don't support this because it's probably super unsafe and it's no longer really supported.
       You can build a kernel that still has this syscall, but glibc supposedly doesn't need it
       anymore. */
    (void)library;
    zerror("uselib not supported.");
    return -1;
}

struct zweak_map_iter {
    void** snapshot;
    __SIZE_TYPE__ index_plus_two;
};

zweak_map_iter* zweak_map_get_iter(zweak_map* map)
{
    zweak_map_iter* result = zgc_alloc(sizeof(zweak_map_iter));
    result->snapshot = zweak_map_snapshot_impl(map);
    result->index_plus_two = 0;
    return result;
}

filc_bool zweak_map_iter_next(zweak_map_iter* iter)
{
    if (!iter->snapshot[iter->index_plus_two + 1])
        return 0;
    iter->index_plus_two += 2;
    return 1;
}

void* zweak_map_iter_key(zweak_map_iter* iter)
{
    return iter->snapshot[iter->index_plus_two - 2];
}

void* zweak_map_iter_value(zweak_map_iter* iter)
{
    return iter->snapshot[iter->index_plus_two - 1];
}

void zgc_request_and_wait(void)
{
    zgc_wait(zgc_request_fresh());
}

/* Support for atomic accesses that the compiler cannot handle natively. Two kinds of calls
   end up here:

   - Atomics on objects bigger than 16 bytes: the frontend emits calls to __atomic_load,
     __atomic_store, __atomic_exchange, __atomic_compare_exchange, and __atomic_is_lock_free.

   - Atomics whose alignment is less than their size (e.g. a 16-byte atomic with 8-byte
     alignment, which cannot use cmpxchg16b on x86_64): a pre-pass in the pizlonator
     (convertMisalignedAtomicsToLibcalls in FilPizlonator.cpp) converts those into calls to
     __atomic_load & friends. If we didn't do that, the backend - which runs after the
     pizlonator - would emit raw, unpizlonated calls to those symbols, which could never bind
     to the pizlonated implementations here; worse, such a raw libcall would copy the payload
     but not the capabilities of any pointers in the accessed memory. (Note that even with
     the pizlonator's conversion, the capabilities of pointers inside flattened-integer
     atomics are lost, since the frontend emits such atomics on flattened integer types.
     That's consistent with how aligned 16-byte atomics behave.)

   This is the same deal as compiler-rt's atomic.c: since every atomic access to such an
   object goes through these functions, we can make the accesses appear atomic by serializing
   them with locks.

   We use a hashed array of locks rather than a per-object lock, so that we don't need any
   per-object state. */

#define ATOMIC_LOCK_COUNT_LOG2 10
#define ATOMIC_LOCK_COUNT (1 << ATOMIC_LOCK_COUNT_LOG2)
#define ATOMIC_LOCK_MASK (ATOMIC_LOCK_COUNT - 1)

/* The locks are zero-initialized, and zero is LOCK_NOT_HELD, so there is no need to call
   lock_init on them. */
static struct lock atomic_locks[ATOMIC_LOCK_COUNT];

/* We cannot use memcmp from libc here, since we are part of libpizlo, which is linked after
   libc. Note that __builtin_memcmp would just turn into a bcmp call, which has the same
   problem. */
static int atomic_mem_equal(void* a, void* b, __SIZE_TYPE__ size)
{
    unsigned char* ac = a;
    unsigned char* bc = b;
    while (size--) {
        if (*ac++ != *bc++)
            return 0;
    }
    return 1;
}

static struct lock* atomic_lock_for_ptr(void* ptr)
{
    __UINTPTR_TYPE__ hash = (__UINTPTR_TYPE__)ptr;
    /* Disregard the lowest 4 bits. We want all values that may be part of the same memory
       operation to hash to the same value and therefore use the same lock. */
    hash >>= 4;
    /* Use the next bits as the basis for the hash. */
    __UINTPTR_TYPE__ low = hash & ATOMIC_LOCK_MASK;
    /* Now use the higher bits to perturb the hash, so that we don't get collisions from atomic
       fields in a single object. */
    hash >>= 16;
    hash ^= low;
    return atomic_locks + (hash & ATOMIC_LOCK_MASK);
}

/* We cannot define functions called __atomic_load & friends directly, because the compiler
   reserves those names for builtins. So, we define them with a _c suffix and use this pragma to
   give them the real symbol names, just like compiler-rt's atomic.c does. The pizlonator then
   mangles those names the same way it mangles the call sites, so everything links up. */
#pragma redefine_extname __atomic_load_c __atomic_load
#pragma redefine_extname __atomic_store_c __atomic_store
#pragma redefine_extname __atomic_exchange_c __atomic_exchange
#pragma redefine_extname __atomic_compare_exchange_c __atomic_compare_exchange
#pragma redefine_extname __atomic_is_lock_free_c __atomic_is_lock_free

void __atomic_load_c(__SIZE_TYPE__ size, void* src, void* dest, int model)
{
    /* Taking the lock gives us seq_cst semantics regardless of the requested model, which is
       always a valid way to honor the model. */
    (void)model;
    struct lock* lock = atomic_lock_for_ptr(src);
    lock_lock(lock);
    __builtin_memcpy(dest, src, size);
    lock_unlock(lock);
}

void __atomic_store_c(__SIZE_TYPE__ size, void* dest, void* src, int model)
{
    (void)model;
    struct lock* lock = atomic_lock_for_ptr(dest);
    lock_lock(lock);
    __builtin_memcpy(dest, src, size);
    lock_unlock(lock);
}

void __atomic_exchange_c(__SIZE_TYPE__ size, void* ptr, void* val, void* old, int model)
{
    (void)model;
    struct lock* lock = atomic_lock_for_ptr(ptr);
    lock_lock(lock);
    __builtin_memcpy(old, ptr, size);
    __builtin_memcpy(ptr, val, size);
    lock_unlock(lock);
}

int __atomic_compare_exchange_c(__SIZE_TYPE__ size, void* ptr, void* expected, void* desired,
                                int success, int failure)
{
    (void)success;
    (void)failure;
    struct lock* lock = atomic_lock_for_ptr(ptr);
    lock_lock(lock);
    if (atomic_mem_equal(ptr, expected, size)) {
        __builtin_memcpy(ptr, desired, size);
        lock_unlock(lock);
        return 1;
    }
    __builtin_memcpy(expected, ptr, size);
    lock_unlock(lock);
    return 0;
}

_Bool __atomic_is_lock_free_c(__SIZE_TYPE__ size, void* ptr)
{
    /* clang does not fold __atomic_is_lock_free to a constant at call sites; it emits a real
       call to this function (verified for sizes 16 and 17). So this function is genuinely
       reachable and must return correct answers.

       An atomic operation is lock-free iff its size is 1, 2, 4, 8, or 16 and its alignment is
       at least its size (16-byte atomics use cmpxchg16b on x86_64, which requires 16-byte
       alignment). Atomics whose alignment is less than their size are converted to calls to
       the lock-based functions above by a pre-pass in the pizlonator
       (convertMisalignedAtomicsToLibcalls in FilPizlonator.cpp), and atomics bigger than 16
       bytes get libcalls from the frontend, so those are not lock-free. Note that all we can
       go on is the pointer value we see; if the compiler had less alignment information at
       compile time than the pointer happens to have at runtime, then the accesses use locks
       even though we return 1 here. */
    switch (size) {
    case 1:
    case 2:
    case 4:
    case 8:
        return !((__UINTPTR_TYPE__)ptr & (size - 1));
    case 16:
        return !((__UINTPTR_TYPE__)ptr & 15);
    default:
        return 0;
    }
}

