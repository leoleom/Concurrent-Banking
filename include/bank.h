#ifndef BANK_H
#define BANK_H

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

#define MAX_ACCOUNTS 100

typedef struct
{
    int account_id;        // Account number
    int balance_centavos;  // Balance in centavos
    pthread_rwlock_t lock; // Per-account lock
} Account;

typedef struct
{
    Account accounts[MAX_ACCOUNTS];
    int num_accounts;
    pthread_mutex_t bank_lock; // Protects bank metadata
} Bank;

/* Prototypes */
void bank_init(Bank *bank);
void bank_destroy(Bank *bank);
int bank_add_account(Bank *bank, int account_id, int balance_centavos);
Account *bank_find_account(Bank *bank, int account_id);

void deposit(int account_id, int amount_centavos);
bool withdraw(int account_id, int amount_centavos);
bool transfer(int from_id, int to_id, int amount_centavos);
int get_balance(int account_id);

#endif