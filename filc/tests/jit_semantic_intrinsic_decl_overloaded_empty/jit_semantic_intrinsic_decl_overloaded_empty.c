#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* LLVMGetIntrinsicDeclaration of an overloaded intrinsic with no parameter
   types.  The declaration getter indexes the Tys array while decoding the
   intrinsic's type table, so an empty Tys for an overloaded id hits the
   "Invalid index!" ArrayRef assert (ArrayRef.h).  This must panic in the
   wrapper instead of that out-of-bounds read.
   Deliberate tripwire: the manifest's output-excludes greps for LLVM's assert
   text, so if the check ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned abs_id = LLVMLookupIntrinsicID("llvm.abs", 8);
    if (!abs_id || !LLVMIntrinsicIsOverloaded(abs_id)) {
        fprintf(stderr, "bad: llvm.abs lookup failed\n");
        abort();
    }
    LLVMGetIntrinsicDeclaration(module, abs_id, NULL, 0);
    return 0;
}
