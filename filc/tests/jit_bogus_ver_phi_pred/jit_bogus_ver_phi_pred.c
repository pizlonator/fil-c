#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Malformed IR: a phi whose incoming block is not a predecessor.  "other" has
   exactly one predecessor ("root"), but the phi records "other" itself as the
   incoming block.  CompileModule must panic "filc llvm jit error" (exit 42)
   with the verifier's "PHI node entries do not match predecessors!". */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef other = LLVMAppendBasicBlockInContext(context, fn, "other");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildBr(builder, other);

    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, other);
    LLVMValueRef phi = LLVMBuildPhi(builder2, i32, "p");
    LLVMValueRef values[1] = { LLVMConstInt(i32, 42, false) };
    LLVMBasicBlockRef blocks[1] = { other };
    LLVMAddIncoming(phi, values, blocks, 1);
    LLVMBuildRet(builder2, phi);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
