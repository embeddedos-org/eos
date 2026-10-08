# The .eapp package manager.
#
# eos_pkg_anchor.c is the trust-anchor state machine (set/refuse/lookup).
# It needs only eos_crypto and libc, so it builds for every target as
# eos_pkg_anchor -- including the bare-metal cross targets, where the
# anchor decision gets the compile coverage it never had (#133).
#
# The rest (eos_pkg.c: install/verify/remove, the CLI) lists directories,
# spawns processes and waits on them, so it needs <dirent.h>, <unistd.h>
# and <sys/wait.h>. Bare-metal newlib has none of those -- <dirent.h> is a
# hard #error there -- so eos_eapp stays host-only, gated the same way
# net_posix.c is.
add_library(eos_pkg_anchor STATIC
    eos_pkg_anchor.c
)

target_include_directories(eos_pkg_anchor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(eos_pkg_anchor PUBLIC eos_crypto)

target_compile_options(eos_pkg_anchor PRIVATE
    $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

if(NOT CMAKE_CROSSCOMPILING)
    add_library(eos_eapp STATIC
        eos_pkg.c
    )

    target_include_directories(eos_eapp PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(eos_eapp PUBLIC eos_pkg_anchor)

    target_compile_options(eos_eapp PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
        $<$<C_COMPILER_ID:MSVC>:/W4>
    )
endif()
