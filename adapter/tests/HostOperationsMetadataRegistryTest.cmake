if(NOT DEFINED GENERATOR)
  message(FATAL_ERROR "GENERATOR is required")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/host-operations-metadata-registry-test")
file(MAKE_DIRECTORY "${work}")
file(WRITE "${work}/missing-owner.lisp" "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk log) (arity 0) (effect mutate) (input none) (result nil) (ffi LogPrimitive) (status live) (evidence vertical-slice))\n)\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/missing-owner.lisp -DOUTPUT=${work}/missing-owner.hpp -P "${GENERATOR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(rc EQUAL 0)
  message(FATAL_ERROR "registry accepted an operation without owner metadata")
endif()
message(STATUS "host operation registry rejects missing mandatory metadata")
