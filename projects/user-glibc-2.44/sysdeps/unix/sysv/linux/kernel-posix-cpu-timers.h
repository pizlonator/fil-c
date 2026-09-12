/*
  Parameters for the Linux kernel ABI for CPU clocks, the bit fields within
  a clockid:

  - The most significant 29 bits hold either a pid or a file descriptor.
  - Bit 2 indicates whether a cpu clock refers to a thread or a process.
  - Bits 1 and 0 give the type: PROF=0, VIRT=1, SCHED=2, or FD=3.
  - A clockid is invalid if bits 2, 1, and 0 are all set.
 */

#define CPUCLOCK_PID(clock)		((pid_t) ~((clock) >> 3))
#define CPUCLOCK_PERTHREAD(clock) \
	(((clock) & (clockid_t) CPUCLOCK_PERTHREAD_MASK) != 0)
#define CPUCLOCK_PID_MASK	7
#define CPUCLOCK_PERTHREAD_MASK	4
#define CPUCLOCK_WHICH(clock)	((clock) & (clockid_t) CPUCLOCK_CLOCK_MASK)
#define CPUCLOCK_CLOCK_MASK	3
#define CPUCLOCK_PROF		0
#define CPUCLOCK_VIRT		1
#define CPUCLOCK_SCHED		2
#define CPUCLOCK_MAX		3

static inline clockid_t
make_process_cpuclock (unsigned int pid, clockid_t clock)
{
  return ((~pid) << 3) | clock;
}

static inline clockid_t
make_thread_cpuclock (unsigned int tid, clockid_t clock)
{
  return make_process_cpuclock (tid, clock | CPUCLOCK_PERTHREAD_MASK);
}

#define PROCESS_CLOCK  make_process_cpuclock (0, CPUCLOCK_SCHED)
#define THREAD_CLOCK   make_thread_cpuclock (0, CPUCLOCK_SCHED)
