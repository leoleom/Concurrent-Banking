#include "bank.h"
#include <stdio.h>
#include <string.h>

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