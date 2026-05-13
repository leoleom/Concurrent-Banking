#ifndef LOCK_MGR_H
#define LOCK_MGR_H
 
#include "bank.h"
#include "transaction.h"
 
/*
 * Deadlock prevention (Strategy A):
 * Always acquire write locks in ascending account_id order.
 */

void lock_single_account(Account *acc);
void unlock_single_account (Account *acc);
void acquire_locks_ordered(Account *a, Account *b, Transaction * tx);
void release_two_locks(Account *a, Account *b);
 
#endif 
 