//===- Annotate.cpp -------------------------------------------------------===//

#include "Annotate.h"

#include "mlir/IR/AsmState.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"

using namespace mlir;

namespace sign {

void printAnnotated(
    Operation *root,
    llvm::function_ref<std::string(Value, AsmState &)> describe,
    llvm::raw_ostream &os) {
  // Print once with a location map, so annotations can be keyed to the lines
  // the printer actually produced -- including its choice of SSA names,
  // aliases, and nesting.  Recomputing those by hand would drift.
  AsmState::LocationMap locations;
  AsmState asmState(root, OpPrintingFlags(), &locations);

  std::string assembly;
  llvm::raw_string_ostream assemblyStream(assembly);
  root->print(assemblyStream, asmState);

  struct LineAnnotations {
    llvm::SmallVector<std::string> arguments;
    llvm::SmallVector<std::string> results;
    unsigned indent = 0;
  };
  llvm::DenseMap<unsigned, LineAnnotations> annotations;

  root->walk([&](Operation *op) {
    auto position = locations.find(op);
    if (position == locations.end())
      return;
    auto [line, column] = position->second;

    for (Value result : op->getResults()) {
      std::string fact = describe(result, asmState);
      if (!fact.empty())
        annotations[line].results.push_back(std::move(fact));
    }

    for (Region &region : op->getRegions()) {
      for (Block &block : region) {
        if (block.getNumArguments() == 0)
          continue;
        // A block's first operation is a stable insertion point just after its
        // label, or after the enclosing function's signature for entry blocks.
        auto anchor = block.empty() ? position : locations.find(&block.front());
        if (anchor == locations.end())
          continue;
        auto [argumentLine, argumentColumn] = anchor->second;
        for (BlockArgument argument : block.getArguments()) {
          std::string fact = describe(argument, asmState);
          if (fact.empty())
            continue;
          auto &entry = annotations[argumentLine];
          entry.indent = argumentColumn;
          entry.arguments.push_back(std::move(fact));
        }
      }
    }
  });

  // Emit the listing verbatim, interleaving comments.  Splitting on '\n' alone
  // is deliberate: the printer always emits LF, whatever the host platform.
  StringRef remaining(assembly);
  for (unsigned lineNumber = 1; !remaining.empty(); ++lineNumber) {
    auto [line, rest] = remaining.split('\n');
    auto entry = annotations.find(lineNumber);
    if (entry != annotations.end()) {
      for (const std::string &argument : entry->second.arguments)
        os.indent(entry->second.indent) << "// argument: " << argument << "\n";
    }
    os << line;
    if (entry != annotations.end() && !entry->second.results.empty()) {
      os << " // ";
      llvm::interleave(entry->second.results, os, "; ");
    }
    os << "\n";
    remaining = rest;
  }
}

} // namespace zero
