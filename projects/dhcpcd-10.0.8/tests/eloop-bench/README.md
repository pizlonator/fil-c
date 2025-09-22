# eloop-bench

eloop is a portable event loop designed to be dropped into the code of a
program. It is not in any library to date.
The basic requirement of eloop is a descriptor polling mechanism which
allows the safe delivery of signals.
As such, select(2) and poll(2) are not suitable.

This is an eloop benchmark to test the performance of the various
polling mechanisms. It's inspired by libevent/bench.

eloop needs to be compiled for a specific polling mechanism.
eloop will try and work out which one to use, but you can influence which one
by giving one of these CPPFLAGS to the Makefile:
  *  `HAVE_KQUEUE`
  *  `HAVE_EPOLL`
  *  `HAVE_PSELECT`
  *  `HAVE_POLLTS`
  *  `HAVE_PPOLL`

kqueue(2) is found on modern BSD kernels.
epoll(7) is found on modern Linux and Solaris kernels.
These two *should* be the best performers.

pselect(2) *should* be found on any POSIX libc.
This *should* be the worst performer.

pollts(2) and ppoll(2) are NetBSD and Linux specific variants on poll(2),
but allow safe signal delivery like pselect(2).
Aside from the function name, the arguments and functionality are identical.
They are of little use as both platforms have kqueue(2) and epoll(2),
but there is an edge case where system doesn't have epoll(2) compiled hence
it's inclusion here.

## using eloop-bench

The benchmark runs by setting up npipes to read/write to and attaching
an eloop callback for each pipe reader.
Once setup, it will perform a run by writing to nactive pipes.
For each successful pipe read, if nwrites >0 then the reader will reduce
nwrites by one on successful write back to itself.
Once nwrites is 0, the timed run will end once the last write has been read.
At the end of run, the time taken in seconds and nanoseconds is printed.

The following arguments can influence the benchmark:
  *  `-a active`  
     The number of active pipes, default 1.
  *  `-n pipes`  
     The number of pipes to create and attach an eloop callback to, defalt 100.
  *  `-r runs`  
     The number of timed runs to make, default 25.
  *  `-w writes`  
     The number of writes to make by the read callback, default 100.
