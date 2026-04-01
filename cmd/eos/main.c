// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eos/types.h"
#include "eos/error.h"
#include "eos/log.h"
#include "eos/config.h"
#include "eos/graph.h"
#include "eos/scheduler.h"
#include "eos/cache.h"
#include "eos/backend.h"
#include "eos/package.h"
#include "eos/toolchain.h"
#include "eos/system.h"

#define EOS_VERSION "0.2.0"
#define DEFAULT_CONFIG "eos.yaml"

static void print_banner(void) {
    printf("\n");
    printf("  _____ ___  ____   ____ _____ ___  ____  \n");
    printf(" |  ___/ _ \\|  _ \\ / ___| ____/ _ \\/ ___| \n");
    printf(" | |_ | | | | |_) | |  _|  _|| | | \\___ \\ \n");
    printf(" |  _|| |_| |  _ <| |_| | |__| |_| |___) |\n");
    printf(" |_|   \\___/|_| \\_\\\\____|_____\\___/|____/ \n");
    printf("\n");
    printf(" EoS v%s — Next-gen embedded OS build system\n", EOS_VERSION);
    printf("\n");
}

static void print_usage(void) {
    print_banner();
    printf("Usage: eos <command> [options]\n\n");
    printf("Commands:\n");
    printf("  build          Build all packages in the project\n");
    printf("  system         Build complete Linux OS image (kernel + rootfs + image)\n");
    printf("  firmware       Build RTOS firmware image\n");
    printf("  hybrid         Build hybrid system (Linux + RTOS)\n");
    printf("  add <package>  Add a package to eos.yaml\n");
    printf("  clean          Remove build artifacts\n");
    printf("  info           Show project info, resolved deps, toolchain\n");
    printf("  help           Show this help message\n");
    printf("\nOptions:\n");
    printf("  -c, --config <file>  Config file (default: eos.yaml)\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -n, --dry-run        Show what would be done without executing\n");
    printf("  -j, --jobs <N>       Parallel build jobs (default: 4)\n");
    printf("      --no-docs        Skip documentation generation\n");
    printf("  --help               Show command-specific help\n");
    printf("\nExamples:\n");
    printf("  eos build                Build all packages\n");
    printf("  eos system               Build complete Linux OS image\n");
    printf("  eos firmware             Build RTOS firmware\n");
    printf("  eos firmware --dry-run   Preview RTOS build pipeline\n");
    printf("  eos hybrid               Build Linux + RTOS hybrid system\n");
    printf("  eos add zlib             Add zlib to project\n");
    printf("  eos info                 Show project details\n");
    printf("\n");
}

static void print_build_help(void) {
    printf("Usage: eos build [options]\n\n");
    printf("Build all packages defined in eos.yaml.\n\n");
    printf("Options:\n");
    printf("  -c, --config <file>  Config file (default: eos.yaml)\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -n, --dry-run        Show build plan without executing\n");
    printf("  -j, --jobs <N>       Parallel build jobs (default: 4)\n");
    printf("  --help               Show this help\n\n");
    printf("The build command:\n");
    printf("  1. Parses eos.yaml config\n");
    printf("  2. Resolves package dependencies\n");
    printf("  3. Constructs build graph (DAG)\n");
    printf("  4. Topologically sorts for build order\n");
    printf("  5. Invokes appropriate backend for each package\n");
}

static void print_system_help(void) {
    printf("Usage: eos system [options]\n\n");
    printf("Build a complete Linux OS image: packages + kernel + rootfs + disk image.\n\n");
    printf("Options:\n");
    printf("  -c, --config <file>  Config file (default: eos.yaml)\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -n, --dry-run        Show full pipeline without executing\n");
    printf("  --help               Show this help\n\n");
    printf("The system command:\n");
    printf("  1. Build all packages (cross-compiled)\n");
    printf("  2. Install packages to staging directory\n");
    printf("  3. Build kernel (provider-specific)\n");
    printf("  4. Assemble rootfs (directory skeleton, init, users)\n");
    printf("  5. Create bootable disk image\n");
}

static void print_firmware_help(void) {
    printf("Usage: eos firmware [target] [options]\n\n");
    printf("Build RTOS firmware for embedded targets.\n\n");
    printf("Options:\n");
    printf("  -c, --config <file>  Config file (default: eos.yaml)\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -n, --dry-run        Show firmware build pipeline without executing\n");
    printf("  --help               Show this help\n\n");
    printf("Supported RTOS providers:\n");
    printf("  freertos     FreeRTOS (CMake/Make/vendor SDK)\n");
    printf("  zephyr       Zephyr RTOS (west/CMake)\n");
    printf("  nuttx        Apache NuttX (native config/build)\n");
    printf("  threadx      Azure ThreadX (vendor SDK)\n\n");
    printf("The firmware command:\n");
    printf("  1. Resolve RTOS SDK / source tree\n");
    printf("  2. Configure firmware build for target board\n");
    printf("  3. Cross-compile firmware\n");
    printf("  4. Generate output (.bin, .elf, .hex, .uf2)\n");
    printf("  5. Post-build size report and checksums\n");
}

static void print_hybrid_help(void) {
    printf("Usage: eos hybrid [options]\n\n");
    printf("Build a hybrid system: Linux image + RTOS firmware(s).\n\n");
    printf("Options:\n");
    printf("  -c, --config <file>  Config file (default: eos.yaml)\n");
    printf("  -v, --verbose        Enable verbose output\n");
    printf("  -n, --dry-run        Show full hybrid pipeline without executing\n");
    printf("  --help               Show this help\n\n");
    printf("The hybrid command:\n");
    printf("  Phase 1: Build Linux system (kernel + rootfs + image)\n");
    printf("  Phase 2: Build each RTOS firmware target\n");
    printf("  Phase 3: Combine outputs for multi-core deployment\n");
}

typedef struct {
    const char *command;
    const char *config_path;
    const char *add_package;
    const char *firmware_target;
    int verbose;
    int dry_run;
    int jobs;
    int show_help;
    int no_docs;
} CliArgs;

static void parse_args(CliArgs *args, int argc, char **argv) {
    memset(args, 0, sizeof(*args));
    args->config_path = DEFAULT_CONFIG;
    args->jobs = 4;

    for (int i = 1; i < argc; i++) {
        if (!args->command && argv[i][0] != '-') {
            args->command = argv[i];
            if (strcmp(args->command, "add") == 0 && i + 1 < argc) {
                args->add_package = argv[++i];
            }
            if (strcmp(args->command, "firmware") == 0 && i + 1 < argc &&
                argv[i + 1][0] != '-') {
                args->firmware_target = argv[++i];
            }
            continue;
        }

        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            args->verbose = 1;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            args->dry_run = 1;
        } else if (strcmp(argv[i], "--no-docs") == 0) {
            args->no_docs = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            args->show_help = 1;
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) && i + 1 < argc) {
            args->config_path = argv[++i];
        } else if ((strcmp(argv[i], "-j") == 0 || strcmp(argv[i], "--jobs") == 0) && i + 1 < argc) {
            args->jobs = atoi(argv[++i]);
            if (args->jobs < 1) args->jobs = 1;
        }
    }
}

static void init_backends(void) {
    static EosBackend cmake_be, ninja_be, make_be, kbuild_be;
    static EosBackend zephyr_be, freertos_be, nuttx_be;
    static EosBackend meson_be, autotools_be, buildroot_be, cargo_be;

    eos_backend_registry_init();

    eos_backend_cmake_init(&cmake_be);
    eos_backend_register(&cmake_be);

    eos_backend_ninja_init(&ninja_be);
    eos_backend_register(&ninja_be);

    eos_backend_make_init(&make_be);
    eos_backend_register(&make_be);

    eos_backend_kbuild_init(&kbuild_be);
    eos_backend_register(&kbuild_be);

    eos_backend_zephyr_init(&zephyr_be);
    eos_backend_register(&zephyr_be);

    eos_backend_freertos_init(&freertos_be);
    eos_backend_register(&freertos_be);

    eos_backend_nuttx_init(&nuttx_be);
    eos_backend_register(&nuttx_be);

    eos_backend_meson_init(&meson_be);
    eos_backend_register(&meson_be);

    eos_backend_autotools_init(&autotools_be);
    eos_backend_register(&autotools_be);

    eos_backend_buildroot_init(&buildroot_be);
    eos_backend_register(&buildroot_be);

    eos_backend_cargo_init(&cargo_be);
    eos_backend_register(&cargo_be);
}

static int cmd_info(const CliArgs *args) {
    static EosConfig cfg;
    EosResult res = eos_config_load(&cfg, args->config_path);
    if (res != EOS_OK) {
        EOS_ERROR("Failed to load config: %s (%s)", args->config_path, eos_error_str(res));
        return 1;
    }

    print_banner();
    eos_config_dump(&cfg);

    /* Load and display toolchain */
    static EosToolchain tc;
    if (cfg.toolchain.target[0]) {
        eos_toolchain_load_by_target(&tc, cfg.toolchain.target, "toolchains");
    } else {
        eos_toolchain_detect_host(&tc);
    }
    printf("\n");
    eos_toolchain_dump(&tc);

    /* Display RTOS toolchain if configured */
    if (cfg.toolchain.rtos_target[0]) {
        static EosToolchain rtos_tc;
        eos_toolchain_load_by_target(&rtos_tc, cfg.toolchain.rtos_target, "toolchains");
        printf("\nRTOS ");
        eos_toolchain_dump(&rtos_tc);
    }

    /* Build and display dependency graph */
    static EosPackageSet pkgs;
    eos_package_set_from_config(&pkgs, &cfg);
    if (pkgs.count > 0) {
        printf("\n");
        eos_package_dump(&pkgs);

        static EosGraph graph;
        eos_package_build_graph(&pkgs, &graph);
        eos_graph_topological_sort(&graph);
        printf("\n");
        eos_graph_dump(&graph);
    }

    /* Display system config based on kind */
    switch (cfg.system.kind) {
    case EOS_SYSTEM_LINUX:
        if (cfg.system.kernel.provider[0] || cfg.system.rootfs.provider[0]) {
            static EosSystem sys;
            eos_system_init(&sys, &cfg);
            printf("\n");
            eos_system_dump(&sys);
        }
        break;
    case EOS_SYSTEM_RTOS:
        for (int i = 0; i < cfg.system.rtos_count; i++) {
            static EosFirmware fw;
            eos_firmware_init(&fw, &cfg.system.rtos[i], &cfg);
            printf("\n");
            eos_firmware_dump(&fw);
        }
        break;
    case EOS_SYSTEM_HYBRID: {
        static EosHybridSystem hybrid;
        eos_hybrid_init(&hybrid, &cfg);
        printf("\n");
        eos_hybrid_dump(&hybrid);
        break;
    }
    default:
        break;
    }

    printf("\n");
    eos_docs_dump(&cfg.docs, &cfg);

    return 0;
}

static int cmd_build(const CliArgs *args) {
    static EosConfig cfg;
    EOS_CHECK(eos_config_load(&cfg, args->config_path));

    /* Resolve toolchain */
    static EosToolchain tc;
    if (cfg.toolchain.target[0]) {
        eos_toolchain_load_by_target(&tc, cfg.toolchain.target, "toolchains");
    } else {
        eos_toolchain_detect_host(&tc);
    }

    /* Load packages and resolve deps */
    static EosPackageSet pkgs;
    eos_package_set_from_config(&pkgs, &cfg);
    EOS_CHECK(eos_package_resolve(&pkgs));

    /* Build dependency graph */
    static EosGraph graph;
    EOS_CHECK(eos_package_build_graph(&pkgs, &graph));

    /* Schedule and execute */
    static EosScheduler sched;
    eos_scheduler_init(&sched, &graph);
    sched.build_dir = cfg.workspace.build_dir;
    sched.toolchain_target = tc.target_triple;
    sched.packages = &pkgs;
    sched.dry_run = args->dry_run;
    sched.verbose = args->verbose;
    sched.parallel_jobs = args->jobs;

    EOS_CHECK(eos_scheduler_execute(&sched));
    return 0;
}

static int cmd_system(const CliArgs *args) {
    static EosConfig cfg;
    EOS_CHECK(eos_config_load(&cfg, args->config_path));

    /* Resolve toolchain */
    static EosToolchain tc;
    if (cfg.toolchain.target[0]) {
        eos_toolchain_load_by_target(&tc, cfg.toolchain.target, "toolchains");
    } else {
        eos_toolchain_detect_host(&tc);
    }

    EOS_INFO("Target toolchain: %s (%s)", tc.name, eos_arch_str(tc.arch));

    /* Build all packages first */
    if (cfg.package_count > 0) {
        static EosPackageSet pkgs;
        eos_package_set_from_config(&pkgs, &cfg);
        eos_package_resolve(&pkgs);

        static EosGraph graph;
        eos_package_build_graph(&pkgs, &graph);

        static EosScheduler sched;
        eos_scheduler_init(&sched, &graph);
        sched.build_dir = cfg.workspace.build_dir;
        sched.toolchain_target = tc.target_triple;
        sched.packages = &pkgs;
        sched.dry_run = args->dry_run;
        sched.parallel_jobs = args->jobs;

        EOS_CHECK(eos_scheduler_execute(&sched));
    }

    /* Build system image */
    static EosSystem sys;
    eos_system_init(&sys, &cfg);
    sys.dry_run = args->dry_run;
    sys.verbose = args->verbose;

    EOS_CHECK(eos_system_build(&sys));

    if (!args->no_docs) {
        eos_docs_build(&cfg.docs, &cfg, args->dry_run);
    }

    return 0;
}

static int cmd_firmware(const CliArgs *args) {
    static EosConfig cfg;
    EOS_CHECK(eos_config_load(&cfg, args->config_path));

    if (cfg.system.rtos_count == 0) {
        EOS_ERROR("No RTOS firmware targets defined in config.");
        EOS_ERROR("Add a system.rtos section to %s", args->config_path);
        return 1;
    }

    /* Build packages first if any */
    if (cfg.package_count > 0) {
        static EosToolchain tc;
        const char *target = cfg.toolchain.rtos_target[0] ?
                             cfg.toolchain.rtos_target : cfg.toolchain.target;
        if (target[0]) {
            eos_toolchain_load_by_target(&tc, target, "toolchains");
        } else {
            eos_toolchain_detect_host(&tc);
        }

        static EosPackageSet pkgs;
        eos_package_set_from_config(&pkgs, &cfg);
        eos_package_resolve(&pkgs);

        static EosGraph graph;
        eos_package_build_graph(&pkgs, &graph);

        static EosScheduler sched;
        eos_scheduler_init(&sched, &graph);
        sched.build_dir = cfg.workspace.build_dir;
        sched.toolchain_target = tc.target_triple;
        sched.packages = &pkgs;
        sched.dry_run = args->dry_run;
        sched.parallel_jobs = args->jobs;

        EOS_CHECK(eos_scheduler_execute(&sched));
    }

    /* Build firmware targets */
    for (int i = 0; i < cfg.system.rtos_count; i++) {
        /* If a specific target was requested, filter by core name */
        if (args->firmware_target) {
            const EosRtosConfig *rc = &cfg.system.rtos[i];
            if (rc->core[0] && strcmp(rc->core, args->firmware_target) != 0 &&
                strcmp(eos_rtos_provider_str(rc->provider), args->firmware_target) != 0) {
                continue;
            }
        }

        static EosFirmware fw;
        eos_firmware_init(&fw, &cfg.system.rtos[i], &cfg);
        fw.dry_run = args->dry_run;
        fw.verbose = args->verbose;
        EOS_CHECK(eos_firmware_build(&fw));
    }

    if (!args->no_docs) {
        eos_docs_build(&cfg.docs, &cfg, args->dry_run);
    }

    return 0;
}

static int cmd_hybrid(const CliArgs *args) {
    static EosConfig cfg;
    EOS_CHECK(eos_config_load(&cfg, args->config_path));

    if (cfg.system.kind != EOS_SYSTEM_HYBRID) {
        EOS_WARN("Config system.kind is '%s', not 'hybrid'",
                 eos_system_kind_str(cfg.system.kind));
        EOS_WARN("Proceeding with hybrid build using available config");
    }

    /* Build packages first */
    if (cfg.package_count > 0) {
        static EosToolchain tc;
        if (cfg.toolchain.target[0]) {
            eos_toolchain_load_by_target(&tc, cfg.toolchain.target, "toolchains");
        } else {
            eos_toolchain_detect_host(&tc);
        }

        static EosPackageSet pkgs;
        eos_package_set_from_config(&pkgs, &cfg);
        eos_package_resolve(&pkgs);

        static EosGraph graph;
        eos_package_build_graph(&pkgs, &graph);

        static EosScheduler sched;
        eos_scheduler_init(&sched, &graph);
        sched.build_dir = cfg.workspace.build_dir;
        sched.toolchain_target = tc.target_triple;
        sched.packages = &pkgs;
        sched.dry_run = args->dry_run;
        sched.parallel_jobs = args->jobs;

        EOS_CHECK(eos_scheduler_execute(&sched));
    }

    /* Build hybrid system */
    static EosHybridSystem hybrid;
    eos_hybrid_init(&hybrid, &cfg);
    hybrid.dry_run = args->dry_run;
    hybrid.verbose = args->verbose;

    EOS_CHECK(eos_hybrid_build(&hybrid));

    if (!args->no_docs) {
        eos_docs_build(&cfg.docs, &cfg, args->dry_run);
    }

    return 0;
}

static int cmd_add(const CliArgs *args) {
    if (!args->add_package) {
        EOS_ERROR("Usage: eos add <package-name>");
        return 1;
    }

    EOS_INFO("Adding package: %s", args->add_package);

    /* Read existing config and append package */
    FILE *fp = fopen(args->config_path, "a");
    if (!fp) {
        /* Config doesn't exist, create minimal one */
        fp = fopen(args->config_path, "w");
        if (!fp) {
            EOS_ERROR("Cannot open %s for writing", args->config_path);
            return 1;
        }
        fprintf(fp, "project:\n");
        fprintf(fp, "  name: my-project\n");
        fprintf(fp, "  version: 0.1.0\n\n");
        fprintf(fp, "packages:\n");
    }

    fprintf(fp, "  - name: %s\n", args->add_package);
    fprintf(fp, "    version: latest\n");
    fprintf(fp, "    build:\n");
    fprintf(fp, "      type: cmake\n");
    fclose(fp);

    EOS_INFO("Added %s to %s", args->add_package, args->config_path);
    return 0;
}

static int cmd_clean(const CliArgs *args) {
    static EosConfig cfg;
    EosResult res = eos_config_load(&cfg, args->config_path);

    const char *build_dir = ".eos/build";
    if (res == EOS_OK && cfg.workspace.build_dir[0]) {
        build_dir = cfg.workspace.build_dir;
    }

    EOS_INFO("Cleaning build artifacts: %s", build_dir);

    if (args->dry_run) {
        EOS_INFO("  (dry-run) Would remove: %s", build_dir);
        return 0;
    }

    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "if exist \"%s\" rmdir /s /q \"%s\"", build_dir, build_dir);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", build_dir);
#endif
    int rc = system(cmd);
    if (rc == 0) {
        EOS_INFO("Clean complete");
    } else {
        EOS_WARN("Clean returned non-zero: %d", rc);
    }

    return 0;
}

int main(int argc, char **argv) {
    CliArgs args;
    parse_args(&args, argc, argv);

    if (args.verbose) {
        eos_log_set_level(EOS_LOG_DEBUG);
    }

    init_backends();

    if (!args.command || strcmp(args.command, "help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(args.command, "build") == 0) {
        if (args.show_help) { print_build_help(); return 0; }
        return cmd_build(&args);
    }

    if (strcmp(args.command, "system") == 0) {
        if (args.show_help) { print_system_help(); return 0; }
        return cmd_system(&args);
    }

    if (strcmp(args.command, "firmware") == 0) {
        if (args.show_help) { print_firmware_help(); return 0; }
        return cmd_firmware(&args);
    }

    if (strcmp(args.command, "hybrid") == 0) {
        if (args.show_help) { print_hybrid_help(); return 0; }
        return cmd_hybrid(&args);
    }

    if (strcmp(args.command, "add") == 0) {
        return cmd_add(&args);
    }

    if (strcmp(args.command, "clean") == 0) {
        return cmd_clean(&args);
    }

    if (strcmp(args.command, "info") == 0) {
        return cmd_info(&args);
    }

    EOS_ERROR("Unknown command: %s", args.command);
    print_usage();
    return 1;
}
