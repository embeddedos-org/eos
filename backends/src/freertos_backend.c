// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/backend.h"
#include "eos/shell_cmd.h"
#include "eos/log.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* -D<key>=<value> for every option; the key unquoted as one word, the value
 * quoted. A value with spaces used to split into two arguments. */
static void append_defines(EosShellCmd *cmd, const EosPackage *pkg,
                           const char *skip_key) {
    int option_count = eos_pkg_option_count(pkg);
    for (int i = 0; i < option_count; i++) {
        const EosKeyValue *opt = eos_pkg_option_at(pkg, i);
        if (skip_key && strcmp(opt->key, skip_key) == 0) continue;
        eos_shell_cmd_text(cmd, " -D");
        eos_shell_cmd_word(cmd, opt->key);
        eos_shell_cmd_text(cmd, "=");
        eos_shell_cmd_arg(cmd, opt->value);
    }
}

static EosResult freertos_configure(EosBackend *self, const char *src_dir,
                                    const char *build_dir, const char *toolchain_file,
                                    const EosPackage *pkg) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake -S ");
    eos_shell_cmd_arg(&cmd, src_dir);
    eos_shell_cmd_text(&cmd, " -B ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -G Ninja");
    if (toolchain_file && toolchain_file[0]) {
        eos_shell_cmd_text(&cmd, " -DCMAKE_TOOLCHAIN_FILE=");
        eos_shell_cmd_arg(&cmd, toolchain_file);
    }
    /* Pass FREERTOS_KERNEL_PATH and the rest as -D options. */
    append_defines(&cmd, pkg, NULL);
    return eos_shell_cmd_run(&cmd, "FreeRTOS configure");
}

static EosResult freertos_build(EosBackend *self, const char *build_dir, int jobs) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --build ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -j ");
    eos_shell_cmd_int(&cmd, jobs > 0 ? jobs : 4);
    return eos_shell_cmd_run(&cmd, "FreeRTOS build");
}

#ifdef _WIN32
/* Copy every build_dir\*.<ext> into install_dir. Returns 0, or -1 if a match
 * could not be copied or its path did not fit. No match is not an error. */
static int copy_kind_win32(const char *build_dir, const char *install_dir, const char *ext) {
    char pattern[MAX_PATH], src[MAX_PATH], dst[MAX_PATH];
    int n = snprintf(pattern, sizeof(pattern), "%s\\*.%s", build_dir, ext);
    if (n < 0 || (size_t)n >= sizeof(pattern)) return -1;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;
    int rc = 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        n = snprintf(src, sizeof(src), "%s\\%s", build_dir, fd.cFileName);
        if (n < 0 || (size_t)n >= sizeof(src)) { rc = -1; continue; }
        n = snprintf(dst, sizeof(dst), "%s\\%s", install_dir, fd.cFileName);
        if (n < 0 || (size_t)n >= sizeof(dst)) { rc = -1; continue; }
        if (!CopyFileA(src, dst, FALSE)) rc = -1;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return rc;
}
#endif

static EosResult freertos_install(EosBackend *self, const char *build_dir,
                                  const char *install_dir) {
    (void)self;
#ifdef _WIN32
    /* In-process, like the POSIX find: a cmd.exe chain of
     * "copy *.bin & copy *.elf & copy *.hex" took its exit status from the
     * last copy, so a build with a .bin but no .hex failed to install. */
    /* The same refusal every backend gives (test_shell_cmd.c checks it):
     * nothing here goes through a shell, but a value the shell builder would
     * refuse is not a directory name this code should act on. */
    if (!eos_shell_arg_is_safe(build_dir) || !eos_shell_arg_is_safe(install_dir)) {
        EOS_ERROR("FreeRTOS install: refusing an unsafe directory name");
        return EOS_ERR_INVALID;
    }
    DWORD attrs = GetFileAttributesA(build_dir);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        EOS_ERROR("FreeRTOS install: build directory not found: %s", build_dir);
        return EOS_ERR_NOT_FOUND;
    }
    if (!CreateDirectoryA(install_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        EOS_ERROR("FreeRTOS install: cannot create %s", install_dir);
        return EOS_ERR_IO;
    }
    static const char *const kinds[] = { "bin", "elf", "hex" };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        if (copy_kind_win32(build_dir, install_dir, kinds[i]) != 0) {
            EOS_ERROR("FreeRTOS install: copying *.%s from %s failed", kinds[i], build_dir);
            return EOS_ERR_IO;
        }
    }
    EOS_INFO("FreeRTOS install: images from %s copied to %s", build_dir, install_dir);
    return EOS_OK;
#else
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "mkdir -p ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, " && find ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " -maxdepth 2 \\( -name '*.bin' -o -name '*.elf' -o -name '*.hex' \\) -exec cp {} ");
    eos_shell_cmd_arg(&cmd, install_dir);
    eos_shell_cmd_text(&cmd, "/ \\;");
    return eos_shell_cmd_run(&cmd, "FreeRTOS install");
#endif
}

static EosResult freertos_clean(EosBackend *self, const char *build_dir) {
    (void)self;
    EosShellCmd cmd;
    eos_shell_cmd_init(&cmd);
    eos_shell_cmd_text(&cmd, "cmake --build ");
    eos_shell_cmd_arg(&cmd, build_dir);
    eos_shell_cmd_text(&cmd, " --target clean");
    return eos_shell_cmd_run(&cmd, "FreeRTOS clean");
}

void eos_backend_freertos_init(EosBackend *b) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, "freertos", EOS_MAX_NAME - 1);
    b->type = EOS_BUILD_FREERTOS;
    b->configure = freertos_configure;
    b->build = freertos_build;
    b->install = freertos_install;
    b->clean = freertos_clean;
}
