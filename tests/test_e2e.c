// SPDX-License-Identifier: MIT
// End-to-end integration test: boot->kernel->task->sync->IPC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "eos/kernel.h"
#include "eos/kernel_internal.h"
#include "eos/boot_params.h"
#include "eos/mem.h"
uint32_t *eos_port_init_stack(uint32_t *s,void(*e)(void*),void*a){(void)e;(void)a;return s-17;}
void eos_port_start_first_task(void){}
void eos_port_yield(void){}
uint32_t eos_port_enter_critical(void){return 0;}
void eos_port_exit_critical(uint32_t s){(void)s;}
static int tr=0,tp=0;
#define T(c,m) do{tr++;if(!(c)){printf("  [FAIL] %s (%d)\n",m,__LINE__);return 1;}tp++;}while(0)
extern void eos_task_wake_check(uint32_t);
extern void eos_swtimer_tick(uint32_t);
static uint32_t ck(const eos_boot_params_t*p){const uint32_t*w=(const uint32_t*)p;uint32_t n=sizeof(*p)/4-1,cs=0;for(uint32_t i=0;i<n;i++)cs^=w[i];return cs;}
static int test_boot(void){
    printf("\n-- Boot Params --\n");
    eos_boot_params_t bp;memset(&bp,0,sizeof(bp));
    bp.magic=EOS_BOOT_MAGIC;bp.version=1;bp.clock_hz=168000000;bp.ram_start=0x20000000;bp.ram_size=0x20000;bp.tick_rate_hz=1000;
    bp.boot_flags=EOS_BOOT_FLAG_DEBUG|EOS_BOOT_FLAG_WDT;bp.checksum=ck(&bp);
    T(bp.magic==0x454F5342U,"magic=EOSB");T(bp.checksum==ck(&bp),"checksum valid");
    bp.clock_hz=84000000;T(bp.checksum!=ck(&bp),"corruption detected");
    T(bp.boot_flags&EOS_BOOT_FLAG_DEBUG,"DEBUG set");T(!(bp.boot_flags&EOS_BOOT_FLAG_SECURE),"SECURE unset");
    printf("  [PASS] boot: 5 checks\n");return 0;}
static void dummy(void*a){(void)a;}
static int ga,gb,gc;
static int test_kinit(void){
    printf("\n-- Kernel Init --\n");
    T(eos_kernel_init()==EOS_KERN_OK,"init OK");T(!eos_kernel_is_running(),"not running");
    T(eos_task_get_state(0)==EOS_TASK_READY,"idle READY");T(strcmp(eos_task_get_name(0),"idle")==0,"idle name");
    T(eos_task_get_priority_internal(0)==255,"idle prio=255");T(eos_tick_get()==0,"tick=0");
    printf("  [PASS] init: 6 checks\n");return 0;}
static int test_tasks(void){
    printf("\n-- Tasks --\n");
    ga=eos_task_create("high",dummy,NULL,1,512);gb=eos_task_create("mid",dummy,NULL,5,512);gc=eos_task_create("low",dummy,NULL,10,512);
    T(ga>0,"A created");T(gb>0,"B created");T(gc>0,"C created");T(ga!=gb&&gb!=gc,"unique");
    T(eos_task_get_state((uint8_t)ga)==EOS_TASK_READY,"A READY");
    T(eos_task_get_priority_internal((uint8_t)ga)==1,"A prio=1");
    T(eos_task_create("x",NULL,NULL,5,512)==EOS_KERN_INVALID,"NULL rejected");
    printf("  [PASS] tasks: 7 checks\n");return 0;}
static int test_acc(void){
    printf("\n-- Accessors --\n");
    uint8_t h=(uint8_t)gb;
    T(eos_task_get_priority_internal(h)==5,"prio=5");
    eos_task_set_priority_internal(h,2);T(eos_task_get_priority_internal(h)==2,"prio->2");
    eos_task_set_priority_internal(h,5);
    eos_task_block_with_timeout(h,100);T(g_tasks[h].state==EOS_TASK_BLOCKED,"blocked");
    T(g_tasks[h].wake_tick==g_tick+100,"wake=tick+100");
    eos_task_unblock(h);T(g_tasks[h].state==EOS_TASK_READY,"unblocked");T(g_tasks[h].wake_tick==0,"wake=0");
    eos_task_block_with_timeout(h,EOS_WAIT_FOREVER);T(g_tasks[h].wake_tick==0,"FOREVER=0");eos_task_unblock(h);
    T(eos_task_get_priority_internal(EOS_MAX_TASKS)==255,"OOB=255");
    printf("  [PASS] acc: 9 checks\n");return 0;}
static int test_tick(void){
    printf("\n-- Tick --\n");
    uint8_t h=(uint8_t)gc;uint32_t t0=eos_tick_get();
    for(int i=0;i<5;i++)eos_kernel_tick();T(eos_tick_get()==t0+5,"tick+5");
    eos_task_block_with_timeout(h,10);uint32_t w=g_tasks[h].wake_tick;
    for(int i=0;i<5;i++)eos_kernel_tick();eos_task_wake_check(eos_tick_get());
    T(g_tasks[h].state==EOS_TASK_BLOCKED,"still blocked");
    while(eos_tick_get()<w)eos_kernel_tick();eos_task_wake_check(eos_tick_get());
    T(g_tasks[h].state==EOS_TASK_READY,"woke");
    printf("  [PASS] tick: 3 checks\n");return 0;}
static int test_mtx(void){
    printf("\n-- Mutex --\n");
    eos_mutex_handle_t m;
    T(eos_mutex_create(&m)==EOS_KERN_OK,"created");
    T(eos_mutex_lock(m,0)==EOS_KERN_OK,"locked");
    T(eos_mutex_lock(m,0)==EOS_KERN_OK,"recursive");
    T(eos_mutex_unlock(m)==EOS_KERN_OK,"unlock 2->1");T(eos_mutex_unlock(m)==EOS_KERN_OK,"unlock 1->0");
    T(eos_mutex_delete(m)==EOS_KERN_OK,"deleted");
    T(eos_mutex_create(NULL)==EOS_KERN_INVALID,"NULL");
    eos_mutex_handle_t ms[EOS_MAX_MUTEXES];
    for(int i=0;i<EOS_MAX_MUTEXES;i++)eos_mutex_create(&ms[i]);
    eos_mutex_handle_t ex;T(eos_mutex_create(&ex)==EOS_KERN_NO_MEMORY,"exhausted");
    for(int i=0;i<EOS_MAX_MUTEXES;i++)eos_mutex_delete(ms[i]);
    T(eos_mutex_create(&ex)==EOS_KERN_OK,"recovered");eos_mutex_delete(ex);
    printf("  [PASS] mutex: 9 checks\n");return 0;}
static int test_sem(void){
    printf("\n-- Semaphore --\n");
    eos_sem_handle_t s;T(eos_sem_create(&s,3,5)==EOS_KERN_OK,"created");
    T(eos_sem_get_count(s)==3,"cnt=3");
    T(eos_sem_wait(s,0)==EOS_KERN_OK,"w1");T(eos_sem_wait(s,0)==EOS_KERN_OK,"w2");T(eos_sem_wait(s,0)==EOS_KERN_OK,"w3");
    T(eos_sem_get_count(s)==0,"cnt=0");T(eos_sem_wait(s,0)==EOS_KERN_TIMEOUT,"empty");
    T(eos_sem_post(s)==EOS_KERN_OK,"p1");T(eos_sem_get_count(s)==1,"cnt=1");
    eos_sem_post(s);eos_sem_post(s);eos_sem_post(s);eos_sem_post(s);
    T(eos_sem_get_count(s)==5,"cnt=5");T(eos_sem_post(s)==EOS_KERN_FULL,"full");
    eos_sem_delete(s);
    printf("  [PASS] sem: 11 checks\n");return 0;}
typedef struct{int id;char t[4];}msg_t;
static int test_q(void){
    printf("\n-- Queue --\n");
    eos_queue_handle_t q;T(eos_queue_create(&q,sizeof(msg_t),4)==EOS_KERN_OK,"created");T(eos_queue_is_empty(q),"empty");
    msg_t m1={1,"A"},m2={2,"B"},m3={3,"C"};
    T(eos_queue_send(q,&m1,0)==EOS_KERN_OK,"s1");T(eos_queue_send(q,&m2,0)==EOS_KERN_OK,"s2");T(eos_queue_send(q,&m3,0)==EOS_KERN_OK,"s3");
    T(eos_queue_count(q)==3,"cnt=3");msg_t pk;T(eos_queue_peek(q,&pk)==EOS_KERN_OK,"peek");T(pk.id==1,"peek=1st");
    msg_t o;T(eos_queue_receive(q,&o,0)==EOS_KERN_OK,"r1");T(o.id==1&&strcmp(o.t,"A")==0,"msg1 ok");
    T(eos_queue_receive(q,&o,0)==EOS_KERN_OK,"r2");T(o.id==2,"msg2");
    T(eos_queue_receive(q,&o,0)==EOS_KERN_OK,"r3");T(o.id==3,"msg3");
    T(eos_queue_is_empty(q),"drained");T(eos_queue_receive(q,&o,0)==EOS_KERN_EMPTY,"empty recv");
    msg_t f={99,"X"};for(int i=0;i<4;i++)eos_queue_send(q,&f,0);
    T(eos_queue_is_full(q),"full");T(eos_queue_send(q,&f,0)==EOS_KERN_FULL,"full send");
    eos_queue_delete(q);printf("  [PASS] queue: 17 checks\n");return 0;}
static int tcount=0;
static void tcb(eos_swtimer_handle_t h,void*c){(void)h;(*(int*)c)++;}
static int test_tmr(void){
    printf("\n-- Timer --\n");tcount=0;
    eos_swtimer_handle_t t;T(eos_swtimer_create(&t,"t",5,false,tcb,&tcount)==EOS_KERN_OK,"created");
    eos_swtimer_start(t);for(int i=0;i<10;i++){eos_kernel_tick();eos_swtimer_tick(eos_tick_get());}
    T(tcount==1,"oneshot");
    for(int i=0;i<10;i++){eos_kernel_tick();eos_swtimer_tick(eos_tick_get());}T(tcount==1,"no refire");
    int rc=0;eos_swtimer_handle_t t2;eos_swtimer_create(&t2,"r",3,true,tcb,&rc);eos_swtimer_start(t2);
    for(int i=0;i<15;i++){eos_kernel_tick();eos_swtimer_tick(eos_tick_get());}T(rc>=4,"reload fires");
    eos_swtimer_stop(t2);int fr=rc;for(int i=0;i<10;i++){eos_kernel_tick();eos_swtimer_tick(eos_tick_get());}
    T(rc==fr,"stopped");eos_swtimer_delete(t);eos_swtimer_delete(t2);
    printf("  [PASS] timer: 5 checks\n");return 0;}
static int test_lc(void){
    printf("\n-- Lifecycle --\n");uint8_t h=(uint8_t)gb;
    T(eos_task_suspend(h)==EOS_KERN_OK,"suspend");T(eos_task_get_state(h)==EOS_TASK_SUSPENDED,"SUSP");
    T(eos_task_resume(h)==EOS_KERN_OK,"resume");T(eos_task_get_state(h)==EOS_TASK_READY,"READY");
    T(eos_task_delete(h)==EOS_KERN_OK,"delete");T(eos_task_get_state(h)==EOS_TASK_DELETED,"DEL");
    T(eos_task_delete(h)==EOS_KERN_INVALID,"double-del guard");
    printf("  [PASS] lifecycle: 7 checks\n");return 0;}
static uint8_t ha[2048] __attribute__((aligned(8)));
static int test_heap(void){
    printf("\n-- Heap --\n");T(eos_heap_init(ha,sizeof(ha))==0,"init");
    void*p1=eos_malloc(64);T(p1!=NULL,"m64");T(((uintptr_t)p1&7)==0,"align");
    void*p2=eos_malloc(128);T(p2!=NULL,"m128");T(p1!=p2,"no overlap");
    eos_free(p1);eos_free(p2);void*p3=eos_malloc(192);T(p3!=NULL,"coalesce");
    eos_free(p3);uint8_t*zp=(uint8_t*)eos_calloc(16,4);T(zp!=NULL,"calloc");
    int ok=1;for(int i=0;i<64;i++)if(zp[i]!=0)ok=0;T(ok,"zeroed");eos_free(zp);
    void*big=eos_malloc(sizeof(ha));T(big==NULL,"exhaust");eos_free(NULL);
    printf("  [PASS] heap: 9 checks\n");return 0;}
int main(void){
    printf("================================================\n");
    printf("  EoS E2E: boot->kernel->task->sync->IPC\n");
    printf("================================================\n");
    int f=0;
    f|=test_boot();f|=test_kinit();f|=test_tasks();f|=test_acc();f|=test_tick();
    f|=test_mtx();f|=test_sem();f|=test_q();f|=test_tmr();f|=test_lc();f|=test_heap();
    printf("\n================================================\n");
    if(f)printf("  FAILED (%d/%d)\n",tp,tr);
    else printf("  ALL %d CHECKS PASSED\n",tp);
    printf("================================================\n");
    return f;}
