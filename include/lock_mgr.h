#ifndef LOCK_MGR_H
#define LOCK_MGR_H
 
#include "bank.h"
 
/*
 * Deadlock prevention (Strategy A):
 * Always acquire write locks in ascending account_id order.
 */

void acquire_locks_ordered(Account *a, Account *b);
void release_two_locks(Account *a, Account *b);
 
#endif 
 