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
Transaction *current_tx = NULL;
extern int verbose;

/* abort helper */
void abort_transaction(Transaction *tx, const char *reason)
{
    if (!tx)
        return; // null check

    tx->status = TX_ABORTED;
    tx->actual_end = global_tick;
    if (reason)
        fprintf(stderr, "[tx] ABORT T%d: %s\n", tx->tx_id, reason);
}

// run transactions
void *execute_transaction(void *arg)
{
    // null check
    if (!arg)
        return NULL;

    Transaction *tx = (Transaction *)arg;
    current_tx = tx;   

    wait_until_tick(tx->start_tick);
    tx->actual_start = global_tick;
    tx->status = TX_RUNNING;

    for (int i = 0; i < tx->num_ops; i++)
    {
        Operation *op = &tx->ops[i];

        switch (op->type)
        {
        case OP_DEPOSIT:

            if (verbose){
                printf("  T%d started: DEPOSIT account %d amount PHP %d.%02d\n",
                    tx->tx_id,
                    op->account_id,
                    op->amount_centavos / 100,
                    op->amount_centavos % 100);
            }
            wait_until_tick(global_tick + 1);
            pthread_mutex_lock(&tick_lock);
            pthread_mutex_unlock(&tick_lock);

            {
                Account *acc = bank_find_account(g_bank, op->account_id);
                if (!acc)
                {
                    abort_transaction(tx, "DEPOSIT account not found");
                    return NULL;
                }

                buffer_pool_load(g_pool, op->account_id, acc);
                deposit(op->account_id, op->amount_centavos);
                buffer_pool_unload(g_pool, op->account_id);
                
                if (verbose){
                    printf("  T%d completed: DEPOSIT successful\n", tx->tx_id);
                }
            }

            // {
            //     Account *acc = bank_find_account(g_bank, op->account_id);
            //     buffer_pool_load(g_pool, op->account_id, acc);
            //     //deposit(op->account_id, op->amount_centavos);
            //     buffer_pool_unload(g_pool, op->account_id);
            //     break;
            // }
        break;

        case OP_WITHDRAW:
            
            if (verbose){
                printf("  T%d started: WITHDRAW account %d amount PHP %d.%02d\n",
                    tx->tx_id,
                    op->account_id,
                    op->amount_centavos / 100,
                    op->amount_centavos % 100);
            }
            
            pthread_mutex_lock(&tick_lock);
            pthread_mutex_unlock(&tick_lock);

            {
                Account *acc = bank_find_account(g_bank, op->account_id);
                if (!acc)
                {
                    abort_transaction(tx, "WITHDRAW account not found");
                    return NULL;
                }

                buffer_pool_load(g_pool, op->account_id, acc);
                if (!withdraw(op->account_id, op->amount_centavos))
                {
                    buffer_pool_unload(g_pool, op->account_id);
                    abort_transaction(tx, "WITHDRAW insufficient funds");
                    return NULL;
                }
                wait_until_tick(global_tick + 1);
                buffer_pool_unload(g_pool, op->account_id);
                if (verbose){
                    printf("  T%d completed: WITHDRAW successful\n", tx->tx_id);
                }
            }

            break;

        case OP_TRANSFER:
        {
            Account *src = bank_find_account(g_bank, op->account_id);
            Account *dst = bank_find_account(g_bank, op->target_account);

            if (!src || !dst) {
                abort_transaction(tx, "TRANSFER account(s) not found");
                return NULL;
            }

            if (verbose){
                printf("  T%d started: TRANSFER from %d to %d amount PHP %d.%02d\n",
                    tx->tx_id, op->account_id, op->target_account,
                    op->amount_centavos / 100, op->amount_centavos % 100);
            }
            wait_until_tick(global_tick + 1);

            buffer_pool_load(g_pool, op->account_id, src);
            buffer_pool_load(g_pool, op->target_account, dst);

            if (!transfer(op->account_id, op->target_account, op->amount_centavos, tx)) {
                buffer_pool_unload(g_pool, op->account_id);
                buffer_pool_unload(g_pool, op->target_account);
                abort_transaction(tx, "TRANSFER failed");
                return NULL;
            }

            if (verbose){

                printf("  T%d acquired locks on accounts %d\n",
                    tx->tx_id, op->account_id);
                printf("  [DEADLOCK PREVENTED] Lock ordering: T%d waiting for account %d\n",
                    tx->tx_id, op->target_account);
            }
            buffer_pool_unload(g_pool, op->account_id);
            buffer_pool_unload(g_pool, op->target_account);

            wait_until_tick(global_tick + 1);   // delay completion

            if (verbose){
                printf("  T%d completed: TRANSFER successful\n", tx->tx_id);
            }
            break;
        }
        case OP_BALANCE:

            if (verbose){
                printf("  T%d started: BALANCE account %d\n",
                    tx->tx_id,
                    op->account_id);
                }

            {
                Account *acc = bank_find_account(g_bank, op->account_id);
                if (!acc)
                {
                    abort_transaction(tx, "BALANCE account not found");
                    return NULL;
                }

                buffer_pool_load(g_pool, op->account_id, acc);
                //metrics.total_loads++;
                int balance = get_balance(op->account_id);

                if (verbose){
                    printf("  T%d: Account %d balance = PHP %d.%02d\n",
                        tx->tx_id,
                        op->account_id,
                        balance / 100,
                        balance % 100);
                }
                buffer_pool_unload(g_pool, op->account_id);
            }
        }

    }

    tx->actual_end = global_tick;
    tx->status = TX_COMMITTED;
    current_tx = NULL; // <— reset
    return NULL;
}
