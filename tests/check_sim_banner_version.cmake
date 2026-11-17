# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
#
# The sim/ boot banners must print the project version, not a literal.
# Preprocesses each sim entry file with the host compiler (no code is
# compiled or run, so the bare-metal sources need no cross toolchain) and
# checks that every "EoS ... v" banner is followed by EOS_VERSION's value.
#
# cmake -DCC=<compiler> -DINC=<build>/generated -DVERSION=<x.y.z>
#       -DSOURCES=<a.c;b.c;...> -P check_sim_banner_version.cmake

foreach(var CC INC VERSION SOURCES)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "check_sim_banner_version: ${var} is not set")
    endif()
endforeach()

set(failed 0)
foreach(src IN LISTS SOURCES)
    execute_process(
        COMMAND "${CC}" -E -P "-I${INC}" "${src}"
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message("FAIL ${src}: preprocessing failed (${rc}): ${err}")
        math(EXPR failed "${failed} + 1")
        continue()
    endif()
    # A hard-coded version reads  ...v0.6.0 ...  inside one literal; the
    # macro leaves  ...v" "0.5.0" "...  after preprocessing.
    string(REGEX MATCHALL "EoS[^\"\n]* v[0-9]+\\.[0-9]+\\.[0-9]+" literal "${out}")
    string(REGEX MATCHALL "EoS[^\"\n]* v\" \"[^\"]*\"" banners "${out}")
    set(good "")
    set(bad "")
    foreach(b IN LISTS banners)
        if(b MATCHES "v\" \"${VERSION}\"$")
            list(APPEND good "${b}")
        else()
            list(APPEND bad "${b}")
        endif()
    endforeach()
    list(LENGTH good ngood)
    if(literal OR bad OR ngood EQUAL 0)
        message("FAIL ${src}: hard-coded ${literal}; wrong ${bad}; ${ngood} banner(s) with ${VERSION}")
        math(EXPR failed "${failed} + 1")
    else()
        message("PASS ${src}: ${ngood} banner(s) print EOS_VERSION (${VERSION})")
    endif()
endforeach()

if(failed GREATER 0)
    message(FATAL_ERROR "${failed} sim banner file(s) do not print the project version")
endif()
