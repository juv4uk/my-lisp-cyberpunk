# my-lisp-cyberpunk RED4ext host adapter.
#
# The adapter loads the host-neutral WSM runtime DLL into the Cyberpunk 2077
# process and initializes one Lisp session. It deliberately contains no Lisp
# reader/evaluator implementation: that lives in ../host-runtime (migrated
# from wsm-my-lisp/dll, wsm-my-lisp#15 Phase C — see host-runtime/MIGRATION.md).
cmake_minimum_required(VERSION 3.21)

project(
  my-lisp-cyberpunk-plugin
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(HOST_OPERATIONS_REGISTRY "${CMAKE_CURRENT_SOURCE_DIR}/host-operations.lisp")
set(HOST_OPERATIONS_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/host_operations.generated.hpp")
set(HOST_BINDINGS_REGISTRY "${CMAKE_CURRENT_SOURCE_DIR}/host-bindings.lisp")
set(HOST_BINDINGS_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/host_bindings.generated.hpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HOST_OPERATIONS_REGISTRY}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HOST_BINDINGS_REGISTRY}")
execute_process(COMMAND ${CMAKE_COMMAND} -DINPUT=${HOST_OPERATIONS_REGISTRY} -DOUTPUT=${HOST_OPERATIONS_HEADER} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateHostOperations.cmake" RESULT_VARIABLE HOST_OPERATIONS_RESULT)
if(NOT HOST_OPERATIONS_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not generate host operation projection")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -DINPUT=${HOST_BINDINGS_REGISTRY} -DOUTPUT=${HOST_BINDINGS_HEADER} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateHostBindings.cmake" RESULT_VARIABLE HOST_BINDINGS_RESULT)
if(NOT HOST_BINDINGS_RESULT EQUAL 0)
  message(FATAL_ERROR "Could not generate host binding projection")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/red4ext.sdk/CMakeLists.txt")
  message(FATAL_ERROR
    "deps/red4ext.sdk is empty -- run: git submodule update --init plugin/deps/red4ext.sdk"
  )
endif()

set(RED4EXT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/red4ext.sdk)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME host-operations-registry-mutation COMMAND ${CMAKE_COMMAND} -DGENERATOR=${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateHostOperations.cmake -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/HostOperationsRegistryTest.cmake)
  add_test(NAME host-bindings-registry-mutation COMMAND ${CMAKE_COMMAND} -DGENERATOR=${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateHostBindings.cmake -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/HostBindingsRegistryTest.cmake)
  add_test(NAME canon-spelling-guard COMMAND ${CMAKE_COMMAND} -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/../host-runtime/src/eval.rs -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCanonSpellings.cmake)
  add_test(NAME canon-spelling-guard-mutation COMMAND ${CMAKE_COMMAND} -DGUARD=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCanonSpellings.cmake -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/CanonSpellingGuardTest.cmake)
endif()

add_subdirectory(src)
