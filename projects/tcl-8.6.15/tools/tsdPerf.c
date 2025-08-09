#include <tcl.h>

extern DLLEXPORT Tcl_PackageInitProc Tsdperf_Init;

static Tcl_ThreadDataKey key;

typedef struct {
    int value;
} TsdPerf;


static int
tsdPerfSetObjCmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
    TsdPerf *perf = Tcl_GetThreadData(&key, sizeof(TsdPerf));
    int i;

    if (2 != objc) {
	Tcl_WrongNumArgs(interp, 1, objv, "value");
	return TCL_ERROR;
    }

    if (TCL_OK != Tcl_GetIntFromObj(interp, objv[1], &i)) {
	return TCL_ERROR;
    }

    perf->value = i;

    return TCL_OK;
}

static int
tsdPerfGetObjCmd(ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
    TsdPerf *perf = Tcl_GetThreadData(&key, sizeof(TsdPerf));


    Tcl_SetObjResult(interp, Tcl_NewIntObj(perf->value));

    return TCL_OK;
}

int
Tsdperf_Init(Tcl_Interp *interp) {
    if (Tcl_InitStubs(interp, "8.5-", 0) == NULL) {
	return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "tsdPerfSet", tsdPerfSetObjCmd, NULL, NULL);
    Tcl_CreateObjCommand(interp, "tsdPerfGet", tsdPerfGetObjCmd, NULL, NULL);

    return TCL_OK;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
