/* ipcs.c - provide information on ipc facilities
 *
 * Copyright 2014 Sandeep Sharma <sandeep.jack2756@gmail.com>
 *
 * see http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ipcs.html

USE_IPCS(NEWTOY(ipcs, "acptulsqmi#", TOYFLAG_USR|TOYFLAG_BIN))

config IPCS
  bool "ipcs"
  default n
  help
    usage: ipcs [[-smq] -i shmid] | [[-asmq] [-tcplu]]

    -i Show specific resource
    Resource specification:
    -a All (default)
    -m Shared memory segments
    -q Message queues
    -s Semaphore arrays
    Output format:
    -c Creator
    -l Limits
    -p Pid
    -t Time
    -u Summary
*/

#define FOR_ipcs
#include "toys.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

GLOBALS(
  int id;
)

//used many times, so good to paste it
#define flag(x) (toys.optflags & FLAG_ ## x)

union semun { //man says declare it yourself
  int              val;
  struct semid_ds *buf;
  unsigned short  *array;
  struct seminfo  *__buf;
};

static void show_msg_id(void)
{
  struct msqid_ds buf;
  int ret;

  if ((ret = msgctl(TT.id, IPC_STAT, &buf)) < 0) {
    perror_msg("msgctl");
    return;
  }

#define ipcperm buf.msg_perm

  printf("\nMessage Queue msqid=%d\n"
      "uid=%d\tgid=%d\tcuid=%d\tcgid=%d\tmode=%#o\n",
      TT.id, ipcperm.uid, ipcperm.gid, ipcperm.cuid, ipcperm.cgid,ipcperm.mode);
  printf ("cbytes=%ld\tqbytes=%ld\tqnum=%ld\tlspid=%d\tlrpid=%d\n",
      (long) buf.msg_cbytes, (long) buf.msg_qbytes,
      (long) buf.msg_qnum, buf.msg_lspid, buf.msg_lrpid);

  printf("send_time=%-26.24s\nrcv_time=%-26.24s\nchange_time=%-26.24s\n\n",
      buf.msg_stime ? ctime(&buf.msg_stime) : "Not set",
      buf.msg_rtime ? ctime(&buf.msg_rtime) : "Not set",
      buf.msg_ctime ? ctime(&buf.msg_ctime) : "Not set");
#undef ipcperm
}

static void show_sem_id(void)
{
  struct semid_ds buf;
  union semun n;
  int ret, i;

  n.buf = &buf;
  if ((ret = semctl(TT.id, 0, IPC_STAT, n)) < 0) {
    perror_msg("semctl");
    return;
  }

#define ipcperm buf.sem_perm
  printf("\nSemaphore Array semid=%d\n"
      "uid=%d\t gid=%d\t cuid=%d\t cgid=%d\n"
      "mode=%#o, access_perms=%#o\n"
      "nsems = %ld\n"
      "otime = %-26.24s\n",
      TT.id,
      ipcperm.uid, ipcperm.gid, ipcperm.cuid, ipcperm.cgid,
      ipcperm.mode, ipcperm.mode & 0777,
      (long) buf.sem_nsems,
      buf.sem_otime ? ctime(&buf.sem_otime) : "Not set");
  printf("ctime = %-26.24s\n"
      "%-10s %-10s %-10s %-10s %-10s\n",
      ctime(&buf.sem_ctime),
      "semnum", "value", "ncount", "zcount", "pid");
#undef ipcperm

  for (i = 0; i < buf.sem_nsems; i++) {
    int val, nc, zc, pid;
    val = semctl(TT.id, i, GETVAL, n);
    nc = semctl(TT.id, i, GETNCNT, n);
    zc = semctl(TT.id, i, GETZCNT, n);
    pid = semctl(TT.id, i, GETPID, n);
    if (val < 0 || nc < 0 || zc < 0 || pid < 0)
      perror_exit("semctl");
    printf("%-10d %-10d %-10d %-10d %-10d\n", i, val, nc, zc, pid);
  }
  xputc('\n');
}

static void show_shm_id(void)
{
  struct shmid_ds buf;
  int ret;

  if ((ret = shmctl(TT.id, IPC_STAT, &buf)) < 0) {
    perror_msg("shmctl");
    return;
  }

#define ipcperm buf.shm_perm

  printf("\nShared memory Segment shmid=%d\n"
      "uid=%d\tgid=%d\tcuid=%d\tcgid=%d\n"
      "mode=%#o\taccess_perms=%#o\n"
      "bytes=%ld\tlpid=%d\tcpid=%d\tnattch=%ld\n",
      TT.id,
      ipcperm.uid, ipcperm.gid, ipcperm.cuid, ipcperm.cgid,
      ipcperm.mode, (ipcperm.mode & 0777),
      (long) buf.shm_segsz, buf.shm_lpid, buf.shm_cpid,
      (long) buf.shm_nattch);
  printf("att_time=%-26.24s\n",
      buf.shm_atime ? ctime(&buf.shm_atime) : "Not set");
  printf("det_time=%-26.24s\n",
      buf.shm_dtime ? ctime(&buf.shm_dtime) : "Not set");
  printf("change_time=%-26.24s\n\n", ctime(&buf.shm_ctime));
#undef ipcperm
}

static void shm_array(void)
{
  struct shm_info shm_buf;
  struct shminfo ipc_buf;
  struct shmid_ds buf;
  int max_nr, i, shmid;
  struct passwd *pw;
  struct group *gr;

  if ((max_nr = shmctl(0, SHM_INFO, (struct shmid_ds*)&shm_buf)) < 0) {
    perror_msg("kernel not configured for shared memory");
    return;
  }

  if (flag(u)) {
    printf("------ Shared Memory Status --------\n");
    printf("segments allocated %d\n"
        "pages allocated %ld\n"
        "pages resident  %ld\n"
        "pages swapped   %ld\n"
        "Swap performance: %ld attempts\t%ld successes\n",
        shm_buf.used_ids,
        shm_buf.shm_tot,
        shm_buf.shm_rss,
        shm_buf.shm_swp,
        shm_buf.swap_attempts, shm_buf.swap_successes);
    return;
  }
  if (flag(l)) {
    if ((shmctl(0, 3, (struct shmid_ds*)&ipc_buf)) < 0) return; //IPC_INFO
    printf("------ Shared Memory Limits --------\n");
    printf("max number of segments = %lu\n"
        "max seg size (kbytes) = %lu\n"
        "max total shared memory (pages) = %lu\n"
        "min seg size (bytes) = %lu\n",
        (unsigned long) ipc_buf.shmmni,
        (unsigned long) (ipc_buf.shmmax >> 10),
        (unsigned long) ipc_buf.shmall,
        (unsigned long) ipc_buf.shmmin);
    return;
  }

  if (flag(t)) {
    printf("------ Shared Memory Attach/Detach/Change Times --------\n");
    printf("%-10s %-10s %-20s %-20s %-20s\n",
        "shmid", "owner", "attached", "detached", "changed");
  } else if (flag(p)) {
    printf("------ Shared Memory Creator/Last-op --------\n");
    printf("%-10s %-10s %-10s %-10s\n",
        "shmid", "owner", "cpid", "lpid");
  } else if (flag(c)) {
    printf("------ Shared Memory Segment Creators/Owners --------\n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s\n",
        "shmid", "perms", "cuid", "cgid", "uid", "gid");
  } else {
    printf("------ Shared Memory Segments --------\n");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s %-12s\n",
        "key", "shmid", "owner", "perms", "bytes", "nattch",
        "status");
  }

  for (i = 0; i <= max_nr; i++) {
    if ((shmid = shmctl(i, SHM_STAT, &buf)) < 0 ) continue;
    if (flag(t)) {
      if ((pw = getpwuid(buf.shm_perm.uid)))
        printf("%-10d %-10.10s", shmid, pw->pw_name);
      else printf("%-10d %-10.10d", shmid, buf.shm_perm.uid);
      printf(" %-20.16s", buf.shm_atime
          ? ctime(&buf.shm_atime) + 4 : "Not set");
      printf(" %-20.16s", buf.shm_dtime
          ? ctime(&buf.shm_dtime) + 4 : "Not set");
      printf(" %-20.16s\n", buf.shm_ctime
          ? ctime(&buf.shm_ctime) + 4 : "Not set");
    } else if (flag(p)) {
      if ((pw = getpwuid(buf.shm_perm.uid)))
        printf("%-10d %-10.10s", shmid, pw->pw_name);
      else printf("%-10d %-10.10d", shmid, buf.shm_perm.uid);
      printf(" %-10d %-10d\n", buf.shm_cpid, buf.shm_lpid);
    } else if (flag(c)) {
      printf("%-10d %-10o", shmid, buf.shm_perm.mode & 0777);
      if ((pw = getpwuid(buf.shm_perm.cuid))) printf(" %-10s", pw->pw_name);
      else printf(" %-10d", buf.shm_perm.cuid);
      if ((gr = getgrgid(buf.shm_perm.cgid))) printf(" %-10s", gr->gr_name);
      else printf(" %-10d", buf.shm_perm.cgid);
      if ((pw = getpwuid(buf.shm_perm.uid))) printf(" %-10s", pw->pw_name);
      else printf(" %-10d", buf.shm_perm.uid);
      if ((gr = getgrgid(buf.shm_perm.gid))) printf(" %-10s\n", gr->gr_name);
      else printf(" %-10d\n", buf.shm_perm.gid);
    } else {
      printf("0x%08x ", buf.shm_perm.__key);
      if ((pw = getpwuid(buf.shm_perm.uid)))
        printf("%-10d %-10.10s", shmid, pw->pw_name);
      else printf("%-10d %-10.10d", shmid, buf.shm_perm.uid);
      printf(" %-10o %-10lu %-10ld %-6s %-6s\n", buf.shm_perm.mode & 0777,
          (unsigned long) buf.shm_segsz,
          (long) buf.shm_nattch,
          buf.shm_perm.mode & SHM_DEST ? "dest" : " ",
          buf.shm_perm.mode & SHM_LOCKED ? "locked" : " ");
    }
  }
}

static void sem_array(void)
{
  struct seminfo info_buf;
  struct semid_ds buf;
  union semun u;
  int max_nr, i,semid;
  struct passwd *pw;
  struct group *gr;

  u.array = (unsigned short *)&info_buf;
  if ((max_nr = semctl(0, 0, SEM_INFO, u)) < 0) {
    perror_msg("kernel is not configured for semaphores");
    return;
  }


  if (flag(u)) {
    printf("------ Semaphore Status --------\n");
    printf("used arrays = %d\n"
        "allocated semaphores = %d\n",
        info_buf.semusz, info_buf.semaem);
    return;
  }
  if (flag(l)) {
    printf("------ Semaphore Limits --------\n");
    u.array = (unsigned short *)&info_buf;
    if ((semctl(0, 0, 3, u)) < 0) //IPC_INFO
      return;
    printf("max number of arrays = %d\n"
        "max semaphores per array = %d\n"
        "max semaphores system wide = %d\n"
        "max ops per semop call = %d\n"
        "semaphore max value = %d\n",
        info_buf.semmni,
        info_buf.semmsl,
        info_buf.semmns, info_buf.semopm, info_buf.semvmx);
    return;
  }

  if (flag(t)) {
    printf("------ Semaphore Operation/Change Times --------\n");
    printf("%-8s %-10s %-26.24s %-26.24s\n",
        "shmid", "owner", "last-op", "last-changed");
  } else if (flag(c)) {
    printf("------ Semaphore %s --------\n", "Arrays Creators/Owners");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s\n",
        "semid", "perms", "cuid", "cgid", "uid", "gid");

  } else if (flag(p)){
    return;
  } else {
    printf("------ Semaphore %s --------\n", "Arrays");
    printf("%-10s %-10s %-10s %-10s %-10s\n",
        "key", "semid", "owner", "perms", "nsems");
  }

  for (i = 0; i <= max_nr; i++) {
    u.buf = &buf;
    if ((semid = semctl(i, 0, SEM_STAT, u)) < 0) continue;
    pw = getpwuid(buf.sem_perm.uid);
    if (flag(t)) {
      if (pw) printf("%-8d %-10.10s", semid, pw->pw_name);
      else printf("%-8d %-10d", semid, buf.sem_perm.uid);

      printf("  %-26.24s", buf.sem_otime
          ? ctime(&buf.sem_otime) : "Not set");
      printf(" %-26.24s\n", buf.sem_ctime
          ? ctime(&buf.sem_ctime) : "Not set");
    } else if (flag(c)) {
      printf("%-10d %-10o", semid, buf.sem_perm.mode & 0777);
      if ((pw = getpwuid(buf.sem_perm.cuid))) printf(" %-10s", pw->pw_name);
      else printf(" %-10d", buf.sem_perm.cuid);
      if ((gr = getgrgid(buf.sem_perm.cgid))) printf(" %-10s", gr->gr_name);
      else printf(" %-10d", buf.sem_perm.cgid);
      if ((pw = getpwuid(buf.sem_perm.uid))) printf(" %-10s", pw->pw_name);
      else printf(" %-10d", buf.sem_perm.uid);
      if ((gr = getgrgid(buf.sem_perm.gid))) printf(" %-10s\n", gr->gr_name);
      else printf(" %-10d\n", buf.sem_perm.gid);
    } else {
      printf("0x%08x ", buf.sem_perm.__key);
      if (pw) printf("%-10d %-10.9s", semid, pw->pw_name);
      else printf("%-10d %-9d", semid, buf.sem_perm.uid);
      printf(" %-10o %-10ld\n", buf.sem_perm.mode & 0777,
          (long) buf.sem_nsems);
    }
  }
}

static void msg_array(void)
{
  struct msginfo info_buf;
  struct msqid_ds buf;
  int max_nr, i, msqid;
  struct passwd *pw;
  struct group *gr;

  if ((max_nr = msgctl(0, MSG_INFO, (struct msqid_ds*)&info_buf)) < 0) {
    perror_msg("kernel not configured for message queue");
    return;
  }

  if (flag(u)) {
    printf("------ Message%s --------\n", "s: Status");
    printf("allocated queues = %d\n"
        "used headers = %d\n"
        "used space = %d bytes\n",
        info_buf.msgpool, info_buf.msgmap, info_buf.msgtql);
    return;
  }
  if (flag(l)) {
    if ((msgctl(0, 3, (struct msqid_ds*)&info_buf)) < 0) return; //IPC_INFO
    printf("------ Messages: Limits --------\n");
    printf("max queues system wide = %d\n"
        "max size of message (bytes) = %d\n"
        "default max size of queue (bytes) = %d\n",
        info_buf.msgmni, info_buf.msgmax, info_buf.msgmnb);
    return;
  }

  if (flag(t)) {
    printf("------ Message%s --------\n", " Queues Send/Recv/Change Times");
    printf("%-8s %-10s %-20s %-20s %-20s\n",
        "msqid", "owner", "send", "recv", "change");
  } else if (flag(p)) {
    printf("------ Message%s --------\n", " Queues PIDs");
    printf("%-10s %-10s %-10s %-10s\n",
        "msqid", "owner", "lspid", "lrpid");
  } else if (flag(c)) {
    printf("------ Message%s --------\n", " Queues: Creators/Owners");
    printf("%-10s %-10s %-10s %-10s %-10s %-10s\n",
        "msqid", "perms", "cuid", "cgid", "uid", "gid");
  } else {
    printf("------ Message%s --------\n", " Queues");
    printf("%-10s %-10s %-10s %-10s %-12s %-12s\n",
        "key", "msqid", "owner", "perms", "used-bytes", "messages");
  }

  for (i = 0; i <= max_nr; i++) {
    if ((msqid = msgctl(i, MSG_STAT, &buf)) < 0 ) continue;
    pw = getpwuid(buf.msg_perm.uid);
    if (flag(t)) {
      if (pw) printf("%-8d %-10.10s", msqid, pw->pw_name);
      else printf("%-8d %-10d", msqid, buf.msg_perm.uid);
      printf(" %-20.16s", buf.msg_stime
          ? ctime(&buf.msg_stime) + 4 : "Not set");
      printf(" %-20.16s", buf.msg_rtime
          ? ctime(&buf.msg_rtime) + 4 : "Not set");
      printf(" %-20.16s\n", buf.msg_ctime
          ? ctime(&buf.msg_ctime) + 4 : "Not set");
    } else if (flag(p)) {
      if (pw) printf("%-8d %-10.10s", msqid, pw->pw_name);
      else printf("%-8d %-10d", msqid, buf.msg_perm.uid);
      printf("  %5d     %5d\n", buf.msg_lspid, buf.msg_lrpid);
    } else if (flag(c)) {
      printf("%-10d %-10o", msqid, buf.msg_perm.mode & 0777);
      if ((pw = getpwuid(buf.msg_perm.cuid))) printf(" %-10s", pw->pw_name);
      else printf(" %-10d", buf.msg_perm.cuid);
      if ((gr = getgrgid(buf.msg_perm.cgid))) printf(" %-10s", gr->gr_name);
      else printf(" %-10d", buf.msg_perm.cgid);
      if ((pw = getpwuid(buf.msg_perm.uid))) printf(" %-10s", pw->pw_name);
      else printf(" %-10d", buf.msg_perm.uid);
      if ((gr = getgrgid(buf.msg_perm.gid))) printf(" %-10s\n", gr->gr_name);
      else printf(" %-10d\n", buf.msg_perm.gid);
    } else {
      printf("0x%08x ", buf.msg_perm.__key);
      if (pw) printf("%-10d %-10.10s", msqid, pw->pw_name);
      else printf("%-10d %-10d", msqid, buf.msg_perm.uid);
      printf(" %-10o %-12ld %-12ld\n", buf.msg_perm.mode & 0777,
          (long) buf.msg_cbytes, (long) buf.msg_qnum);
    }
  }
}

void ipcs_main(void)
{
  if (flag(i)) {
    if (flag(m)) show_shm_id();
    else if (flag(s)) show_sem_id();
    else if (flag(q)) show_msg_id();
    else help_exit(0);
    return;
  }

  if (!(flag(m) || flag(s) || flag(q)) || flag(a)) toys.optflags |= (FLAG_m|FLAG_s|FLAG_q);

  xputc('\n');
  if (flag(m)) {
    shm_array();
    xputc('\n');
  }
  if (flag(s)) {
    sem_array();
    xputc('\n');
  }
  if (flag(q)) {
    msg_array();
    xputc('\n');
  }
}
