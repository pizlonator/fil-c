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
#include <llvm/ExecutionEngine/JITLink/aarch64.h>
#include <llvm/ExecutionEngine/JITLink/x86_64.h>
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/AllocationActions.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include <llvm/IR/LegacyPassManager.h>
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/RegisterFilPasses.h"
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
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/IR/DataLayout.h>
#include "llvm-c/Core.h"

#include "filc_llvm_jit_api.h"

using namespace llvm;
using namespace llvm::jitlink;

struct ShimJIT {
    std::unordered_map<std::string, std::string> options;
    FilJITConfiguration filConfig;
};

struct ShimExportMapping {
    std::unordered_map<std::string, void*> mapping;
};

namespace {

const shim_runtime_api* runtime;

// This is an always-on assert. The code below strongly expects it to be always on.
#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    runtime->pas_panic("%s:%d: %s: assertion %s failed.\n", \
                       __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
} while (0)

// This is an always-on assert that is used as a safety check - i.e. it's guarding against something
// that an adversarial user might do. The code below strongly expects it to be always on.
#define CHECK(exp) do { \
    if ((exp)) \
        break; \
    runtime->shim_panic("%s:%d: %s: safety assertion %s failed.", \
                        __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
} while (0)

Error makeErr(const Twine &Msg) {
    return make_error<StringError>(Msg, inconvertibleErrorCode());
}

std::unordered_map<std::string, void*> RuntimeMapping;

void initOnce() {
    // TargetRegistry is process-global and registration isn't race-safe.
    static std::once_flag Flag;
    std::call_once(Flag, [] {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
#include "mapping_init.inc"
    });
}

void* AllocateSection(void* my_thread, void* executable_module,
                      uint64_t size64, uint64_t alignment64, bool executable,
                      const std::string& name)
{
    size_t size = (size_t)size64;
    size_t alignment = (size_t)alignment64;
    ASSERT(size == size64);
    ASSERT(alignment == alignment64);
    
    bool isObject = name.starts_with("filc_jit_object_section");
    bool isAux = name.starts_with("filc_jit_aux_section");

    if (isObject) {
        ASSERT(!executable);
        ASSERT(!isAux);
        return runtime->shim_executable_module_allocate_object(
            my_thread, executable_module, size, alignment);
    }

    if (executable) {
        ASSERT(!isObject);
        ASSERT(!isAux);
        ASSERT(alignment <= 16);
        return runtime->shim_executable_module_allocate_executable(
            my_thread, executable_module, size);
    }

    if (isAux) {
        ASSERT(!isObject);
        ASSERT(!executable);
        ASSERT(alignment <= 8);
        return runtime->shim_executable_module_allocate_aux(my_thread, executable_module, size);
    }

    return runtime->shim_executable_module_allocate_data(
        my_thread, executable_module, size, alignment);
}

void* ResolveSymbol(const std::string& name)
{
    auto iter = RuntimeMapping.find(name);
    if (iter == RuntimeMapping.end())
        return nullptr;
    ASSERT(iter->second);
    return iter->second;
}

// JITLink's default GOT/PLT builder (aarch64 and x86_64 both) puts its GOT in
// this section. Its PLT stubs reach their GOT slot with a short PC-relative
// reference (ADRP+LDR, +-4GB / jmp *x(%rip), +-2GB), so the GOT must live in the
// 128MB executable pool alongside the stubs. Everything else non-executable can
// live anywhere: under the large code model, code reaches data via 64-bit
// absolute materialization.
constexpr StringLiteral GOTSectionName = "$__GOT";

// True for sections that get allocated in the 128MB executable pool.
bool isPoolSection(const Section &Sec) {
    return (Sec.getMemProt() & orc::MemProt::Exec) != orc::MemProt::None ||
        Sec.getName() == GOTSectionName;
}

// ---------------------------------------------------------------------------
// Memory manager: one AllocateSection call per section, no ownership retained.
// ---------------------------------------------------------------------------
class HostSectionAllocator final : public JITLinkMemoryManager {
    class InFlight final : public InFlightAlloc {
        LinkGraph &G;

    public:
        InFlight(LinkGraph &G) : G(G) {}

        void finalize(OnFinalizedFunction OnFinalized) override {
            // No ORC platform, so nothing should have registered alloc actions.
            if (!G.allocActions().empty())
                return OnFinalized(makeErr("unexpected JITLink allocation actions"));
            OnFinalized(FinalizedAlloc(orc::ExecutorAddr()));
        }

        void abandon(OnAbandonedFunction OnAbandoned) override {
            // No free hook: sections allocated for a failed link are leaked.
            OnAbandoned(Error::success());
        }
    };

    void* my_thread;
    void* executable_module;

public:
    HostSectionAllocator(void* my_thread, void* executable_module)
        : my_thread(my_thread), executable_module(executable_module) { }
    
    void allocate(const JITLinkDylib *, LinkGraph &G,
                  OnAllocatedFunction OnAllocated) override {
        for (auto &Sec : G.sections()) {
            if (Sec.blocks().empty())
                continue;
            if (Sec.getMemLifetime() == orc::MemLifetime::NoAlloc)
                return OnAllocated(
                    makeErr("unexpected NoAlloc section " + Sec.getName()));

            SmallVector<Block *, 8> Blocks(Sec.blocks().begin(), Sec.blocks().end());
            llvm::sort(Blocks, [](Block *L, Block *R) {
                if (L->getAddress() != R->getAddress())
                    return L->getAddress() < R->getAddress();
                return L->getSize() < R->getSize();
            });
            std::stable_partition(Blocks.begin(), Blocks.end(),
                                  [](Block *B) { return !B->isZeroFill(); });

            uint64_t Size = 0, Align = 1;
            SmallVector<uint64_t, 8> Offsets;
            for (auto *B : Blocks) {
                Size = alignToBlock(Size, *B);
                Offsets.push_back(Size);
                Size += B->getSize();
                Align = std::max<uint64_t>(Align, B->getAlignment());
            }
            Size = std::max<uint64_t>(Size, 1);

            auto *Base = static_cast<char *>(
                AllocateSection(my_thread, executable_module, Size, Align,
                                /*executable=*/isPoolSection(Sec),
                                Sec.getName().str()));
            if (!Base)
                return OnAllocated(
                    makeErr("AllocateSection failed for " + Sec.getName()));

            std::memset(Base, 0, Size);
            for (size_t I = 0; I != Blocks.size(); ++I) {
                Block *B = Blocks[I];
                char *P = Base + Offsets[I];
                B->setAddress(orc::ExecutorAddr::fromPtr(P));
                if (!B->isZeroFill()) {
                    std::memcpy(P, B->getContent().data(), B->getSize());
                    B->setMutableContent({P, static_cast<size_t>(B->getSize())});
                }
            }
        }

        OnAllocated(std::make_unique<InFlight>(G));
    }

    void deallocate(std::vector<FinalizedAlloc> Allocs,
                    OnDeallocatedFunction OnDeallocated) override {
        for (auto &A : Allocs)
            (void)A.release();
        OnDeallocated(Error::success());
    }
};

// ---------------------------------------------------------------------------
// Graph passes.
// ---------------------------------------------------------------------------

// Nothing registers unwind info (no ORC platform), so .eh_frame and LSDAs are
// dead weight -- and they carry 32-bit pcrel fixups into code. Drop them.
Error removeUnwindSections(LinkGraph &G) {
    SmallPtrSet<Section *, 4> Dead;
    for (auto &Sec : G.sections())
        if (Sec.getName() == ".eh_frame" ||
            Sec.getName().starts_with(".gcc_except_table"))
            Dead.insert(&Sec);
    if (Dead.empty())
        return Error::success();

    for (auto *B : G.blocks()) {
        if (Dead.count(&B->getSection()))
            continue;
        for (auto &E : B->edges())
            if (E.getTarget().isDefined() &&
                Dead.count(&E.getTarget().getBlock().getSection()))
                return makeErr("code/data references unwind section " +
                               E.getTarget().getBlock().getSection().getName());
    }
    for (auto *S : Dead)
        G.removeSection(*S);
    return Error::success();
}

// Deterministic check: every relocation that leaves its own section must either
// stay within the 128MB executable pool (code, JITLink's stubs, and its GOT), or
// reach the full 64-bit address space. Otherwise failures would depend on where
// AllocateSection happened to put things.
Error verifyFarReach(LinkGraph &G) {
    auto Arch = G.getTargetTriple().getArch();
    auto FarSafe = [Arch](Edge::Kind K) {
        switch (Arch) {
        case Triple::aarch64:
            return K == aarch64::Pointer64 || K == aarch64::MoveWide16 ||
                K == aarch64::Delta64;
        case Triple::x86_64:
            return K == x86_64::Pointer64 || K == x86_64::Delta64;
        default:
            return false;
        }
    };
    // PC-relative kinds whose range is below 128MB. These must never cross
    // sections, even within the pool.
    auto TooShortForPool = [Arch](Edge::Kind K) {
        return Arch == Triple::aarch64 &&
            (K == aarch64::CondBranch19PCRel || K == aarch64::TestAndBranch14PCRel ||
             K == aarch64::LDRLiteral19 || K == aarch64::ADRLiteral21);
    };

    for (auto *B : G.blocks())
        for (auto &E : B->edges()) {
            if (!E.isRelocation())
                continue;
            auto &T = E.getTarget();
            if (T.isDefined()) {
                const Section &TS = T.getBlock().getSection();
                if (&TS == &B->getSection())
                    continue;
                if (isPoolSection(B->getSection()) && isPoolSection(TS) &&
                    !TooShortForPool(E.getKind()))
                    continue;
            }
            if (FarSafe(E.getKind()))
                continue;
            return makeErr(Twine("edge ") + G.getEdgeKindName(E.getKind()) +
                           " in " + B->getSection().getName() + " to " +
                           (T.hasName() ? (*T.getName()) : StringRef("<anon>")) +
                           " cannot reach an arbitrarily distant target");
        }
    return Error::success();
}

// ---------------------------------------------------------------------------
// Link context.
// ---------------------------------------------------------------------------
struct LinkOutput {
    Error Err = Error::success();
    bool Finalized = false;
    std::unordered_map<std::string, void *> Exports;
};

class LinkContext final : public JITLinkContext {
    JITLinkMemoryManager &MM;
    LinkOutput &Out;

public:
    LinkContext(JITLinkMemoryManager &MM, LinkOutput &Out)
        : JITLinkContext(/*JD=*/nullptr), MM(MM), Out(Out) {}

    JITLinkMemoryManager &getMemoryManager() override { return MM; }

    void notifyFailed(Error E) override {
        Out.Err = joinErrors(std::move(Out.Err), std::move(E));
    }

    void lookup(const LookupMap &Symbols,
                std::unique_ptr<JITLinkAsyncLookupContinuation> LC) override {
        AsyncLookupResult Result;
        for (auto &KV : Symbols) {
            std::string Name = (*KV.first).str();
            if (void *Addr = ResolveSymbol(Name))
                Result[KV.first] = orc::ExecutorSymbolDef(
                    orc::ExecutorAddr::fromPtr(Addr), JITSymbolFlags::Exported);
            else if (KV.second != SymbolLookupFlags::WeaklyReferencedSymbol)
                return LC->run(makeErr("unresolved external symbol: " + Name));
            // Missing weak references are omitted and resolve to null.
        }
        LC->run(std::move(Result));
    }

    Error notifyResolved(LinkGraph &G) override {
        for (auto *Sym : G.defined_symbols())
            if (Sym->hasName() && Sym->getScope() == Scope::Default)
                Out.Exports[(*Sym->getName()).str()] =
                    Sym->getAddress().toPtr<void *>();
        return Error::success();
    }

    void notifyFinalized(JITLinkMemoryManager::FinalizedAlloc A) override {
        (void)A.release();
        Out.Finalized = true;
    }

    LinkGraphPassFunction getMarkLivePass(const Triple &) const override {
        return markAllSymbolsLive;
    }

    Error modifyPassConfig(LinkGraph &G, PassConfiguration &Config) override {
        Config.PrePrunePasses.insert(Config.PrePrunePasses.begin(),
                                     removeUnwindSections);
        // After the default GOT/PLT builder, so it sees the final edges.
        Config.PostPrunePasses.push_back(verifyFarReach);
        return Error::success();
    }
};

// The FilPizlonator pass's HandleDeclaration (FilPizlonator.cpp) asserts that
// every declaration it registers a getter for has an entry in the global
// mapping.  "Unmapped declaration" is not a verifier error, so the pre-check
// below is the only thing standing between the guest and that abort.  It
// mirrors the pass's own skip lists exactly: intrinsics and the pass-through
// functions and global variables are exempt, and the remaining global values
// must be mapped (with a non-null flight pointer) iff they are declarations.
void precheckGlobalMapping(Module &M, FilJITConfiguration &filConfig) {
    auto mapped = [&filConfig](const std::string &Name) {
        auto iter = filConfig.GlobalMapping.find(Name);
        return iter != filConfig.GlobalMapping.end() && iter->second.ptr &&
            iter->second.lower;
    };
    auto passThroughFunction = [](const std::string &Name) {
        return Name == "__divdc3" || Name == "__muldc3" || Name == "__divsc3" ||
            Name == "__mulsc3" || Name == "__multc3" || Name == "__mulxc3" ||
            Name == "setjmp" || Name == "_setjmp" || Name == "sigsetjmp";
    };
    auto passThroughGlobal = [](const std::string &Name) {
        return Name == "llvm.global_ctors" || Name == "llvm.global_dtors" ||
            Name == "llvm.used" || Name == "llvm.compiler.used";
    };
    auto require = [&](bool ok, const Twine &Name) {
        if (ok)
            return;
        runtime->shim_panic("Cannot find %s in global mapping.", Name.str().c_str());
    };
    for (GlobalVariable &G : M.globals()) {
        if (G.isDeclaration() && !passThroughGlobal(G.getName().str()))
            require(mapped(G.getName().str()), G.getName());
    }
    for (Function &F : M.functions()) {
        if (F.isDeclaration() && !F.isIntrinsic() &&
            !passThroughFunction(F.getName().str()))
            require(mapped(F.getName().str()), F.getName());
    }
    for (GlobalAlias &G : M.aliases()) {
        if (G.isDeclaration())
            require(mapped(G.getName().str()), G.getName());
    }
    for (GlobalIFunc &G : M.ifuncs()) {
        if (G.isDeclaration())
            require(mapped(G.getName().str()), G.getName());
    }
}

// ---------------------------------------------------------------------------
// Pipeline.
// ---------------------------------------------------------------------------
void optimize(Module &M, TargetMachine &TM, OptimizationLevel Level, FilJITConfiguration& filConfig) {
    CHECK(!verifyModule(M, &errs()));

    // Reject unmapped declarations before the FilPizlonator pass runs: the
    // pass's HandleDeclaration aborts on them, and "unmapped declaration" is
    // not a verifier error, so this pre-check is the only guard.  It runs
    // after the verifier so that modules with verifier errors keep reporting
    // the verifier's message, exactly as before.
    precheckGlobalMapping(M, filConfig);
    
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;

    PassBuilder PB(&TM);
    TargetLibraryInfoImpl TLII(TM.getTargetTriple());
    FAM.registerPass([&] { return TargetLibraryAnalysis(TLII); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    RegisterFilPasses(PB, &filConfig);

    ModulePassManager MPM = Level == OptimizationLevel::O0
        ? PB.buildO0DefaultPipeline(Level)
        : PB.buildPerModuleDefaultPipeline(Level);
    MPM.run(M, MAM);
}

} // anonymous namespace



ShimJIT* ShimJITCreate() {
    return new ShimJIT;
}

void ShimJITSetOption(ShimJIT* jit, const char* key, const char* value) {
    jit->options[key] = value;
}

void ShimJITSetGlobalMapping(ShimJIT* jit, const char* key, shim_ptr ptr) {
    jit->filConfig.GlobalMapping[key] = ptr;
}

void ShimJITSetPointerMapping(ShimJIT* jit, shim_ptr ptr) {
    jit->filConfig.PointerMapping[ptr.ptr] = ptr.lower;
}

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

ShimExportMapping* ShimJITCompile(
    void* my_thread, void* executable_module, ShimJIT* session, LLVMModuleRef CMod) {
    initOnce();

    OptimizationLevel OL = OptimizationLevel::O2;
    CodeGenOptLevel CGL = CodeGenOptLevel::Default;

    auto iter = session->options.find("opt-level");
    if (iter != session->options.end()) {
        if (iter->second == "0") {
            OL = OptimizationLevel::O0;
            CGL = CodeGenOptLevel::None;
        } else if (iter->second == "1") {
            OL = OptimizationLevel::O1;
            CGL = CodeGenOptLevel::Less;
        } else if (iter->second == "3") {
            OL = OptimizationLevel::O3;
            CGL = CodeGenOptLevel::Aggressive;
        }
    }

    // Host triple/CPU/features, in this process's address space.
    auto JTMB = orc::JITTargetMachineBuilder::detectHost();
    ASSERT(JTMB);
    const Triple &TT = JTMB->getTargetTriple();
    ASSERT(TT.isOSBinFormatELF());
    ASSERT(TT.getArch() == Triple::aarch64 || TT.getArch() == Triple::x86_64);

    JTMB->setCodeModel(CodeModel::Large)
        .setRelocationModel(Reloc::Static)
        .setCodeGenOptLevel(CGL);
    auto TM = JTMB->createTargetMachine();
    ASSERT(TM);

    // Verify (and precheck the global mapping of) the CALLER'S MODULE before
    // cloning it.  The clone of a module that contains an unterminated basic
    // block aborts inside CloneFunctionBodyInto ("dyn_cast on a non-existent
    // value", Casting.h) before the verifier inside optimize() could turn the
    // module state into a Fil-C panic, so the check has to run first.  The
    // verifier runs before the mapping pre-check so that modules with
    // verifier errors keep reporting the verifier's message, exactly as
    // before.  optimize() verifies the clone as well: the clone is verified
    // with the JIT data layout set, which the caller's module does not have.
    CHECK(!verifyModule(*unwrap(CMod), &errs()));
    precheckGlobalMapping(*unwrap(CMod), session->filConfig);

    // Clone, optimize, codegen. The caller's module is never touched.
    SmallVector<char, 0> Obj;
    {
        std::unique_ptr<Module> M = CloneModule(*unwrap(CMod));
        M->setTargetTriple((*TM)->getTargetTriple().str());
        if (TT.getArch() == Triple::x86_64) {
            M->setDataLayout("e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i64:"
                             "64-i128:128-f80:128-n8:16:32:64-S128");
            M->setDataLayoutAfterFilC("e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:"
                                      "64-i128:128-f80:128-n8:16:32:64-S128");
        } else {
            ASSERT(TT.getArch() == Triple::aarch64);
            M->setDataLayout("e-m:e-ni:0-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                             "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32");
            M->setDataLayoutAfterFilC("e-m:e-p270:32:32-p271:32:32-p272:64:64-i8:8:32-"
                                      "i16:16:32-i64:64-i128:128-n32:64-S128-Fn32");
        }

        optimize(*M, **TM, OL, session->filConfig);

        raw_svector_ostream OS(Obj);
        legacy::PassManager CG;
        ASSERT(!(*TM)->addPassesToEmitFile(CG, OS, nullptr,
                                           CodeGenFileType::ObjectFile));
        CG.run(*M);
    } // clone destroyed; nothing below touches the LLVMContext

    // Link. Fully synchronous: returns only after finalize or failure.
    auto SSP = std::make_shared<orc::SymbolStringPool>();
    LinkOutput Out;
    {
        auto G = createLinkGraphFromObject(
            MemoryBufferRef(StringRef(Obj.data(), Obj.size()), "jit-object"), SSP);
        ASSERT(G);
        HostSectionAllocator MemMgr(my_thread, executable_module);
        link(std::move(*G), std::make_unique<LinkContext>(MemMgr, Out));
    } // graph, context, and all SymbolStringPtrs gone before SSP

    if (Out.Err) {
        std::string Msg = toString(std::move(Out.Err));
        errs() << "JIT link failed: " << Msg << "\n";
    }
    ASSERT(!Out.Err);
    ASSERT(Out.Finalized);

    ShimExportMapping* mapping = new ShimExportMapping();
    mapping->mapping = std::move(Out.Exports);

    return mapping;
}

void ShimJITDestroy(ShimJIT* jit) {
    delete jit;
}

void* ShimExportMappingLookup(ShimExportMapping* mapping, const char* name) {
    std::string mangledName = std::string("pizlonated_") + name;
    auto iter = mapping->mapping.find(mangledName);
    if (iter == mapping->mapping.end())
        return nullptr;
    return iter->second;
}

void ShimExportMappingDestroy(ShimExportMapping* mapping) {
    delete mapping;
}

// Subclass predicates used by the runtime's wrapper checks.  These mirror the
// shapes that the LLVM C API's own implementations demand (see Core.cpp): a
// metadata ref may only be an MDNode where MDNode is required, and a value
// may only be a MetadataAsValue with a specific kind of inner metadata.  All
// of them are graceful (dyn_cast, never cast), so they are safe to call on
// any handle that the runtime has already established to be a Value or a
// Metadata.
LLVMBool ShimMetadataIsAMDNode(LLVMMetadataRef MD) {
    return MD && isa<MDNode>(reinterpret_cast<Metadata*>(MD));
}

LLVMBool ShimValueIsMDNodeValue(LLVMValueRef Val) {
    auto* MAV = dyn_cast_or_null<MetadataAsValue>(reinterpret_cast<Value*>(Val));
    return MAV && isa<MDNode>(MAV->getMetadata());
}

LLVMBool ShimValueExtractMDNodeOK(LLVMValueRef Val) {
    auto* MAV = dyn_cast_or_null<MetadataAsValue>(reinterpret_cast<Value*>(Val));
    return MAV && (isa<MDNode>(MAV->getMetadata()) ||
                   isa<ConstantAsMetadata>(MAV->getMetadata()));
}

LLVMBool ShimValueIsMDNodeOrValueAsMetadata(LLVMValueRef Val) {
    auto* MAV = dyn_cast_or_null<MetadataAsValue>(reinterpret_cast<Value*>(Val));
    return MAV && (isa<MDNode>(MAV->getMetadata()) ||
                   isa<ValueAsMetadata>(MAV->getMetadata()));
}

LLVMBool ShimValueIsLocalAsMetadataValue(LLVMValueRef Val) {
    auto* MAV = dyn_cast_or_null<MetadataAsValue>(reinterpret_cast<Value*>(Val));
    return MAV && isa<LocalAsMetadata>(MAV->getMetadata());
}

// Semantic-check predicates: the runtime's cross-argument checks need to ask
// things that the LLVM C API cannot.  All of them are total (they never
// assert), so they are safe on any handle the runtime has already established.

// Whether an attribute kind id is one of the constant-range kinds.  This is
// the same check Attribute::get(Context, Kind, ConstantRange) performs
// (Attributes.cpp), except graceful: an out-of-range kind id is simply not a
// constant-range kind.  Used by LLVMCreateConstantRangeAttribute.
LLVMBool ShimKindIsConstantRangeAttr(unsigned KindID) {
    return Attribute::isConstantRangeAttrKind((Attribute::AttrKind)KindID);
}

// Whether an attribute kind id is one of the type kinds.  This is the same
// check Attribute::get(Context, Kind, Type*) performs (Attributes.cpp,
// "Not a type attribute"), except graceful: an out-of-range kind id is
// simply not a type kind.  Used by LLVMCreateTypeAttribute.

LLVMBool ShimKindIsTypeAttr(unsigned KindID) {
    return Attribute::isTypeAttrKind((Attribute::AttrKind)KindID);
}

// Whether an attribute kind id and value pair is one that
// Attribute::get(Context, Kind, Val) accepts (Attributes.cpp): the kind must
// be an enum or int kind, and for enum kinds the value must be zero.  This
// mirrors the two assertions in Attribute::get except graceful: an
// out-of-range kind id is simply neither an enum nor an int kind.  Used by
// LLVMCreateEnumAttribute.
LLVMBool ShimEnumAttributeArgsOK(unsigned KindID, unsigned long long Val) {
    auto Kind = (Attribute::AttrKind)KindID;
    if (!(Attribute::isEnumAttrKind(Kind) || Attribute::isIntAttrKind(Kind)))
        return false;
    if (Attribute::isEnumAttrKind(Kind) && Val != 0)
        return false;
    return true;
}

// Whether an inline-asm constraint string parses and matches a function type.
// InlineAsm::get cantFails on the same Error that InlineAsm::verify returns
// (InlineAsm.cpp), so verifying first converts that abort into a graceful
// answer.  Used by LLVMGetInlineAsm and LLVMConstInlineAsm.
LLVMBool ShimInlineAsmConstraintsOK(LLVMTypeRef FnTy, const char* Constraints,
                                    size_t Len) {
    auto* FTy = dyn_cast_or_null<FunctionType>(unwrap(FnTy));
    if (!FTy)
        return false;
    Error Err = InlineAsm::verify(
        FTy, StringRef(Constraints ? Constraints : "", Len));
    bool OK = !Err;
    consumeError(std::move(Err));
    return OK;
}

// Whether a struct still accepts a body at all: StructType::setBody asserts
// isOpaque() ("Struct body already set!", Type.cpp), so only an opaque
// (unresolved) struct can take one.  Used by LLVMStructSetBody.
LLVMBool ShimStructIsOpaque(LLVMTypeRef StructTy) {
    auto* ST = dyn_cast_or_null<StructType>(unwrap(StructTy));
    if (!ST)
        return false;
    return ST->isOpaque();
}

// Whether a struct body can be set: it requires an opaque struct (see
// ShimStructIsOpaque) and the same walk that StructType::checkBody performs
// (Type.cpp) before StructType::setBody cantFails on the error: a worklist of
// subtypes, where a hit on the struct being set means recursion.  Used by
// LLVMStructSetBody.
static bool structBodyWouldRecurse(StructType *ST, ArrayRef<Type *> Elems) {
    SmallSetVector<Type *, 8> Worklist(Elems.begin(), Elems.end());
    for (unsigned I = 0; I < Worklist.size(); ++I) {
        Type *Ty = Worklist[I];
        if (Ty == ST)
            return true;
        Worklist.insert(Ty->subtype_begin(), Ty->subtype_end());
    }
    return false;
}

LLVMBool ShimStructSetBodyOK(LLVMTypeRef StructTy, LLVMTypeRef* Elems,
                             unsigned Count) {
    auto* ST = dyn_cast_or_null<StructType>(unwrap(StructTy));
    if (!ST)
        return false;
    if (!ST->isOpaque())
        return false;
    for (unsigned I = 0; I < Count; ++I)
        if (!Elems || !Elems[I])
            return false;
    return !structBodyWouldRecurse(
        ST, ArrayRef<Type *>(unwrap(Elems), Count));
}

// Whether a data layout string parses.  Module::setDataLayout aborts via the
// DataLayout(StringRef) constructor (DataLayout.cpp), which reports the same
// Error that the graceful DataLayout::parse returns (DataLayout.h), so
// parsing first converts that abort into a graceful answer.  Used by
// LLVMSetDataLayout.
LLVMBool ShimDataLayoutSpecOK(const char* Spec) {
    auto Parsed = DataLayout::parse(StringRef(Spec ? Spec : ""));
    bool OK = !!Parsed;
    consumeError(Parsed.takeError());
    return OK;
}

// APInt::fromString's digit acceptance (APInt.cpp): '0'..'radix-1', with
// letters accepted for radix 16 and 36.  Anything else is not a digit.  This
// mirrors APInt.cpp's own getDigit, which is file-local.
static unsigned shimAPIntDigit(char CDigit, uint8_t Radix) {
    unsigned R;
    if (Radix == 16 || Radix == 36) {
        R = CDigit - '0';
        if (R <= 9)
            return R;
        R = CDigit - 'A';
        if (R <= Radix - 11U)
            return R + 10;
        R = CDigit - 'a';
        if (R <= Radix - 11U)
            return R + 10;
        Radix = 10;
    }
    R = CDigit - '0';
    if (R < Radix)
        return R;
    return UINT_MAX;
}

// Whether an integer literal string parses for an integer type and radix.
// ConstantInt::get(IntegerType, StringRef, radix) asserts in
// APInt::fromString (APInt.cpp) on an empty string, a radix other than
// 2/8/10/16/36, a sign with no digits, a digit count the bit width cannot
// hold, and a digit that is invalid for the radix.  Mirroring those
// conditions converts that abort into a graceful answer.  Used by
// LLVMConstIntOfString and LLVMConstIntOfStringAndSize.
LLVMBool ShimAPIntStringOK(LLVMTypeRef IntTy, const char* Text, size_t Len,
                           unsigned Radix) {
    auto* ITy = dyn_cast_or_null<IntegerType>(unwrap(IntTy));
    if (!ITy)
        return false;
    // Core.cpp passes the radix as uint8_t, so a larger radix is truncated
    // before LLVM ever sees it.
    uint8_t R = (uint8_t)Radix;
    StringRef Str(Text ? Text : "", Len);
    if (Str.empty())
        return false;
    if (!(R == 10 || R == 8 || R == 16 || R == 2 || R == 36))
        return false;
    size_t SLen = Str.size();
    if (Str.front() == '-' || Str.front() == '+') {
        Str = Str.substr(1);
        SLen = Str.size();
        if (!SLen)
            return false;
    }
    unsigned NumBits = ITy->getBitWidth();
    if (R == 2 && SLen > NumBits)
        return false;
    if (R == 8 && (SLen - 1) * 3 > NumBits)
        return false;
    if (R == 16 && (SLen - 1) * 4 > NumBits)
        return false;
    if (R == 10 && ((SLen - 1) * 64) / 22 > NumBits)
        return false;
    for (char C : Str)
        if (shimAPIntDigit(C, R) >= R)
            return false;
    return true;
}

// Whether a floating-point literal string parses for a floating-point type.
// ConstantFP::get(Type, StringRef) builds APFloat(Semantics, Str), whose ctor
// asserts on the Error that convertFromString returns (APFloat.cpp).  The
// conversion itself is graceful: it only fails on malformed strings, never on
// representability (rounding and overflow are opStatus values, not errors).
// Used by LLVMConstRealOfString and LLVMConstRealOfStringAndSize.
LLVMBool ShimAPFloatStringOK(LLVMTypeRef RealTy, const char* Text,
                             size_t Len) {
    auto* Ty = dyn_cast_or_null<Type>(unwrap(RealTy));
    if (!Ty || !Ty->isFPOrFPVectorTy())
        return false;
    APFloat F(Ty->getScalarType()->getFltSemantics());
    auto StatusOrErr = F.convertFromString(
        StringRef(Text ? Text : "", Len), APFloat::rmNearestTiesToEven);
    bool OK = !!StatusOrErr;
    consumeError(StatusOrErr.takeError());
    return OK;
}

// Whether ConstantInt::getSExtValue() would get past APInt::getSExtValue()'s
// "Too many bits for int64_t" assertion (APInt.h): the constant's significant
// bits must fit in 64.  The single-word fast path never asserts, and
// isSignedIntN(64) is exactly the asserted condition (getSignificantBits()
// <= 64), so this mirrors the assert gracefully.  Used by
// LLVMConstIntGetSExtValue.
LLVMBool ShimConstIntSExtFits(LLVMValueRef Val) {
    auto* CI = dyn_cast_or_null<ConstantInt>(unwrap(Val));
    if (!CI)
        return false;
    return CI->getValue().isSignedIntN(64);
}

// Whether ConstantInt::getZExtValue() would get past APInt::getZExtValue()'s
// "Too many bits for uint64_t" assertion (APInt.h): the constant's active
// bits must fit in 64.  The single-word fast path never asserts, and
// isIntN(64) is exactly the asserted condition (getActiveBits() <= 64), so
// this mirrors the assert gracefully.  Used by LLVMConstIntGetZExtValue.
LLVMBool ShimConstIntZExtFits(LLVMValueRef Val) {
    auto* CI = dyn_cast_or_null<ConstantInt>(unwrap(Val));
    if (!CI)
        return false;
    return CI->getValue().isIntN(64);
}

// Whether an intrinsic id is one the LLVM C API's own implementations accept:
// LLVMGetIntrinsicDeclaration, LLVMIntrinsicGetType, and
// LLVMIntrinsicIsOverloaded all map the id through llvm_map_to_intrinsic_id,
// which asserts ID < Intrinsic::num_intrinsics ("Intrinsic ID out of range",
// Core.cpp).  This mirrors that check gracefully.  Used by
// LLVMGetIntrinsicDeclaration, LLVMIntrinsicGetType, and
// LLVMIntrinsicIsOverloaded.
LLVMBool ShimIntrinsicIDOK(unsigned ID) {
    return ID < Intrinsic::num_intrinsics;
}

// The intrinsic type-table machinery that Intrinsic::getType runs (and that
// Intrinsic::getOrInsertDeclaration runs on its way to the name mangling)
// walks the intrinsic's IIT descriptors while indexing the caller's Tys array
// and constructing types, and it asserts on bad input in several ways: the
// Argument and VecOfAnyPtrsToElt descriptors index the Tys array through
// ArrayRef::operator[] ("Invalid index!", ArrayRef.h), the argument
// transforms cast<> their inputs to VectorType or IntegerType
// ("cast<Ty>() argument of incompatible type!", Casting.h), the vector
// transforms assert their vector shape and element constraints
// ("Expected an argument of Vector Type", "Cannot halve vector with odd
// number of elements", "Cannot truncate vector element with odd bit-width",
// "Cannot create narrower fp vector element type",
// "Element size must be of a non-zero size", DerivedTypes.h), the vector
// constructors assert their element validity and non-zero count
// ("#Elements of a VectorType must be greater than 0" / "Element type of a
// VectorType must be an integer, floating point, or pointer type",
// Type.cpp), the integer constructor bounds its width ("bitwidth too
// small"/"bitwidth too large", Type.cpp), and the FunctionType constructor
// that finishes the decode asserts isValidReturnType and
// isValidArgumentType ("invalid return type for function" / "Not a valid
// type for function argument!", Type.cpp).  This predicate mirrors that
// decode exactly, replacing every asserting operation with a graceful
// false return: it walks the same IIT descriptor table through the public
// Intrinsic::getIntrinsicInfoTableEntries and fails iff the real decode
// would assert.  Nothing is constructed: only the constraints that the
// real decode's constructors enforce are checked.  Used by
// LLVMGetIntrinsicDeclaration and LLVMIntrinsicGetType (and, for the
// label/token mangling aborts, LLVMIntrinsicCopyOverloadedName2).

// LLVM bounds integer widths at MAX_INT_BITS (Type.cpp: "bitwidth too
// small"/"bitwidth too large").
static constexpr unsigned ShimMaxIntBits = 1 << 23;

namespace {

// A decoded intrinsic type, tracked at the granularity the decode's
// consumers need: vector elements must be scalars the VectorType
// constructors accept, FunctionType return/argument slots must satisfy
// isValidReturnType/isValidArgumentType, and the argument transforms only
// ever read Tys elements directly (as actual Types).
struct ShimIntrinsicTy {
    enum Flavor {
        FixedVoid,
        FixedToken,
        FixedMetadata,
        FixedHalf,
        FixedBFloat,
        FixedFloat,
        FixedDouble,
        FixedQuad,
        FixedPPCQuad,
        FixedInteger,
        FixedPointer,
        FixedVector,
        FixedStruct,
        FixedTargetExt,
        // A type handed in through the Tys array; Ty is the actual type.
        OfTys
    };
    Flavor Flavor;
    Type* Ty;
    unsigned Bits;
};

using ShimIITDescriptor = Intrinsic::IITDescriptor;

// Whether the decoded type would be accepted as a vector element type
// (VectorType::isValidElementType: integer, floating-point, or pointer).
bool shimVectorElementOK(const ShimIntrinsicTy& Ty)
{
    switch (Ty.Flavor) {
    case ShimIntrinsicTy::FixedInteger:
    case ShimIntrinsicTy::FixedHalf:
    case ShimIntrinsicTy::FixedBFloat:
    case ShimIntrinsicTy::FixedFloat:
    case ShimIntrinsicTy::FixedDouble:
    case ShimIntrinsicTy::FixedQuad:
    case ShimIntrinsicTy::FixedPPCQuad:
    case ShimIntrinsicTy::FixedPointer:
        return true;
    case ShimIntrinsicTy::OfTys:
        return Ty.Ty->isIntegerTy() || Ty.Ty->isFloatingPointTy() ||
               Ty.Ty->isPointerTy() ||
               Ty.Ty->getTypeID() == Type::TypedPointerTyID;
    default:
        // Vectors, structs, token, metadata, void, and target extension
        // types are rejected by the VectorType constructor asserts.
        return false;
    }
}

// Whether the decoded type satisfies FunctionType::isValidReturnType (not
// function, label, or metadata).  Freshly constructed flavors never are.
bool shimValidReturnTy(const ShimIntrinsicTy& Ty)
{
    if (Ty.Flavor == ShimIntrinsicTy::OfTys)
        return !Ty.Ty->isFunctionTy() && !Ty.Ty->isLabelTy() &&
               !Ty.Ty->isMetadataTy();
    return true;
}

// Whether the decoded type satisfies FunctionType::isValidArgumentType
// (isFirstClassType: not function and not void).  Freshly constructed
// flavors are all first-class except void (the IIT Void and VarArg
// descriptors decode to void).
bool shimFirstClassTy(const ShimIntrinsicTy& Ty)
{
    if (Ty.Flavor == ShimIntrinsicTy::OfTys)
        return !Ty.Ty->isFunctionTy() && !Ty.Ty->isVoidTy();
    return Ty.Flavor != ShimIntrinsicTy::FixedVoid;
}

// Whether VectorType::getTruncatedElementVectorType would accept the
// element of the given vector: floating-point elements must be double or
// float (anything else hits llvm_unreachable "Cannot create narrower fp
// vector element type"), and everything else truncates through
// IntegerType::get(bits / 2), which bounds the width and rejects a zero
// size (non-single-value elements have getPrimitiveSizeInBits() == 0, and
// IntegerType::get(0) trips "bitwidth too small").
bool shimTruncVectorElementOK(VectorType* VTy)
{
    Type* EltTy = VTy->getElementType();
    if (EltTy->isFloatingPointTy())
        return EltTy->getTypeID() == Type::DoubleTyID ||
               EltTy->getTypeID() == Type::FloatTyID;
    unsigned Bits = EltTy->getPrimitiveSizeInBits();
    return Bits && !(Bits & 1) && Bits / 2 <= ShimMaxIntBits;
}

// The mirror of DecodeFixedType: consumes descriptors exactly the way the
// real decode does and returns false instead of asserting.  Out receives
// the decoded type's flavor.
bool shimDecodeFixedType(ArrayRef<ShimIITDescriptor>& Infos,
                         ArrayRef<Type*> Tys, ShimIntrinsicTy& Out)
{
    if (Infos.empty())
        return false;
    ShimIITDescriptor D = Infos.front();
    Infos = Infos.slice(1);
    switch (D.Kind) {
    case ShimIITDescriptor::Void:
    case ShimIITDescriptor::VarArg:
        Out = {ShimIntrinsicTy::FixedVoid, nullptr, 0};
        return true;
    case ShimIITDescriptor::MMX:
        // FixedVectorType::get(IntegerType::get(64), 1): count 1, valid
        // element; never asserts.
        Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
        return true;
    case ShimIITDescriptor::AMX:
    case ShimIITDescriptor::AArch64Svcount:
        Out = {ShimIntrinsicTy::FixedTargetExt, nullptr, 0};
        return true;
    case ShimIITDescriptor::Token:
        Out = {ShimIntrinsicTy::FixedToken, nullptr, 0};
        return true;
    case ShimIITDescriptor::Metadata:
        Out = {ShimIntrinsicTy::FixedMetadata, nullptr, 0};
        return true;
    case ShimIITDescriptor::Half:
        Out = {ShimIntrinsicTy::FixedHalf, nullptr, 0};
        return true;
    case ShimIITDescriptor::BFloat:
        Out = {ShimIntrinsicTy::FixedBFloat, nullptr, 0};
        return true;
    case ShimIITDescriptor::Float:
        Out = {ShimIntrinsicTy::FixedFloat, nullptr, 0};
        return true;
    case ShimIITDescriptor::Double:
        Out = {ShimIntrinsicTy::FixedDouble, nullptr, 0};
        return true;
    case ShimIITDescriptor::Quad:
        Out = {ShimIntrinsicTy::FixedQuad, nullptr, 0};
        return true;
    case ShimIITDescriptor::PPCQuad:
        Out = {ShimIntrinsicTy::FixedPPCQuad, nullptr, 0};
        return true;
    case ShimIITDescriptor::Integer:
        // IntegerType::get(D.Integer_Width) bounds the width; the table
        // only encodes legal widths.
        Out = {ShimIntrinsicTy::FixedInteger, nullptr, D.Integer_Width};
        return true;
    case ShimIITDescriptor::Vector: {
        ShimIntrinsicTy Elem;
        if (!shimDecodeFixedType(Infos, Tys, Elem))
            return false;
        if (!shimVectorElementOK(Elem))
            return false;
        if (!D.Vector_Width.getKnownMinValue())
            return false;
        Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
        return true;
    }
    case ShimIITDescriptor::Pointer:
        // PointerType::get(Context, D.Pointer_AddressSpace) does not
        // assert for any address space.
        Out = {ShimIntrinsicTy::FixedPointer, nullptr, 0};
        return true;
    case ShimIITDescriptor::Struct: {
        // StructType::get(Context, Elts) accepts any element types.
        for (unsigned I = 0, E = D.Struct_NumElements; I != E; ++I) {
            ShimIntrinsicTy Elem;
            if (!shimDecodeFixedType(Infos, Tys, Elem))
                return false;
        }
        Out = {ShimIntrinsicTy::FixedStruct, nullptr, 0};
        return true;
    }
    case ShimIITDescriptor::Argument:
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        Out = {ShimIntrinsicTy::OfTys, Tys[D.getArgumentNumber()], 0};
        return true;
    case ShimIITDescriptor::ExtendArgument: {
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        Type* Ty = Tys[D.getArgumentNumber()];
        if (auto* VTy = dyn_cast<VectorType>(Ty)) {
            // VectorType::getExtendedElementVectorType asserts the vector
            // is a vector of ints, then widens the element through
            // IntegerType::get(2 * width).
            if (!VTy->isIntOrIntVectorTy())
                return false;
            unsigned Bits =
                cast<IntegerType>(VTy->getElementType())->getBitWidth();
            if (Bits > ShimMaxIntBits / 2)
                return false;
            Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
            return true;
        }
        // IntegerType::get(Context, 2 * cast<IntegerType>(Ty)->getBitWidth()).
        auto* ITy = dyn_cast<IntegerType>(Ty);
        if (!ITy)
            return false;
        unsigned Bits = ITy->getBitWidth();
        if (Bits > ShimMaxIntBits / 2)
            return false;
        Out = {ShimIntrinsicTy::FixedInteger, nullptr, Bits * 2};
        return true;
    }
    case ShimIITDescriptor::TruncArgument: {
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        Type* Ty = Tys[D.getArgumentNumber()];
        if (auto* VTy = dyn_cast<VectorType>(Ty)) {
            if (!shimTruncVectorElementOK(VTy))
                return false;
            Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
            return true;
        }
        auto* ITy = dyn_cast<IntegerType>(Ty);
        if (!ITy)
            return false;
        // assert(ITy->getBitWidth() % 2 == 0), then IntegerType::get(width / 2).
        if (ITy->getBitWidth() % 2 != 0)
            return false;
        Out = {ShimIntrinsicTy::FixedInteger, nullptr, ITy->getBitWidth() / 2};
        return true;
    }
    case ShimIITDescriptor::Subdivide2Argument:
    case ShimIITDescriptor::Subdivide4Argument: {
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        auto* VTy = dyn_cast_or_null<VectorType>(Tys[D.getArgumentNumber()]);
        // assert(VTy && "Expected an argument of Vector Type").
        if (!VTy)
            return false;
        // getSubdividedVectorType doubles the elements
        // (assert(KnownMin * 2 <= UINT_MAX)) and then truncates them.
        if (VTy->getElementCount().getKnownMinValue() >
            UINT_MAX / 2u)
            return false;
        if (!shimTruncVectorElementOK(VTy))
            return false;
        Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
        return true;
    }
    case ShimIITDescriptor::HalfVecArgument: {
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        auto* VTy = dyn_cast_or_null<VectorType>(Tys[D.getArgumentNumber()]);
        // cast<VectorType>(Tys[...]) in getHalfElementsVectorType.
        if (!VTy)
            return false;
        // assert(EltCnt.isKnownEven() && "Cannot halve vector with odd
        // number of elements.").
        if (!VTy->getElementCount().isKnownEven())
            return false;
        Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
        return true;
    }
    case ShimIITDescriptor::SameVecWidthArgument: {
        ShimIntrinsicTy EltTy;
        if (!shimDecodeFixedType(Infos, Tys, EltTy))
            return false;
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        Type* Ty = Tys[D.getArgumentNumber()];
        if (auto* VTy = dyn_cast<VectorType>(Ty)) {
            // VectorType::get(EltTy, VTy->getElementCount()) asserts the
            // element validity and the non-zero count; the count comes
            // from the argument vector and only the element validity
            // needs checking.
            if (!shimVectorElementOK(EltTy))
                return false;
            if (!VTy->getElementCount().getKnownMinValue())
                return false;
            Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
            return true;
        }
        Out = EltTy;
        return true;
    }
    case ShimIITDescriptor::VecElementArgument: {
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        auto* VTy = dyn_cast_or_null<VectorType>(Tys[D.getArgumentNumber()]);
        // dyn_cast then llvm_unreachable("Expected an argument of Vector
        // Type").
        if (!VTy)
            return false;
        Out = {ShimIntrinsicTy::OfTys, VTy->getElementType(), 0};
        return true;
    }
    case ShimIITDescriptor::VecOfBitcastsToInt: {
        if (D.getArgumentNumber() >= Tys.size())
            return false;
        auto* VTy = dyn_cast_or_null<VectorType>(Tys[D.getArgumentNumber()]);
        // assert(VTy && "Expected an argument of Vector Type").
        if (!VTy)
            return false;
        // VectorType::getInteger asserts a non-zero element size and then
        // builds IntegerType::get(bits).
        unsigned Bits = VTy->getElementType()->getPrimitiveSizeInBits();
        if (!Bits || Bits > ShimMaxIntBits)
            return false;
        Out = {ShimIntrinsicTy::FixedVector, nullptr, 0};
        return true;
    }
    case ShimIITDescriptor::VecOfAnyPtrsToElt:
        if (D.getOverloadArgNumber() >= Tys.size())
            return false;
        Out = {ShimIntrinsicTy::OfTys, Tys[D.getOverloadArgNumber()], 0};
        return true;
    }
    return false;
}

} // unnamed namespace

LLVMBool ShimIntrinsicTysOK(unsigned ID, LLVMTypeRef* Tys, size_t Count) {
    if (!ID || ID >= Intrinsic::num_intrinsics)
        return false;
    SmallVector<ShimIITDescriptor, 8> Table;
    Intrinsic::getIntrinsicInfoTableEntries(ID, Table);
    ArrayRef<ShimIITDescriptor> Infos = Table;
    ArrayRef<Type*> TyArr(unwrap(Tys), Count);
    ShimIntrinsicTy ResultTy;
    if (!shimDecodeFixedType(Infos, TyArr, ResultTy))
        return false;
    if (!shimValidReturnTy(ResultTy))
        return false;
    // The remaining descriptors decode the argument types; a trailing void
    // becomes the vararg marker and is popped before FunctionType::get
    // checks the remaining arguments for first-classness.
    SmallVector<ShimIntrinsicTy, 8> ArgTys;
    while (!Infos.empty()) {
        ShimIntrinsicTy ArgTy;
        if (!shimDecodeFixedType(Infos, TyArr, ArgTy))
            return false;
        ArgTys.push_back(ArgTy);
    }
    if (!ArgTys.empty() &&
        (ArgTys.back().Flavor == ShimIntrinsicTy::FixedVoid ||
         (ArgTys.back().Flavor == ShimIntrinsicTy::OfTys &&
          ArgTys.back().Ty->isVoidTy())))
        ArgTys.pop_back();
    for (const ShimIntrinsicTy& ArgTy : ArgTys) {
        if (!shimFirstClassTy(ArgTy))
            return false;
    }
    return true;
}

// Whether an instruction is atomic: LLVMGetAtomicSyncScopeID and
// LLVMSetAtomicSyncScopeID assert I->isAtomic() ("Expected an atomic
// instruction", Core.cpp) after the C API's own Instruction downcast.  This
// mirrors that check gracefully, including for handles that are not
// instructions at all.  Used by LLVMGetAtomicSyncScopeID and
// LLVMSetAtomicSyncScopeID.
LLVMBool ShimInstructionIsAtomic(LLVMValueRef Inst) {
    auto* I = dyn_cast_or_null<Instruction>(unwrap(Inst));
    return I && I->isAtomic();
}

// Reads a shuffle-vector mask into the int mask that both shuffle-vector
// constructors work from, the way ShuffleVectorInst::getShuffleMask
// (Instructions.cpp) does, but gracefully: it fails, rather than asserting,
// when the mask is not a constant vector of integers, when a scalable mask
// is neither zeroinitializer nor undef ("Scalable vector shuffle mask must
// be undef or zeroinitializer"), or when an element cannot be read as a
// ConstantInt (getShuffleMask cast<ConstantInt>s the non-undef elements of a
// non-ConstantDataSequential mask).  PoisonValue is a subclass of UndefValue,
// so poison elements read as PoisonMaskElem exactly as they do in
// getShuffleMask.
static bool shimShuffleMaskToInts(Constant* Mask,
                                  SmallVectorImpl<int>& Result) {
    auto* Ty = dyn_cast<VectorType>(Mask->getType());
    if (!Ty || !Ty->getElementType()->isIntegerTy())
        return false;
    ElementCount EC = Ty->getElementCount();
    if (isa<ConstantAggregateZero>(Mask)) {
        Result.resize(EC.getKnownMinValue(), 0);
        return true;
    }
    if (EC.isScalable()) {
        // getShuffleMask asserts scalable masks are undef or zeroinitializer;
        // zeroinitializer was handled above.
        if (!isa<UndefValue>(Mask))
            return false;
        Result.resize(EC.getKnownMinValue(), PoisonMaskElem);
        return true;
    }
    if (auto* CDS = dyn_cast<ConstantDataSequential>(Mask)) {
        for (unsigned I = 0; I != EC.getKnownMinValue(); ++I)
            Result.push_back((int)CDS->getElementAsInteger(I));
        return true;
    }
    for (unsigned I = 0; I != EC.getKnownMinValue(); ++I) {
        Constant* Elt = Mask->getAggregateElement(I);
        if (!Elt)
            return false;
        if (isa<UndefValue>(Elt)) {
            Result.push_back(PoisonMaskElem);
            continue;
        }
        auto* CI = dyn_cast<ConstantInt>(Elt);
        if (!CI || !CI->getValue().isIntN(64))
            return false;
        // getShuffleMask reads the element through APInt::getZExtValue, which
        // asserts past 64 bits, and stores it in an int.
        Result.push_back((int)CI->getZExtValue());
    }
    return true;
}

// Whether a shuffle-vector mask is one the shuffle-vector constructors
// accept: both IRBuilderBase::CreateShuffleVector (IRBuilder.h) and
// ConstantExpr::getShuffleVector (Constants.cpp) read the mask through
// ShuffleVectorInst::getShuffleMask and then assert
// ShuffleVectorInst::isValidOperands (Instructions.cpp) on the resulting
// ints: every element must be poison or less than twice the source vector's
// element count, and a scalable source needs a uniform mask of all zeros or
// all poison.  This mirrors both steps gracefully; V1's vector-ness and the
// V1/V2 same-type rule are checked by the runtime's own operand checks, but
// they are re-checked here so the predicate stays total.  Used by
// LLVMBuildShuffleVector and LLVMConstShuffleVector.
LLVMBool ShimShuffleMaskOK(LLVMValueRef V1, LLVMValueRef Mask) {
    auto* V1Val = dyn_cast_or_null<Value>(unwrap(V1));
    auto* MaskVal = dyn_cast_or_null<Constant>(unwrap(Mask));
    if (!V1Val || !MaskVal)
        return false;
    auto* V1Ty = dyn_cast<VectorType>(V1Val->getType());
    if (!V1Ty)
        return false;
    SmallVector<int, 16> IntMask;
    if (!shimShuffleMaskToInts(MaskVal, IntMask) || IntMask.empty())
        return false;
    int V1Size = (int)V1Ty->getElementCount().getKnownMinValue();
    for (int Elem : IntMask)
        if (Elem != PoisonMaskElem &&
            Elem >= V1Size * 2)
            return false;
    if (isa<ScalableVectorType>(V1Ty)) {
        if (IntMask[0] != 0 &&
            IntMask[0] != PoisonMaskElem)
            return false;
        for (int Elem : IntMask)
            if (Elem != IntMask[0])
                return false;
    }
    return true;
}

// Whether Constant::getNullValue accepts a type.  Target extension types
// dispatch to ConstantTargetNone::get, which asserts
// Ty->hasProperty(HasZeroInit) ("Target extension type not allowed to have a
// zeroinitializer", Constants.cpp); the property list is LLVM-internal and
// keyed on the type's name (getTargetTypeInfo, Type.cpp).  Types that are not
// target extension types are accepted.  Used by LLVMConstNull.
LLVMBool ShimTargetExtNullOK(LLVMTypeRef Ty) {
    auto* TETy = dyn_cast_or_null<TargetExtType>(unwrap(Ty));
    if (!TETy)
        return true;
    return TETy->hasProperty(TargetExtType::HasZeroInit);
}

static const struct shim_api my_api = {
#include "shim_api_init.inc"
};

// The single dynamically exported symbol of libfilcllvmjit.so.  Default
// visibility is spelled out because the TU compiles with
// -fvisibility=hidden (a version script alone cannot re-export a hidden
// symbol).
extern "C" __attribute__((visibility("default")))
const struct shim_api *FilLLVMJIT_GetApi(const shim_runtime_api* runtime_api,
                                         const char* options)
{
    if (options) {
        BumpPtrAllocator Alloc;
        StringSaver Saver(Alloc);
        SmallVector<const char *, 16> Argv{"llvm"};  // argv[0]; the parser skips it
        cl::TokenizeGNUCommandLine(options, Saver, Argv);
        
        cl::ParseCommandLineOptions(Argv.size(), Argv.data(),
                                    /*Overview=*/"", /*Errs=*/nullptr);
    }
    
    runtime = runtime_api;
    ASSERT(runtime->version == SHIM_API_VERSION);
    return &my_api;
}

extern "C" __attribute__((visibility("hidden")))
int atexit(void (*f)(void))
{
    return 0;
}

extern "C" __attribute__((visibility("hidden")))
int __cxa_atexit(void (*f)(void *), void *arg, void *dso)
{
    return 0;
}
