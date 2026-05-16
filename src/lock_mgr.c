#include "lock_mgr.h"
#include "timer.h"
#include "transaction.h"
#include "metrics.h"
#include <pthread.h>
#include <stdio.h>

extern int verbose;

void acquire_locks_ordered(Account *a, Account *b, Transaction *tx)
{
    if (!a || !b || !tx)
        return;

    Account *first = (a->account_id < b->account_id) ? a : b;
    Account *second = (first == a) ? b : a;

    int tick_before = global_tick;

    pthread_rwlock_wrlock(&first->lock);

    tx->wait_ticks++; // increment wait tick if there is a lock acquisition

    pthread_rwlock_wrlock(&second->lock);

    int tick_after = global_tick;
    tx->wait_ticks += (tick_after - tick_before);
}

/* release both locks */
void release_two_locks(Account *a, Account *b)
{
    if (!a || !b)
        return; // null guard
    pthread_rwlock_unlock(&a->lock);
    pthread_rwlock_unlock(&b->lock);
}