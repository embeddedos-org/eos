# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
#
# No two headers in the tree may share an include guard.
#
# Three guards were each used by two headers (#260): EOS_CONFIG_H by
# include/eos/eos_config.h and core/include/eos/config.h, EOS_PACKAGE_H and
# EOS_GPS_NMEA_H by a pair of same-named headers each. Whichever header of a
# pair came first defined the guard and the preprocessor then dropped the
# whole body of the other, with no diagnostic.
#
# A header's guard is read as its first two preprocessor directives when they
# are `#ifndef X` followed by `#define X` with nothing after the name. A header
# that opens any other way (`#pragma once`, a bare `#if`, no directives) has
# no guard as far as this check is concerned and is only counted.
#
# Every *.h under ROOT is checked except top-level directories whose name
# starts with "build" (the in-tree build directories .gitignore lists) and,
# when given, the directory named by BINARY.
#
# cmake -DROOT=<source dir> [-DBINARY=<build dir>] -P check_include_guards.cmake

if(NOT DEFINED ROOT OR "${ROOT}" STREQUAL "")
    message(FATAL_ERROR "check_include_guards: ROOT is not set")
endif()
if(NOT IS_DIRECTORY "${ROOT}")
    message(FATAL_ERROR "check_include_guards: ROOT is not a directory: ${ROOT}")
endif()

set(binary_prefix "")
if(DEFINED BINARY AND NOT "${BINARY}" STREQUAL "")
    file(RELATIVE_PATH binary_rel "${ROOT}" "${BINARY}")
    if(NOT binary_rel MATCHES "^\\.\\.")
        set(binary_prefix "${binary_rel}/")
    endif()
endif()

file(GLOB_RECURSE headers RELATIVE "${ROOT}" "${ROOT}/*.h")
list(SORT headers)

set(checked 0)
set(unguarded 0)
set(guards "")
foreach(header IN LISTS headers)
    if(header MATCHES "^build[^/]*/")
        continue()
    endif()
    if(NOT binary_prefix STREQUAL "")
        string(FIND "${header}" "${binary_prefix}" at)
        if(at EQUAL 0)
            continue()
        endif()
    endif()
    math(EXPR checked "${checked} + 1")

    # Only the first two directives matter. Later lines may hold ';' or
    # brackets, which split or merge list elements, but never these two.
    file(STRINGS "${ROOT}/${header}" directives REGEX "^[ \t]*#[ \t]*[A-Za-z]+")
    list(LENGTH directives count)
    set(guard "")
    if(count GREATER 1)
        list(GET directives 0 first)
        list(GET directives 1 second)
        if(first MATCHES "^[ \t]*#[ \t]*ifndef[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*$")
            set(name "${CMAKE_MATCH_1}")
            if(second MATCHES "^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*$"
               AND "${CMAKE_MATCH_1}" STREQUAL "${name}")
                set(guard "${name}")
            endif()
        endif()
    endif()

    if(guard STREQUAL "")
        math(EXPR unguarded "${unguarded} + 1")
        continue()
    endif()
    if(NOT DEFINED users_${guard})
        list(APPEND guards "${guard}")
    endif()
    list(APPEND users_${guard} "${header}")
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR "check_include_guards: no headers found under ${ROOT}")
endif()

set(collisions 0)
foreach(guard IN LISTS guards)
    list(LENGTH users_${guard} users)
    if(users GREATER 1)
        math(EXPR collisions "${collisions} + 1")
        message("FAIL ${guard} is the include guard of ${users} headers:")
        foreach(header IN LISTS users_${guard})
            message("       ${header}")
        endforeach()
    endif()
endforeach()

list(LENGTH guards unique)
if(collisions GREATER 0)
    message(FATAL_ERROR
        "${collisions} include guard(s) are shared by more than one header; "
        "give each header its own guard (see #260)")
endif()
message("PASS ${checked} headers checked: ${unique} include guards, each used "
        "by one header; ${unguarded} header(s) open without a guard")
