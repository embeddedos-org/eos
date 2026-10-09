# cmake/clang-tidy.cmake
# Optional clang-tidy integration via CMake properties

option(EOS_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis target" OFF)

if(EOS_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
    if(CLANG_TIDY_EXECUTABLE)
        file(GLOB_RECURSE ALL_C_FILES CONFIGURE_DEPENDS "${EoS_SOURCE_DIR}/*.c")
        list(FILTER ALL_C_FILES EXCLUDE REGEX "${EoS_SOURCE_DIR}/(build.*|\\.venv|\\.git)/")
        
        add_custom_target(clang-tidy
            COMMAND ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR} ${ALL_C_FILES}
            WORKING_DIRECTORY ${EoS_SOURCE_DIR}
            COMMENT "Running clang-tidy static analysis..."
        )
    else()
        add_custom_target(clang-tidy
            COMMAND ${CMAKE_COMMAND} -E echo "Warning: clang-tidy not found. Skipping static analysis."
        )
    endif()
endif()
