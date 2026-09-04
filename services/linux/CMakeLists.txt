add_library(eos_linux_security STATIC
    src/linux_security.c
)

target_include_directories(eos_linux_security PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# For EOS_ERROR. Same resolution as #108 took for eos_ota: report through the
# tree's logger rather than fprintf(stderr). No cycle -- eos_core does not
# depend on eos_linux_security.
#
# PRIVATE: eos/log.h is used by linux_security.c and by nothing under
# include/, so there is no reason to push eos_core's include directories onto
# every consumer. CMake still records the link requirement of a static
# library's PRIVATE dependency as $<LINK_ONLY:>, so consumers still link it.
target_link_libraries(eos_linux_security PRIVATE eos_core)

target_compile_options(eos_linux_security PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
