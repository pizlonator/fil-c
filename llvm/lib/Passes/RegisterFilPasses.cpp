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

#include <cstdlib>

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
// IsRust (i.e. the Fil-C rustc) disables the pre-pizlonation simplification
// and inlining pipeline by default because it still miscompiles rustc-shaped
// IR in the mem::swap class: std's swap_nonoverlapping_chunks
// (library/core/src/ptr/mod.rs) moves the bytes of pointer-containing
// elements through MaybeUninit<[u8; N]> block buffers (N =
// size_of::<*const ()>()), which are NUMERIC-typed allocas, and upstream
// SROA promotes those allocas, widening their typed byte copies into integer
// (i64) load/store pairs.  Under GIMSO semantics that is NOT
// semantics-preserving: the Fil-Pizlonator moves payload+shadow for
// aggregate/byte copies (a pointer at rest carries its capability in the
// shadow of the copied bytes), while integer loads/stores touch only the
// payload, so the capabilities of the pointers inside the swapped element
// are dropped at the swap.  The reviewer-shaped repro is a Vec of 40-byte
// Node structs ({String, *mut u8, u32}) shuffled with std::mem::swap: at -O
// the subsequent Vec::clone -> String::clone memmove then reads a source
// pointer whose capability the swap destroyed ("filc safety error: cannot
// read source pointer with ptr < lower", exit 133; -C opt-level=0 and
// -C llvm-args=-filc-optimize=0 both run correctly).  Even a bare
// mem::swap::<String> miscompiles the same way, so this is the mem::swap
// class, not one element shape.
//
// The rustc-side typed-allocas fix (see the comment inside the function
// below, and filc_ptr_struct_type in rustc_codegen_llvm/src/type_of.rs)
// cannot cover this class: filc_may_contain_pointers correctly declines
// NUMERIC types -- MaybeUninit<[u8; N]> is one -- so those block-buffer
// allocas stay untyped byte arrays and SROA still integer-widens their
// partitions.  The SROA-side guard that closes the class (in the rewriter:
// never widen a memintrinsic use into a non-pointer-typed access) was
// rejected for performance: it pointer-types the memcpy-only partitions of
// NUMERIC aggregates too, and pointer-typed partitions make the pizlonator
// emit pointer checks for data that is just bytes -- that cost ~1.4x on
// simdutf.  That fix lives on the fil/fixed_sroa branch and is NOT in this
// tree (llvm/lib/Transforms/Scalar/SROA.cpp is byte-identical to upstream).
// The rustc-side fix DID close the earlier spawn+join shape (see below),
// which is why rustc above O0 runs KillUB -> coro lowering -> FilPizlonator,
// with the regular optimization pipeline still running after the
// FilPizlonator on the pizlonated IR -- exactly as it does for every Fil-C
// clang compilation -- so codegen is still fully optimized without the
// pre-pizlonation pipeline.  FILCRUST_FILCINLINE in the environment
// re-enables the pre-pizlonation block under rustc (any set value), for
// debugging the pizlonator interaction.

void llvm::FilCAddPipelineStartPasses(ModulePassManager &MPM,
                                      OptimizationLevel Level,
                                      bool IsRust,
                                      FilJITConfiguration *jitConfig) {
  if (FilCKillUB)
    MPM.addPass(KillUBPass());
  // Fil-C: above O0 clang also runs its "Fil-C optimization" pipeline here
  // (an early SimplifyCFG/SROA/EarlyCSE, then -- under FilCInline -- a full
  // inliner invocation whose per-CGSCC pipeline is SROA/EarlyCSE/
  // JumpThreading/CVP/SimplifyCFG/InstCombine/GVN/SCCP/ADCE/InstCombine), to
  // create inlining opportunities before pizlonation.  IsRust (i.e. the
  // Fil-C rustc) disables it by default because it still miscompiles
  // rustc-shaped IR in the mem::swap class -- see the FilCAddPipelineStartPasses
  // comment above: std moves swapped element bytes through NUMERIC-typed
  // MaybeUninit<[u8; N]> block buffers, whose partitions upstream SROA
  // integer-widens, dropping GIMSO capabilities; the rustc-side typed-allocas
  // fix (below) correctly declines numeric types and so cannot cover that
  // class.  FILCRUST_FILCINLINE in the environment turns it back on (useful
  // for debugging the pizlonator interaction).
  if (Level != OptimizationLevel::O0 && FilCOptimize &&
      (!IsRust || getenv("FILCRUST_FILCINLINE"))) {
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

void llvm::RegisterFilPasses(PassBuilder& PB, FilJITConfiguration* jitConfig,
                             bool IsRust) {
  PB.registerPipelineStartEPCallback(
    [jitConfig, IsRust](ModulePassManager &MPM, OptimizationLevel Level) {
      FilCAddPipelineStartPasses(MPM, Level, IsRust, jitConfig);
    });

  PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM,
                                        OptimizationLevel Level,
                                        ThinOrFullLTOPhase) {
    MPM.addPass(DeleteRedundantPollchecksPass());
  });
}

