/*
 * tclStubLibTbl.c --
 *
 *	Stub object that will be statically linked into extensions that want
 *	to access Tcl.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 1998 Paul Duffin.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 *----------------------------------------------------------------------
 *
 * TclInitStubTable --
 *
 *	Initialize the stub table, using the structure pointed at
 *	by the "version" argument.
 *
 * Results:
 *	Outputs the value of the "version" argument.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */
MODULE_SCOPE const char *
TclInitStubTable(
	const char *version) /* points to the version field of a
	                        TclStubInfoType structure variable. */
{
    tclStubsPtr = ((const TclStubInfoType *) version)->stubs;

    if (tclStubsPtr->hooks) {
	tclPlatStubsPtr = tclStubsPtr->hooks->tclPlatStubs;
	tclIntStubsPtr = tclStubsPtr->hooks->tclIntStubs;
	tclIntPlatStubsPtr = tclStubsPtr->hooks->tclIntPlatStubs;
    } else {
	tclPlatStubsPtr = NULL;
	tclIntStubsPtr = NULL;
	tclIntPlatStubsPtr = NULL;
    }

    return version;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
