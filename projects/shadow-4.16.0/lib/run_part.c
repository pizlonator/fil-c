#include <config.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <lib/prototypes.h>

#include "alloc.h"
#include "run_part.h"
#include "shadowlog_internal.h"


int run_part (char *script_path, const char *name, const char *action)
{
	int pid;
	int wait_status;
	int pid_status;
	char *args[] = { script_path, NULL };

	pid=fork();
	if (pid==-1) {
		perror ("Could not fork");
		return 1;
	}
	if (pid==0) {
		setenv ("ACTION",action,1);
		setenv ("SUBJECT",name,1);
		execv (script_path,args);
		perror ("execv");
		exit(1);
	}

	pid_status = wait (&wait_status);
	if (pid_status == pid) {
		return (wait_status);
	}

	perror ("waitpid");
	return (1);
}

int run_parts (const char *directory, const char *name, const char *action)
{
	struct dirent **namelist;
	int scanlist;
	int n;
	int execute_result = 0;

	scanlist = scandir (directory, &namelist, 0, alphasort);
	if (scanlist<=0) {
		return (0);
	}

	for (n=0; n<scanlist; n++) {
		char         *s;
		struct stat  sb;

		if (asprintf(&s, "%s/%s", directory, namelist[n]->d_name) == -1) {
			fprintf(stderr, "could not allocate memory\n");
			for (; n<scanlist; n++) {
				free(namelist[n]);
			}
			free(namelist);
			return (1);
		}

		execute_result = 0;
		if (stat (s, &sb) == -1) {
			perror ("stat");
			free(s);
			for (; n<scanlist; n++) {
				free (namelist[n]);
			}
			free (namelist);
			return (1);
		}

		if (S_ISREG (sb.st_mode) || S_ISLNK (sb.st_mode)) {
			execute_result = run_part (s, name, action);
		}

		free(s);

		if (execute_result!=0) {
			fprintf (shadow_logfd,
				"%s: did not exit cleanly.\n",
			    namelist[n]->d_name);
			for (; n<scanlist; n++) {
				free (namelist[n]);
			}
			break;
		}

		free (namelist[n]);
	}
	free (namelist);

	return (execute_result);
}

