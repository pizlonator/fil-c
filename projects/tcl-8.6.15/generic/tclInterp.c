/*
 * tclInterp.c --
 *
 *	This file implements the "interp" command which allows creation and
 *	manipulation of Tcl interpreters from within Tcl scripts.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 2004 Donal K. Fellows
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tclInt.h"

/*
 * A pointer to a string that holds an initialization script that if non-NULL
 * is evaluated in Tcl_Init() prior to the built-in initialization script
 * above. This variable can be modified by the function below.
 */

static const char *tclPreInitScript = NULL;

/* Forward declaration */
struct Target;

/*
 * struct Alias:
 *
 * Stores information about an alias. Is stored in the child interpreter and
 * used by the source command to find the target command in the parent when
 * the source command is invoked.
 */

typedef struct Alias {
    Tcl_Obj *token;		/* Token for the alias command in the child
				 * interp. This used to be the command name in
				 * the child when the alias was first
				 * created. */
    Tcl_Interp *targetInterp;	/* Interp in which target command will be
				 * invoked. */
    Tcl_Command childCmd;	/* Source command in child interpreter, bound
				 * to command that invokes the target command
				 * in the target interpreter. */
    Tcl_HashEntry *aliasEntryPtr;
				/* Entry for the alias hash table in child.
				 * This is used by alias deletion to remove
				 * the alias from the child interpreter alias
				 * table. */
    struct Target *targetPtr;	/* Entry for target command in parent. This is
				 * used in the parent interpreter to map back
				 * from the target command to aliases
				 * redirecting to it. */
    int objc;			/* Count of Tcl_Obj in the prefix of the
				 * target command to be invoked in the target
				 * interpreter. Additional arguments specified
				 * when calling the alias in the child interp
				 * will be appended to the prefix before the
				 * command is invoked. */
    Tcl_Obj *objPtr;		/* The first actual prefix object - the target
				 * command name; this has to be at the end of
				 * the structure, which will be extended to
				 * accommodate the remaining objects in the
				 * prefix. */
} Alias;

/*
 *
 * struct Child:
 *
 * Used by the "interp" command to record and find information about child
 * interpreters. Maps from a command name in the parent to information about a
 * child interpreter, e.g. what aliases are defined in it.
 */

typedef struct Child {
    Tcl_Interp *parentInterp;	/* Parent interpreter for this child. */
    Tcl_HashEntry *childEntryPtr;
				/* Hash entry in parents child table for this
				 * child interpreter. Used to find this
				 * record, and used when deleting the child
				 * interpreter to delete it from the parent's
				 * table. */
    Tcl_Interp	*childInterp;	/* The child interpreter. */
    Tcl_Command interpCmd;	/* Interpreter object command. */
    Tcl_HashTable aliasTable;	/* Table which maps from names of commands in
				 * child interpreter to struct Alias defined
				 * below. */
} Child;

/*
 * struct Target:
 *
 * Maps from parent interpreter commands back to the source commands in child
 * interpreters. This is needed because aliases can be created between sibling
 * interpreters and must be deleted when the target interpreter is deleted. In
 * case they would not be deleted the source interpreter would be left with a
 * "dangling pointer". One such record is stored in the Parent record of the
 * parent interpreter with the parent for each alias which directs to a
 * command in the parent. These records are used to remove the source command
 * for an from a child if/when the parent is deleted. They are organized in a
 * doubly-linked list attached to the parent interpreter.
 */

typedef struct Target {
    Tcl_Command	childCmd;	/* Command for alias in child interp. */
    Tcl_Interp *childInterp;	/* Child Interpreter. */
    struct Target *nextPtr;	/* Next in list of target records, or NULL if
				 * at the end of the list of targets. */
    struct Target *prevPtr;	/* Previous in list of target records, or NULL
				 * if at the start of the list of targets. */
} Target;

/*
 * struct Parent:
 *
 * This record is used for two purposes: First, childTable (a hashtable) maps
 * from names of commands to child interpreters. This hashtable is used to
 * store information about child interpreters of this interpreter, to map over
 * all children, etc. The second purpose is to store information about all
 * aliases in children (or siblings) which direct to target commands in this
 * interpreter (using the targetsPtr doubly-linked list).
 *
 * NB: the flags field in the interp structure, used with SAFE_INTERP mask
 * denotes whether the interpreter is safe or not. Safe interpreters have
 * restricted functionality, can only create safe interpreters and can
 * only load safe extensions.
 */

typedef struct Parent {
    Tcl_HashTable childTable;	/* Hash table for child interpreters. Maps
				 * from command names to Child records. */
    Target *targetsPtr;		/* The head of a doubly-linked list of all the
				 * target records which denote aliases from
				 * children or sibling interpreters that direct
				 * to commands in this interpreter. This list
				 * is used to remove dangling pointers from
				 * the child (or sibling) interpreters when
				 * this interpreter is deleted. */
} Parent;

/*
 * The following structure keeps track of all the Parent and Child information
 * on a per-interp basis.
 */

typedef struct InterpInfo {
    Parent parent;		/* Keeps track of all interps for which this
				 * interp is the Parent. */
    Child child;		/* Information necessary for this interp to
				 * function as a child. */
} InterpInfo;

/*
 * Limit callbacks handled by scripts are modelled as structures which are
 * stored in hashes indexed by a two-word key. Note that the type of the
 * 'type' field in the key is not int; this is to make sure that things are
 * likely to work properly on 64-bit architectures.
 */

typedef struct ScriptLimitCallback {
    Tcl_Interp *interp;		/* The interpreter in which to execute the
				 * callback. */
    Tcl_Obj *scriptObj;		/* The script to execute to perform the
				 * user-defined part of the callback. */
    int type;			/* What kind of callback is this. */
    Tcl_HashEntry *entryPtr;	/* The entry in the hash table maintained by
				 * the target interpreter that refers to this
				 * callback record, or NULL if the entry has
				 * already been deleted from that hash
				 * table. */
} ScriptLimitCallback;

typedef struct ScriptLimitCallbackKey {
    Tcl_Interp *interp;		/* The interpreter that the limit callback was
				 * attached to. This is not the interpreter
				 * that the callback runs in! */
    long type;			/* The type of callback that this is. */
} ScriptLimitCallbackKey;

/*
 * TIP#143 limit handler internal representation.
 */

struct LimitHandler {
    int flags;			/* The state of this particular handler. */
    Tcl_LimitHandlerProc *handlerProc;
				/* The handler callback. */
    ClientData clientData;	/* Opaque argument to the handler callback. */
    Tcl_LimitHandlerDeleteProc *deleteProc;
				/* How to delete the clientData. */
    LimitHandler *prevPtr;	/* Previous item in linked list of
				 * handlers. */
    LimitHandler *nextPtr;	/* Next item in linked list of handlers. */
};

/*
 * Values for the LimitHandler flags field.
 *      LIMIT_HANDLER_ACTIVE - Whether the handler is currently being
 *              processed; handlers are never to be reentered.
 *      LIMIT_HANDLER_DELETED - Whether the handler has been deleted. This
 *              should not normally be observed because when a handler is
 *              deleted it is also spliced out of the list of handlers, but
 *              even so we will be careful.
 */

#define LIMIT_HANDLER_ACTIVE    0x01
#define LIMIT_HANDLER_DELETED   0x02



/*
 * Prototypes for local static functions:
 */

static int		AliasCreate(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, Tcl_Interp *parentInterp,
			    Tcl_Obj *namePtr, Tcl_Obj *targetPtr, int objc,
			    Tcl_Obj *const objv[]);
static int		AliasDelete(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, Tcl_Obj *namePtr);
static int		AliasDescribe(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, Tcl_Obj *objPtr);
static int		AliasList(Tcl_Interp *interp, Tcl_Interp *childInterp);
static int		AliasObjCmd(ClientData dummy,
			    Tcl_Interp *currentInterp, int objc,
			    Tcl_Obj *const objv[]);
static Tcl_ObjCmdProc		AliasNRCmd;
static Tcl_CmdDeleteProc	AliasObjCmdDeleteProc;
static Tcl_Interp *	GetInterp(Tcl_Interp *interp, Tcl_Obj *pathPtr);
static Tcl_Interp *	GetInterp2(Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static Tcl_InterpDeleteProc	InterpInfoDeleteProc;
static int		ChildBgerror(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, int objc,
			    Tcl_Obj *const objv[]);
static Tcl_Interp *	ChildCreate(Tcl_Interp *interp, Tcl_Obj *pathPtr,
			    int safe);
static int		ChildDebugCmd(Tcl_Interp *interp,
			    Tcl_Interp *childInterp,
			    int objc, Tcl_Obj *const objv[]);
static int		ChildEval(Tcl_Interp *interp, Tcl_Interp *childInterp,
			    int objc, Tcl_Obj *const objv[]);
static int		ChildExpose(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, int objc,
			    Tcl_Obj *const objv[]);
static int		ChildHide(Tcl_Interp *interp, Tcl_Interp *childInterp,
			    int objc, Tcl_Obj *const objv[]);
static int		ChildHidden(Tcl_Interp *interp,
			    Tcl_Interp *childInterp);
static int		ChildInvokeHidden(Tcl_Interp *interp,
			    Tcl_Interp *childInterp,
			    const char *namespaceName,
			    int objc, Tcl_Obj *const objv[]);
static int		ChildMarkTrusted(Tcl_Interp *interp,
			    Tcl_Interp *childInterp);
static int		ChildObjCmd(ClientData dummy, Tcl_Interp *interp,
			    int objc, Tcl_Obj *const objv[]);
static Tcl_CmdDeleteProc	ChildObjCmdDeleteProc;
static int		ChildRecursionLimit(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, int objc,
			    Tcl_Obj *const objv[]);
static int		ChildCommandLimitCmd(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, int consumedObjc,
			    int objc, Tcl_Obj *const objv[]);
static int		ChildTimeLimitCmd(Tcl_Interp *interp,
			    Tcl_Interp *childInterp, int consumedObjc,
			    int objc, Tcl_Obj *const objv[]);
static void		InheritLimitsFromParent(Tcl_Interp *childInterp,
			    Tcl_Interp *parentInterp);
static void		SetScriptLimitCallback(Tcl_Interp *interp, int type,
			    Tcl_Interp *targetInterp, Tcl_Obj *scriptObj);
static void		CallScriptLimitCallback(ClientData clientData,
			    Tcl_Interp *interp);
static void		DeleteScriptLimitCallback(ClientData clientData);
static void		RunLimitHandlers(LimitHandler *handlerPtr,
			    Tcl_Interp *interp);
static void		TimeLimitCallback(ClientData clientData);

/* NRE enabling */
static Tcl_NRPostProc	NRPostInvokeHidden;
static Tcl_ObjCmdProc	NRInterpCmd;
static Tcl_ObjCmdProc	NRChildCmd;


/*
 *----------------------------------------------------------------------
 *
 * TclSetPreInitScript --
 *
 *	This routine is used to change the value of the internal variable,
 *	tclPreInitScript.
 *
 * Results:
 *	Returns the current value of tclPreInitScript.
 *
 * Side effects:
 *	Changes the way Tcl_Init() routine behaves.
 *
 *----------------------------------------------------------------------
 */

const char *
TclSetPreInitScript(
    const char *string)		/* Pointer to a script. */
{
    const char *prevString = tclPreInitScript;
    tclPreInitScript = string;
    return prevString;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Init --
 *
 *	This function is typically invoked by Tcl_AppInit functions to find
 *	and source the "init.tcl" script, which should exist somewhere on the
 *	Tcl library path.
 *
 * Results:
 *	Returns a standard Tcl completion code and sets the interp's result if
 *	there is an error.
 *
 * Side effects:
 *	Depends on what's in the init.tcl script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_Init(
    Tcl_Interp *interp)		/* Interpreter to initialize. */
{
    if (tclPreInitScript != NULL) {
	if (Tcl_Eval(interp, tclPreInitScript) == TCL_ERROR) {
	    return TCL_ERROR;
	}
    }

    /*
     * In order to find init.tcl during initialization, the following script
     * is invoked by Tcl_Init(). It looks in several different directories:
     *
     *	$tcl_library		- can specify a primary location, if set, no
     *				  other locations will be checked. This is the
     *				  recommended way for a program that embeds
     *				  Tcl to specifically tell Tcl where to find
     *				  an init.tcl file.
     *
     *	$env(TCL_LIBRARY)	- highest priority so user can always override
     *				  the search path unless the application has
     *				  specified an exact directory above
     *
     *	$tclDefaultLibrary	- INTERNAL: This variable is set by Tcl on
     *				  those platforms where it can determine at
     *				  runtime the directory where it expects the
     *				  init.tcl file to be. After [tclInit] reads
     *				  and uses this value, it [unset]s it.
     *				  External users of Tcl should not make use of
     *				  the variable to customize [tclInit].
     *
     *	$tcl_libPath		- OBSOLETE: This variable is no longer set by
     *				  Tcl itself, but [tclInit] examines it in
     *				  case some program that embeds Tcl is
     *				  customizing [tclInit] by setting this
     *				  variable to a list of directories in which
     *				  to search.
     *
     *	[tcl::pkgconfig get scriptdir,runtime]
     *				- the directory determined by configure to be
     *				  the place where Tcl's script library is to
     *				  be installed.
     *
     * The first directory on this path that contains a valid init.tcl script
     * will be set as the value of tcl_library.
     *
     * Note that this entire search mechanism can be bypassed by defining an
     * alternate tclInit command before calling Tcl_Init().
     */

    return Tcl_Eval(interp,
"if {[namespace which -command tclInit] eq \"\"} {\n"
"  proc tclInit {} {\n"
"    global tcl_libPath tcl_library env tclDefaultLibrary\n"
"    rename tclInit {}\n"
"    if {[info exists tcl_library]} {\n"
"	set scripts {{set tcl_library}}\n"
"    } else {\n"
"	set scripts {}\n"
"	if {[info exists env(TCL_LIBRARY)] && ($env(TCL_LIBRARY) ne {})} {\n"
"	    lappend scripts {set env(TCL_LIBRARY)}\n"
"	    lappend scripts {\n"
"if {[regexp ^tcl(.*)$ [file tail $env(TCL_LIBRARY)] -> tail] == 0} continue\n"
"if {$tail eq [info tclversion]} continue\n"
"file join [file dirname $env(TCL_LIBRARY)] tcl[info tclversion]}\n"
"	}\n"
"	if {[info exists tclDefaultLibrary]} {\n"
"	    lappend scripts {set tclDefaultLibrary}\n"
"	} else {\n"
"	    lappend scripts {::tcl::pkgconfig get scriptdir,runtime}\n"
"	}\n"
"	lappend scripts {\n"
"set parentDir [file dirname [file dirname [info nameofexecutable]]]\n"
"set grandParentDir [file dirname $parentDir]\n"
"file join $parentDir lib tcl[info tclversion]} \\\n"
"	{file join $grandParentDir lib tcl[info tclversion]} \\\n"
"	{file join $parentDir library} \\\n"
"	{file join $grandParentDir library} \\\n"
"	{file join $grandParentDir tcl[info patchlevel] library} \\\n"
"	{\n"
"file join [file dirname $grandParentDir] tcl[info patchlevel] library}\n"
"	if {[info exists tcl_libPath]\n"
"		&& [catch {llength $tcl_libPath} len] == 0} {\n"
"	    for {set i 0} {$i < $len} {incr i} {\n"
"		lappend scripts [list lindex \\$tcl_libPath $i]\n"
"	    }\n"
"	}\n"
"    }\n"
"    set dirs {}\n"
"    set errors {}\n"
"    foreach script $scripts {\n"
"	lappend dirs [eval $script]\n"
"	set tcl_library [lindex $dirs end]\n"
"	set tclfile [file join $tcl_library init.tcl]\n"
"	if {[file exists $tclfile]} {\n"
"	    if {[catch {uplevel #0 [list source $tclfile]} msg opts]} {\n"
"		append errors \"$tclfile: $msg\n\"\n"
"		append errors \"[dict get $opts -errorinfo]\n\"\n"
"		continue\n"
"	    }\n"
"	    unset -nocomplain tclDefaultLibrary\n"
"	    return\n"
"	}\n"
"    }\n"
"    unset -nocomplain tclDefaultLibrary\n"
"    set msg \"Can't find a usable init.tcl in the following directories: \n\"\n"
"    append msg \"    $dirs\n\n\"\n"
"    append msg \"$errors\n\n\"\n"
"    append msg \"This probably means that Tcl wasn't installed properly.\n\"\n"
"    error $msg\n"
"  }\n"
"}\n"
"tclInit");
}

/*
 *---------------------------------------------------------------------------
 *
 * TclInterpInit --
 *
 *	Initializes the invoking interpreter for using the parent, child and
 *	safe interp facilities. This is called from inside Tcl_CreateInterp().
 *
 * Results:
 *	Always returns TCL_OK for backwards compatibility.
 *
 * Side effects:
 *	Adds the "interp" command to an interpreter and initializes the
 *	interpInfoPtr field of the invoking interpreter.
 *
 *---------------------------------------------------------------------------
 */

int
TclInterpInit(
    Tcl_Interp *interp)		/* Interpreter to initialize. */
{
    InterpInfo *interpInfoPtr;
    Parent *parentPtr;
    Child *childPtr;

    interpInfoPtr = (InterpInfo *)ckalloc(sizeof(InterpInfo));
    ((Interp *) interp)->interpInfo = interpInfoPtr;

    parentPtr = &interpInfoPtr->parent;
    Tcl_InitHashTable(&parentPtr->childTable, TCL_STRING_KEYS);
    parentPtr->targetsPtr = NULL;

    childPtr = &interpInfoPtr->child;
    childPtr->parentInterp	= NULL;
    childPtr->childEntryPtr	= NULL;
    childPtr->childInterp	= interp;
    childPtr->interpCmd		= NULL;
    Tcl_InitHashTable(&childPtr->aliasTable, TCL_STRING_KEYS);

    Tcl_NRCreateCommand(interp, "interp", Tcl_InterpObjCmd, NRInterpCmd,
	    NULL, NULL);

    Tcl_CallWhenDeleted(interp, InterpInfoDeleteProc, NULL);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * InterpInfoDeleteProc --
 *
 *	Invoked when an interpreter is being deleted. It releases all storage
 *	used by the parent/child/safe interpreter facilities.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up storage. Sets the interpInfoPtr field of the interp to NULL.
 *
 *---------------------------------------------------------------------------
 */

static void
InterpInfoDeleteProc(
    ClientData clientData,	/* Ignored. */
    Tcl_Interp *interp)		/* Interp being deleted. All commands for
				 * child interps should already be deleted. */
{
    InterpInfo *interpInfoPtr;
    Child *childPtr;
    Parent *parentPtr;
    Target *targetPtr;

    interpInfoPtr = (InterpInfo *) ((Interp *) interp)->interpInfo;

    /*
     * There shouldn't be any commands left.
     */

    parentPtr = &interpInfoPtr->parent;
    if (parentPtr->childTable.numEntries != 0) {
	Tcl_Panic("InterpInfoDeleteProc: still exist commands");
    }
    Tcl_DeleteHashTable(&parentPtr->childTable);

    /*
     * Tell any interps that have aliases to this interp that they should
     * delete those aliases. If the other interp was already dead, it would
     * have removed the target record already.
     */

    for (targetPtr = parentPtr->targetsPtr; targetPtr != NULL; ) {
	Target *tmpPtr = targetPtr->nextPtr;
	Tcl_DeleteCommandFromToken(targetPtr->childInterp,
		targetPtr->childCmd);
	targetPtr = tmpPtr;
    }

    childPtr = &interpInfoPtr->child;
    if (childPtr->interpCmd != NULL) {
	/*
	 * Tcl_DeleteInterp() was called on this interpreter, rather "interp
	 * delete" or the equivalent deletion of the command in the parent.
	 * First ensure that the cleanup callback doesn't try to delete the
	 * interp again.
	 */

	childPtr->childInterp = NULL;
	Tcl_DeleteCommandFromToken(childPtr->parentInterp,
		childPtr->interpCmd);
    }

    /*
     * There shouldn't be any aliases left.
     */

    if (childPtr->aliasTable.numEntries != 0) {
	Tcl_Panic("InterpInfoDeleteProc: still exist aliases");
    }
    Tcl_DeleteHashTable(&childPtr->aliasTable);

    ckfree(interpInfoPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_InterpObjCmd --
 *
 *	This function is invoked to process the "interp" Tcl command. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_InterpObjCmd(
    ClientData clientData,		/* Unused. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const objv[])		/* Argument objects. */
{
    return Tcl_NRCallObjProc(interp, NRInterpCmd, clientData, objc, objv);
}

static int
NRInterpCmd(
    ClientData clientData,		/* Unused. */
    Tcl_Interp *interp,			/* Current interpreter. */
    int objc,				/* Number of arguments. */
    Tcl_Obj *const objv[])		/* Argument objects. */
{
    Tcl_Interp *childInterp;
    int index;
    static const char *const options[] = {
	"alias",	"aliases",	"bgerror",	"cancel",
	"children",	"create",	"debug",	"delete",
	"eval",		"exists",	"expose",
	"hide",		"hidden",	"issafe",
	"invokehidden",	"limit",	"marktrusted",	"recursionlimit",
	"slaves",	"share",	"target",	"transfer",
	NULL
    };
    enum interpOptionEnum {
	OPT_ALIAS,	OPT_ALIASES,	OPT_BGERROR,	OPT_CANCEL,
	OPT_CHILDREN,	OPT_CREATE,	OPT_DEBUG,	OPT_DELETE,
	OPT_EVAL,	OPT_EXISTS,	OPT_EXPOSE,
	OPT_HIDE,	OPT_HIDDEN,	OPT_ISSAFE,
	OPT_INVOKEHID,	OPT_LIMIT,	OPT_MARKTRUSTED,OPT_RECLIMIT,
	OPT_SLAVES,	OPT_SHARE,	OPT_TARGET,	OPT_TRANSFER
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "cmd ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }
    switch ((enum interpOptionEnum)index) {
    case OPT_ALIAS: {
	Tcl_Interp *parentInterp;

	if (objc < 4) {
	aliasArgs:
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "slavePath slaveCmd ?masterPath masterCmd? ?arg ...?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    return AliasDescribe(interp, childInterp, objv[3]);
	}
	if ((objc == 5) && (TclGetString(objv[4])[0] == '\0')) {
	    return AliasDelete(interp, childInterp, objv[3]);
	}
	if (objc > 5) {
	    parentInterp = GetInterp(interp, objv[4]);
	    if (parentInterp == NULL) {
		return TCL_ERROR;
	    }

	    return AliasCreate(interp, childInterp, parentInterp, objv[3],
		    objv[5], objc - 6, objv + 6);
	}
	goto aliasArgs;
    }
    case OPT_ALIASES:
	childInterp = GetInterp2(interp, objc, objv);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return AliasList(interp, childInterp);
    case OPT_BGERROR:
	if (objc != 3 && objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path ?cmdPrefix?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildBgerror(interp, childInterp, objc - 3, objv + 3);
    case OPT_CANCEL: {
	int i, flags;
	Tcl_Obj *resultObjPtr;
	static const char *const cancelOptions[] = {
	    "-unwind",	"--",	NULL
	};
	enum optionCancelEnum {
	    OPT_UNWIND,	OPT_LAST
	};

	flags = 0;

	for (i = 2; i < objc; i++) {
	    if (TclGetString(objv[i])[0] != '-') {
		break;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[i], cancelOptions, "option",
		    0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }

	    switch ((enum optionCancelEnum) index) {
	    case OPT_UNWIND:
		/*
		 * The evaluation stack in the target interp is to be unwound.
		 */

		flags |= TCL_CANCEL_UNWIND;
		break;
	    case OPT_LAST:
		i++;
		goto endOfForLoop;
	    }
	}

    endOfForLoop:
	if (i < objc - 2) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "?-unwind? ?--? ?path? ?result?");
	    return TCL_ERROR;
	}

	/*
	 * Did they specify a child interp to cancel the script in progress
	 * in?  If not, use the current interp.
	 */

	if (i < objc) {
	    childInterp = GetInterp(interp, objv[i]);
	    if (childInterp == NULL) {
		return TCL_ERROR;
	    }
	    i++;
	} else {
	    childInterp = interp;
	}

	if (i < objc) {
	    resultObjPtr = objv[i];

	    /*
	     * Tcl_CancelEval removes this reference.
	     */

	    Tcl_IncrRefCount(resultObjPtr);
	    i++;
	} else {
	    resultObjPtr = NULL;
	}

	return Tcl_CancelEval(childInterp, resultObjPtr, 0, flags);
    }
    case OPT_CREATE: {
	int i, last, safe;
	Tcl_Obj *childPtr;
	char buf[16 + TCL_INTEGER_SPACE];
	static const char *const createOptions[] = {
	    "-safe",	"--", NULL
	};
	enum option {
	    OPT_SAFE,	OPT_LAST
	};

	safe = Tcl_IsSafe(interp);

	/*
	 * Weird historical rules: "-safe" is accepted at the end, too.
	 */

	childPtr = NULL;
	last = 0;
	for (i = 2; i < objc; i++) {
	    if ((last == 0) && (TclGetString(objv[i])[0] == '-')) {
		if (Tcl_GetIndexFromObj(interp, objv[i], createOptions,
			"option", 0, &index) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (index == OPT_SAFE) {
		    safe = 1;
		    continue;
		}
		i++;
		last = 1;
	    }
	    if (childPtr != NULL) {
		Tcl_WrongNumArgs(interp, 2, objv, "?-safe? ?--? ?path?");
		return TCL_ERROR;
	    }
	    if (i < objc) {
		childPtr = objv[i];
	    }
	}
	buf[0] = '\0';
	if (childPtr == NULL) {
	    /*
	     * Create an anonymous interpreter -- we choose its name and the
	     * name of the command. We check that the command name that we use
	     * for the interpreter does not collide with an existing command
	     * in the parent interpreter.
	     */

	    for (i = 0; ; i++) {
		Tcl_CmdInfo cmdInfo;

		snprintf(buf, sizeof(buf), "interp%d", i);
		if (Tcl_GetCommandInfo(interp, buf, &cmdInfo) == 0) {
		    break;
		}
	    }
	    childPtr = Tcl_NewStringObj(buf, -1);
	}
	if (ChildCreate(interp, childPtr, safe) == NULL) {
	    if (buf[0] != '\0') {
		Tcl_DecrRefCount(childPtr);
	    }
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, childPtr);
	return TCL_OK;
    }
    case OPT_DEBUG:		/* TIP #378 */
	/*
	 * Currently only -frame supported, otherwise ?-option ?value??
	 */

	if (objc < 3 || objc > 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path ?-frame ?bool??");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildDebugCmd(interp, childInterp, objc - 3, objv + 3);
    case OPT_DELETE: {
	int i;
	InterpInfo *iiPtr;

	for (i = 2; i < objc; i++) {
	    childInterp = GetInterp(interp, objv[i]);
	    if (childInterp == NULL) {
		return TCL_ERROR;
	    } else if (childInterp == interp) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"cannot delete the current interpreter", -1));
		Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			"DELETESELF", (char *)NULL);
		return TCL_ERROR;
	    }
	    iiPtr = (InterpInfo *) ((Interp *) childInterp)->interpInfo;
	    Tcl_DeleteCommandFromToken(iiPtr->child.parentInterp,
		    iiPtr->child.interpCmd);
	}
	return TCL_OK;
    }
    case OPT_EVAL:
	if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path arg ?arg ...?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildEval(interp, childInterp, objc - 3, objv + 3);
    case OPT_EXISTS: {
	int exists = 1;

	childInterp = GetInterp2(interp, objc, objv);
	if (childInterp == NULL) {
	    if (objc > 3) {
		return TCL_ERROR;
	    }
	    Tcl_ResetResult(interp);
	    exists = 0;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(exists));
	return TCL_OK;
    }
    case OPT_EXPOSE:
	if ((objc < 4) || (objc > 5)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path hiddenCmdName ?cmdName?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildExpose(interp, childInterp, objc - 3, objv + 3);
    case OPT_HIDE:
	if ((objc < 4) || (objc > 5)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path cmdName ?hiddenCmdName?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildHide(interp, childInterp, objc - 3, objv + 3);
    case OPT_HIDDEN:
	childInterp = GetInterp2(interp, objc, objv);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildHidden(interp, childInterp);
    case OPT_ISSAFE:
	childInterp = GetInterp2(interp, objc, objv);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(Tcl_IsSafe(childInterp)));
	return TCL_OK;
    case OPT_INVOKEHID: {
	int i;
	const char *namespaceName;
	static const char *const hiddenOptions[] = {
	    "-global",	"-namespace",	"--", NULL
	};
	enum hiddenOption {
	    OPT_GLOBAL,	OPT_NAMESPACE,	OPT_LAST
	};

	namespaceName = NULL;
	for (i = 3; i < objc; i++) {
	    if (TclGetString(objv[i])[0] != '-') {
		break;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[i], hiddenOptions, "option",
		    0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (index == OPT_GLOBAL) {
		namespaceName = "::";
	    } else if (index == OPT_NAMESPACE) {
		if (++i == objc) { /* There must be more arguments. */
		    break;
		} else {
		    namespaceName = TclGetString(objv[i]);
		}
	    } else {
		i++;
		break;
	    }
	}
	if (objc - i < 1) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "path ?-namespace ns? ?-global? ?--? cmd ?arg ..?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildInvokeHidden(interp, childInterp, namespaceName, objc - i,
		objv + i);
    }
    case OPT_LIMIT: {
	static const char *const limitTypes[] = {
	    "commands", "time", NULL
	};
	enum LimitTypes {
	    LIMIT_TYPE_COMMANDS, LIMIT_TYPE_TIME
	};
	int limitType;

	if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "path limitType ?-option value ...?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[3], limitTypes, "limit type", 0,
		&limitType) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch ((enum LimitTypes) limitType) {
	case LIMIT_TYPE_COMMANDS:
	    return ChildCommandLimitCmd(interp, childInterp, 4, objc,objv);
	case LIMIT_TYPE_TIME:
	    return ChildTimeLimitCmd(interp, childInterp, 4, objc, objv);
	}
    }
    break;
    case OPT_MARKTRUSTED:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildMarkTrusted(interp, childInterp);
    case OPT_RECLIMIT:
	if (objc != 3 && objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path ?newlimit?");
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	return ChildRecursionLimit(interp, childInterp, objc - 3, objv + 3);
    case OPT_CHILDREN:
    case OPT_SLAVES: {
	InterpInfo *iiPtr;
	Tcl_Obj *resultPtr;
	Tcl_HashEntry *hPtr;
	Tcl_HashSearch hashSearch;
	char *string;

	childInterp = GetInterp2(interp, objc, objv);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	iiPtr = (InterpInfo *) ((Interp *) childInterp)->interpInfo;
	TclNewObj(resultPtr);
	hPtr = Tcl_FirstHashEntry(&iiPtr->parent.childTable, &hashSearch);
	for ( ; hPtr != NULL; hPtr = Tcl_NextHashEntry(&hashSearch)) {
	    string = (char *)Tcl_GetHashKey(&iiPtr->parent.childTable, hPtr);
	    Tcl_ListObjAppendElement(NULL, resultPtr,
		    Tcl_NewStringObj(string, -1));
	}
	Tcl_SetObjResult(interp, resultPtr);
	return TCL_OK;
    }
    case OPT_TRANSFER:
    case OPT_SHARE: {
	Tcl_Interp *parentInterp;	/* The parent of the child. */
	Tcl_Channel chan;

	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "srcPath channelId destPath");
	    return TCL_ERROR;
	}
	parentInterp = GetInterp(interp, objv[2]);
	if (parentInterp == NULL) {
	    return TCL_ERROR;
	}
	chan = Tcl_GetChannel(parentInterp, TclGetString(objv[3]), NULL);
	if (chan == NULL) {
	    Tcl_TransferResult(parentInterp, TCL_OK, interp);
	    return TCL_ERROR;
	}
	childInterp = GetInterp(interp, objv[4]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}
	Tcl_RegisterChannel(childInterp, chan);
	if (index == OPT_TRANSFER) {
	    /*
	     * When transferring, as opposed to sharing, we must unhitch the
	     * channel from the interpreter where it started.
	     */

	    if (Tcl_UnregisterChannel(parentInterp, chan) != TCL_OK) {
		Tcl_TransferResult(parentInterp, TCL_OK, interp);
		return TCL_ERROR;
	    }
	}
	return TCL_OK;
    }
    case OPT_TARGET: {
	InterpInfo *iiPtr;
	Tcl_HashEntry *hPtr;
	Alias *aliasPtr;
	const char *aliasName;

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "path alias");
	    return TCL_ERROR;
	}

	childInterp = GetInterp(interp, objv[2]);
	if (childInterp == NULL) {
	    return TCL_ERROR;
	}

	aliasName = TclGetString(objv[3]);

	iiPtr = (InterpInfo *) ((Interp *) childInterp)->interpInfo;
	hPtr = Tcl_FindHashEntry(&iiPtr->child.aliasTable, aliasName);
	if (hPtr == NULL) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "alias \"%s\" in path \"%s\" not found",
		    aliasName, TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "ALIAS", aliasName,
		    (char *)NULL);
	    return TCL_ERROR;
	}
	aliasPtr = (Alias *)Tcl_GetHashValue(hPtr);
	if (Tcl_GetInterpPath(interp, aliasPtr->targetInterp) != TCL_OK) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "target interpreter for alias \"%s\" in path \"%s\" is "
		    "not my descendant", aliasName, TclGetString(objv[2])));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
		    "TARGETSHROUDED", (char *)NULL);
	    return TCL_ERROR;
	}
	return TCL_OK;
    }
    }
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * GetInterp2 --
 *
 *	Helper function for Tcl_InterpObjCmd() to convert the interp name
 *	potentially specified on the command line to an Tcl_Interp.
 *
 * Results:
 *	The return value is the interp specified on the command line, or the
 *	interp argument itself if no interp was specified on the command line.
 *	If the interp could not be found or the wrong number of arguments was
 *	specified on the command line, the return value is NULL and an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------------
 */

static Tcl_Interp *
GetInterp2(
    Tcl_Interp *interp,		/* Default interp if no interp was specified
				 * on the command line. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc == 2) {
	return interp;
    } else if (objc == 3) {
	return GetInterp(interp, objv[2]);
    } else {
	Tcl_WrongNumArgs(interp, 2, objv, "?path?");
	return NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateAlias --
 *
 *	Creates an alias between two interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates a new alias, manipulates the result field of childInterp.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CreateAlias(
    Tcl_Interp *childInterp,	/* Interpreter for source command. */
    const char *childCmd,	/* Command to install in child. */
    Tcl_Interp *targetInterp,	/* Interpreter for target command. */
    const char *targetCmd,	/* Name of target command. */
    int argc,			/* How many additional arguments? */
    const char *const *argv)	/* These are the additional args. */
{
    Tcl_Obj *childObjPtr, *targetObjPtr;
    Tcl_Obj **objv;
    int i;
    int result;

    objv = (Tcl_Obj **)TclStackAlloc(childInterp, sizeof(Tcl_Obj *) * argc);
    for (i = 0; i < argc; i++) {
	objv[i] = Tcl_NewStringObj(argv[i], -1);
	Tcl_IncrRefCount(objv[i]);
    }

    childObjPtr = Tcl_NewStringObj(childCmd, -1);
    Tcl_IncrRefCount(childObjPtr);

    targetObjPtr = Tcl_NewStringObj(targetCmd, -1);
    Tcl_IncrRefCount(targetObjPtr);

    result = AliasCreate(childInterp, childInterp, targetInterp, childObjPtr,
	    targetObjPtr, argc, objv);

    for (i = 0; i < argc; i++) {
	Tcl_DecrRefCount(objv[i]);
    }
    TclStackFree(childInterp, objv);
    Tcl_DecrRefCount(targetObjPtr);
    Tcl_DecrRefCount(childObjPtr);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateAliasObj --
 *
 *	Object version: Creates an alias between two interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates a new alias.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_CreateAliasObj(
    Tcl_Interp *childInterp,	/* Interpreter for source command. */
    const char *childCmd,	/* Command to install in child. */
    Tcl_Interp *targetInterp,	/* Interpreter for target command. */
    const char *targetCmd,	/* Name of target command. */
    int objc,			/* How many additional arguments? */
    Tcl_Obj *const objv[])	/* Argument vector. */
{
    Tcl_Obj *childObjPtr, *targetObjPtr;
    int result;

    childObjPtr = Tcl_NewStringObj(childCmd, -1);
    Tcl_IncrRefCount(childObjPtr);

    targetObjPtr = Tcl_NewStringObj(targetCmd, -1);
    Tcl_IncrRefCount(targetObjPtr);

    result = AliasCreate(childInterp, childInterp, targetInterp, childObjPtr,
	    targetObjPtr, objc, objv);

    Tcl_DecrRefCount(childObjPtr);
    Tcl_DecrRefCount(targetObjPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAlias --
 *
 *	Gets information about an alias.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetAlias(
    Tcl_Interp *interp,		/* Interp to start search from. */
    const char *aliasName,	/* Name of alias to find. */
    Tcl_Interp **targetInterpPtr,
				/* (Return) target interpreter. */
    const char **targetCmdPtr,	/* (Return) name of target command. */
    int *argcPtr,		/* (Return) count of addnl args. */
    const char ***argvPtr)	/* (Return) additional arguments. */
{
    InterpInfo *iiPtr = (InterpInfo *) ((Interp *) interp)->interpInfo;
    Tcl_HashEntry *hPtr;
    Alias *aliasPtr;
    int i, objc;
    Tcl_Obj **objv;

    hPtr = Tcl_FindHashEntry(&iiPtr->child.aliasTable, aliasName);
    if (hPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"alias \"%s\" not found", aliasName));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "ALIAS", aliasName, (char *)NULL);
	return TCL_ERROR;
    }
    aliasPtr = (Alias *)Tcl_GetHashValue(hPtr);
    objc = aliasPtr->objc;
    objv = &aliasPtr->objPtr;

    if (targetInterpPtr != NULL) {
	*targetInterpPtr = aliasPtr->targetInterp;
    }
    if (targetCmdPtr != NULL) {
	*targetCmdPtr = TclGetString(objv[0]);
    }
    if (argcPtr != NULL) {
	*argcPtr = objc - 1;
    }
    if (argvPtr != NULL) {
	*argvPtr = (const char **)
		ckalloc(sizeof(const char *) * (objc - 1));
	for (i = 1; i < objc; i++) {
	    (*argvPtr)[i - 1] = TclGetString(objv[i]);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetAliasObj --
 *
 *	Object version: Gets information about an alias.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetAliasObj(
    Tcl_Interp *interp,		/* Interp to start search from. */
    const char *aliasName,	/* Name of alias to find. */
    Tcl_Interp **targetInterpPtr,
				/* (Return) target interpreter. */
    const char **targetCmdPtr,	/* (Return) name of target command. */
    int *objcPtr,		/* (Return) count of addnl args. */
    Tcl_Obj ***objvPtr)		/* (Return) additional args. */
{
    InterpInfo *iiPtr = (InterpInfo *) ((Interp *) interp)->interpInfo;
    Tcl_HashEntry *hPtr;
    Alias *aliasPtr;
    int objc;
    Tcl_Obj **objv;

    hPtr = Tcl_FindHashEntry(&iiPtr->child.aliasTable, aliasName);
    if (hPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"alias \"%s\" not found", aliasName));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "ALIAS", aliasName, (char *)NULL);
	return TCL_ERROR;
    }
    aliasPtr = (Alias *)Tcl_GetHashValue(hPtr);
    objc = aliasPtr->objc;
    objv = &aliasPtr->objPtr;

    if (targetInterpPtr != NULL) {
	*targetInterpPtr = aliasPtr->targetInterp;
    }
    if (targetCmdPtr != NULL) {
	*targetCmdPtr = TclGetString(objv[0]);
    }
    if (objcPtr != NULL) {
	*objcPtr = objc - 1;
    }
    if (objvPtr != NULL) {
	*objvPtr = objv + 1;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclPreventAliasLoop --
 *
 *	When defining an alias or renaming a command, prevent an alias loop
 *	from being formed.
 *
 * Results:
 *	A standard Tcl object result.
 *
 * Side effects:
 *	If TCL_ERROR is returned, the function also stores an error message in
 *	the interpreter's result object.
 *
 * NOTE:
 *	This function is public internal (instead of being static to this
 *	file) because it is also used from TclRenameCommand.
 *
 *----------------------------------------------------------------------
 */

int
TclPreventAliasLoop(
    Tcl_Interp *interp,		/* Interp in which to report errors. */
    Tcl_Interp *cmdInterp,	/* Interp in which the command is being
				 * defined. */
    Tcl_Command cmd)		/* Tcl command we are attempting to define. */
{
    Command *cmdPtr = (Command *) cmd;
    Alias *aliasPtr, *nextAliasPtr;
    Tcl_Command aliasCmd;
    Command *aliasCmdPtr;

    /*
     * If we are not creating or renaming an alias, then it is always OK to
     * create or rename the command.
     */

    if (cmdPtr->objProc != AliasObjCmd) {
	return TCL_OK;
    }

    /*
     * OK, we are dealing with an alias, so traverse the chain of aliases. If
     * we encounter the alias we are defining (or renaming to) any in the
     * chain then we have a loop.
     */

    aliasPtr = (Alias *)cmdPtr->objClientData;
    nextAliasPtr = aliasPtr;
    while (1) {
	Tcl_Obj *cmdNamePtr;

	/*
	 * If the target of the next alias in the chain is the same as the
	 * source alias, we have a loop.
	 */

	if (Tcl_InterpDeleted(nextAliasPtr->targetInterp)) {
	    /*
	     * The child interpreter can be deleted while creating the alias.
	     * [Bug #641195]
	     */

	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "cannot define or rename alias \"%s\": interpreter deleted",
		    Tcl_GetCommandName(cmdInterp, cmd)));
	    return TCL_ERROR;
	}
	cmdNamePtr = nextAliasPtr->objPtr;
	aliasCmd = Tcl_FindCommand(nextAliasPtr->targetInterp,
		TclGetString(cmdNamePtr),
		Tcl_GetGlobalNamespace(nextAliasPtr->targetInterp),
		/*flags*/ 0);
	if (aliasCmd == NULL) {
	    return TCL_OK;
	}
	aliasCmdPtr = (Command *) aliasCmd;
	if (aliasCmdPtr == cmdPtr) {
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		    "cannot define or rename alias \"%s\": would create a loop",
		    Tcl_GetCommandName(cmdInterp, cmd)));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
		    "ALIASLOOP", (char *)NULL);
	    return TCL_ERROR;
	}

	/*
	 * Otherwise, follow the chain one step further. See if the target
	 * command is an alias - if so, follow the loop to its target command.
	 * Otherwise we do not have a loop.
	 */

	if (aliasCmdPtr->objProc != AliasObjCmd) {
	    return TCL_OK;
	}
	nextAliasPtr = (Alias *)aliasCmdPtr->objClientData;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AliasCreate --
 *
 *	Helper function to do the work to actually create an alias.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	An alias command is created and entered into the alias table for the
 *	child interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
AliasCreate(
    Tcl_Interp *interp,		/* Interp for error reporting. */
    Tcl_Interp *childInterp,	/* Interp where alias cmd will live or from
				 * which alias will be deleted. */
    Tcl_Interp *parentInterp,	/* Interp in which target command will be
				 * invoked. */
    Tcl_Obj *namePtr,		/* Name of alias cmd. */
    Tcl_Obj *targetCmdPtr,	/* Name of target cmd. */
    int objc,			/* Additional arguments to store */
    Tcl_Obj *const objv[])	/* with alias. */
{
    Alias *aliasPtr;
    Tcl_HashEntry *hPtr;
    Target *targetPtr;
    Child *childPtr;
    Parent *parentPtr;
    Tcl_Obj **prefv;
    int isNew, i;

    aliasPtr = (Alias *)ckalloc(sizeof(Alias) + objc * sizeof(Tcl_Obj *));
    aliasPtr->token = namePtr;
    Tcl_IncrRefCount(aliasPtr->token);
    aliasPtr->targetInterp = parentInterp;

    aliasPtr->objc = objc + 1;
    prefv = &aliasPtr->objPtr;

    *prefv = targetCmdPtr;
    Tcl_IncrRefCount(targetCmdPtr);
    for (i = 0; i < objc; i++) {
	*(++prefv) = objv[i];
	Tcl_IncrRefCount(objv[i]);
    }

    Tcl_Preserve(childInterp);
    Tcl_Preserve(parentInterp);

    if (childInterp == parentInterp) {
	aliasPtr->childCmd = Tcl_NRCreateCommand(childInterp,
		TclGetString(namePtr), AliasObjCmd, AliasNRCmd, aliasPtr,
		AliasObjCmdDeleteProc);
    } else {
    aliasPtr->childCmd = Tcl_CreateObjCommand(childInterp,
	    TclGetString(namePtr), AliasObjCmd, aliasPtr,
	    AliasObjCmdDeleteProc);
    }

    if (TclPreventAliasLoop(interp, childInterp,
	    aliasPtr->childCmd) != TCL_OK) {
	/*
	 * Found an alias loop! The last call to Tcl_CreateObjCommand made the
	 * alias point to itself. Delete the command and its alias record. Be
	 * careful to wipe out its client data first, so the command doesn't
	 * try to delete itself.
	 */

	Command *cmdPtr;

	Tcl_DecrRefCount(aliasPtr->token);
	Tcl_DecrRefCount(targetCmdPtr);
	for (i = 0; i < objc; i++) {
	    Tcl_DecrRefCount(objv[i]);
	}

	cmdPtr = (Command *) aliasPtr->childCmd;
	cmdPtr->clientData = NULL;
	cmdPtr->deleteProc = NULL;
	cmdPtr->deleteData = NULL;
	Tcl_DeleteCommandFromToken(childInterp, aliasPtr->childCmd);

	ckfree(aliasPtr);

	/*
	 * The result was already set by TclPreventAliasLoop.
	 */

	Tcl_Release(childInterp);
	Tcl_Release(parentInterp);
	return TCL_ERROR;
    }

    /*
     * Make an entry in the alias table. If it already exists, retry.
     */

    childPtr = &((InterpInfo *) ((Interp *) childInterp)->interpInfo)->child;
    while (1) {
	Tcl_Obj *newToken;
	const char *string;

	string = TclGetString(aliasPtr->token);
	hPtr = Tcl_CreateHashEntry(&childPtr->aliasTable, string, &isNew);
	if (isNew != 0) {
	    break;
	}

	/*
	 * The alias name cannot be used as unique token, it is already taken.
	 * We can produce a unique token by prepending "::" repeatedly. This
	 * algorithm is a stop-gap to try to maintain the command name as
	 * token for most use cases, fearful of possible backwards compat
	 * problems. A better algorithm would produce unique tokens that need
	 * not be related to the command name.
	 *
	 * ATTENTION: the tests in interp.test and possibly safe.test depend
	 * on the precise definition of these tokens.
	 */

	TclNewLiteralStringObj(newToken, "::");
	Tcl_AppendObjToObj(newToken, aliasPtr->token);
	Tcl_DecrRefCount(aliasPtr->token);
	aliasPtr->token = newToken;
	Tcl_IncrRefCount(aliasPtr->token);
    }

    aliasPtr->aliasEntryPtr = hPtr;
    Tcl_SetHashValue(hPtr, aliasPtr);

    /*
     * Create the new command. We must do it after deleting any old command,
     * because the alias may be pointing at a renamed alias, as in:
     *
     * interp alias {} foo {} bar		# Create an alias "foo"
     * rename foo zop				# Now rename the alias
     * interp alias {} foo {} zop		# Now recreate "foo"...
     */

    targetPtr = (Target *)ckalloc(sizeof(Target));
    targetPtr->childCmd = aliasPtr->childCmd;
    targetPtr->childInterp = childInterp;

    parentPtr = &((InterpInfo*) ((Interp*) parentInterp)->interpInfo)->parent;
    targetPtr->nextPtr = parentPtr->targetsPtr;
    targetPtr->prevPtr = NULL;
    if (parentPtr->targetsPtr != NULL) {
	parentPtr->targetsPtr->prevPtr = targetPtr;
    }
    parentPtr->targetsPtr = targetPtr;
    aliasPtr->targetPtr = targetPtr;

    Tcl_SetObjResult(interp, aliasPtr->token);

    Tcl_Release(childInterp);
    Tcl_Release(parentInterp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasDelete --
 *
 *	Deletes the given alias from the child interpreter given.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Deletes the alias from the child interpreter.
 *
 *----------------------------------------------------------------------
 */

static int
AliasDelete(
    Tcl_Interp *interp,		/* Interpreter for result & errors. */
    Tcl_Interp *childInterp,	/* Interpreter containing alias. */
    Tcl_Obj *namePtr)		/* Name of alias to delete. */
{
    Child *childPtr;
    Alias *aliasPtr;
    Tcl_HashEntry *hPtr;

    /*
     * If the alias has been renamed in the child, the parent can still use
     * the original name (with which it was created) to find the alias to
     * delete it.
     */

    childPtr = &((InterpInfo *) ((Interp *) childInterp)->interpInfo)->child;
    hPtr = Tcl_FindHashEntry(&childPtr->aliasTable, TclGetString(namePtr));
    if (hPtr == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"alias \"%s\" not found", TclGetString(namePtr)));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "ALIAS",
		TclGetString(namePtr), (char *)NULL);
	return TCL_ERROR;
    }
    aliasPtr = (Alias *)Tcl_GetHashValue(hPtr);
    Tcl_DeleteCommandFromToken(childInterp, aliasPtr->childCmd);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasDescribe --
 *
 *	Sets the interpreter's result object to a Tcl list describing the
 *	given alias in the given interpreter: its target command and the
 *	additional arguments to prepend to any invocation of the alias.
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
AliasDescribe(
    Tcl_Interp *interp,		/* Interpreter for result & errors. */
    Tcl_Interp *childInterp,	/* Interpreter containing alias. */
    Tcl_Obj *namePtr)		/* Name of alias to describe. */
{
    Child *childPtr;
    Tcl_HashEntry *hPtr;
    Alias *aliasPtr;
    Tcl_Obj *prefixPtr;

    /*
     * If the alias has been renamed in the child, the parent can still use
     * the original name (with which it was created) to find the alias to
     * describe it.
     */

    childPtr = &((InterpInfo *) ((Interp *) childInterp)->interpInfo)->child;
    hPtr = Tcl_FindHashEntry(&childPtr->aliasTable, TclGetString(namePtr));
    if (hPtr == NULL) {
	return TCL_OK;
    }
    aliasPtr = (Alias *)Tcl_GetHashValue(hPtr);
    prefixPtr = Tcl_NewListObj(aliasPtr->objc, &aliasPtr->objPtr);
    Tcl_SetObjResult(interp, prefixPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasList --
 *
 *	Computes a list of aliases defined in a child interpreter.
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
AliasList(
    Tcl_Interp *interp,		/* Interp for data return. */
    Tcl_Interp *childInterp)	/* Interp whose aliases to compute. */
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch hashSearch;
    Tcl_Obj *resultPtr;
    Alias *aliasPtr;
    Child *childPtr;

    TclNewObj(resultPtr);
    childPtr = &((InterpInfo *) ((Interp *) childInterp)->interpInfo)->child;

    entryPtr = Tcl_FirstHashEntry(&childPtr->aliasTable, &hashSearch);
    for ( ; entryPtr != NULL; entryPtr = Tcl_NextHashEntry(&hashSearch)) {
	aliasPtr = (Alias *)Tcl_GetHashValue(entryPtr);
	Tcl_ListObjAppendElement(NULL, resultPtr, aliasPtr->token);
    }
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AliasObjCmd --
 *
 *	This is the function that services invocations of aliases in a child
 *	interpreter. One such command exists for each alias. When invoked,
 *	this function redirects the invocation to the target command in the
 *	parent interpreter as designated by the Alias record associated with
 *	this command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Causes forwarding of the invocation; all possible side effects may
 *	occur as a result of invoking the command to which the invocation is
 *	forwarded.
 *
 *----------------------------------------------------------------------
 */

static int
AliasNRCmd(
    ClientData clientData,	/* Alias record. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument vector. */
{
    Alias *aliasPtr = (Alias *)clientData;
    int prefc, cmdc, i;
    Tcl_Obj **prefv, **cmdv;
    Tcl_Obj *listPtr;
    List *listRep;
    int flags = TCL_EVAL_INVOKE;

    /*
     * Append the arguments to the command prefix and invoke the command in
     * the target interp's global namespace.
     */

    prefc = aliasPtr->objc;
    prefv = &aliasPtr->objPtr;
    cmdc = prefc + objc - 1;

    listPtr = Tcl_NewListObj(cmdc, NULL);
    listRep = (List *)listPtr->internalRep.twoPtrValue.ptr1;
    listRep->elemCount = cmdc;
    cmdv = &listRep->elements;

    prefv = &aliasPtr->objPtr;
    memcpy(cmdv, prefv, prefc * sizeof(Tcl_Obj *));
    memcpy(cmdv+prefc, objv+1, (objc-1) * sizeof(Tcl_Obj *));

    for (i=0; i<cmdc; i++) {
	Tcl_IncrRefCount(cmdv[i]);
    }

    /*
     * Use the ensemble rewriting machinery to ensure correct error messages:
     * only the source command should show, not the full target prefix.
     */

    if (TclInitRewriteEnsemble(interp, 1, prefc, objv)) {
	TclNRAddCallback(interp, TclClearRootEnsemble, NULL, NULL, NULL, NULL);
    }
    TclSkipTailcall(interp);
    return Tcl_NREvalObj(interp, listPtr, flags);
}

static int
AliasObjCmd(
    ClientData clientData,	/* Alias record. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument vector. */
{
#define ALIAS_CMDV_PREALLOC 10
    Alias *aliasPtr = (Alias *)clientData;
    Tcl_Interp *targetInterp = aliasPtr->targetInterp;
    int result, prefc, cmdc, i;
    Tcl_Obj **prefv, **cmdv;
    Tcl_Obj *cmdArr[ALIAS_CMDV_PREALLOC];
    Interp *tPtr = (Interp *) targetInterp;
    int isRootEnsemble;

    /*
     * Append the arguments to the command prefix and invoke the command in
     * the target interp's global namespace.
     */

    prefc = aliasPtr->objc;
    prefv = &aliasPtr->objPtr;
    cmdc = prefc + objc - 1;
    if (cmdc <= ALIAS_CMDV_PREALLOC) {
	cmdv = cmdArr;
    } else {
	cmdv = (Tcl_Obj **)TclStackAlloc(interp, cmdc * sizeof(Tcl_Obj *));
    }

    memcpy(cmdv, prefv, prefc * sizeof(Tcl_Obj *));
    memcpy(cmdv+prefc, objv+1, (objc-1) * sizeof(Tcl_Obj *));

    Tcl_ResetResult(targetInterp);

    for (i=0; i<cmdc; i++) {
	Tcl_IncrRefCount(cmdv[i]);
    }

    /*
     * Use the ensemble rewriting machinery to ensure correct error messages:
     * only the source command should show, not the full target prefix.
     */

    isRootEnsemble = TclInitRewriteEnsemble((Tcl_Interp *)tPtr, 1, prefc, objv);

    /*
     * Protect the target interpreter if it isn't the same as the source
     * interpreter so that we can continue to work with it after the target
     * command completes.
     */

    if (targetInterp != interp) {
	Tcl_Preserve(targetInterp);
    }

    /*
     * Execute the target command in the target interpreter.
     */

    result = Tcl_EvalObjv(targetInterp, cmdc, cmdv, TCL_EVAL_INVOKE);

    /*
     * Clean up the ensemble rewrite info if we set it in the first place.
     */

    if (isRootEnsemble) {
	TclResetRewriteEnsemble((Tcl_Interp *)tPtr, 1);
    }

    /*
     * If it was a cross-interpreter alias, we need to transfer the result
     * back to the source interpreter and release the lock we previously set
     * on the target interpreter.
     */

    if (targetInterp != interp) {
	Tcl_TransferResult(targetInterp, result, interp);
	Tcl_Release(targetInterp);
    }

    for (i=0; i<cmdc; i++) {
	Tcl_DecrRefCount(cmdv[i]);
    }
    if (cmdv != cmdArr) {
	TclStackFree(interp, cmdv);
    }
    return result;
#undef ALIAS_CMDV_PREALLOC
}

/*
 *----------------------------------------------------------------------
 *
 * AliasObjCmdDeleteProc --
 *
 *	Is invoked when an alias command is deleted in a child. Cleans up all
 *	storage associated with this alias.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the alias record and its entry in the alias table for the
 *	interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
AliasObjCmdDeleteProc(
    ClientData clientData)	/* The alias record for this alias. */
{
    Alias *aliasPtr = (Alias *)clientData;
    Target *targetPtr;
    int i;
    Tcl_Obj **objv;

    Tcl_DecrRefCount(aliasPtr->token);
    objv = &aliasPtr->objPtr;
    for (i = 0; i < aliasPtr->objc; i++) {
	Tcl_DecrRefCount(objv[i]);
    }
    Tcl_DeleteHashEntry(aliasPtr->aliasEntryPtr);

    /*
     * Splice the target record out of the target interpreter's parent list.
     */

    targetPtr = aliasPtr->targetPtr;
    if (targetPtr->prevPtr != NULL) {
	targetPtr->prevPtr->nextPtr = targetPtr->nextPtr;
    } else {
	Parent *parentPtr = &((InterpInfo *) ((Interp *)
		aliasPtr->targetInterp)->interpInfo)->parent;

	parentPtr->targetsPtr = targetPtr->nextPtr;
    }
    if (targetPtr->nextPtr != NULL) {
	targetPtr->nextPtr->prevPtr = targetPtr->prevPtr;
    }

    ckfree(targetPtr);
    ckfree(aliasPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CreateChild --
 *
 *	Creates a child interpreter. The childPath argument denotes the name
 *	of the new child relative to the current interpreter; the child is a
 *	direct descendant of the one-before-last component of the path,
 *	e.g. it is a descendant of the current interpreter if the childPath
 *	argument contains only one component. Optionally makes the child
 *	interpreter safe.
 *
 * Results:
 *	Returns the interpreter structure created, or NULL if an error
 *	occurred.
 *
 * Side effects:
 *	Creates a new interpreter and a new interpreter object command in the
 *	interpreter indicated by the childPath argument.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_CreateChild(
    Tcl_Interp *interp,		/* Interpreter to start search at. */
    const char *childPath,	/* Name of child to create. */
    int isSafe)			/* Should new child be "safe" ? */
{
    Tcl_Obj *pathPtr;
    Tcl_Interp *childInterp;

    pathPtr = Tcl_NewStringObj(childPath, -1);
    childInterp = ChildCreate(interp, pathPtr, isSafe);
    Tcl_DecrRefCount(pathPtr);

    return childInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetChild --
 *
 *	Finds a child interpreter by its path name.
 *
 * Results:
 *	Returns a Tcl_Interp * for the named interpreter or NULL if not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_GetChild(
    Tcl_Interp *interp,		/* Interpreter to start search from. */
    const char *childPath)	/* Path of child to find. */
{
    Tcl_Obj *pathPtr;
    Tcl_Interp *childInterp;

    pathPtr = Tcl_NewStringObj(childPath, -1);
    childInterp = GetInterp(interp, pathPtr);
    Tcl_DecrRefCount(pathPtr);

    return childInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetParent --
 *
 *	Finds the parent interpreter of a child interpreter.
 *
 * Results:
 *	Returns a Tcl_Interp * for the parent interpreter or NULL if none.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Interp *
Tcl_GetParent(
    Tcl_Interp *interp)		/* Get the parent of this interpreter. */
{
    Child *childPtr;		/* Child record of this interpreter. */

    if (interp == NULL) {
	return NULL;
    }
    childPtr = &((InterpInfo *) ((Interp *) interp)->interpInfo)->child;
    return childPtr->parentInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSetChildCancelFlags --
 *
 *	This function marks all child interpreters belonging to a given
 *	interpreter as being canceled or not canceled, depending on the
 *	provided flags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclSetChildCancelFlags(
    Tcl_Interp *interp,		/* Set cancel flags of this interpreter. */
    int flags,			/* Collection of OR-ed bits that control
				 * the cancellation of the script. Only
				 * TCL_CANCEL_UNWIND is currently
				 * supported. */
    int force)			/* Non-zero to ignore numLevels for the purpose
				 * of resetting the cancellation flags. */
{
    Parent *parentPtr;		/* Parent record of given interpreter. */
    Tcl_HashEntry *hPtr;	/* Search element. */
    Tcl_HashSearch hashSearch;	/* Search variable. */
    Child *childPtr;		/* Child record of interpreter. */
    Interp *iPtr;

    if (interp == NULL) {
	return;
    }

    flags &= (CANCELED | TCL_CANCEL_UNWIND);

    parentPtr = &((InterpInfo *) ((Interp *) interp)->interpInfo)->parent;

    hPtr = Tcl_FirstHashEntry(&parentPtr->childTable, &hashSearch);
    for ( ; hPtr != NULL; hPtr = Tcl_NextHashEntry(&hashSearch)) {
	childPtr = (Child *)Tcl_GetHashValue(hPtr);
	iPtr = (Interp *) childPtr->childInterp;

	if (iPtr == NULL) {
	    continue;
	}

	if (flags == 0) {
	    TclResetCancellation((Tcl_Interp *) iPtr, force);
	} else {
	    TclSetCancelFlags(iPtr, flags);
	}

	/*
	 * Now, recursively handle this for the children of this child
	 * interpreter.
	 */

	TclSetChildCancelFlags((Tcl_Interp *) iPtr, flags, force);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetInterpPath --
 *
 *	Sets the result of the asking interpreter to a proper Tcl list
 *	containing the names of interpreters between the asking and target
 *	interpreters. The target interpreter must be either the same as the
 *	asking interpreter or one of its children (including recursively).
 *
 * Results:
 *	TCL_OK if the target interpreter is the same as, or a descendant of,
 *	the asking interpreter; TCL_ERROR else. This way one can distinguish
 *	between the case where the asking and target interps are the same (an
 *	empty list is the result, and TCL_OK is returned) and when the target
 *	is not a descendant of the asking interpreter (in which case the Tcl
 *	result is an error message and the function returns TCL_ERROR).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetInterpPath(
    Tcl_Interp *interp,	/* Interpreter to start search from. */
    Tcl_Interp *targetInterp)	/* Interpreter to find. */
{
    InterpInfo *iiPtr;

    if (targetInterp == interp) {
	Tcl_SetObjResult(interp, Tcl_NewObj());
	return TCL_OK;
    }
    if (targetInterp == NULL) {
	return TCL_ERROR;
    }
    iiPtr = (InterpInfo *) ((Interp *) targetInterp)->interpInfo;
    if (Tcl_GetInterpPath(interp, iiPtr->child.parentInterp) != TCL_OK){
	return TCL_ERROR;
    }
    Tcl_ListObjAppendElement(NULL, Tcl_GetObjResult(interp),
	    Tcl_NewStringObj((const char *)Tcl_GetHashKey(&iiPtr->parent.childTable,
		    iiPtr->child.childEntryPtr), -1));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetInterp --
 *
 *	Helper function to find a child interpreter given a pathname.
 *
 * Results:
 *	Returns the child interpreter known by that name in the calling
 *	interpreter, or NULL if no interpreter known by that name exists.
 *
 * Side effects:
 *	Assigns to the pointer variable passed in, if not NULL.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Interp *
GetInterp(
    Tcl_Interp *interp,		/* Interp. to start search from. */
    Tcl_Obj *pathPtr)		/* List object containing name of interp. to
				 * be found. */
{
    Tcl_HashEntry *hPtr;	/* Search element. */
    Child *childPtr;		/* Interim child record. */
    Tcl_Obj **objv;
    int objc, i;
    Tcl_Interp *searchInterp;	/* Interim storage for interp. to find. */
    InterpInfo *parentInfoPtr;

    if (TclListObjGetElements(interp, pathPtr, &objc, &objv) != TCL_OK) {
	return NULL;
    }

    searchInterp = interp;
    for (i = 0; i < objc; i++) {
	parentInfoPtr = (InterpInfo *) ((Interp *) searchInterp)->interpInfo;
	hPtr = Tcl_FindHashEntry(&parentInfoPtr->parent.childTable,
		TclGetString(objv[i]));
	if (hPtr == NULL) {
	    searchInterp = NULL;
	    break;
	}
	childPtr = (Child *)Tcl_GetHashValue(hPtr);
	searchInterp = childPtr->childInterp;
	if (searchInterp == NULL) {
	    break;
	}
    }
    if (searchInterp == NULL) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"could not find interpreter \"%s\"", TclGetString(pathPtr)));
	Tcl_SetErrorCode(interp, "TCL", "LOOKUP", "INTERP",
		TclGetString(pathPtr), (char *)NULL);
    }
    return searchInterp;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildBgerror --
 *
 *	Helper function to set/query the background error handling command
 *	prefix of an interp
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	When (objc == 1), childInterp will be set to a new background handler
 *	of objv[0].
 *
 *----------------------------------------------------------------------
 */

static int
ChildBgerror(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* Interp in which limit is set/queried. */
    int objc,			/* Set or Query. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    if (objc) {
	int length;

	if (TCL_ERROR == TclListObjLength(NULL, objv[0], &length)
		|| (length < 1)) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "cmdPrefix must be list of length >= 1", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
		    "BGERRORFORMAT", (char *)NULL);
	    return TCL_ERROR;
	}
	TclSetBgErrorHandler(childInterp, objv[0]);
    }
    Tcl_SetObjResult(interp, TclGetBgErrorHandler(childInterp));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildCreate --
 *
 *	Helper function to do the actual work of creating a child interp and
 *	new object command. Also optionally makes the new child interpreter
 *	"safe".
 *
 * Results:
 *	Returns the new Tcl_Interp * if successful or NULL if not. If failed,
 *	the result of the invoking interpreter contains an error message.
 *
 * Side effects:
 *	Creates a new child interpreter and a new object command.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Interp *
ChildCreate(
    Tcl_Interp *interp,		/* Interp. to start search from. */
    Tcl_Obj *pathPtr,		/* Path (name) of child to create. */
    int safe)			/* Should we make it "safe"? */
{
    Tcl_Interp *parentInterp, *childInterp;
    Child *childPtr;
    InterpInfo *parentInfoPtr;
    Tcl_HashEntry *hPtr;
    const char *path;
    int isNew, objc;
    Tcl_Obj **objv;

    if (TclListObjGetElements(interp, pathPtr, &objc, &objv) != TCL_OK) {
	return NULL;
    }
    if (objc < 2) {
	parentInterp = interp;
	path = TclGetString(pathPtr);
    } else {
	Tcl_Obj *objPtr;

	objPtr = Tcl_NewListObj(objc - 1, objv);
	parentInterp = GetInterp(interp, objPtr);
	Tcl_DecrRefCount(objPtr);
	if (parentInterp == NULL) {
	    return NULL;
	}
	path = TclGetString(objv[objc - 1]);
    }
    if (safe == 0) {
	safe = Tcl_IsSafe(parentInterp);
    }

    parentInfoPtr = (InterpInfo *) ((Interp *) parentInterp)->interpInfo;
    hPtr = Tcl_CreateHashEntry(&parentInfoPtr->parent.childTable, path,
	    &isNew);
    if (isNew == 0) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf(
		"interpreter named \"%s\" already exists, cannot create",
		path));
	return NULL;
    }

    childInterp = Tcl_CreateInterp();
    childPtr = &((InterpInfo *) ((Interp *) childInterp)->interpInfo)->child;
    childPtr->parentInterp = parentInterp;
    childPtr->childEntryPtr = hPtr;
    childPtr->childInterp = childInterp;
    childPtr->interpCmd = Tcl_NRCreateCommand(parentInterp, path,
	    ChildObjCmd, NRChildCmd, childInterp, ChildObjCmdDeleteProc);
    Tcl_InitHashTable(&childPtr->aliasTable, TCL_STRING_KEYS);
    Tcl_SetHashValue(hPtr, childPtr);
    Tcl_SetVar(childInterp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);

    /*
     * Inherit the recursion limit.
     */

    ((Interp *) childInterp)->maxNestingDepth =
	    ((Interp *) parentInterp)->maxNestingDepth;

    if (safe) {
	if (Tcl_MakeSafe(childInterp) == TCL_ERROR) {
	    goto error;
	}
    } else {
	if (Tcl_Init(childInterp) == TCL_ERROR) {
	    goto error;
	}

	/*
	 * This will create the "memory" command in child interpreters if we
	 * compiled with TCL_MEM_DEBUG, otherwise it does nothing.
	 */

	Tcl_InitMemory(childInterp);
    }

    /*
     * Inherit the TIP#143 limits.
     */

    InheritLimitsFromParent(childInterp, parentInterp);

    /*
     * The [clock] command presents a safe API, but uses unsafe features in
     * its implementation. This means it has to be implemented in safe interps
     * as an alias to a version in the (trusted) parent.
     */

    if (safe) {
	Tcl_Obj *clockObj;
	int status;

	TclNewLiteralStringObj(clockObj, "clock");
	Tcl_IncrRefCount(clockObj);
	status = AliasCreate(interp, childInterp, parentInterp, clockObj,
		clockObj, 0, NULL);
	Tcl_DecrRefCount(clockObj);
	if (status != TCL_OK) {
	    goto error2;
	}
    }

    return childInterp;

  error:
    Tcl_TransferResult(childInterp, TCL_ERROR, interp);
  error2:
    Tcl_DeleteInterp(childInterp);

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildObjCmd --
 *
 *	Command to manipulate an interpreter, e.g. to send commands to it to
 *	be evaluated. One such command exists for each child interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See user documentation for details.
 *
 *----------------------------------------------------------------------
 */

static int
ChildObjCmd(
    ClientData clientData,	/* Child interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    return Tcl_NRCallObjProc(interp, NRChildCmd, clientData, objc, objv);
}

static int
NRChildCmd(
    ClientData clientData,	/* Child interpreter. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    Tcl_Interp *childInterp = (Tcl_Interp *)clientData;
    int index;
    static const char *const options[] = {
	"alias",	"aliases",	"bgerror",	"debug",
	"eval",		"expose",	"hide",		"hidden",
	"issafe",	"invokehidden",	"limit",	"marktrusted",
	"recursionlimit", NULL
    };
    enum childCmdOptionsEnum {
	OPT_ALIAS,	OPT_ALIASES,	OPT_BGERROR,	OPT_DEBUG,
	OPT_EVAL,	OPT_EXPOSE,	OPT_HIDE,	OPT_HIDDEN,
	OPT_ISSAFE,	OPT_INVOKEHIDDEN, OPT_LIMIT,	OPT_MARKTRUSTED,
	OPT_RECLIMIT
    };

    if (childInterp == NULL) {
	Tcl_Panic("ChildObjCmd: interpreter has been deleted");
    }

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "cmd ?arg ...?");
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum childCmdOptionsEnum) index) {
    case OPT_ALIAS:
	if (objc > 2) {
	    if (objc == 3) {
		return AliasDescribe(interp, childInterp, objv[2]);
	    }
	    if (TclGetString(objv[3])[0] == '\0') {
		if (objc == 4) {
		    return AliasDelete(interp, childInterp, objv[2]);
		}
	    } else {
		return AliasCreate(interp, childInterp, interp, objv[2],
			objv[3], objc - 4, objv + 4);
	    }
	}
	Tcl_WrongNumArgs(interp, 2, objv, "aliasName ?targetName? ?arg ...?");
	return TCL_ERROR;
    case OPT_ALIASES:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    return TCL_ERROR;
	}
	return AliasList(interp, childInterp);
    case OPT_BGERROR:
	if (objc != 2 && objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?cmdPrefix?");
	    return TCL_ERROR;
	}
	return ChildBgerror(interp, childInterp, objc - 2, objv + 2);
    case OPT_DEBUG:
	/*
	 * TIP #378
	 * Currently only -frame supported, otherwise ?-option ?value? ...?
	 */
	if (objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?-frame ?bool??");
	    return TCL_ERROR;
	}
	return ChildDebugCmd(interp, childInterp, objc - 2, objv + 2);
    case OPT_EVAL:
	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "arg ?arg ...?");
	    return TCL_ERROR;
	}
	return ChildEval(interp, childInterp, objc - 2, objv + 2);
    case OPT_EXPOSE:
	if ((objc < 3) || (objc > 4)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "hiddenCmdName ?cmdName?");
	    return TCL_ERROR;
	}
	return ChildExpose(interp, childInterp, objc - 2, objv + 2);
    case OPT_HIDE:
	if ((objc < 3) || (objc > 4)) {
	    Tcl_WrongNumArgs(interp, 2, objv, "cmdName ?hiddenCmdName?");
	    return TCL_ERROR;
	}
	return ChildHide(interp, childInterp, objc - 2, objv + 2);
    case OPT_HIDDEN:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    return TCL_ERROR;
	}
	return ChildHidden(interp, childInterp);
    case OPT_ISSAFE:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, Tcl_NewBooleanObj(Tcl_IsSafe(childInterp)));
	return TCL_OK;
    case OPT_INVOKEHIDDEN: {
	int i;
	const char *namespaceName;
	static const char *const hiddenOptions[] = {
	    "-global",	"-namespace",	"--", NULL
	};
	enum hiddenOption {
	    OPT_GLOBAL,	OPT_NAMESPACE,	OPT_LAST
	};

	namespaceName = NULL;
	for (i = 2; i < objc; i++) {
	    if (TclGetString(objv[i])[0] != '-') {
		break;
	    }
	    if (Tcl_GetIndexFromObj(interp, objv[i], hiddenOptions, "option",
		    0, &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (index == OPT_GLOBAL) {
		namespaceName = "::";
	    } else if (index == OPT_NAMESPACE) {
		if (++i == objc) { /* There must be more arguments. */
		    break;
		} else {
		    namespaceName = TclGetString(objv[i]);
		}
	    } else {
		i++;
		break;
	    }
	}
	if (objc - i < 1) {
	    Tcl_WrongNumArgs(interp, 2, objv,
		    "?-namespace ns? ?-global? ?--? cmd ?arg ..?");
	    return TCL_ERROR;
	}
	return ChildInvokeHidden(interp, childInterp, namespaceName,
		objc - i, objv + i);
    }
    case OPT_LIMIT: {
	static const char *const limitTypes[] = {
	    "commands", "time", NULL
	};
	enum LimitTypes {
	    LIMIT_TYPE_COMMANDS, LIMIT_TYPE_TIME
	};
	int limitType;

	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "limitType ?-option value ...?");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[2], limitTypes, "limit type", 0,
		&limitType) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch ((enum LimitTypes) limitType) {
	case LIMIT_TYPE_COMMANDS:
	    return ChildCommandLimitCmd(interp, childInterp, 3, objc,objv);
	case LIMIT_TYPE_TIME:
	    return ChildTimeLimitCmd(interp, childInterp, 3, objc, objv);
	}
    }
    break;
    case OPT_MARKTRUSTED:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, NULL);
	    return TCL_ERROR;
	}
	return ChildMarkTrusted(interp, childInterp);
    case OPT_RECLIMIT:
	if (objc != 2 && objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?newlimit?");
	    return TCL_ERROR;
	}
	return ChildRecursionLimit(interp, childInterp, objc - 2, objv + 2);
    }

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildObjCmdDeleteProc --
 *
 *	Invoked when an object command for a child interpreter is deleted;
 *	cleans up all state associated with the child interpreter and destroys
 *	the child interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Cleans up all state associated with the child interpreter and destroys
 *	the child interpreter.
 *
 *----------------------------------------------------------------------
 */

static void
ChildObjCmdDeleteProc(
    ClientData clientData)	/* The ChildRecord for the command. */
{
    Child *childPtr;		/* Interim storage for Child record. */
    Tcl_Interp *childInterp = (Tcl_Interp *)clientData;
				/* And for a child interp. */

    childPtr = &((InterpInfo *) ((Interp *) childInterp)->interpInfo)->child;

    /*
     * Unlink the child from its parent interpreter.
     */

    Tcl_DeleteHashEntry(childPtr->childEntryPtr);

    /*
     * Set to NULL so that when the InterpInfo is cleaned up in the child it
     * does not try to delete the command causing all sorts of grief. See
     * ChildRecordDeleteProc().
     */

    childPtr->interpCmd = NULL;

    if (childPtr->childInterp != NULL) {
	Tcl_DeleteInterp(childPtr->childInterp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChildDebugCmd -- TIP #378
 *
 *	Helper function to handle 'debug' command in a child interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	May modify INTERP_DEBUG_FRAME flag in the child.
 *
 *----------------------------------------------------------------------
 */

static int
ChildDebugCmd(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* The child interpreter in which command
				 * will be evaluated. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *const debugTypes[] = {
	"-frame", NULL
    };
    enum DebugTypes {
	DEBUG_TYPE_FRAME
    };
    int debugType;
    Interp *iPtr;
    Tcl_Obj *resultPtr;

    iPtr = (Interp *) childInterp;
    if (objc == 0) {
	TclNewObj(resultPtr);
	Tcl_ListObjAppendElement(NULL, resultPtr,
		Tcl_NewStringObj("-frame", -1));
	Tcl_ListObjAppendElement(NULL, resultPtr,
		Tcl_NewBooleanObj(iPtr->flags & INTERP_DEBUG_FRAME));
	Tcl_SetObjResult(interp, resultPtr);
    } else {
	if (Tcl_GetIndexFromObj(interp, objv[0], debugTypes, "debug option",
		0, &debugType) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (debugType == DEBUG_TYPE_FRAME) {
	    if (objc == 2) { /* set */
		if (Tcl_GetBooleanFromObj(interp, objv[1], &debugType)
			!= TCL_OK) {
		    return TCL_ERROR;
		}

		/*
		 * Quietly ignore attempts to disable interp debugging.  This
		 * is a one-way switch as frame debug info is maintained in a
		 * stack that must be consistent once turned on.
		 */

		if (debugType) {
		    iPtr->flags |= INTERP_DEBUG_FRAME;
		}
	    }
	    Tcl_SetObjResult(interp,
		    Tcl_NewBooleanObj(iPtr->flags & INTERP_DEBUG_FRAME));
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildEval --
 *
 *	Helper function to evaluate a command in a child interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the command does.
 *
 *----------------------------------------------------------------------
 */

static int
ChildEval(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* The child interpreter in which command
				 * will be evaluated. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result;

    /*
     * TIP #285: If necessary, reset the cancellation flags for the child
     * interpreter now; otherwise, canceling a script in a parent interpreter
     * can result in a situation where a child interpreter can no longer
     * evaluate any scripts unless somebody calls the TclResetCancellation
     * function for that particular Tcl_Interp.
     */

    TclSetChildCancelFlags(childInterp, 0, 0);

    Tcl_Preserve(childInterp);
    Tcl_AllowExceptions(childInterp);

    if (objc == 1) {
	/*
	 * TIP #280: Make actual argument location available to eval'd script.
	 */

	Interp *iPtr = (Interp *) interp;
	CmdFrame *invoker = iPtr->cmdFramePtr;
	int word = 0;

	TclArgumentGet(interp, objv[0], &invoker, &word);

	result = TclEvalObjEx(childInterp, objv[0], 0, invoker, word);
    } else {
	Tcl_Obj *objPtr = Tcl_ConcatObj(objc, objv);
	Tcl_IncrRefCount(objPtr);
	result = Tcl_EvalObjEx(childInterp, objPtr, 0);
	Tcl_DecrRefCount(objPtr);
    }
    Tcl_TransferResult(childInterp, result, interp);

    Tcl_Release(childInterp);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildExpose --
 *
 *	Helper function to expose a command in a child interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call scripts in the child will be able to invoke the newly
 *	exposed command.
 *
 *----------------------------------------------------------------------
 */

static int
ChildExpose(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* Interp in which command will be exposed. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    const char *name;

    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"permission denied: safe interpreter cannot expose commands",
		-1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "UNSAFE",
		(char *)NULL);
	return TCL_ERROR;
    }

    name = TclGetString(objv[(objc == 1) ? 0 : 1]);
    if (Tcl_ExposeCommand(childInterp, TclGetString(objv[0]),
	    name) != TCL_OK) {
	Tcl_TransferResult(childInterp, TCL_ERROR, interp);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildRecursionLimit --
 *
 *	Helper function to set/query the Recursion limit of an interp
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	When (objc == 1), childInterp will be set to a new recursion limit of
 *	objv[0].
 *
 *----------------------------------------------------------------------
 */

static int
ChildRecursionLimit(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* Interp in which limit is set/queried. */
    int objc,			/* Set or Query. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    Interp *iPtr;
    int limit;

    if (objc) {
	if (Tcl_IsSafe(interp)) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj("permission denied: "
		    "safe interpreters cannot change recursion limit", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "UNSAFE",
		    (char *)NULL);
	    return TCL_ERROR;
	}
	if (TclGetIntFromObj(interp, objv[0], &limit) == TCL_ERROR) {
	    return TCL_ERROR;
	}
	if (limit <= 0) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "recursion limit must be > 0", -1));
	    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "BADLIMIT",
		    (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_SetRecursionLimit(childInterp, limit);
	iPtr = (Interp *) childInterp;
	if (interp == childInterp && iPtr->numLevels > limit) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "falling back due to new recursion limit", -1));
	    Tcl_SetErrorCode(interp, "TCL", "RECURSION", (char *)NULL);
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, objv[0]);
	return TCL_OK;
    } else {
	limit = Tcl_SetRecursionLimit(childInterp, 0);
	Tcl_SetObjResult(interp, Tcl_NewIntObj(limit));
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChildHide --
 *
 *	Helper function to hide a command in a child interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call scripts in the child will no longer be able to invoke
 *	the named command.
 *
 *----------------------------------------------------------------------
 */

static int
ChildHide(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* Interp in which command will be exposed. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument strings. */
{
    const char *name;

    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"permission denied: safe interpreter cannot hide commands",
		-1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "UNSAFE",
		(char *)NULL);
	return TCL_ERROR;
    }

    name = TclGetString(objv[(objc == 1) ? 0 : 1]);
    if (Tcl_HideCommand(childInterp, TclGetString(objv[0]), name) != TCL_OK) {
	Tcl_TransferResult(childInterp, TCL_ERROR, interp);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildHidden --
 *
 *	Helper function to compute list of hidden commands in a child
 *	interpreter.
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
ChildHidden(
    Tcl_Interp *interp,		/* Interp for data return. */
    Tcl_Interp *childInterp)	/* Interp whose hidden commands to query. */
{
    Tcl_Obj *listObjPtr;		/* Local object pointer. */
    Tcl_HashTable *hTblPtr;		/* For local searches. */
    Tcl_HashEntry *hPtr;		/* For local searches. */
    Tcl_HashSearch hSearch;		/* For local searches. */

    TclNewObj(listObjPtr);
    hTblPtr = ((Interp *) childInterp)->hiddenCmdTablePtr;
    if (hTblPtr != NULL) {
	for (hPtr = Tcl_FirstHashEntry(hTblPtr, &hSearch);
		hPtr != NULL;
		hPtr = Tcl_NextHashEntry(&hSearch)) {
	    Tcl_ListObjAppendElement(NULL, listObjPtr,
		    Tcl_NewStringObj((const char *)Tcl_GetHashKey(hTblPtr, hPtr), -1));
	}
    }
    Tcl_SetObjResult(interp, listObjPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildInvokeHidden --
 *
 *	Helper function to invoke a hidden command in a child interpreter.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Whatever the hidden command does.
 *
 *----------------------------------------------------------------------
 */

static int
ChildInvokeHidden(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp,	/* The child interpreter in which command will
				 * be invoked. */
    const char *namespaceName,	/* The namespace to use, if any. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result;

    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"not allowed to invoke hidden commands from safe interpreter",
		-1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "UNSAFE",
		(char *)NULL);
	return TCL_ERROR;
    }

    Tcl_Preserve(childInterp);
    Tcl_AllowExceptions(childInterp);

    if (namespaceName == NULL) {
	NRE_callback *rootPtr = TOP_CB(childInterp);

	Tcl_NRAddCallback(interp, NRPostInvokeHidden, childInterp,
		rootPtr, NULL, NULL);
	return TclNRInvoke(NULL, childInterp, objc, objv);
    } else {
	Namespace *nsPtr, *dummy1, *dummy2;
	const char *tail;

	result = TclGetNamespaceForQualName(childInterp, namespaceName, NULL,
		TCL_FIND_ONLY_NS | TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG
		| TCL_CREATE_NS_IF_UNKNOWN, &nsPtr, &dummy1, &dummy2, &tail);
	if (result == TCL_OK) {
	    result = TclObjInvokeNamespace(childInterp, objc, objv,
		    (Tcl_Namespace *) nsPtr, TCL_INVOKE_HIDDEN);
	}
    }

    Tcl_TransferResult(childInterp, result, interp);

    Tcl_Release(childInterp);
    return result;
}

static int
NRPostInvokeHidden(
    ClientData data[],
    Tcl_Interp *interp,
    int result)
{
    Tcl_Interp *childInterp = (Tcl_Interp *)data[0];
    NRE_callback *rootPtr = (NRE_callback *)data[1];

    if (interp != childInterp) {
	result = TclNRRunCallbacks(childInterp, result, rootPtr);
	Tcl_TransferResult(childInterp, result, interp);
    }
    Tcl_Release(childInterp);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ChildMarkTrusted --
 *
 *	Helper function to mark a child interpreter as trusted (unsafe).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	After this call the hard-wired security checks in the core no longer
 *	prevent the child from performing certain operations.
 *
 *----------------------------------------------------------------------
 */

static int
ChildMarkTrusted(
    Tcl_Interp *interp,		/* Interp for error return. */
    Tcl_Interp *childInterp)	/* The child interpreter which will be marked
				 * trusted. */
{
    if (Tcl_IsSafe(interp)) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"permission denied: safe interpreter cannot mark trusted",
		-1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "UNSAFE",
		(char *)NULL);
	return TCL_ERROR;
    }
    ((Interp *) childInterp)->flags &= ~SAFE_INTERP;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_IsSafe --
 *
 *	Determines whether an interpreter is safe
 *
 * Results:
 *	1 if it is safe, 0 if it is not.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_IsSafe(
    Tcl_Interp *interp)		/* Is this interpreter "safe" ? */
{
    Interp *iPtr = (Interp *) interp;

    if (iPtr == NULL) {
	return 0;
    }
    return (iPtr->flags & SAFE_INTERP) ? 1 : 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeSafe --
 *
 *	Makes its argument interpreter contain only functionality that is
 *	defined to be part of Safe Tcl. Unsafe commands are hidden, the env
 *	array is unset, and the standard channels are removed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Hides commands in its argument interpreter, and removes settings and
 *	channels.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_MakeSafe(
    Tcl_Interp *interp)		/* Interpreter to be made safe. */
{
    Tcl_Channel chan;		/* Channel to remove from safe interpreter. */
    Interp *iPtr = (Interp *) interp;
    Tcl_Interp *parent = ((InterpInfo*) iPtr->interpInfo)->child.parentInterp;

    TclHideUnsafeCommands(interp);

    if (parent != NULL) {
	/*
	 * Alias these function implementations in the child to those in the
	 * parent; the overall implementations are safe, but they're normally
	 * defined by init.tcl which is not sourced by safe interpreters.
	 * Assume these functions all work. [Bug 2895741]
	 */

	(void) Tcl_Eval(interp,
		"namespace eval ::tcl {namespace eval mathfunc {}}");
	(void) Tcl_CreateAlias(interp, "::tcl::mathfunc::min", parent,
		"::tcl::mathfunc::min", 0, NULL);
	(void) Tcl_CreateAlias(interp, "::tcl::mathfunc::max", parent,
		"::tcl::mathfunc::max", 0, NULL);
    }

    iPtr->flags |= SAFE_INTERP;

    /*
     * Unsetting variables : (which should not have been set in the first
     * place, but...)
     */

    /*
     * No env array in a safe interpreter.
     */

    Tcl_UnsetVar(interp, "env", TCL_GLOBAL_ONLY);

    /*
     * Remove unsafe parts of tcl_platform
     */

    Tcl_UnsetVar2(interp, "tcl_platform", "os", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "tcl_platform", "osVersion", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "tcl_platform", "machine", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar2(interp, "tcl_platform", "user", TCL_GLOBAL_ONLY);

    /*
     * Unset path information variables (the only one remaining is [info
     * nameofexecutable])
     */

    Tcl_UnsetVar(interp, "tclDefaultLibrary", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar(interp, "tcl_library", TCL_GLOBAL_ONLY);
    Tcl_UnsetVar(interp, "tcl_pkgPath", TCL_GLOBAL_ONLY);

    /*
     * Remove the standard channels from the interpreter; safe interpreters do
     * not ordinarily have access to stdin, stdout and stderr.
     *
     * NOTE: These channels are not added to the interpreter by the
     * Tcl_CreateInterp call, but may be added later, by another I/O
     * operation. We want to ensure that the interpreter does not have these
     * channels even if it is being made safe after being used for some time..
     */

    chan = Tcl_GetStdChannel(TCL_STDIN);
    if (chan != NULL) {
	Tcl_UnregisterChannel(interp, chan);
    }
    chan = Tcl_GetStdChannel(TCL_STDOUT);
    if (chan != NULL) {
	Tcl_UnregisterChannel(interp, chan);
    }
    chan = Tcl_GetStdChannel(TCL_STDERR);
    if (chan != NULL) {
	Tcl_UnregisterChannel(interp, chan);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitExceeded --
 *
 *	Tests whether any limit has been exceeded in the given interpreter
 *	(i.e. whether the interpreter is currently unable to process further
 *	scripts).
 *
 * Results:
 *	A boolean value.
 *
 * Side effects:
 *	None.
 *
 * Notes:
 *	If you change this function, you MUST also update TclLimitExceeded() in
 *	tclInt.h.
 *----------------------------------------------------------------------
 */

int
Tcl_LimitExceeded(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;

    return iPtr->limit.exceeded != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitReady --
 *
 *	Find out whether any limit has been set on the interpreter, and if so
 *	check whether the granularity of that limit is such that the full
 *	limit check should be carried out.
 *
 * Results:
 *	A boolean value that indicates whether to call Tcl_LimitCheck.
 *
 * Side effects:
 *	Increments the limit granularity counter.
 *
 * Notes:
 *	If you change this function, you MUST also update TclLimitReady() in
 *	tclInt.h.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LimitReady(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;

    if (iPtr->limit.active != 0) {
	int ticker = ++iPtr->limit.granularityTicker;

	if ((iPtr->limit.active & TCL_LIMIT_COMMANDS) &&
		((iPtr->limit.cmdGranularity == 1) ||
		    (ticker % iPtr->limit.cmdGranularity == 0))) {
	    return 1;
	}
	if ((iPtr->limit.active & TCL_LIMIT_TIME) &&
		((iPtr->limit.timeGranularity == 1) ||
		    (ticker % iPtr->limit.timeGranularity == 0))) {
	    return 1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitCheck --
 *
 *	Check all currently set limits in the interpreter (where permitted by
 *	granularity). If a limit is exceeded, call its callbacks and, if the
 *	limit is still exceeded after the callbacks have run, make the
 *	interpreter generate an error that cannot be caught within the limited
 *	interpreter.
 *
 * Results:
 *	A Tcl result value (TCL_OK if no limit is exceeded, and TCL_ERROR if a
 *	limit has been exceeded).
 *
 * Side effects:
 *	May invoke system calls. May invoke other interpreters. May be
 *	reentrant. May put the interpreter into a state where it can no longer
 *	execute commands without outside intervention.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LimitCheck(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    int ticker = iPtr->limit.granularityTicker;

    if (Tcl_InterpDeleted(interp)) {
	return TCL_OK;
    }

    if ((iPtr->limit.active & TCL_LIMIT_COMMANDS) &&
	    ((iPtr->limit.cmdGranularity == 1) ||
		    (ticker % iPtr->limit.cmdGranularity == 0)) &&
	    (iPtr->limit.cmdCount < iPtr->cmdCount)) {
	iPtr->limit.exceeded |= TCL_LIMIT_COMMANDS;
	Tcl_Preserve(interp);
	RunLimitHandlers(iPtr->limit.cmdHandlers, interp);
	if (iPtr->limit.cmdCount >= iPtr->cmdCount) {
	    iPtr->limit.exceeded &= ~TCL_LIMIT_COMMANDS;
	} else if (iPtr->limit.exceeded & TCL_LIMIT_COMMANDS) {
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(
		    "command count limit exceeded", -1));
	    Tcl_SetErrorCode(interp, "TCL", "LIMIT", "COMMANDS", (char *)NULL);
	    Tcl_Release(interp);
	    return TCL_ERROR;
	}
	Tcl_Release(interp);
    }

    if ((iPtr->limit.active & TCL_LIMIT_TIME) &&
	    ((iPtr->limit.timeGranularity == 1) ||
		(ticker % iPtr->limit.timeGranularity == 0))) {
	Tcl_Time now;

	Tcl_GetTime(&now);
	if (iPtr->limit.time.sec < now.sec ||
		(iPtr->limit.time.sec == now.sec &&
		iPtr->limit.time.usec < now.usec)) {
	    iPtr->limit.exceeded |= TCL_LIMIT_TIME;
	    Tcl_Preserve(interp);
	    RunLimitHandlers(iPtr->limit.timeHandlers, interp);
	    if (iPtr->limit.time.sec > now.sec ||
		    (iPtr->limit.time.sec == now.sec &&
		    iPtr->limit.time.usec >= now.usec)) {
		iPtr->limit.exceeded &= ~TCL_LIMIT_TIME;
	    } else if (iPtr->limit.exceeded & TCL_LIMIT_TIME) {
		Tcl_SetObjResult(interp, Tcl_NewStringObj(
			"time limit exceeded", -1));
		Tcl_SetErrorCode(interp, "TCL", "LIMIT", "TIME", (char *)NULL);
		Tcl_Release(interp);
		return TCL_ERROR;
	    }
	    Tcl_Release(interp);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * RunLimitHandlers --
 *
 *	Invoke all the limit handlers in a list (for a particular limit).
 *	Note that no particular limit handler callback will be invoked
 *	reentrantly.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the limit handlers.
 *
 *----------------------------------------------------------------------
 */

static void
RunLimitHandlers(
    LimitHandler *handlerPtr,
    Tcl_Interp *interp)
{
    LimitHandler *nextPtr;
    for (; handlerPtr!=NULL ; handlerPtr=nextPtr) {
	if (handlerPtr->flags & (LIMIT_HANDLER_DELETED|LIMIT_HANDLER_ACTIVE)) {
	    /*
	     * Reentrant call or something seriously strange in the delete
	     * code.
	     */

	    nextPtr = handlerPtr->nextPtr;
	    continue;
	}

	/*
	 * Set the ACTIVE flag while running the limit handler itself so we
	 * cannot reentrantly call this handler and know to use the alternate
	 * method of deletion if necessary.
	 */

	handlerPtr->flags |= LIMIT_HANDLER_ACTIVE;
	handlerPtr->handlerProc(handlerPtr->clientData, interp);
	handlerPtr->flags &= ~LIMIT_HANDLER_ACTIVE;

	/*
	 * Rediscover this value; it might have changed during the processing
	 * of a limit handler. We have to record it here because we might
	 * delete the structure below, and reading a value out of a deleted
	 * structure is unsafe (even if actually legal with some
	 * malloc()/free() implementations.)
	 */

	nextPtr = handlerPtr->nextPtr;

	/*
	 * If we deleted the current handler while we were executing it, we
	 * will have spliced it out of the list and set the
	 * LIMIT_HANDLER_DELETED flag.
	 */

	if (handlerPtr->flags & LIMIT_HANDLER_DELETED) {
	    if (handlerPtr->deleteProc != NULL) {
		handlerPtr->deleteProc(handlerPtr->clientData);
	    }
	    ckfree(handlerPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitAddHandler --
 *
 *	Add a callback handler for a particular resource limit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Extends the internal linked list of handlers for a limit.
 *
 *----------------------------------------------------------------------
 */

/* Bug 52dbc4b3f8: wrap Tcl_Free since it is not a Tcl_LimitHandlerDeleteProc. */
static void
WrapFree(
    void *ptr)
{
    ckfree(ptr);
}

void
Tcl_LimitAddHandler(
    Tcl_Interp *interp,
    int type,
    Tcl_LimitHandlerProc *handlerProc,
    ClientData clientData,
    Tcl_LimitHandlerDeleteProc *deleteProc)
{
    Interp *iPtr = (Interp *) interp;
    LimitHandler *handlerPtr;

    /*
     * Convert everything into a real deletion callback.
     */

    if (deleteProc == (Tcl_LimitHandlerDeleteProc *) TCL_DYNAMIC) {
	deleteProc = WrapFree;
    }

    /*
     * Allocate a handler record.
     */

    handlerPtr = (LimitHandler *)ckalloc(sizeof(LimitHandler));
    handlerPtr->flags = 0;
    handlerPtr->handlerProc = handlerProc;
    handlerPtr->clientData = clientData;
    handlerPtr->deleteProc = deleteProc;
    handlerPtr->prevPtr = NULL;

    /*
     * Prepend onto the front of the correct linked list.
     */

    switch (type) {
    case TCL_LIMIT_COMMANDS:
	handlerPtr->nextPtr = iPtr->limit.cmdHandlers;
	if (handlerPtr->nextPtr != NULL) {
	    handlerPtr->nextPtr->prevPtr = handlerPtr;
	}
	iPtr->limit.cmdHandlers = handlerPtr;
	return;

    case TCL_LIMIT_TIME:
	handlerPtr->nextPtr = iPtr->limit.timeHandlers;
	if (handlerPtr->nextPtr != NULL) {
	    handlerPtr->nextPtr->prevPtr = handlerPtr;
	}
	iPtr->limit.timeHandlers = handlerPtr;
	return;
    }

    Tcl_Panic("unknown type of resource limit");
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitRemoveHandler --
 *
 *	Remove a callback handler for a particular resource limit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The handler is spliced out of the internal linked list for the limit,
 *	and if not currently being invoked, deleted. Otherwise it is just
 *	marked for deletion and removed when the limit handler has finished
 *	executing.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitRemoveHandler(
    Tcl_Interp *interp,
    int type,
    Tcl_LimitHandlerProc *handlerProc,
    ClientData clientData)
{
    Interp *iPtr = (Interp *) interp;
    LimitHandler *handlerPtr;

    switch (type) {
    case TCL_LIMIT_COMMANDS:
	handlerPtr = iPtr->limit.cmdHandlers;
	break;
    case TCL_LIMIT_TIME:
	handlerPtr = iPtr->limit.timeHandlers;
	break;
    default:
	Tcl_Panic("unknown type of resource limit");
	return;
    }

    for (; handlerPtr!=NULL ; handlerPtr=handlerPtr->nextPtr) {
	if ((handlerPtr->handlerProc != handlerProc) ||
		(handlerPtr->clientData != clientData)) {
	    continue;
	}

	/*
	 * We've found the handler to delete; mark it as doomed if not already
	 * so marked (which shouldn't actually happen).
	 */

	if (handlerPtr->flags & LIMIT_HANDLER_DELETED) {
	    return;
	}
	handlerPtr->flags |= LIMIT_HANDLER_DELETED;

	/*
	 * Splice the handler out of the doubly-linked list.
	 */

	if (handlerPtr->prevPtr == NULL) {
	    switch (type) {
	    case TCL_LIMIT_COMMANDS:
		iPtr->limit.cmdHandlers = handlerPtr->nextPtr;
		break;
	    case TCL_LIMIT_TIME:
		iPtr->limit.timeHandlers = handlerPtr->nextPtr;
		break;
	    }
	} else {
	    handlerPtr->prevPtr->nextPtr = handlerPtr->nextPtr;
	}
	if (handlerPtr->nextPtr != NULL) {
	    handlerPtr->nextPtr->prevPtr = handlerPtr->prevPtr;
	}

	/*
	 * If nothing is currently executing the handler, delete its client
	 * data and the overall handler structure now. Otherwise it will all
	 * go away when the handler returns.
	 */

	if (!(handlerPtr->flags & LIMIT_HANDLER_ACTIVE)) {
	    if (handlerPtr->deleteProc != NULL) {
		handlerPtr->deleteProc(handlerPtr->clientData);
	    }
	    ckfree(handlerPtr);
	}
	return;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclLimitRemoveAllHandlers --
 *
 *	Remove all limit callback handlers for an interpreter. This is invoked
 *	as part of deleting the interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Limit handlers are deleted or marked for deletion (as with
 *	Tcl_LimitRemoveHandler).
 *
 *----------------------------------------------------------------------
 */

void
TclLimitRemoveAllHandlers(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    LimitHandler *handlerPtr, *nextHandlerPtr;

    /*
     * Delete all command-limit handlers.
     */

    for (handlerPtr=iPtr->limit.cmdHandlers, iPtr->limit.cmdHandlers=NULL;
	    handlerPtr!=NULL; handlerPtr=nextHandlerPtr) {
	nextHandlerPtr = handlerPtr->nextPtr;

	/*
	 * Do not delete here if it has already been marked for deletion.
	 */

	if (handlerPtr->flags & LIMIT_HANDLER_DELETED) {
	    continue;
	}
	handlerPtr->flags |= LIMIT_HANDLER_DELETED;
	handlerPtr->prevPtr = NULL;
	handlerPtr->nextPtr = NULL;

	/*
	 * If nothing is currently executing the handler, delete its client
	 * data and the overall handler structure now. Otherwise it will all
	 * go away when the handler returns.
	 */

	if (!(handlerPtr->flags & LIMIT_HANDLER_ACTIVE)) {
	    if (handlerPtr->deleteProc != NULL) {
		handlerPtr->deleteProc(handlerPtr->clientData);
	    }
	    ckfree(handlerPtr);
	}
    }

    /*
     * Delete all time-limit handlers.
     */

    for (handlerPtr=iPtr->limit.timeHandlers, iPtr->limit.timeHandlers=NULL;
	    handlerPtr!=NULL; handlerPtr=nextHandlerPtr) {
	nextHandlerPtr = handlerPtr->nextPtr;

	/*
	 * Do not delete here if it has already been marked for deletion.
	 */

	if (handlerPtr->flags & LIMIT_HANDLER_DELETED) {
	    continue;
	}
	handlerPtr->flags |= LIMIT_HANDLER_DELETED;
	handlerPtr->prevPtr = NULL;
	handlerPtr->nextPtr = NULL;

	/*
	 * If nothing is currently executing the handler, delete its client
	 * data and the overall handler structure now. Otherwise it will all
	 * go away when the handler returns.
	 */

	if (!(handlerPtr->flags & LIMIT_HANDLER_ACTIVE)) {
	    if (handlerPtr->deleteProc != NULL) {
		handlerPtr->deleteProc(handlerPtr->clientData);
	    }
	    ckfree(handlerPtr);
	}
    }

    /*
     * Delete the timer callback that is used to trap limits that occur in
     * [vwait]s...
     */

    if (iPtr->limit.timeEvent != NULL) {
	Tcl_DeleteTimerHandler(iPtr->limit.timeEvent);
	iPtr->limit.timeEvent = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitTypeEnabled --
 *
 *	Check whether a particular limit has been enabled for an interpreter.
 *
 * Results:
 *	A boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LimitTypeEnabled(
    Tcl_Interp *interp,
    int type)
{
    Interp *iPtr = (Interp *) interp;

    return (iPtr->limit.active & type) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitTypeExceeded --
 *
 *	Check whether a particular limit has been exceeded for an interpreter.
 *
 * Results:
 *	A boolean value (note that Tcl_LimitExceeded will always return
 *	non-zero when this function returns non-zero).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LimitTypeExceeded(
    Tcl_Interp *interp,
    int type)
{
    Interp *iPtr = (Interp *) interp;

    return (iPtr->limit.exceeded & type) != 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitTypeSet --
 *
 *	Enable a particular limit for an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The limit is turned on and will be checked in future at an interval
 *	determined by the frequency of calling of Tcl_LimitReady and the
 *	granularity of the limit in question.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitTypeSet(
    Tcl_Interp *interp,
    int type)
{
    Interp *iPtr = (Interp *) interp;

    iPtr->limit.active |= type;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitTypeReset --
 *
 *	Disable a particular limit for an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The limit is disabled. If the limit was exceeded when this function
 *	was called, the limit will no longer be exceeded afterwards and the
 *	interpreter will be free to execute further scripts (assuming it isn't
 *	also deleted, of course).
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitTypeReset(
    Tcl_Interp *interp,
    int type)
{
    Interp *iPtr = (Interp *) interp;

    iPtr->limit.active &= ~type;
    iPtr->limit.exceeded &= ~type;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitSetCommands --
 *
 *	Set the command limit for an interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Also resets whether the command limit was exceeded. This might permit
 *	a small amount of further execution in the interpreter even if the
 *	limit itself is theoretically exceeded.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitSetCommands(
    Tcl_Interp *interp,
    int commandLimit)
{
    Interp *iPtr = (Interp *) interp;

    iPtr->limit.cmdCount = commandLimit;
    iPtr->limit.exceeded &= ~TCL_LIMIT_COMMANDS;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitGetCommands --
 *
 *	Get the number of commands that may be executed in the interpreter
 *	before the command-limit is reached.
 *
 * Results:
 *	An upper bound on the number of commands.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LimitGetCommands(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;

    return iPtr->limit.cmdCount;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitSetTime --
 *
 *	Set the time limit for an interpreter by copying it from the value
 *	pointed to by the timeLimitPtr argument.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Also resets whether the time limit was exceeded. This might permit a
 *	small amount of further execution in the interpreter even if the limit
 *	itself is theoretically exceeded.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitSetTime(
    Tcl_Interp *interp,
    Tcl_Time *timeLimitPtr)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_Time nextMoment;

    memcpy(&iPtr->limit.time, timeLimitPtr, sizeof(Tcl_Time));
    if (iPtr->limit.timeEvent != NULL) {
	Tcl_DeleteTimerHandler(iPtr->limit.timeEvent);
    }
    nextMoment.sec = timeLimitPtr->sec;
    nextMoment.usec = timeLimitPtr->usec+10;
    if (nextMoment.usec >= 1000000) {
	nextMoment.sec++;
	nextMoment.usec -= 1000000;
    }
    iPtr->limit.timeEvent = TclCreateAbsoluteTimerHandler(&nextMoment,
	    TimeLimitCallback, interp);
    iPtr->limit.exceeded &= ~TCL_LIMIT_TIME;
}

/*
 *----------------------------------------------------------------------
 *
 * TimeLimitCallback --
 *
 *	Callback that allows time limits to be enforced even when doing a
 *	blocking wait for events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May put the interpreter into a state where it can no longer execute
 *	commands. May make callbacks into other interpreters.
 *
 *----------------------------------------------------------------------
 */

static void
TimeLimitCallback(
    ClientData clientData)
{
    Tcl_Interp *interp = (Tcl_Interp *)clientData;
    Interp *iPtr = (Interp *)clientData;
    int code;

    Tcl_Preserve(interp);
    iPtr->limit.timeEvent = NULL;

    /*
     * Must reset the granularity ticker here to force an immediate full
     * check. This is OK because we're swallowing the cost in the overall cost
     * of the event loop. [Bug 2891362]
     */

    iPtr->limit.granularityTicker = 0;

    code = Tcl_LimitCheck(interp);
    if (code != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (while waiting for event)");
	Tcl_BackgroundException(interp, code);
    }
    Tcl_Release(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitGetTime --
 *
 *	Get the current time limit.
 *
 * Results:
 *	The time limit (by it being copied into the variable pointed to by the
 *	timeLimitPtr).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitGetTime(
    Tcl_Interp *interp,
    Tcl_Time *timeLimitPtr)
{
    Interp *iPtr = (Interp *) interp;

    memcpy(timeLimitPtr, &iPtr->limit.time, sizeof(Tcl_Time));
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitSetGranularity --
 *
 *	Set the granularity divisor (which must be positive) for a particular
 *	limit.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The granularity is updated.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_LimitSetGranularity(
    Tcl_Interp *interp,
    int type,
    int granularity)
{
    Interp *iPtr = (Interp *) interp;
    if (granularity < 1) {
	Tcl_Panic("limit granularity must be positive");
    }

    switch (type) {
    case TCL_LIMIT_COMMANDS:
	iPtr->limit.cmdGranularity = granularity;
	return;
    case TCL_LIMIT_TIME:
	iPtr->limit.timeGranularity = granularity;
	return;
    }
    Tcl_Panic("unknown type of resource limit");
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LimitGetGranularity --
 *
 *	Get the granularity divisor for a particular limit.
 *
 * Results:
 *	The granularity divisor for the given limit.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_LimitGetGranularity(
    Tcl_Interp *interp,
    int type)
{
    Interp *iPtr = (Interp *) interp;

    switch (type) {
    case TCL_LIMIT_COMMANDS:
	return iPtr->limit.cmdGranularity;
    case TCL_LIMIT_TIME:
	return iPtr->limit.timeGranularity;
    }
    Tcl_Panic("unknown type of resource limit");
    return -1; /* NOT REACHED */
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteScriptLimitCallback --
 *
 *	Callback for when a script limit (a limit callback implemented as a
 *	Tcl script in a parent interpreter, as set up from Tcl) is deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The reference to the script callback from the controlling interpreter
 *	is removed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteScriptLimitCallback(
    ClientData clientData)
{
    ScriptLimitCallback *limitCBPtr = (ScriptLimitCallback *)clientData;

    Tcl_DecrRefCount(limitCBPtr->scriptObj);
    if (limitCBPtr->entryPtr != NULL) {
	Tcl_DeleteHashEntry(limitCBPtr->entryPtr);
    }
    ckfree(limitCBPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CallScriptLimitCallback --
 *
 *	Invoke a script limit callback. Used to implement limit callbacks set
 *	at the Tcl level on child interpreters.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the callback script. Errors are reported as background
 *	errors.
 *
 *----------------------------------------------------------------------
 */

static void
CallScriptLimitCallback(
    ClientData clientData,
    Tcl_Interp *interp)		/* Interpreter which failed the limit */
{
    ScriptLimitCallback *limitCBPtr = (ScriptLimitCallback *)clientData;
    int code;

    if (Tcl_InterpDeleted(limitCBPtr->interp)) {
	return;
    }
    Tcl_Preserve(limitCBPtr->interp);
    code = Tcl_EvalObjEx(limitCBPtr->interp, limitCBPtr->scriptObj,
	    TCL_EVAL_GLOBAL);
    if (code != TCL_OK && !Tcl_InterpDeleted(limitCBPtr->interp)) {
	Tcl_BackgroundException(limitCBPtr->interp, code);
    }
    Tcl_Release(limitCBPtr->interp);
}

/*
 *----------------------------------------------------------------------
 *
 * SetScriptLimitCallback --
 *
 *	Install (or remove, if scriptObj is NULL) a limit callback script that
 *	is called when the target interpreter exceeds the type of limit
 *	specified. Each interpreter may only have one callback set on another
 *	interpreter through this mechanism (though as many interpreters may be
 *	limited as the programmer chooses overall).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A limit callback implemented as an invocation of a Tcl script in
 *	another interpreter is either installed or removed.
 *
 *----------------------------------------------------------------------
 */

static void
SetScriptLimitCallback(
    Tcl_Interp *interp,
    int type,
    Tcl_Interp *targetInterp,
    Tcl_Obj *scriptObj)
{
    ScriptLimitCallback *limitCBPtr;
    Tcl_HashEntry *hashPtr;
    int isNew;
    ScriptLimitCallbackKey key;
    Interp *iPtr = (Interp *) interp;

    if (interp == targetInterp) {
	Tcl_Panic("installing limit callback to the limited interpreter");
    }

    key.interp = targetInterp;
    key.type = type;

    if (scriptObj == NULL) {
	hashPtr = Tcl_FindHashEntry(&iPtr->limit.callbacks, (char *) &key);
	if (hashPtr != NULL) {
	    Tcl_LimitRemoveHandler(targetInterp, type, CallScriptLimitCallback,
		    Tcl_GetHashValue(hashPtr));
	}
	return;
    }

    hashPtr = Tcl_CreateHashEntry(&iPtr->limit.callbacks, &key,
	    &isNew);
    if (!isNew) {
	limitCBPtr = (ScriptLimitCallback *)Tcl_GetHashValue(hashPtr);
	limitCBPtr->entryPtr = NULL;
	Tcl_LimitRemoveHandler(targetInterp, type, CallScriptLimitCallback,
		limitCBPtr);
    }

    limitCBPtr = (ScriptLimitCallback *)ckalloc(sizeof(ScriptLimitCallback));
    limitCBPtr->interp = interp;
    limitCBPtr->scriptObj = scriptObj;
    limitCBPtr->entryPtr = hashPtr;
    limitCBPtr->type = type;
    Tcl_IncrRefCount(scriptObj);

    Tcl_LimitAddHandler(targetInterp, type, CallScriptLimitCallback,
	    limitCBPtr, DeleteScriptLimitCallback);
    Tcl_SetHashValue(hashPtr, limitCBPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TclRemoveScriptLimitCallbacks --
 *
 *	Remove all script-implemented limit callbacks that make calls back
 *	into the given interpreter. This invoked as part of deleting an
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The script limit callbacks are removed or marked for later removal.
 *
 *----------------------------------------------------------------------
 */

void
TclRemoveScriptLimitCallbacks(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hashPtr;
    Tcl_HashSearch search;
    ScriptLimitCallbackKey *keyPtr;

    hashPtr = Tcl_FirstHashEntry(&iPtr->limit.callbacks, &search);
    while (hashPtr != NULL) {
	keyPtr = (ScriptLimitCallbackKey *)
		Tcl_GetHashKey(&iPtr->limit.callbacks, hashPtr);
	Tcl_LimitRemoveHandler(keyPtr->interp, keyPtr->type,
		CallScriptLimitCallback, Tcl_GetHashValue(hashPtr));
	hashPtr = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&iPtr->limit.callbacks);
}

/*
 *----------------------------------------------------------------------
 *
 * TclInitLimitSupport --
 *
 *	Initialise all the parts of the interpreter relating to resource limit
 *	management. This allows an interpreter to both have limits set upon
 *	itself and set limits upon other interpreters.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The resource limit subsystem is initialised for the interpreter.
 *
 *----------------------------------------------------------------------
 */

void
TclInitLimitSupport(
    Tcl_Interp *interp)
{
    Interp *iPtr = (Interp *) interp;

    iPtr->limit.active = 0;
    iPtr->limit.granularityTicker = 0;
    iPtr->limit.exceeded = 0;
    iPtr->limit.cmdCount = 0;
    iPtr->limit.cmdHandlers = NULL;
    iPtr->limit.cmdGranularity = 1;
    memset(&iPtr->limit.time, 0, sizeof(Tcl_Time));
    iPtr->limit.timeHandlers = NULL;
    iPtr->limit.timeEvent = NULL;
    iPtr->limit.timeGranularity = 10;
    Tcl_InitHashTable(&iPtr->limit.callbacks,
	    sizeof(ScriptLimitCallbackKey)/sizeof(int));
}

/*
 *----------------------------------------------------------------------
 *
 * InheritLimitsFromParent --
 *
 *	Derive the interpreter limit configuration for a child interpreter
 *	from the limit config for the parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The child interpreter limits are set so that if the parent has a
 *	limit, it may not exceed it by handing off work to child interpreters.
 *	Note that this does not transfer limit callbacks from the parent to
 *	the child.
 *
 *----------------------------------------------------------------------
 */

static void
InheritLimitsFromParent(
    Tcl_Interp *childInterp,
    Tcl_Interp *parentInterp)
{
    Interp *childPtr = (Interp *) childInterp;
    Interp *parentPtr = (Interp *) parentInterp;

    if (parentPtr->limit.active & TCL_LIMIT_COMMANDS) {
	childPtr->limit.active |= TCL_LIMIT_COMMANDS;
	childPtr->limit.cmdCount = 0;
	childPtr->limit.cmdGranularity = parentPtr->limit.cmdGranularity;
    }
    if (parentPtr->limit.active & TCL_LIMIT_TIME) {
	childPtr->limit.active |= TCL_LIMIT_TIME;
	memcpy(&childPtr->limit.time, &parentPtr->limit.time,
		sizeof(Tcl_Time));
	childPtr->limit.timeGranularity = parentPtr->limit.timeGranularity;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChildCommandLimitCmd --
 *
 *	Implementation of the [interp limit $i commands] and [$i limit
 *	commands] subcommands. See the interp manual page for a full
 *	description.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on the arguments.
 *
 *----------------------------------------------------------------------
 */

static int
ChildCommandLimitCmd(
    Tcl_Interp *interp,		/* Current interpreter. */
    Tcl_Interp *childInterp,	/* Interpreter being adjusted. */
    int consumedObjc,		/* Number of args already parsed. */
    int objc,			/* Total number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *const options[] = {
	"-command", "-granularity", "-value", NULL
    };
    enum Options {
	OPT_CMD, OPT_GRAN, OPT_VAL
    };
    Interp *iPtr = (Interp *) interp;
    int index;
    ScriptLimitCallbackKey key;
    ScriptLimitCallback *limitCBPtr;
    Tcl_HashEntry *hPtr;

    /*
     * First, ensure that we are not reading or writing the calling
     * interpreter's limits; it may only manipulate its children. Note that
     * the low level API enforces this with Tcl_Panic, which we want to
     * avoid. [Bug 3398794]
     */

    if (interp == childInterp) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"limits on current interpreter inaccessible", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "SELF", (char *)NULL);
	return TCL_ERROR;
    }

    if (objc == consumedObjc) {
	Tcl_Obj *dictPtr;

	TclNewObj(dictPtr);
	key.interp = childInterp;
	key.type = TCL_LIMIT_COMMANDS;
	hPtr = Tcl_FindHashEntry(&iPtr->limit.callbacks, (char *) &key);
	if (hPtr != NULL) {
	    limitCBPtr = (ScriptLimitCallback *)Tcl_GetHashValue(hPtr);
	    if (limitCBPtr != NULL && limitCBPtr->scriptObj != NULL) {
		TclDictPut(NULL, dictPtr, options[0], limitCBPtr->scriptObj);
	    } else {
		goto putEmptyCommandInDict;
	    }
	} else {
	    Tcl_Obj *empty;

	putEmptyCommandInDict:
	    TclNewObj(empty);
	    TclDictPut(NULL, dictPtr, options[0], empty);
	}
	TclDictPut(NULL, dictPtr, options[1], Tcl_NewIntObj(
		Tcl_LimitGetGranularity(childInterp, TCL_LIMIT_COMMANDS)));

	if (Tcl_LimitTypeEnabled(childInterp, TCL_LIMIT_COMMANDS)) {
	    TclDictPut(NULL, dictPtr, options[2], Tcl_NewIntObj(
		    Tcl_LimitGetCommands(childInterp)));
	} else {
	    Tcl_Obj *empty;

	    TclNewObj(empty);
	    TclDictPut(NULL, dictPtr, options[2], empty);
	}
	Tcl_SetObjResult(interp, dictPtr);
	return TCL_OK;
    } else if (objc == consumedObjc+1) {
	if (Tcl_GetIndexFromObj(interp, objv[consumedObjc], options, "option",
		0, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch ((enum Options) index) {
	case OPT_CMD:
	    key.interp = childInterp;
	    key.type = TCL_LIMIT_COMMANDS;
	    hPtr = Tcl_FindHashEntry(&iPtr->limit.callbacks, (char *) &key);
	    if (hPtr != NULL) {
		limitCBPtr = (ScriptLimitCallback *)Tcl_GetHashValue(hPtr);
		if (limitCBPtr != NULL && limitCBPtr->scriptObj != NULL) {
		    Tcl_SetObjResult(interp, limitCBPtr->scriptObj);
		}
	    }
	    break;
	case OPT_GRAN:
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(
		    Tcl_LimitGetGranularity(childInterp, TCL_LIMIT_COMMANDS)));
	    break;
	case OPT_VAL:
	    if (Tcl_LimitTypeEnabled(childInterp, TCL_LIMIT_COMMANDS)) {
		Tcl_SetObjResult(interp,
			Tcl_NewIntObj(Tcl_LimitGetCommands(childInterp)));
	    }
	    break;
	}
	return TCL_OK;
    } else if ((objc-consumedObjc) & 1 /* isOdd(objc-consumedObjc) */) {
	Tcl_WrongNumArgs(interp, consumedObjc, objv, "?-option value ...?");
	return TCL_ERROR;
    } else {
	int i, scriptLen = 0, limitLen = 0;
	Tcl_Obj *scriptObj = NULL, *granObj = NULL, *limitObj = NULL;
	int gran = 0, limit = 0;

	for (i=consumedObjc ; i<objc ; i+=2) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
		    &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch ((enum Options) index) {
	    case OPT_CMD:
		scriptObj = objv[i+1];
		(void) TclGetStringFromObj(scriptObj, &scriptLen);
		break;
	    case OPT_GRAN:
		granObj = objv[i+1];
		if (TclGetIntFromObj(interp, objv[i+1], &gran) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (gran < 1) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "granularity must be at least 1", -1));
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADVALUE", (char *)NULL);
		    return TCL_ERROR;
		}
		break;
	    case OPT_VAL:
		limitObj = objv[i+1];
		(void) TclGetStringFromObj(objv[i+1], &limitLen);
		if (limitLen == 0) {
		    break;
		}
		if (TclGetIntFromObj(interp, objv[i+1], &limit) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (limit < 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "command limit value must be at least 0", -1));
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADVALUE", (char *)NULL);
		    return TCL_ERROR;
		}
		break;
	    }
	}
	if (scriptObj != NULL) {
	    SetScriptLimitCallback(interp, TCL_LIMIT_COMMANDS, childInterp,
		    (scriptLen > 0 ? scriptObj : NULL));
	}
	if (granObj != NULL) {
	    Tcl_LimitSetGranularity(childInterp, TCL_LIMIT_COMMANDS, gran);
	}
	if (limitObj != NULL) {
	    if (limitLen > 0) {
		Tcl_LimitSetCommands(childInterp, limit);
		Tcl_LimitTypeSet(childInterp, TCL_LIMIT_COMMANDS);
	    } else {
		Tcl_LimitTypeReset(childInterp, TCL_LIMIT_COMMANDS);
	    }
	}
	return TCL_OK;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChildTimeLimitCmd --
 *
 *	Implementation of the [interp limit $i time] and [$i limit time]
 *	subcommands. See the interp manual page for a full description.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on the arguments.
 *
 *----------------------------------------------------------------------
 */

static int
ChildTimeLimitCmd(
    Tcl_Interp *interp,			/* Current interpreter. */
    Tcl_Interp *childInterp,		/* Interpreter being adjusted. */
    int consumedObjc,			/* Number of args already parsed. */
    int objc,				/* Total number of arguments. */
    Tcl_Obj *const objv[])		/* Argument objects. */
{
    static const char *const options[] = {
	"-command", "-granularity", "-milliseconds", "-seconds", NULL
    };
    enum Options {
	OPT_CMD, OPT_GRAN, OPT_MILLI, OPT_SEC
    };
    Interp *iPtr = (Interp *) interp;
    int index;
    ScriptLimitCallbackKey key;
    ScriptLimitCallback *limitCBPtr;
    Tcl_HashEntry *hPtr;

    /*
     * First, ensure that we are not reading or writing the calling
     * interpreter's limits; it may only manipulate its children. Note that
     * the low level API enforces this with Tcl_Panic, which we want to
     * avoid. [Bug 3398794]
     */

    if (interp == childInterp) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"limits on current interpreter inaccessible", -1));
	Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP", "SELF", (char *)NULL);
	return TCL_ERROR;
    }

    if (objc == consumedObjc) {
	Tcl_Obj *dictPtr;

	TclNewObj(dictPtr);
	key.interp = childInterp;
	key.type = TCL_LIMIT_TIME;
	hPtr = Tcl_FindHashEntry(&iPtr->limit.callbacks, (char *) &key);
	if (hPtr != NULL) {
	    limitCBPtr = (ScriptLimitCallback *)Tcl_GetHashValue(hPtr);
	    if (limitCBPtr != NULL && limitCBPtr->scriptObj != NULL) {
		TclDictPut(NULL, dictPtr, options[0], limitCBPtr->scriptObj);
	    } else {
		goto putEmptyCommandInDict;
	    }
	} else {
	    Tcl_Obj *empty;
	putEmptyCommandInDict:
	    TclNewObj(empty);
	    TclDictPut(NULL, dictPtr, options[0], empty);
	}
	TclDictPut(NULL, dictPtr, options[1], Tcl_NewIntObj(
		Tcl_LimitGetGranularity(childInterp, TCL_LIMIT_TIME)));

	if (Tcl_LimitTypeEnabled(childInterp, TCL_LIMIT_TIME)) {
	    Tcl_Time limitMoment;

	    Tcl_LimitGetTime(childInterp, &limitMoment);
	    TclDictPut(NULL, dictPtr, options[2],
		    Tcl_NewLongObj(limitMoment.usec / 1000));
	    TclDictPut(NULL, dictPtr, options[3],
		    Tcl_NewLongObj(limitMoment.sec));
	} else {
	    Tcl_Obj *empty;

	    TclNewObj(empty);
	    TclDictPut(NULL, dictPtr, options[2], empty);
	    TclDictPut(NULL, dictPtr, options[3], empty);
	}
	Tcl_SetObjResult(interp, dictPtr);
	return TCL_OK;
    } else if (objc == consumedObjc+1) {
	if (Tcl_GetIndexFromObj(interp, objv[consumedObjc], options, "option",
		0, &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	switch ((enum Options) index) {
	case OPT_CMD:
	    key.interp = childInterp;
	    key.type = TCL_LIMIT_TIME;
	    hPtr = Tcl_FindHashEntry(&iPtr->limit.callbacks, (char *) &key);
	    if (hPtr != NULL) {
		limitCBPtr = (ScriptLimitCallback *)Tcl_GetHashValue(hPtr);
		if (limitCBPtr != NULL && limitCBPtr->scriptObj != NULL) {
		    Tcl_SetObjResult(interp, limitCBPtr->scriptObj);
		}
	    }
	    break;
	case OPT_GRAN:
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(
		    Tcl_LimitGetGranularity(childInterp, TCL_LIMIT_TIME)));
	    break;
	case OPT_MILLI:
	    if (Tcl_LimitTypeEnabled(childInterp, TCL_LIMIT_TIME)) {
		Tcl_Time limitMoment;

		Tcl_LimitGetTime(childInterp, &limitMoment);
		Tcl_SetObjResult(interp,
			Tcl_NewLongObj(limitMoment.usec/1000));
	    }
	    break;
	case OPT_SEC:
	    if (Tcl_LimitTypeEnabled(childInterp, TCL_LIMIT_TIME)) {
		Tcl_Time limitMoment;

		Tcl_LimitGetTime(childInterp, &limitMoment);
		Tcl_SetObjResult(interp, Tcl_NewWideIntObj(limitMoment.sec));
	    }
	    break;
	}
	return TCL_OK;
    } else if ((objc-consumedObjc) & 1 /* isOdd(objc-consumedObjc) */) {
	Tcl_WrongNumArgs(interp, consumedObjc, objv, "?-option value ...?");
	return TCL_ERROR;
    } else {
	int i, scriptLen = 0, milliLen = 0, secLen = 0;
	Tcl_Obj *scriptObj = NULL, *granObj = NULL;
	Tcl_Obj *milliObj = NULL, *secObj = NULL;
	int gran = 0;
	Tcl_Time limitMoment;
	int tmp;

	Tcl_LimitGetTime(childInterp, &limitMoment);
	for (i=consumedObjc ; i<objc ; i+=2) {
	    if (Tcl_GetIndexFromObj(interp, objv[i], options, "option", 0,
		    &index) != TCL_OK) {
		return TCL_ERROR;
	    }
	    switch ((enum Options) index) {
	    case OPT_CMD:
		scriptObj = objv[i+1];
		(void) TclGetStringFromObj(objv[i+1], &scriptLen);
		break;
	    case OPT_GRAN:
		granObj = objv[i+1];
		if (TclGetIntFromObj(interp, objv[i+1], &gran) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (gran < 1) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "granularity must be at least 1", -1));
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADVALUE", (char *)NULL);
		    return TCL_ERROR;
		}
		break;
	    case OPT_MILLI:
		milliObj = objv[i+1];
		(void) TclGetStringFromObj(objv[i+1], &milliLen);
		if (milliLen == 0) {
		    break;
		}
		if (TclGetIntFromObj(interp, objv[i+1], &tmp) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (tmp < 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "milliseconds must be at least 0", -1));
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADVALUE", (char *)NULL);
		    return TCL_ERROR;
		}
		limitMoment.usec = ((long) tmp)*1000;
		break;
	    case OPT_SEC: {
		Tcl_WideInt sec;
		secObj = objv[i+1];
		(void) TclGetStringFromObj(objv[i+1], &secLen);
		if (secLen == 0) {
		    break;
		}
		if (TclGetWideIntFromObj(interp, objv[i+1], &sec) != TCL_OK) {
		    return TCL_ERROR;
		}
		if (sec > LONG_MAX) {
		    Tcl_SetObjResult(interp, Tcl_ObjPrintf(
			    "seconds must be between 0 and %ld", LONG_MAX));
		    goto badValue;
		}
		if (sec < 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "seconds must be at least 0", -1));
		badValue:
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADVALUE", (char *)NULL);
		    return TCL_ERROR;
		}
		limitMoment.sec = sec;
		break;
	    }
	    }
	}
	if (milliObj != NULL || secObj != NULL) {
	    if (milliObj != NULL) {
		/*
		 * Setting -milliseconds but clearing -seconds, or resetting
		 * -milliseconds but not resetting -seconds? Bad voodoo!
		 */

		if (secObj != NULL && secLen == 0 && milliLen > 0) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "may only set -milliseconds if -seconds is not "
			    "also being reset", -1));
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADUSAGE", (char *)NULL);
		    return TCL_ERROR;
		}
		if (milliLen == 0 && (secObj == NULL || secLen > 0)) {
		    Tcl_SetObjResult(interp, Tcl_NewStringObj(
			    "may only reset -milliseconds if -seconds is "
			    "also being reset", -1));
		    Tcl_SetErrorCode(interp, "TCL", "OPERATION", "INTERP",
			    "BADUSAGE", (char *)NULL);
		    return TCL_ERROR;
		}
	    }

	    if (milliLen > 0 || secLen > 0) {
		/*
		 * Force usec to be in range [0..1000000), possibly
		 * incrementing sec in the process. This makes it much easier
		 * for people to write scripts that do small time increments.
		 */

		limitMoment.sec += limitMoment.usec / 1000000;
		limitMoment.usec %= 1000000;

		Tcl_LimitSetTime(childInterp, &limitMoment);
		Tcl_LimitTypeSet(childInterp, TCL_LIMIT_TIME);
	    } else {
		Tcl_LimitTypeReset(childInterp, TCL_LIMIT_TIME);
	    }
	}
	if (scriptObj != NULL) {
	    SetScriptLimitCallback(interp, TCL_LIMIT_TIME, childInterp,
		    (scriptLen > 0 ? scriptObj : NULL));
	}
	if (granObj != NULL) {
	    Tcl_LimitSetGranularity(childInterp, TCL_LIMIT_TIME, gran);
	}
	return TCL_OK;
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
