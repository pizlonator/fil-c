#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdlib.h>

int main()
{
    LLVMContextRef ctx = LLVMContextCreate();
    LLVMModuleRef mod = LLVMModuleCreateWithNameInContext("test", ctx);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(ctx);
    LLVMValueRef ci32 = LLVMConstInt(i32, 42, false);
    LLVMValueRef g = LLVMAddGlobal(mod, i32, "g");
    LLVMSetInitializer(g, ci32);
    LLVMMetadataRef mdmeta = LLVMMDStringInContext2(ctx, "s", 1);

    /* LLVMGlobalSetMetadata requires a llvm::MDNode; the MDString metadata
       mdmeta is not one.  This must panic in the wrapper instead of tripping an
       LLVM assertion. */
    LLVMGlobalSetMetadata(g, 0, mdmeta);

    return 0;
}
