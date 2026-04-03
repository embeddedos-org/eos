# cmake/cppcheck.cmake
# Optional cppcheck integration for static analysis
#
# Usage:
#   cmake -B build -DEOS_ENABLE_CPPCHECK=ON
#   cmake --build build --target cppcheck

option(EOS_ENABLE_CPPCHECK "Enable cppcheck static analysis target" OFF)

if(EOS_ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXECUTABLE cppcheck)

    if(CPPCHECK_EXECUTABLE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXECUTABLE}")

        set(CPPCHECK_ARGS
            --enable=all
            --suppress=missingIncludeSystem
            --suppress=constVariablePointer
            --suppress=constParameterPointer
            --suppress=variableScope
            --suppress=unusedFunction
            --error-exitcode=1
            --inline-suppr
            --std=c11
            -I ${CMAKE_SOURCE_DIR}/include
            -I ${CMAKE_SOURCE_DIR}/hal/include
            -I ${CMAKE_SOURCE_DIR}/kernel/include
            -I ${CMAKE_SOURCE_DIR}/drivers/include
            -I ${CMAKE_SOURCE_DIR}/services/crypto/include
            -I ${CMAKE_SOURCE_DIR}/services/security/include
            -I ${CMAKE_SOURCE_DIR}/services/sensor/include
            -I ${CMAKE_SOURCE_DIR}/services/motor/include
            -I ${CMAKE_SOURCE_DIR}/services/ota/include
            -I ${CMAKE_SOURCE_DIR}/services/filesystem/include
            -I ${CMAKE_SOURCE_DIR}/net/include
            -I ${CMAKE_SOURCE_DIR}/power/include
            -I ${CMAKE_SOURCE_DIR}/core/include
            -I ${CMAKE_SOURCE_DIR}/debug/include
            ${CMAKE_SOURCE_DIR}/kernel/
            ${CMAKE_SOURCE_DIR}/hal/
            ${CMAKE_SOURCE_DIR}/services/
            ${CMAKE_SOURCE_DIR}/drivers/
            ${CMAKE_SOURCE_DIR}/net/
            ${CMAKE_SOURCE_DIR}/power/
            ${CMAKE_SOURCE_DIR}/core/
        )

        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE} ${CPPCHECK_ARGS}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running cppcheck static analysis..."
        )
    else()
        message(WARNING "cppcheck not found — 'cppcheck' target will not be available")
    endif()
endif()
