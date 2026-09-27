# CP-PROBE-INPROCESS-V0 (#31): the minimal in-process bridge.
#
# Deliberately its own top-level CMake project, separate from
# adapter/ -- adapter/ is the old RED4ext-era plugin
# (docs/owner-decision-2026-09-16-pause-and-revert-to-vanilla.md);
# this is a from-scratch proxy DLL with no RED4ext/RED4ext.SDK
# dependency at all, per #19's boundary.
cmake_minimum_required(VERSION 3.21)

project(
  my-lisp-cyberpunk-bridge
  VERSION 0.0.1
  LANGUAGES CXX
)

if(NOT WIN32)
  message(FATAL_ERROR "bridge/ targets Windows only (DllMain, version.dll proxy)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The canonical runtime is built from the exact pinned runtime/my-lisp gitlink.
# Cargo/rustc owns both the static artifact path and its native linker inputs;
# this bridge consumes those facts rather than maintaining a copied list.
set(MY_LISP_EMBED_STATIC "" CACHE FILEPATH "Cargo-produced canonical my-lisp-embed static library")
if(NOT EXISTS "${MY_LISP_EMBED_STATIC}")
  message(FATAL_ERROR "MY_LISP_EMBED_STATIC must point at the pinned canonical static library")
endif()

set(MY_LISP_NATIVE_LIBS "" CACHE STRING "Rust-reported native linker inputs required by my-lisp-embed")
if(MY_LISP_NATIVE_LIBS STREQUAL "")
  message(FATAL_ERROR "MY_LISP_NATIVE_LIBS must come from rustc --print native-static-libs")
endif()

set(MY_LISP_SHA "" CACHE STRING "Exact 40-hex runtime/my-lisp gitlink revision")
string(LENGTH "${MY_LISP_SHA}" MY_LISP_SHA_LENGTH)
if(NOT MY_LISP_SHA_LENGTH EQUAL 40 OR NOT MY_LISP_SHA MATCHES "^[0-9a-fA-F]+$")
  message(FATAL_ERROR "MY_LISP_SHA must be the exact pinned runtime/my-lisp commit")
endif()

set(MY_LISP_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${MY_LISP_GENERATED_DIR}")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/MyLispBuildProvenance.hpp.in"
  "${MY_LISP_GENERATED_DIR}/MyLispBuildProvenance.hpp"
  @ONLY
)

# Reuse the repo's already-proven mechanism-only loopback transport from the
# historical adapter.  The bridge does NOT reuse the RED4ext adapter itself:
# only LocalReplServer/RequestQueue, which contain Winsock + queue mechanics
# and no evaluator/game-framework dependency.
add_library(
  version
  SHARED
    src/BridgeDllMain.cpp
    src/BridgeRuntime.cpp
    src/CanonicalReplHost.cpp
    ../adapter/src/LocalReplServer.cpp
)
set_target_properties(version PROPERTIES
  OUTPUT_NAME "version"
  PREFIX ""
)
target_include_directories(
  version
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../adapter/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../runtime/my-lisp/crates/my-lisp-embed/include"
    "${MY_LISP_GENERATED_DIR}"
)
target_link_libraries(version PRIVATE "${MY_LISP_EMBED_STATIC}" ws2_32)

# Preserve rustc's distinction between ordinary native libraries and raw MSVC
# linker options (for example /defaultlib:msvcrt), exactly as the upstream
# my-lisp#316 static consumer witness does.
foreach(native_input IN LISTS MY_LISP_NATIVE_LIBS)
  if(native_input MATCHES "^/")
    target_link_options(version PRIVATE "${native_input}")
  else()
    target_link_libraries(version PRIVATE "${native_input}")
  endif()
endforeach()

target_link_options(version PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/version-proxy.def")

# Standalone, non-injected harness: proves LoadLibrary + export
# forwarding and now the canonical in-process REPL contract before this DLL
# ever goes near the actual game process.
add_executable(bridge-load-test tests/LoadTest.cpp)
target_link_libraries(bridge-load-test PRIVATE ws2_32)
