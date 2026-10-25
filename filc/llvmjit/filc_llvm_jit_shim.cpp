/*
   Copyright (c) 2026 Filip Pizlo. All Rights Reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
   OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* libfilcllvmjit.so implementation.

   This translation unit is the only code (besides LLVM itself) linked into
   libfilcllvmjit.so.  It exposes exactly one dynamic symbol,
   FilLLVMJIT_GetApi (enforced by filc_llvm_jit_shim.version plus
   -fvisibility=hidden and -Wl,--exclude-libs,ALL), which returns a pointer
   to a FilLLVMJITApi function-pointer table.  libpizlo dlopens this library
   lazily on first use of the Fil-C LLVM JIT API and forwards calls through
   that table, so libpizlo itself never links LLVM.

   Contents:
    - lazy process-wide init on first Create/GetHeapMemory: LLVM
      native-target setup
      plus a single 128MB RWX mmap handed to jit_heap_add_fresh_memory when
      that symbol is visible (via dlsym on the host process), and always
      retrievable via FilLLVMJIT_GetHeapMemory.
    - an ORC LLJIT session per FilLLVMJITRef, each with its own
      lazy-resolution state (resolve callback/context and zexact table live
      in a per-session map under g_mu).  Resolution is routed per session:
      at AddModule time every use of the blessed lazy-resolution declarations
      (filc_llvm_jit_resolve, filc_llvm_jit_resolve_raw_pointer, zexternptr)
      that is mapped to this library's default helper is rewritten to a
      module-private wrapper carrying the owning session, so JITed code can
      only ever invoke its own session's callback/table -- never another
      live session's.  A second live session is fully supported;
      sessions are immortal (no destroy entry point pins them process-wide,
      mirroring the runtime's session objects), so per-session entries and
      baked session pointers stay valid forever.
    - a test-only executor (FilLLVMJIT_InvokeIntFunction) that runs a JITed
      no-arg int function through its pizlonated getter + fast entrypoint,
      so host harnesses can prove call-through without a runtime bridge.
    - Fil-C pass pipeline mirroring clang/lib/CodeGen/BackendUtil.cpp:
      KillUB -> EarlyFPM (LowerExpectIntrinsic, SimplifyCFG, SROA, EarlyCSE,
      DSE?) -> ModuleInlinerWrapperPass pipeline -> Coro pipeline (CoroEarly,
      CoroSplit, CoroCleanup, GlobalDCE under CoroConditionalWrapper) ->
      FilPizlonator -> DeleteRedundantPollchecks.
    - AddGlobalMapping-style hook: extern declarations in the compiled module
      whose names have mapped addresses are bound to their host addresses.
      The blessed names filc_llvm_jit_resolve, filc_llvm_jit_resolve_raw_pointer,
      and zexternptr use the raw (unpizlonated) ABI and are passed through
      FilPizlonator untouched, so their declarations are rebound to inttoptr
      constants before the pipeline runs.  Every other mapped function name
      denotes a pizlonated extern: its declaration is left alone so that
      FilPizlonator authors the usual lazy-extern worker for it
      (__filc_jit_get_<name>, which wraps the resolved address as a flight),
      and after the pipeline that worker's body is stubbed to return the
      mapped address wrapped as a flight {addr,addr}.  Rebinding a pizlonated
      function declaration to a raw ptr constant instead would be a type
      mismatch against the flight-pointer ABI: call sites lowered by
      FilPizlonator expect a {ptr,lower} flight, so a bare ptr would build a
      broken flight (e.g. {addr,null}) that fails verification or panics in
      checkFunctionAndGetLower at run time.
      When a mapped name (in particular a mapped data
      global) is still referenced after the pipeline but has no bindable
      pizlonated_<name> getter or __filc_jit_get_<name> worker, AddModule
      fails with an explanatory message instead of silently dropping the
      mapping (see failOnUnboundMappings).
      The blessed names are pre-mapped to host-side helpers implementing lazy
      extern resolution (via the user-provided resolve callback) and
      zexact_ptrtable decoding.

   NOTE on libc: this .so is dlopened from inside Fil-C processes, so it is
   linked with -nodefaultlibs plus static libstdc++/libgcc and carries no
   shared NEEDEDs; its remaining undefined libc references bind to the
   hosting process's globals. The one libc function the static libstdc++
   needs that Fil-C processes do not provide is atexit (used by
   std::condition_variable); it is defined below in terms of the
   __cxa_atexit the host process does provide. The definition stays local
   (hidden visibility + version script export only FilLLVMJIT_GetApi), so it
   satisfies just this library's internal references.
 */

extern "C" int __cxa_atexit(void (*func)(void *), void *arg, void *dso);
extern "C" int atexit(void (*func)(void)) {
    return __cxa_atexit(
        reinterpret_cast<void (*)(void *)>(func), nullptr, nullptr);
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <sys/mman.h>
#include <set>

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/InlineAdvisor.h"
#include "llvm/Analysis/InlineCost.h"
#include "llvm/Analysis/InlineOrder.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
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

namespace {

using namespace llvm;
using namespace llvm::orc;

/* Compile options.  Defaults mirror BackendUtil.cpp's Fil-C flags.

   KillUB and the FilPizlonator pass are intentionally NOT tunable: they
   always run (see runFilCPipeline).  There is no JITOptions field for them
   and SetOption rejects every spelling that would disable them. */
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

struct JITSession {
    std::unique_ptr<LLJIT> JIT;
    JITOptions Opts;
    std::map<std::string, void *> Mappings;
    uint64_t NextModuleId = 1;
};

/* Per-session lazy-resolution state.  The resolve callback, its context,
   and the zexact pointer table are configured per session (the owning
   JITSession* is the map key); the old process-global variables they replace
   meant the last Set* call clobbered every other session.  All access is
   under g_mu.  Seq records Set* call order; it only orders the legacy
   scan-based shared helpers (kept for the API table and custom-mapped
   modules) -- newly compiled modules bypass the scan via session-carrying
   wrappers (see bindSessionResolvers), so Seq no longer affects them.
   Sessions are immortal (no destroy entry point pins them process-wide,
   mirroring the runtime's session objects), so entries are never removed. */
struct SessionResolveState {
    FilLLVMJITResolveFunction Callback = nullptr;
    void *Context = nullptr;
    void *ZExactTable = nullptr;
    uint64_t Seq = 0;
};

struct JITModuleRec {
    JITSession *Session;
    uint64_t Id;
    std::string Name;
};

std::recursive_mutex g_mu;
bool g_initialized = false;
void *g_heapBase = nullptr;
unsigned long long g_heapSize = 0;
std::map<JITSession *, SessionResolveState> g_sessionStates;
uint64_t g_resolveSeq = 0;
/* Cached dlsym results for host runtime helpers (may be absent). */
void *(*g_decodeFn)(void *, size_t) = nullptr;
bool g_decodeLookedUp = false;
void (*g_jitHeapAddFreshMemory)(uintptr_t, uintptr_t) = nullptr;
bool g_jitHeapLookedUp = false;
thread_local std::string g_lastError;

void setError(const std::string &s) { g_lastError = s; }

void setError(Error &&E) {
    std::string S;
    raw_string_ostream OS(S);
    logAllUnhandledErrors(std::move(E), OS);
    g_lastError = std::move(S);
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

/* Run the Fil-C IR pass pipeline over M.  Mirrors the pipeline registered
   in clang/lib/CodeGen/BackendUtil.cpp for Fil-C compilations. */
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
    /* KillUB is mandatory and non-optional: it always runs, regardless of
       user options.  UB in JITed IR must be killed before FilPizlonator
       reasons about it, exactly like the clang Fil-C pipeline. */
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
                                                      /*EagerlyInvalidate=*/true));
        if (Opts.Inline) {
            InlineParams IP = getInlineParams(Level.getSpeedupLevel(),
                                              Level.getSizeLevel());
            ModuleInlinerWrapperPass MIWP(
                IP, /*MandatoryFirst=*/true,
                InlineContext{ThinOrFullLTOPhase::None,
                              InlinePass::CGSCCInliner},
                InliningAdvisorMode::Default,
                /*MaxDevirtIterations=*/4);
            MIWP.addModulePass(RequireAnalysisPass<GlobalsAA, Module>());
            MIWP.addModulePass(createModuleToFunctionPassAdaptor(
                InvalidateAnalysisPass<AAManager>()));
            MIWP.addModulePass(
                RequireAnalysisPass<ProfileSummaryAnalysis, Module>());
            CGSCCPassManager &MainCGPipeline = MIWP.getPM();
            MainCGPipeline.addPass(
                PostOrderFunctionAttrsPass(/*SkipNonRecursive=*/true));
            FunctionPassManager InlinerFPM;
            if (Opts.InlineSROA)
                InlinerFPM.addPass(SROAPass(SROAOptions::ModifyCFG));
            if (Opts.InlineEarlyCSE)
                InlinerFPM.addPass(EarlyCSEPass(true /* Enable mem-ssa. */));
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
                std::move(InlinerFPM),
                /*EagerlyInvalidateAnalyses=*/true, /*NoRerun=*/true));
            MainCGPipeline.addPass(PostOrderFunctionAttrsPass());
            MainCGPipeline.addPass(createCGSCCToFunctionPassAdaptor(
                RequireAnalysisPass<ShouldNotRunFunctionPassesAnalysis,
                                    Function>()));
            MIWP.addLateModulePass(createModuleToFunctionPassAdaptor(
                InvalidateAnalysisPass<ShouldNotRunFunctionPassesAnalysis>()));
            MPM.addPass(std::move(MIWP));
        }
    }
    /* Lower C++20 coroutines before pizlonating, exactly like the clang
       pipeline does: CoroSplit turns each coroutine into ordinary
       ramp/resume/destroy functions whose frame is a normal heap
       allocation, so FilPizlonator never sees the llvm.coro.* intrinsics. */
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
    /* JIT-compiled modules are linked at run time, so externs must resolve
       lazily: run FilPizlonator in JIT mode so it authors per-extern lazy
       getters (via filc_llvm_jit_resolve) and lowers the zexternptr idiom
       (via filc_llvm_jit_resolve_raw_pointer). */
    MPM.addPass(FilPizlonatorPass(/*JitMode=*/true));
    MPM.addPass(DeleteRedundantPollchecksPass());
    MPM.run(M, MAM);
}

/* Names whose declarations use the raw (unpizlonated) ABI and are passed
   through FilPizlonator untouched (see FilPizlonator::shouldPassThrough in
   JitMode).  Rebinding these to a raw inttoptr constant is type-correct:
   their call sites keep the declared raw FunctionType.  Every other mapped
   function name denotes a pizlonated extern whose call sites become flight
   calls after the pipeline, so a raw pointer constant would be a type
   mismatch against the flight-pointer ABI; those are bound post-pipeline by
   stubbing the lazy-extern worker FilPizlonator authors for them. */
bool isRawAbiExternName(const std::string &Name) {
    return Name == "filc_llvm_jit_resolve" ||
           Name == "filc_llvm_jit_resolve_raw_pointer" ||
           Name == "zexternptr";
}

/* Raw-ABI globals: FilPizlonator passes these through untouched (see
   FilPizlonator::shouldPassThrough(GlobalVariable*) in JitMode), so their
   uses keep the raw pointer type and a direct inttoptr rebind is
   type-correct.  Every other mapped data global denotes a pizlonated extern:
   FilPizlonator deletes the original declaration and routes all uses through
   the pizlonated_<name> getter (PizlonatedGetterTy, returning a flight), so
   a pre-pipeline raw rebind would be discarded with the declaration; those
   are bound post-pipeline by stubbing the getter to return the mapped
   address wrapped as a flight {addr,addr}. */
bool isRawAbiGlobalName(const std::string &Name) {
    return Name == "zexact_ptrtable";
}

/* The blessed lazy-resolution declarations.  In JitMode FilPizlonator passes
   these through untouched, so both their pre-pipeline declarations and their
   post-pipeline uses keep the raw ABI.  Each has a default host-side helper
   (see defaultBlessedHelper below); a mapping that still points at the
   default is routed per session (module-private bound wrapper, see
   bindSessionResolvers), while a custom override (e.g. the Fil-C runtime's
   root-recording shims installed at session creation) keeps the legacy
   direct inttoptr rebind. */
bool isBlessedExternName(const std::string &Name) {
    return Name == "filc_llvm_jit_resolve" ||
           Name == "filc_llvm_jit_resolve_raw_pointer" ||
           Name == "zexternptr";
}

/* Shape check for pizlonated flights: the real %filc_flight_ptr type.

   This is NOT a string-name check on an arbitrary
   struct.  The caller passes the module's uniqued flight type (looked up
   via Module::getTypeByName("filc_flight_ptr"), i.e. the exact StructType*
   FilPizlonator created), and identity against it is required.  A purely
   structural {ptr,ptr} check would misclassify user structs (e.g. {i8*,i8*}
   pairs) as flights and stub the wrong declarations; a pure name check
   would misfire on a same-named unrelated struct.  The element-shape check
   below is belt-and-braces on top of uniqued identity. */
bool isFlightType(Type *T, StructType *FlightPtrTy) {
    if (!T || !FlightPtrTy || T != FlightPtrTy)
        return false;
    return FlightPtrTy->getNumElements() == 2 &&
           FlightPtrTy->getElementType(0)->isPointerTy() &&
           FlightPtrTy->getElementType(1)->isPointerTy();
}

/* Wrap a host address as a flight value {addr,addr} of the given flight
   type.  This mirrors exactly what FilPizlonator itself authors for lazy
   externs (createFlightPtr(ResolvedRaw, ResolvedRaw) in emitJitLazyExterns
   and the zexternptr lowering) and what the runtime bridge hands JITed code
   (filc_llvm_jit_resolve_impl returns filc_ptr_ptr, wrapped as a flight by
   the worker): payload flights carry the payload in both lanes (see
   flightPtrForPayload), so for function payloads this rebuilds the exact
   original flight.  Data globals use the same convention: the single address
   carried by AddGlobalMapping is the payload (upper), and stubbing the
   pizlonated_<name> data getter with it is behavior-identical to a lazy
   resolution returning that address. */
Value *flightValueForAddress(LLVMContext &C, Type *FlightTy, void *Addr,
                             BasicBlock *BB) {
    PointerType *PtrTy = PointerType::get(C, 0);
    Constant *AddrConst = ConstantExpr::getIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Addr))),
        PtrTy);
    Value *Flight = InsertValueInst::Create(
        UndefValue::get(FlightTy), AddrConst, 0, "filc_jit_rebound_ptr", BB);
    Flight = InsertValueInst::Create(
        Flight, AddrConst, 1, "filc_jit_rebound_lower", BB);
    return Flight;
}

/* Bind a pizlonated lazy-extern worker (or a still-undefined pizlonated
   getter) to a host address by giving it a body that returns the address
   wrapped as a flight {addr,addr}.  Any existing body (e.g. the
   resolve-cache-resolve worker FilPizlonator authored) is discarded; the
   function's type, linkage, and attributes are preserved, so every existing
   use (getter forwarders, call-site thunks) keeps verifying and now resolves
   eagerly to the mapped address.  Returns false when F is missing or does
   not return a flight (caller falls back to the raw rebind).  FlightTy is
   the module's uniqued flight type (see isFlightType); a null FlightTy
   (module never pizlonated) refuses every stub so nothing is misclassified. */
bool stubFlightGetterToAddress(Function *F, void *Addr, StructType *FlightTy) {
    if (!F || !isFlightType(F->getReturnType(), FlightTy))
        return false;
    if (!F->isDeclaration()) {
        /* The worker already has a body (e.g. the resolve-cache-resolve
           worker FilPizlonator authored for this extern).  Discard it so the
           stub below replaces the whole lazy protocol; the function's type,
           linkage, and attributes are preserved. */
        F->deleteBody();
        assert(F->isDeclaration());
    }
    LLVMContext &C = F->getContext();
    BasicBlock *BB = BasicBlock::Create(C, "filc_jit_rebound", F);
    Value *Flight = flightValueForAddress(C, F->getReturnType(), Addr, BB);
    ReturnInst::Create(C, Flight, BB);
    return true;
}

/* Bind the lazy-extern worker FilPizlonator authored for the extern Name
   (__filc_jit_get_<Name>) to the mapped host address.  No-op when the module
   never referenced the extern (no worker exists) or the worker does not
   return a flight. */
bool stubLazyExternWorker(Module &M, const std::string &Name, void *Addr) {
    return stubFlightGetterToAddress(
        M.getFunction("__filc_jit_get_" + Name), Addr,
        StructType::getTypeByName(M.getContext(), "filc_flight_ptr"));
}

/* Rebind an extern declaration to a host address by replacing all of its
   uses with an inttoptr constant of the declaration's pointer type.

   Only valid for declarations whose uses keep the raw pointer ABI: the
   blessed raw-ABI helper functions (see isRawAbiExternName) and raw-ABI
   globals (see isRawAbiGlobalName).  Must NOT be used on other function or
   data declarations: after FilPizlonator runs, their uses go through
   pizlonated_<name> getters and expect flights, so a raw ptr constant is a
   type mismatch versus the pizlonated flight-pointer ABI (and the rebind
   would be discarded with the declaration anyway).  Those are bound
   post-pipeline by stubbing the getter / lazy-extern worker FilPizlonator
   authors for them.  The guards live here (rather than only at the AddModule
   call site) so the GlobalAlias recursion below inherits them: recursing
   into a pizlonated aliasee must skip exactly like a direct mapping would
   (rebinding it would corrupt the alias into a non-GEP constant aliasee
   that FilPizlonator rejects). */
void rebindExternToAddress(Module &M, const std::string &Name, void *Addr) {
    LLVMContext &C = M.getContext();
    PointerType *PtrTy = PointerType::get(C, 0);
    Constant *AddrConst = ConstantExpr::getIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                          static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Addr))),
        PtrTy);
    if (Function *F = M.getFunction(Name)) {
        if (F->isDeclaration() && !isRawAbiExternName(Name))
            return;
        if (F->isDeclaration())
            F->replaceAllUsesWith(AddrConst);
        return;
    }
    if (GlobalVariable *G = M.getNamedGlobal(Name)) {
        if (G->isDeclaration() && !isRawAbiGlobalName(Name))
            return;
        if (G->isDeclaration())
            G->replaceAllUsesWith(AddrConst);
        return;
    }
    if (GlobalAlias *A = M.getNamedAlias(Name)) {
        /* Rebind alongside the aliasee: an alias forwards to its aliasee,
           so a mapping spelled via the alias name also binds the aliasee
           declaration (subject to the same raw-ABI guards above, which the
           recursion inherits).  Alias-to-alias chains
           are resolved iteratively to the ultimate aliasee (visited set, so
           cyclic alias graphs terminate instead of recursing forever), and
           only then is the single recursive rebind issued against a
           function/global target.
           Post-pipeline, pizlonated_<alias> getters forward to the
           pizlonated_<aliasee> getter FilPizlonator authors, so stubbing the
           aliasee covers alias spellings too (and a mapping spelled via the
           alias name stubs pizlonated_<alias> directly). */
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
            rebindExternToAddress(M, Target->getName().str(), Addr);
        return;
    }
}

/* Fil-C data layouts, mirroring the resetDataLayout calls in
   clang/lib/Basic/Targets/X86.h (x86-64) and AArch64.cpp (AArch64leTargetInfo
   ELF).  The "before" layout has a non-integral address space 0, which
   FilPizlonator requires on input; the "after" layout is what FilPizlonator
   leaves behind (and what codegen uses). */
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

} // namespace

/* Host-side lazy resolvers.  These addresses are baked into JITed modules
   via the AddGlobalMapping-style hook (see shim_AddModule), so they must
   never be exported dynamically; the single-export rule for this library
   is unaffected.  The TU compiles with -fvisibility=hidden and the version
   script keeps only FilLLVMJIT_GetApi, and every helper below additionally
   carries an explicit hidden attribute so the single-export rule survives
   even if the compile flag is ever dropped. */
#define FILC_JIT_HIDDEN __attribute__((visibility("hidden")))
extern "C" {

FILC_JIT_HIDDEN void *FilLLVMJIT_shim_resolveByName(const char *name) {
    /* Owning-session lookup: the JITed caller passes only the symbol name,
       so the session cannot be named directly.  Scan the per-session states
       instead: with a single configured session (the supported production
       shape) its callback is used; with several, the most-recently-set one
       wins, mirroring the old last-Set-wins global behavior. */
    FilLLVMJITResolveFunction cb = nullptr;
    void *ctx = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        uint64_t best = 0;
        for (const auto &kv : g_sessionStates) {
            if (kv.second.Callback && kv.second.Seq >= best) {
                best = kv.second.Seq;
                cb = kv.second.Callback;
                ctx = kv.second.Context;
            }
        }
    }
    if (!cb || !name)
        return nullptr;
    return cb(name, ctx);
}

FILC_JIT_HIDDEN void *FilLLVMJIT_shim_resolveRaw(unsigned long long encoded) {
    const void *table = nullptr;
    void *(*decode)(void *, size_t) = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        uint64_t best = 0;
        for (const auto &kv : g_sessionStates) {
            if (kv.second.ZExactTable && kv.second.Seq >= best) {
                best = kv.second.Seq;
                table = kv.second.ZExactTable;
            }
        }
        if (!g_decodeLookedUp) {
            g_decodeLookedUp = true;
            void *sym = dlsym(RTLD_DEFAULT, "zexact_ptrtable_decode");
            /* Prototype: void *zexact_ptrtable_decode(zexact_ptrtable *,
               size_t).  The table pointer is opaque here. */
            g_decodeFn = reinterpret_cast<void *(*)(void *, size_t)>(sym);
        }
        decode = g_decodeFn;
    }
    if (table && decode)
        return decode(const_cast<void *>(table), static_cast<size_t>(encoded));
    return reinterpret_cast<void *>(static_cast<uintptr_t>(encoded));
}

/* Session-routed resolution.  The legacy helpers above
   scan every session and hand the name to the most-recently-configured
   callback/table, so JITed code compiled by an older session silently calls
   a newer session's resolver.  The ForSession variants below consult ONLY
   the named session's state; AddModule rewrites each compiled module's
   blessed declarations to module-private wrappers that pass the owning
   session explicitly (see bindSessionResolvers), so older code can never
   reach a newer callback.  The legacy scan-based helpers stay for ABI
   stability (the FilLLVMJITApi table's ResolveByName/ResolveRawPointer
   fields and custom-mapped modules) but no newly compiled module routes
   through them by default. */
FILC_JIT_HIDDEN void *FilLLVMJIT_shim_resolveByNameForSession(
    const char *name, void *session) {
    FilLLVMJITResolveFunction cb = nullptr;
    void *ctx = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        auto it = g_sessionStates.find(
            reinterpret_cast<JITSession *>(session));
        if (it != g_sessionStates.end()) {
            cb = it->second.Callback;
            ctx = it->second.Context;
        }
    }
    if (!cb || !name)
        return nullptr;
    return cb(name, ctx);
}

FILC_JIT_HIDDEN void *FilLLVMJIT_shim_resolveRawForSession(
    unsigned long long encoded, void *session) {
    const void *table = nullptr;
    void *(*decode)(void *, size_t) = nullptr;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        auto it = g_sessionStates.find(
            reinterpret_cast<JITSession *>(session));
        if (it != g_sessionStates.end())
            table = it->second.ZExactTable;
        if (!g_decodeLookedUp) {
            g_decodeLookedUp = true;
            void *sym = dlsym(RTLD_DEFAULT, "zexact_ptrtable_decode");
            /* Prototype: void *zexact_ptrtable_decode(zexact_ptrtable *,
               size_t).  The table pointer is opaque here. */
            g_decodeFn = reinterpret_cast<void *(*)(void *, size_t)>(sym);
        }
        decode = g_decodeFn;
    }
    if (table && decode)
        return decode(const_cast<void *>(table), static_cast<size_t>(encoded));
    return reinterpret_cast<void *>(static_cast<uintptr_t>(encoded));
}

} // extern "C"

namespace {

/* Default host-side helper for a blessed lazy-resolution name: the address
   shim_Create pre-installs in S->Mappings.  A mapping that still points here
   takes the session-bound wrapper path (see bindSessionResolvers); anything
   else (e.g. the Fil-C runtime's root-recording shims, or a host harness
   override) keeps the legacy direct rebind. */
void *defaultBlessedHelper(const std::string &Name) {
    if (Name == "filc_llvm_jit_resolve")
        return reinterpret_cast<void *>(FilLLVMJIT_shim_resolveByName);
    /* Both raw-pointer spellings share one helper. */
    return reinterpret_cast<void *>(FilLLVMJIT_shim_resolveRaw);
}

/* Author a module-private wrapper that routes one blessed declaration to
   the owning session's resolver state:
     define internal ptr @__filc_jit_resolve_bound_<name>_<id>(<arg>) {
       %fn = inttoptr i64 <ForSessionAddr> to ptr (<argty>, ptr)*
       %sess = inttoptr i64 <session> to ptr
       %r = call ptr %fn(<arg>, %sess)
       ret ptr %r
     }
   All addresses are baked as integers, so the wrapper needs no symbol
   resolution at JIT-link time.  TakesEncoded selects the (i64)->ptr shape
   used by filc_llvm_jit_resolve_raw_pointer/zexternptr; otherwise the
   (ptr)->ptr shape of filc_llvm_jit_resolve. */
Function *authorSessionBoundWrapper(Module &M, const char *BlessedName,
                                    void *ForSessionAddr, JITSession *S,
                                    uint64_t Id, bool TakesEncoded) {
    LLVMContext &C = M.getContext();
    PointerType *PtrTy = PointerType::get(C, 0);
    Type *ArgTy = TakesEncoded ? static_cast<Type *>(Type::getInt64Ty(C))
                               : static_cast<Type *>(PtrTy);
    FunctionType *WTy = FunctionType::get(PtrTy, {ArgTy}, false);
    std::string WName = std::string("__filc_jit_resolve_bound_") +
                        BlessedName + "_" + std::to_string(Id);
    Function *W =
        Function::Create(WTy, GlobalValue::InternalLinkage, WName, &M);
    W->addFnAttr(Attribute::NoUnwind);
    BasicBlock *BB = BasicBlock::Create(C, "entry", W);
    IRBuilder<> B(BB);
    FunctionType *TTy = FunctionType::get(PtrTy, {ArgTy, PtrTy}, false);
    Value *TFn = B.CreateIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                         static_cast<uint64_t>(
                             reinterpret_cast<uintptr_t>(ForSessionAddr))),
        PointerType::get(TTy, 0));
    Value *Sess = B.CreateIntToPtr(
        ConstantInt::get(Type::getInt64Ty(C),
                         static_cast<uint64_t>(
                             reinterpret_cast<uintptr_t>(S))),
        PtrTy);
    Value *R = B.CreateCall(TTy, TFn, {W->getArg(0), Sess});
    B.CreateRet(R);
    return W;
}

/* Bind the blessed lazy-resolution declarations of a
   freshly compiled module to its owning session.  For each blessed name
   still declared (FilPizlonator passes them through untouched) with live
   uses: when the session's mapping still points at this library's default
   helper, replace every use with a module-private wrapper carrying the
   session (authorSessionBoundWrapper) and drop the declaration, so the
   module can only ever resolve through its own session's callback/table;
   when the mapping was overridden (custom host address, e.g. the Fil-C
   runtime's root-recording shims), keep the legacy direct inttoptr rebind;
   when unmapped, leave the declaration for ORC's process-symbol fallback.
   Returns false with g_lastError set when wrapper authoring fails (cannot
   happen without OOM/assert, but AddModule must not proceed unbound). */
bool bindSessionResolvers(Module &M, JITSession *S, uint64_t Id) {
    struct Blessed {
        const char *Name;
        bool TakesEncoded;
    };
    static const Blessed List[] = {
        {"filc_llvm_jit_resolve", false},
        {"filc_llvm_jit_resolve_raw_pointer", true},
        {"zexternptr", true},
    };
    for (const Blessed &B : List) {
        Function *F = M.getFunction(B.Name);
        if (!F || F->use_empty())
            continue;
        void *Mapped = nullptr;
        bool HasMapping = false;
        {
            std::lock_guard<std::recursive_mutex> lock(g_mu);
            auto It = S->Mappings.find(B.Name);
            if (It != S->Mappings.end()) {
                HasMapping = true;
                Mapped = It->second;
            }
        }
        if (HasMapping && Mapped && Mapped != defaultBlessedHelper(B.Name)) {
            /* Custom override: legacy direct rebind to the mapped address. */
            rebindExternToAddress(M, B.Name, Mapped);
            continue;
        }
        if (!HasMapping || !Mapped)
            continue;
        void *ForSession = (B.TakesEncoded)
                               ? reinterpret_cast<void *>(
                                     FilLLVMJIT_shim_resolveRawForSession)
                               : reinterpret_cast<void *>(
                                     FilLLVMJIT_shim_resolveByNameForSession);
        Function *W = authorSessionBoundWrapper(M, B.Name, ForSession, S, Id,
                                                B.TakesEncoded);
        if (!W || W->getFunctionType() != F->getFunctionType()) {
            setError(std::string("FilLLVMJIT_AddModule: cannot author "
                                 "session-bound resolver for ") +
                     B.Name);
            return false;
        }
        F->replaceAllUsesWith(W);
        F->eraseFromParent();
    }
    return true;
}

/* Refuse to silently drop a mapped name.  After all
   binding ran, any still-undefined function/global declaration with live
   uses whose (pizlonated_-stripped) name carries a non-raw,
   non-blessed mapping is a reference the pipeline left behind that nothing
   bound: JIT-linking it would dangle, so fail the AddModule with a message
   naming the symbol.  Unmapped lazy externs (normal: resolved at run time
   through the session's resolver) and unreferenced declarations are
   untouched. */
bool failOnUnboundMappings(Module &M, JITSession *S, const std::string &ModName) {
    std::map<std::string, void *> Mappings;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        Mappings = S->Mappings;
    }
    auto check = [&](const std::string &DeclName, const char *Kind) -> bool {
        if (isRawAbiExternName(DeclName) || isBlessedExternName(DeclName))
            return true;
        constexpr char Prefix[] = "pizlonated_";
        constexpr size_t PrefixLen = sizeof(Prefix) - 1;
        auto It = Mappings.find(DeclName);
        if (It == Mappings.end() && DeclName.compare(0, PrefixLen, Prefix) == 0)
            It = Mappings.find(DeclName.substr(PrefixLen));
        if (It == Mappings.end() || !It->second)
            return true;
        if (isRawAbiExternName(It->first) || isBlessedExternName(It->first) ||
            isRawAbiGlobalName(It->first))
            return true;
        setError(std::string("FilLLVMJIT_AddModule: mapped ") + Kind + " '" +
                 It->first + "' is still referenced in module '" + ModName +
                 "' but has no pizlonated getter or lazy-extern worker to "
                 "bind; refusing to drop the mapping silently");
        return false;
    };
    for (Function &F : M.functions()) {
        if (!F.isDeclaration() || F.use_empty())
            continue;
        if (!check(F.getName().str(), "extern"))
            return false;
    }
    for (GlobalVariable &G : M.globals()) {
        if (!G.isDeclaration() || G.use_empty())
            continue;
        if (!check(G.getName().str(), "data global"))
            return false;
    }
    return true;
}

int shim_EnsureInitializedImpl() {
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    if (g_initialized)
        return 0;
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    /* Boot the Fil-C JIT heap with a single 128MB RWX mapping.  16B
       alignment is trivially satisfied (page-aligned). Lazily booted on
       first Create/GetHeapMemory: there is no user-visible Initialize entry
       (API v4); both entry points below call here idempotently. */
    constexpr unsigned long long heapSize = 128ULL << 20;
    void *base = mmap(nullptr, static_cast<size_t>(heapSize),
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        setError("FilLLVMJIT lazy init: 128MB RWX mmap failed");
        return -1;
    }
    g_heapBase = base;
    g_heapSize = heapSize;
    if (!g_jitHeapLookedUp) {
        g_jitHeapLookedUp = true;
        void *sym = dlsym(RTLD_DEFAULT, "jit_heap_add_fresh_memory");
        /* Prototype: void jit_heap_add_fresh_memory(pas_range), where
           pas_range is { uintptr_t begin, end }. */
        g_jitHeapAddFreshMemory =
            reinterpret_cast<void (*)(uintptr_t, uintptr_t)>(sym);
    }
    if (g_jitHeapAddFreshMemory) {
        uintptr_t begin = reinterpret_cast<uintptr_t>(base);
        g_jitHeapAddFreshMemory(begin, begin + heapSize);
    }
    /* If jit_heap_add_fresh_memory is not (yet) visible, the Fil-C runtime
       can feed the range to the JIT heap itself; see GetHeapMemory. */
    g_initialized = true;
    return 0;
}

int shim_SetOptionImpl(JITSession *S, const char *key, const char *value) {
    /* Locking contract: callers hold g_mu (see shim_SetOption); AddModule
       copies Opts under the same lock, so reads and writes never race. */
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
    /* KillUB and the FilPizlonator pass are mandatory and cannot be turned
       off: the pipeline above always runs KillUBPass and
       FilPizlonatorPass.  Reject every legacy spelling that would disable
       them, whatever the value (even "1": there is no key that controls
       them, so accepting any value would mislead). */
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

void *shim_LookupImpl(JITSession *S, const char *name) {
    if (!S || !name) {
        setError("FilLLVMJIT lookup: null argument");
        return nullptr;
    }
    /* The Fil-C pass pipeline (FilPizlonator) renames IR functions to their
       pizlonated ABI form (pizlonated_<name>), so an exact lookup usually
       misses.  Transparently retry with that prefix: callers ask for the
       name they authored in IR and get the JITed (pizlonated-ABI) address. */
    if (auto Sym = S->JIT->lookup(name)) {
        g_lastError.clear();
        return Sym->toPtr<void *>();
    } else {
        std::string S2;
        raw_string_ostream OS(S2);
        logAllUnhandledErrors(Sym.takeError(), OS);
        setError(S2);
    }
    std::string prefixed = std::string("pizlonated_") + name;
    if (auto Sym = S->JIT->lookup(prefixed)) {
        g_lastError.clear();
        return Sym->toPtr<void *>();
    } else {
        std::string S2;
        raw_string_ostream OS(S2);
        logAllUnhandledErrors(Sym.takeError(), OS);
        /* Keep the error for the name the caller actually asked about,
           which was recorded above. */
    }
    return nullptr;
}

} // namespace

/* Entry points collected into the FilLLVMJITApi table below.  These are
   plain functions with C linkage in this TU; only FilLLVMJIT_GetApi is
   exported dynamically (see filc_llvm_jit_shim.version). */

int shim_GetHeapMemory(void **base, unsigned long long *size) {
    /* Lazy auto-init: first use boots LLVM targets and the JIT heap. */
    if (shim_EnsureInitializedImpl())
        return -1;
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    if (!g_initialized || !g_heapBase) {
        setError("FilLLVMJIT_GetHeapMemory: JIT not initialized");
        return -1;
    }
    if (base)
        *base = g_heapBase;
    if (size)
        *size = g_heapSize;
    return 0;
}

FilLLVMJITRef shim_Create(const char *triple) {
    /* Lazy auto-init: no user-visible Initialize (API v4). */
    if (shim_EnsureInitializedImpl())
        return nullptr;
    auto JTMB = JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        setError(JTMB.takeError());
        return nullptr;
    }
    if (triple && triple[0] != '\0')
        JTMB->getTargetTriple() = Triple(triple);
    LLJITBuilder Builder;
    Builder.setJITTargetMachineBuilder(std::move(*JTMB));
    auto J = Builder.create();
    if (!J) {
        setError(J.takeError());
        return nullptr;
    }
    std::unique_ptr<JITSession> S = std::make_unique<JITSession>();
    S->JIT = std::move(*J);
    /* Pizlonated code calls Fil-C runtime helpers (filc_pollcheck,
       filc_stack_overflow_failure, ...) that live in the host process
       (libpizlo).  Let the main JITDylib fall back to current-process
       symbols for anything the JITed modules do not define.  NOTE: this
       requires libpizlo's symbols to be globally visible (normal linkage
       of the Fil-C executable, or RTLD_GLOBAL dlopen). */
    {
        auto Gen = DynamicLibrarySearchGenerator::GetForCurrentProcess(
            S->JIT->getDataLayout().getGlobalPrefix());
        if (!Gen) {
            setError(Gen.takeError());
            return nullptr;
        }
        S->JIT->getMainJITDylib().addGenerator(std::move(*Gen));
    }
    /* Pre-map the blessed lazy-resolution names to the host helpers above.
       Modules declaring these externs get them bound at AddModule time
       (AddGlobalMapping-style hook): by default to session-carrying wrappers
       (see bindSessionResolvers), or by direct rebind when overridden (e.g.
       the Fil-C runtime shadows them with root-recording implementations at
       session creation). */
    S->Mappings["filc_llvm_jit_resolve"] =
        reinterpret_cast<void *>(FilLLVMJIT_shim_resolveByName);
    S->Mappings["filc_llvm_jit_resolve_raw_pointer"] =
        reinterpret_cast<void *>(FilLLVMJIT_shim_resolveRaw);
    S->Mappings["zexternptr"] =
        reinterpret_cast<void *>(FilLLVMJIT_shim_resolveRaw);
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        g_sessionStates[S.get()] = SessionResolveState{};
    }
    return reinterpret_cast<FilLLVMJITRef>(S.release());
}

int shim_SetOption(FilLLVMJITRef jit, const char *key, const char *value) {
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    return shim_SetOptionImpl(reinterpret_cast<JITSession *>(jit), key, value);
}

void shim_SetResolveCallback(FilLLVMJITRef jit, FilLLVMJITResolveFunction cb,
                             void *ctx) {
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    JITSession *S = reinterpret_cast<JITSession *>(jit);
    if (!S) {
        setError("FilLLVMJIT_SetResolveCallback: null session");
        return;
    }
    SessionResolveState &st = g_sessionStates[S];
    st.Callback = cb;
    st.Context = ctx;
    st.Seq = ++g_resolveSeq;
}

int shim_SetZExactPointerTable(FilLLVMJITRef jit, void *table) {
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    JITSession *S = reinterpret_cast<JITSession *>(jit);
    if (!S) {
        setError("FilLLVMJIT_SetZExactPointerTable: null session");
        return -1;
    }
    SessionResolveState &st = g_sessionStates[S];
    st.ZExactTable = table;
    st.Seq = ++g_resolveSeq;
    return 0;
}

int shim_AddModule(FilLLVMJITRef jit, LLVMModuleRef mod, const char *name,
                   FilLLVMJITModuleRef *out) {
    JITSession *S = reinterpret_cast<JITSession *>(jit);
    if (!S || !mod || !out) {
        setError("FilLLVMJIT_AddModule: null argument");
        return -1;
    }
    std::string modName = (name && name[0] != '\0') ? name : "jit-module";
    /* Serialize the caller's module so the pipeline below works on a copy;
       the original is left unmodified. */
    SmallVector<char, 0> Buf;
    {
        BitcodeWriter Writer(Buf);
        Writer.writeModule(*unwrap(mod));
        Writer.writeStrtab();
    }
    auto Ctx = std::make_unique<LLVMContext>();
    MemoryBufferRef MB(StringRef(Buf.data(), Buf.size()), modName);
    auto M = parseBitcodeFile(MB, *Ctx);
    if (!M) {
        setError(M.takeError());
        return -1;
    }
    Module *MP = M->get();
    Triple JTTriple(S->JIT->getTargetTriple().str());
    FilCLayouts Layouts;
    if (!filCLayoutsFor(JTTriple, Layouts)) {
        setError(std::string("FilLLVMJIT_AddModule: unsupported JIT target ") +
                 S->JIT->getTargetTriple().str());
        return -1;
    }
    MP->setTargetTriple(S->JIT->getTargetTriple().str());
    MP->setDataLayout(Layouts.Before);
    MP->setDataLayoutAfterFilC(Layouts.After);
    /* AddGlobalMapping-style hook: rebind mapped extern declarations to
       their host addresses before running the pipeline.  rebindExternToAddress
       applies the raw-ABI guards itself (raw-ABI helpers/globals are rebound
       to inttoptr constants; pizlonated externs and alias-to-pizlonated
       spellings are skipped for post-pipeline getter/worker stubbing).
       Blessed lazy-resolution names still mapped to this library's default
       helpers are skipped here: they are bound post-pipeline to
       session-carrying wrappers (see bindSessionResolvers), which is what
       keeps concurrently live sessions isolated. */
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        for (const auto &kv : S->Mappings) {
            if (!kv.second)
                continue;
            if (isBlessedExternName(kv.first) &&
                kv.second == defaultBlessedHelper(kv.first))
                continue;
            rebindExternToAddress(*MP, kv.first, kv.second);
        }
    }
    /* TargetMachine driving the Fil-C pass pipeline. */
    JITTargetMachineBuilder JTMB(S->JIT->getTargetTriple());
    auto TM = JTMB.createTargetMachine();
    if (!TM) {
        setError(TM.takeError());
        return -1;
    }
    /* Snapshot the session compile options under the session lock: a
       concurrent SetOption must not race the pipeline's reads. */
    JITOptions OptsCopy;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        OptsCopy = S->Opts;
    }
    runFilCPipeline(*MP, OptsCopy, **TM);
    /* FilPizlonator renames extern declarations it preserves (e.g. an
       extern "foo" it keeps becomes "pizlonated_foo").  Re-apply the
       AddGlobalMapping hook to any still-undefined declarations whose
       unprefixed name has a mapping, so JITed modules stay fully resolved.

       Function declarations that return flights (pizlonated getters) are
       bound by stubbing in a body that returns the mapped address wrapped
       as a flight {addr,addr}, preserving the declaration's type; a raw
       inttoptr rebind would be a type mismatch (call sites invoke the
       getter's FunctionType and expect a flight back).  Anything else keeps
       the direct inttoptr rebind, which is type-correct for finalized IR.

       Finally, bind the pizlonated externs deferred from the pre-pipeline
       hook: stub each one's lazy-extern worker (__filc_jit_get_<name>) to
       return the mapped address as a flight.

       Then bind the blessed lazy-resolution declarations to the owning
       session (session-carrying wrappers), and finally refuse to silently
       drop any mapped name that is still referenced but unbound. */
    uint64_t bindId = 0;
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        bindId = S->NextModuleId++;
    }
    StructType *FlightTy =
        StructType::getTypeByName(MP->getContext(), "filc_flight_ptr");
    {
        std::lock_guard<std::recursive_mutex> lock(g_mu);
        constexpr char Prefix[] = "pizlonated_";
        constexpr size_t PrefixLen = sizeof(Prefix) - 1;
        std::vector<std::pair<std::string, void *>> pending;
        for (Function &F : MP->functions()) {
            if (!F.isDeclaration())
                continue;
            StringRef Name = F.getName();
            if (!Name.starts_with(Prefix))
                continue;
            auto It = S->Mappings.find(Name.substr(PrefixLen).str());
            if (It != S->Mappings.end() && It->second)
                pending.emplace_back(Name.str(), It->second);
        }
        for (GlobalVariable &G : MP->globals()) {
            if (!G.isDeclaration())
                continue;
            StringRef Name = G.getName();
            if (!Name.starts_with(Prefix))
                continue;
            auto It = S->Mappings.find(Name.substr(PrefixLen).str());
            if (It != S->Mappings.end() && It->second)
                pending.emplace_back(Name.str(), It->second);
        }
        for (const auto &kv : pending) {
            if (Function *F = MP->getFunction(kv.first)) {
                if (stubFlightGetterToAddress(F, kv.second, FlightTy))
                    continue;
            }
            rebindExternToAddress(*MP, kv.first, kv.second);
        }
        for (const auto &kv : S->Mappings) {
            if (!kv.second || isRawAbiExternName(kv.first))
                continue;
            stubLazyExternWorker(*MP, kv.first, kv.second);
        }
    }
    /* Session-routed lazy resolution and the
       no-silent-drop rule. */
    if (!bindSessionResolvers(*MP, S, bindId))
        return -1;
    if (!failOnUnboundMappings(*MP, S, modName))
        return -1;
    if (const char *dump = getenv("FILC_LLVMJIT_DUMP")) {
        if (dump[0] != '\0' && dump[0] != '0') {
            errs() << "=== FilLLVMJIT module '" << modName
                   << "' after Fil-C pipeline ===\n"
                   << *MP << "\n";
        }
    }
    /* The pipeline (and especially FilPizlonator) must leave verifiable IR
       behind; check before handing the module to the JIT. */
    {
        std::string V;
        raw_string_ostream OS(V);
        if (verifyModule(*MP, &OS)) {
            setError(std::string("FilLLVMJIT_AddModule: invalid IR after "
                                 "Fil-C pipeline in module ") +
                     modName + ": " + OS.str());
            return -1;
        }
    }
    ThreadSafeModule TSM(std::move(*M), ThreadSafeContext(std::move(Ctx)));
    if (Error Err = S->JIT->addIRModule(std::move(TSM))) {
        setError(std::move(Err));
        return -1;
    }
    uint64_t id = bindId;
    JITModuleRec *rec = new JITModuleRec{S, id, modName};
    *out = reinterpret_cast<FilLLVMJITModuleRef>(rec);
    return 0;
}

int shim_AddGlobalMapping(FilLLVMJITRef jit, const char *name, void *address) {
    JITSession *S = reinterpret_cast<JITSession *>(jit);
    if (!S || !name || name[0] == '\0') {
        setError("FilLLVMJIT_AddGlobalMapping: null argument");
        return -1;
    }
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    if (!address)
        S->Mappings.erase(name);
    else
        S->Mappings[name] = address;
    return 0;
}

void *shim_GetFunctionAddress(FilLLVMJITRef jit, const char *name) {
    return shim_LookupImpl(reinterpret_cast<JITSession *>(jit), name);
}

void *shim_GetGlobalValueAddress(FilLLVMJITRef jit, const char *name) {
    return shim_LookupImpl(reinterpret_cast<JITSession *>(jit), name);
}

void *shim_GetPointerToGlobal(FilLLVMJITRef jit, const char *name) {
    return shim_LookupImpl(reinterpret_cast<JITSession *>(jit), name);
}

/* Thread for the test-only InvokeIntFunction executor.  Inside a Fil-C
   process this is the hosting thread (stack limits, GC state, and origins
   are all real).  Outside one (host-side bring-up proofs) there is no
   filc_get_my_thread symbol; fall back to a private zeroed 1MB page.  The
   only thread word straight-line JITed callees read is the stack-limit word
   at offset 0, which reads 0 and therefore never trips the underflow check,
   and they touch no other thread state.
   Hidden like the resolve helpers: only FilLLVMJIT_GetApi is exported. */
FILC_JIT_HIDDEN void *g_fakeInvokeThread = nullptr;

FILC_JIT_HIDDEN void *currentInvokeThread() {
    std::lock_guard<std::recursive_mutex> lock(g_mu);
    if (void *sym = dlsym(RTLD_DEFAULT, "filc_get_my_thread")) {
        using GetThreadFn = void *(*)();
        return reinterpret_cast<GetThreadFn>(sym)();
    }
    if (!g_fakeInvokeThread) {
        void *page = mmap(nullptr, 1 << 20, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
            return nullptr;
        g_fakeInvokeThread = page;
    }
    return g_fakeInvokeThread;
}

/* Test-only executor behind FilLLVMJIT_InvokeIntFunction (see the header
   docs for scope and limits).  Mechanism: the pipeline renames every
   function `name` to its pizlonated getter `pizlonated_<name>` of type
   (thread, origin) -> flight, so look the getter up (exact or prefixed),
   call it, and run the returned flight's fast entrypoint with the fast CC
   for `int name(void)`: (thread, callee-lower) -> {i1 has_exception, i64
   value} (i32 widens to i64 in the fast CC).  The 16-byte {i1,i64} result
   arrives in RAX:RDX, modeled here as a two-word struct return.  The fast
   entrypoint address is payload word 0
   (see FunctionPayloadTy layout in FilPizlonator). */
int shim_InvokeIntFunction(FilLLVMJITRef jit, const char *name, int *out) {
    JITSession *S = reinterpret_cast<JITSession *>(jit);
    if (!S || !name || name[0] == '\0' || !out) {
        setError("FilLLVMJIT_InvokeIntFunction: null argument");
        return -1;
    }
    void *thread = currentInvokeThread();
    if (!thread) {
        setError("FilLLVMJIT_InvokeIntFunction: no Fil-C thread and "
                 "fake-thread mmap failed");
        return -1;
    }
    void *getterAddr = shim_LookupImpl(S, name);
    if (!getterAddr)
        return -1;
    struct Flight {
        void *ptr;
        void *lower;
    };
    using GetterFn = Flight (*)(void *, void *);
    Flight f = reinterpret_cast<GetterFn>(getterAddr)(thread, nullptr);
    if (!f.lower) {
        setError("FilLLVMJIT_InvokeIntFunction: getter returned null lower "
                 "(unresolved extern?)");
        return -1;
    }
    void *fastEP = nullptr;
    memcpy(&fastEP, f.lower, sizeof fastEP);
    if (!fastEP) {
        setError("FilLLVMJIT_InvokeIntFunction: null fast entrypoint "
                 "(only fast-CC functions are supported)");
        return -1;
    }
    struct FastRet {
        uint64_t lo;
        int64_t hi;
    };
    using FastFn = FastRet (*)(void *, void *);
    FastRet r = reinterpret_cast<FastFn>(fastEP)(thread, f.lower);
    if (r.lo & 1u) {
        setError("FilLLVMJIT_InvokeIntFunction: callee raised an exception");
        return -1;
    }
    *out = static_cast<int>(static_cast<int32_t>(r.hi));
    return 0;
}

const char *shim_GetLastError(void) { return g_lastError.c_str(); }

static const struct FilLLVMJITApi g_api = {
#include "shim_api_init.inc"
};

/* The single dynamically exported symbol of libfilcllvmjit.so.  Default
   visibility is spelled out because the TU compiles with
   -fvisibility=hidden (a version script alone cannot re-export a hidden
   symbol). */
extern "C" __attribute__((visibility("default")))
const struct FilLLVMJITApi *FilLLVMJIT_GetApi(void) {
    return &g_api;
}
