# Verifies tools/observation/crosscheck.lisp (#32) against its five
# required scenarios (adapter/tests/fixtures/crosscheck/five-scenarios.lisp),
# by actually running canonical my-lisp -- not asserting the expected
# outcome as a comment. #32's acceptance requires: agreement classified
# confirmed, exactly-one-channel-failed classified partial (never
# silently masked), and a real discrepancy beyond tolerance classified
# broken (never fudged into agreement).

if(NOT DEFINED MY_LISP_EXE OR NOT EXISTS "${MY_LISP_EXE}")
  message(FATAL_ERROR "MY_LISP_EXE must name a built my-lisp.exe (run tools/build-my-lisp-runtime.ps1 first)")
endif()

if(NOT DEFINED POLICY_FILE OR NOT EXISTS "${POLICY_FILE}")
  message(FATAL_ERROR "POLICY_FILE must name tools/observation/crosscheck.lisp")
endif()

if(NOT DEFINED FIXTURE OR NOT EXISTS "${FIXTURE}")
  message(FATAL_ERROR "FIXTURE must name adapter/tests/fixtures/crosscheck/five-scenarios.lisp")
endif()

set(driver "${CMAKE_CURRENT_BINARY_DIR}/crosscheck-fixtures-driver.lisp")
file(READ "${POLICY_FILE}" policy_source)
file(READ "${FIXTURE}" fixture_source)
file(WRITE "${driver}" "${policy_source}\n${fixture_source}\n")

execute_process(
  COMMAND "${MY_LISP_EXE}" "${driver}"
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  message(FATAL_ERROR "canonical my-lisp evaluation of the crosscheck fixtures failed (exit=${result}): ${output}")
endif()

string(STRIP "${output}" trimmed_output)
set(expected "(confirmed confirmed partial partial broken)")
if(NOT trimmed_output STREQUAL expected)
  message(FATAL_ERROR "crosscheck classification mismatch: expected '${expected}', got '${trimmed_output}'")
endif()

message(STATUS "observation crosscheck: all 5 required #32 scenarios classified correctly (${trimmed_output})")
