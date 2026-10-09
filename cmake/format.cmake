# cmake/format.cmake
# Optional clang-format integration

option(EOS_ENABLE_CLANG_FORMAT "Enable clang-format target" OFF)

if(EOS_ENABLE_CLANG_FORMAT)
    find_program(CLANG_FORMAT_EXECUTABLE clang-format)
    if(CLANG_FORMAT_EXECUTABLE)
        add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -DCLANG_FORMAT_EXECUTABLE=${CLANG_FORMAT_EXECUTABLE} -DEoS_SOURCE_DIR=${EoS_SOURCE_DIR} -P ${EoS_SOURCE_DIR}/cmake/format-diff.cmake
            WORKING_DIRECTORY ${EoS_SOURCE_DIR}
            COMMENT "Formatting changed C/C++ code with clang-format"
        )
    else()
        add_custom_target(format
            COMMAND ${CMAKE_COMMAND} -E echo "Warning: clang-format not found. Skipping formatting."
        )
        message(WARNING "clang-format not found — skipping formatting")
    endif()
endif()
