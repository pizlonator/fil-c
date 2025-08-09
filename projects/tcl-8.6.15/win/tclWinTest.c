/*
 * tclWinTest.c --
 *
 *	Contains commands for platform specific tests on Windows.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef USE_TCL_STUBS
#   define USE_TCL_STUBS
#endif
#include "tclInt.h"

/*
 * For TestplatformChmod on Windows
 */
#include <aclapi.h>
#include <sddl.h>

/*
 * MinGW 3.4.2 does not define this.
 */
#ifndef INHERITED_ACE
#define INHERITED_ACE (0x10)
#endif

/*
 * Forward declarations of functions defined later in this file:
 */

static Tcl_ObjCmdProc	TesteventloopCmd;
static Tcl_ObjCmdProc	TestvolumetypeCmd;
static Tcl_ObjCmdProc	TestwinclockCmd;
static Tcl_ObjCmdProc	TestwinsleepCmd;
static Tcl_ObjCmdProc	TestExceptionCmd;
static int		TestplatformChmod(const char *nativePath, int pmode);
static Tcl_ObjCmdProc	TestchmodCmd;

/*
 *----------------------------------------------------------------------
 *
 * TclplatformtestInit --
 *
 *	Defines commands that test platform specific functionality for Windows
 *	platforms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines new commands.
 *
 *----------------------------------------------------------------------
 */

int
TclplatformtestInit(
    Tcl_Interp *interp)		/* Interpreter to add commands to. */
{
    /*
     * Add commands for platform specific tests for Windows here.
     */

    Tcl_CreateObjCommand(interp, "testchmod", TestchmodCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testeventloop", TesteventloopCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testvolumetype", TestvolumetypeCmd,
	    NULL, NULL);
    Tcl_CreateObjCommand(interp, "testwinclock", TestwinclockCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testwinsleep", TestwinsleepCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "testexcept", TestExceptionCmd, NULL, NULL);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TesteventloopCmd --
 *
 *	This function implements the "testeventloop" command. It is used to
 *	test the Tcl notifier from an "external" event loop (i.e. not
 *	Tcl_DoOneEvent()).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TesteventloopCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static int *framePtr = NULL;/* Pointer to integer on stack frame of
				 * innermost invocation of the "wait"
				 * subcommand. */
    (void)clientData;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ...");
	return TCL_ERROR;
    }
    if (strcmp(Tcl_GetString(objv[1]), "done") == 0) {
	*framePtr = 1;
    } else if (strcmp(Tcl_GetString(objv[1]), "wait") == 0) {
	int *oldFramePtr, done;
	int oldMode = Tcl_SetServiceMode(TCL_SERVICE_ALL);

	/*
	 * Save the old stack frame pointer and set up the current frame.
	 */

	oldFramePtr = framePtr;
	framePtr = &done;

	/*
	 * Enter a standard Windows event loop until the flag changes. Note
	 * that we do not explicitly call Tcl_ServiceEvent().
	 */

	done = 0;
	while (!done) {
	    MSG msg;

	    if (!GetMessageW(&msg, NULL, 0, 0)) {
		/*
		 * The application is exiting, so repost the quit message and
		 * start unwinding.
		 */

		PostQuitMessage((int) msg.wParam);
		break;
	    }
	    TranslateMessage(&msg);
	    DispatchMessageW(&msg);
	}
	(void) Tcl_SetServiceMode(oldMode);
	framePtr = oldFramePtr;
    } else {
	Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]),
		"\": must be done or wait", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Testvolumetype --
 *
 *	This function implements the "testvolumetype" command. It is used to
 *	check the volume type (FAT, NTFS) of a volume.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestvolumetypeCmd(
    ClientData clientData,	/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
#define VOL_BUF_SIZE 32
    int found;
    char volType[VOL_BUF_SIZE];
    const char *path;

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?name?");
	return TCL_ERROR;
    }
    if (objc == 2) {
	/*
	 * path has to be really a proper volume, but we don't get query APIs
	 * for that until NT5
	 */

	path = Tcl_GetString(objv[1]);
    } else {
	path = NULL;
    }
    found = GetVolumeInformationA(path, NULL, 0, NULL, NULL, NULL, volType,
	    VOL_BUF_SIZE);

    if (found == 0) {
	Tcl_AppendResult(interp, "could not get volume type for \"",
		(path?path:""), "\"", NULL);
	TclWinConvertError(GetLastError());
	return TCL_ERROR;
    }
    Tcl_AppendResult(interp, volType, NULL);
    return TCL_OK;
#undef VOL_BUF_SIZE
}

/*
 *----------------------------------------------------------------------
 *
 * TestwinclockCmd --
 *
 *	Command that returns the seconds and microseconds portions of the
 *	system clock and of the Tcl clock so that they can be compared to
 *	validate that the Tcl clock is staying in sync.
 *
 * Usage:
 *	testclock
 *
 * Parameters:
 *	None.
 *
 * Results:
 *	Returns a standard Tcl result comprising a four-element list: the
 *	seconds and microseconds portions of the system clock, and the seconds
 *	and microseconds portions of the Tcl clock.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TestwinclockCmd(
    ClientData dummy,		/* Unused */
    Tcl_Interp* interp,		/* Tcl interpreter */
    int objc,			/* Argument count */
    Tcl_Obj *const objv[])	/* Argument vector */
{
    static const FILETIME posixEpoch = { 0xD53E8000, 0x019DB1DE };
				/* The Posix epoch, expressed as a Windows
				 * FILETIME */
    Tcl_Time tclTime;		/* Tcl clock */
    FILETIME sysTime;		/* System clock */
    Tcl_Obj *result;		/* Result of the command */
    LARGE_INTEGER t1, t2;
    LARGE_INTEGER p1, p2;

    if (objc != 1) {
	Tcl_WrongNumArgs(interp, 1, objv, "");
	return TCL_ERROR;
    }

    QueryPerformanceCounter(&p1);

    Tcl_GetTime(&tclTime);
    GetSystemTimeAsFileTime(&sysTime);
    t1.LowPart = posixEpoch.dwLowDateTime;
    t1.HighPart = posixEpoch.dwHighDateTime;
    t2.LowPart = sysTime.dwLowDateTime;
    t2.HighPart = sysTime.dwHighDateTime;
    t2.QuadPart -= t1.QuadPart;

    QueryPerformanceCounter(&p2);

    result = Tcl_NewObj();
    Tcl_ListObjAppendElement(interp, result,
	    Tcl_NewIntObj((int) (t2.QuadPart / 10000000)));
    Tcl_ListObjAppendElement(interp, result,
	    Tcl_NewIntObj((int) ((t2.QuadPart / 10) % 1000000)));
    Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(tclTime.sec));
    Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(tclTime.usec));

    Tcl_ListObjAppendElement(interp, result, Tcl_NewWideIntObj(p1.QuadPart));
    Tcl_ListObjAppendElement(interp, result, Tcl_NewWideIntObj(p2.QuadPart));

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
TestwinsleepCmd(
    ClientData clientData,	/* Unused */
    Tcl_Interp* interp,		/* Tcl interpreter */
    int objc,			/* Parameter count */
    Tcl_Obj *const * objv)	/* Parameter vector */
{
    int ms;
    (void)clientData;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "ms");
	return TCL_ERROR;
    }
    if (Tcl_GetIntFromObj(interp, objv[1], &ms) != TCL_OK) {
	return TCL_ERROR;
    }
    Sleep((DWORD) ms);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestExceptionCmd --
 *
 *	Causes this process to end with the named exception. Used for testing
 *	Tcl_WaitPid().
 *
 * Usage:
 *	testexcept <type>
 *
 * Parameters:
 *	Type of exception.
 *
 * Results:
 *	None, this process closes now and doesn't return.
 *
 * Side effects:
 *	This Tcl process closes, hard... Bang!
 *
 *----------------------------------------------------------------------
 */

static int
TestExceptionCmd(
    ClientData dummy,			/* Unused */
    Tcl_Interp* interp,			/* Tcl interpreter */
    int objc,				/* Argument count */
    Tcl_Obj *const objv[])		/* Argument vector */
{
    static const char *const cmds[] = {
	"access_violation", "datatype_misalignment", "array_bounds",
	"float_denormal", "float_divbyzero", "float_inexact",
	"float_invalidop", "float_overflow", "float_stack", "float_underflow",
	"int_divbyzero", "int_overflow", "private_instruction", "inpageerror",
	"illegal_instruction", "noncontinue", "stack_overflow",
	"invalid_disp", "guard_page", "invalid_handle", "ctrl+c",
	NULL
    };
    static const DWORD exceptions[] = {
	EXCEPTION_ACCESS_VIOLATION, EXCEPTION_DATATYPE_MISALIGNMENT,
	EXCEPTION_ARRAY_BOUNDS_EXCEEDED, EXCEPTION_FLT_DENORMAL_OPERAND,
	EXCEPTION_FLT_DIVIDE_BY_ZERO, EXCEPTION_FLT_INEXACT_RESULT,
	EXCEPTION_FLT_INVALID_OPERATION, EXCEPTION_FLT_OVERFLOW,
	EXCEPTION_FLT_STACK_CHECK, EXCEPTION_FLT_UNDERFLOW,
	EXCEPTION_INT_DIVIDE_BY_ZERO, EXCEPTION_INT_OVERFLOW,
	EXCEPTION_PRIV_INSTRUCTION, EXCEPTION_IN_PAGE_ERROR,
	EXCEPTION_ILLEGAL_INSTRUCTION, EXCEPTION_NONCONTINUABLE_EXCEPTION,
	EXCEPTION_STACK_OVERFLOW, EXCEPTION_INVALID_DISPOSITION,
	EXCEPTION_GUARD_PAGE, EXCEPTION_INVALID_HANDLE, CONTROL_C_EXIT
    };
    int cmd;
    (void)dummy;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 0, objv, "<type-of-exception>");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], cmds, "command", 0,
	    &cmd) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Make sure the GPF dialog doesn't popup.
     */

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

    /*
     * As Tcl does not handle structured exceptions, this falls all the way
     * back up the instruction stack to the C run-time portion that called
     * main() where the process will now be terminated with this exception
     * code by the default handler the C run-time provides.
     */

    /* SMASH! */
    RaiseException(exceptions[cmd], EXCEPTION_NONCONTINUABLE, 0, NULL);

    return TCL_OK;
}

/*
 * This "chmod" works sufficiently for test script purposes. Do not expect
 * it to be exact emulation of Unix chmod (not sure if that's even possible)
 */
static int
TestplatformChmod(
    const char *nativePath,
    int pmode)
{
    /*
     * Note FILE_DELETE_CHILD missing from dirWriteMask because we do
     * not want overriding of child's delete setting when testing
     */
    static const DWORD dirWriteMask =
	FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA |
	FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | STANDARD_RIGHTS_WRITE | DELETE |
	SYNCHRONIZE;
    static const DWORD dirReadMask =
	FILE_READ_ATTRIBUTES | FILE_READ_EA | FILE_LIST_DIRECTORY |
	STANDARD_RIGHTS_READ | SYNCHRONIZE;
    /* Note - default user privileges allow ignoring TRAVERSE setting */
    static const DWORD dirExecuteMask =
	FILE_TRAVERSE | STANDARD_RIGHTS_READ | SYNCHRONIZE;

    static const DWORD fileWriteMask =
	FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | FILE_WRITE_DATA |
	FILE_APPEND_DATA | STANDARD_RIGHTS_WRITE | DELETE | SYNCHRONIZE;
    static const DWORD fileReadMask =
	FILE_READ_ATTRIBUTES | FILE_READ_EA | FILE_READ_DATA |
	STANDARD_RIGHTS_READ | SYNCHRONIZE;
    static const DWORD fileExecuteMask =
	FILE_EXECUTE | STANDARD_RIGHTS_READ | SYNCHRONIZE;

    DWORD attr, newAclSize;
    PACL newAcl = NULL;
    int res = 0;

    HANDLE hToken = NULL;
    int i;
    int nSids = 0;
    struct {
	PSID pSid;
	DWORD mask;
	DWORD sidLen;
    } aceEntry[3];
    DWORD dw;
    int isDir;
    TOKEN_USER *pTokenUser = NULL;

    res = -1; /* Assume failure */

    attr = GetFileAttributesA(nativePath);
    if (attr == 0xFFFFFFFF) {
	goto done; /* Not found */
    }

    isDir = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
	goto done;
    }

    /* Get process SID */
    if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dw) &&
	GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
	goto done;
    }
    pTokenUser = (TOKEN_USER *)ckalloc(dw);
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dw, &dw)) {
	goto done;
    }
    aceEntry[nSids].sidLen = GetLengthSid(pTokenUser->User.Sid);
    aceEntry[nSids].pSid = ckalloc(aceEntry[nSids].sidLen);
    if (!CopySid(aceEntry[nSids].sidLen,
		 aceEntry[nSids].pSid,
		 pTokenUser->User.Sid)) {
	ckfree(aceEntry[nSids].pSid); /* Since we have not ++'ed nSids */
	goto done;
    }
    /*
     * Always include DACL modify rights so we don't get locked out
     */
    aceEntry[nSids].mask = READ_CONTROL | WRITE_DAC | WRITE_OWNER | SYNCHRONIZE |
			   FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
    if (pmode & 0700) {
	/* Owner permissions. Assumes current process is owner */
	if (pmode & 0400) {
	    aceEntry[nSids].mask |= isDir ? dirReadMask : fileReadMask;
	}
	if (pmode & 0200) {
	    aceEntry[nSids].mask |= isDir ? dirWriteMask : fileWriteMask;
	}
	if (pmode & 0100) {
	    aceEntry[nSids].mask |= isDir ? dirExecuteMask : fileExecuteMask;
	}
    }
    ++nSids;

    if (pmode & 0070) {
	/* Group permissions. */

	TOKEN_PRIMARY_GROUP *pTokenGroup;

	/* Get primary group SID */
	if (!GetTokenInformation(
		hToken, TokenPrimaryGroup, NULL, 0, &dw) &&
		GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
	    goto done;
	}
	pTokenGroup = (TOKEN_PRIMARY_GROUP *)ckalloc(dw);
	if (!GetTokenInformation(hToken, TokenPrimaryGroup, pTokenGroup, dw, &dw)) {
	    ckfree(pTokenGroup);
	    goto done;
	}
	aceEntry[nSids].sidLen = GetLengthSid(pTokenGroup->PrimaryGroup);
	aceEntry[nSids].pSid = ckalloc(aceEntry[nSids].sidLen);
	if (!CopySid(aceEntry[nSids].sidLen, aceEntry[nSids].pSid, pTokenGroup->PrimaryGroup)) {
	    ckfree(pTokenGroup);
	    ckfree(aceEntry[nSids].pSid); /* Since we have not ++'ed nSids */
	    goto done;
	}
	ckfree(pTokenGroup);

	/* Generate mask for group ACL */

	aceEntry[nSids].mask = 0;
	if (pmode & 0040) {
	    aceEntry[nSids].mask |= isDir ? dirReadMask : fileReadMask;
	}
	if (pmode & 0020) {
	    aceEntry[nSids].mask |= isDir ? dirWriteMask : fileWriteMask;
	}
	if (pmode & 0010) {
	    aceEntry[nSids].mask |= isDir ? dirExecuteMask : fileExecuteMask;
	}
	++nSids;
    }

    if (pmode & 0007) {
	/* World permissions */
	PSID pWorldSid;
	if (!ConvertStringSidToSidA("S-1-1-0", &pWorldSid)) {
	    goto done;
	}
	aceEntry[nSids].sidLen = GetLengthSid(pWorldSid);
	aceEntry[nSids].pSid = ckalloc(aceEntry[nSids].sidLen);
	if (!CopySid(aceEntry[nSids].sidLen, aceEntry[nSids].pSid, pWorldSid)) {
	    LocalFree(pWorldSid);
	    ckfree(aceEntry[nSids].pSid); /* Since we have not ++'ed nSids */
	    goto done;
	}
	LocalFree(pWorldSid);

	/* Generate mask for world ACL */

	aceEntry[nSids].mask = 0;
	if (pmode & 0004) {
	    aceEntry[nSids].mask |= isDir ? dirReadMask : fileReadMask;
	}
	if (pmode & 0002) {
	    aceEntry[nSids].mask |= isDir ? dirWriteMask : fileWriteMask;
	}
	if (pmode & 0001) {
	    aceEntry[nSids].mask |= isDir ? dirExecuteMask : fileExecuteMask;
	}
	++nSids;
    }

    /* Allocate memory and initialize the new ACL. */

    newAclSize = sizeof(ACL);
    /* Add in size required for each ACE entry in the ACL */
    for (i = 0; i < nSids; ++i) {
	newAclSize +=
	    TclOffset(ACCESS_ALLOWED_ACE, SidStart) + aceEntry[i].sidLen;
    }
    newAcl = (PACL)ckalloc(newAclSize);
    if (!InitializeAcl(newAcl, newAclSize, ACL_REVISION)) {
	goto done;
    }

    for (i = 0; i < nSids; ++i) {
	if (!AddAccessAllowedAce(newAcl, ACL_REVISION, aceEntry[i].mask, aceEntry[i].pSid)) {
	    goto done;
	}
    }

    /*
     * Apply the new ACL. Note PROTECTED_DACL_SECURITY_INFORMATION can be used
     * to remove inherited ACL (we need to overwrite the default ACL's in this case)
     */

    if (SetNamedSecurityInfoA((LPSTR)nativePath,
			      SE_FILE_OBJECT,
			      DACL_SECURITY_INFORMATION |
				  PROTECTED_DACL_SECURITY_INFORMATION,
			      NULL,
			      NULL,
			      newAcl,
			      NULL) == ERROR_SUCCESS) {
	res = 0;
    }

  done:
    if (pTokenUser) {
	ckfree(pTokenUser);
    }
    if (hToken) {
	CloseHandle(hToken);
    }
    if (newAcl) {
	ckfree(newAcl);
    }
    for (i = 0; i < nSids; ++i) {
	ckfree(aceEntry[i].pSid);
    }

    if (res != 0) {
	return res;
    }

    /* Run normal chmod command */
    return chmod(nativePath, pmode);
}

/*
 *---------------------------------------------------------------------------
 *
 * TestchmodCmd --
 *
 *	Implements the "testchmod" cmd. Used when testing "file" command. The
 *	only attribute used by the Windows platform is the user write flag; if
 *	this is not set, the file is made read-only. Otherwise, the file is
 *	made read-write.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Changes permissions of specified files.
 *
 *---------------------------------------------------------------------------
 */

static int
TestchmodCmd(
    ClientData dummy,		/* Not used. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Parameter count */
    Tcl_Obj *const * objv)	/* Parameter vector */
{
    int i, mode;
    (void)dummy;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "mode file ?file ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIntFromObj(interp, objv[1], &mode) != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 2; i < objc; i++) {
	Tcl_DString buffer;
	const char *translated;

	translated = Tcl_TranslateFileName(interp, Tcl_GetString(objv[i]), &buffer);
	if (translated == NULL) {
	    return TCL_ERROR;
	}
	if (TestplatformChmod(translated, mode) != 0) {
	    Tcl_AppendResult(interp, translated, ": ", Tcl_PosixError(interp),
		    NULL);
	    return TCL_ERROR;
	}
	Tcl_DStringFree(&buffer);
    }
    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
