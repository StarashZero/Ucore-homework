#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>

static int now_queue;                       //running queue number
static int new_queue;                       //will run queue number

static void
Mutilevel_init(struct run_queue *rq) {
    list_init(&(rq->run_list));
    rq->proc_num = 0;
    now_queue = new_queue = 0;
}

static void
Mutilevel_enqueue(struct run_queue *rq[6], struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));
    list_add_before(&(rq[proc->queue_num]->run_list), &(proc->run_link));
    if (proc->time_slice == 0 || proc->time_slice > rq[proc->queue_num]->max_time_slice) {
        proc->time_slice = rq[proc->queue_num]->max_time_slice;
    }
    proc->rq = rq[proc->queue_num];
    rq[proc->queue_num]->proc_num ++;
    if(proc->queue_num == 0)
        new_queue = 0;
}

static void
Mutilevel_dequeue(struct run_queue *rq[6], struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq[now_queue]);
    list_del_init(&(proc->run_link));
    rq[now_queue]->proc_num --;
}

static struct proc_struct *
Mutilevel_pick_next(struct run_queue *rq[6]) {
    list_entry_t *le;
    while (now_queue<6){
        le = list_next(&(rq[now_queue]->run_list));
        if (le != &(rq[now_queue]->run_list)) 
            return le2proc(le, run_link);
        now_queue++;
        new_queue = now_queue;
    }
    now_queue = 0;
    return NULL;
}

static void
Mutilevel_proc_tick(struct run_queue *rq[6], struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice --;
    }
    if (proc->time_slice == 0) {
        proc->queue_num = proc->queue_num==5?5:(proc->queue_num+1);
        proc->need_resched = 1;
    }
    if (now_queue>new_queue){
        now_queue = new_queue;
        proc->need_resched = 1;
    }
}

struct sched_class default_sched_class = {
    .name = "Mutilevel_scheduler",
    .init = Mutilevel_init,
    .enqueue = Mutilevel_enqueue,
    .dequeue = Mutilevel_dequeue,
    .pick_next = Mutilevel_pick_next,
    .proc_tick = Mutilevel_proc_tick,
};
