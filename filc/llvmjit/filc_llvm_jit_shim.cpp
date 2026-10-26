// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// libfilcllvmjit.so implementation.
//
// This translation unit is the only code (besides LLVM itself) linked into
// libfilcllvmjit.so.  It exposes exactly one dynamic symbol,
// FilLLVMJIT_GetApi (enforced by filc_llvm_jit_shim.version plus
// -fvisibility=hidden and -Wl,--exclude-libs,ALL), which returns a pointer
// to a filc_llvm_jit_api function-pointer table.  libpizlo dlopens this library
// lazily on first use of the Fil-C LLVM JIT API and forwards calls through
// that table, so libpizlo itself never links LLVM.
//
// Contents:
// - lazy process-wide LLVM native-target init on first Create (one shot,
//   no heap mapping owned here).
// - an ORC LLJIT session per ShimJit, plus one JITDylib per compiled
//   module.  Each module's dylib defines the absolute symbol
//   `filc_jit_module` naming that module's jit module object, and links
//   older sibling dylibs so modules compiled earlier in the same session
//   stay visible.  Sessions and modules are refcounted through live-handle
//   registries: every entry point promotes its raw handle to a shared_ptr
//   for the call, Destroy drops the registry entry, and the record dies
//   with the last in-flight holder, so concurrent Destroy cannot free a
//   session or module out from under a compile or lookup.  Lazy resolution
//   is routed per module: FilPizlonator in
//   JIT mode passes the `filc_jit_module` address into every
//   filc_llvm_jit_resolve / filc_llvm_jit_resolve_raw_pointer call, and at
//   CompileModule time every such call is rewritten to a module-private
//   wrapper carrying that compile's runtime resolve callbacks, so JITed
//   code can only ever resolve through its own module's resolver snapshot
//   and zexact table, never another module's or session's.  Resolution
//   state lives in the jit module object (resolver callback plus context,
//   zexact table, recorded roots); sessions hold no resolve state and may
//   be destroyed once compilation finishes, while compiled modules keep
//   the shared LLJIT alive as long as they are reachable.
// - a custom JITLinkMemoryManager (FilCJITLinkMemoryManager) that routes
//   every LinkGraph section through the runtime's memory callbacks installed
//   with SetMemoryCallbacks.  LLVM never allocates executable memory on its
//   own: GC sections (names starting with FILC_LLVM_JIT_GC_SECTION_PREFIX)
//   go through allocate_object, executable sections (LinkGraph MemProt Exec)
//   plus the JITLink-synthesized support sections (names starting with "$__",
//   currently "$__GOT") through allocate_executable on the JIT heap, aux
//   sections through allocate_module_data into the combined-aux part, and the
//   rest through allocate_module_data.  The support sections must live in the
//   JIT heap next to the code and stubs even though they are not executable:
//   code and stubs reach GOT entries through 32-bit PC-relative edges
//   (BranchPCRel32, PCRel32GOTLoad, ...) that only reach 2GB, while the
//   module tail lives in malloc'd memory that can sit arbitrarily far
//   (terabytes) from the JIT heap.  The large code model is forced
//   unconditionally.
//   Link-time layout happens against malloc'd working copies; at finalize the
//   finalize actions run first while the working copies are still alive, and
//   only on their success are the working bytes published to their
//   callback-provided targets (zero-fill plus content patches), so on an
//   actions failure the runtime-owned targets are never touched and stay
//   unmutated.  A release fence then orders the published bytes before GC
//   payloads are handed to finalize_object (sets the alignment bits and
//   points the alignment header's extra pointer at the jit_module, which is
//   the publication point the fence pairs with) and executable ranges get
//   arm64 icache maintenance (exec sections only), and finally Blocks are
//   repointed at their finalized targets before the working copies are freed,
//   so no Block ever dangles.  On an actions failure the objects are never
//   published (no copy, no finalize_object) and the working copies are simply
//   dropped; the untouched targets stay owned by the runtime's jit module
//   object for its destructor to reclaim.  Abandon drops the working copies without
//   repointing Blocks (abandon means discard; Blocks must not be left
//   aliasing runtime-owned dying module memory), and the module pointer
//   itself is the finalized token (deallocate is a no-op success).
// - Fil-C pass pipeline mirroring clang/lib/CodeGen/BackendUtil.cpp:
//   KillUB, early FPM, ModuleInlinerWrapperPass pipeline, Coro pipeline,
//   FilPizlonator, DeleteRedundantPollchecks.
// - AddGlobalMapping-style hook: extern declarations in the compiled module
//   whose names have mapped addresses are bound to their host addresses.
//   The blessed names filc_llvm_jit_resolve and
//   filc_llvm_jit_resolve_raw_pointer use the raw ABI and are passed through
//   FilPizlonator untouched, so their declarations are rebound to
//   module-private wrappers (or to inttoptr constants when overridden).
//   Every other mapped name denotes a low-level C runtime helper (fmod and
//   friends): its declaration is rebound to an inttoptr constant directly.
//   Constant uses of a mapped data global fail the compile instead of being
//   left silently unbound, use-free blessed declarations are
//   erased so they cannot dangle into ORC, and failed compiles drop the
//   ByDylib snapshot they inserted and remove the dylib they created.  The
//   blessed names above resolve through the per-compile runtime callbacks
//   installed with SetMemoryCallbacks.
// - address lookups search the module's own dylib first, then older sibling
//   dylibs, exact name first with a pizlonated_ retry, matching the link
//   order the modules were linked with.
//
// NOTE on libc: this .so is dlopened from inside Fil-C processes, so it is
// linked with -nodefaultlibs plus static libstdc++/libgcc and carries no
// shared NEEDEDs; its remaining undefined libc references bind to the
// hosting process's globals. The one libc function the static libstdc++
// needs that Fil-C processes do not provide is atexit (used by
// std::condition_variable); it is defined below in terms of the
// __cxa_atexit the host process does provide. The definition stays local
// (hidden visibility plus version script export only FilLLVMJIT_GetApi), so it
// satisfies just this library's internal references.

extern "C" int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
extern "C" int atexit(void (*func)(void)) {
    return __cxa_atexit(
        reinterpret_cast<void (*)(void *)>(func), nullptr, nullptr);
}

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/mman.h>

#include <memory>

#include <set>

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/InlineOrder.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/JITLinkMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/AllocationActions.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
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
#include "llvm-c/Core.h"

#include "filc_llvm_jit_api.h"

// Compile options.  Defaults mirror BackendUtil.cpp Fil-C flags.
// KillUB and the FilPizlonator pass are intentionally NOT tunable: they
// always run (see runFilCPipeline).  There is no JITOptions field for them
// and SetOption rejects every spelling that would disable them.  The code
// model is likewise not tunable: the JIT always targets the large code
// model so that GC objects, module tails, and JIT-heap code can live
// arbitrarily far apart.
struct JITOptions {
    int OptLevel = 2;
    bool Verify = true;
    bool Optimize = true;
    bool Inline = true;
    bool InlineSROA = true;
    bool InlineEarlyCSE = true;
    bool InlineJumpThreading = true;
    bool InlineCVP = true;
    bool InlineSimplifyCFG = true;
    bool InlineInstCombineEarly = true;
    bool InlineInstCombineLate = true;
    bool InlineGVN = true;
    bool InlineSCCP = true;
    bool InlineADCE = true;
    bool InlineDSE = true;
    bool DSE = false;
};

// Refcounted per-LLJIT table of the runtime memory callbacks for each
// module dylib.  Linking (and hence the allocate path) runs lazily on first
// lookup, possibly long after CompileModule returned and after the session
// record was destroyed, so the memory manager cannot read session fields.
// Instead CompileModule snapshots that compile's callbacks into this table
// under the session lock, and the memory manager looks them up by dylib.
// The table is shared by the session, the memory manager, and (through the
// LLJIT) every compiled module record, so entries stay valid until the
// LLJIT itself dies.  Entries are removed on DestroyModule and on the
// CompileModule failure path after insertion, so a dead dylib never keeps
// a callback snapshot alive.  The opaque context pointer inside each entry
// is runtime-owned (the compile_ctx malloc'd by filc_llvm_jit.c and passed
// as cbs.context): the shim never allocates, owns, or frees it, it only
// holds the snapshot while the dylib is live; the runtime frees its
// compile_ctx when the jit module object dies.
struct ShimCallbackTable {
    std::mutex Mutex;
    std::map<std::string, struct filc_llvm_jit_memory_callbacks> ByDylib;
};

// Shim-owned session and compiled-module records.  These back the opaque
// user handles FilLLVMJITRef/FilLLVMJITModuleRef, but they are spelled
// here with shim-private names (ShimJit/ShimJitModule, declared in the
// internal header) so the manually managed records are never confused with
// the user-facing opaque refs.  Sessions are refcounted through the live
// registries below: every entry point holds a shared_ptr for the duration
// of the call, DestroySession drops the registry reference, and the record
// dies when the last in-flight CompileModule or option call releases it.
// Compiled modules keep the shared LLJIT (and their own dylibs, which are
// never removed) alive as long as they are reachable, so forgetting the
// session after CompileModule returns is always safe.
//
// All mutable setup (options, mappings, memory callbacks) is a plain
// per-session field guarded by the session's own mutex.  There is no
// global map and no global lock.  Linking runs lazily on first lookup,
// possibly after the session is gone, so at CompileModule time the memory
// callbacks are snapshotted into the shared per-dylib table the memory
// manager reads; linking never touches session state.
struct ShimJitSession {
    std::shared_ptr<llvm::orc::LLJIT> JIT;
    JITOptions Opts;
    std::map<std::string, void *> Mappings;
    std::vector<llvm::orc::JITDylib *> ModuleDylibs;
    uint64_t NextModuleId = 1;
    struct filc_llvm_jit_memory_callbacks MemCallbacks = {};
    bool MemCallbacksSet = false;
    std::shared_ptr<ShimCallbackTable> CallbackTable;
    std::mutex Mutex;
};

struct ShimJitModuleRec {
    std::shared_ptr<llvm::orc::LLJIT> JIT;
    llvm::orc::JITDylib *Dylib;
    std::vector<llvm::orc::JITDylib *> SearchOrder;
    uint64_t Id;
    std::string Name;
    std::string DylibName;
    std::shared_ptr<ShimCallbackTable> CallbackTable;
};

namespace {

using namespace llvm;
using namespace llvm::orc;

thread_local std::string g_lastError;

void setError(const std::string &s) { g_lastError = s; }

void setError(Error &&E) {
    std::string S;
    raw_string_ostream OS(S);
    logAllUnhandledErrors(std::move(E), OS);
    g_lastError = std::move(S);
}

// Stringify a consumed LLVM Error without aborting: every lookup/link
// failure (including OOM inside allocate/finalize) threads back to the
// caller as a NULL return plus this text via GetLastError, never a panic.
// The runtime's CompileModule wrapper eager-links right after CompileModule
// and reports GetLastError on failure, so preserving the underlying cause
// here is what lets an OOM surface as a clean compile failure.
std::string errorToString(Error E) {
    std::string S;
    raw_string_ostream OS(S);
    logAllUnhandledErrors(std::move(E), OS);
    return S;
}

std::once_flag g_llvmInitFlag;

int ensureLLVMInitialized() {
    std::call_once(g_llvmInitFlag, [] {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
    });
    return 0;
}

// Live-handle registries backing the session lifetime race fix.  Every
// Create inserts a shared_ptr under the registry lock and returns the raw
// pointer as the user handle; every entry point re-locks the registry to
// promote the raw handle back to a shared_ptr that stays alive for the
// whole call.  DestroySession/DestroyModule erase the registry entry (and,
// for modules, the ByDylib callback snapshot), so the record dies only
// when the last in-flight call releases it.  A stale handle used after
// destroy misses in the registry and fails with g_lastError instead of a
// use-after-free.  Lookups take shared_ptr module records, so a concurrent
// DestroyModule cannot free the record out from under GetFunctionAddress.
std::mutex &sessionRegistryMutex() {
    static std::mutex M;
    return M;
}

std::unordered_map<ShimJit, std::shared_ptr<ShimJitSession>> &
sessionRegistry() {
    static std::unordered_map<ShimJit, std::shared_ptr<ShimJitSession>> R;
    return R;
}

std::mutex &moduleRegistryMutex() {
    static std::mutex M;
    return M;
}

std::unordered_map<ShimJitModule, std::shared_ptr<ShimJitModuleRec>> &
moduleRegistry() {
    static std::unordered_map<ShimJitModule, std::shared_ptr<ShimJitModuleRec>> R;
    return R;
}

std::shared_ptr<ShimJitSession> lockSession(ShimJit jit) {
    if (!jit)
        return nullptr;
    std::lock_guard<std::mutex> lock(sessionRegistryMutex());
    auto It = sessionRegistry().find(jit);
    if (It == sessionRegistry().end())
        return nullptr;
    return It->second;
}

std::shared_ptr<ShimJitModuleRec> lockModule(ShimJitModule mod) {
    if (!mod)
        return nullptr;
    std::lock_guard<std::mutex> lock(moduleRegistryMutex());
    auto It = moduleRegistry().find(mod);
    if (It == moduleRegistry().end())
        return nullptr;
    return It->second;
}

void eraseByDylib(const std::shared_ptr<ShimCallbackTable> &tables,
                  const std::string &dylibName) {
    if (!tables || dylibName.empty())
        return;
    std::lock_guard<std::mutex> lock(tables->Mutex);
    tables->ByDylib.erase(dylibName);
}

OptimizationLevel optLevelFor(int level) {
    switch (level) {
    case 0:
        return OptimizationLevel::O0;
    case 1:
        return OptimizationLevel::O1;
    case 3:
        return OptimizationLevel::O3;
    case 2:
    default:
        return OptimizationLevel::O2;
    }
}
// Run the Fil-C IR pass pipeline over M.  Mirrors the pipeline registered
// in clang/lib/CodeGen/BackendUtil.cpp for Fil-C compilations.
void runFilCPipeline(Module &M, const JITOptions &Opts, TargetMachine &TM) {
    PassBuilder PB(&TM);
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    auto TLII = std::make_unique<TargetLibraryInfoImpl>(
        Triple(TM.getTargetTriple()));
    FAM.registerPass([&] { return TargetLibraryAnalysis(*TLII); });

    ModulePassManager MPM;
    if (Opts.Verify)
        MPM.addPass(VerifierPass());
    // KillUB is mandatory and non-optional: it always runs, regardless of
    // user options.  UB in JITed IR must be killed before FilPizlonator
    // reasons about it, exactly like the clang Fil-C pipeline.
    MPM.addPass(KillUBPass());
    OptimizationLevel Level = optLevelFor(Opts.OptLevel);
    if (Level != OptimizationLevel::O0 && Opts.Optimize) {
        FunctionPassManager EarlyFPM;
        EarlyFPM.addPass(LowerExpectIntrinsicPass());
        EarlyFPM.addPass(SimplifyCFGPass());
        EarlyFPM.addPass(SROAPass(SROAOptions::ModifyCFG));
        EarlyFPM.addPass(EarlyCSEPass());
        if (Opts.DSE)
            EarlyFPM.addPass(DSEPass());
        MPM.addPass(createModuleToFunctionPassAdaptor(std::move(EarlyFPM),
                                                      true));
        if (Opts.Inline) {
            InlineParams IP = getInlineParams(Level.getSpeedupLevel(),
                                              Level.getSizeLevel());
            ModuleInlinerWrapperPass MIWP(
                IP, true,
                InlineContext{ThinOrFullLTOPhase::None,
                              InlinePass::CGSCCInliner},
                InliningAdvisorMode::Default,
                4);
            MIWP.addModulePass(RequireAnalysisPass<GlobalsAA, Module>());
            MIWP.addModulePass(createModuleToFunctionPassAdaptor(
                InvalidateAnalysisPass<AAManager>()));
            MIWP.addModulePass(
                RequireAnalysisPass<ProfileSummaryAnalysis, Module>());
            CGSCCPassManager &MainCGPipeline = MIWP.getPM();
            MainCGPipeline.addPass(
                PostOrderFunctionAttrsPass(true));
            FunctionPassManager InlinerFPM;
            if (Opts.InlineSROA)
                InlinerFPM.addPass(SROAPass(SROAOptions::ModifyCFG));
            if (Opts.InlineEarlyCSE)
                InlinerFPM.addPass(EarlyCSEPass(true));
            if (Opts.InlineJumpThreading)
                InlinerFPM.addPass(JumpThreadingPass());
            if (Opts.InlineCVP)
                InlinerFPM.addPass(CorrelatedValuePropagationPass());
            if (Opts.InlineSimplifyCFG) {
                InlinerFPM.addPass(SimplifyCFGPass(
                    SimplifyCFGOptions().convertSwitchRangeToICmp(true)));
            }
            if (Opts.InlineInstCombineEarly)
                InlinerFPM.addPass(InstCombinePass());
            if (Opts.InlineGVN)
                InlinerFPM.addPass(GVNPass());
            if (Opts.InlineSCCP)
                InlinerFPM.addPass(SCCPPass());
            if (Opts.InlineADCE)
                InlinerFPM.addPass(ADCEPass());
            if (Opts.InlineDSE && Opts.DSE)
                InlinerFPM.addPass(DSEPass());
            if (Opts.InlineInstCombineLate)
                InlinerFPM.addPass(InstCombinePass());
            MainCGPipeline.addPass(createCGSCCToFunctionPassAdaptor(
                std::move(InlinerFPM), true, true));
            MainCGPipeline.addPass(PostOrderFunctionAttrsPass());
            MainCGPipeline.addPass(createCGSCCToFunctionPassAdaptor(
                RequireAnalysisPass<ShouldNotRunFunctionPassesAnalysis,
                                    Function>()));
            MIWP.addLateModulePass(createModuleToFunctionPassAdaptor(
                InvalidateAnalysisPass<ShouldNotRunFunctionPassesAnalysis>()));
            MPM.addPass(std::move(MIWP));
        }
    }
    // Lower C++20 coroutines before pizlonating, exactly like the clang
    // pipeline does: CoroSplit turns each coroutine into ordinary
    // ramp/resume/destroy functions whose frame is a normal heap
    // allocation, so FilPizlonator never sees the llvm.coro intrinsics.
    {
        ModulePassManager CoroPM;
        CoroPM.addPass(CoroEarlyPass());
        CGSCCPassManager CoroCGPM;
        CoroCGPM.addPass(CoroSplitPass(Level != OptimizationLevel::O0));
        CoroPM.addPass(
            createModuleToPostOrderCGSCCPassAdaptor(std::move(CoroCGPM)));
        CoroPM.addPass(CoroCleanupPass());
        CoroPM.addPass(GlobalDCEPass());
        MPM.addPass(CoroConditionalWrapper(std::move(CoroPM)));
    }
    // JIT-compiled modules are linked at run time, so externs must resolve
    // lazily: run FilPizlonator in JIT mode so it authors per-extern lazy
    // getters (via filc_llvm_jit_resolve and
    // filc_llvm_jit_resolve_raw_pointer).
    MPM.addPass(FilPizlonatorPass(true));
    MPM.addPass(DeleteRedundantPollchecksPass());
    MPM.run(M, MAM);
}

// In-flight allocation backing one LinkGraph.  Working copies of every
// section live in malloc'd memory owned here; targets come from the
// runtime callbacks and are written at finalize, once the linked bits are
// final.  finalize actions run first while the working copies are still
// alive; only after they succeed are the working bytes published to the
// runtime-owned targets, so a failed link never mutates already-visible
// targets (no copy, no finalize_object runs, and the untouched targets stay
// owned by the runtime's jit module object for its destructor to reclaim).
// GC section payloads are then handed to finalize_object so the runtime can
// set the alignment bits and point the alignment header's extra pointer at
// the jit_module; a release fence between the publish and that handoff
// orders the published bytes before the extra-pointer publication.
class FilCInFlightAlloc : public jitlink::JITLinkMemoryManager::InFlightAlloc {
public:
    struct ContentPatch {
        jitlink::Block *Block;
        size_t Offset;
    };
    struct SectionRecord {
        std::string Name;
        bool IsGC = false;
        bool IsExec = false;
        void *Target = nullptr;
        char *Working = nullptr;
        size_t Size = 0;
        std::vector<ContentPatch> Patches;
    };

    FilCInFlightAlloc(struct filc_llvm_jit_memory_callbacks cbs,
                      void *jitModule, jitlink::LinkGraph &graph)
        : Cbs(cbs), JitModule(jitModule), Graph(graph) {}

    std::vector<SectionRecord> Records;

    void abandon(OnAbandonedFunction OnAbandoned) override {
        // Abandon means discard: free the malloc'd working copies only.
        // Blocks are left alone (they still name the working copies, whose
        // graph is dead and will be destroyed without further reads) and
        // are specifically NOT repointed at Targets: Targets are
        // runtime-owned dying module memory, and repointing Blocks there
        // would leave them aliasing memory the runtime reclaims.
        for (auto &r : Records) {
            free(r.Working);
            r.Working = nullptr;
        }
        Records.clear();
        OnAbandoned(Error::success());
    }

    void finalize(OnFinalizedFunction OnFinalized) override {
        // Run finalize actions FIRST, while the working copies are still
        // alive, so any reads through Block content remain valid.  Nothing
        // has been published yet: the runtime-owned targets are untouched,
        // so on failure below there is nothing to restore or free on the
        // shim side -- just drop the working copies and report the error.
        // The untouched targets stay owned by the runtime's jit module
        // object (tail, JIT-heap bases, GC object refs) for its destructor
        // to reclaim.
        if (auto actions = shared::runFinalizeActions(Graph.allocActions());
            !actions) {
            for (auto &r : Records) {
                free(r.Working);
                r.Working = nullptr;
            }
            Records.clear();
            OnFinalized(actions.takeError());
            return;
        }
        // Actions succeeded: publish the final working bytes to their
        // callback-provided targets (zero-fill so padding is deterministic;
        // content patches carry the linked bytes, including any writes the
        // finalize actions made to the working copies).
        for (auto &r : Records) {
            if (r.Size && r.Target) {
                memset(r.Target, 0, r.Size);
                for (auto &p : r.Patches) {
                    size_t blockSize = p.Block->getSize();
                    if (blockSize) {
                        memcpy(static_cast<char *>(r.Target) + p.Offset,
                               p.Block->getContent().data(), blockSize);
                    }
                }
            }
        }
        // Release fence: order the published bytes before the publication.
        // finalize_object below stores the jit_module into each GC object's
        // alignment header extra pointer (in the runtime, which owns the
        // header shape); any thread that acquires that pointer must see the
        // bytes published here.  The runtime's finalize_object performs the
        // publishing store, and this fence pairs with it.
        std::atomic_thread_fence(std::memory_order_release);
        // Publish GC payloads (sets the alignment bits and points the
        // alignment header's extra pointer at the jit_module), then icache
        // maintenance for executable ranges.
        for (auto &r : Records) {
            if (r.IsGC && Cbs.finalize_object) {
                Cbs.finalize_object(Cbs.context, JitModule, r.Target,
                                    static_cast<unsigned long long>(r.Size));
            }
#if defined(__aarch64__)
            // Executable sections only: maintain the icache for the
            // freshly linked bytes (dc to PoU, dsb, ic invalidate, dsb).
            // Data sections need no maintenance; x86 is ordered by the
            // cross-modifying-code fence the runtime runs on return.
            if (r.IsExec && r.Size && r.Target) {
                __builtin___clear_cache(static_cast<char *>(r.Target),
                                        static_cast<char *>(r.Target) +
                                            r.Size);
            }
#endif
        }
        // The working bytes above are the final linked bytes.  Repoint each
        // Block at its finalized target bytes before freeing Working so no
        // Block dangles, then free the working copies.
        for (auto &r : Records) {
            for (auto &p : r.Patches) {
                if (r.Target && p.Block->getSize()) {
                    p.Block->setContent({static_cast<char *>(r.Target) +
                                             p.Offset,
                                         p.Block->getSize()});
                }
            }
            free(r.Working);
            r.Working = nullptr;
        }
        Records.clear();
        OnFinalized(jitlink::JITLinkMemoryManager::FinalizedAlloc(
            ExecutorAddr::fromPtr(JitModule)));
    }

private:
    struct filc_llvm_jit_memory_callbacks Cbs;
    void *JitModule;
    jitlink::LinkGraph &Graph;
};

// JITLink-synthesized support sections carry the "$__" name prefix
// ("$__GOT" holding GOT entries on every target, "$__STUBS" holding jump
// stubs where those are not already executable-marked, and any future
// per-graph linker support).  Code and stubs reach these sections through
// 32-bit PC-relative edges, so they must be allocated within 2GB of the
// code: they go to the JIT heap even when they are not executable.  (The
// heap mapping is already RWX, so placing read-only GOT entries there grants
// nothing the executable sections do not already have.)
bool isJITLinkSupportSection(const std::string &Name) {
    return Name.starts_with("$__");
}

// JITLinkMemoryManager that routes every section through the runtime memory
// callbacks.  GC sections start with FILC_LLVM_JIT_GC_SECTION_PREFIX and
// aux sections with FILC_LLVM_JIT_AUX_SECTION_PREFIX; executability comes
// from the LinkGraph section memory protections (MemProt::Exec), so
// read-only data like .rodata/.eh_frame stays non-executable and lands in
// the module tail while only true code goes to the JIT heap, except that
// the "$__"-prefixed JITLink support sections above join the code in the
// heap so the 32-bit PC-relative stub/GOT edges stay in range.  Everything
// else lands in the module tail.  The shim never interprets
// object, header, or range shapes; all of that lives behind the callbacks.
// Per-dylib callback snapshots come from the shared table (never from the
// session, which may be gone by link time); the table keeps them alive.
class FilCJITLinkMemoryManager : public jitlink::JITLinkMemoryManager {
public:
    explicit FilCJITLinkMemoryManager(
        std::shared_ptr<ShimCallbackTable> tables)
        : Tables(std::move(tables)) {}

    void allocate(const jitlink::JITLinkDylib *JD, jitlink::LinkGraph &G,
                  OnAllocatedFunction OnAllocated) override {
        (void)JD;
        struct filc_llvm_jit_memory_callbacks cbs = {};
        bool have = false;
        {
            std::lock_guard<std::mutex> lock(Tables->Mutex);
            auto It = Tables->ByDylib.find(JD->getName());
            if (It != Tables->ByDylib.end()) {
                cbs = It->second;
                have = true;
            }
        }
        if (!have) {
            // No callbacks for this dylib: the module was added without
            // going through CompileModule, or its snapshot is gone.  LLVM
            // must never allocate for Fil-C modules on its own, so fail.
            OnAllocated(make_error<StringError>(
                "FilLLVMJIT: no runtime memory callbacks for module dylib",
                inconvertibleErrorCode()));
            return;
        }
        auto fail = [&](const std::string &msg,
                        std::unique_ptr<FilCInFlightAlloc> &alloc) {
            if (alloc) {
                alloc->abandon([](Error e) { consumeError(std::move(e)); });
            }
            OnAllocated(make_error<StringError>(msg, inconvertibleErrorCode()));
        };
        struct Plan {
            jitlink::Section *Sec = nullptr;
            std::string Name;
            bool IsGC = false;
            bool IsAux = false;
            bool IsExec = false;
            bool InHeap = false;
            size_t Size = 0;
            uint64_t Align = 16;
            std::vector<jitlink::Block *> Content;
            std::vector<jitlink::Block *> ZeroFill;
        };
        std::vector<Plan> plans;
        unsigned long long auxTotal = 0;
        unsigned long long otherTotal = 0;
        for (auto &sec : G.sections()) {
            if (sec.blocks().empty() ||
                sec.getMemLifetime() == MemLifetime::NoAlloc)
                continue;
            Plan p;
            p.Sec = &sec;
            p.Name = sec.getName().str();
            p.IsGC = sec.getName().starts_with(FILC_LLVM_JIT_GC_SECTION_PREFIX);
            p.IsAux =
                sec.getName().starts_with(FILC_LLVM_JIT_AUX_SECTION_PREFIX);
            p.IsExec =
                (sec.getMemProt() & MemProt::Exec) != MemProt::None;
            // GOT/stub support must sit next to the code in the JIT heap no
            // matter its protections; see isJITLinkSupportSection.
            p.InHeap = p.IsExec || isJITLinkSupportSection(p.Name);
            for (auto *b : sec.blocks()) {
                if (b->isZeroFill())
                    p.ZeroFill.push_back(b);
                else
                    p.Content.push_back(b);
                p.Align = std::max(p.Align, b->getAlignment());
            }
            auto byAddr = [](jitlink::Block *a, jitlink::Block *b) {
                if (a->getAddress() != b->getAddress())
                    return a->getAddress() < b->getAddress();
                return a->getSize() < b->getSize();
            };
            llvm::sort(p.Content, byAddr);
            llvm::sort(p.ZeroFill, byAddr);
            uint64_t off = 0;
            for (auto *b : p.Content) {
                off = alignToBlock(off, *b);
                off += b->getSize();
            }
            for (auto *b : p.ZeroFill) {
                off = alignToBlock(off, *b);
                off += b->getSize();
            }
            p.Size = static_cast<size_t>(alignTo(std::max<uint64_t>(off, 16), Align(16)));
            if (!p.IsGC && !p.InHeap) {
                if (p.IsAux)
                    auxTotal +=
                        static_cast<unsigned long long>(p.Size) + p.Align;
                else
                    otherTotal +=
                        static_cast<unsigned long long>(p.Size) + p.Align;
            }
            plans.push_back(std::move(p));
        }
        void *module =
            cbs.allocate_jit_module(cbs.context, auxTotal, otherTotal);
        if (!module) {
            std::unique_ptr<FilCInFlightAlloc> empty;
            fail("FilLLVMJIT: allocate_jit_module failed", empty);
            return;
        }
        auto alloc =
            std::make_unique<FilCInFlightAlloc>(cbs, module, G);
        for (auto &p : plans) {
            void *target = nullptr;
            if (p.IsGC) {
                target = cbs.allocate_object(
                    cbs.context, module, p.Name.c_str(),
                    static_cast<unsigned long long>(p.Size), p.Align);
            } else if (p.InHeap) {
                target = cbs.allocate_executable(
                    cbs.context, module,
                    static_cast<unsigned long long>(p.Size), p.Align,
                    p.Name.c_str());
            } else {
                target = cbs.allocate_module_data(
                    cbs.context, module, p.Name.c_str(),
                    static_cast<unsigned long long>(p.Size), p.Align);
            }
            if (!target) {
                fail("FilLLVMJIT: section allocation failed for '" + p.Name +
                         "'",
                     alloc);
                return;
            }
            FilCInFlightAlloc::SectionRecord rec;
            rec.Name = p.Name;
            rec.IsGC = p.IsGC;
            rec.IsExec = p.IsExec;
            rec.Target = target;
            rec.Size = p.Size;
            rec.Working = p.Size ? static_cast<char *>(malloc(p.Size)) : nullptr;
            if (p.Size && !rec.Working) {
                fail("FilLLVMJIT: working memory malloc failed", alloc);
                return;
            }
            uint64_t base = reinterpret_cast<uint64_t>(target);
            uint64_t off = 0;
            for (auto *b : p.Content) {
                off = alignToBlock(off, *b);
                b->setAddress(ExecutorAddr(base + off));
                size_t blockSize = b->getSize();
                if (blockSize) {
                    memcpy(rec.Working + off, b->getContent().data(), blockSize);
                    b->setMutableContent(
                        {rec.Working + off, blockSize});
                    rec.Patches.push_back({b, static_cast<size_t>(off)});
                }
                off += b->getSize();
            }
            for (auto *b : p.ZeroFill) {
                off = alignToBlock(off, *b);
                b->setAddress(ExecutorAddr(base + off));
                off += b->getSize();
            }
            alloc->Records.push_back(std::move(rec));
        }
        OnAllocated(std::move(alloc));
    }

    void deallocate(std::vector<FinalizedAlloc> Allocs,
                    OnDeallocatedFunction OnDeallocated) override {
        // Every allocation is owned by its jit_module object, so there is
        // nothing to release.  The tokens still have to be consumed.
        for (auto &a : Allocs)
            (void)a.release();
        OnDeallocated(Error::success());
    }

private:
    std::shared_ptr<ShimCallbackTable> Tables;
};

// The blessed lazy-resolution declarations.  In JitMode FilPizlonator passes
// these through untouched, so both their pre-pipeline declarations and their
// post-pipeline uses keep the raw ABI, now with the calling module first:
//   ptr @filc_llvm_jit_resolve(ptr %jit_module, ptr %name)
//   ptr @filc_llvm_jit_resolve_raw_pointer(ptr %jit_module, i64 %id)
// Each post-pipeline use is rewritten to a module-private wrapper carrying
// that compile's runtime resolve callbacks (see bindModuleResolvers below).
// A mapping that points anywhere else (a custom host override) keeps the
// direct inttoptr rebind.
bool isBlessedExternName(const std::string &Name) {
    return Name == "filc_llvm_jit_resolve" ||
           Name == "filc_llvm_jit_resolve_raw_pointer";
}

// Rebind an extern declaration to a host address by replacing all of its
// uses with an inttoptr constant of the declaration's pointer type.
//
// Only valid for declarations whose uses keep the raw pointer ABI:
// low-level C runtime helpers (fmod and friends), plus raw-ABI globals such
// as zexact_ptrtable.  The blessed lazy-resolution names never reach this
// path with their default handling (those are routed per module after the
// pipeline runs); only a custom override mapping lands here.  Returns false
// with g_lastError set when a mapped data global still has constant uses
// that cannot be rebound; constant users are never left silently unbound.
bool rebindExternToAddress(Module &M, const std::string &Name, void *Addr) {
    LLVMContext &C = M.getContext();
    PointerType *PtrTy = PointerType::get(C, 0);
    Constant *AddrConst = ConstantExpr::getIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Addr))),
        PtrTy);
    if (Function *F = M.getFunction(Name)) {
        if (F->isDeclaration())
            F->replaceAllUsesWith(AddrConst);
        return true;
    }
    if (GlobalVariable *G = M.getNamedGlobal(Name)) {
        if (G->isDeclaration()) {
            std::vector<Use*> todo;
            for (Use &U : G->uses())
                todo.push_back(&U);
            for (Use *U : todo) {
                User *Usr = U->getUser();
                if (isa<Constant>(Usr)) {
                    setError(std::string("FilLLVMJIT_CompileModule: constant use of mapped "
                                         "extern remains unbound: ") +
                             Name);
                    return false;
                }
                U->set(AddrConst);
            }
            if (G->use_empty())
                G->eraseFromParent();
            else {
                setError(std::string("FilLLVMJIT_CompileModule: mapped extern still "
                                     "referenced after rebind: ") +
                         Name);
                return false;
            }
        }
        return true;
    }
    if (GlobalAlias *A = M.getNamedAlias(Name)) {
        // Rebind alongside the aliasee: an alias forwards to its aliasee,
        // so a mapping spelled via the alias name also binds the aliasee
        // declaration.  Alias-to-alias chains are resolved iteratively to
        // the ultimate aliasee (visited set, so cyclic alias graphs
        // terminate instead of recursing forever), and only then is the
        // single recursive rebind issued against a function or global target.
        const GlobalValue *Target = A;
        std::set<const GlobalValue *> Visited;
        Visited.insert(A);
        while (const GlobalAlias *CA = dyn_cast<GlobalAlias>(Target)) {
            const Constant *AE = CA->getAliasee();
            if (!AE)
                break;
            const GlobalValue *Next =
                dyn_cast<GlobalValue>(AE->stripPointerCasts());
            if (!Next || !Visited.insert(Next).second)
                break;
            Target = Next;
        }
        if (Target != A && (isa<Function>(Target) || isa<GlobalVariable>(Target)) &&
            Target->getName() != Name)
            return rebindExternToAddress(M, Target->getName().str(), Addr);
        return true;
    }
    return true;
}

// Fil-C data layouts, mirroring the resetDataLayout calls in
// clang/lib/Basic/Targets/X86.h (x86-64) and AArch64.cpp (AArch64leTargetInfo
// ELF).  The "before" layout has a non-integral address space 0, which
// FilPizlonator requires on input; the "after" layout is what FilPizlonator
// leaves behind (and what codegen uses).
struct FilCLayouts {
    const char *Before;
    const char *After;
};

bool filCLayoutsFor(const Triple &T, FilCLayouts &Out) {
    if (T.getArch() == Triple::x86_64) {
        Out.Before = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:"
                     "64-i128:128-f80:128-n8:16:32:64-S128";
        Out.After = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:"
                    "64-i128:128-f80:128-n8:16:32:64-S128";
        return true;
    }
    if (T.getArch() == Triple::aarch64) {
        Out.Before = "e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                     "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32";
        Out.After = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                    "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32";
        return true;
    }
    return false;
}

// Author a module-private wrapper routing one blessed declaration to that
// compile's runtime resolve callbacks.  All addresses are baked as integers,
// so the wrapper needs no symbol resolution at JIT-link time.  TakesEncoded
// selects the (ptr, i64)->ptr shape of filc_llvm_jit_resolve_raw_pointer;
// otherwise the (ptr, ptr)->ptr shape of filc_llvm_jit_resolve.  The first
// parameter (the calling module's jit_module, taken from its filc_jit_module
// global) is forwarded untouched; the baked context selects the runtime
// callback state installed with SetMemoryCallbacks.
Function *authorModuleBoundWrapper(Module &M, const char *BlessedName,
                                   void *ResolveFn, void *Context,
                                   uint64_t Id, bool TakesEncoded) {
    LLVMContext &C = M.getContext();
    PointerType *PtrTy = PointerType::get(C, 0);
    Type *SecondTy = TakesEncoded ? static_cast<Type *>(Type::getInt64Ty(C))
                                  : static_cast<Type *>(PtrTy);
    FunctionType *WTy = FunctionType::get(PtrTy, {PtrTy, SecondTy}, false);
    std::string WName = std::string("__filc_jit_resolve_bound_") +
                        BlessedName + "_" + std::to_string(Id);
    Function *W =
        Function::Create(WTy, GlobalValue::InternalLinkage, WName, &M);
    W->addFnAttr(Attribute::NoUnwind);
    BasicBlock *BB = BasicBlock::Create(C, "entry", W);
    IRBuilder<> B(BB);
    FunctionType *TTy =
        FunctionType::get(PtrTy, {PtrTy, PtrTy, SecondTy}, false);
    Value *TFn = B.CreateIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                         static_cast<uint64_t>(
                             reinterpret_cast<uintptr_t>(ResolveFn))),
        PointerType::get(TTy, 0));
    Value *Ctx = B.CreateIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                         static_cast<uint64_t>(
                             reinterpret_cast<uintptr_t>(Context))),
        PtrTy);
    Value *R = B.CreateCall(TTy, TFn, {Ctx, W->getArg(0), W->getArg(1)});
    B.CreateRet(R);
    return W;
}

// Bind the blessed lazy-resolution declarations of a freshly compiled
// module to that compile's runtime resolve callbacks.  For each blessed
// name still declared with live uses: when the session's mapping holds a
// custom host override, keep the direct inttoptr rebind; otherwise replace
// every use with a module-private wrapper carrying the resolve callbacks
// from SetMemoryCallbacks and drop the declaration, so the module can only
// ever resolve through its own module's resolver snapshot and zexact table.
// Returns false with g_lastError set when wrapper authoring fails or the
// resolve callbacks are missing.
bool bindModuleResolvers(Module &M, const std::shared_ptr<ShimJitSession> &S,
                           uint64_t Id) {
    struct Blessed {
        const char *Name;
        bool TakesEncoded;
    };
    static const Blessed List[] = {
        {"filc_llvm_jit_resolve", false},
        {"filc_llvm_jit_resolve_raw_pointer", true},
    };
    std::map<std::string, void *> mappings;
    struct filc_llvm_jit_memory_callbacks cbs = {};
    bool haveCallbacks = false;
    {
        std::lock_guard<std::mutex> lock(S->Mutex);
        mappings = S->Mappings;
        cbs = S->MemCallbacks;
        haveCallbacks = S->MemCallbacksSet;
    }
    for (const Blessed &B : List) {
        Function *F = M.getFunction(B.Name);
        if (!F)
            continue;
        if (F->use_empty()) {
            F->eraseFromParent();
            continue;
        }
        auto It = mappings.find(B.Name);
        if (It != mappings.end() && It->second) {
            // Custom override: direct rebind to the mapped address.
            if (!rebindExternToAddress(M, B.Name, It->second))
                return false;
            continue;
        }
        void *resolveFn = B.TakesEncoded
                              ? reinterpret_cast<void *>(cbs.resolve_raw_pointer)
                              : reinterpret_cast<void *>(cbs.resolve_symbol);
        if (!haveCallbacks || !resolveFn) {
            setError(std::string("FilLLVMJIT_CompileModule: no resolve "
                                 "callbacks installed for ") +
                     B.Name);
            return false;
        }
        Function *W = authorModuleBoundWrapper(M, B.Name, resolveFn,
                                               cbs.context, Id,
                                               B.TakesEncoded);
        if (!W || W->getFunctionType() != F->getFunctionType()) {
            setError(std::string("FilLLVMJIT_CompileModule: cannot author "
                                 "module-bound resolver for ") +
                     B.Name);
            return false;
        }
        F->replaceAllUsesWith(W);
        F->eraseFromParent();
    }
    return true;
}

int shim_SetOptionImpl(ShimJitSession *S, const char *key, const char *value) {
    // Locking contract: callers hold S->Mutex (see shim_SetOption);
    // CompileModule copies Opts under the same lock, so reads and writes
    // never race.
    if (!S || !key || !value) {
        setError("FilLLVMJIT_SetOption: null argument");
        return -1;
    }
    auto parseBool = [&](const char *v, bool &field) -> bool {
        if (!strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "yes")) {
            field = true;
            return true;
        }
        if (!strcmp(v, "0") || !strcmp(v, "false") || !strcmp(v, "no")) {
            field = false;
            return true;
        }
        return false;
    };
    JITOptions &O = S->Opts;
    if (!strcmp(key, "opt-level")) {
        char *end = nullptr;
        long level = strtol(value, &end, 10);
        if (end == value || *end != '\0' || level < 0 || level > 3) {
            setError("FilLLVMJIT_SetOption: opt-level must be 0..3");
            return -1;
        }
        O.OptLevel = static_cast<int>(level);
        return 0;
    }
#define FILC_JIT_BOOL_OPTION(name, field)                                       \
    if (!strcmp(key, name)) {                                                   \
        if (!parseBool(value, O.field)) {                                       \
            setError(std::string("FilLLVMJIT_SetOption: boolean expected for ") \
                     + name);                                                   \
            return -1;                                                          \
        }                                                                       \
        return 0;                                                               \
    }
    FILC_JIT_BOOL_OPTION("verify", Verify)
    FILC_JIT_BOOL_OPTION("filc-optimize", Optimize)
    FILC_JIT_BOOL_OPTION("filc-inline", Inline)
    FILC_JIT_BOOL_OPTION("filc-inline-sroa", InlineSROA)
    FILC_JIT_BOOL_OPTION("filc-inline-early-cse", InlineEarlyCSE)
    FILC_JIT_BOOL_OPTION("filc-inline-jump-threading", InlineJumpThreading)
    FILC_JIT_BOOL_OPTION("filc-inline-cvp", InlineCVP)
    FILC_JIT_BOOL_OPTION("filc-inline-simplify-cfg", InlineSimplifyCFG)
    FILC_JIT_BOOL_OPTION("filc-inline-inst-combine-early", InlineInstCombineEarly)
    FILC_JIT_BOOL_OPTION("filc-inline-inst-combine-late", InlineInstCombineLate)
    FILC_JIT_BOOL_OPTION("filc-inline-gvn", InlineGVN)
    FILC_JIT_BOOL_OPTION("filc-inline-sccp", InlineSCCP)
    FILC_JIT_BOOL_OPTION("filc-inline-adce", InlineADCE)
    FILC_JIT_BOOL_OPTION("filc-inline-dse", InlineDSE)
    FILC_JIT_BOOL_OPTION("filc-dse", DSE)
#undef FILC_JIT_BOOL_OPTION
    // KillUB and the FilPizlonator pass are mandatory and cannot be turned
    // off: the pipeline above always runs KillUBPass and
    // FilPizlonatorPass.  Reject every legacy spelling that would disable
    // them, whatever the value (even "1": there is no key that controls
    // them, so accepting any value would mislead).
    if (!strcmp(key, "filc-killub") || !strcmp(key, "killub") ||
        !strcmp(key, "filc-pizlonator") || !strcmp(key, "pizlonator") ||
        !strcmp(key, "enable-pizlonator")) {
        setError(std::string("FilLLVMJIT_SetOption: ") + key +
                 " is not a tunable option; KillUB and FilPizlonator always run");
        return -1;
    }
    setError(std::string("FilLLVMJIT_SetOption: unknown option ") + key);
    return -1;
}

void *shim_LookupImpl(const std::shared_ptr<ShimJitModuleRec> &rec,
                        const char *name) {
    if (!rec || !name) {
        setError("FilLLVMJIT lookup: null argument");
        return nullptr;
    }
    // Search the module's own dylib first, then the older sibling dylibs it
    // was linked against, mirroring the link order.  Sibling dylibs are
    // never removed, so these pointers stay valid.  The shared_ptr caller
    // holds keeps the record (and its shared LLJIT) alive for the call, so
    // a concurrent DestroyModule cannot free it out from under the lookup.
    //
    // Lookup/link failures (a lazy allocate or finalize OOM, a missing
    // symbol, ...) arrive here as LLVM Errors.  The first one is kept: if
    // every candidate misses, it is reported after the "not found" prefix
    // so the real cause (not a generic string) threads back to the caller
    // through GetLastError.  Later errors are consumed; an Error left
    // unconsumed would abort.  On success the sticky error is cleared.
    std::string firstDetail;
    auto noteError = [&](Error E) {
        if (firstDetail.empty())
            firstDetail = errorToString(std::move(E));
        else
            consumeError(std::move(E));
    };
    for (auto *JD : rec->SearchOrder) {
        if (auto Sym = rec->JIT->lookup(*JD, name)) {
            g_lastError.clear();
            return Sym->toPtr<void *>();
        } else {
            noteError(Sym.takeError());
        }
        std::string prefixed = std::string("pizlonated_") + name;
        if (auto Sym = rec->JIT->lookup(*JD, prefixed)) {
            g_lastError.clear();
            return Sym->toPtr<void *>();
        } else {
            noteError(Sym.takeError());
        }
    }
    std::string msg =
        std::string("FilLLVMJIT lookup: symbol not found: ") + name;
    if (!firstDetail.empty()) {
        msg += ": ";
        msg += firstDetail;
    }
    setError(msg);
    return nullptr;
}

} // namespace

// Entry points collected into the filc_llvm_jit_api table below.  These are
// plain functions with C linkage in this TU; only FilLLVMJIT_GetApi is
// exported dynamically (see filc_llvm_jit_shim.version).

ShimJit shim_Create(void) {
    // Lazy auto-init: compilation always targets the running process; there
    // is no triple selection.
    ensureLLVMInitialized();
    auto JTMB = JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        setError(JTMB.takeError());
        return nullptr;
    }
    // The large code model is forced unconditionally: GC objects, module
    // tails, and JIT-heap code can land arbitrarily far apart, and there is
    // no option to turn this off.
    JTMB->setCodeModel(CodeModel::Large);
    // The shim is built with -fno-exceptions, so throwing C++ allocations
    // would terminate instead of failing cleanly.  Explicit heap ownership
    // here uses nothrow new with NULL checks, so OOM becomes a NULL return
    // plus a GetLastError string (a clean compile failure), never an abort.
    ShimJitSession *rawSession = new (std::nothrow) ShimJitSession();
    if (!rawSession) {
        setError("FilLLVMJIT_Create: out of memory");
        return nullptr;
    }
    auto S = std::shared_ptr<ShimJitSession>(rawSession);
    ShimCallbackTable *rawTables = new (std::nothrow) ShimCallbackTable();
    if (!rawTables) {
        setError("FilLLVMJIT_Create: out of memory");
        return nullptr;
    }
    S->CallbackTable = std::shared_ptr<ShimCallbackTable>(rawTables);
    std::shared_ptr<ShimCallbackTable> tables = S->CallbackTable;
    LLJITBuilder Builder;
    Builder.setJITTargetMachineBuilder(std::move(*JTMB));
    // Route every LinkGraph section through the runtime memory callbacks.
    // The manager reads per-dylib snapshots from the shared table, so it
    // never touches the session and stays valid after the session dies.
    Builder.setObjectLinkingLayerCreator(
        [tables](ExecutionSession &ES,
                 const Triple &TT) -> Expected<std::unique_ptr<ObjectLayer>> {
            (void)TT;
            auto MM = std::make_unique<FilCJITLinkMemoryManager>(tables);
            return std::make_unique<ObjectLinkingLayer>(ES, std::move(MM));
        });
    auto J = Builder.create();
    if (!J) {
        setError(J.takeError());
        return nullptr;
    }
    S->JIT = std::move(*J);
    // Pizlonated code calls Fil-C runtime helpers (filc_pollcheck,
    // filc_stack_overflow_failure, ...) that live in the host process
    // (libpizlo).  Let the main JITDylib fall back to current-process
    // symbols for anything the JITed modules do not define.  NOTE: this
    // requires libpizlo's symbols to be globally visible (normal linkage
    // of the Fil-C executable, or RTLD_GLOBAL dlopen).  Per-module dylibs
    // created below inherit the session default link order, which carries
    // the process-symbols dylib, so they resolve runtime helpers the same
    // way.
    {
        auto Gen = DynamicLibrarySearchGenerator::GetForCurrentProcess(
            S->JIT->getDataLayout().getGlobalPrefix());
        if (!Gen) {
            setError(Gen.takeError());
            return nullptr;
        }
        S->JIT->getMainJITDylib().addGenerator(std::move(*Gen));
    }
    ShimJit raw = S.get();
    {
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        sessionRegistry()[raw] = std::move(S);
    }
    return raw;
}

void shim_DestroySession(ShimJit jit) {
    // Refcounted teardown: drop the registry reference so the record dies
    // when the last in-flight CompileModule or option call releases its
    // shared_ptr.  Compiled modules hold their own shared reference to the
    // LLJIT, so code stays callable after the session is gone.  Destroy is
    // safe concurrently with in-flight compiles and lookups: calls that
    // already promoted the handle keep it alive, and stale handles after
    // destroy fail via the registry instead of a use-after-free.
    if (!jit) {
        setError("FilLLVMJIT_DestroySession: null session");
        return;
    }
    std::shared_ptr<ShimJitSession> held;
    {
        std::lock_guard<std::mutex> lock(sessionRegistryMutex());
        auto It = sessionRegistry().find(jit);
        if (It == sessionRegistry().end()) {
            setError("FilLLVMJIT_DestroySession: unknown session");
            return;
        }
        held = It->second;
        sessionRegistry().erase(It);
    }
}

int shim_SetOption(ShimJit jit, const char *key, const char *value) {
    auto S = lockSession(jit);
    if (!S) {
        setError("FilLLVMJIT_SetOption: null session");
        return -1;
    }
    std::lock_guard<std::mutex> lock(S->Mutex);
    return shim_SetOptionImpl(S.get(), key, value);
}

int shim_SetMemoryCallbacks(
    ShimJit jit, const struct filc_llvm_jit_memory_callbacks *callbacks) {
    auto S = lockSession(jit);
    if (!S || !callbacks) {
        setError("FilLLVMJIT_SetMemoryCallbacks: null argument");
        return -1;
    }
    if (!callbacks->allocate_jit_module || !callbacks->allocate_executable ||
        !callbacks->allocate_object || !callbacks->allocate_module_data ||
        !callbacks->finalize_object || !callbacks->resolve_symbol ||
        !callbacks->resolve_raw_pointer) {
        setError("FilLLVMJIT_SetMemoryCallbacks: required callback missing");
        return -1;
    }
    std::lock_guard<std::mutex> lock(S->Mutex);
    S->MemCallbacks = *callbacks;
    S->MemCallbacksSet = true;
    return 0;
}

// Compile a module and return the compiled record, or null on failure.
// On success the module and its dedicated context move to the JIT (no
// bitcode copy is made; the caller must have built the module in a
// dedicated context nothing else shares) and the caller hands ownership to
// ORC: the runtime nulls its handles.  On failure the module and context
// stay owned by the caller (the runtime keeps its handles and disposes
// them later), so the shim must not dispose them; it only reports the
// error, drops any ByDylib snapshot it inserted, and removes the dylib it
// created.  The session stays usable for further compiles and may be
// destroyed once compilation finishes: the shared_ptr held for the whole
// call keeps it alive across a concurrent DestroySession.  The caller
// passes its jit module object, which the new per-module JITDylib exposes
// as the absolute symbol `filc_jit_module` for the FilPizlonator-authored
// resolve calls to name.
ShimJitModule shim_CompileModule(ShimJit jit, LLVMModuleRef mod,
                                 const char *name, void *jitModule) {
    auto S = lockSession(jit);
    if (!S || !mod || !jitModule) {
        setError("FilLLVMJIT_CompileModule: null argument");
        return nullptr;
    }
    std::string modName = (name && name[0] != '\0') ? name : "jit-module";
    // Claim a module id and snapshot the sibling dylibs under the session
    // lock.  The new dylib links older siblings so modules compiled earlier
    // in this session stay visible; sibling dylibs are never removed.
    // Snapshot the memory-callbacks presence up front so a missing install
    // fails before any dylib is created.
    uint64_t bindId = 0;
    std::vector<llvm::orc::JITDylib *> siblings;
    {
        std::lock_guard<std::mutex> lock(S->Mutex);
        if (!S->MemCallbacksSet) {
            setError("FilLLVMJIT_CompileModule: no memory callbacks installed");
            return nullptr;
        }
        bindId = S->NextModuleId++;
        siblings = S->ModuleDylibs;
    }
    Module *MP = unwrap(mod);
    LLVMContext &CtxRef = MP->getContext();
    Triple JTTriple(S->JIT->getTargetTriple().str());
    FilCLayouts Layouts;
    if (!filCLayoutsFor(JTTriple, Layouts)) {
        setError(std::string("FilLLVMJIT_CompileModule: unsupported JIT target ") +
                 S->JIT->getTargetTriple().str());
        return nullptr;
    }
    MP->setTargetTriple(S->JIT->getTargetTriple().str());
    MP->setDataLayout(Layouts.Before);
    MP->setDataLayoutAfterFilC(Layouts.After);
    // AddGlobalMapping-style hook: rebind mapped extern declarations to
    // their host addresses before running the pipeline.  Blessed
    // lazy-resolution names are skipped here even when mapped: a custom
    // override still rebinds directly, but default handling is bound
    // post-pipeline to module-private wrappers, which is what keeps
    // concurrently live modules isolated.  A failed rebind (for example a
    // constant use of a mapped data global that cannot be rebound) fails
    // the compile with g_lastError set; the module stays caller-owned.
    {
        std::map<std::string, void *> mappings;
        {
            std::lock_guard<std::mutex> lock(S->Mutex);
            mappings = S->Mappings;
        }
        for (const auto &kv : mappings) {
            if (!kv.second)
                continue;
            if (isBlessedExternName(kv.first))
                continue;
            if (!rebindExternToAddress(*MP, kv.first, kv.second))
                return nullptr;
        }
    }
    // TargetMachine driving the Fil-C pass pipeline, large code model like
    // the JIT itself.
    JITTargetMachineBuilder JTMB(S->JIT->getTargetTriple());
    JTMB.setCodeModel(CodeModel::Large);
    auto TM = JTMB.createTargetMachine();
    if (!TM) {
        setError(TM.takeError());
        return nullptr;
    }
    // Snapshot the session compile options under the session lock: a
    // concurrent SetOption must not race the pipeline's reads.
    JITOptions OptsCopy;
    {
        std::lock_guard<std::mutex> lock(S->Mutex);
        OptsCopy = S->Opts;
    }
    runFilCPipeline(*MP, OptsCopy, **TM);
    // Module-routed lazy resolution.
    if (!bindModuleResolvers(*MP, S, bindId))
        return nullptr;
    if (const char *dump = getenv("FILC_LLVMJIT_DUMP")) {
        if (dump[0] != '\0' && dump[0] != '0') {
            errs() << "=== FilLLVMJIT module '" << modName
                   << "' after Fil-C pipeline ===\n"
                   << *MP << "\n";
        }
    }
    // The pipeline (and especially FilPizlonator) must leave verifiable IR
    // behind; check before handing the module to the JIT.
    {
        std::string V;
        raw_string_ostream OS(V);
        if (verifyModule(*MP, &OS)) {
            setError(std::string("FilLLVMJIT_CompileModule: invalid IR after "
                                 "Fil-C pipeline in module ") +
                     modName + ": " + OS.str());
            return nullptr;
        }
    }
    // Each module gets its own JITDylib defining the absolute symbol
    // `filc_jit_module` for this module's jit module object, so the
    // FilPizlonator-authored resolve calls name their own module.  The
    // dylib inherits the session default link order (process symbols) and
    // links older sibling dylibs.
    std::string dylibName = modName + "$" + std::to_string(bindId);
    auto JDOrErr = S->JIT->createJITDylib(dylibName);
    if (!JDOrErr) {
        setError(JDOrErr.takeError());
        return nullptr;
    }
    llvm::orc::JITDylib &JD = *JDOrErr;
    {
        ExecutionSession &ES = S->JIT->getExecutionSession();
        SymbolMap Absolute;
        Absolute[ES.intern("filc_jit_module")] = {
            ExecutorAddr::fromPtr(jitModule), JITSymbolFlags::Exported};
        if (Error Err = JD.define(absoluteSymbols(std::move(Absolute)))) {
            setError(std::move(Err));
            consumeError(S->JIT->getExecutionSession().removeJITDylib(JD));
            return nullptr;
        }
    }
    for (auto *sibling : siblings)
        JD.addToLinkOrder(*sibling, JITDylibLookupFlags::MatchExportedSymbolsOnly);
    // Snapshot this compile's runtime callbacks for the new dylib.  Linking
    // runs lazily on first lookup, so the memory manager reads them from
    // the shared table by dylib instead of from the session.
    {
        std::lock_guard<std::mutex> lock(S->Mutex);
        std::lock_guard<std::mutex> tableLock(S->CallbackTable->Mutex);
        S->CallbackTable->ByDylib[dylibName] = S->MemCallbacks;
    }
    // Take over the module and its dedicated context as is; no copy is
    // made.  The context object comes from the caller's LLVMContextCreate
    // and is exclusively owned by this module, so unique ownership moves
    // to the JIT here.
    ThreadSafeModule TSM{std::unique_ptr<Module>(MP),
                         ThreadSafeContext{std::unique_ptr<LLVMContext>(&CtxRef)}};
    if (Error Err = S->JIT->addIRModule(JD, std::move(TSM))) {
        setError(std::move(Err));
        eraseByDylib(S->CallbackTable, dylibName);
        consumeError(S->JIT->getExecutionSession().removeJITDylib(JD));
        return nullptr;
    }
    std::vector<llvm::orc::JITDylib *> searchOrder;
    searchOrder.reserve(siblings.size() + 1);
    searchOrder.push_back(&JD);
    for (auto *sibling : siblings)
        searchOrder.push_back(sibling);
    {
        std::lock_guard<std::mutex> lock(S->Mutex);
        S->ModuleDylibs.push_back(&JD);
    }
    // Nothrow: OOM here is a clean compile failure (NULL + GetLastError),
    // never an abort; see shim_Create.
    ShimJitModuleRec *rawRec = new (std::nothrow) ShimJitModuleRec();
    if (!rawRec) {
        setError("FilLLVMJIT_CompileModule: out of memory");
        eraseByDylib(S->CallbackTable, dylibName);
        consumeError(S->JIT->getExecutionSession().removeJITDylib(JD));
        // The dylib was already linked into the session's sibling list
        // above; drop it again so later compiles never link against the
        // removed dylib.
        {
            std::lock_guard<std::mutex> lock(S->Mutex);
            auto &dylibs = S->ModuleDylibs;
            dylibs.erase(std::remove(dylibs.begin(), dylibs.end(), &JD),
                         dylibs.end());
        }
        return nullptr;
    }
    auto rec = std::shared_ptr<ShimJitModuleRec>(rawRec);
    rec->JIT = S->JIT;
    rec->Dylib = &JD;
    rec->SearchOrder = std::move(searchOrder);
    rec->Id = bindId;
    rec->Name = modName;
    rec->DylibName = dylibName;
    rec->CallbackTable = S->CallbackTable;
    ShimJitModule raw = rec.get();
    {
        std::lock_guard<std::mutex> lock(moduleRegistryMutex());
        moduleRegistry()[raw] = std::move(rec);
    }
    return raw;
}

void shim_DestroyModule(ShimJitModule mod) {
    // Drop the compiled-module record and its ByDylib callback snapshot.
    // The shared LLJIT reference dies with the last record; dylibs are
    // never removed, so older-sibling links held by younger modules stay
    // valid.  Erasing the registry entry first makes a concurrent lookup
    // that already promoted the handle keep it alive, while later lookups
    // with the stale handle fail instead of dereferencing freed memory.
    if (!mod)
        return;
    std::shared_ptr<ShimJitModuleRec> held;
    {
        std::lock_guard<std::mutex> lock(moduleRegistryMutex());
        auto It = moduleRegistry().find(mod);
        if (It == moduleRegistry().end())
            return;
        held = It->second;
        moduleRegistry().erase(It);
    }
    eraseByDylib(held->CallbackTable, held->DylibName);
}

int shim_AddGlobalMapping(ShimJit jit, const char *name, void *address) {
    auto S = lockSession(jit);
    if (!S || !name || name[0] == '\0') {
        setError("FilLLVMJIT_AddGlobalMapping: null argument");
        return -1;
    }
    std::lock_guard<std::mutex> lock(S->Mutex);
    if (!address)
        S->Mappings.erase(name);
    else
        S->Mappings[name] = address;
    return 0;
}

// Address lookups run against a compiled module, which keeps the shared
// LLJIT alive; the session may be forgotten once compilation finishes.
void *shim_GetFunctionAddress(ShimJitModule mod, const char *name) {
    auto rec = lockModule(mod);
    if (!rec) {
        setError("FilLLVMJIT lookup: unknown module");
        return nullptr;
    }
    return shim_LookupImpl(rec, name);
}

void *shim_GetGlobalValueAddress(ShimJitModule mod, const char *name) {
    auto rec = lockModule(mod);
    if (!rec) {
        setError("FilLLVMJIT lookup: unknown module");
        return nullptr;
    }
    return shim_LookupImpl(rec, name);
}

void *shim_GetPointerToGlobal(ShimJitModule mod, const char *name) {
    auto rec = lockModule(mod);
    if (!rec) {
        setError("FilLLVMJIT lookup: unknown module");
        return nullptr;
    }
    return shim_LookupImpl(rec, name);
}

const char *shim_GetLastError(void) { return g_lastError.c_str(); }

static const struct filc_llvm_jit_api g_api = {
#include "shim_api_init.inc"
};

// The single dynamically exported symbol of libfilcllvmjit.so.  Default
// visibility is spelled out because the TU compiles with
// -fvisibility=hidden (a version script alone cannot re-export a hidden
// symbol).
extern "C" __attribute__((visibility("default")))
const struct filc_llvm_jit_api *FilLLVMJIT_GetApi(void)
{
    return &g_api;
}

extern "C" __attribute__((visibility("hidden")))
int __cxa_atexit(void (*f)(void *), void *arg, void *dso)
{
    return 0;
}
