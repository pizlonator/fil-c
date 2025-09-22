/*
 * Copyright (C) 2006 Hewlett-Packard Development Company, L.P.
 * Huschaam Hussain <Huschaam.Hussain@hp.com>
 *	TSG Solution Alliances Engineering
 *	SAP Technology Group
 *
 * Copyright (C) 2015 Karel Zak <kzak@redhat.com>
 *
 *
 * The test heavily uses shared memory, to enlarge maximal size of shared
 * segment use:
 *
 *	echo "4294967295" > /proc/sys/kernel/shmm
 *
 * The test is compiled against in-tree libuuid, if you want to test uuidd
 * installed to the system then make sure that libuuid uses the same socket
 * like the running uuidd. You can start the uuidd manually, for example:
 *
 *	uuidd --debug --no-fork --no-pid --socket /run/uuidd/request
 *
 * if the $runstatedir (as defined by build-system) is /run. If you want
 * to overwrite the built-in default then use:
 *
 *	make uuidd uuidgen runstatedir=/var/run
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "uuid.h"
#include "c.h"
#include "xalloc.h"
#include "strutils.h"
#include "nls.h"

#define LOG(level,args) if (loglev >= level) { fprintf args; }

static size_t nprocesses = 4;
static size_t nthreads = 4;
static size_t nobjects = 4096;
static size_t loglev = 1;

struct processentry {
	pid_t		pid;
	int		status;
};
typedef struct processentry process_t;

struct threadentry {
	process_t	*proc;
	pthread_t	tid;		/* pthread_self() / phtread_create() */
	pthread_attr_t	thread_attr;
	size_t		index;		/* index in object[] */
	int		retval;		/* pthread exit() */
};
typedef struct threadentry thread_t;

/* this is in shared memory, keep it as small as possible */
struct objectentry {
	uuid_t		uuid;
	pthread_t	tid;
	pid_t		pid;
	size_t		idx;
};
typedef struct objectentry object_t;

static int shmem_id;
static object_t *objects;


static void __attribute__((__noreturn__)) usage(void)
{
	printf("\n %s [options]\n", program_invocation_short_name);

	printf("  -p <num>     number of nprocesses (default:%zu)\n", nprocesses);
	printf("  -t <num>     number of nthreads (default:%zu)\n", nthreads);
	printf("  -o <num>     number of nobjects (default:%zu)\n", nobjects);
	printf("  -l <level>   log level (default:%zu)\n", loglev);
	printf("  -h           display help\n");

	exit(EXIT_SUCCESS);
}

static void allocate_segment(int *id, void **address, size_t number, size_t size)
{
	*id = shmget(IPC_PRIVATE, number * size, IPC_CREAT | 0600);
	if (*id == -1)
		err(EXIT_FAILURE, "shmget failed to create %zu bytes shared memory", number * size);

	*address = shmat(*id, NULL, 0);
	if (*address == (void *)-1)
		err(EXIT_FAILURE, "shmat failed");

	LOG(2, (stderr,
	     "allocate shared memory segment [id=%d,address=0x%p]\n",
	     *id, *address));

	memset(*address, 0, number * size);
}

static void remove_segment(int id, void *address)
{
	if (shmdt(address) == -1)
		err(EXIT_FAILURE, "shmdt failed");
	if (shmctl(id, IPC_RMID, NULL) == -1)
		err(EXIT_FAILURE, "shmctl failed");
	LOG(2,
	    (stderr,
	     "remove shared memory segment [id=%d,address=0x%p]\n",
	     id, address));
}

static void object_uuid_create(object_t * object)
{
	uuid_generate_time(object->uuid);
}

static void object_uuid_to_string(object_t * object, char **string_uuid)
{
	uuid_unparse(object->uuid, *string_uuid);
}

static int object_uuid_compare(const void *object1, const void *object2)
{
	uuid_t *uuid1 = &((object_t *) object1)->uuid,
	       *uuid2 = &((object_t *) object2)->uuid;

	return uuid_compare(*uuid1, *uuid2);
}

static void *create_uuids(thread_t *th)
{
	size_t i;

	for (i = th->index; i < th->index + nobjects; i++) {
		object_t *obj = &objects[i];

		object_uuid_create(obj);
		obj->tid = th->tid;
		obj->pid = th->proc->pid;
		obj->idx = th->index + i;
	}
	return NULL;
}

static void *thread_body(void *arg)
{
	thread_t *th = (thread_t *) arg;

	return create_uuids(th);
}

static void create_nthreads(process_t *proc, size_t index)
{
	thread_t *threads;
	size_t i, ncreated = 0;
	int rc;

	threads = xcalloc(nthreads, sizeof(thread_t));

	for (i = 0; i < nthreads; i++) {
		thread_t *th = &threads[i];

		rc = pthread_attr_init(&th->thread_attr);
		if (rc) {
			errno = rc;
			warn("%d: pthread_attr_init failed", proc->pid);
			break;
		}

		th->index = index;
		th->proc = proc;
		rc = pthread_create(&th->tid, &th->thread_attr, &thread_body, th);

		if (rc) {
			errno = rc;
			warn("%d: pthread_create failed", proc->pid);
			break;
		}

		LOG(2, (stderr, "%d: started thread [tid=%jd,index=%zu]\n",
		     proc->pid, (intmax_t) (intptr_t) th->tid, th->index));
		index += nobjects;
		ncreated++;
	}

	if (ncreated != nthreads)
		fprintf(stderr, "%d: %zu threads not created and ~%zu objects will be ignored\n",
				proc->pid, nthreads - ncreated,
				(nthreads - ncreated) * nobjects);

	for (i = 0; i < ncreated; i++) {
		thread_t *th = &threads[i];

		rc = pthread_join(th->tid, (void *) &th->retval);
		if (rc) {
			errno = rc;
			err(EXIT_FAILURE, "pthread_join failed");
		}

		LOG(2, (stderr, "%d: thread exited [tid=%jd,return=%d]\n",
		     proc->pid, (intmax_t) (intptr_t) th->tid, th->retval));
	}

	free(threads);
}

static void create_nprocesses(void)
{
	process_t *process;
	size_t i;

	process = xcalloc(nprocesses, sizeof(process_t));

	for (i = 0; i < nprocesses; i++) {
		process_t *proc = &process[i];

		proc->pid = fork();
		switch (proc->pid) {
		case -1: /* error */
			err(EXIT_FAILURE, "fork failed");
			break;
		case 0: /* child */
			proc->pid = getpid();
			create_nthreads(proc, i * nthreads * nobjects);
			exit(EXIT_SUCCESS);
			break;
		default: /* parent */
			LOG(2, (stderr, "started process [pid=%d]\n", proc->pid));
			break;
		}
	}

	for (i = 0; i < nprocesses; i++) {
		process_t *proc = &process[i];

		if (waitpid(proc->pid, &proc->status, 0) == (pid_t) - 1)
			err(EXIT_FAILURE, "waitpid failed");
		LOG(2,
		    (stderr, "process exited [pid=%d,status=%d]\n",
		     proc->pid, proc->status));
	}

	free(process);
}

static void object_dump(size_t idx, object_t *obj)
{
	char uuid_string[UUID_STR_LEN], *p;

	p = uuid_string;
	object_uuid_to_string(obj, &p);

	fprintf(stderr, "object[%zu]: {\n", idx);
	fprintf(stderr, "  uuid:    <%s>\n", p);
	fprintf(stderr, "  idx:     %zu\n", obj->idx);
	fprintf(stderr, "  process: %d\n", (int) obj->pid);
	fprintf(stderr, "  thread:  %jd\n", (intmax_t) (intptr_t) obj->tid);
	fprintf(stderr, "}\n");
}

#define MSG_TRY_HELP "Try '-h' for help."

int main(int argc, char *argv[])
{
	size_t i, nfailed = 0, nignored = 0;
	int c;

	while (((c = getopt(argc, argv, "p:t:o:l:h")) != -1)) {
		switch (c) {
		case 'p':
			nprocesses = strtou32_or_err(optarg, "invalid nprocesses number argument");
			break;
		case 't':
			nthreads = strtou32_or_err(optarg, "invalid nthreads number argument");
			break;
		case 'o':
			nobjects = strtou32_or_err(optarg, "invalid nobjects number argument");
			break;
		case 'l':
			loglev = strtou32_or_err(optarg, "invalid log level argument");
			break;
		case 'h':
			usage();
			break;
		default:
			fprintf(stderr, MSG_TRY_HELP);
			exit(EXIT_FAILURE);
		}
	}

	if (optind != argc)
		errx(EXIT_FAILURE, "bad usage\n" MSG_TRY_HELP);

	if (loglev == 1)
		fprintf(stderr, "requested: %zu processes, %zu threads, %zu objects per thread (%zu objects = %zu bytes)\n",
				nprocesses, nthreads, nobjects,
				nprocesses * nthreads * nobjects,
				nprocesses * nthreads * nobjects * sizeof(object_t));

	allocate_segment(&shmem_id, (void **)&objects,
			 nprocesses * nthreads * nobjects, sizeof(object_t));

	create_nprocesses();

	if (loglev >= 3) {
		for (i = 0; i < nprocesses * nthreads * nobjects; i++)
			object_dump(i, &objects[i]);
	}

	qsort(objects, nprocesses * nthreads * nobjects, sizeof(object_t),
	      object_uuid_compare);

	for (i = 0; i < nprocesses * nthreads * nobjects - 1; i++) {
		object_t *obj1 = &objects[i],
			 *obj2 = &objects[i + 1];

		if (!obj1->tid) {
			LOG(3, (stderr, "ignore unused object #%zu\n", i));
			nignored++;
			continue;
		}

		if (object_uuid_compare(obj1, obj2) == 0) {
			if (loglev >= 1)
				fprintf(stderr, "nobjects #%zu and #%zu have duplicate UUIDs\n",
					i, i + 1);
					object_dump(i, obj1),
					object_dump(i + 1, obj2);
			nfailed++;
		}
	}

	remove_segment(shmem_id, objects);
	if (nignored)
		printf("%zu objects ignored\n", nignored);
	if (!nfailed)
		printf("test successful (no duplicate UUIDs found)\n");
	else
		printf("test failed (found %zu duplicate UUIDs)\n", nfailed);

	return nfailed ? EXIT_FAILURE : EXIT_SUCCESS;
}
