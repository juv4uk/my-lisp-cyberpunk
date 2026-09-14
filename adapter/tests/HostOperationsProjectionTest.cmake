if(NOT DEFINED GENERATOR)
  message(FATAL_ERROR "GENERATOR is required")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/host-operations-projection-test")
file(MAKE_DIRECTORY "${work}")
set(input "${work}/operations.lisp")
set(header "${work}/operations.hpp")
set(markdown "${work}/operations.md")
file(WRITE "${input}" "(host-operations/2\n  (cp:0042 (semantic-id none) (surface uk тест) (arity 1) (effect read) (input game-handle) (result string) (owner cyberpunk-host) (ffi TestPrimitive) (status built) (evidence projection-witness))\n)\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${input} -DOUTPUT=${header} -DMARKDOWN_OUTPUT=${markdown} -P "${GENERATOR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "registry projection failed: ${stderr}")
endif()
if(NOT EXISTS "${markdown}")
  message(FATAL_ERROR "registry did not produce Markdown projection")
endif()
file(READ "${markdown}" rendered)
if(NOT rendered MATCHES "cp:0042" OR NOT rendered MATCHES "cyberpunk-host" OR NOT rendered MATCHES "TestPrimitive")
  message(FATAL_ERROR "Markdown projection lost registry metadata: ${rendered}")
endif()
