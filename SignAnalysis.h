//===- SignAnalysis.h - Sparse forward analysis over SignState ------------===//

#ifndef SIGN_ANALYSIS_H
#define SIGN_ANALYSIS_H

#include "SignDomain.h"
#include "mlir/Analysis/DataFlow/SparseAnalysis.h"

namespace sign {

using SignLattice = mlir::dataflow::Lattice<SignState>;

class SignAnalysis
    : public mlir::dataflow::SparseForwardDataFlowAnalysis<SignLattice> {
public:
  using SparseForwardDataFlowAnalysis::SparseForwardDataFlowAnalysis;

  /// Transfer function: given the states of `op`'s operands, set the states of
  /// its results.  Must be monotone in the operand states.
  mlir::LogicalResult
  visitOperation(mlir::Operation *op,
                 llvm::ArrayRef<const SignLattice *> operands,
                 llvm::ArrayRef<SignLattice *> results) override;

  /// The state of anything entering the analysis from outside: function
  /// arguments, and results the transfer function declines to reason about.
  void setToEntryState(SignLattice *lattice) override;
};

} // namespace sign

#endif
