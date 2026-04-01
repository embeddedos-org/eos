// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/system.h"
#include "eos/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#define PATH_SEP "\\"
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#define PATH_SEP "/"
#endif

static void resolve_output_dir(char *out, size_t sz,
                               const EosDocsConfig *docs_cfg,
                               const EosConfig *cfg) {
    if (docs_cfg->output_dir[0]) {
        strncpy(out, docs_cfg->output_dir, sz - 1);
    } else {
        snprintf(out, sz, "%s%sdocs", cfg->workspace.build_dir, PATH_SEP);
    }
}

static EosResult build_api_docs(const EosDocsConfig *docs_cfg,
                                const EosConfig *cfg,
                                const char *output_dir, int dry_run) {
    char api_dir[EOS_MAX_PATH];
    snprintf(api_dir, sizeof(api_dir), "%s%sapi", output_dir, PATH_SEP);

    EOS_INFO("  API docs: tool=%s", docs_cfg->api_tool);

    if (strcmp(docs_cfg->api_tool, "doxygen") == 0) {
        char cmd[2048];

        if (docs_cfg->api_config[0]) {
            snprintf(cmd, sizeof(cmd), "doxygen \"%s\"", docs_cfg->api_config);
        } else {
            /*
             * Generate a minimal Doxyfile on-the-fly, writing output to the
             * product build directory so docs ship with the image.
             */
            char doxyfile[EOS_MAX_PATH];
            snprintf(doxyfile, sizeof(doxyfile), "%s%sDoxyfile.auto",
                     cfg->workspace.build_dir, PATH_SEP);

            if (!dry_run) {
                MKDIR(cfg->workspace.build_dir);
                FILE *fp = fopen(doxyfile, "w");
                if (fp) {
                    fprintf(fp, "PROJECT_NAME = \"%s\"\n",
                            docs_cfg->title[0] ? docs_cfg->title : cfg->project.name);
                    fprintf(fp, "PROJECT_NUMBER = \"%s\"\n", cfg->project.version);
                    fprintf(fp, "OUTPUT_DIRECTORY = \"%s\"\n", api_dir);
                    fprintf(fp, "INPUT = core/include backends/include "
                                "pkg/include systems/include toolchains/include\n");
                    fprintf(fp, "FILE_PATTERNS = *.h\n");
                    fprintf(fp, "RECURSIVE = YES\n");
                    fprintf(fp, "EXTRACT_ALL = YES\n");
                    fprintf(fp, "GENERATE_HTML = YES\n");
                    fprintf(fp, "GENERATE_LATEX = NO\n");
                    fprintf(fp, "GENERATE_XML = YES\n");
                    fprintf(fp, "OPTIMIZE_OUTPUT_FOR_C = YES\n");
                    fprintf(fp, "QUIET = YES\n");
                    fclose(fp);
                    EOS_DEBUG("  Generated auto-Doxyfile: %s", doxyfile);
                }
            }

            snprintf(cmd, sizeof(cmd), "doxygen \"%s\"", doxyfile);
        }

        if (dry_run) {
            EOS_INFO("  (dry-run) Would run: %s", cmd);
        } else {
            MKDIR(api_dir);
            EOS_INFO("  Running: %s", cmd);
            int rc = system(cmd);
            if (rc != 0) {
                EOS_WARN("  Doxygen returned %d (is doxygen installed?)", rc);
            } else {
                EOS_INFO("  API docs generated: %s", api_dir);
            }
        }
    } else {
        EOS_WARN("  Unknown API doc tool: %s (supported: doxygen)", docs_cfg->api_tool);
    }

    return EOS_OK;
}

static EosResult build_user_docs(const EosDocsConfig *docs_cfg,
                                 const EosConfig *cfg,
                                 const char *output_dir, int dry_run) {
    char site_dir[EOS_MAX_PATH];
    snprintf(site_dir, sizeof(site_dir), "%s%ssite", output_dir, PATH_SEP);

    EOS_INFO("  User docs: tool=%s", docs_cfg->site_tool);

    if (strcmp(docs_cfg->site_tool, "mkdocs") == 0) {
        char cmd[2048];

        if (docs_cfg->site_config[0]) {
            snprintf(cmd, sizeof(cmd),
                     "mkdocs build -f \"%s\" -d \"%s\" --quiet",
                     docs_cfg->site_config, site_dir);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "mkdocs build -d \"%s\" --quiet", site_dir);
        }

        if (dry_run) {
            EOS_INFO("  (dry-run) Would run: %s", cmd);
        } else {
            EOS_INFO("  Running: %s", cmd);
            int rc = system(cmd);
            if (rc != 0) {
                EOS_WARN("  MkDocs returned %d (is mkdocs installed?)", rc);
            } else {
                EOS_INFO("  User docs generated: %s", site_dir);
            }
        }
    } else if (strcmp(docs_cfg->site_tool, "docusaurus") == 0) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "npx docusaurus build --out-dir \"%s\"", site_dir);

        if (dry_run) {
            EOS_INFO("  (dry-run) Would run: %s", cmd);
        } else {
            EOS_INFO("  Running: %s", cmd);
            int rc = system(cmd);
            if (rc != 0) {
                EOS_WARN("  Docusaurus returned %d", rc);
            }
        }
    } else {
        EOS_WARN("  Unknown site tool: %s (supported: mkdocs, docusaurus)",
                 docs_cfg->site_tool);
    }

    return EOS_OK;
}

EosResult eos_docs_build(const EosDocsConfig *docs_cfg, const EosConfig *cfg,
                         int dry_run) {
    if (!docs_cfg->enabled) {
        EOS_DEBUG("  Docs disabled, skipping");
        return EOS_OK;
    }

    char output_dir[EOS_MAX_PATH];
    resolve_output_dir(output_dir, sizeof(output_dir), docs_cfg, cfg);

    EOS_INFO("--- Documentation Build ---");
    EOS_INFO("  Project: %s v%s", cfg->project.name, cfg->project.version);
    EOS_INFO("  Output:  %s", output_dir);

    if (!dry_run) {
        MKDIR(output_dir);
    }

    if (docs_cfg->api_docs) {
        EOS_CHECK(build_api_docs(docs_cfg, cfg, output_dir, dry_run));
    }

    if (docs_cfg->user_docs) {
        EOS_CHECK(build_user_docs(docs_cfg, cfg, output_dir, dry_run));
    }

    EOS_INFO("--- Documentation Complete ---");
    return EOS_OK;
}

void eos_docs_dump(const EosDocsConfig *docs_cfg, const EosConfig *cfg) {
    char output_dir[EOS_MAX_PATH];
    resolve_output_dir(output_dir, sizeof(output_dir), docs_cfg, cfg);

    printf("Documentation:\n");
    printf("  enabled:    %s\n", docs_cfg->enabled ? "yes" : "no");
    if (docs_cfg->enabled) {
        printf("  api_docs:   %s (tool: %s)\n",
               docs_cfg->api_docs ? "yes" : "no", docs_cfg->api_tool);
        printf("  user_docs:  %s (tool: %s)\n",
               docs_cfg->user_docs ? "yes" : "no", docs_cfg->site_tool);
        printf("  output_dir: %s\n", output_dir);
        if (docs_cfg->api_config[0])
            printf("  api_config: %s\n", docs_cfg->api_config);
        if (docs_cfg->site_config[0])
            printf("  site_config:%s\n", docs_cfg->site_config);
    }
}
