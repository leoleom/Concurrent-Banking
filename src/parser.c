#include "parser.h"
#include "bank.h"
#include "transaction.h"
#include "timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Bank *g_bank;
extern int verbose;

int parse_args(int argc, char *argv[],
               char **accounts_file,
               char **trace_file,
               int *tick_ms)
{
    *accounts_file = NULL;
    *trace_file = NULL;
    *tick_ms = TICK_INTERVAL_MS;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--accounts=", 11) == 0)
            *accounts_file = argv[i] + 11;
        else if (strncmp(argv[i], "--trace=", 8) == 0)
            *trace_file = argv[i] + 8;
        else if (strncmp(argv[i], "--tick-ms=", 10) == 0)
            *tick_ms = atoi(argv[i] + 10);
        else if (strcmp(argv[i], "--verbose") == 0)
            verbose = 1;
        else {
            fprintf(stderr, "ERROR: unknown flag: %s\n", argv[i]);
            return -1;
        }
    }

    if (!*accounts_file) {
        fprintf(stderr, "ERROR: --accounts=FILE is required\n");
        return -1;
    }
    if (!*trace_file) {
        fprintf(stderr, "ERROR: --trace=FILE is required\n");
        return -1;
    }

    return 0;
}


int load_accounts(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open accounts file: %s\n", path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        int id, bal;
        if (sscanf(line, "%d %d", &id, &bal) != 2) {
            fprintf(stderr, "WARNING: invalid line: %s\n", line);
            continue;
        }
        if (bank_add_account(g_bank, id, bal) != 0) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

int load_trace(const char *path,
               Transaction *txs, int *tx_count, int max_tx)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "ERROR: cannot open trace file: %s\n", path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        int tx_id, start_tick;
        char op_str[256];

        if (sscanf(line, "T%d %d %[^\n]", &tx_id, &start_tick, op_str) != 3) {
            fprintf(stderr, "WARNING: invalid line: %s\n", line);
            continue;
        }

        Transaction *tx = NULL;
        for (int i = 0; i < *tx_count; i++) {
            if (txs[i].tx_id == tx_id) {
                tx = &txs[i];
                break;
            }
        }
        if (!tx) {
            if (*tx_count >= max_tx) {
                fprintf(stderr, "ERROR: too many transactions (max %d)\n", max_tx);
                fclose(f);
                return -1;
            }
            tx = &txs[(*tx_count)++];
            memset(tx, 0, sizeof(Transaction));
            tx->tx_id = tx_id;
            tx->start_tick = start_tick;
            tx->status = TX_PENDING;
        }

        if (tx->num_ops >= MAX_OPERATIONS) {
            fprintf(stderr, "ERROR: Too many operations in T%d\n", tx_id);
            fclose(f);
            return -1;
        }

        if (parse_operation(op_str, &tx->ops[tx->num_ops]) == 0)
            tx->num_ops++;
        else
            fprintf(stderr, "WARNING: invalid operation T%d: %s\n",
                    tx_id, op_str);
    }

    fclose(f);
    return 0;
}

int parse_operation(const char *line, Operation *op)
{
    if (!line || !op)
        return -1;

    char type[16];
    if (sscanf(line, "%15s", type) != 1)
        return -1;
    if (type[0] == '#')
        return -1;

    if (strcmp(type, "DEPOSIT") == 0) {
        op->type = OP_DEPOSIT;
        op->target_account = -1;
        return (sscanf(line, "%*s %d %d",
                       &op->account_id,
                       &op->amount_centavos) == 2)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "WITHDRAW") == 0) {
        op->type = OP_WITHDRAW;
        op->target_account = -1;
        return (sscanf(line, "%*s %d %d",
                       &op->account_id,
                       &op->amount_centavos) == 2)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "BALANCE") == 0) {
        op->type = OP_BALANCE;
        op->target_account = -1;
        op->amount_centavos = 0;
        return (sscanf(line, "%*s %d",
                       &op->account_id) == 1)
                   ? 0
                   : -1;
    }
    if (strcmp(type, "TRANSFER") == 0) {
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
