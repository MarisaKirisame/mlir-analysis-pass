cmake_minimum_required(VERSION 3.20)
# C is enabled although no C is compiled here: HandleLLVMOptions probes flags
# with check_c_compiler_flag, and hard-errors if the C compiler is unknown.
project(ZeroAnalysis LANGUAGES C CXX)

#===----------------------------------------------------------------------===#
# Locate MLIR
#
# Every platform puts MLIRConfig.cmake somewhere different -- a hand-built
# LLVM on WSL2, Homebrew's llvm on macOS, and Ubuntu's libmlir-dev all
# disagree.  Rather than hard-code a path, derive it from whichever
# llvm-config is on PATH.  Override with -DMLIR_DIR=/path/to/lib/cmake/mlir.
#===----------------------------------------------------------------------===#
if(NOT MLIR_DIR)
  find_program(LLVM_CONFIG_EXECUTABLE
    NAMES llvm-config
          llvm-config-22 llvm-config-21 llvm-config-20
          llvm-config-19 llvm-config-18 llvm-config-17
    HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin
    DOC "llvm-config used to locate MLIR's CMake package")

  if(LLVM_CONFIG_EXECUTABLE)
    execute_process(
      COMMAND "${LLVM_CONFIG_EXECUTABLE}" --cmakedir
      OUTPUT_VARIABLE llvm_cmakedir
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE llvm_config_status)
    if(llvm_config_status EQUAL 0)
      # .../lib/cmake/llvm  ->  .../lib/cmake/mlir
      get_filename_component(cmake_package_root "${llvm_cmakedir}" DIRECTORY)
      if(EXISTS "${cmake_package_root}/mlir/MLIRConfig.cmake")
        set(MLIR_DIR "${cmake_package_root}/mlir"
            CACHE PATH "Directory containing MLIRConfig.cmake")
      endif()
    endif()
  endif()
endif()

if(NOT MLIR_DIR)
  message(FATAL_ERROR
    "Could not find MLIR.  Put an llvm-config for an MLIR-enabled build on "
    "PATH, or configure with -DMLIR_DIR=<prefix>/lib/cmake/mlir.")
endif()

find_package(MLIR REQUIRED CONFIG)

message(STATUS "Using MLIRConfig.cmake in ${MLIR_DIR}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")
message(STATUS "LLVM/MLIR version ${LLVM_PACKAGE_VERSION}")

#===----------------------------------------------------------------------===#
# Adopt LLVM's build settings
#
# HandleLLVMOptions is not optional here.  Among other things it:
#   * adds "-Wl,-flat_namespace -Wl,-undefined -Wl,dynamic_lookup" to
#     CMAKE_MODULE_LINKER_FLAGS on macOS, without which this plugin cannot
#     link at all (its MLIR symbols are resolved from mlir-opt at load time);
#   * adds -fno-rtti / -fno-exceptions when LLVM was built without them.
#     Getting this wrong produces link errors or silent ODR violations, and
#     the answer differs between distro packages and local builds.
#===----------------------------------------------------------------------===#
list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}" "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_llvm_library(MODULE) quietly declines to create a target when the host
# LLVM cannot load plugins, which turns into a confusing "no such file" much
# later.  Fail here instead, while there is still a useful message to give.
if(NOT LLVM_ENABLE_PLUGINS AND NOT LLVM_EXPORT_SYMBOLS_FOR_PLUGINS)
  message(FATAL_ERROR
    "This LLVM/MLIR build has loadable plugins disabled, so "
    "--load-pass-plugin cannot work.  Rebuild LLVM with "
    "-DLLVM_ENABLE_PLUGINS=ON (the default on Linux and macOS).")
endif()

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${MLIR_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

#===----------------------------------------------------------------------===#
# The plugin
#
# MODULE + PLUGIN_TOOL means: build a loadable module that deliberately leaves
# its LLVM/MLIR symbols undefined, to be satisfied by the mlir-opt process
# that dlopens it.  Linking MLIR's static libraries in here instead would
# duplicate every cl::opt and dialect registry, which fails at load time.
#
# The resulting file is ZeroAnalysis.dylib on macOS and ZeroAnalysis.so on
# Linux and WSL2.  Nothing in this project should spell that suffix out;
# ask CMake via $<TARGET_FILE:ZeroAnalysis>, or let run.sh probe for it.
#===----------------------------------------------------------------------===#
add_llvm_library(SignAnalysis MODULE PARTIAL_SOURCES_INTENDED
  SignAnalysis.cpp
  Annotate.cpp
  Plugin.cpp
  PLUGIN_TOOL mlir-opt
)

set_target_properties(SignAnalysis PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

#===----------------------------------------------------------------------===#
# Tests
#
# Driven by a CMake script rather than a shell script so that the test suite
# has exactly the same dependencies as the build itself.
#===----------------------------------------------------------------------===#
find_program(MLIR_OPT_EXECUTABLE mlir-opt
  HINTS ${MLIR_TOOLS_BINARY_DIR} ${LLVM_TOOLS_BINARY_DIR}
  DOC "mlir-opt used to run the plugin")

if(MLIR_OPT_EXECUTABLE)
  # A plugin built against one LLVM and loaded into another is rejected at
  # run time.  Catching the mismatch now saves a baffling debugging session.
  execute_process(COMMAND "${MLIR_OPT_EXECUTABLE}" --version
    OUTPUT_VARIABLE mlir_opt_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+[A-Za-z0-9.-]*" mlir_opt_version "${mlir_opt_version}")
  if(mlir_opt_version AND NOT mlir_opt_version STREQUAL LLVM_PACKAGE_VERSION)
    message(WARNING
      "mlir-opt at ${MLIR_OPT_EXECUTABLE} reports version ${mlir_opt_version}, "
      "but this plugin is being built against ${LLVM_PACKAGE_VERSION}.  "
      "Loading it will fail; point MLIR_DIR at the matching build.")
  endif()

  enable_testing()
  add_test(NAME zero-analysis
    COMMAND ${CMAKE_COMMAND}
      -DMLIR_OPT=${MLIR_OPT_EXECUTABLE}
      -DPLUGIN=$<TARGET_FILE:ZeroAnalysis>
      -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/test/zero.mlir
      -DCHECKS=${CMAKE_CURRENT_SOURCE_DIR}/test/zero.expected
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunTest.cmake)
else()
  message(STATUS "mlir-opt not found; skipping tests")
endif()
