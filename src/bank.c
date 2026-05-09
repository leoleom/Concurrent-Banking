#include "bank.h"
#include "buffer_pool.h"
#include "lock_mgr.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "transaction.h"

/* set by main() before any threads are spawned */
extern Bank *g_bank;
extern BufferPool *g_pool;

void bank_init(Bank *bank)
{
    memset(bank, 0, sizeof(Bank));
    pthread_mutex_init(&bank->bank_lock, NULL);
    for (int i = 0; i < MAX_ACCOUNTS; i++)
    {
        bank->accounts[i].account_id = -1;
        bank->accounts[i].balance_centavos = 0;
        pthread_rwlock_init(&bank->accounts[i].lock, NULL);
    }
}

int bank_add_account(Bank *bank, int account_id, int64_t balance_centavos)
{
    pthread_mutex_lock(&bank->bank_lock);

    if (bank->num_accounts >= MAX_ACCOUNTS)
    {
        pthread_mutex_unlock(&bank->bank_lock);
        fprintf(stderr, "[bank] ERROR: max accounts reached\n");
        return -1;
    }
    for (int i = 0; i < bank->num_accounts; i++)
    {
        if (bank->accounts[i].account_id == account_id)
        {
            pthread_mutex_unlock(&bank->bank_lock);
            fprintf(stderr, "[bank] ERROR: duplicate account %d\n", account_id);
            return -1;
        }
    }

    Account *a = &bank->accounts[bank->num_accounts++];
    a->account_id = account_id;
    a->balance_centavos = balance_centavos;

    pthread_mutex_unlock(&bank->bank_lock);
    return 0;
}

Account *bank_find_account(Bank *bank, int account_id)
{
    for (int i = 0; i < bank->num_accounts; i++)
    {
        if (bank->accounts[i].account_id == account_id)
            return &bank->accounts[i];
    }
    return NULL;
}

void bank_destroy(Bank *bank)
{
    for (int i = 0; i < bank->num_accounts; i++)
        pthread_rwlock_destroy(&bank->accounts[i].lock);
    pthread_mutex_destroy(&bank->bank_lock);
}



/* helpers functions */
int get_balance(int account_id) {
    Account* acc = &g_bank->accounts[account_id];
    
    pthread_rwlock_rdlock(&acc->lock);
    int balance = acc->balance_centavos;
    pthread_rwlock_unlock(&acc->lock);
    
    return balance;
}

void deposit(int account_id, int amount_centavos) {
    Account* acc = &g_bank->accounts[account_id];
    
    pthread_rwlock_wrlock(&acc->lock);
    acc->balance_centavos += amount_centavos;
    pthread_rwlock_unlock(&acc->lock);
}

bool withdraw(int account_id, int amount_centavos) {
    Account* acc = &g_bank->accounts[account_id];
    
    pthread_rwlock_wrlock(&acc->lock);
    
    if (acc->balance_centavos < amount_centavos) {
        pthread_rwlock_unlock(&acc->lock);
        return false;  // Insufficient funds
    }
    
    acc->balance_centavos -= amount_centavos;
    pthread_rwlock_unlock(&acc->lock);
    return true;
}

bool transfer(int from_id, int to_id, int amount_centavos) {
    // Acquire locks in consistent order to prevent deadlock
    int first = (from_id < to_id) ? from_id : to_id;
    int second = (from_id < to_id) ? to_id : from_id;
    
    Account* acc_first = &g_bank->accounts[first];
    Account* acc_second = &g_bank->accounts[second];
    
    pthread_rwlock_wrlock(&acc_first->lock);
    pthread_rwlock_wrlock(&acc_second->lock);
    
    // Check sufficient funds
    Account* from_acc = &g_bank->accounts[from_id];
    if (from_acc->balance_centavos < amount_centavos) {
        pthread_rwlock_unlock(&acc_second->lock);
        pthread_rwlock_unlock(&acc_first->lock);
        return false;
    }
    
    // Perform transfer
    g_bank->accounts[from_id].balance_centavos -= amount_centavos;
    g_bank->accounts[to_id].balance_centavos += amount_centavos;
    
    pthread_rwlock_unlock(&acc_second->lock);
    pthread_rwlock_unlock(&acc_first->lock);
    return true;
}

