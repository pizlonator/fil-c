#include <filc_llvm_jit.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef int v8i32 __attribute__((vector_size(32)));

/*
 * Exercise the shim's x86 AVX2 gather lowering.
 *
 * The guest module declares the raw llvm.x86.avx2.gather.d.d.256 intrinsic
 * (the shape mesa's gallivm emits for vertex/index fetch) and wraps it in a
 * small function that sums the gathered lanes.  The lowering in the shim
 * must turn the gather into per-lane predicated plain loads, so:
 *
 * - with an all-ones mask, the sum must reflect the memory contents
 *   (capability-checked loads of the caller's array), and
 * - a lane whose mask is clear must contribute the passthru value even
 *   though its index points into wild memory -- if the lowering ever
 *   performed that load, the capability check would panic instead of
 *   returning the passthru.
 */

#define NUM_LANES 8

int main()
{
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("test", context);
    LLVMTypeRef i32 = LLVMInt32TypeInContext(context);
    LLVMTypeRef i8 = LLVMInt8TypeInContext(context);
    LLVMTypeRef ptrty = LLVMPointerTypeInContext(context, 0);
    LLVMTypeRef v8 = LLVMVectorType(i32, NUM_LANES);

    LLVMTypeRef gatherType = LLVMFunctionType(
        v8, (LLVMTypeRef[]) { v8, ptrty, v8, v8, i8 }, 5, false);
    LLVMValueRef gatherFn = LLVMAddFunction(
        module, "llvm.x86.avx2.gather.d.d.256", gatherType);

    LLVMTypeRef testType = LLVMFunctionType(
        i32, (LLVMTypeRef[]) { ptrty, v8, v8, v8 }, 4, false);
    LLVMValueRef testFn = LLVMAddFunction(module, "gather_test", testType);
    LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(context, testFn, "root");
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder, block);

    LLVMValueRef arrParam = LLVMGetParam(testFn, 0);
    LLVMValueRef idxsParam = LLVMGetParam(testFn, 1);
    LLVMValueRef maskParam = LLVMGetParam(testFn, 2);
    LLVMValueRef passthruParam = LLVMGetParam(testFn, 3);

    LLVMValueRef gatherArgs[5] = {
        passthruParam, arrParam, idxsParam, maskParam,
        LLVMConstInt(i8, 1, false), /* byte scale, like mesa */
    };
    LLVMValueRef gathered = LLVMBuildCall2(
        builder, gatherType, gatherFn, gatherArgs, 5, "gathered");

    LLVMValueRef sum = LLVMBuildExtractElement(
        builder, gathered, LLVMConstInt(i32, 0, false), "");
    for (unsigned i = 1; i < NUM_LANES; ++i) {
        LLVMValueRef lane = LLVMBuildExtractElement(
            builder, gathered, LLVMConstInt(i32, i, false), "");
        sum = LLVMBuildAdd(builder, sum, lane, "");
    }
    LLVMBuildRet(builder, sum);

    /* Gather + shufflevector (mimics mesa's byte unpack: the gather result
     * feeds interleave shuffles before any extraction). */
    LLVMTypeRef shfType = LLVMFunctionType(
        i32, (LLVMTypeRef[]) { ptrty, v8, v8 }, 3, false);
    LLVMValueRef shfFn = LLVMAddFunction(module, "gather_shuffle", shfType);
    LLVMBasicBlockRef shfb = LLVMAppendBasicBlockInContext(context, shfFn, "root");
    LLVMBuilderRef shb = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(shb, shfb);
    LLVMValueRef shArr = LLVMGetParam(shfFn, 0);
    LLVMValueRef shIdxs = LLVMGetParam(shfFn, 1);
    LLVMValueRef shMask = LLVMGetParam(shfFn, 2);
    LLVMValueRef shArgs[5] = { LLVMConstNull(v8), shArr, shIdxs, shMask,
                               LLVMConstInt(i8, 1, false) };
    LLVMValueRef shG = LLVMBuildCall2(shb, gatherType, gatherFn, shArgs, 5,
                                      "shgather");
    /* interleave with a second (reversed) copy, then extract pairs */
    LLVMValueRef shrev = LLVMBuildShuffleVector(shb, shG, shG,
        LLVMConstVector((LLVMValueRef[]) {
            LLVMConstInt(i32, 7, false), LLVMConstInt(i32, 6, false),
            LLVMConstInt(i32, 5, false), LLVMConstInt(i32, 4, false),
            LLVMConstInt(i32, 3, false), LLVMConstInt(i32, 2, false),
            LLVMConstInt(i32, 1, false), LLVMConstInt(i32, 0, false)}, 8), "");
    LLVMValueRef shlo = LLVMBuildShuffleVector(shb, shG, shrev,
        LLVMConstVector((LLVMValueRef[]) {
            LLVMConstInt(i32, 0, false), LLVMConstInt(i32, 8, false),
            LLVMConstInt(i32, 1, false), LLVMConstInt(i32, 9, false),
            LLVMConstInt(i32, 2, false), LLVMConstInt(i32, 10, false),
            LLVMConstInt(i32, 3, false), LLVMConstInt(i32, 11, false)}, 8), "");
    LLVMValueRef shsum = LLVMBuildExtractElement(shb, shlo,
                                                 LLVMConstInt(i32, 0, false), "");
    for (unsigned i = 1; i < NUM_LANES; ++i)
        shsum = LLVMBuildAdd(shb, shsum,
                             LLVMBuildExtractElement(shb, shlo,
                                 LLVMConstInt(i32, i, false), ""), "");
    LLVMBuildRet(shb, shsum);

    /* Echo function: sums the lanes of a vector argument, so the host can
     * verify what the JIT'd code actually receives. */
    LLVMTypeRef sumvType = LLVMFunctionType(
        i32, (LLVMTypeRef[]) { v8 }, 1, false);
    LLVMValueRef sumvFn = LLVMAddFunction(module, "sum_vector", sumvType);
    LLVMBasicBlockRef svb = LLVMAppendBasicBlockInContext(context, sumvFn, "root");
    LLVMBuilderRef svbuilder = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(svbuilder, svb);
    LLVMValueRef sv = LLVMGetParam(sumvFn, 0);
    LLVMValueRef svsum = LLVMBuildExtractElement(svbuilder, sv,
                                                 LLVMConstInt(i32, 0, false), "");
    for (unsigned i = 1; i < NUM_LANES; ++i)
        svsum = LLVMBuildAdd(svbuilder, svsum,
                             LLVMBuildExtractElement(svbuilder, sv,
                                 LLVMConstInt(i32, i, false), ""), "");
    LLVMBuildRet(svbuilder, svsum);

    /* Second function with TWO gather call sites (e.g. mesa's draw module
     * lowers an elts fetch and an attribute fetch in the same variant):
     * gather from two different arrays and sum everything. */
    LLVMTypeRef test2Type = LLVMFunctionType(
        i32, (LLVMTypeRef[]) { ptrty, ptrty, v8, v8, v8, v8 }, 6, false);
    LLVMValueRef test2Fn = LLVMAddFunction(module, "gather_test2", test2Type);
    LLVMBasicBlockRef block2 = LLVMAppendBasicBlockInContext(context, test2Fn, "root");
    LLVMBuilderRef builder2 = LLVMCreateBuilderInContext(context);
    LLVMPositionBuilderAtEnd(builder2, block2);
    LLVMValueRef arr1P = LLVMGetParam(test2Fn, 0);
    LLVMValueRef arr2P = LLVMGetParam(test2Fn, 1);
    LLVMValueRef idxsP = LLVMGetParam(test2Fn, 2);
    LLVMValueRef maskP = LLVMGetParam(test2Fn, 3);
    LLVMValueRef pass1P = LLVMGetParam(test2Fn, 4);
    LLVMValueRef pass2P = LLVMGetParam(test2Fn, 5);
    LLVMValueRef g1args[5] = { pass1P, arr1P, idxsP, maskP,
                               LLVMConstInt(i8, 1, false) };
    LLVMValueRef g1 = LLVMBuildCall2(builder2, gatherType, gatherFn,
                                     g1args, 5, "g1");
    LLVMValueRef g2args[5] = { pass2P, arr2P, idxsP, maskP,
                               LLVMConstInt(i8, 1, false) };
    LLVMValueRef g2 = LLVMBuildCall2(builder2, gatherType, gatherFn,
                                     g2args, 5, "g2");
    LLVMValueRef sum2 = NULL;
    for (unsigned which = 0; which < 2; ++which) {
        LLVMValueRef vec = which ? g2 : g1;
        for (unsigned i = 0; i < NUM_LANES; ++i) {
            LLVMValueRef lane = LLVMBuildExtractElement(
                builder2, vec, LLVMConstInt(i32, i, false), "");
            sum2 = (which == 0 && i == 0)
                ? lane : LLVMBuildAdd(builder2, sum2, lane, "");
        }
    }
    LLVMBuildRet(builder2, sum2);

    FilLLVMJITRef jit = FilLLVMJIT_Create();
    if (getenv("GATHER_OPT0"))
        FilLLVMJIT_SetOption(jit, "opt-level", "0");
    FilLLVMJITModuleRef result = FilLLVMJIT_CompileModule(jit, module);
    int (*gather_test)(int *, v8i32, v8i32, v8i32) =
        (int (*)(int *, v8i32, v8i32, v8i32))
        FilLLVMJIT_GetPointerToGlobal(result, "gather_test");

    int array[16];
    for (unsigned i = 0; i < 16; ++i)
        array[i] = 1000 + (int) i * 3;

    v8i32 idxs, mask, passthru;
    for (unsigned i = 0; i < NUM_LANES; ++i) {
        /* byte offsets, scale 1 (exactly how mesa gathers dwords) */
        idxs[i] = (int) (4 * i);
        mask[i] = -1;
        passthru[i] = 70000 + (int) i;
    }

    /* All lanes active: the sum of the gathered elements. */
    int expected = 0;
    for (unsigned i = 0; i < NUM_LANES; ++i)
        expected += array[idxs[i] / 4];
    int got = gather_test(array, idxs, mask, passthru);
    if (got != expected) {
        printf("all-active: got %d, expected %d\n", got, expected);
        return 1;
    }

    /* Mask lane 3 off and point its index into wild memory: the passthru
     * must come back and the load must never happen. */
    idxs[3] = 0x7fffff00;
    mask[3] = 0;
    expected = 0;
    for (unsigned i = 0; i < NUM_LANES; ++i)
        expected += (mask[i] ? array[idxs[i] / 4] : passthru[i]);
    got = gather_test(array, idxs, mask, passthru);
    if (got != expected) {
        printf("masked: got %d, expected %d\n", got, expected);
        return 1;
    }

    /* two gathers in one function: both arrays gathered, both sums added */
    {
        int array2[16];
        for (unsigned i = 0; i < 16; ++i)
            array2[i] = 2000 + (int) i * 5;
        v8i32 idxs2, mask2, pass2;
        for (unsigned i = 0; i < NUM_LANES; ++i) {
            idxs2[i] = (int) (4 * i);
            mask2[i] = -1;
            pass2[i] = 80000 + (int) i;
        }
        int (*gather_test2)(int *, int *, v8i32, v8i32, v8i32, v8i32) =
            (int (*)(int *, int *, v8i32, v8i32, v8i32, v8i32))
            FilLLVMJIT_GetPointerToGlobal(result, "gather_test2");
        int exp2 = 0;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            exp2 += (mask[i] ? array[idxs[i] / 4] : passthru[i])
                  + (mask[i] ? array2[idxs[i] / 4] : pass2[i]);
        int got2 = gather_test2(array, array2, idxs, mask, passthru, pass2);
        if (got2 != exp2) {
            printf("two-gather: got %d, expected %d\n", got2, exp2);
            return 1;
        }
        /* now mask lane 2 off in BOTH gathers */
        mask[2] = 0;
        mask2[2] = 0;
        idxs[2] = 0x7fffff00;
        idxs2[2] = 0x7ffffe00;
        /* NOTE: the guest function takes ONE mask (both gathers share it);
         * mask2 is host-side only, so the expectation must use mask for
         * both terms. */
        int exp3 = 0;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            exp3 += (mask[i] ? array[idxs[i] / 4] : passthru[i])
                  + (mask[i] ? array2[idxs2[i] / 4] : pass2[i]);
        int got3 = gather_test2(array, array2, idxs, mask, passthru, pass2);
        if (got3 != exp3) {
            printf("two-gather-masked: got %d, expected %d\n", got3, exp3);
            int exp1 = 0;
            for (unsigned i = 0; i < NUM_LANES; ++i)
                exp1 += mask[i] ? array[idxs[i] / 4] : passthru[i];
            int got1 = gather_test(array, idxs, mask, passthru);
            printf("  single-gather same data: got %d, expected %d\n",
                   got1, exp1);
            int exp2m = 0;
            for (unsigned i = 0; i < NUM_LANES; ++i)
                exp2m += mask2[i] ? array2[idxs2[i] / 4] : pass2[i];
            printf("  g1 alone would be %d, g2 alone would be %d\n",
                   exp1, exp2m);
            return 1;
        }
    }

    /* Verify the vector arguments cross the JIT boundary intact. */
    {
        int (*sum_vector)(v8i32) =
            (int (*)(v8i32)) FilLLVMJIT_GetPointerToGlobal(result, "sum_vector");
        v8i32 probe;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            probe[i] = 100 * ((int) i + 1);
        int want = 0;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            want += probe[i];
        int gotv = sum_vector(probe);
        if (gotv != want) {
            printf("vector-arg: got %d, want %d\n", gotv, want);
            return 1;
        }
        probe[2] = 0;
        probe[5] = -7;
        want = 0;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            want += probe[i];
        gotv = sum_vector(probe);
        if (gotv != want) {
            printf("vector-arg-2: got %d, want %d\n", gotv, want);
            return 1;
        }
    }

    /* gather + shuffle: the shuffled sum must match the direct sum */
    {
        int (*gather_shuffle)(int *, v8i32, v8i32) =
            (int (*)(int *, v8i32, v8i32))
            FilLLVMJIT_GetPointerToGlobal(result, "gather_shuffle");
        v8i32 sidxs, smask;
        for (unsigned i = 0; i < NUM_LANES; ++i) {
            sidxs[i] = (int) (4 * i);
            smask[i] = -1;
        }
        int want = 0;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            want += array[sidxs[i] / 4];
        int got = gather_shuffle(array, sidxs, smask);
        if (got != want) {
            printf("gather-shuffle: got %d, want %d\n", got, want);
            return 1;
        }
        /* and with lane 3 masked off */
        smask[3] = 0;
        want = 0;
        for (unsigned i = 0; i < NUM_LANES; ++i)
            want += smask[i] ? array[sidxs[i] / 4] : 0;
        got = gather_shuffle(array, sidxs, smask);
        if (got != want) {
            printf("gather-shuffle-masked: got %d, want %d\n", got, want);
            return 1;
        }
    }

    printf("JIT-GATHER PASS\n");
    return 0;
}
