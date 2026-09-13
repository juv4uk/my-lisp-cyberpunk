if(NOT DEFINED GENERATOR)
  message(FATAL_ERROR "GENERATOR is required")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/host-bindings-registry-test")
file(MAKE_DIRECTORY "${work}")
file(WRITE "${work}/unknown.wsm" "(host-bindings/1\n  (cpb:0001 (surface uk player) (kind game-handle) (owner cyberpunk))\n  (cpb:ABCD (surface uk broken) (kind game-handle) (owner cyberpunk))\n)\n")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/unknown.wsm -DOUTPUT=${work}/unknown.hpp -P "${GENERATOR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE stdout
  ERROR_VARIABLE stderr
)

if(rc EQUAL 0)
  message(FATAL_ERROR "unknown cpb: ID was accepted; expected fail-closed rejection")
endif()

message(STATUS "host binding registry rejects unknown cpb: IDs")
