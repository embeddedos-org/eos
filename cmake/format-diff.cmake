# This script is invoked by the `format` custom target at build time.
# Instead of formatting the entire repository, it uses Git to find only the files
# that have been modified or are newly added (untracked).

# 1. Get the list of modified files (staged and unstaged) relative to HEAD
execute_process(
    COMMAND git diff --name-only HEAD
    WORKING_DIRECTORY ${EoS_SOURCE_DIR}
    OUTPUT_VARIABLE changed_files
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 2. Get the list of untracked files (new files not yet added to Git)
execute_process(
    COMMAND git ls-files --others --exclude-standard
    WORKING_DIRECTORY ${EoS_SOURCE_DIR}
    OUTPUT_VARIABLE untracked_files
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Combine both lists and split them into a CMake list (separated by semicolons)
set(all_changed "${changed_files}\n${untracked_files}")
string(REPLACE "\n" ";" changed_files_list "${all_changed}")

# 3. Filter the list to include only C/C++ source files and exclude vendored code
set(files_to_format "")
foreach(f ${changed_files_list})
    if(f MATCHES "\\.(c|h)$")
        # Exclude vendored cryptographic sources.
        # Modifying them would cause synchronization drift and conflict with upstream.
        if(NOT f MATCHES "^services/crypto/src/(ed25519_.*|precomp_data\\.h|fe\\.h|ge\\.h|sc\\.h|fixedint\\.h)")
            list(APPEND files_to_format "${EoS_SOURCE_DIR}/${f}")
        endif()
    endif()
endforeach()

# 4. Run clang-format if there are any applicable files
if(files_to_format)
    execute_process(COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${files_to_format} WORKING_DIRECTORY ${EoS_SOURCE_DIR})
    message(STATUS "Formatted ${files_to_format}")
else()
    message(STATUS "No C/C++ files changed. Skipping format.")
endif()
