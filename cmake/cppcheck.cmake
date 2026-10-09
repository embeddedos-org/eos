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
            --error-exitcode=1
            --suppress=missingIncludeSystem
            --suppress=constVariablePointer
            --suppress=constParameterPointer
            --suppress=variableScope
            --suppress=unusedFunction
            --inline-suppr
            --std=c11
            -I ${EoS_SOURCE_DIR}/include
            -I ${EoS_SOURCE_DIR}/hal/include
            -I ${EoS_SOURCE_DIR}/kernel/include
            -I ${EoS_SOURCE_DIR}/drivers/include
            -I ${EoS_SOURCE_DIR}/services/crypto/include
            -I ${EoS_SOURCE_DIR}/services/security/include
            -I ${EoS_SOURCE_DIR}/services/sensor/include
            -I ${EoS_SOURCE_DIR}/services/motor/include
            -I ${EoS_SOURCE_DIR}/services/ota/include
            -I ${EoS_SOURCE_DIR}/services/filesystem/include
            -I ${EoS_SOURCE_DIR}/net/include
            -I ${EoS_SOURCE_DIR}/power/include
            -I ${EoS_SOURCE_DIR}/core/include
            -I ${EoS_SOURCE_DIR}/debug/include
            ${EoS_SOURCE_DIR}/kernel/
            ${EoS_SOURCE_DIR}/hal/
            ${EoS_SOURCE_DIR}/services/
            ${EoS_SOURCE_DIR}/drivers/
            ${EoS_SOURCE_DIR}/net/
            ${EoS_SOURCE_DIR}/power/
            ${EoS_SOURCE_DIR}/core/
        )

        add_custom_target(cppcheck
            COMMAND ${CPPCHECK_EXECUTABLE} ${CPPCHECK_ARGS}
            WORKING_DIRECTORY ${EoS_SOURCE_DIR}
            COMMENT "Running cppcheck static analysis..."
        )
    else()
        add_custom_target(cppcheck
            COMMAND ${CMAKE_COMMAND} -E echo "Warning: cppcheck not found. Skipping static analysis."
        )
        message(WARNING "cppcheck not found — skipping static analysis")
    endif()
endif()
