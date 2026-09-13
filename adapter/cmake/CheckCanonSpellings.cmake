# Reject direct branching on Canon display spellings in the reduced runtime.
# Canon surfaces must arrive through build.rs-generated QUOTE_SPELLINGS and
# COND_SPELLINGS, never through a new `name == "..."` branch in eval.rs.
if(NOT DEFINED INPUT)
  message(FATAL_ERROR "INPUT is required")
endif()

file(READ "${INPUT}" source)
# Comments may cite historical spellings while explaining why this guard
# exists. They are not executable dispatch and must not cause a false alarm.
string(REGEX REPLACE "//[^\n]*" "" executable_source "${source}")

foreach(spelling IN ITEMS quote cond як-є за-умовою svarūpa anukrama)
  set(direct_comparison "name[ \t]*(==|!=)[ \t]*\"${spelling}\"")
  if(executable_source MATCHES "${direct_comparison}")
    message(FATAL_ERROR
      "hardcoded Canon spelling '${spelling}' controls dispatch in ${INPUT}; use the generated Canon projection instead")
  endif()
endforeach()

message(STATUS "Canon spelling guard passed: ${INPUT}")
