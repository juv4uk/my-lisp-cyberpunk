if(NOT DEFINED GUARD)
  message(FATAL_ERROR "GUARD is required")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/canon-spelling-guard-test")
file(MAKE_DIRECTORY "${work}")

file(WRITE "${work}/generated.rs" "if QUOTE_SPELLINGS.contains(&name) { return evaluate(); }\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/generated.rs -P "${GUARD}"
  RESULT_VARIABLE generated_rc
)
if(NOT generated_rc EQUAL 0)
  message(FATAL_ERROR "generated Canon projection was rejected")
endif()

file(WRITE "${work}/hardcoded.rs" "if name == \"quote\" { return evaluate(); }\n")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${work}/hardcoded.rs -P "${GUARD}"
  RESULT_VARIABLE hardcoded_rc
)
if(hardcoded_rc EQUAL 0)
  message(FATAL_ERROR "hardcoded Canon spelling was accepted")
endif()

message(STATUS "Canon spelling guard rejects direct surface dispatch")
