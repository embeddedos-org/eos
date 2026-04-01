// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/kernel.h"
#include <string.h>
#include <stdio.h>

static eos_task_t g_tasks[EOS_MAX_TASKS];
static int g_task_count = 0;
static int g_current = -1;
static int g_running = 0;
static uint32_t g_tick = 0;

int eos_kernel_init(void) {
    memset(g_tasks, 0, sizeof(g_tasks));
    g_task_count = 0;
    g_current = -1;
    g_running = 0;
    g_tick = 0;
    return EOS_KERN_OK;
}

static int find_next_task(void) {
    int best = -1;
    uint8_t best_prio = 255;
    int start = (g_current + 1) % EOS_MAX_TASKS;
    for (int i = 0; i < EOS_MAX_TASKS; i++) {
        int idx = (start + i) % EOS_MAX_TASKS;
        eos_task_t *t = &g_tasks[idx];
        if (t->state != EOS_TASK_READY) continue;
        if (t->priority < best_prio) { best_prio = t->priority; best = idx; }
    }
    return best;
}

void eos_kernel_start(void) {
    g_running = 1;
    while (g_running) {
        for (int i = 0; i < EOS_MAX_TASKS; i++) {
            eos_task_t *t = &g_tasks[i];
            if (t->state == EOS_TASK_BLOCKED && t->wake_tick > 0 && g_tick >= t->wake_tick)
                t->state = EOS_TASK_READY;
        }
        int next = find_next_task();
        if (next < 0) { g_tick++; continue; }
        if (g_current >= 0 && g_tasks[g_current].state == EOS_TASK_RUNNING)
            g_tasks[g_current].state = EOS_TASK_READY;
        g_current = next;
        g_tasks[next].state = EOS_TASK_RUNNING;
        g_tasks[next].run_count++;
        g_tick++;
        if (g_tasks[next].entry) g_tasks[next].entry(g_tasks[next].arg);
        if (g_tasks[next].state == EOS_TASK_RUNNING)
            g_tasks[next].state = EOS_TASK_DELETED;
    }
}

bool eos_kernel_is_running(void) { return g_running != 0; }

void eos_kernel_tick(void) { g_tick++; }

int eos_task_create(const char *name, eos_task_func_t entry, void *arg,
                     uint8_t priority, uint32_t stack_size) {
    if (!entry) return EOS_KERN_INVALID;
    int slot = -1;
    for (int i = 0; i < EOS_MAX_TASKS; i++)
        if (g_tasks[i].state == EOS_TASK_DELETED || (g_tasks[i].id == 0 && g_tasks[i].entry == NULL && i > 0) || (i == 0 && g_task_count == 0 && g_tasks[i].entry == NULL))
            { slot = i; break; }
    if (slot < 0) {
        for (int i = 0; i < EOS_MAX_TASKS; i++)
            if (g_tasks[i].entry == NULL && g_tasks[i].state != EOS_TASK_RUNNING) { slot = i; break; }
    }
    if (slot < 0) return EOS_KERN_NO_MEMORY;
    memset(&g_tasks[slot], 0, sizeof(eos_task_t));
    g_tasks[slot].id = (uint8_t)slot;
    g_tasks[slot].name = name;
    g_tasks[slot].entry = entry;
    g_tasks[slot].arg = arg;
    g_tasks[slot].priority = priority;
    g_tasks[slot].stack_size = stack_size > 0 ? stack_size : 1024;
    g_tasks[slot].state = EOS_TASK_READY;
    g_task_count++;
    return slot;
}

int eos_task_delete(eos_task_handle_t h) {
    if (h >= EOS_MAX_TASKS) return EOS_KERN_INVALID;
    g_tasks[h].state = EOS_TASK_DELETED;
    g_tasks[h].entry = NULL;
    g_task_count--;
    return EOS_KERN_OK;
}

int eos_task_suspend(eos_task_handle_t h) {
    if (h >= EOS_MAX_TASKS || g_tasks[h].entry == NULL) return EOS_KERN_INVALID;
    g_tasks[h].state = EOS_TASK_SUSPENDED;
    return EOS_KERN_OK;
}

int eos_task_resume(eos_task_handle_t h) {
    if (h >= EOS_MAX_TASKS || g_tasks[h].entry == NULL) return EOS_KERN_INVALID;
    if (g_tasks[h].state == EOS_TASK_SUSPENDED) g_tasks[h].state = EOS_TASK_READY;
    return EOS_KERN_OK;
}

void eos_task_yield(void) {
    if (g_current >= 0) g_tasks[g_current].state = EOS_TASK_READY;
}

void eos_task_delay_ms(uint32_t ms) {
    if (g_current >= 0) {
        g_tasks[g_current].wake_tick = g_tick + (ms / 10) + 1;
        g_tasks[g_current].state = EOS_TASK_BLOCKED;
    }
}

eos_task_handle_t eos_task_get_current(void) {
    return (g_current >= 0) ? (eos_task_handle_t)g_current : 0xFF;
}

eos_task_state_t eos_task_get_state(eos_task_handle_t h) {
    if (h >= EOS_MAX_TASKS) return EOS_TASK_DELETED;
    return g_tasks[h].state;
}

const char *eos_task_get_name(eos_task_handle_t h) {
    if (h >= EOS_MAX_TASKS || !g_tasks[h].name) return "invalid";
    return g_tasks[h].name;
}