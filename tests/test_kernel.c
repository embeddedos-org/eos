/**
 * @file test_kernel.c
 * @brief Unit tests for EoS lightweight RTOS kernel
 */

#include <eos/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void run_##name(void) { \
        eos_kernel_init(); \
        printf("  %-50s ", #name); \
        name(); \
        tests_passed++; \
        printf("[PASS]\n"); \
    } \
    static void name(void)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

static void dummy_task(void *arg) { (void)arg; }

TEST(test_kernel_init) {
    ASSERT(eos_kernel_is_running() == false);
}

TEST(test_task_create) {
    int id = eos_task_create("task1", dummy_task, NULL, 1, 1024);
    ASSERT(id >= 0);
    ASSERT(eos_task_get_state((eos_task_handle_t)id) == EOS_TASK_READY);
    ASSERT(strcmp(eos_task_get_name((eos_task_handle_t)id), "task1") == 0);
}

TEST(test_task_create_null_entry) {
    int id = eos_task_create("bad", NULL, NULL, 1, 512);
    ASSERT(id == EOS_KERN_INVALID);
}

TEST(test_task_suspend_resume) {
    int id = eos_task_create("sr", dummy_task, NULL, 1, 512);
    ASSERT(id >= 0);
    ASSERT(eos_task_suspend((eos_task_handle_t)id) == EOS_KERN_OK);
    ASSERT(eos_task_get_state((eos_task_handle_t)id) == EOS_TASK_SUSPENDED);
    ASSERT(eos_task_resume((eos_task_handle_t)id) == EOS_KERN_OK);
    ASSERT(eos_task_get_state((eos_task_handle_t)id) == EOS_TASK_READY);
}

TEST(test_task_delete) {
    int id = eos_task_create("del", dummy_task, NULL, 1, 512);
    ASSERT(id >= 0);
    ASSERT(eos_task_delete((eos_task_handle_t)id) == EOS_KERN_OK);
    ASSERT(eos_task_get_state((eos_task_handle_t)id) == EOS_TASK_DELETED);
}

TEST(test_mutex_create_lock_unlock) {
    eos_mutex_handle_t m;
    ASSERT(eos_mutex_create(&m) == EOS_KERN_OK);
    ASSERT(eos_mutex_lock(m, 0) == EOS_KERN_OK);
    /* Second lock should timeout (no blocking in this impl) */
    ASSERT(eos_mutex_lock(m, 0) == EOS_KERN_TIMEOUT);
    ASSERT(eos_mutex_unlock(m) == EOS_KERN_OK);
    ASSERT(eos_mutex_lock(m, 0) == EOS_KERN_OK);
    ASSERT(eos_mutex_delete(m) == EOS_KERN_OK);
}

TEST(test_semaphore) {
    eos_sem_handle_t s;
    ASSERT(eos_sem_create(&s, 2, 3) == EOS_KERN_OK);
    ASSERT(eos_sem_get_count(s) == 2);
    ASSERT(eos_sem_wait(s, 0) == EOS_KERN_OK);
    ASSERT(eos_sem_get_count(s) == 1);
    ASSERT(eos_sem_wait(s, 0) == EOS_KERN_OK);
    ASSERT(eos_sem_get_count(s) == 0);
    ASSERT(eos_sem_wait(s, 0) == EOS_KERN_TIMEOUT);
    ASSERT(eos_sem_post(s) == EOS_KERN_OK);
    ASSERT(eos_sem_get_count(s) == 1);
}

TEST(test_queue) {
    eos_queue_handle_t q;
    ASSERT(eos_queue_create(&q, sizeof(int), 4) == EOS_KERN_OK);
    ASSERT(eos_queue_is_empty(q) == true);

    int val = 42;
    ASSERT(eos_queue_send(q, &val, 0) == EOS_KERN_OK);
    ASSERT(eos_queue_count(q) == 1);
    ASSERT(eos_queue_is_empty(q) == false);

    int out = 0;
    ASSERT(eos_queue_receive(q, &out, 0) == EOS_KERN_OK);
    ASSERT(out == 42);
    ASSERT(eos_queue_is_empty(q) == true);
}

TEST(test_queue_fifo_order) {
    eos_queue_handle_t q;
    eos_queue_create(&q, sizeof(int), 8);

    for (int i = 0; i < 5; i++) {
        eos_queue_send(q, &i, 0);
    }
    ASSERT(eos_queue_count(q) == 5);

    for (int i = 0; i < 5; i++) {
        int out;
        eos_queue_receive(q, &out, 0);
        ASSERT(out == i);
    }
}

TEST(test_queue_full) {
    eos_queue_handle_t q;
    eos_queue_create(&q, sizeof(int), 2);

    int v1 = 1, v2 = 2, v3 = 3;
    ASSERT(eos_queue_send(q, &v1, 0) == EOS_KERN_OK);
    ASSERT(eos_queue_send(q, &v2, 0) == EOS_KERN_OK);
    ASSERT(eos_queue_send(q, &v3, 0) == EOS_KERN_FULL);
    ASSERT(eos_queue_is_full(q) == true);
}

int main(void) {
    printf("=== EoS: Kernel Unit Tests ===\n\n");
    run_test_kernel_init();
    run_test_task_create();
    run_test_task_create_null_entry();
    run_test_task_suspend_resume();
    run_test_task_delete();
    run_test_mutex_create_lock_unlock();
    run_test_semaphore();
    run_test_queue();
    run_test_queue_fifo_order();
    run_test_queue_full();
    tests_run = 10;
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
