# The .eapp package manager. Nothing built this directory until now, which is
# why its all-zero trust anchor survived: no target compiled the file, so no
# test could reach it. See #98.
#
# The target is eos_eapp, not eos_pkg: pkg/ already claims that name for the
# build-recipe package registry, a different module with a similar one.

# eos_pkg_trust_anchor.c is pure in-memory state (#133) -- no directory
# listing, no process spawning -- so unlike eos_eapp below it builds on every
# target, host or cross. This is what lets a test link the real
# eos_pkg_set_trust_anchor()/eos_pkg_trust_anchor() policy instead of only the
# crypto predicate underneath it.
add_library(eos_pkg_trust_anchor STATIC
    eos_pkg_trust_anchor.c
)

target_include_directories(eos_pkg_trust_anchor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(eos_pkg_trust_anchor PUBLIC eos_crypto)

target_compile_options(eos_pkg_trust_anchor PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# eos_pkg.c itself lists directories, spawns processes and waits on them for
# install/remove/run, so it needs <dirent.h>, <unistd.h> and <sys/wait.h> --
# a hard #error under bare-metal newlib. Host-only, the same reason
# net_posix.c is gated in the top-level CMakeLists.txt.
if(NOT CMAKE_CROSSCOMPILING)
    add_library(eos_eapp STATIC
        eos_pkg.c
    )

    target_include_directories(eos_eapp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(eos_eapp PUBLIC eos_crypto eos_pkg_trust_anchor)

    target_compile_options(eos_eapp PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4>
    )
endif()
