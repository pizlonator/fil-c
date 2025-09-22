/*
 * test_shm -- program to create a shared memory segment for testing
 *
 * Copyright 2022 Craig Small <csmall@dropbear.xyz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include "c.h"

#define DEFAULT_SLEEPTIME 300
#define SHM_SIZE 50

static void usage(void)
{
    fprintf(stderr, " %s [options]\n", program_invocation_short_name);
    fprintf(stderr, " -s <seconds>\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int sleep_time, opt;
    int shm_id;
    void *shm_addr;

    sleep_time = DEFAULT_SLEEPTIME;
    while ((opt = getopt(argc, argv, "s:")) != -1)
    {
        switch(opt)
        {
	    case 's':
            sleep_time = atoi(optarg);
            if (sleep_time < 1)
            {
                fprintf(stderr, "sleep time must be 1 second or more\n");
                usage();
            }
            break;
        default:
            usage();
        }
    }

    /* get some shared memory */
    if ( (shm_id = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0666)) < 0)
        xerr(EXIT_FAILURE, "Unable to shmget()");
    if ( (shm_addr = shmat(shm_id, NULL, SHM_RDONLY)) < 0)
        xerr(EXIT_FAILURE, "Unable to shmat()");
    printf("SHMID: %x\n", shm_id);
    sleep(sleep_time);
    return EXIT_SUCCESS;
}

