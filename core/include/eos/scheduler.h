// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_SCHEDULER_H
#define EOS_SCHEDULER_H

#include "eos/graph.h"
#include "eos/error.h"
#include "eos/cache.h"

typedef struct EosBackend EosBackend;
typedef struct EosPackageSet EosPackageSet;

typedef struct {
    EosGraph *graph;
    const char *build_dir;
    const char *toolchain_target;
    EosCache cache;
    const EosPackageSet *packages;
    int dry_run;
    int verbose;
    int parallel_jobs;
    int built_count;
    int cached_count;
    int failed_count;
} EosScheduler;

void eos_scheduler_init(EosScheduler *sched, EosGraph *graph);
EosResult eos_scheduler_execute(EosScheduler *sched);

#endif /* EOS_SCHEDULER_H */
