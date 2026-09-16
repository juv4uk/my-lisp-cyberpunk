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

add_library(version SHARED src/BridgeDllMain.cpp)
set_target_properties(version PROPERTIES
  OUTPUT_NAME "version"
  PREFIX ""
)
target_link_options(version PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/version-proxy.def")

# Standalone, non-injected harness: proves LoadLibrary + export
# forwarding + background thread + file write work before this DLL
# ever goes near the actual game process.
add_executable(bridge-load-test tests/LoadTest.cpp)
