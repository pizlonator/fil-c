/* DELIBERATE TRIPWIRE: pins a known JIT shim limitation. A module whose IR
   contains a loop back-edge cannot be compiled — the FilPizlonator pass
   needs a filc_pollcheck_slow safepoint inside the loop, and the JIT's
   resolver has no mapping for it, so linking fails with
   "JIT link failed: unresolved external symbol: filc_pollcheck_slow"
   (LLVM then trips its own assertion, hence a crash, not an exit-42 panic).
   This is why jit_safe_boundary_sum, jit_safe_two_way_args, and
   jit_safe_many_calls unroll their sums instead of looping.
   If someone fixes the shim to resolve filc_pollcheck_slow, THIS TEST STARTS
   FAILING — that is the signal to remove it and to replace the unrolled
   workarounds in jit_safe_boundary_sum / jit_safe_two_way_args /
   jit_safe_many_calls with real loops.
   The back-edge condition is laundered through the mapped opaque() helper so
   nothing folds it away; at runtime it would be false, so a fixed shim would
   run one iteration and exit rather than hang. */

#include <filc_llvm_jit.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef opaqueType = LLVMFunctionType(ptrType, &ptrType, 1, false);
    LLVMTypeRef functionType = LLVMFunctionType(ptrType, NULL, 0, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef callee = LLVMAddFunction(module, "opaque", opaqueType);

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, function, "entry");
    LLVMBasicBlockRef loop = LLVMAppendBasicBlockInContext(context, function, "loop");
    LLVMBasicBlockRef exit = LLVMAppendBasicBlockInContext(context, function, "exit");

    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMBuildBr(builder, loop);

    LLVMPositionBuilderAtEnd(builder, loop);
    /* The condition is opaque to the compiler (it comes out of the mapped
       host helper) but false at runtime, so the IR back-edge cannot be
       folded away and a fixed shim would terminate immediately. */
    LLVMValueRef zeroPtr = LLVMConstNull(ptrType);
    LLVMValueRef args[1] = { zeroPtr };
    LLVMValueRef result = LLVMBuildCall2(builder, opaqueType, callee, args, 1, "r");
    LLVMValueRef flag = LLVMBuildICmp(builder, LLVMIntNE, result, zeroPtr, "flag");
    LLVMBuildCondBr(builder, flag, loop, exit);

    LLVMPositionBuilderAtEnd(builder, exit);
    LLVMBuildRet(builder, zeroPtr);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "opaque", opaque);
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    if (compiled) {
        fprintf(stderr, "unexpectedly compiled a loop back-edge\n");
        return 1;
    }

    printf("bogus loop backedge ok\n");
    return 0;
}
