/*
 * pkgua.c --
 *
 *	This file contains a simple Tcl package "pkgua" that is intended for
 *	testing the Tcl dynamic unloading facilities.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 2004 Georgios Petasis
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#undef STATIC_BUILD
#include "tcl.h"

/*
 * In the following hash table we are going to store a struct that holds all
 * the command tokens created by Tcl_CreateObjCommand in an interpreter,
 * indexed by the interpreter. In this way, we can find which command tokens
 * we have registered in a specific interpreter, in order to unload them. We
 * need to keep the various command tokens we have registered, as they are the
 * only safe way to unregister our registered commands, even if they have been
 * renamed.
 */

typedef struct ThreadSpecificData {
    int interpTokenMapInitialised;
    Tcl_HashTable interpTokenMap;
} ThreadSpecificData;
static Tcl_ThreadDataKey dataKey;
#define MAX_REGISTERED_COMMANDS 2

static void
CommandDeleted(ClientData clientData)
{
    Tcl_Command *cmdToken = (Tcl_Command *)clientData;
    *cmdToken = NULL;
}

static void
PkguaInitTokensHashTable(void)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData((&dataKey), sizeof(ThreadSpecificData));

    if (tsdPtr->interpTokenMapInitialised) {
	return;
    }
    Tcl_InitHashTable(&tsdPtr->interpTokenMap, TCL_ONE_WORD_KEYS);
    tsdPtr->interpTokenMapInitialised = 1;
}

static void
PkguaFreeTokensHashTable(void)
{
    Tcl_HashSearch search;
    Tcl_HashEntry *entryPtr;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData((&dataKey), sizeof(ThreadSpecificData));

    for (entryPtr = Tcl_FirstHashEntry(&tsdPtr->interpTokenMap, &search);
	    entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&search)) {
	Tcl_Free((char *) Tcl_GetHashValue(entryPtr));
    }
    tsdPtr->interpTokenMapInitialised = 0;
}

static Tcl_Command *
PkguaInterpToTokens(
    Tcl_Interp *interp)
{
    int newEntry;
    Tcl_Command *cmdTokens;
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData((&dataKey), sizeof(ThreadSpecificData));
    Tcl_HashEntry *entryPtr =
	    Tcl_CreateHashEntry(&tsdPtr->interpTokenMap, (char *) interp, &newEntry);

    if (newEntry) {
	cmdTokens = (Tcl_Command *)
		Tcl_Alloc(sizeof(Tcl_Command) * (MAX_REGISTERED_COMMANDS));
	for (newEntry=0 ; newEntry<MAX_REGISTERED_COMMANDS ; ++newEntry) {
	    cmdTokens[newEntry] = NULL;
	}
	Tcl_SetHashValue(entryPtr, cmdTokens);
    } else {
	cmdTokens = (Tcl_Command *) Tcl_GetHashValue(entryPtr);
    }
    return cmdTokens;
}

static void
PkguaDeleteTokens(
    Tcl_Interp *interp)
{
    ThreadSpecificData *tsdPtr = (ThreadSpecificData *)Tcl_GetThreadData((&dataKey), sizeof(ThreadSpecificData));
    Tcl_HashEntry *entryPtr =
	    Tcl_FindHashEntry(&tsdPtr->interpTokenMap, (char *) interp);

    if (entryPtr) {
	Tcl_Free((char *) Tcl_GetHashValue(entryPtr));
	Tcl_DeleteHashEntry(entryPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PkguaEqObjCmd --
 *
 *	This procedure is invoked to process the "pkgua_eq" Tcl command. It
 *	expects two arguments and returns 1 if they are the same, 0 if they
 *	are different.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
PkguaEqObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result;
    const char *str1, *str2;
    int len1, len2;
    (void)dummy;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv,  "string1 string2");
	return TCL_ERROR;
    }

    str1 = Tcl_GetStringFromObj(objv[1], &len1);
    str2 = Tcl_GetStringFromObj(objv[2], &len2);
    len1 = Tcl_NumUtfChars(str1, len1);
    len2 = Tcl_NumUtfChars(str2, len2);
    if (len1 == len2) {
	result = (Tcl_UtfNcmp(str1, str2, (size_t)len1) == 0);
    } else {
	result = 0;
    }
    Tcl_SetObjResult(interp, Tcl_NewIntObj(result));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PkguaQuoteObjCmd --
 *
 *	This procedure is invoked to process the "pkgua_quote" Tcl command. It
 *	expects one argument, which it returns as result.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
PkguaQuoteObjCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    (void)dummy;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, objv[1]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Pkgua_Init --
 *
 *	This is a package initialization procedure, which is called by Tcl
 *	when this package is to be added to an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Pkgua_Init(
    Tcl_Interp *interp)		/* Interpreter in which the package is to be
				 * made available. */
{
    int code;
    Tcl_Command *cmdTokens;

    if (Tcl_InitStubs(interp, "8.5-", 0) == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize our Hash table, where we store the registered command tokens
     * for each interpreter.
     */

    PkguaInitTokensHashTable();

    code = Tcl_PkgProvide(interp, "pkgua", "1.0");
    if (code != TCL_OK) {
	return code;
    }

    Tcl_SetVar2(interp, "::pkgua_loaded", NULL, ".", TCL_APPEND_VALUE);

    cmdTokens = PkguaInterpToTokens(interp);
    cmdTokens[0] =
	    Tcl_CreateObjCommand(interp, "pkgua_eq", PkguaEqObjCmd, &cmdTokens[0],
		    CommandDeleted);
    cmdTokens[1] =
	    Tcl_CreateObjCommand(interp, "pkgua_quote", PkguaQuoteObjCmd,
		    &cmdTokens[1], CommandDeleted);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Pkgua_SafeInit --
 *
 *	This is a package initialization procedure, which is called by Tcl
 *	when this package is to be added to a safe interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Pkgua_SafeInit(
    Tcl_Interp *interp)		/* Interpreter in which the package is to be
				 * made available. */
{
    return Pkgua_Init(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Pkgua_Unload --
 *
 *	This is a package unloading initialization procedure, which is called
 *	by Tcl when this package is to be unloaded from an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Pkgua_Unload(
    Tcl_Interp *interp,		/* Interpreter from which the package is to be
				 * unloaded. */
    int flags)			/* Flags passed by the unloading mechanism */
{
    int code, cmdIndex;
    Tcl_Command *cmdTokens = PkguaInterpToTokens(interp);

    for (cmdIndex=0 ; cmdIndex<MAX_REGISTERED_COMMANDS ; cmdIndex++) {
	if (cmdTokens[cmdIndex] == NULL) {
	    continue;
	}
	code = Tcl_DeleteCommandFromToken(interp, cmdTokens[cmdIndex]);
	if (code != TCL_OK) {
	    return code;
	}
    }

    PkguaDeleteTokens(interp);

    Tcl_SetVar2(interp, "::pkgua_detached", NULL, ".", TCL_APPEND_VALUE);

    if (flags == TCL_UNLOAD_DETACH_FROM_PROCESS) {
	/*
	 * Tcl is ready to detach this library from the running application.
	 * We should free all the memory that is not related to any
	 * interpreter.
	 */

	PkguaFreeTokensHashTable();
	Tcl_SetVar2(interp, "::pkgua_unloaded", NULL, ".", TCL_APPEND_VALUE);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Pkgua_SafeUnload --
 *
 *	This is a package unloading initialization procedure, which is called
 *	by Tcl when this package is to be unloaded from an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

DLLEXPORT int
Pkgua_SafeUnload(
    Tcl_Interp *interp,		/* Interpreter from which the package is to be
				 * unloaded. */
    int flags)			/* Flags passed by the unloading mechanism */
{
    return Pkgua_Unload(interp, flags);
}
