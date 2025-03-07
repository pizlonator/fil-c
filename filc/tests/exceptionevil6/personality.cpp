#include <stdfil.h>
#include <unwind.h>
#include <inttypes.h>

extern "C" _Unwind_Reason_Code __gxx_personality_v0(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    _Unwind_Exception* unwind_exception, _Unwind_Context* context)
{
    static unsigned count;
    zprintf("personality: count = %u.\n", count);
    zdump_stack();
    if (++count != 2)
        return _URC_CONTINUE_UNWIND;
    if (actions & _UA_SEARCH_PHASE) {
        zprintf("personality: search.\n");
        return _URC_HANDLER_FOUND;
    }
    zprintf("personality: install.\n");
    return _URC_INSTALL_CONTEXT;
}
