#include "bank.h"
#include "lock_mgr.h"
#include "transaction.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* set by main() before any threads are spawned */
extern Bank *g_bank;

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

int bank_add_account(Bank *bank, int account_id, int balance_centavos)
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
    // null check
    if (!bank)
        return NULL;

    for (int i = 0; i < bank->num_accounts; i++)
    {
        if (bank->accounts[i].account_id == account_id)
            return &bank->accounts[i];
    }
    return NULL;
}

void bank_destroy(Bank *bank)
{
    // null check
    if (!bank)
        return;

    for (int i = 0; i < bank->num_accounts; i++)
        pthread_rwlock_destroy(&bank->accounts[i].lock);
    pthread_mutex_destroy(&bank->bank_lock);
}

/* helpers functions */
int get_balance(int account_id)
{
    Account *acc = bank_find_account(g_bank, account_id);

    if (!acc)
    {
        fprintf(stderr, "[bank] ERROR: balance: account %d not found\n", account_id);
        return -1;
    }

    pthread_rwlock_rdlock(&acc->lock);
    int balance = acc->balance_centavos;
    pthread_rwlock_unlock(&acc->lock);

    return balance;
}


void deposit(int account_id, int amount_centavos)
{
    Account *acc = bank_find_account(g_bank, account_id);
    if (!acc) {
        fprintf(stderr, "[bank] ERROR: deposit: account %d not found\n", account_id);
        return;
    }

    int tick_before = global_tick; // record tick before lock attempt
    pthread_rwlock_wrlock(&acc->lock);
    int tick_after = global_tick;  // record tick after lock acquired
    int wait_ticks = tick_after - tick_before;

    // Perform operation
    acc->balance_centavos += amount_centavos;

    pthread_rwlock_unlock(&acc->lock);

    //accumulate wait time into metrics
    current_tx->wait_ticks += wait_ticks;
}

int withdraw(int account_id, int amount_centavos)
{
    Account *acc = bank_find_account(g_bank, account_id);
    if (!acc) {
        fprintf(stderr, "[bank] ERROR: withdraw: account %d not found\n", account_id);
        return false;
    }

    int tick_before = global_tick;
    pthread_rwlock_wrlock(&acc->lock);
    int tick_after = global_tick;
    int wait_ticks = tick_after - tick_before;

    bool success = false;
    if (acc->balance_centavos >= amount_centavos) {
        acc->balance_centavos -= amount_centavos;
        success = true;
    }

    pthread_rwlock_unlock(&acc->lock);
    if (current_tx) current_tx->wait_ticks += wait_ticks;

    return success;
}

int transfer(int src_id, int dst_id, int amount_centavos, Transaction *tx)
{
    Account *src = bank_find_account(g_bank, src_id);
    Account *dst = bank_find_account(g_bank, dst_id);

    if (!src || !dst) {
        fprintf(stderr, "[bank] ERROR: transfer: invalid account(s)\n");
        return 0;
    }

    acquire_locks_ordered(src, dst, tx);

    int success = 0;
    if (src->balance_centavos >= amount_centavos) {
        src->balance_centavos -= amount_centavos;
        dst->balance_centavos += amount_centavos;
        success = 1;
    }

    release_two_locks(src, dst);
    return success;
}