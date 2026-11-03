/* GC liveness for module data: the JITed function reads its own module's
   internal string global. After the compile helper returns only the function
   pointer is kept; forced GC plus churn must preserve both the code and the
   module's data symbols. The fixed GC-race scenario from jit_hello_bad1, but
   exercised through a data read instead of a host callback. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>
#include <string.h>

static const char* theString = "gc string global survives";

static __attribute__((noinline)) int (*buildTest(void))(int)
{
    size_t len = strlen(theString);
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMValueRef stringConst = LLVMConstStringInContext(
        context, theString, (unsigned)len, false);
    LLVMValueRef stringGlobal = LLVMAddGlobal(
        module, LLVMTypeOf(stringConst), "msg");
    LLVMSetInitializer(stringGlobal, stringConst);
    LLVMSetLinkage(stringGlobal, LLVMInternalLinkage);

    LLVMTypeRef paramTypes[1] = { LLVMInt32TypeInContext(context) };
    LLVMTypeRef functionType = LLVMFunctionType(
        LLVMInt32TypeInContext(context), paramTypes, 1, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef index = LLVMGetParam(function, 0);
    LLVMTypeRef i8Type = LLVMInt8TypeInContext(context);
    LLVMValueRef elementPtr = LLVMBuildInBoundsGEP2(
        builder, i8Type, stringGlobal, &index, 1, "msgp");
    LLVMValueRef byte = LLVMBuildLoad2(builder, i8Type, elementPtr, "b");
    LLVMValueRef result = LLVMBuildZExt(builder, byte, LLVMInt32TypeInContext(context), "r");
    LLVMBuildRet(builder, result);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    int (*test)(int) = FilLLVMJIT_GetPointerToGlobal(compiled, "test");
    return test;
}

int main()
{
    int (*test)(int) = buildTest();

    unsigned k;
    for (k = 0; k < 10; ++k) {
        void* churn = zgc_alloc(1024 * 1024);
        (void)churn;
        zgc_request_and_wait();
    }

    int i;
    for (i = 0; theString[i]; ++i) {
        int r = test(i);
        if (r != (unsigned char)theString[i]) {
            fprintf(stderr, "bad byte %d at %d, expected %d\n",
                    r, i, (unsigned char)theString[i]);
            return 1;
        }
    }
    if (test((int)strlen(theString)) != 0) {
        fprintf(stderr, "bad null terminator read\n");
        return 1;
    }

    printf("gc string ok\n");
    return 0;
}
