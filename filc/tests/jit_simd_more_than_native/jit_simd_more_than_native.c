/* The "more than native" overshoot: this JITed function requests
   "target-features" "+avx512fp16" — a feature the HOST may not have — and
   calls llvm.x86.avx512fp16.add.ph.512 (VADDPH zmm). On a host without
   AVX512-FP16 the JIT must not pretend: with this toolchain the compile
   fails hard (LLVM ERROR: Cannot select -> abort => SIGABRT); with a
   toolchain that can select the instruction, executing it SIGILLs. Either
   way the process dies nonzero, which the manifest's return: crash expects.
   Kept out of the default run with skip: true; run it deliberately with
   --test jit_simd_more_than_native.
   NOTE: the JITed function is loop-free because JITed code cannot contain IR
   loops yet. Pointers crossing from the host into JITed code are laundered
   through opaque() so the JITed bounds checks stay honest. */

#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
#include "utils.h"

#define LANES 32

/* Looks up name, fails loudly (exit 1) if it does not resolve, and declares
   it in module. Tys are passed iff the intrinsic is overloaded. The
   intrinsic's FunctionType (for LLVMBuildCall2) is stored through
   outFunctionType. */
static LLVMValueRef declareIntrinsic(LLVMModuleRef module, LLVMContextRef context,
                                     const char* name, LLVMTypeRef* tys, unsigned nTys,
                                     LLVMTypeRef* outFunctionType)
{
    unsigned id = LLVMLookupIntrinsicID(name, strlen(name));
    if (!id) {
        fprintf(stderr, "no intrinsic named %s\n", name);
        exit(1);
    }
    LLVMBool overloaded = LLVMIntrinsicIsOverloaded(id);
    *outFunctionType = LLVMIntrinsicGetType(
        context, id, overloaded ? tys : NULL, overloaded ? (size_t)nTys : 0);
    return LLVMGetIntrinsicDeclaration(
        module, id, overloaded ? tys : NULL, overloaded ? (size_t)nTys : 0);
}

int main()
{
    printf("about to overshoot\n");
    fflush(stdout);

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef f16Type = LLVMHalfTypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(f16Type, LANES);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[3] = { ptrType, ptrType, ptrType };
    LLVMTypeRef functionType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 3, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    /* The overshoot: demand a feature the host may not have. */
    LLVMAddTargetDependentFunctionAttr(function, "target-features", "+avx512fp16");
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMTypeRef tys[1] = { vecType };
    LLVMTypeRef callType;
    LLVMValueRef intrinsic = declareIntrinsic(
        module, context, "llvm.x86.avx512fp16.add.ph.512", tys, 1, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 1), "b");
    LLVMValueRef args[3] = {
        a,
        b,
        LLVMConstAllOnes(LLVMInt32TypeInContext(context)) /* 32-lane write mask */
    };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 3, "sum");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 2));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    void (*test)(char*, char*, char*) = (void (*)(char*, char*, char*))
        FilLLVMJIT_GetPointerToGlobal(compiled, "test");

    _Alignas(64) char storage[64 * 3];
    test((char*)opaque(storage), (char*)opaque(storage + 64),
         (char*)opaque(storage + 128));

    /* Only reachable on a host that actually executed VADDPH. */
    printf("jit simd overshoot executed (host has AVX512-FP16!)\n");
    return 0;
}
