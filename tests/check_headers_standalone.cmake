# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
#
# Every tracked public header must compile on its own. A public header that
# only compiles after some other header is a trap for the first application
# that includes it alone: services/posix/include/eos/posix_ipc.h used
# ssize_t but included only <stdint.h>, <stddef.h> and <stdbool.h> (#264),
# so a translation unit including it first failed with
# "unknown type name 'ssize_t'".
#
# What counts as a public header: the *.h files git tracks under ROOT that
# live in include/ or in a */include/eos/ directory -- the same 60 the
# #264 sweep measured. Which headers are read follows
# check_include_guards.cmake: git ls-files where there is a git work tree,
# a directory scan elsewhere (build trees, packaging copies and virtual
# environments in the working directory are not part of the source tree).
#
# How each header is compiled: one translation unit containing only
# #include "<path of the header relative to ROOT>", with ROOT and every
# include directory of the tree on the include path, under
# ${CC} -std=c11 -fsyntax-only. The include is by repo-relative path, not
# <eos/name.h>: two headers share a basename (package.h, gps_nmea.h), and
# the <eos/...> form would test whichever one the include order finds
# first. A header that needs a generated header or a product define to
# compile is a failure of this test, not of the header -- say so in the
# failure output if one ever appears.
#
# MSVC is not covered: cl spells the syntax-only check /Zs, not
# -fsyntax-only (same reason eos_sim_banner_version is skipped there).
# The MSVC-sensitive part of a fix -- a fallback typedef where MSVC's
# <sys/types.h> has no ssize_t -- is confirmed in review, not by this test.
#
# cmake -DROOT=<source dir> -DBINARY=<build dir> -DCC=<C compiler> \
#     -P check_headers_standalone.cmake

cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ROOT OR "${ROOT}" STREQUAL "")
    message(FATAL_ERROR "check_headers_standalone: ROOT is not set")
endif()
if(NOT IS_DIRECTORY "${ROOT}")
    message(FATAL_ERROR "check_headers_standalone: ROOT is not a directory: ${ROOT}")
endif()
if(NOT DEFINED CC OR "${CC}" STREQUAL "")
    message(FATAL_ERROR "check_headers_standalone: CC is not set")
endif()

# ---- which headers ----
set(headers "")
set(listed_by "git ls-files")
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ls-files -- "include/*.h" "*/include/eos/*.h"
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
    file(GLOB_RECURSE found RELATIVE "${ROOT}" "${ROOT}/include/eos/*.h")
    foreach(header IN LISTS found)
        list(APPEND headers "${header}")
    endforeach()
    file(GLOB_RECURSE found RELATIVE "${ROOT}" "${ROOT}/*/include/eos/*.h")
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
list(REMOVE_DUPLICATES headers)

# ---- where the working file goes ----
if(DEFINED BINARY AND NOT "${BINARY}" STREQUAL "")
    set(work "${BINARY}/header-standalone-tmp")
else()
    file(TO_CMAKE_PATH "$ENV{TMPDIR}" tmp_base)
    if("${tmp_base}" STREQUAL "")
        set(tmp_base "/tmp")
    endif()
    set(work "${tmp_base}/eos-header-standalone")
endif()
file(MAKE_DIRECTORY "${work}")
set(tu "${work}/tu.c")

# ---- include path: ROOT plus every include directory of the tree ----
# Each header's own include root is the directory ending in /include on
# the path to it (include/ itself for the top-level headers). The union
# of them is every */include directory, as in the #264 sweep.
set(inc_dirs "")
foreach(header IN LISTS headers)
    string(REGEX MATCH "^(.*)/eos/[^/]+$" _ "${header}")
    if(CMAKE_MATCH_1)
        set(root "${ROOT}/${CMAKE_MATCH_1}")
    else()
        set(root "${ROOT}")
    endif()
    list(APPEND inc_dirs "${root}")
endforeach()
list(REMOVE_DUPLICATES inc_dirs)
set(inc_args "")
foreach(dir IN LISTS inc_dirs)
    list(APPEND inc_args "-I${dir}")
endforeach()

# ---- compile each header on its own ----
set(checked 0)
set(failures "")
foreach(header IN LISTS headers)
    if(NOT EXISTS "${ROOT}/${header}")
        continue()    # tracked but deleted in the working tree
    endif()
    math(EXPR checked "${checked} + 1")
    file(WRITE "${tu}" "#include \"${header}\"\n")
    execute_process(
        COMMAND "${CC}" -std=c11 -fsyntax-only "-I${ROOT}" ${inc_args} "${tu}"
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err
        RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        list(APPEND failures "${header}")
        message("FAIL ${header} does not compile on its own:")
        message("${err}")
    endif()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR "check_headers_standalone: no headers found under ${ROOT}")
endif()

list(LENGTH failures failed)
if(failed GREATER 0)
    message(FATAL_ERROR
        "${failed} of ${checked} public headers do not compile on their own "
        "(${listed_by}); include what each header uses (see #264)")
endif()
message("PASS ${checked} public headers compile on their own (${listed_by})")
