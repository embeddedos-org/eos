# SPDX-License-Identifier: MIT
# EoS Dependency Resolution — Auto-fetch eboot + ebuild
#
# Resolution chain (tried in order):
#   1. User override:  -DEBOOT_SOURCE_DIR=<path>
#   2. Sibling dir:    ../eBoot/ (multi-repo checkout)
#   3. Cached repos:   ~/.eos/repos/eboot/ (bootstrap.sh)
#   4. FetchContent:   Clone from git at configure time
#
# Usage:
#   cmake -B build                           # auto-resolve everything
#   cmake -B build -DEOS_FETCH_DEPS=OFF      # skip auto-fetch (offline)
#   cmake -B build -DEBOOT_SOURCE_DIR=/path  # explicit path
#   cmake -B build -DEOS_WITH_EBOOT=OFF      # disable eboot integration

include(FetchContent)

# ====================================================================
# Options
# ====================================================================
option(EOS_WITH_EBOOT   "Integrate eBoot bootloader"              ON)
option(EOS_WITH_EBUILD  "Integrate ebuild layers (EAI, ENI, etc)" OFF)
option(EOS_FETCH_DEPS   "Auto-fetch deps via git if not found"    ON)

set(EBOOT_SOURCE_DIR  "" CACHE PATH "Explicit path to eBoot source tree")
set(EBUILD_SOURCE_DIR "" CACHE PATH "Explicit path to ebuild source tree")

set(EOS_EBOOT_GIT_REPO  "https://github.com/AethOS-EoS/eBoot.git" CACHE STRING "eBoot git URL")
set(EOS_EBOOT_GIT_TAG   "main" CACHE STRING "eBoot git tag/branch to fetch")
set(EOS_EBUILD_GIT_REPO "https://github.com/AethOS-EoS/ebuild.git" CACHE STRING "ebuild git URL")
set(EOS_EBUILD_GIT_TAG  "main" CACHE STRING "ebuild git tag/branch to fetch")

# ====================================================================
# Helper: resolve a dependency through the fallback chain
# ====================================================================
#   _eos_resolve_dep(<NAME> <source_dir_var> <git_repo> <git_tag>
#                    <sibling_names...>)
#
# Sets ${source_dir_var} in parent scope to the resolved path, or ""
# if not found and EOS_FETCH_DEPS is OFF.
#
macro(_eos_resolve_dep DEP_NAME SOURCE_VAR GIT_REPO GIT_TAG)
    set(_resolved "")
    set(_sibling_names ${ARGN})

    # 1. User override — already set via -D<SOURCE_VAR>=<path>
    if(${SOURCE_VAR} AND EXISTS "${${SOURCE_VAR}}/CMakeLists.txt")
        set(_resolved "${${SOURCE_VAR}}")
        message(STATUS "  [${DEP_NAME}] User override: ${_resolved}")
    endif()

    # 2. Sibling directories (multi-repo checkout)
    if(NOT _resolved)
        foreach(_name ${_sibling_names})
            if(EXISTS "${CMAKE_SOURCE_DIR}/../${_name}/CMakeLists.txt")
                get_filename_component(_resolved "${CMAKE_SOURCE_DIR}/../${_name}" ABSOLUTE)
                message(STATUS "  [${DEP_NAME}] Sibling dir: ${_resolved}")
                break()
            endif()
        endforeach()
    endif()

    # 3. Cached repos (~/.eos/repos/<name>)
    if(NOT _resolved)
        set(_cache_base "")
        if(DEFINED ENV{HOME})
            set(_cache_base "$ENV{HOME}/.eos/repos")
        elseif(DEFINED ENV{USERPROFILE})
            set(_cache_base "$ENV{USERPROFILE}/.eos/repos")
        endif()

        if(_cache_base)
            string(TOLOWER "${DEP_NAME}" _lower_name)
            if(EXISTS "${_cache_base}/${_lower_name}/CMakeLists.txt")
                set(_resolved "${_cache_base}/${_lower_name}")
                message(STATUS "  [${DEP_NAME}] Cached: ${_resolved}")
            endif()
        endif()
    endif()

    # 4. FetchContent — clone from git
    if(NOT _resolved AND EOS_FETCH_DEPS)
        string(TOLOWER "${DEP_NAME}" _fc_name)
        message(STATUS "  [${DEP_NAME}] Fetching from ${GIT_REPO} (${GIT_TAG})...")

        FetchContent_Declare(${_fc_name}
            GIT_REPOSITORY ${GIT_REPO}
            GIT_TAG        ${GIT_TAG}
            GIT_SHALLOW    TRUE
            GIT_PROGRESS   TRUE
        )
        FetchContent_GetProperties(${_fc_name})
        if(NOT ${_fc_name}_POPULATED)
            FetchContent_Populate(${_fc_name})
        endif()
        set(_resolved "${${_fc_name}_SOURCE_DIR}")
        message(STATUS "  [${DEP_NAME}] Fetched: ${_resolved}")
    endif()

    # Store result
    if(_resolved)
        set(${SOURCE_VAR} "${_resolved}" CACHE PATH "Resolved ${DEP_NAME} source" FORCE)
    endif()
endmacro()

# ====================================================================
# Resolve eBoot
# ====================================================================
if(EOS_WITH_EBOOT)
    message(STATUS "--- Resolving eBoot ---")

    _eos_resolve_dep(eBoot EBOOT_SOURCE_DIR
        "${EOS_EBOOT_GIT_REPO}" "${EOS_EBOOT_GIT_TAG}"
        "eBoot" "eboot" "eBot"
    )

    if(EBOOT_SOURCE_DIR AND EXISTS "${EBOOT_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${EBOOT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/eboot-build")
        set(EOS_HAS_EBOOT TRUE)
        message(STATUS "  eBoot: INTEGRATED (${EBOOT_SOURCE_DIR})")
    else()
        set(EOS_HAS_EBOOT FALSE)
        if(EOS_FETCH_DEPS)
            message(WARNING "eBoot not found and fetch failed. "
                "Run scripts/bootstrap.sh or pass -DEBOOT_SOURCE_DIR=<path>")
        else()
            message(STATUS "  eBoot: SKIPPED (EOS_FETCH_DEPS=OFF, not found locally)")
        endif()
    endif()
else()
    set(EOS_HAS_EBOOT FALSE)
    message(STATUS "  eBoot: DISABLED (EOS_WITH_EBOOT=OFF)")
endif()

# ====================================================================
# Resolve ebuild (for layers — EAI, ENI, EIPC, eOSuite)
# ====================================================================
if(EOS_WITH_EBUILD)
    message(STATUS "--- Resolving ebuild ---")

    _eos_resolve_dep(ebuild EBUILD_SOURCE_DIR
        "${EOS_EBUILD_GIT_REPO}" "${EOS_EBUILD_GIT_TAG}"
        "ebuild" "eBuild"
    )

    if(EBUILD_SOURCE_DIR AND EXISTS "${EBUILD_SOURCE_DIR}/CMakeLists.txt")
        set(EOS_HAS_EBUILD TRUE)
        message(STATUS "  ebuild: FOUND (${EBUILD_SOURCE_DIR})")
        # Don't add_subdirectory for ebuild's top-level — it would re-add eos.
        # Instead, we selectively include layers via cmake/ebuild_layers.cmake
    else()
        set(EOS_HAS_EBUILD FALSE)
        if(EOS_FETCH_DEPS)
            message(WARNING "ebuild not found and fetch failed. "
                "Run scripts/bootstrap.sh or pass -DEBUILD_SOURCE_DIR=<path>")
        else()
            message(STATUS "  ebuild: SKIPPED (EOS_FETCH_DEPS=OFF, not found locally)")
        endif()
    endif()
else()
    set(EOS_HAS_EBUILD FALSE)
    message(STATUS "  ebuild: DISABLED (EOS_WITH_EBUILD=OFF)")
endif()

# ====================================================================
# Export variables for downstream use
# ====================================================================
if(EOS_HAS_EBOOT)
    add_compile_definitions(EOS_HAS_EBOOT=1)
endif()

if(EOS_HAS_EBUILD)
    add_compile_definitions(EOS_HAS_EBUILD=1)
endif()

message(STATUS "--- Dependency Resolution Complete ---")
message(STATUS "  eBoot:  ${EOS_HAS_EBOOT}")
message(STATUS "  ebuild: ${EOS_HAS_EBUILD}")
