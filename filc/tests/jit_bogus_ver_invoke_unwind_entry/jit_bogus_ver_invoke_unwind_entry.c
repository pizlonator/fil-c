#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdlib.h>

/* Malformed IR: an invoke whose unwind destination is the entry block (and
   the entry block has no landing pad).  CompileModule must panic "filc llvm
   jit error" (exit 42) with the verifier's "Entry block to function must not
   have predecessors!". */

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i32Fn = LLVMFunctionType(i32, NULL, 0, false);
    LLVMValueRef fn = LLVMAddFunction(module, "test", i32Fn);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "root");
    LLVMBasicBlockRef cont = LLVMAppendBasicBlockInContext(context, fn, "cont");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMBuildInvoke2(builder, i32Fn, fn, NULL, 0, cont, entry, "");

    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, cont);
    LLVMBuildRet(builder2, LLVMConstInt(i32, 0, false));

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_CompileModule(jit, module);

    return 0;
}
