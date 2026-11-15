# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
#
# No two headers of the source tree may open with the same include guard.
#
# Three guards were each used by two headers (#260): EOS_CONFIG_H by
# include/eos/eos_config.h and core/include/eos/config.h, EOS_PACKAGE_H and
# EOS_GPS_NMEA_H by a pair of same-named headers each. Whichever header of a
# pair came first defined the guard and the preprocessor then dropped the
# whole body of the other, with no diagnostic.
#
# What counts as a guard: the first two preprocessor directives of a header,
# when they are `#ifndef X` and `#define X` for the same X with nothing after
# the name. A header that opens any other way (`#pragma once`, `#if
# !defined(X)`, `#define X 1`, an #include first, no directives) has no guard
# as far as this check can see. Such headers are listed by name, not judged,
# so a guard written in another form is NOT compared with the others.
#
# Which headers are read: the *.h files git tracks under ROOT. Build trees,
# packaging copies and virtual environments in the working directory are not
# part of the source tree and must not change the verdict. Where ROOT is not
# a git work tree (a release tarball) or git is missing, every *.h under ROOT
# is read instead, except directories that hold a CMakeCache.txt (build trees,
# whatever they are called), top-level build*/ directories, and BINARY.
#
# cmake -DROOT=<source dir> [-DBINARY=<build dir>] -P check_include_guards.cmake

# Script mode sets no policies; without this, file(GLOB_RECURSE) follows
# directory symlinks on older CMake and counts a linked header twice.
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ROOT OR "${ROOT}" STREQUAL "")
    message(FATAL_ERROR "check_include_guards: ROOT is not set")
endif()
if(NOT IS_DIRECTORY "${ROOT}")
    message(FATAL_ERROR "check_include_guards: ROOT is not a directory: ${ROOT}")
endif()

# ---- which headers ----
set(headers "")
set(listed_by "git ls-files")
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ls-files -- "*.h"
        WORKING_DIRECTORY "${ROOT}"
        OUTPUT_VARIABLE tracked
        ERROR_QUIET
        RESULT_VARIABLE git_rc)
    if(git_rc EQUAL 0 AND NOT "${tracked}" STREQUAL "")
        string(REGEX REPLACE "\n$" "" tracked "${tracked}")
        string(REPLACE "\n" ";" headers "${tracked}")
    endif()
endif()

if("${headers}" STREQUAL "")
    set(listed_by "directory scan")
    set(skip "")
    if(DEFINED BINARY AND NOT "${BINARY}" STREQUAL "")
        file(RELATIVE_PATH binary_rel "${ROOT}" "${BINARY}")
        if(NOT binary_rel MATCHES "^\\.\\.")
            list(APPEND skip "${binary_rel}/")
        endif()
    endif()
    file(GLOB_RECURSE caches RELATIVE "${ROOT}" "${ROOT}/CMakeCache.txt")
    foreach(cache IN LISTS caches)
        get_filename_component(tree "${cache}" DIRECTORY)
        if(NOT "${tree}" STREQUAL "")
            list(APPEND skip "${tree}/")
        endif()
    endforeach()
    file(GLOB_RECURSE found RELATIVE "${ROOT}" "${ROOT}/*.h")
    foreach(header IN LISTS found)
        set(keep TRUE)
        if(header MATCHES "^build[^/]*/")
            set(keep FALSE)
        endif()
        foreach(prefix IN LISTS skip)
            string(FIND "${header}" "${prefix}" at)
            if(at EQUAL 0)
                set(keep FALSE)
            endif()
        endforeach()
        if(keep)
            list(APPEND headers "${header}")
        endif()
    endforeach()
endif()
list(SORT headers)

# ---- read each header's guard ----
set(checked 0)
set(guards "")
set(unguarded "")
foreach(header IN LISTS headers)
    if(NOT EXISTS "${ROOT}/${header}")
        continue()    # tracked but deleted in the working tree
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
            # Nested on purpose: in one if(), ${CMAKE_MATCH_1} would be
            # expanded before the second MATCHES ran and still hold `name`.
            if(second MATCHES "^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*$")
                if("${CMAKE_MATCH_1}" STREQUAL "${name}")
                    set(guard "${name}")
                endif()
            endif()
        endif()
    endif()

    if(guard STREQUAL "")
        list(APPEND unguarded "${header}")
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
list(LENGTH unguarded bare)
foreach(header IN LISTS unguarded)
    message("note ${header} opens without an #ifndef/#define guard; not compared")
endforeach()
if(collisions GREATER 0)
    message(FATAL_ERROR
        "${collisions} include guard(s) are shared by more than one header; "
        "give each header its own guard (see #260)")
endif()
message("PASS ${checked} headers checked (${listed_by}): ${unique} include "
        "guards, each used by one header; ${bare} header(s) open without a guard")
