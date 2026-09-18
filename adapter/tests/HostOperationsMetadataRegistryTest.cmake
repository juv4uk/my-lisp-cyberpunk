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

file(WRITE "${work}/invalid-effect.lisp" "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk log) (arity 0) (effect execute-anything) (input none) (result nil) (owner cyberpunk-host) (ffi LogPrimitive) (status live) (evidence vertical-slice))\n)\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/invalid-effect.lisp -DOUTPUT=${work}/invalid-effect.hpp -P "${GENERATOR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(rc EQUAL 0)
  message(FATAL_ERROR "registry accepted an unknown effect")
endif()
file(WRITE "${work}/invalid-input-kind.lisp" "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk log) (arity 1) (effect read) (input raw-pointer) (result string) (owner cyberpunk-host) (ffi LogPrimitive) (status built) (evidence kind-mutation))\n)\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/invalid-input-kind.lisp -DOUTPUT=${work}/invalid-input-kind.hpp -P "${GENERATOR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(rc EQUAL 0)
  message(FATAL_ERROR "registry accepted raw-pointer as a host operation input kind")
endif()

file(WRITE "${work}/invalid-result-kind.lisp" "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk log) (arity 0) (effect inspect) (input none) (result float) (owner cyberpunk-host) (ffi LogPrimitive) (status built) (evidence kind-mutation))\n)\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/invalid-result-kind.lisp -DOUTPUT=${work}/invalid-result-kind.hpp -P "${GENERATOR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(rc EQUAL 0)
  message(FATAL_ERROR "registry accepted float as a host operation result kind")
endif()

message(STATUS "host operation registry rejects missing mandatory metadata")
