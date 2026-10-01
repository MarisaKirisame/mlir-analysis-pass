# MLIR sign analysis

A analysis over integer with 2^3 abstract values - whether an int can be 0, positive, or negative. examples/example.c demo the rules we support.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

That is the whole procedure on Linux, macOS, and WSL2. There is no platform
flag to set and no path to edit. `CMakeLists.txt` finds MLIR by asking
whichever `llvm-config` is on your `PATH` where its CMake package lives, so if
`mlir-opt` runs, the build should configure.

To build against a specific MLIR instead:

```sh
cmake -S . -B build -DMLIR_DIR=/path/to/prefix/lib/cmake/mlir
```

You need an LLVM built with MLIR enabled and plugins enabled
(`-DLLVM_ENABLE_PROJECTS=mlir -DLLVM_ENABLE_PLUGINS=ON`; both are ordinary on
Linux and macOS). Distribution packages work: on Debian and Ubuntu that is
`libmlir-dev` alongside `llvm-dev`. On macOS, Homebrew's `llvm` is the easy
route if it ships `mlir-opt` for your version; otherwise build LLVM yourself.
The configure step diagnoses the cases it can detect — no MLIR
found, plugins disabled in the host LLVM, or an `mlir-opt` on `PATH` whose
version does not match what you are building against.

## Running

```sh
./run.sh input.mlir
```

`run.sh` locates the plugin whatever it is called on your platform and puts the
annotated listing on stdout. Or invoke `mlir-opt` yourself:

```sh
mlir-opt --load-pass-plugin=build/SignAnalysis.so \
         --pass-pipeline='builtin.module(sign-analysis)' \
         input.mlir -o /dev/null
```

using `build/SignAnalysis.dylib` on macOS. The pass leaves the IR unchanged and
writes it to stdout as usual; the annotated view goes to stderr, so the two
streams can be redirected independently. Annotations are comments, so the
annotated listing is still valid MLIR. Values at top or bottom are left
unannotated, so that what prints is exactly what was proved.

Get input in the LLVM dialect from C with:

```sh
clang -O0 -Xclang -disable-O0-optnone -S -emit-llvm -o - examples/example.c |
  opt -passes=mem2reg -S |
  mlir-translate --import-llvm > build/example.mlir
./run.sh build/example.mlir
```

`examples/example.c` uses every transfer rule, with the expected result noted
beside each line. The `mem2reg` step matters: without it every variable stays
in memory and the analysis sees only loads, while at `-O1` and above LLVM folds
the arithmetic away before the analysis sees it. `-disable-O0-optnone` is there
because `optnone` makes `opt` skip the function.

## What is where

Two files hold the analysis; the rest is reusable scaffolding.

| File | |
|---|---|
| `SignDomain.h` | The abstract domain: the lattice elements and their join. |
| `SignAnalysis.cpp` | The transfer functions. |
| `SignAnalysis.h` | Ties the domain to MLIR's sparse forward analysis. |
| `Annotate.{h,cpp}` | Prints IR with a comment on each value. Domain-agnostic. |
| `Plugin.cpp` | The pass, the solver setup, and the `mlir-opt` entry point. |
| `cmake/RunTest.cmake` | The test runner. |

## How the analysis works

`Plugin.cpp` loads three analyses into one solver. `DeadCodeAnalysis` supplies
reachability — without it the solver must assume every branch is taken — and
`SparseConstantPropagation` resolves branch conditions on its behalf. These are
prerequisites for a precise result, not optional extras. `ZeroAnalysis` then
propagates zeroness through operations and block arguments until the solver
reaches a fixed point, which is when the pass queries it.

The transfer function implement constant reading, arithmetic operations, and bitwise operations.

Everything else is unknown. That is always sound, just imprecise.

Values reaching the analysis from outside — function arguments, and results of
any operation without a rule — start at top. The domain's fourth element,
bottom, means "not yet proved reachable"; the solver starts everything there
and raises it as facts arrive, which is what makes the fixed-point iteration
terminate.
