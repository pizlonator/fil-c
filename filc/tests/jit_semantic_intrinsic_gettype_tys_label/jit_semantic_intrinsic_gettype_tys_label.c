#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* LLVMIntrinsicGetType with a label type parameter.  llvm.fma's return and
   first argument share one type parameter, so a label type becomes the
   function's return type and trips isValidReturnType (Type.cpp:345).  The
   decode mirroring in the wrapper must panic instead.  Deliberate tripwire:
   the manifest's output-excludes greps for LLVM's assert text, so if the
   check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    unsigned fma_id = LLVMLookupIntrinsicID("llvm.fma", 8);
    if (!fma_id || !LLVMIntrinsicIsOverloaded(fma_id)) {
        fprintf(stderr, "bad: llvm.fma not found\n");
        abort();
    }
    LLVMTypeRef tys[1];
    tys[0] = LLVMLabelTypeInContext(context);
    LLVMIntrinsicGetType(context, fma_id, tys, 1);
    return 0;
}
