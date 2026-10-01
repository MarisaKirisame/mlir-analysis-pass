//===- Annotate.h - Print IR with per-value annotations -------------------===//
//
// Scaffolding, reusable by any analysis: print `root` as ordinary MLIR with a
// comment attached to each value that `describe` has something to say about.
// The output stays parseable MLIR, so it can be diffed, or fed back through
// mlir-opt after stripping comments.
//
//===----------------------------------------------------------------------===//

#ifndef ZERO_ANNOTATE_H
#define ZERO_ANNOTATE_H

#include "mlir/IR/Operation.h"
#include "llvm/ADT/STLFunctionalExtras.h"
#include "llvm/Support/raw_ostream.h"

namespace sign {

/// `describe` returns the annotation for a value, or an empty string to leave
/// that value unannotated.  It is passed an AsmState so it can print SSA names
/// that match the listing.
void printAnnotated(
    mlir::Operation *root,
    llvm::function_ref<std::string(mlir::Value, mlir::AsmState &)> describe,
    llvm::raw_ostream &os);

} // namespace zero

#endif
