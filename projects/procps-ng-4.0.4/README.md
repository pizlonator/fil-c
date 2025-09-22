[![build status](https://gitlab.com/ci/projects/2142/status.png?ref=master)](https://gitlab.com/ci/projects/2142?ref=master)
procps
======

procps is a set of command line and full-screen utilities that provide
information out of the pseudo-filesystem most commonly located at /proc.
This filesystem provides a simple interface to the kernel data structures.
The programs of procps generally concentrate on the structures that describe
the processess running on the system.

The following programs are found in procps:
* *free* - Report the amount of free and used memory in the system
* *kill* - Send a signal to a process based on PID
* *pgrep* - List processes based on name or other attributes
* *pkill* - Send a signal to a process based on name or other attributes
* *pmap* - Report memory map of a process
* *ps* - Report information of processes
* *pwdx* - Report current directory of a process
* *skill* - Obsolete version of pgrep/pkill
* *slabtop* - Display kernel slab cache information in real time
* *snice* - Renice a process
* *sysctl* - Read or Write kernel parameters at run-time
* *tload* - Graphical representation of system load average
* *top* - Dynamic real-time view of running processes
* *uptime* - Display how long the system has been running
* *vmstat* - Report virtual memory statistics
* *w* - Report logged in users and what they are doing
* *watch* - Execute a program periodically, showing output fullscreen

## Reporting Bugs
There are a few ways of reporting bugs or feature requests:

1. Your distribution's bug reporter. If you are using a distribution your first
port of call is their bug tracker. This is because each distribution has their
own patches and way of dealing with bugs. Also bug reporting often does not need
any subscription to websites.
2. GitLab Issues - To the left of this page is the issue tracker. You can report
bugs here.
3. Email list - We have an email list (see below) where you can report bugs.
The problem with this method is bug reports often get lost and cannot be
tracked. This is especially a big problem when its something that will take
time to resolve.

If you need to report bugs, there is more details on the
[Bug Reporting](https://gitlab.com/procps-ng/procps/blob/master/Documentation/bugs.md)
page.

## Email List
The email list for the developers and users of procps is found at
http://www.freelists.org/archive/procps/
This email list discusses the development of procps and is used by distributions
to also forward or discuss bugs.
