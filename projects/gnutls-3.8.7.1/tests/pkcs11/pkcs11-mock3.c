/*
 * Copyright (C) 2023 Red Hat, Inc.
 *
 * Author: Daiki Ueno
 *
 * This file is part of GnuTLS.
 *
 * GnuTLS is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuTLS is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dlfcn.h>
#include <p11-kit/pkcs11.h>
#include <p11-kit/pkcs11x.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "softhsm.h"

/* This provides a mock PKCS #11 module that delegates all the
 * operations to SoftHSM except that it returns
 * CKA_NSS_SERVER_DISTRUST_AFTER upon C_GetAttributeValue.
 */

static void *dl;
static CK_C_GetAttributeValue base_C_GetAttributeValue;
static CK_FUNCTION_LIST override_funcs;

#ifdef __sun
#pragma fini(mock_deinit)
#pragma init(mock_init)
#define _CONSTRUCTOR
#define _DESTRUCTOR
#else
#define _CONSTRUCTOR __attribute__((constructor))
#define _DESTRUCTOR __attribute__((destructor))
#endif

/* Should be a date before the activation time of chain[0] in
 * pkcs11/distrust-after.c: Tue Mar 14 10:04:03 UTC 2023
 */
#define DISTRUST_AFTER "230314000000Z"

static CK_RV override_C_GetAttributeValue(CK_SESSION_HANDLE hSession,
					  CK_OBJECT_HANDLE hObject,
					  CK_ATTRIBUTE_PTR pTemplate,
					  CK_ULONG ulCount)
{
	CK_ATTRIBUTE *template;
	CK_ULONG count = 0, i, offset = ulCount;
	CK_RV rv;

	template = malloc(ulCount * sizeof(CK_ATTRIBUTE));
	if (!template) {
		return CKR_HOST_MEMORY;
	}

	for (i = 0; i < ulCount; i++) {
		if (pTemplate[i].type == CKA_NSS_SERVER_DISTRUST_AFTER) {
			offset = i;
		} else {
			template[count++] = pTemplate[i];
		}
	}

	rv = base_C_GetAttributeValue(hSession, hObject, template, count);

	for (i = 0; i < offset; i++) {
		pTemplate[i] = template[i];
	}

	if (offset < ulCount) {
		if (!pTemplate[offset].pValue) {
			pTemplate[offset].ulValueLen =
				sizeof(DISTRUST_AFTER) - 1;
		} else if (pTemplate[offset].ulValueLen <
			   sizeof(DISTRUST_AFTER) - 1) {
			pTemplate[offset].ulValueLen =
				CK_UNAVAILABLE_INFORMATION;
			rv = CKR_BUFFER_TOO_SMALL;
		} else {
			memcpy(pTemplate[offset].pValue, DISTRUST_AFTER,
			       sizeof(DISTRUST_AFTER) - 1);
			pTemplate[offset].ulValueLen =
				sizeof(DISTRUST_AFTER) - 1;
		}
	}

	for (i = offset + 1; i < ulCount; i++) {
		pTemplate[i] = template[i];
	}

	free(template);

	return rv;
}

CK_RV C_GetFunctionList(CK_FUNCTION_LIST **function_list)
{
	CK_C_GetFunctionList func;
	CK_FUNCTION_LIST *funcs;

	assert(dl);

	func = dlsym(dl, "C_GetFunctionList");
	if (func == NULL) {
		return CKR_GENERAL_ERROR;
	}

	func(&funcs);

	base_C_GetAttributeValue = funcs->C_GetAttributeValue;

	memcpy(&override_funcs, funcs, sizeof(CK_FUNCTION_LIST));
	override_funcs.C_GetAttributeValue = override_C_GetAttributeValue;
	*function_list = &override_funcs;

	return CKR_OK;
}

static _CONSTRUCTOR void mock_init(void)
{
	const char *lib;

	/* suppress compiler warning */
	(void)set_softhsm_conf;

	lib = softhsm_lib();

	dl = dlopen(lib, RTLD_NOW);
	if (dl == NULL)
		exit(77);
}

static _DESTRUCTOR void mock_deinit(void)
{
	dlclose(dl);
}
