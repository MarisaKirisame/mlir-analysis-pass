//===- SignAnalysis.cpp - Transfer functions ------------------------------===//
//
// The transfer function: given what is known about an operation's operands,
// state what is known about its results.  This file and SignDomain.h are the
// two to replace when building a different analysis; the rest of the project
// is scaffolding.
//
//===----------------------------------------------------------------------===//

#include "SignAnalysis.h"

#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/IR/Matchers.h"

using namespace mlir;

namespace sign {

void SignAnalysis::setToEntryState(SignLattice *lattice) {
  propagateIfChanged(lattice, lattice->join(SignState::top()));
}

LogicalResult
SignAnalysis::visitOperation(Operation *op,
                             ArrayRef<const SignLattice *> operands,
                             ArrayRef<SignLattice *> results) {
  // Raising a result to top says "this operation could produce anything",
  // which is always a sound answer and is what every unhandled case does.
  auto unknown = [&] {
    setAllToEntryStates(results);
    return success();
  };

  auto ov = dyn_cast<LLVM::IntegerOverflowFlagsInterface>(op);
  bool nsw = ov && ov.hasNoSignedWrap();

  // Only single-result integer operations are interesting here.  Calls, loads,
  // floats, and vectors all land in `unknown`.
  if (op->getNumResults() != 1 || !op->getResult(0).getType().isIntOrIndex())
    return unknown();
  SignLattice *result = results[0];

  // Rule 1: a constant is zero or nonzero according to what it says.
  // This is the only rule that does not consult its operands, and without some
  // rule of this kind the analysis would have no facts to propagate at all.
  IntegerAttr value;
  auto fromValue = [&](const APInt &x) {
    if (x.isNegative()) {
      return (Sign::Negative);
    }
    else if (x.isStrictlyPositive()) {
      return (Sign::Positive);
    }
    else {
      return (Sign::Zero);
    }
  };
  if (matchPattern(op, m_Constant(&value))) {
    auto x = value.getValue();
    propagateIfChanged(result, result->join(SignState(fromValue(x))));
    return success();
  }

  // Rule 2: `x & y` is zero if either operand is zero, since a zero operand
  // clears every bit.  Note what this rule does *not* say: two nonzero
  // operands tell us nothing, because 1 & 2 is 0.
  if (isa<LLVM::AndOp>(op)) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (lhs == Sign::Zero || rhs == Sign::Zero) {
        return SignState(Sign::Zero);
      } else if (lhs == Sign::Negative && rhs == Sign::Negative) {
        return SignState(Sign::Negative);
      } else {
        return SignState({Sign::Positive, Sign::Zero});
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }

  if (isa<LLVM::OrOp>(op)) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (lhs == Sign::Zero) {
        return SignState(rhs);
      } else if (rhs == Sign::Zero) {
        return SignState(lhs);
      } else if (lhs == Sign::Negative || rhs == Sign::Negative) {
        return SignState(Sign::Negative);
      } else {
        return SignState({Sign::Positive});
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }

  if (isa<LLVM::XOrOp>(op)) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (lhs == Sign::Zero) {
        return SignState(rhs);
      } else if (rhs == Sign::Zero) {
        return SignState(lhs);
      } else if (lhs == rhs) {
        return SignState({Sign::Positive, Sign::Zero});
      } else {
        return SignState(Sign::Negative);
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }

  if (isa<LLVM::AddOp>(op) && nsw) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (lhs == Sign::Zero) {
        return SignState(rhs);
      } else if (rhs == Sign::Zero) {
        return SignState(lhs);
      } else if (lhs == Sign::Positive && rhs == Sign::Positive) {
        return SignState(Sign::Positive);
      } else if (lhs == Sign::Negative && rhs == Sign::Negative) {
        return SignState(Sign::Negative);
      } else {
        return SignState::top();
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }

  if (isa<LLVM::SubOp>(op) && nsw) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (rhs == Sign::Zero) {
        return SignState(lhs);
      } else if (lhs == Sign::Zero) {
        if (rhs == Sign::Positive) {
          return SignState(Sign::Negative);
        } else {
          return SignState(Sign::Positive);
        }
      }
      else if (lhs == Sign::Positive && rhs == Sign::Negative) {
        return SignState(Sign::Positive);
      } else if (lhs == Sign::Negative && rhs == Sign::Positive) {
        return SignState(Sign::Negative);
      } else {
        return SignState::top();
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }

  if (isa<LLVM::MulOp>(op) && nsw) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (lhs == Sign::Zero || rhs == Sign::Zero) {
        return SignState(Sign::Zero);
      } else if (lhs == Sign::Positive && rhs == Sign::Positive) {
        return SignState(Sign::Positive);
      } else if (lhs == Sign::Negative && rhs == Sign::Negative) {
        return SignState(Sign::Positive);
      } else if (lhs == Sign::Positive && rhs == Sign::Negative) {
        return SignState(Sign::Negative);
      } else if (lhs == Sign::Negative && rhs == Sign::Positive) {
        return SignState(Sign::Negative);
      } else {
        return SignState::top();
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }
  if (isa<LLVM::SDivOp>(op)) {
    SignState lhs = operands[0]->getValue();
    SignState rhs = operands[1]->getValue();

    auto tf = [](Sign lhs, Sign rhs){
      if (rhs == Sign::Zero) {
        return SignState::bottom();
      } else if (lhs == Sign::Zero) {
        return SignState(Sign::Zero);
      } else if (lhs == Sign::Positive && rhs == Sign::Positive) {
        return SignState({Sign::Positive, Sign::Zero});
      } else if (lhs == Sign::Negative && rhs == Sign::Negative) {
        return SignState({Sign::Positive, Sign::Zero});
      } else if (lhs == Sign::Positive && rhs == Sign::Negative) {
        return SignState({Sign::Negative, Sign::Zero});
      } else if (lhs == Sign::Negative && rhs == Sign::Positive) {
        return SignState({Sign::Negative, Sign::Zero});
      } else {
        return SignState::top();
      }
    };
    propagateIfChanged(result, result->join(
      SignState::transfer(lhs, rhs, tf)));
    return success();
  }

  return unknown();
}

} // namespace sign
