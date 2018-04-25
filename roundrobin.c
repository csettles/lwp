/*=============================================================================
 *
 *       Author:  Caitlin Settles
 *        Class:  CSC 453 Section 01
 *     Due Date:  04/25/18
 *
 *-----------------------------------------------------------------------------
 *
 *  Description:  An implementation of a round robin scheduler for lwps.
 *                Maintains a queue of schedules, where the top of the
 *                queue is the next thread to be run and the back is the
 *                most recently admitted thread.
 *
 *===========================================================================*/

#include "lwp.h"

/* scheduler maintains a queue of threads, with the top of the queue
 * being the next thread to run */
thread first = NULL;
thread last = NULL;

/**
 * Round robin algorithm to add a thread to the queue.
 * Assumes that thread->prev and thread->next are NULL.
 *
 * @param new the thread to be scheduled
 */
void rr_admit(thread new) {
    if (!first && !last) {
        /* queue is empty */
        first = new;
        last = new;
        return;
    }

    /* add to end of queue */
    last->next = new;
    new->prev = last;
    last = new;
}

/**
 * Round robin algorithm to remove a thread from the queue.
 * Starts its search at the end of the list to optimize for use
 * case (most of the time, sched->next() is called and THEN
 * the previously running thread is removed). This makes the
 * amortized run time O(1).
 *
 * @param victim thread to be unscheduled
 */
void rr_remove(thread victim) {
    thread tmp = last;
    while (tmp && tmp->tid != victim->tid) {
        tmp = tmp->prev;
    }

    if (tmp == NULL || tmp->tid != victim->tid) {
        /* no match found */
        return;
    }

    if (tmp->prev) {
        tmp->prev->next = tmp->next;
    } else {
        /* we were at the top of the queue */
        first = tmp->next;
    }

    if (tmp->next) {
        tmp->next->prev = tmp->prev;
    } else {
        /* we were at the bottom of the queue */
        last = tmp->prev;
    }
}

/**
 * Round robin algorithm to get the next thread to be run.
 *
 * @return the next thread to run
 */
thread rr_next(void) {
    thread tmp;

    if (!first) {
        /* no more threads in queue */
        return NULL;
    }

    /* otherwise, dequeue and reschedule */
    tmp = first;
    first = first->next;
    if (first) {
        first->prev = NULL; /* remove reference completely */
    } else {
        last = NULL; /* tmp was last thread in queue */
    }

    /* reset positions */
    tmp->next = NULL;
    tmp->prev = NULL;
    rr_admit(tmp);

    return tmp;
}
