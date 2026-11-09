//===- RegisterFilPasses.cpp - Fil-C pass pipeline registration ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// (c) 2026 Filip Pizlo. All Rights Reserved.
//
// This file is a part of Fil-C. https://fil-c.org/
//
//===----------------------------------------------------------------------===//

#include "llvm/Passes/RegisterFilPasses.h"

#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Coroutines/CoroCleanup.h"
#include "llvm/Transforms/Coroutines/CoroConditionalWrapper.h"
#include "llvm/Transforms/Coroutines/CoroEarly.h"
#include "llvm/Transforms/Coroutines/CoroSplit.h"
#include "llvm/Transforms/IPO/FunctionAttrs.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
#include "llvm/Transforms/IPO/Inliner.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Instrumentation/DeleteRedundantPollchecks.h"
#include "llvm/Transforms/Instrumentation/FilPizlonator.h"
#include "llvm/Transforms/Instrumentation/KillUB.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/CorrelatedValuePropagation.h"
#include "llvm/Transforms/Scalar/DeadStoreElimination.h"
#include "llvm/Transforms/Scalar/EarlyCSE.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/JumpThreading.h"
#include "llvm/Transforms/Scalar/LowerExpectIntrinsic.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/Transforms/Scalar/SROA.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"

using namespace llvm;

static cl::opt<bool> FilCKillUB(
  "filc-killub", cl::desc("Run the KillUB pass"), cl::Hidden, cl::init(true));
static cl::opt<bool> FilCOptimize(
  "filc-optimize", cl::desc("Run Fil-C optimization pipeline"), cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInline(
  "filc-inline", cl::desc("Run Fil-C inlining pipeline"), cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineSROA(
  "filc-inline-sroa", cl::desc("Run SROA during Fil-C inlining pipeline"), cl::Hidden,
  cl::init(true));
static cl::opt<bool> FilCInlineEarlyCSE(
  "filc-inline-early-cse", cl::desc("Run EarlyCSE during Fil-C inlining pipeline"), cl::Hidden,
  cl::init(true));
static cl::opt<bool> FilCInlineJumpThreading(
  "filc-inline-jump-threading", cl::desc("Run JumpThreading during Fil-C inlining pipeline"),
  cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineCVP(
  "filc-inline-cvp", cl::desc("Run CorrelatedValuePropagation during Fil-C inlining pipeline"),
  cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineSimplifyCFG(
  "filc-inline-simplify-cfg", cl::desc("Run SimplifyCFG during Fil-C inlining pipeline"),
  cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineInstCombineEarly(
  "filc-inline-inst-combine-early",
  cl::desc("Run InstCombine during Fil-C inlining pipeline (early)"),
  cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineInstCombineLate(
  "filc-inline-inst-combine-late",
  cl::desc("Run InstCombine during Fil-C inlining pipeline (late)"),
  cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineGVN(
  "filc-inline-gvn", cl::desc("Run GVN during Fil-C inlining pipeline"), cl::Hidden, cl::init(true));
static cl::opt<bool> FilCInlineSCCP(
  "filc-inline-sccp", cl::desc("Run SCCP during Fil-C inlining pipeline"), cl::Hidden,
  cl::init(true));
static cl::opt<bool> FilCInlineADCE(
  "filc-inline-adce", cl::desc("Run ADCE during Fil-C inlining pipeline"), cl::Hidden,
  cl::init(true));
static cl::opt<bool> FilCInlineDSE(
  "filc-inline-dse", cl::desc("Run DSE during Fil-C inlining pipeline"), cl::Hidden, cl::init(true));
static cl::opt<bool> FilCDSE(
  "filc-dse", cl::desc("Run DSE during Fil-C pipeline"), cl::Hidden, cl::init(false));

// The Fil-C pipeline-start passes: KillUB, then (above O0) the Fil-C early
// simplification and inlining pipeline, then C++20 coroutine lowering (so that
// FilPizlonator never sees llvm.coro.* intrinsics), and finally the
// FilPizlonator itself.  This is the directly-applicable form of the
// pipeline-start half of RegisterFilPasses() below: frontends whose pass
// pipelines are not always built through a PassBuilder (e.g. rustc's
// -C no-prepopulate-passes modules, which apply their extension-point passes
// to a fresh ModulePassManager instead of building a default pipeline) call
// this so that they pizlonate exactly like the PassBuilder-driven paths do.
//
// Every frontend gets the same pipeline-start block.  clang registers it with
// no arguments (clang/lib/CodeGen/BackendUtil.cpp) and the Fil-C rustc
// registers it through the same entry points (see PassWrapper.cpp in the
// projeny patch), so rust-compiled modules -- including std itself -- get
// KillUB -> pre-pizlonation simplification/inlining -> coro lowering ->
// FilPizlonator exactly like clang-compiled ones, with the regular
// optimization pipeline still running after the FilPizlonator on the
// pizlonated IR -- so codegen is fully optimized in both cases.  The block
// used to be gated off for rustc (an IsRust parameter, now gone, plus a
// FILCRUST_FILCINLINE environment override) because the pre-pizlonation
// pipeline miscompiled rustc-shaped IR in two classes:
//
//  - The spawn+join shape (std::thread::spawn + join at -O handing
//    pthread_join a thread handle whose flight-pointer capability had been
//    dropped): fixed by the rustc-side typed allocas -- aggregates that may
//    contain pointers get typed allocas with all-pointer struct types
//    (filc_may_contain_pointers / filc_ptr_struct_type in the projeny patch,
//    mirroring clang's union representation hack from
//    CGRecordLayoutBuilder.cpp commit 4a54ae4076b4), so SROA's type-driven
//    partitioning gives the memcpy-only partitions of those allocas pointer
//    types and the copied capabilities survive.
//
//  - The mem::swap class (std::mem::swap of pointer-containing elements at
//    -O): std's swap_nonoverlapping_chunks moved the bytes of swapped
//    elements through MaybeUninit<[u8; N]> block buffers (N =
//    size_of::<*const ()>()), which are NUMERIC-typed allocas that
//    filc_may_contain_pointers correctly declines, so upstream SROA promoted
//    their partitions and widened the typed byte copies into integer (i64)
//    load/store pairs.  Under GIMSO semantics that is NOT
//    semantics-preserving: the Fil-Pizlonator moves payload+shadow for
//    aggregate/byte copies (a pointer at rest carries its capability in the
//    shadow of the copied bytes), while integer loads/stores touch only the
//    payload, so the capabilities of the pointers inside the swapped element
//    were dropped at the swap.  The reviewer-shaped repro was a Vec of
//    40-byte Node structs ({String, *mut u8, u32}) shuffled with
//    std::mem::swap: at -O the subsequent Vec::clone -> String::clone memmove
//    then read a source pointer whose capability the swap destroyed ("filc
//    safety error: cannot read source pointer with ptr < lower" / "ptr >=
//    upper", exit 133).  This is fixed in std itself now
//    (library/core/src/ptr/mod.rs in the projeny patch):
//    swap_nonoverlapping_bytes swaps pointer-aligned regions in pointer-typed
//    MaybeUninit<*const ()> chunks, so the swap's copies stay
//    capability-preserving however far the pipeline reshapes them -- the same
//    house pattern the Fil-C user libc uses for scratch memory that receives
//    whole elements (qsort's) and for may-hold-pointer slots generally.
//
// The SROA-side guard that would have closed the mem::swap class without the
// std change (in the rewriter: never widen a memintrinsic use into a
// non-pointer-typed access) was rejected for performance: it pointer-types the
// memcpy-only partitions of NUMERIC aggregates too, and pointer-typed
// partitions make the pizlonator emit pointer checks for data that is just
// bytes -- that cost ~1.4x on simdutf.  That fix lives on the fil/fixed_sroa
// branch and is NOT in this tree (llvm/lib/Transforms/Scalar/SROA.cpp is
// byte-identical to upstream).

void llvm::FilCAddPipelineStartPasses(ModulePassManager &MPM,
                                      OptimizationLevel Level,
                                      FilJITConfiguration *jitConfig) {
  if (FilCKillUB)
    MPM.addPass(KillUBPass());
  // Fil-C: above O0 clang also runs its "Fil-C optimization" pipeline here
  // (an early SimplifyCFG/SROA/EarlyCSE, then -- under FilCInline -- a full
  // inliner invocation whose per-CGSCC pipeline is SROA/EarlyCSE/
  // JumpThreading/CVP/SimplifyCFG/InstCombine/GVN/SCCP/ADCE/InstCombine), to
  // create inlining opportunities before pizlonation.  This runs for every
  // frontend: the rustc-shaped miscompiles that used to justify keeping it
  // off for rustc (the IsRust gate) are fixed at their sources -- see the
  // FilCAddPipelineStartPasses comment above (the rustc-side typed allocas
  // for the spawn+join shape; std's pointer-typed swap chunk buffers for the
  // mem::swap class).
  if (Level != OptimizationLevel::O0 && FilCOptimize) {
    FunctionPassManager EarlyFPM;
    EarlyFPM.addPass(LowerExpectIntrinsicPass());
    EarlyFPM.addPass(SimplifyCFGPass());
    EarlyFPM.addPass(SROAPass(SROAOptions::ModifyCFG));
    EarlyFPM.addPass(EarlyCSEPass());
    if (FilCDSE)
      EarlyFPM.addPass(DSEPass());
    MPM.addPass(createModuleToFunctionPassAdaptor(
                  std::move(EarlyFPM), /*EagerlyInvalidate =*/ true));

    if (FilCInline) {
      InlineParams IP = getInlineParams(Level.getSpeedupLevel(), Level.getSizeLevel());
      ModuleInlinerWrapperPass MIWP(IP, /*MandatoryFirst=*/true,
                                    InlineContext{
                                      ThinOrFullLTOPhase::None,
                                      InlinePass::CGSCCInliner},
                                    InliningAdvisorMode::Default,
                                    /*MaxDevirtIterations=*/4);
      MIWP.addModulePass(RequireAnalysisPass<GlobalsAA, llvm::Module>());
      MIWP.addModulePass(
        createModuleToFunctionPassAdaptor(InvalidateAnalysisPass<AAManager>()));
      MIWP.addModulePass(RequireAnalysisPass<ProfileSummaryAnalysis, llvm::Module>());
      CGSCCPassManager &MainCGPipeline = MIWP.getPM();
      MainCGPipeline.addPass(PostOrderFunctionAttrsPass(/*SkipNonRecursive*/ true));
      FunctionPassManager InlinerFPM;
      if (FilCInlineSROA)
        InlinerFPM.addPass(SROAPass(SROAOptions::ModifyCFG));
      if (FilCInlineEarlyCSE)
        InlinerFPM.addPass(EarlyCSEPass(true /* Enable mem-ssa. */));
      if (FilCInlineJumpThreading)
        InlinerFPM.addPass(JumpThreadingPass());
      if (FilCInlineCVP)
        InlinerFPM.addPass(CorrelatedValuePropagationPass());
      if (FilCInlineSimplifyCFG) {
        InlinerFPM.addPass(
          SimplifyCFGPass(SimplifyCFGOptions().convertSwitchRangeToICmp(true)));
      }
      if (FilCInlineInstCombineEarly)
        InlinerFPM.addPass(InstCombinePass());
      if (FilCInlineGVN)
        InlinerFPM.addPass(GVNPass());
      if (FilCInlineSCCP)
        InlinerFPM.addPass(SCCPPass());
      if (FilCInlineADCE)
        InlinerFPM.addPass(ADCEPass());
      if (FilCInlineDSE && FilCDSE)
        InlinerFPM.addPass(DSEPass());
      if (FilCInlineInstCombineLate)
        InlinerFPM.addPass(InstCombinePass());
      MainCGPipeline.addPass(
        createCGSCCToFunctionPassAdaptor(
          std::move(InlinerFPM),
          /*EagerlyInvalidateAnalyses=*/true, /*NoRerun=*/true));
      MainCGPipeline.addPass(PostOrderFunctionAttrsPass());
      MainCGPipeline.addPass(
        createCGSCCToFunctionPassAdaptor(
          RequireAnalysisPass<ShouldNotRunFunctionPassesAnalysis, Function>()));
      MIWP.addLateModulePass(
        createModuleToFunctionPassAdaptor(
          InvalidateAnalysisPass<ShouldNotRunFunctionPassesAnalysis>()));
      MPM.addPass(std::move(MIWP));
    }
  }
  // Lower C++20 coroutines before pizlonating. CoroSplit turns each
  // coroutine into ordinary ramp/resume/destroy functions whose frame
  // is a normal heap allocation, so FilPizlonator never sees the
  // llvm.coro.* intrinsics. The coroutine passes in the regular
  // pipeline later become no-ops.
  ModulePassManager CoroPM;
  CoroPM.addPass(CoroEarlyPass());
  CGSCCPassManager CoroCGPM;
  CoroCGPM.addPass(CoroSplitPass(Level != OptimizationLevel::O0));
  CoroPM.addPass(
    createModuleToPostOrderCGSCCPassAdaptor(std::move(CoroCGPM)));
  CoroPM.addPass(CoroCleanupPass());
  CoroPM.addPass(GlobalDCEPass());
  MPM.addPass(CoroConditionalWrapper(std::move(CoroPM)));

  MPM.addPass(FilPizlonatorPass(jitConfig));
}

void llvm::RegisterFilPasses(PassBuilder& PB, FilJITConfiguration* jitConfig) {
  PB.registerPipelineStartEPCallback(
    [jitConfig](ModulePassManager &MPM, OptimizationLevel Level) {
      FilCAddPipelineStartPasses(MPM, Level, jitConfig);
    });

  PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM,
                                        OptimizationLevel Level,
                                        ThinOrFullLTOPhase) {
    MPM.addPass(DeleteRedundantPollchecksPass());
  });
}

