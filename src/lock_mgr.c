#include "lock_mgr.h"

void lock_single_account(Account *a) {
    pthread_rwlock_wrlock(&a->lock);
}

void unlock_single_account(Account *a) {
    pthread_rwlock_unlock(&a->lock);
}

void acquire_locks_ordered(Account *a, Account *b)
{
    /* Always lock the lower account_id first — breaks circular wait */
    Account *first  = (a->account_id < b->account_id) ? a : b;
    Account *second = (a->account_id < b->account_id) ? b : a;

    pthread_rwlock_wrlock(&first->lock);
    pthread_rwlock_wrlock(&second->lock);
}

void release_two_locks(Account *a, Account *b)
{
    pthread_rwlock_unlock(&a->lock);
    pthread_rwlock_unlock(&b->lock);
}