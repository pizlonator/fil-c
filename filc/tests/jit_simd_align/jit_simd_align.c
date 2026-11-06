/* Deliberately misaligned wide-vector memory through the LLVM JIT: <8 x i32>
   loads and a store at pointers that are offset by odd byte counts into
   64-byte-aligned storage. A <8 x i32> access has an ABI alignment of 32 in
   the x86_64 data layout, so a load/store that just inherits the ABI
   alignment may be lowered to an aligned move (vmovaps) — which #GP-faults
   on a misaligned buffer. Passing LLVMSetAlignment(..., 1) on the memory
   instructions tells the backend the accesses are unaligned so they lower
   to unaligned moves (vmovups) instead. This test would crash (or corrupt)
   without the alignment fixup.
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

#define LANES 8
/* One aligned slab; the three buffers are carved out at odd offsets. */
#define SLAB (64 * 3 + 8)

static __attribute__((noinline)) void (*buildAddFn(void))(char*, char*, char*)
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32Type = LLVMInt32TypeInContext(context);
    LLVMTypeRef vecType = LLVMVectorType(i32Type, LANES);
    LLVMTypeRef ptrType = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef paramTypes[3] = { ptrType, ptrType, ptrType };
    LLVMTypeRef functionType =
        LLVMFunctionType(LLVMVoidTypeInContext(context), paramTypes, 3, false);
    LLVMValueRef function = LLVMAddFunction(module, "test", functionType);
    LLVMBasicBlockRef rootBlock = LLVMAppendBasicBlockInContext(context, function, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, rootBlock);
    LLVMValueRef a = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 0), "a");
    LLVMValueRef b = LLVMBuildLoad2(builder, vecType, LLVMGetParam(function, 1), "b");
    LLVMValueRef sum = LLVMBuildAdd(builder, a, b, "sum");
    LLVMValueRef store = LLVMBuildStore(builder, sum, LLVMGetParam(function, 2));
    /* The ABI alignment of <8 x i32> is 32, but the buffers are misaligned:
       force byte alignment so the backend emits unaligned moves. */
    LLVMSetAlignment(a, 1);
    LLVMSetAlignment(b, 1);
    LLVMSetAlignment(store, 1);
    LLVMBuildRetVoid(builder);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    FilLLVMJITModuleRef compiled = FilLLVMJIT_CompileModule(jit, module);
    return (void (*)(char*, char*, char*))FilLLVMJIT_GetPointerToGlobal(compiled, "test");
}

int main()
{
    void (*test)(char*, char*, char*) = buildAddFn();

    _Alignas(64) char slab[SLAB];
    /* Offsets chosen so that no buffer is 4-byte aligned, let alone 32. */
    char* pa = slab + 3;
    char* pb = slab + 64 + 5;
    char* pc = slab + 128 + 7;

    _Alignas(64) int av[LANES];
    _Alignas(64) int bv[LANES];
    int i;
    for (i = 0; i < LANES; ++i) {
        av[i] = i * 4096 + i;
        bv[i] = -(i * 1000) - 1;
    }
    memcpy(pa, av, LANES * 4);
    memcpy(pb, bv, LANES * 4);

    test((char*)opaque(pa), (char*)opaque(pb), (char*)opaque(pc));

    for (i = 0; i < LANES; ++i) {
        int got;
        memcpy(&got, pc + i * 4, 4);
        if (got != av[i] + bv[i]) {
            fprintf(stderr, "lane %d: %d, expected %d\n", i, got, av[i] + bv[i]);
            return 1;
        }
    }

    printf("jit simd align ok\n");
    return 0;
}
