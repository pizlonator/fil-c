/*
 * Copyright (c) 2024-2025 Epic Games, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY EPIC GAMES, INC. ``AS IS AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL EPIC GAMES, INC. OR
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
	const void* timeout;
	volatile void* uaddr2;
	unsigned long val3;
};

#define FUTEX_WAIT    0
#define FUTEX_WAKE    1
#define FUTEX_PRIVATE 128

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
    case 202: /* SYS_futex */ {
        struct futex_args* args = (struct futex_args*)syscall_args;
        switch (args->futex_op) {
        case FUTEX_WAIT:
        case FUTEX_WAIT | FUTEX_PRIVATE:
            ZASSERT(!args->timeout);
            zsys_futex_wait(args->uaddr, args->val, args->futex_op & FUTEX_PRIVATE);
            return 0;
        case FUTEX_WAKE:
        case FUTEX_WAKE | FUTEX_PRIVATE:
            zsys_futex_wake(args->uaddr, args->val, args->futex_op & FUTEX_PRIVATE);
            return 0;
        default:
            zerrorf("unsupported futex op: %d.", args->futex_op);
            return -1;
        }
    }

    case 217 /* SYS_getdents64 */:
        callee = zsys_getdents;
        break;

    case 238 /* SYS_set_mempolicy */:
        callee = zsys_set_mempolicy;
        break;

    case 239 /* SYS_get_mempolicy */:
        callee = zsys_get_mempolicy;
        break;

    case 444 /* SYS_landlock_create_ruleset */:
        callee = zsys_landlock_create_ruleset;
        break;

    case 445 /* SYS_landlock_add_rule */:
        callee = zsys_landlock_add_rule;
        break;

    case 446 /* SYS_landlock_restrict_self */:
        callee = zsys_landlock_restrict_self;
        break;

    case 298 /* SYS_perf_event_open */:
        callee = zsys_perf_event_open;
        break;

    case 186 /* SYS_gettid */:
        callee = zthread_self_id;
        break;

    case 318 /* SYS_getrandom */:
        callee = zsys_getrandom;
        break;

    case 39 /* SYS_getpid */:
        callee = zsys_getpid;
        break;

    case 452 /* SYS_fchmodat2 */:
        /* The pizlonated fchmodat syscall is really an interface to fchmodat2 on modern kernels, and
           on older kernels, it's an interface to a reasonably faithful emulation of fchmodat2.
        
           In particular, zsys_fchmodat does take a flags argument, and correctly handles the case
           where it's nonzero. */
        callee = zsys_fchmodat;
        break;

    case 164 /* SYS_settimeofday */:
        callee = zsys_settimeofday;
        break;

    case 3 /* SYS_close */:
        /* NOTE: Folks do this because they want a "nocancel" version of close(2). */
        callee = zsys_close;
        break;

    case 332 /* SYS_statx */:
        callee = zsys_statx;
        break;

    case 326 /* SYS_copy_file_range */:
        callee = zsys_copy_file_range;
        break;

    case 316 /* SYS_renameat2 */:
        callee = zsys_renameat2;
        break;

    case 434 /* SYS_pidfd_open */:
        callee = zsys_pidfd_open;
        break;

    case 113 /* SYS_setreuid */:
        callee = zsys_setreuid;
        break;

    case 114 /* SYS_setregid */:
        callee = zsys_setregid;
        break;

    case 117 /* SYS_setresuid */:
        callee = zsys_setresuid;
        break;

    case 250 /* SYS_keyctl */:
        callee = zsys_keyctl;
        break;

    case 203 /* SYS_sched_getaffinity */:
        callee = zsys_sched_getaffinity;
        break;

    case 204 /* SYS_sched_setaffinity */:
        callee = zsys_sched_setaffinity;
        break;

    case 248 /* SYS_add_key */:
        callee = zsys_add_key;
        break;

    case 249 /* SYS_request_key */:
        callee = zsys_request_key;
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
