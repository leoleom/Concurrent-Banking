#include "transaction.h"
#include "bank.h"
#include "buffer_pool.h"
#include "timer.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

extern BufferPool *g_pool;

/* run transactions */
void* execute_transaction(void* arg) {
    Transaction* tx = (Transaction*)arg;
    wait_until_tick(tx->start_tick);
    tx->actual_start = global_tick;

    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];
        int tick_before = global_tick;

        switch (op->type) {
            case OP_DEPOSIT: {
                Account* acc = lookup(tx, op->account_id);
                buffer_pool_load(g_pool, op->account_id, acc);
                deposit(op->account_id, op->amount_centavos);
                buffer_pool_unload(g_pool, op->account_id);
                break;
            }

            case OP_WITHDRAW: {
                Account* acc = lookup(tx, op->account_id);
                buffer_pool_load(g_pool, op->account_id, acc);
                if (!withdraw(op->account_id, op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    buffer_pool_unload(g_pool, op->account_id);
                    return NULL;
                }
                buffer_pool_unload(g_pool, op->account_id);
                break;
            }

            case OP_TRANSFER: {
                Account* src = lookup(tx, op->account_id);
                Account* dst = lookup(tx, op->target_account);
                buffer_pool_load(g_pool, op->account_id, src);
                buffer_pool_load(g_pool, op->target_account, dst);
                if (!transfer(op->account_id, op->target_account, op->amount_centavos)) {
                    tx->status = TX_ABORTED;
                    buffer_pool_unload(g_pool, op->account_id);
                    buffer_pool_unload(g_pool, op->target_account);
                    return NULL;
                }
                buffer_pool_unload(g_pool, op->account_id);
                buffer_pool_unload(g_pool, op->target_account);
                break;
            }

            case OP_BALANCE: {
                Account* acc = lookup(tx, op->account_id);
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
        return (sscanf(line, "%*s %d %" SCNd64,
                       &op->account_id, &op->amount_centavos) == 2)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "WITHDRAW") == 0)
    {
        op->type = OP_WITHDRAW;
        op->target_account = -1;
        return (sscanf(line, "%*s %d %" SCNd64,
                       &op->account_id, &op->amount_centavos) == 2)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "BALANCE") == 0)
    {
        op->type = OP_BALANCE;
        op->target_account = -1;
        op->amount_centavos = 0;
        return (sscanf(line, "%*s %d", &op->account_id) == 1) ? 0 : -1;
    }
    if (strcmp(type, "TRANSFER") == 0)
    {
        op->type = OP_TRANSFER;
        return (sscanf(line, "%*s %d %d %" SCNd64,
                       &op->account_id,
                       &op->target_account,
                       &op->amount_centavos) == 3)
                   ? 0
                   : -1;
    }

    fprintf(stderr, "[parser] unknown operation: %s\n", type);
    return -1;
}