#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMIntrinsicCopyOverloadedName2 of an overloaded intrinsic with no
   parameter types.  The Copy2 native is documented as copying the name of the
   intrinsic for the given id and types, so passing no types for an overloaded
   id asks for the name of an intrinsic instance that cannot exist; the
   wrapper applies the same exact Tys/overloadedness rule as
   LLVMGetIntrinsicDeclaration.  (LLVM itself happens to return the unmangled
   base name here, so this pins the API contract rather than an abort.) */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned abs_id = LLVMLookupIntrinsicID("llvm.abs", 8);
    size_t length = 0;
    if (!abs_id || !LLVMIntrinsicIsOverloaded(abs_id)) {
        fprintf(stderr, "bad: llvm.abs lookup failed\n");
        abort();
    }
    LLVMIntrinsicCopyOverloadedName2(module, abs_id, NULL, 0, &length);
    return 0;
}
