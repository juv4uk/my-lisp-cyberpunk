# my-lisp-cyberpunk RED4ext host adapter.
#
# The adapter loads the host-neutral WSM runtime DLL into the Cyberpunk 2077
# process and initializes one Lisp session. It deliberately contains no Lisp
# reader/evaluator implementation: that belongs to wsm-my-lisp.
cmake_minimum_required(VERSION 3.21)

project(
  my-lisp-cyberpunk-plugin
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/red4ext.sdk/CMakeLists.txt")
  message(FATAL_ERROR
    "deps/red4ext.sdk is empty -- run: git submodule update --init plugin/deps/red4ext.sdk"
  )
endif()

set(RED4EXT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/red4ext.sdk)

add_subdirectory(src)
