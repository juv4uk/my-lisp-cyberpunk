if(NOT DEFINED AUTHORITY OR NOT DEFINED ADAPTER_CMAKE OR NOT DEFINED MAIN_CPP OR
   NOT DEFINED FUNCTION_TABLE OR NOT DEFINED DECISION)
  message(FATAL_ERROR "authority contract inputs are required")
endif()

foreach(path IN ITEMS "${AUTHORITY}" "${ADAPTER_CMAKE}" "${MAIN_CPP}" "${FUNCTION_TABLE}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "authority contract input missing: ${path}")
  endif()
endforeach()

if(NOT EXISTS "${DECISION}")
  message(FATAL_ERROR "host operations authority ratification decision missing: ${DECISION}")
endif()

file(READ "${AUTHORITY}" authority)
if(NOT authority MATCHES "\\(host-operations/2")
  message(FATAL_ERROR "ratified authority is not host-operations/2 data")
endif()
foreach(required IN ITEMS "cp:0001" "cp:0002" "cp:0003" "запиши-лог" "гравець-присутній?" "клас")
  string(FIND "${authority}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "ratified authority is missing required v0 entry/surface: ${required}")
  endif()
endforeach()

file(READ "${ADAPTER_CMAKE}" cmake)
string(FIND "${cmake}" "set(HOST_OPERATIONS_REGISTRY \"\${CMAKE_CURRENT_SOURCE_DIR}/host-operations.lisp\")" authority_binding)
if(authority_binding EQUAL -1)
  message(FATAL_ERROR "adapter CMake does not bind the ratified authority path")
endif()
string(FIND "${cmake}" "-DINPUT=\${HOST_OPERATIONS_REGISTRY}" generator_input)
string(FIND "${cmake}" "-DOUTPUT=\${HOST_OPERATIONS_HEADER}" generator_header)
string(FIND "${cmake}" "-DMARKDOWN_OUTPUT=\${HOST_OPERATIONS_MARKDOWN}" generator_markdown)
if(generator_input EQUAL -1 OR generator_header EQUAL -1 OR generator_markdown EQUAL -1)
  message(FATAL_ERROR "C++/Markdown projections are not mechanically generated from the ratified authority")
endif()

file(READ "${MAIN_CPP}" main_cpp)
string(FIND "${main_cpp}" "#include \"generated/host_operations.generated.hpp\"" generated_include)
if(generated_include EQUAL -1)
  message(FATAL_ERROR "production adapter does not consume generated host operation projection")
endif()
foreach(id IN ITEMS "0001" "0002" "0003")
  string(FIND "${main_cpp}" "host_operations::OP_CP_${id}.surface" use_site)
  if(use_site EQUAL -1)
    message(FATAL_ERROR "production adapter does not register cp:${id} through generated identity")
  endif()
endforeach()

file(READ "${FUNCTION_TABLE}" audit_doc)
string(FIND "${audit_doc}" "generated from `adapter/host-operations.lisp`" generated_doc_claim)
string(FIND "${audit_doc}" "current details are intentionally not copied here" no_second_table_claim)
if(generated_doc_claim EQUAL -1 OR no_second_table_claim EQUAL -1)
  message(FATAL_ERROR "human identity audit no longer points at generated host-operation documentation")
endif()

file(READ "${DECISION}" decision)
foreach(claim IN ITEMS
    "Ratified authority: `adapter/host-operations.lisp`"
    "canonical repo-owned Lisp data"
    "cyberpunk-operations.всм"
    "superseded proposal"
    "not a second operation table")
  string(FIND "${decision}" "${claim}" claim_pos)
  if(claim_pos EQUAL -1)
    message(FATAL_ERROR "authority ratification decision missing claim: ${claim}")
  endif()
endforeach()

message(STATUS "host operations authority path and projections are ratified")
