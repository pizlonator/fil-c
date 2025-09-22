#include <stdio.h>

#include "./capshdoc.h"

/*
 * A line by line explanation of each named capability value
 */
static const char *explanation0[] = {  /* cap_chown = 0 */
    "Allows a process to arbitrarily change the user and",
    "group ownership of a file.",
    NULL
};
static const char *explanation1[] = {  /* cap_dac_override = 1 */
    "Allows a process to override of all Discretionary",
    "Access Control (DAC) access, including ACL execute",
    "access. That is read, write or execute files that the",
    "process would otherwise not have access to. This",
    "excludes DAC access covered by CAP_LINUX_IMMUTABLE.",
    NULL
};
static const char *explanation2[] = {  /* cap_dac_read_search = 2 */
    "Allows a process to override all DAC restrictions",
    "limiting the read and search of files and",
    "directories. This excludes DAC access covered by",
    "CAP_LINUX_IMMUTABLE.",
    NULL
};
static const char *explanation3[] = {  /* cap_fowner = 3 */
    "Allows a process to perform operations on files, even",
    "where file owner ID should otherwise need be equal to",
    "the UID, except where CAP_FSETID is applicable. It",
    "doesn't override MAC and DAC restrictions.",
    "",
    "This capability permits the deletion of a file owned",
    "by another UID in a directory protected by the sticky",
    "(t) bit.",
    NULL
};
static const char *explanation4[] = {  /* cap_fsetid = 4 */
    "Allows a process to set the S_ISUID and S_ISUID bits of",
    "the file permissions, even when the process' effective",
    "UID or GID/supplementary GIDs do not match that of the",
    "file.",
    NULL
};
static const char *explanation5[] = {  /* cap_kill = 5 */
    "Allows a process to send a kill(2) signal to any other",
    "process - overriding the limitation that there be a",
    "[E]UID match between source and target process.",
    NULL
};
static const char *explanation6[] = {  /* cap_setgid = 6 */
    "Allows a process to freely manipulate its own GIDs:",
    "  - arbitrarily set the GID, EGID, REGID, RESGID values",
    "  - arbitrarily set the supplementary GIDs",
    "  - allows the forging of GID credentials passed over a",
    "    socket",
    NULL
};
static const char *explanation7[] = {  /* cap_setuid = 7 */
    "Allows a process to freely manipulate its own UIDs:",
    "  - arbitrarily set the UID, EUID, REUID and RESUID",
    "    values",
    "  - allows the forging of UID credentials passed over a",
    "    socket",
    NULL
};
static const char *explanation8[] = {  /* cap_setpcap = 8 */
    "Allows a process to freely manipulate its inheritable",
    "capabilities.",
    "",
    "Linux supports the POSIX.1e Inheritable set, the POXIX.1e (X",
    "vector) known in Linux as the Bounding vector, as well as",
    "the Linux extension Ambient vector.",
    "",
    "This capability permits dropping bits from the Bounding",
    "vector (ie. raising B bits in the libcap IAB",
    "representation). It also permits the process to raise",
    "Ambient vector bits that are both raised in the Permitted",
    "and Inheritable sets of the process. This capability cannot",
    "be used to raise Permitted bits, Effective bits beyond those",
    "already present in the process' permitted set, or",
    "Inheritable bits beyond those present in the Bounding",
    "vector.",
    "",
    "[Historical note: prior to the advent of file capabilities",
    "(2008), this capability was suppressed by default, as its",
    "unsuppressed behavior was not auditable: it could",
    "asynchronously grant its own Permitted capabilities to and",
    "remove capabilities from other processes arbitrarily. The",
    "former leads to undefined behavior, and the latter is better",
    "served by the kill system call.]",
    NULL
};
static const char *explanation9[] = {  /* cap_linux_immutable = 9 */
    "Allows a process to modify the S_IMMUTABLE and",
    "S_APPEND file attributes.",
    NULL
};
static const char *explanation10[] = {  /* cap_net_bind_service = 10 */
    "Allows a process to bind to privileged ports:",
    "  - TCP/UDP sockets below 1024",
    "  - ATM VCIs below 32",
    NULL
};
static const char *explanation11[] = {  /* cap_net_broadcast = 11 */
    "Allows a process to broadcast to the network and to",
    "listen to multicast.",
    NULL
};
static const char *explanation12[] = {  /* cap_net_admin = 12 */
    "Allows a process to perform network configuration",
    "operations:",
    "  - interface configuration",
    "  - administration of IP firewall, masquerading and",
    "    accounting",
    "  - setting debug options on sockets",
    "  - modification of routing tables",
    "  - setting arbitrary process, and process group",
    "    ownership on sockets",
    "  - binding to any address for transparent proxying",
    "    (this is also allowed via CAP_NET_RAW)",
    "  - setting TOS (Type of service)",
    "  - setting promiscuous mode",
    "  - clearing driver statistics",
    "  - multicasing",
    "  - read/write of device-specific registers",
    "  - activation of ATM control sockets",
    NULL
};
static const char *explanation13[] = {  /* cap_net_raw = 13 */
    "Allows a process to use raw networking:",
    "  - RAW sockets",
    "  - PACKET sockets",
    "  - binding to any address for transparent proxying",
    "    (also permitted via CAP_NET_ADMIN)",
    NULL
};
static const char *explanation14[] = {  /* cap_ipc_lock = 14 */
    "Allows a process to lock shared memory segments for IPC",
    "purposes.  Also enables mlock and mlockall system",
    "calls.",
    NULL
};
static const char *explanation15[] = {  /* cap_ipc_owner = 15 */
    "Allows a process to override IPC ownership checks.",
    NULL
};
static const char *explanation16[] = {  /* cap_sys_module = 16 */
    "Allows a process to initiate the loading and unloading",
    "of kernel modules. This capability can effectively",
    "modify kernel without limit.",
    NULL
};
static const char *explanation17[] = {  /* cap_sys_rawio = 17 */
    "Allows a process to perform raw IO:",
    "  - permit ioper/iopl access",
    "  - permit sending USB messages to any device via",
    "    /dev/bus/usb",
    NULL
};
static const char *explanation18[] = {  /* cap_sys_chroot = 18 */
    "Allows a process to perform a chroot syscall to change",
    "the effective root of the process' file system:",
    "redirect to directory \"/\" to some other location.",
    NULL
};
static const char *explanation19[] = {  /* cap_sys_ptrace = 19 */
    "Allows a process to perform a ptrace() of any other",
    "process.",
    NULL
};
static const char *explanation20[] = {  /* cap_sys_pacct = 20 */
    "Allows a process to configure process accounting.",
    NULL
};
static const char *explanation21[] = {  /* cap_sys_admin = 21 */
    "Allows a process to perform a somewhat arbitrary",
    "grab-bag of privileged operations. Over time, this",
    "capability should weaken as specific capabilities are",
    "created for subsets of CAP_SYS_ADMINs functionality:",
    "  - configuration of the secure attention key",
    "  - administration of the random device",
    "  - examination and configuration of disk quotas",
    "  - setting the domainname",
    "  - setting the hostname",
    "  - calling bdflush()",
    "  - mount() and umount(), setting up new SMB connection",
    "  - some autofs root ioctls",
    "  - nfsservctl",
    "  - VM86_REQUEST_IRQ",
    "  - to read/write pci config on alpha",
    "  - irix_prctl on mips (setstacksize)",
    "  - flushing all cache on m68k (sys_cacheflush)",
    "  - removing semaphores",
    "  - Used instead of CAP_CHOWN to \"chown\" IPC message",
    "    queues, semaphores and shared memory",
    "  - locking/unlocking of shared memory segment",
    "  - turning swap on/off",
    "  - forged pids on socket credentials passing",
    "  - setting readahead and flushing buffers on block",
    "    devices",
    "  - setting geometry in floppy driver",
    "  - turning DMA on/off in xd driver",
    "  - administration of md devices (mostly the above, but",
    "    some extra ioctls)",
    "  - tuning the ide driver",
    "  - access to the nvram device",
    "  - administration of apm_bios, serial and bttv (TV)",
    "    device",
    "  - manufacturer commands in isdn CAPI support driver",
    "  - reading non-standardized portions of PCI",
    "    configuration space",
    "  - DDI debug ioctl on sbpcd driver",
    "  - setting up serial ports",
    "  - sending raw qic-117 commands",
    "  - enabling/disabling tagged queuing on SCSI",
    "    controllers and sending arbitrary SCSI commands",
    "  - setting encryption key on loopback filesystem",
    "  - setting zone reclaim policy",
    NULL
};
static const char *explanation22[] = {  /* cap_sys_boot = 22 */
    "Allows a process to initiate a reboot of the system.",
    NULL
};
static const char *explanation23[] = {  /* cap_sys_nice = 23 */
    "Allows a process to maipulate the execution priorities",
    "of arbitrary processes:",
    "  - those involving different UIDs",
    "  - setting their CPU affinity",
    "  - alter the FIFO vs. round-robin (realtime)",
    "    scheduling for itself and other processes.",
    NULL
};
static const char *explanation24[] = {  /* cap_sys_resource = 24 */
    "Allows a process to adjust resource related parameters",
    "of processes and the system:",
    "  - set and override resource limits",
    "  - override quota limits",
    "  - override the reserved space on ext2 filesystem",
    "    (this can also be achieved via CAP_FSETID)",
    "  - modify the data journaling mode on ext3 filesystem,",
    "    which uses journaling resources",
    "  - override size restrictions on IPC message queues",
    "  - configure more than 64Hz interrupts from the",
    "    real-time clock",
    "  - override the maximum number of consoles for console",
    "    allocation",
    "  - override the maximum number of keymaps",
    NULL
};
static const char *explanation25[] = {  /* cap_sys_time = 25 */
    "Allows a process to perform time manipulation of clocks:",
    "  - alter the system clock",
    "  - enable irix_stime on MIPS",
    "  - set the real-time clock",
    NULL
};
static const char *explanation26[] = {  /* cap_sys_tty_config = 26 */
    "Allows a process to manipulate tty devices:",
    "  - configure tty devices",
    "  - perform vhangup() of a tty",
    NULL
};
static const char *explanation27[] = {  /* cap_mknod = 27 */
    "Allows a process to perform privileged operations with",
    "the mknod() system call.",
    NULL
};
static const char *explanation28[] = {  /* cap_lease = 28 */
    "Allows a process to take leases on files.",
    NULL
};
static const char *explanation29[] = {  /* cap_audit_write = 29 */
    "Allows a process to write to the audit log via a",
    "unicast netlink socket.",
    NULL
};
static const char *explanation30[] = {  /* cap_audit_control = 30 */
    "Allows a process to configure audit logging via a",
    "unicast netlink socket.",
    NULL
};
static const char *explanation31[] = {  /* cap_setfcap = 31 */
    "Allows a process to set capabilities on files.",
    "Permits a process to uid_map the uid=0 of the",
    "parent user namespace into that of the child",
    "namespace. Also, permits a process to override",
    "securebits locks through user namespace",
    "creation.",
    NULL
};
static const char *explanation32[] = {  /* cap_mac_override = 32 */
    "Allows a process to override Manditory Access Control",
    "(MAC) access. Not all kernels are configured with a MAC",
    "mechanism, but this is the capability reserved for",
    "overriding them.",
    NULL
};
static const char *explanation33[] = {  /* cap_mac_admin = 33 */
    "Allows a process to configure the Mandatory Access",
    "Control (MAC) policy. Not all kernels are configured",
    "with a MAC enabled, but if they are this capability is",
    "reserved for code to perform administration tasks.",
    NULL
};
static const char *explanation34[] = {  /* cap_syslog = 34 */
    "Allows a process to configure the kernel's syslog",
    "(printk) behavior.",
    NULL
};
static const char *explanation35[] = {  /* cap_wake_alarm = 35 */
    "Allows a process to trigger something that can wake the",
    "system up.",
    NULL
};
static const char *explanation36[] = {  /* cap_block_suspend = 36 */
    "Allows a process to block system suspends - prevent the",
    "system from entering a lower power state.",
    NULL
};
static const char *explanation37[] = {  /* cap_audit_read = 37 */
    "Allows a process to read the audit log via a multicast",
    "netlink socket.",
    NULL
};
static const char *explanation38[] = {  /* cap_perfmon = 38 */
    "Allows a process to enable observability of privileged",
    "operations related to performance. The mechanisms",
    "include perf_events, i915_perf and other kernel",
    "subsystems.",
    NULL
};
static const char *explanation39[] = {  /* cap_bpf = 39 */
    "Allows a process to manipulate aspects of the kernel",
    "enhanced Berkeley Packet Filter (BPF) system. This is",
    "an execution subsystem of the kernel, that manages BPF",
    "programs. CAP_BPF permits a process to:",
    "  - create all types of BPF maps",
    "  - advanced verifier features:",
    "    - indirect variable access",
    "    - bounded loops",
    "    - BPF to BPF function calls",
    "    - scalar precision tracking",
    "    - larger complexity limits",
    "    - dead code elimination",
    "    - potentially other features",
    "",
    "Other capabilities can be used together with CAP_BFP to",
    "further manipulate the BPF system:",
    "  - CAP_PERFMON relaxes the verifier checks as follows:",
    "    - BPF programs can use pointer-to-integer",
    "      conversions",
    "    - speculation attack hardening measures can be",
    "      bypassed",
    "    - bpf_probe_read to read arbitrary kernel memory is",
    "      permitted",
    "    - bpf_trace_printk to print the content of kernel",
    "      memory",
    "  - CAP_SYS_ADMIN permits the following:",
    "    - use of bpf_probe_write_user",
    "    - iteration over the system-wide loaded programs,",
    "      maps, links BTFs and convert their IDs to file",
    "      descriptors.",
    "  - CAP_PERFMON is required to load tracing programs.",
    "  - CAP_NET_ADMIN is required to load networking",
    "    programs.",
    NULL
};
static const char *explanation40[] = {  /* cap_checkpoint_restore = 40 */
    "Allows a process to perform checkpoint",
    "and restore operations. Also permits",
    "explicit PID control via clone3() and",
    "also writing to ns_last_pid.",
    NULL
};
const char **explanations[] = {
    explanation0,
    explanation1,
    explanation2,
    explanation3,
    explanation4,
    explanation5,
    explanation6,
    explanation7,
    explanation8,
    explanation9,
    explanation10,
    explanation11,
    explanation12,
    explanation13,
    explanation14,
    explanation15,
    explanation16,
    explanation17,
    explanation18,
    explanation19,
    explanation20,
    explanation21,
    explanation22,
    explanation23,
    explanation24,
    explanation25,
    explanation26,
    explanation27,
    explanation28,
    explanation29,
    explanation30,
    explanation31,
    explanation32,
    explanation33,
    explanation34,
    explanation35,
    explanation36,
    explanation37,
    explanation38,
    explanation39,
    explanation40,
};

const int capsh_doc_limit = 41;
