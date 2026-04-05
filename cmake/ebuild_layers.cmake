# SPDX-License-Identifier: MIT
# EoS — Optional layer integration from ebuild
#
# When EBUILD_SOURCE_DIR is resolved and EOS_WITH_EBUILD=ON,
# this file selectively integrates ebuild's optional layers
# (EAI, ENI, EIPC, eOSuite) into the eos build tree.
#
# Each layer is independently togglable via EOS_WITH_<LAYER>.
# This avoids add_subdirectory on ebuild's top-level CMakeLists.txt
# (which would create a circular dependency by re-adding eos).

if(NOT EOS_HAS_EBUILD)
    return()
endif()

if(NOT EBUILD_SOURCE_DIR)
    return()
endif()

# ====================================================================
# Layer options
# ====================================================================
option(EOS_WITH_EAI     "Build EAI — AI inference layer"                   OFF)
option(EOS_WITH_ENI     "Build ENI — Neural interface layer"               OFF)
option(EOS_WITH_EIPC    "Build EIPC — Secure IPC layer (C SDK)"            OFF)
option(EOS_WITH_EOSUITE "Build eOSuite — Dev tools and GUI apps"           OFF)
option(EOS_WITH_ALL_LAYERS "Build all optional layers"                     OFF)

if(EOS_WITH_ALL_LAYERS)
    set(EOS_WITH_EAI     ON CACHE BOOL "" FORCE)
    set(EOS_WITH_ENI     ON CACHE BOOL "" FORCE)
    set(EOS_WITH_EIPC    ON CACHE BOOL "" FORCE)
    set(EOS_WITH_EOSUITE ON CACHE BOOL "" FORCE)
endif()

# ====================================================================
# EAI — AI Inference (llama.cpp, Ebot server, model registry)
# ====================================================================
if(EOS_WITH_EAI)
    set(_eai_dir "${EBUILD_SOURCE_DIR}/layers/eai")
    if(EXISTS "${_eai_dir}/CMakeLists.txt")
        add_subdirectory("${_eai_dir}" "${CMAKE_BINARY_DIR}/_deps/layer-eai-build")
        message(STATUS "  Layer/EAI: ON (${_eai_dir})")
    else()
        message(WARNING "EAI layer requested but not found at ${_eai_dir}")
    endif()
endif()

# ====================================================================
# ENI — Neural Interface (Neuralink adapter, BCI framework)
# ====================================================================
if(EOS_WITH_ENI)
    set(_eni_dir "${EBUILD_SOURCE_DIR}/layers/eni")
    if(EXISTS "${_eni_dir}/CMakeLists.txt")
        add_subdirectory("${_eni_dir}" "${CMAKE_BINARY_DIR}/_deps/layer-eni-build")
        message(STATUS "  Layer/ENI: ON (${_eni_dir})")
    else()
        message(WARNING "ENI layer requested but not found at ${_eni_dir}")
    endif()
endif()

# ====================================================================
# EIPC — Secure IPC (C SDK only — Go server built separately)
# ====================================================================
if(EOS_WITH_EIPC)
    set(_eipc_dir "${EBUILD_SOURCE_DIR}/layers/eipc/sdk/c")
    if(EXISTS "${_eipc_dir}/CMakeLists.txt")
        add_subdirectory("${_eipc_dir}" "${CMAKE_BINARY_DIR}/_deps/layer-eipc-build")
        message(STATUS "  Layer/EIPC: ON (${_eipc_dir})")
    else()
        message(WARNING "EIPC layer requested but not found at ${_eipc_dir}")
    endif()
endif()

# ====================================================================
# eOSuite — Dev tools (Ebot AI client, GUI apps)
# ====================================================================
if(EOS_WITH_EOSUITE AND NOT WIN32)
    set(_eosuite_dir "${EBUILD_SOURCE_DIR}/layers/eosuite")
    if(EXISTS "${_eosuite_dir}/CMakeLists.txt")
        add_subdirectory("${_eosuite_dir}" "${CMAKE_BINARY_DIR}/_deps/layer-eosuite-build")
        message(STATUS "  Layer/eOSuite: ON (${_eosuite_dir})")
    else()
        message(WARNING "eOSuite layer requested but not found at ${_eosuite_dir}")
    endif()
else()
    if(EOS_WITH_EOSUITE AND WIN32)
        message(STATUS "  Layer/eOSuite: SKIPPED (not supported on Windows)")
    endif()
endif()

# ====================================================================
# Summary
# ====================================================================
message(STATUS "--- Layers ---")
message(STATUS "  EAI:     ${EOS_WITH_EAI}")
message(STATUS "  ENI:     ${EOS_WITH_ENI}")
message(STATUS "  EIPC:    ${EOS_WITH_EIPC}")
message(STATUS "  eOSuite: ${EOS_WITH_EOSUITE}")
