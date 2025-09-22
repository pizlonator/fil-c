/*
 * NUMA node support for <PIDS> & <STAT> interfaces
 *
 * Copyright © 2017-2023 Jim Warner <james.warner@comcast.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef NUMA_DISABLE
#include <dlfcn.h>
#endif
#include <stdlib.h>

#include "numa.h"

/*
 * We're structured so that if numa_init() is NOT called or that ./configure |
 * --disable-numa WAS specified, then calls to both of our primary functions |
 * of numa_max_node() plus numa_node_of_cpu() would always return a negative |
 * 1 which signifies that NUMA information isn't available. That ./configure |
 * option might be required when libdl.so (necessary for dlopen) is missing. |
 */


/* ------------------------------------------------------------------------- +
   a strictly development #define, existing specifically for the top program |
   ( and it has no affect if ./configure --disable-numa has been specified ) | */
//#define PRETEND_NUMA     // pretend there are 3 'discontiguous' numa nodes |
// ------------------------------------------------------------------------- +


static int null_max_node (void) { return -1; }
static int null_node_of_cpu (int n) { (void)n; return -1; }


#ifndef NUMA_DISABLE
 #ifdef PRETEND_NUMA
static int fake_max_node (void) { return 3; }
static int fake_node_of_cpu (int n) { return (1 == (n % 4)) ? 0 : (n % 4); }
 #endif
#endif


#ifndef NUMA_DISABLE
static void *libnuma_handle;
#endif
int (*numa_max_node) (void)   = null_max_node;
int (*numa_node_of_cpu) (int) = null_node_of_cpu;


void numa_init (void) {
    static int initialized;

    if (initialized)
        return;

#ifndef NUMA_DISABLE
 #ifndef PRETEND_NUMA
    // we'll try for the most recent version, then a version we know works...
    if ((libnuma_handle = dlopen("libnuma.so", RTLD_LAZY))
    || (libnuma_handle = dlopen("libnuma.so.1", RTLD_LAZY))) {
        numa_max_node = dlsym(libnuma_handle, "numa_max_node");
        numa_node_of_cpu = dlsym(libnuma_handle, "numa_node_of_cpu");
        if (numa_max_node == NULL
        || (numa_node_of_cpu == NULL)) {
            // this dlclose is safe - we've yet to call numa_node_of_cpu
            // ( there's one other dlclose which has now been disabled )
            dlclose(libnuma_handle);
            libnuma_handle = NULL;
            numa_max_node = null_max_node;
            numa_node_of_cpu = null_node_of_cpu;
        }
    }
 #else
    libnuma_handle = (void *)-1;
    numa_max_node = fake_max_node;
    numa_node_of_cpu = fake_node_of_cpu;
 #endif
#endif
    initialized = 1;
} // end: numa_init


void numa_uninit (void) {
#ifndef PRETEND_NUMA
    /* note: we'll skip a dlcose() to avoid the following libnuma memory
     *       leak which is triggered after a call to numa_node_of_cpu():
     *         ==1234== LEAK SUMMARY:
     *         ==1234==    definitely lost: 512 bytes in 1 blocks
     *         ==1234==    indirectly lost: 48 bytes in 2 blocks
     *         ==1234==    ...
     * [ thanks very much libnuma for all the pains you have caused us ]
     */
//  if (libnuma_handle)
//      dlclose(libnuma_handle);
#endif
} // end: numa_uninit


#if defined(PRETEND_NUMA) && defined(NUMA_DISABLE)
# warning 'PRETEND_NUMA' ignored, 'NUMA_DISABLE' is active
#endif
