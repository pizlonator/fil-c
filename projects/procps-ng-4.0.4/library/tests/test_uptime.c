/*
 * libprocps - Library to read proc filesystem
 * Tests for version library calls
 *
 * Copyright 2016 Craig Small <csmall@dropbear.xyz>
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

#include "misc.h"
#include "tests.h"

int check_uptime(void *data)
{
    testname = "procps_uptime()";
    double up=0, idle=0;
    int rc;
    rc = procps_uptime(&up, &idle);
    return (rc == 0 && up > 0 && idle > 0);
}

int check_uptime_nullup(void *data)
{
    double idle=0;
    int rc;
    testname = "procps_uptime() (up=NULL)";
    rc = procps_uptime(NULL, &idle);
    return (rc == 0 && idle > 0);
}

int check_uptime_nullidle(void *data)
{
    double up=0;
    int rc;
    testname = "procps_uptime() (idle=NULL)";
    rc = procps_uptime(&up, NULL);
    return (rc == 0 && up > 0);
}

int check_uptime_nullall(void *data)
{
    int rc;
    testname = "procps_uptime() (up,idle=NULL)";
    rc = procps_uptime(NULL, NULL);
    return (rc == 0);
}

int check_uptime_sprint(void *data)
{
    char *str;
    testname = "procps_uptime_sprint()";

    str = procps_uptime_sprint();

    return (str != NULL && str[0] != '\0');
}

int check_uptime_sprint_short(void *data)
{
    char *str;
    testname = "procps_uptime_sprint_short()";

    str = procps_uptime_sprint_short();

    return (str != NULL && str[0] != '\0');
}

TestFunction test_funcs[] = {
    check_uptime,
    check_uptime_nullup,
    check_uptime_nullidle,
    check_uptime_nullall,
    check_uptime_sprint,
    check_uptime_sprint_short,
    NULL,
};

int main(int argc, char *argv[])
{
    return run_tests(test_funcs, NULL);
}


