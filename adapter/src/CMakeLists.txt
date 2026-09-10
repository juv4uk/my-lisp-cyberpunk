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

# `.my` є програмою мода, не C++-константою. Кладемо fixed dispatch поряд із
# DLL, щоб змінити поведінку можна було заміною сценарію без зміни адаптера.
add_custom_command(
  TARGET my-lisp-cyberpunk-plugin
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts"
    "$<TARGET_FILE_DIR:my-lisp-cyberpunk-plugin>/scripts"
)


add_executable(
  my-lisp-cyberpunk-dispatch-witness
  ../tests/DispatchWitness.cpp
)

if(MSVC)
  target_compile_options(my-lisp-cyberpunk-dispatch-witness PRIVATE /utf-8)
endif()

add_executable(
  my-lisp-cyberpunk-dispatch-benchmark
  ../tests/DispatchBenchmark.cpp
)

if(MSVC)
  target_compile_options(my-lisp-cyberpunk-dispatch-benchmark PRIVATE /utf-8)
endif()
