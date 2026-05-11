#include "transaction.h"
#include "metrics.h"
#include "lock_mgr.h"
#include "bank.h"
#include "buffer_pool.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>

extern Bank *g_bank;
extern BufferPool *g_pool;

/* run transactions */
void *execute_transaction(void *arg)
{
    Transaction *tx = (Transaction *)arg;

    wait_until_tick(tx->start_tick);
    tx->actual_start = global_tick;
    tx->status = TX_RUNNING;

    for (int i = 0; i < tx->num_ops; i++)
    {
        Operation *op = &tx->ops[i];
        int tick_before = global_tick;

        switch (op->type)
        {
        case OP_DEPOSIT:

            printf("  T%d started: DEPOSIT account %d amount PHP %d.%02d\n",
                   tx->tx_id,
                   op->account_id,
                   op->amount_centavos / 100,
                   op->amount_centavos % 100);

            wait_until_tick(global_tick + 1);
            pthread_mutex_lock(&tick_lock);
            pthread_mutex_unlock(&tick_lock);

            {
                Account *acc = bank_find_account(g_bank, op->account_id);
                if (!acc)
                {
                    fprintf(stderr, "[tx] ERROR: DEPOSIT account %d not found\n", op->account_id);
                    tx->status = TX_ABORTED;
                    tx->actual_end = global_tick;
                    return NULL;
                }

                buffer_pool_load(g_pool, op->account_id, acc);
                deposit(op->account_id, op->amount_centavos);
                buffer_pool_unload(g_pool, op->account_id);
            }

            printf("  T%d completed: DEPOSIT successful\n", tx->tx_id);

        {
            Account *acc = bank_find_account(g_bank, op->account_id);
            buffer_pool_load(g_pool, op->account_id, acc);
            deposit(op->account_id, op->amount_centavos);
            buffer_pool_unload(g_pool, op->account_id);
            break;
        }

        case OP_WITHDRAW:

            printf("  T%d started: WITHDRAW account %d amount PHP %d.%02d\n",
                   tx->tx_id,
                   op->account_id,
                   op->amount_centavos / 100,
                   op->amount_centavos % 100);

            wait_until_tick(global_tick + 1);
            pthread_mutex_lock(&tick_lock);
            pthread_mutex_unlock(&tick_lock);

            {
                Account *acc = bank_find_account(g_bank, op->account_id);
                if (!acc)
                {
                    fprintf(stderr, "[tx] ERROR: WITHDRAW account %d not found\n", op->account_id);
                    tx->status = TX_ABORTED;
                    tx->actual_end = global_tick;
                    return NULL;
                }

                buffer_pool_load(g_pool, op->account_id, acc);
                if (!withdraw(op->account_id, op->amount_centavos))
                {
                    tx->status = TX_ABORTED;
                    buffer_pool_unload(g_pool, op->account_id);
                    tx->actual_end = global_tick;
                    return NULL;
                }
                buffer_pool_unload(g_pool, op->account_id);
            }

            printf("  T%d completed: WITHDRAW successful\n", tx->tx_id);
        {
            Account *acc = bank_find_account(g_bank, op->account_id);
            buffer_pool_load(g_pool, op->account_id, acc);
            if (!withdraw(op->account_id, op->amount_centavos))
            {
                tx->status = TX_ABORTED;
                buffer_pool_unload(g_pool, op->account_id);
                tx->actual_end = global_tick;
                return NULL;
            }
            buffer_pool_unload(g_pool, op->account_id);
            break;
        }

        case OP_TRANSFER:
        {
            Account *src = bank_find_account(g_bank, op->account_id);
            Account *dst = bank_find_account(g_bank, op->target_account);

            printf("  T%d started: TRANSFER from %d to %d amount PHP %d.%02d\n",
                   tx->tx_id,
                   op->account_id,
                   op->target_account,
                   op->amount_centavos / 100,
                   op->amount_centavos % 100);

            wait_until_tick(global_tick + 1);
            pthread_mutex_lock(&tick_lock);
            pthread_mutex_unlock(&tick_lock);

            buffer_pool_load(g_pool, op->account_id, src);
            buffer_pool_load(g_pool, op->target_account, dst);

            printf("  T%d acquired lock on account %d\n",
                   tx->tx_id,
                   op->account_id);

            printf("  [DEADLOCK PREVENTED] Lock ordering: T%d waiting for account %d\n",
                   tx->tx_id,
                   op->target_account);

            if (!transfer(op->account_id,
                          op->target_account,
                          op->amount_centavos))
            if (!transfer(op->account_id, op->target_account,
                          op->amount_centavos))
            {
                tx->status = TX_ABORTED;
                buffer_pool_unload(g_pool, op->account_id);
                buffer_pool_unload(g_pool, op->target_account);
                tx->actual_end = global_tick;
                return NULL;
            }
            buffer_pool_unload(g_pool, op->account_id);
            buffer_pool_unload(g_pool, op->target_account);

            printf("  T%d completed: TRANSFER successful\n",
                   tx->tx_id);

            release_two_locks(src, dst);

            break;
        }

        case OP_BALANCE:

            printf("  T%d started: BALANCE account %d\n",
                   tx->tx_id,
                   op->account_id);

            {
                Account *acc = bank_find_account(g_bank, op->account_id);
                if (!acc)
                {
                    fprintf(stderr, "[tx] ERROR: BALANCE account %d not found\n", op->account_id);
                    tx->status = TX_ABORTED;
                    tx->actual_end = global_tick;
                    return NULL;
                }

                buffer_pool_load(g_pool, op->account_id, acc);
                metrics.total_loads++;
                int balance = get_balance(op->account_id);
                printf("  T%d: Account %d balance = PHP %d.%02d\n",
                       tx->tx_id,
                       op->account_id,
                       balance / 100,
                       balance % 100);
                buffer_pool_unload(g_pool, op->account_id);
            }

        {
            Account *acc = bank_find_account(g_bank, op->account_id);
            buffer_pool_load(g_pool, op->account_id, acc);
            int balance = get_balance(op->account_id);
            printf("T%d: Account %d balance = PHP %d.%02d\n",
                   tx->tx_id, op->account_id,
                   balance / 100, balance % 100);
            buffer_pool_unload(g_pool, op->account_id);
            break;
        }
        }

        tx->wait_ticks += (global_tick - tick_before);
    }

    tx->actual_end = global_tick;
    tx->status = TX_COMMITTED;
    return NULL;
}

/* trace parser */
int parse_operation(const char *line, Operation *op)
{
    char type[16];
    if (sscanf(line, "%15s", type) != 1)
        return -1;
    if (type[0] == '#')
        return -1;

    if (strcmp(type, "DEPOSIT") == 0)
    {
        op->type = OP_DEPOSIT;
        op->target_account = -1;
        return (sscanf(line, "%*s %d %d",
                       &op->account_id,
                       &op->amount_centavos) == 2)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "WITHDRAW") == 0)
    {
        op->type = OP_WITHDRAW;
        op->target_account = -1;
        return (sscanf(line, "%*s %d %d",
                       &op->account_id,
                       &op->amount_centavos) == 2)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "BALANCE") == 0)
    {
        op->type = OP_BALANCE;
        op->target_account = -1;
        op->amount_centavos = 0;
        return (sscanf(line, "%*s %d",
                       &op->account_id) == 1)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "TRANSFER") == 0)
    {
        op->type = OP_TRANSFER;
        return (sscanf(line, "%*s %d %d %d",
                       &op->account_id,
                       &op->target_account,
                       &op->amount_centavos) == 3)
                   ? 0
                   : -1;
    }

    fprintf(stderr, "[parser] unknown operation: %s\n", type);
    return -1;
}