# Verifies the game-observation/1 fixtures (docs/observation-contract-v0.md,
# issue #28) against three acceptance points:
#   1. Every fixture parses under the canonical my-lisp reader as plain data,
#      with no new evaluator and no new reader syntax (oracle-check is
#      canonical my-lisp's own reference parse-only tool).
#   2. The MEMORY and SCREEN valid fixtures name the same `fact` (so #32's
#      cross-channel comparison has something to compare) while declaring
#      different `source`s.
#   3. The unsupported-build fixture is a genuine negative witness: it
#      declares a non-`valid` `validity` and carries no usable value.

if(NOT DEFINED MY_LISP_EXE OR NOT EXISTS "${MY_LISP_EXE}")
  message(FATAL_ERROR "MY_LISP_EXE must name a built my-lisp.exe (run tools/build-my-lisp-runtime.ps1 first)")
endif()

if(NOT DEFINED FIXTURES_DIR OR NOT EXISTS "${FIXTURES_DIR}")
  message(FATAL_ERROR "FIXTURES_DIR must name adapter/tests/fixtures/observation")
endif()

set(memory_valid "${FIXTURES_DIR}/memory-player-health-valid.lisp")
set(screen_valid "${FIXTURES_DIR}/screen-player-health-valid.lisp")
set(memory_unsupported "${FIXTURES_DIR}/memory-unsupported-build.lisp")

foreach(fixture IN ITEMS "${memory_valid}" "${screen_valid}" "${memory_unsupported}")
  if(NOT EXISTS "${fixture}")
    message(FATAL_ERROR "missing fixture: ${fixture}")
  endif()

  execute_process(
    COMMAND "${MY_LISP_EXE}" --oracle-check "${fixture}"
    OUTPUT_VARIABLE oracle_output
    RESULT_VARIABLE oracle_result
  )

  if(NOT oracle_output MATCHES "\\(outcome valid\\)")
    message(FATAL_ERROR "${fixture} did not parse as valid canonical my-lisp data (oracle-check exit=${oracle_result}): ${oracle_output}")
  endif()

  message(STATUS "canonical my-lisp roundtrip OK: ${fixture}")
endforeach()

file(READ "${memory_valid}" memory_source)
file(READ "${screen_valid}" screen_source)
file(READ "${memory_unsupported}" unsupported_source)

if(NOT memory_source MATCHES "\\(fact player-health-ratio\\)")
  message(FATAL_ERROR "MEMORY fixture is missing the expected (fact player-health-ratio)")
endif()
if(NOT screen_source MATCHES "\\(fact player-health-ratio\\)")
  message(FATAL_ERROR "SCREEN fixture is missing the expected (fact player-health-ratio)")
endif()
if(NOT memory_source MATCHES "\\(source memory\\)")
  message(FATAL_ERROR "MEMORY fixture does not declare (source memory)")
endif()
if(NOT screen_source MATCHES "\\(source screen\\)")
  message(FATAL_ERROR "SCREEN fixture does not declare (source screen)")
endif()

if(NOT unsupported_source MATCHES "\\(validity unsupported-build\\)")
  message(FATAL_ERROR "negative-witness fixture does not declare (validity unsupported-build)")
endif()
if(unsupported_source MATCHES "\\(value 0\\.")
  message(FATAL_ERROR "negative-witness fixture must not carry a usable numeric value")
endif()

message(STATUS "observation contract fixtures: MEMORY/SCREEN share one fact, negative witness confirmed")
