/*
 * eos_pkg_cli.c — CLI wrapper for eos-pkg package manager
 *
 * Usage:
 *   eos-pkg install ecal.eapp
 *   eos-pkg remove com.eos.eapps.ecal
 *   eos-pkg update ecal.eapp
 *   eos-pkg list
 *   eos-pkg info com.eos.eapps.ecal
 *   eos-pkg verify ecal.eapp
 *   eos-pkg run com.eos.eapps.ecal [args...]
 *   eos-pkg stop com.eos.eapps.ecal
 *   eos-pkg enable com.eos.eapps.ecal
 *   eos-pkg disable com.eos.eapps.ecal
 *
 * Copyright (c) 2024-2026 EmbeddedOS Project. MIT License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eos_pkg.h"

#ifdef _WIN32
  #define EOS_DEFAULT_APPS_DIR "C:\\apps"
#else
  #define EOS_DEFAULT_APPS_DIR "/apps"
#endif

#define EOS_PKG_VERSION "0.1.0"

static void print_usage(const char *prog)
{
    printf("eos-pkg %s — EoS Package Manager\n\n", EOS_PKG_VERSION);
    printf("Usage: %s <command> [arguments]\n\n", prog);
    printf("Commands:\n");
    printf("  install <file.eapp>       Install a .eapp package\n");
    printf("  remove  <package_id>      Uninstall a package\n");
    printf("  update  <file.eapp>       Update an installed package\n");
    printf("  list                      List all installed packages\n");
    printf("  info    <package_id>      Show detailed package info\n");
    printf("  verify  <file.eapp>       Verify a .eapp file integrity\n");
    printf("  run     <package_id> ...  Launch a package binary\n");
    printf("  stop    <package_id>      Stop a running package\n");
    printf("  enable  <package_id>      Enable a disabled package\n");
    printf("  disable <package_id>      Disable a package\n");
    printf("  arch                      Show detected host architecture\n");
    printf("  help                      Show this help message\n");
    printf("\nEnvironment:\n");
    printf("  EOS_APPS_DIR              Override default apps directory\n");
    printf("                            (default: %s)\n", EOS_DEFAULT_APPS_DIR);
}

static const char* get_apps_dir(void)
{
    const char *env = getenv("EOS_APPS_DIR");
    if (env && strlen(env) > 0) {
        return env;
    }
    return EOS_DEFAULT_APPS_DIR;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    /* Handle commands that don't need database */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("eos-pkg %s\n", EOS_PKG_VERSION);
        return 0;
    }

    if (strcmp(cmd, "arch") == 0) {
        eapp_arch_t arch = eos_pkg_detect_arch();
        printf("Host architecture: %s\n", eos_pkg_arch_name(arch));
        return 0;
    }

    if (strcmp(cmd, "verify") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing .eapp file path\n");
            fprintf(stderr, "Usage: %s verify <file.eapp>\n", argv[0]);
            return 1;
        }
        return eos_pkg_verify(argv[2]) == 0 ? 0 : 1;
    }

    /* All other commands need the database */
    const char *apps_dir = get_apps_dir();
    eapp_db_t db;

    if (eos_pkg_init(&db, apps_dir) != 0) {
        fprintf(stderr, "eos-pkg: failed to initialize (apps_dir=%s)\n", apps_dir);
        return 1;
    }

    int rc = 0;

    if (strcmp(cmd, "install") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing .eapp file path\n");
            fprintf(stderr, "Usage: %s install <file.eapp>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_install(&db, argv[2]);

    } else if (strcmp(cmd, "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing package ID\n");
            fprintf(stderr, "Usage: %s remove <package_id>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_remove(&db, argv[2]);

    } else if (strcmp(cmd, "update") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing .eapp file path\n");
            fprintf(stderr, "Usage: %s update <file.eapp>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_update(&db, argv[2]);

    } else if (strcmp(cmd, "list") == 0) {
        rc = eos_pkg_list(&db);

    } else if (strcmp(cmd, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing package ID\n");
            fprintf(stderr, "Usage: %s info <package_id>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_info(&db, argv[2]);

    } else if (strcmp(cmd, "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing package ID\n");
            fprintf(stderr, "Usage: %s run <package_id> [args...]\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_run(&db, argv[2], argc - 3, &argv[3]);

    } else if (strcmp(cmd, "stop") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing package ID\n");
            fprintf(stderr, "Usage: %s stop <package_id>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_stop(&db, argv[2]);

    } else if (strcmp(cmd, "enable") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing package ID\n");
            fprintf(stderr, "Usage: %s enable <package_id>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_enable(&db, argv[2]);

    } else if (strcmp(cmd, "disable") == 0) {
        if (argc < 3) {
            fprintf(stderr, "eos-pkg: missing package ID\n");
            fprintf(stderr, "Usage: %s disable <package_id>\n", argv[0]);
            return 1;
        }
        rc = eos_pkg_disable(&db, argv[2]);

    } else {
        fprintf(stderr, "eos-pkg: unknown command '%s'\n", cmd);
        fprintf(stderr, "Run '%s help' for usage.\n", argv[0]);
        return 1;
    }

    return rc == 0 ? 0 : 1;
}
