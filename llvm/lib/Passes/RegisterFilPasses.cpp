#include "llvm/Passes/RegisterFilPasses.h"

#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Transforms/Coroutines/CoroCleanup.h"
#include "llvm/Transforms/Coroutines/CoroConditionalWrapper.h"
#include "llvm/Transforms/Coroutines/CoroEarly.h"
#include "llvm/Transforms/Coroutines/CoroSplit.h"
#include "llvm/Transforms/IPO/GlobalDCE.h"
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

void RegisterFilPasses(PassBuilder& PB, FilJITConfiguration* jitConfig) {
  PB.registerPipelineStartEPCallback(
    [](ModulePassManager &MPM, OptimizationLevel Level) {
      if (FilCKillUB)
        MPM.addPass(KillUBPass());
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
    });

  PB.registerOptimizerLastEPCallback([](ModulePassManager &MPM,
                                        OptimizationLevel Level,
                                        ThinOrFullLTOPhase) {
    MPM.addPass(DeleteRedundantPollchecksPass());
  });
}

