#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* LLVMIntrinsicCopyOverloadedName2 with a label type parameter.  llvm.memset
   takes two type parameters (the pointer and the length's integer), so a
   {label, i32} Tys decodes fine -- llvm.memset has a fixed void return, so
   the label only lands in an argument slot the FunctionType constructor
   accepts -- but the same Tys feeds getMangledTypeStr (Intrinsics.cpp),
   which llvm_unreachables ("Unhandled type") for label (and token) types.
   The wrapper's mangle check must panic instead.  Deliberate tripwire: the
   manifest's output-excludes greps for LLVM's assert text, so if the check
   ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned memset_id = LLVMLookupIntrinsicID("llvm.memset", 11);
    if (!memset_id || !LLVMIntrinsicIsOverloaded(memset_id)) {
        fprintf(stderr, "bad: llvm.memset not found\n");
        abort();
    }
    LLVMTypeRef tys[2];
    tys[0] = LLVMLabelTypeInContext(context);
    tys[1] = LLVMInt32TypeInContext(context);
    size_t name_len = 0;
    LLVMIntrinsicCopyOverloadedName2(module, memset_id, tys, 2, &name_len);
    return 0;
}
