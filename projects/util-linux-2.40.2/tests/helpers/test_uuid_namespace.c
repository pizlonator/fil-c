/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2017 Philip Prindeville <philipp@redfish-solutions.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libuuid/src/uuid.h"

static void get_template(const char *ns)
{
	const uuid_t *uuidptr;
	char buf[UUID_STR_LEN];

	uuidptr = uuid_get_template(ns);
	if (uuidptr == NULL)
		strcpy(buf, "NULL");
	else
		uuid_unparse_lower(*uuidptr, buf);

	printf("uuid_get_template %s returns %s\n", (ns ? ns : "NULL"), buf);
}

int main(void)
{
	get_template("dns");

	get_template("url");

	get_template("oid");

	get_template("x500");

	get_template(NULL);
	get_template("");
	get_template("unknown");

	exit(0);
}

