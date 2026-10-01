//===- Plugin.cpp - Pass definition and plugin entry point ----------------===//
//
// Scaffolding: wires the analysis into a pass and exposes it to mlir-opt.
//
//===----------------------------------------------------------------------===//

#include "Annotate.h"
#include "SignAnalysis.h"

#include "mlir/Analysis/DataFlow/ConstantPropagationAnalysis.h"
#include "mlir/Analysis/DataFlow/DeadCodeAnalysis.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"

using namespace mlir;

namespace sign {

struct SignAnalysisPass
    : PassWrapper<SignAnalysisPass, OperationPass<ModuleOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(SignAnalysisPass)

  StringRef getArgument() const final { return "sign-analysis"; }

  StringRef getDescription() const final {
    return "Determine sign";
  }

  void runOnOperation() override {
    DataFlowConfig config;
    config.setInterprocedural(false);

    DataFlowSolver solver(config);
    // DeadCodeAnalysis supplies reachability, without which the solver must
    // assume every branch is taken; SparseConstantPropagation resolves branch
    // conditions for it.  Both are prerequisites, not extras.
    solver.load<dataflow::DeadCodeAnalysis>();
    solver.load<dataflow::SparseConstantPropagation>();
    solver.load<sign::SignAnalysis>();

    if (failed(solver.initializeAndRun(getOperation()))) {
      getOperation().emitError("sign analysis failed to reach a fixed point");
      return signalPassFailure();
    }

    // Query states only now that the solver has converged.
    auto describe = [&](Value value, AsmState &asmState) -> std::string {
      const auto *lattice = solver.lookupState<sign::SignLattice>(value);
      if (!lattice)
        return {};
      auto kind = lattice->getValue();
      // Top and bottom say nothing; printing them would bury the real facts.
      if (kind.isBottom() || kind.isTop())
        return {};
      std::string description;
      llvm::raw_string_ostream os(description);
      value.printAsOperand(os, asmState);
      os << " is " << kind;
      return description;
    };

    // stderr, so that mlir-opt's stdout stays the unmodified IR and the two can
    // be redirected independently.
    sign::printAnnotated(getOperation(), describe, llvm::errs());

    // This pass only reads.
    markAllAnalysesPreserved();
  }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo mlirGetPassPluginInfo() {
  // LLVM_VERSION_STRING is baked in at compile time and checked by mlir-opt at
  // load time, which is what turns an ABI mismatch into a clear diagnostic.
  return {MLIR_PLUGIN_API_VERSION, "SignAnalysis", LLVM_VERSION_STRING,
          []() { PassRegistration<sign::SignAnalysisPass>(); }};
}
