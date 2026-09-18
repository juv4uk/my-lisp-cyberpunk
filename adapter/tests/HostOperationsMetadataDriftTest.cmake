if(NOT DEFINED GENERATOR)
  message(FATAL_ERROR "GENERATOR is required")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/host-operations-metadata-drift-test")
file(MAKE_DIRECTORY "${work}")

function(expect_rejected name source expected)
  set(input "${work}/${name}.lisp")
  set(output "${work}/${name}.hpp")
  file(WRITE "${input}" "${source}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${input} -DOUTPUT=${output} -P "${GENERATOR}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(rc EQUAL 0)
    message(FATAL_ERROR "registry accepted forbidden metadata drift: ${name}")
  endif()
  set(diag "${stdout}\n${stderr}")
  if(NOT diag MATCHES "${expected}")
    message(FATAL_ERROR "registry rejected ${name} for the wrong reason; expected '${expected}', got: ${diag}")
  endif()
endfunction()

expect_rejected(
  invalid-id
  "(host-operations/2\n  (cp:ABCD (semantic-id none) (surface uk log) (arity 0) (effect mutate) (input none) (result nil) (owner cyberpunk-host) (ffi LogPrimitive) (status live) (evidence id-drift))\n)\n"
  "unknown ID format"
)

expect_rejected(
  invalid-owner
  "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk log) (arity 0) (effect mutate) (input none) (result nil) (owner plugin-runtime) (ffi LogPrimitive) (status live) (evidence owner-drift))\n)\n"
  "invalid owner"
)

expect_rejected(
  zero-arity-with-input
  "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk class) (arity 0) (effect read) (input game-handle) (result string) (owner cyberpunk-host) (ffi ClassPrimitive) (status built) (evidence arity-drift))\n)\n"
  "arity/input mismatch"
)

expect_rejected(
  unary-with-no-input
  "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk class) (arity 1) (effect read) (input none) (result string) (owner cyberpunk-host) (ffi ClassPrimitive) (status built) (evidence arity-drift))\n)\n"
  "arity/input mismatch"
)

expect_rejected(
  unsupported-multi-arity
  "(host-operations/2\n  (cp:0001 (semantic-id none) (surface uk compare) (arity 2) (effect read) (input string) (result truth) (owner cyberpunk-host) (ffi ComparePrimitive) (status built) (evidence arity-drift))\n)\n"
  "unsupported arity"
)

message(STATUS "host operation registry rejects isolated ID/owner/arity drift")
