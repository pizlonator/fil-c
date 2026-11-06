/* Population count through the LLVM JIT: llvm.ctpop on <16 x i32>. Unlike
   the x86-specific intrinsics, ctpop is an overloaded LLVM intrinsic, so
   this test exercises the overloaded-declaration idiom: declareIntrinsic
   passes Tys (<16 x i32>) iff LLVMIntrinsicIsOverloaded says so. Beyond
   that, the claim is numerics only: the JITed popcounts match a serial bit
   loop. The run-tests needsAVX512 predicate keeps this off hosts without
   AVX512.
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

#define LANES 16

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

static __attribute__((noinline)) void (*buildCtpopFn(void))(unsigned*, unsigned*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(i32Type, LANES);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[2] = { ptrType, ptrType };
    LLVMTypeRef functionType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 2, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMTypeRef tys[1] = { vecType };
    LLVMTypeRef callType;
    LLVMValueRef intrinsic = declareIntrinsic(
        module, context, "llvm.ctpop", tys, 1, &callType);
    LLVMValueRef a = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef args[1] = { a };
    LLVMValueRef res = LLVMBuildCall2(builder, callType, intrinsic, args, 1, "counts");
    LLVMBuildStore(builder, res, LLVMGetParam(function, 1));
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(unsigned*, unsigned*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

int main()
{
    void (*test)(unsigned*, unsigned*) = buildCtpopFn();

    _Alignas(64) unsigned a[LANES];
    _Alignas(64) unsigned c[LANES];
    int i;
    for (i = 0; i < LANES; ++i)
        a[i] = 0x0f0f0f0fu * (unsigned)(i + 1) + (unsigned)i + (1u << (i % 32));

    test((unsigned*)opaque(a), (unsigned*)opaque(c));

    for (i = 0; i < LANES; ++i) {
        unsigned x = a[i];
        int want = 0;
        while (x) {
            want += (int)(x & 1);
            x >>= 1;
        }
        if (c[i] != (unsigned)want) {
            fprintf(stderr, "lane %d: %u, expected %d\n", i, c[i], want);
            return 1;
        }
    }

    printf("jit simd avx512 ctpop ok\n");
    return 0;
}
