#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

/* Success control for the LLVMFunctionType parameter checks: label and
   metadata parameter types are legal (isValidArgumentType only rejects
   function and void), so a function type taking label and metadata
   parameters must build fine. */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef label = LLVMLabelTypeInContext(context);
    LLVMTypeRef metadata = LLVMMetadataTypeInContext(context);
    LLVMTypeRef params[2];
    params[0] = label;
    params[1] = metadata;
    LLVMTypeRef fn_ty = LLVMFunctionType(i32, params, 2, false);
    if (!fn_ty || LLVMGetTypeKind(fn_ty) != LLVMFunctionTypeKind ||
        LLVMCountParamTypes(fn_ty) != 2) {
        fprintf(stderr, "bad: function type did not come back\n");
        abort();
    }
    printf("fn params label md ok\n");
    return 0;
}
