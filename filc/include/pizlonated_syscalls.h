/*
 * Copyright (c) 2023-2025 Epic Games, Inc. All Rights Reserved.
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

#ifndef PIZLONATED_SYSCALLS_H
#define PIZLONATED_SYSCALLS_H

#include <stdfil.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
} /* tell emacs what's up */
#endif

/* This file defines pizlonated syscall APIs.

   There's no guarantee that the APIs in this file will be stable over time.

   Currently, a lot of this is Linux-specific and the implementations assume that all structs are
   shaped according to their musl definitions. */

int zsys_ioctl(int fd, int request, ...);
long zsys_writev(int fd, const void* iov, int iovcnt);
long zsys_read(int fd, void* buf, __SIZE_TYPE__ size);
long zsys_readv(int fd, const void* iov, int iovcnt);
long zsys_write(int fd, const void* buf, __SIZE_TYPE__ size);
int zsys_close_impl(int fd);
int zsys_close(int fd);
long zsys_lseek(int fd, long offset, int whence);
void zsys_exit_soft(int return_code); /* Uses the yolo exit(3), so calls destructors. */
void zsys_exit_hard(int return_code); /* Uses the yolo _Exit(2), so no destructors. */
unsigned zsys_getuid(void);
unsigned zsys_geteuid(void);
unsigned zsys_getgid(void);
unsigned zsys_getegid(void);
int zsys_open(const char* path, int flags, ...);
int zsys_getpid(void);
int zsys_clock_gettime(int clock_id, void* timespec_ptr);
int zsys_fstatat(int fd, const char* path, void* buf, int flag);
int zsys_fstat(int fd, void* buf);
int zsys_fcntl_impl(int fd, int cmd, ...);
int zsys_fcntl(int fd, int cmd, ...);
int zsys_sigaction(int signum, const void* act, void* oact);
int zsys_pipe(int fds[2]);
int zsys_select(int nfds, void* reafds, void* writefds, void* errorfds, void* timeout);
void zsys_sched_yield(void);
int zsys_socket(int domain, int type, int protocol);
int zsys_setsockopt(int sockfd, int level, int optname, const void* optval, unsigned optlen);
int zsys_bind(int sockfd, const void* addr, unsigned addrlen);
int zsys_connect(int sockfd, const void* addr, unsigned addrlen);
int zsys_getsockname(int sockfd, void* addr, unsigned* addrlen);
int zsys_getsockopt(int sockfd, int level, int optname, void* optval, unsigned* optlen);
int zsys_getpeername(int sockfd, void* addr, unsigned* addrlen);
long zsys_sendto(int sockfd, const void* buf, __SIZE_TYPE__ len, int flags,
                 const void* addr, unsigned addrlen);
long zsys_recvfrom(int sockfd, void* buf, __SIZE_TYPE__ len, int flags,
                   void* addr, unsigned* addrlen);
int zsys_getrlimit(int resource, void* rlim);
unsigned zsys_umask(unsigned mask);
int zsys_getitimer(int which, void* curr_value);
int zsys_setitimer(int which, const void* new_value, void* old_value);
int zsys_pause(void);
int zsys_pselect(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout,
                 const void* sigmask);
int zsys_kill(int pid, int sig);
int zsys_raise(int sig);
int zsys_dup_impl(int fd);
int zsys_dup2_impl(int oldfd, int newfd);
int zsys_dup(int fd);
int zsys_dup2(int oldfd, int newfd);
int zsys_sigprocmask(int how, const void* set, void* oldset); /* This is pthread_sigmask, but sets
                                                                 errno and returns -1 on error. */
int zsys_chdir(const char* path);
int zsys_fork_impl(void);
int zsys_fork(void);
int zsys_waitpid(int pid, int* status, int options);
int zsys_listen(int sockfd, int backlog);
int zsys_accept(int sockfd, void* addr, unsigned* addrlen);
int zsys_accept4(int sockfd, void* addr, unsigned* addrlen, int flg);
int zsys_socketpair(int domain, int type, int protocol, int* sv);
int zsys_setsid(void);
int zsys_execve(const char* pathname, char*const* argv, char*const* envp);
int zsys_getppid(void);
int zsys_chroot(const char* path);
int zsys_setuid(unsigned uid);
int zsys_seteuid(unsigned uid);
int zsys_setreuid(unsigned ruid, unsigned euid);
int zsys_setgid(unsigned gid);
int zsys_setegid(unsigned gid);
int zsys_setregid(unsigned rgid, unsigned egid);
int zsys_nanosleep(const void* req, void* rem);
long zsys_readlink(const char* path, char* buf, __SIZE_TYPE__ bufsize);
int zsys_chown(const char* pathname, unsigned owner, unsigned group);
int zsys_lchown(const char* pathname, unsigned owner, unsigned group);
long zsys_sendmsg(int sockfd, const void* msg, int flags);
long zsys_recvmsg(int sockfd, void* msg, int flags);
int zsys_rename(const char* oldname, const char* newname);
int zsys_unlink(const char* path);
int zsys_link(const char* oldname, const char* newname);
void* zsys_mmap(void* address, __SIZE_TYPE__ length, int prot, int flags, int fd, long offset);
int zsys_munmap(void* address, __SIZE_TYPE__ length);
int zsys_ftruncate(int fd, long length);
char* zsys_getcwd(char* buf, __SIZE_TYPE__ size);
void* zsys_dlopen(const char* filename, int flags); /* FIXME: we should add dlclose support eventually,
                                                       but that would require changing the ABI so that
                                                       globals are GC-allocated. Which is fine, we could
                                                       do that. */
void* zsys_dlsym(void* handle, const char* symbol);
int zsys_poll(void* pollfds, unsigned long nfds, int timeout);
int zsys_faccessat(int dirfd, const char* pathname, int mode, int flags);
int zsys_sigwait(const void* sigmask, int* sig);
int zsys_fsync(int fd);
int zsys_shutdown(int fd, int how);
int zsys_rmdir(const char* path);
int zsys_futimens(int fd, const void* times);
int zsys_utimensat(int dirfd, const char* path, const void* times, int flags);
int zsys_fchown(int fd, unsigned uid, unsigned gid);
int zsys_fchownat(int fd, const char* path, unsigned uid, unsigned gid, int flags);
int zsys_fchdir(int fd);
void zsys_sync(void);
int zsys_access(const char* path, int mode);
int zsys_symlink(const char* oldname, const char* newname);
int zsys_mprotect(void* addr, __SIZE_TYPE__ len, int prot);
int zsys_getgroups(int size, unsigned* list);
int zsys_getpgrp(void);
int zsys_getpgid(int pid);
int zsys_setpgid(int pid, int pgrp);
long zsys_pread(int fd, void* buf, __SIZE_TYPE__ nbytes, long offset);
long zsys_preadv(int fd, const void* iov, int iovcnt, long offset);
long zsys_pwrite(int fd, const void* buf, __SIZE_TYPE__ nbytes, long offset);
long zsys_pwritev(int fd, const void* iov, int iovcnt, long offset);
int zsys_getsid(int pid);
int zsys_mlock(const void* addr, __SIZE_TYPE__ len);
int zsys_munlock(const void* addr, __SIZE_TYPE__ len);
int zsys_mlockall(int flags);
int zsys_munlockall(void);
int zsys_sigpending(void* set);
int zsys_truncate(const char* path, long length);
int zsys_linkat(int fd1, const char* name1, int fd2, const char* name2, int flag);
int zsys_chmod(const char* pathname, unsigned mode);
int zsys_lchmod(const char* pathname, unsigned mode);
int zsys_fchmod(int fd, unsigned mode);
int zsys_mkfifo(const char* path, unsigned mode);
int zsys_mkdirat(int dirfd, const char* pathname, unsigned mode);
int zsys_mkdir(const char* path, unsigned mode);
int zsys_fchmodat(int fd, const char* path, unsigned mode, int flag);
int zsys_unlinkat(int dirfd, const char* path, int flags);
int zsys_acct(const char* file);
int zsys_setgroups(__SIZE_TYPE__ size, const unsigned* list);
int zsys_madvise(void* addr, __SIZE_TYPE__ len, int behav);
int zsys_mincore(void* addr, __SIZE_TYPE__ len, char* vec);
int zsys_getpriority(int which, int who);
int zsys_setpriority(int which, int who, int prio);
int zsys_gettimeofday(void* tp, void* tzp);
int zsys_settimeofday(const void* tp, const void* tzp);
int zsys_getrusage(int who, void* rusage);
int zsys_flock(int fd, int operation);
clock_t zsys_times(struct tms *buf);
int zsys_utimes(const char* path, const void* times);
int zsys_lutimes(const char* path, const void* times);
int zsys_adjtime(const void* delta, void* olddelta);
long zsys_pathconf(const char* path, int name);
long zsys_fpathconf(int fd, int name);
int zsys_setrlimit(int resource, const void* rlp);
int zsys_semget(int key, int nsems, int flag);
int zsys_semctl(int semid, int semnum, int cmd, ...);
int zsys_semop(int semid, void* array, __SIZE_TYPE__ nops);
int zsys_semtimedop(int semid, void* array, __SIZE_TYPE__ nops, const void* ts);
int zsys_shmget(int key, __SIZE_TYPE__ size, int flag);
int zsys_shmctl(int shmid, int cmd, void* buf);
void* zsys_shmat(int shmid, const void* addr, int flag);
int zsys_shmdt(const void* addr);
int zsys_msgget(int key, int msgflg);
int zsys_msgctl(int msgid, int cmd, void* buf);
long zsys_msgrcv(int msgid, void* msgp, __SIZE_TYPE__ msgsz, long msgtyp, int msgflg);
int zsys_msgsnd(int msgid, const void* msgp, __SIZE_TYPE__ msgsz, int msgflg);
int zsys_futimes(int fd, const void* times);
int zsys_futimesat(int fd, const char* path, const void* times);
int zsys_clock_settime(int clock_id, const void* tp);
int zsys_clock_getres(int clock_id, void* tp);
int zsys_getresgid(unsigned* rgid, unsigned* egid, unsigned* sgid);
int zsys_getresuid(unsigned* ruid, unsigned* euid, unsigned* suid);
int zsys_setresgid(unsigned rgid, unsigned egid, unsigned sgid);
int zsys_setresuid(unsigned ruid, unsigned euid, unsigned suid);
int zsys_sched_setparam(int pid, const void* param);
int zsys_sched_getparam(int pid, void* param);
int zsys_sched_setscheduler(int pid, int policy, const void* param);
int zsys_sched_getscheduler(int pid);
int zsys_sched_get_priority_max(int policy);
int zsys_sched_get_priority_min(int policy);
int zsys_sched_rr_get_interval(int pid, void* interval);
int zsys_eaccess(const char* path, int mode);
int zsys_fexecve(int fd, char*const* argv, char*const* envp);
int zsys_isatty(int fd);
int zsys_uname(void* buf);
int zsys_sendfile(int out_fd, int in_fd, long* offset, __SIZE_TYPE__ count);
void zsys_futex_wake(volatile int* addr, int cnt, int priv);
void zsys_futex_wait(volatile int* addr, int val, int priv);
/* These futex calls return the errno as a negative value. They do not set errno.
 
   NOTE: the futex_timedwait uses an absolute timeout, which is not what the real futex syscall
   uses! */
int zsys_futex_timedwait(volatile int* addr, int val, int clock_id, const void* absolute_timeout,
                         int priv);
int zsys_futex_unlock_pi(volatile int* addr, int priv);
int zsys_futex_lock_pi(volatile int* addr, int priv, const void* timeout);
void zsys_futex_requeue(volatile int* addr, int priv, int wake_count, int requeue_count,
                        volatile int* addr2);
int zsys_getdents(int fd, void* dirent, __SIZE_TYPE__ size);
long zsys_getrandom(void* buf, __SIZE_TYPE__ buflen, unsigned flags);
int zsys_epoll_create1_impl(int flags);
int zsys_epoll_ctl_impl(int epfd, int op, int fd, void* event);
int zsys_epoll_wait_impl(int epfd, void* events, int maxevents, int timeout);
int zsys_epoll_pwait_impl(int epfd, void* events, int maxevents, int timeout, const void* sigmask);
int zsys_epoll_create1(int flags);
int zsys_epoll_ctl(int epfd, int op, int fd, void* event);
int zsys_epoll_wait(int epfd, void* events, int maxevents, int timeout);
int zsys_epoll_pwait(int epfd, void* events, int maxevents, int timeout, const void* sigmask);
int zsys_sysinfo(void* info);
int zsys_sched_getaffinity(int tid, __SIZE_TYPE__ size, void* set);
int zsys_sched_setaffinity(int tid, __SIZE_TYPE__ size, const void* set);
int zsys_posix_fadvise(int fd, long base, long len, int advice);
int zsys_ppoll(void* fds, unsigned long nfds, const void* to, const void* mask);
int zsys_wait4(int pid, int* status, int options, void* ru);
int zsys_sigsuspend(const void* mask);
int zsys_prctl(int option, ...);
int zsys_eventfd(unsigned initval, int flags);
long zsys_listxattr(const char* path, char* list, __SIZE_TYPE__ size);
long zsys_llistxattr(const char* path, char* list, __SIZE_TYPE__ size);
long zsys_flistxattr(int fd, char* list, __SIZE_TYPE__ size);
int zsys_landlock_create_ruleset(const void* attr, __SIZE_TYPE__ size, unsigned flags);
int zsys_landlock_add_rule(int fd, int rule_type, const void* rule_attr, unsigned flags);
int zsys_landlock_restrict_self(int fd, unsigned flags);
int zsys_perf_event_open(void* attr, int pid, int cpu, int fd, unsigned long flags);
void* zsys_mremap(void* old_address, __SIZE_TYPE__ old_size, __SIZE_TYPE__ new_size, int flags,
                  void* new_address);
int zsys_signalfd(int fd, const void* mask, int flags);
int zsys_clock_nanosleep(int clockid, int flags, const void* request, void* remain);
int zsys_posix_fallocate(int fd, long offset, long len);
int zsys_sigaltstack(const void* ss, void* old_ss);
unsigned zsys_alarm(unsigned seconds);
int zsys_close_range_impl(unsigned first, unsigned last, int flags);
int zsys_dup3_impl(int oldfd, int newfd, int flags);
int zsys_close_range(unsigned first, unsigned last, int flags);
int zsys_dup3(int oldfd, int newfd, int flags);
int zsys_pipe2(int fds[2], int flags);
long zsys_readlinkat(int dirfd, const char* pathname, char* buf, __SIZE_TYPE__ bufsize);
int zsys_symlinkat(const char* target, int newdirfd, const char* linkpath);
long zsys_getxattr(const char* path, const char* name, void* value, __SIZE_TYPE__ size);
long zsys_lgetxattr(const char* path, const char* name, void* value, __SIZE_TYPE__ size);
long zsys_fgetxattr(int fd, const char* name, void* value, __SIZE_TYPE__ size);
int zsys_removexattr(const char* path, const char* name);
int zsys_lremovexattr(const char* path, const char* name);
int zsys_fremovexattr(int fd, const char* name);
int zsys_setxattr(const char* path, const char* name, const void* value, __SIZE_TYPE__ size,
                  int flags);
int zsys_lsetxattr(const char* path, const char* name, const void* value, __SIZE_TYPE__ size,
                   int flags);
int zsys_fsetxattr(int fd, const char* name, const void* value, __SIZE_TYPE__ size, int flags);
int zsys_getdomainname(char* name, __SIZE_TYPE__ len);
int zsys_setdomainname(const char* name, __SIZE_TYPE__ len);
int zsys_gethostname(char* name, __SIZE_TYPE__ len);
int zsys_sethostname(const char* name, __SIZE_TYPE__ len);
int zsys_remap_file_pages(void* addr, __SIZE_TYPE__ size, int prot, __SIZE_TYPE__ pgoff, int flags);
int zsys_swapon(const char* path, int flags);
int zsys_swapoff(const char* path);
int zsys_syncfs(int fd);
long zsys_syscall(long n, ...);
int zsys_vhangup(void);
int zsys_ioperm(unsigned long form, unsigned long num, int turn_on);
int zsys_iopl(int level);
int zsys_personality(unsigned long persona);
int zsys_setfsgid(unsigned fsgid);
int zsys_setfsuid(unsigned fsuid);
int zsys_arch_prctl(int code, void* addr);
int zsys_modify_ldt(int func, void* ptr, unsigned long bytecount);
int zsys_capset(void* header, void* data);
int zsys_capget(void* header, void* data);
int zsys_delete_module(const char* name, int flags);
int zsys_inotify_add_watch(int fd, const char* path, unsigned mask);
int zsys_fsconfig(int fd, unsigned cmd, const char* key, const void* data, int aux);
int zsys_fsmount(int fd, unsigned flags, unsigned ms_flags);
int zsys_fsopen(const char* name, unsigned flags);
int zsys_fspick(int fd, const char* path, unsigned flags);
int zsys_init_module(void* module_image, unsigned long len, const char* param_values);
int zsys_finit_module(int fd, const char* param_values, int flags);
int zsys_inotify_rm_watch(int fd, int wd);
int zsys_inotify_init(void);
int zsys_inotify_init1(int flags);
int zsys_syslog(int type, char* bufp, int len); /* this is really klogctl */
int zsys_mount(const char* source, const char* target, const char* fs_type, unsigned long flags,
               const void* data);
int zsys_mount_setattr(int fd, const char* path, unsigned flags, void* attr, __SIZE_TYPE__ size);
int zsys_move_mount(int from_fd, const char* from_path, int to_fd, const char* to_path,
                    unsigned flags);
int zsys_open_tree(int fd, const char* path, unsigned flags);
int zsys_pidfd_open(int pid, unsigned flags);
int zsys_pidfd_getfd(int pidfd, int targetfd, unsigned flags);
int zsys_pivot_root(const char* new_root, const char* put_old);
int zsys_pidfd_send_signal(int pidfd, int sig, void* info, unsigned flags);
long zsys_process_madvise(int pidfd, const void* iov, __SIZE_TYPE__ n, int advice, unsigned flags);
int zsys_process_mrelease(int pidfd, unsigned flags);
int zsys_fanotify_init(unsigned flags, unsigned event_flags);
int zsys_fanotify_mark(int fd, unsigned flags, unsigned long long mask, int dfd, const char* path);
int zsys_timerfd_create(int clockid, int flags);
int zsys_timerfd_settime(int fd, int flags, const void* new_value, void* old_value);
int zsys_timerfd_gettime(int fd, void* curr_value);
int zsys_quotactl(int cmd, const char* special, int id, void* addr);
int zsys_unshare(int flags);
int zsys_name_to_handle_at(int fd, const char* path, void* handle, int* mound_id, int flags);
int zsys_open_by_handle_at(int mount_fd, void* handle, int flags);
int zsys_pkey_alloc(unsigned flags, unsigned rights);
int zsys_pkey_free(int pkey);
int zsys_memfd_create(const char* name, unsigned flags);
int zsys_setns(int fd, int nstype);
int zsys_gettid(void);
int zsys_tkill(int tid, int sig);
int zsys_tgkill(int tgid, int tid, int sig);
void* zsys_create_module(const char* name, __SIZE_TYPE__ size);
int zsys_get_kernel_syms(void* table);
long zsys_nfsservctl(int cmd, void* argp, void* resp);
int zsys_query_module(const char* name, int which, void* buf, __SIZE_TYPE__ bufsize,
                      __SIZE_TYPE__* ret);
int zsys_uselib(const char* library);
int zsys_sigqueue(int pid, int sig, void* value);
int zsys_openat(int dirfd, const char* path, int flags, ...);
int zsys_statfs(const char* path, void* buf);
int zsys_fstatfs(int fd, void* buf);
int zsys_statvfs(const char* path, void* buf);
int zsys_fstatvfs(int fd, void* buf);
int zsys_renameat(int oldfd, const char* old_path, int newfd, const char* new_path);
int zsys_getcpu(unsigned* cpu, unsigned* node);
int zsys_sched_getcpu(void);
int zsys_msync(void* start, __SIZE_TYPE__ len, int flags);
int zsys_waitid(int idtype, unsigned id, void* infop, int options);
int zsys_sigtimedwait(const void* set, void* info, const void* timeout);
int zsys_fdatasync(int fd);
long zsys_copy_file_range(int fd_in, long* off_in, int fd_out, long* off_out, __SIZE_TYPE__ len,
                          unsigned flags);
int zsys_renameat2(int oldfd, const char* oldpath, int newfd, const char* newpath, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif /* PIZLONATED_SYSCALLS_H */
