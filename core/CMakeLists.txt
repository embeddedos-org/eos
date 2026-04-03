add_library(eos_core STATIC
    src/log.c
    src/config.c
    src/graph.c
    src/cache.c
    src/lockfile.c
    src/layer.c
    src/scheduler.c
)
target_include_directories(eos_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
