#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");

static int pid = 0;
module_param(pid, int, 0);

static int vmaCounter = 0;

struct task_struct* task;
struct mm_struct* mm;
struct vm_area_struct *vma;

ktime_t currtime , interval;
unsigned long timerInterval = 10e9;
unsigned long wCounter = 0;
unsigned long rCounter = 0;
unsigned long sCounter = 0;
unsigned long i = 0;

static struct hrtimer restartTimer;

int testP(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {
    int r = 0;
    if (pte_young(*ptep)) {
        r = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *)&ptep->pte);
    }
    return r;
}

void check_and_update_counters(struct mm_struct* mm, unsigned long start, unsigned long end) {
    unsigned long i;
    pgd_t* pgd;
    p4d_t* p4d;
    pud_t* pud;
    pmd_t* pmd;
    pte_t* ptep, pte;

    for (i = start; i < end; i += PAGE_SIZE) {
        pgd = pgd_offset(mm, i);
        if (!pgd_none(*pgd) && !pgd_bad(*pgd)) {
            p4d = p4d_offset(pgd, i);
            if (!p4d_none(*p4d) && !p4d_bad(*p4d)) {
                pud = pud_offset(p4d, i);
                if (!pud_none(*pud) && !pud_bad(*pud)) {
                    pmd = pmd_offset(pud, i);
                    if (!pmd_none(*pmd) && !pmd_bad(*pmd)) {
                        ptep = pte_offset_map(pmd, i);
                        if (ptep) {
                            pte = *ptep;
                            if (pte_present(pte)) {
                                rCounter += 4;
                                if (testP(vma, i, ptep) == 1) {
                                    wCounter += 4;
                                }
                            } else {
                                sCounter += 4;
                            }
                        }
                    }
                }
            }
        }
    }
}

enum hrtimer_restart timer_function(struct hrtimer* in, enum hrtimer_restart restart_type) {
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    mm = task->mm;
    if (mm != NULL) {
        for (vma = mm->mmap; vma; vma = vma->vm_next) {
            check_and_update_counters(mm, vma->vm_start, vma->vm_end);
            vmaCounter++;
        }
    }
    printk("PID %d : RSS = %lu KB, SWAP = %lu KB, WSS = %lu KB", pid, rCounter, sCounter, wCounter);
    rCounter = sCounter = wCounter = 0;
    
    if (restart_type == HRTIMER_RESTART) {
        currtime = ktime_get();
        interval = ktime_set(0, timerInterval);
        hrtimer_forward(in, currtime, interval);
    }
    
    return restart_type;
}

enum hrtimer_restart restart(struct hrtimer* in) {
    return timer_function(in, HRTIMER_RESTART);
}

enum hrtimer_restart noRestart(struct hrtimer* in) {
    return timer_function(in, HRTIMER_NORESTART);
}

static void startTimer(void) {
    interval = ktime_set(0, timerInterval);
    hrtimer_init(&restartTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    restartTimer.function = &restart;
    hrtimer_start(&restartTimer, interval, HRTIMER_MODE_REL);
}

static int __init initial(void) {
    printk("inserting the module for project 3");
    startTimer();
    return(0);
}

static void __exit term(void) {
    printk("removing the module for project 3\n");
    hrtimer_cancel(&restartTimer);
}

module_init(initial);
module_exit(term);
