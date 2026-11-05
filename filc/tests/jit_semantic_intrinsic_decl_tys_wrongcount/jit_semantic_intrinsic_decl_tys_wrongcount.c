#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* LLVMGetIntrinsicDeclaration with a wrong-count Tys.  llvm.annotation has
   two type parameters, so a one-element Tys sends the type-table decode
   past the end of the array (ArrayRef.h: "Invalid index!").  The decode
   mirroring in the wrapper must panic instead.  Deliberate tripwire: the
   manifest's output-excludes greps for LLVM's assert text, so if the check
   ever fails to fire the test fails loudly. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    unsigned annotation_id = LLVMLookupIntrinsicID("llvm.annotation", 15);
    if (!annotation_id || !LLVMIntrinsicIsOverloaded(annotation_id)) {
        fprintf(stderr, "bad: llvm.annotation not found\n");
        abort();
    }
    LLVMTypeRef f32 = LLVMFloatTypeInContext(context);
    LLVMTypeRef tys[1];
    tys[0] = f32;
    LLVMGetIntrinsicDeclaration(module, annotation_id, tys, 1);
    return 0;
}
