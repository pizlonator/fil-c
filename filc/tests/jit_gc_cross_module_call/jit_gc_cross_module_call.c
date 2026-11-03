/* Cross-module JIT-to-JIT call surviving GC: module B is compiled first and
   its function pointer is mapped into module A's session, so A's JITed code
   calls B's JITed code. All contexts/modules/builders die in the helpers;
   the executable-module refs and function pointers are kept. Forced GC must
   not break the A -> B edge. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

static FilLLVMJITModuleRef bModule; /* kept alive on purpose */
static FilLLVMJITModuleRef aModule; /* kept alive on purpose */

static __attribute__((noinline)) int (*buildB(void))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("testB", context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "bFunc", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef x = LLVMGetParam(function, 0);
    LLVMValueRef result = LLVMBuildMul(
        builder, x, LLVMConstInt(LLVMInt32TypeInContext(context), 7, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    bModule = FilLLVMJIT_CompileModule(jit, module);
    int (*bFunc)(int) = FilLLVMJIT_GetPointerToGlobal(bModule, "bFunc");
    return bFunc;
}

static __attribute__((noinline)) int (*buildA(int (*bFunc)(int)))(int)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("testA", context);
    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMValueRef bFuncValue = LLVMAddFunction(module, "bFunc", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef x = LLVMGetParam(function, 0);
    LLVMValueRef args[1] = { x };
    LLVMValueRef forwarded = LLVMBuildCall2(
        builder, functionType, bFuncValue, args, 1, "forwarded");
    LLVMValueRef result = LLVMBuildAdd(
        builder, forwarded, LLVMConstInt(LLVMInt32TypeInContext(context), 1, false), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJIT_SetGlobalMapping(jit, "bFunc", bFunc);
    aModule = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(aModule, "test");
    return test;
}

int main()
{
    int (*bFunc)(int) = buildB();
    int (*test)(int) = buildA(bFunc);

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();

        int rb = bFunc(3 + (int)k);
        int ra = test(3 + (int)k);
        if (rb != (3 + (int)k) * 7) {
            fprintf(stderr, "round %u: bFunc returned %d\n", k, rb);
            return 1;
        }
        if (ra != (3 + (int)k) * 7 + 1) {
            fprintf(stderr, "round %u: A returned %d\n", k, ra);
            return 1;
        }
    }

    printf("gc xmod ok\n");
    return 0;
}
