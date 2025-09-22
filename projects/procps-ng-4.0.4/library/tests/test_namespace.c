/*
 * libprocps - Library to read proc filesystem
 * Tests for namespace library calls
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "misc.h"
#include "tests.h"

int check_name_minus(void *data)
{
    testname = "procps_ns_get_name() negative id";
    return (procps_ns_get_name(-1) == NULL);
}

int check_name_over(void *data)
{
    testname = "procps_ns_get_name() id over limit";
    return (procps_ns_get_name(999) == NULL);
}

int check_name_ipc(void *data)
{
    testname = "procps_ns_get_name() ipc";
    return (strcmp(procps_ns_get_name(PROCPS_NS_IPC),"ipc")==0);
}

int check_id_null(void *data)
{
    testname = "procps_ns_get_id(NULL)";
    return (procps_ns_get_id(NULL) < 0);
}

int check_id_unfound(void *data)
{
    testname = "procps_ns_get_id(unknown)";
    return (procps_ns_get_id("foobar") < 0);
}

int check_id_mnt(void *data)
{
    testname = "procps_ns_get_id(mnt)";
    return (procps_ns_get_id("mnt") == PROCPS_NS_MNT);
}

TestFunction test_funcs[] = {
    check_name_minus,
    check_name_over,
    check_name_ipc,
    check_id_null,
    check_id_unfound,
    check_id_mnt,
    NULL
};

int main(int argc, char *argv[])
{
    return run_tests(test_funcs, NULL);
}

