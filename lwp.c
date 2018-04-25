/*=============================================================================
 *   Assignment:  Lightweight processes
 *
 *       Author:  Caitlin Settles
 *        Class:  CSC 453 Section 01
 *     Due Date:  04/25/18
 *
 *-----------------------------------------------------------------------------
 *
 *  Description:  An implementation of linux's lightweight processes
 *
 *        Input:  A function to be run by each thread, its argument, and
 *                the stack size of each thread.
 *                OPTIONAL: a customized scheduler for the threads
 *
 *===========================================================================*/

#include "lwp.h"

static struct scheduler rr_publish = {NULL, NULL, rr_admit, rr_remove, rr_next};
scheduler sched = &rr_publish; /* swappable scheduler */

rfile ctx; /* saves program context before threads are run */

tid_t count = 1; /* keeps track of latest thread id */

thread lwps = NULL; /* double linked list of every existing thread */
thread curr = NULL; /* pointer to thread currently executing */

/**
 * Sets up a stack frame for an individual lightweight process.
 *
 * @param func the thread function to run
 * @param arg the argument to the thread function
 * @param stack_size the size of the "stack" in words
 * @return the id number of the created lwp thread
 */
tid_t lwp_create(lwpfun func, void *arg, size_t stack_size) {
    thread tmp;
    unsigned long *stack;

    tmp = malloc(sizeof(context));
    if (!tmp) {
        perror("lwp_create");
        return (tid_t)-1;
    }

    /* pointer to base of stack */
    tmp->stack = (unsigned long *)malloc(stack_size * WORD_SIZE);
    if (!tmp->stack) {
        perror("lwp_create");
        return (tid_t)-1;
    }

    tmp->stacksize = stack_size * WORD_SIZE; /* stack size in bytes */
    tmp->tid = count++;

    /* put stack pointer at highest memory region */
    stack = (tid_t *)((tid_t)tmp->stack + (tid_t)tmp->stacksize - 1);

    /* build stack frame towards lower memory regions */
    stack[0] = (unsigned long)lwp_exit;
    stack[-1] = (unsigned long)func; /* will become rip */
    stack[-2] = (unsigned long)stack; /* will become rbp */

    /* stack - 2 is the address of where we want our stack to grow from */
    tmp->state.rbp = (tid_t)(stack - 2); /* will become rsp */
    tmp->state.rdi = (uintptr_t)arg; /* argument to thread func */
    tmp->state.fxsave = FPU_INIT;

    /* initialize links to NULL */
    tmp->prev = NULL;
    tmp->next = NULL;
    tmp->right = NULL;
    tmp->left = NULL;

    /* schedule it to be run */
    sched->admit(tmp);

    /* add to internal list to keep track */
    if (!lwps) {
        lwps = tmp;
    } else {
        lwps->left = tmp;
        tmp->right = lwps;
        lwps = tmp;
    }

    return tmp->tid;
}

/**
 * Terminates the current LWP and frees its resources. Calls
 * sched->next() to get the next thread. If there are no other
 * threads, restores the original system thread.
 */
void  lwp_exit(void) {
    if (curr) {
        sched->remove(curr); /* remove thread from scheduler */
        SetSP(ctx.rsp); /* set pointer to safe memory */
        exit_thread(); /* this stack frame will build in safe memory region */
    }
    /* if nothing is running, just restore original context */
    load_context(&ctx); /* probably won't ever execute, but it's here
                           as a safeguard */
}

/**
 * Helper function to lwp_exit(). Frees "stack" memory malloced()
 * in the thread, as well as the thread itself, and schedules another
 * thread to be run. If there are no other threads, it returns to
 * the original context.
 *
 * It is necessary to have this helper function because when
 * lwp_exit() is called, our stack frame is on the stack we
 * malloced() for the current thread. We cannot free the thread's
 * stack because it is memory we are currently executing on. Therefore,
 * we must move the stack to a "safe" place (right below where
 * it originally was) and THEN free the thread's stack.
 */
void exit_thread(void) {
    free(curr->stack);
    free(curr);
    curr = sched->next();

    if (!curr) {
        /* threads are done executing, go back to original context */
        load_context(&ctx);
        return; /* should never reach here */
    }

    load_context(&curr->state); /* don't need to save old context */
}

/**
 * Starts the LWP system. Saves the original context (for lwp stop()
 * to use later), picks a LWP and starts it running. If there are no
 * LWPs, returns immediately.
 */
void  lwp_start(void) {
    if (!curr && count == 1){
        /* start() was called without any created threads */
        return; /* do nothing */
    }
    curr = sched->next();
    if (!curr) {
        load_context(&ctx); /* return to original context */
        return; /* should never reach here */
    }
    /* save original context and switch to thread context */
    swap_rfiles(&ctx, &curr->state);
}

/**
 * Yields control to another LWP. Which one depends on the scheduler.
 * Saves the current LWP’s context, picks the next one, restores
 * that thread’s context, and returns.
 */
void  lwp_yield(void) {
    thread tmp;

    tmp = curr;
    curr = sched->next();
    if (!curr) {
        load_context(&ctx); /* return to original context */
        return; /* should never reach here */
    }

    /* save previously executing thread's context,
     * and switch to currently executing thread's context */
    swap_rfiles(&tmp->state, &curr->state);
}

/**
 * Stops the LWP system, restores the original stack pointer and returns
 * to that context. (Wherever lwp start() was called from.
 * lwp stop() does not destroy any existing contexts, and thread
 * processing will be restarted by a call to lwp start().
 */
void  lwp_stop(void) {
    if (!curr) {
        load_context(&ctx);
        return;
    }
    /* save currently executing thread's context and then
     * restore original context */
    swap_rfiles(&curr->state, &ctx);
}

/**
 * @return the tid of the calling LWP or NO THREAD if not called by a
 *         LWP
 */
tid_t lwp_gettid(void) {
    if (!curr) {
        return NO_THREAD;
    }
    return curr->tid;
}

/**
 * Returns the thread corresponding to the given thread ID, or NULL
 * if the ID is invalid.
 *
 * @param tid the ID of the thread
 * @return the thread whose tid matches the given tid
 */
thread tid2thread(tid_t tid) {
    thread tmp;

    tmp = lwps;
    while (tmp) {
        if (tmp->tid == tid) {
            /* we have a match */
            return tmp;
        }
        tmp = tmp->right;
    }

    return NULL; /* thread not found */
}

/**
 * Causes the LWP package to use the given scheduler to choose the
 * next process to run. Transfers all threads from the old scheduler
 * to the new one in next() order. If scheduler is NULL the library
 * should return to round-robin scheduling.
 *
 * @param s a set of functions to schedule threads
 */
void lwp_set_scheduler(scheduler s) {
    thread temp;

    if (s->init) { /* check if init is NULL */
        s->init();
    }

    while ((temp = sched->next())) {
        /* we must remove thread from the current scheduler
         * before we can add it to the new one! */
        sched->remove(temp);
        s->admit(temp);
    }

    if (sched->shutdown) { /* check if shutdown is NULL */
        sched->shutdown();
    }

    sched = s;
}

/**
 * Returns a pointer to the current scheduler.
 *
 * @return pointer to current scheduler
 */
scheduler lwp_get_scheduler(void) {
    return sched;
}
