add_library(my-lisp-cyberpunk-plugin SHARED)

target_sources(
  my-lisp-cyberpunk-plugin
  PRIVATE
    GameHandleTable.cpp
    Main.cpp
)

target_include_directories(
  my-lisp-cyberpunk-plugin
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(
  my-lisp-cyberpunk-plugin
  PRIVATE
    RED4ext::SDK
)

set_target_properties(
  my-lisp-cyberpunk-plugin
  PROPERTIES
    OUTPUT_NAME "my-lisp-cyberpunk-plugin"
)

# The canonical host primitive names are Ukrainian UTF-8 identifiers.
if(MSVC)
  target_compile_options(my-lisp-cyberpunk-plugin PRIVATE /utf-8)
endif()
