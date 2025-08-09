/* ipcrm.c - remove msg que, sem or shared memory
 *
 * Copyright 2014 Ashwini Kumar <ak.ashwini1981@gmail.com>
 *
 * See http://pubs.opengroup.org/onlinepubs/9699919799/utilities/ipcrm.html


USE_IPCRM(NEWTOY(ipcrm, "m*M*s*S*q*Q*", TOYFLAG_USR|TOYFLAG_BIN))

config IPCRM
  bool "ipcrm"
  default n
  help
    usage: ipcrm [ [-q msqid] [-m shmid] [-s semid]
              [-Q msgkey] [-M shmkey] [-S semkey] ... ]

    -mM Remove memory segment after last detach
    -qQ Remove message queue
    -sS Remove semaphore
*/

#define FOR_ipcrm
#include "toys.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

GLOBALS(
  struct arg_list *qkey;
  struct arg_list *qid;
  struct arg_list *skey;
  struct arg_list *sid;
  struct arg_list *mkey;
  struct arg_list *mid;
)

static void do_ipcrm(int key, int ipc, char *name)
{
  char *c;
  int id, ret = 0;

  id = strtol(name, &c, 0);
  if (*c) {
    error_msg("invalid number :%s", name);
    return;
  }

  if (key) {
    if (id == IPC_PRIVATE) {
      error_msg("illegal key (%s)", name);
      return;
    }
    id = ((ipc == 1)?shmget(id, 0, 0) :
         (ipc == 2)? msgget(id, 0): semget(id, 0, 0));
    if (id < 0) {
      perror_msg("key (%s)", name);
      return;
    }
  }

  if (ipc == 1) ret = shmctl(id, IPC_RMID, NULL);
  else if (ipc == 2) ret = msgctl(id, IPC_RMID, NULL);
  else if (ipc == 3) ret = semctl(id, 0, IPC_RMID, NULL);

  if (ret < 0) perror_msg("%s (%s)", ((key)? "key": "id"), name);
}

void ipcrm_main(void)
{
  ++toys.argv;
  if (toys.optc && (!strcmp(*toys.argv, "shm") ||
        !strcmp(*toys.argv, "sem") || !strcmp(*toys.argv, "msg"))) {
    int t = (toys.argv[0][1] == 'h')? 1 : (toys.argv[0][1] == 's')? 2:3;

    while (*(++toys.argv)) do_ipcrm(0, t, *toys.argv);
  } else {
    struct arg_list *tmp;

    for (tmp = TT.mkey; tmp; tmp = tmp->next) do_ipcrm(1, 1, tmp->arg);
    for (tmp = TT.mid; tmp; tmp = tmp->next) do_ipcrm(0, 1, tmp->arg);
    for (tmp = TT.qkey; tmp; tmp = tmp->next) do_ipcrm(1, 2, tmp->arg);
    for (tmp = TT.qid; tmp; tmp = tmp->next) do_ipcrm(0, 2, tmp->arg);
    for (tmp = TT.skey; tmp; tmp = tmp->next) do_ipcrm(1, 3, tmp->arg);
    for (tmp = TT.sid; tmp; tmp = tmp->next) do_ipcrm(0, 3, tmp->arg);
    if (toys.optc) help_exit("unknown argument: %s", *toys.optargs);
  }
}
