#include "bank.h"
#include "buffer_pool.h"
#include "timer.h"
#include "transaction.h"
#include "metrics.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

Bank *g_bank;       // single bank instance pointer
BufferPool *g_pool; // single buffer pool instance pointer
int verbose = 0;

#define MAX_TRANSACTIONS 64

// command line parsing
static int parse_args(int argc, char *argv[],
                      char **accounts_file,
                      char **trace_file,
                      char **deadlock_strategy,
                      int *tick_ms)
{
    *accounts_file = NULL;
    *trace_file = NULL;
    *deadlock_strategy = NULL;
    *tick_ms = TICK_INTERVAL_MS;

    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--accounts=", 11) == 0)
            *accounts_file = argv[i] + 11;
        else if (strncmp(argv[i], "--trace=", 8) == 0)
            *trace_file = argv[i] + 8;
        else if (strncmp(argv[i], "--deadlock=", 11) == 0)
            *deadlock_strategy = argv[i] + 11;
        else if (strncmp(argv[i], "--tick-ms=", 10) == 0)
            *tick_ms = atoi(argv[i] + 10);
        else if (strcmp(argv[i], "--verbose") == 0)
            verbose = 1;
        else
        {
            fprintf(stderr, "ERROR: unknown flag: %s\n", argv[i]);
            return -1;
        }
    }

    if (!*accounts_file)
    {
        fprintf(stderr, "ERROR: --accounts=FILE is required\n");
        return -1;
    }
    if (!*trace_file)
    {
        fprintf(stderr, "ERROR: --trace=FILE is required\n");
        return -1;
    }
    if (!*deadlock_strategy)
    {
        fprintf(stderr, "ERROR: --deadlock=prevention|detection is required\n");
        return -1;
    }
    if (strcmp(*deadlock_strategy, "prevention") != 0 &&
        strcmp(*deadlock_strategy, "detection") != 0)
    {
        fprintf(stderr, "ERROR: --deadlock must be 'prevention' or 'detection'\n");
        return -1;
    }

    return 0;
}

// accounts txt parser
static int load_accounts(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "ERROR: cannot open accounts file: %s\n", path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        int id, bal;
        if (sscanf(line, "%d %d", &id, &bal) != 2)
        {
            fprintf(stderr, "WARNING: invalid line: %s\n", line);
            continue;
        }
        if (bank_add_account(g_bank, id, bal) != 0)
        {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

// trace file parser
static int load_trace(const char *path,
                      Transaction *txs, int *tx_count, int max_tx)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "ERROR: cannot open trace file: %s\n", path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        int tx_id, start_tick;
        char op_str[256];

        if (sscanf(line, "T%d %d %[^\n]", &tx_id, &start_tick, op_str) != 3)
        {
            fprintf(stderr, "WARNING: invalid line: %s\n", line);
            continue;
        }
        Transaction *tx = NULL;
        for (int i = 0; i < *tx_count; i++)
        {
            if (txs[i].tx_id == tx_id)
            {
                tx = &txs[i];
                break;
            }
        }
        if (!tx)
        {
            if (*tx_count >= max_tx)
            {
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

        if (tx->num_ops >= MAX_OPERATIONS)
        {
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

// main

int main(int argc, char *argv[])
{
    char *accounts_file, *trace_file, *deadlock_strategy;
    int tick_ms;

    if (parse_args(argc, argv,
                   &accounts_file, &trace_file,
                   &deadlock_strategy, &tick_ms) != 0)
    {
        fprintf(stderr,
                "Usage: %s --accounts=FILE --trace=FILE "
                "--deadlock=prevention|detection "
                "[--tick-ms=N] [--verbose]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    Bank bank;
    BufferPool pool;

    bank_init(&bank);
    buffer_pool_init(&pool);
    metrics_init();

    g_bank = &bank;
    g_pool = &pool;

    // load accounts from file
    if (load_accounts(accounts_file) != 0)
        return EXIT_FAILURE;

    // initial total from the accounts file
    int initial_total = 0;
    for (int i = 0; i < bank.num_accounts; i++)
        initial_total += bank.accounts[i].balance_centavos;

    // load transactions from trace
    Transaction txs[MAX_TRANSACTIONS];
    int tx_count = 0;
    if (load_trace(trace_file, txs, &tx_count, MAX_TRANSACTIONS) != 0)
        return EXIT_FAILURE;

    printf("=== Banking System Execution Log ===\n");
    printf("Timer thread started (tick interval: %dms)\n\n", tick_ms);
    printf("[main] %d account(s) loaded  initial total: PHP %d.%02d\n",
           bank.num_accounts, initial_total / 100, initial_total % 100);
    printf("[main] %d transaction(s) scheduled  deadlock=%s\n\n",
           tx_count, deadlock_strategy);

    printf("Tick 0:\n");

    pthread_t timer_tid;
    if (pthread_create(&timer_tid, NULL, timer_thread, &tick_ms) != 0)
    {
        fprintf(stderr, "ERROR: failed to create timer thread\n");
        return EXIT_FAILURE;
    }
    pthread_detach(timer_tid); // runs until simulation_running = 0

    // spawn one thread per transaction
    for (int i = 0; i < tx_count; i++)
    {
        if (pthread_create(&txs[i].thread, NULL,
                           execute_transaction, &txs[i]) != 0)
        {
            fprintf(stderr, "ERROR: failed to spawn thread for T%d\n",
                    txs[i].tx_id);
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < tx_count; i++)
        pthread_join(txs[i].thread, NULL);

    simulation_running = 0;

    int final_total = 0;
    printf("\n=== Summary ===\n");

    int committed = 0, aborted = 0;
    for (int i = 0; i < tx_count; i++)
    {
        if (txs[i].status == TX_COMMITTED)
            committed++;
        else
            aborted++;
    }
    printf("Total transactions : %d\n", tx_count);
    printf("Committed          : %d\n", committed);
    printf("Aborted            : %d\n", aborted);
    printf("Total ticks        : %d\n", global_tick + 1);
    printf("ThreadSanitizer    : run with 'make debug' to verify\n");

    printf("\n--- Final Account Balances ---\n");
    for (int i = 0; i < bank.num_accounts; i++)
    {
        Account *a = &bank.accounts[i];

        // null check
        if (!a)
            continue;

        char buf[24];
        fmt_centavos(a->balance_centavos, buf, sizeof(buf));
        printf("  Account %-6d  %s\n", a->account_id, buf);
        final_total += a->balance_centavos;
    }

    // for metrics regis
    for (int i = 0; i < tx_count; i++)
    {
        TxMetrics m;
        m.tx_id = txs[i].tx_id;
        m.start_tick = txs[i].start_tick;
        m.actual_start = txs[i].actual_start;
        m.actual_end = txs[i].actual_end;
        m.wait_ticks = txs[i].wait_ticks;
        m.num_ops = txs[i].num_ops;
        m.committed = (txs[i].status == TX_COMMITTED);
        metrics_record_tx(&m);
    }

    metrics_report(global_tick + 1);
    metrics_conservation_check(initial_total, final_total);

    // clean up
    bank_destroy(&bank);
    buffer_pool_destroy(&pool);
    metrics_destroy();

    return EXIT_SUCCESS;
}