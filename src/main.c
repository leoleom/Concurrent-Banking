#include "bank.h"
#include "buffer_pool.h"
#include "timer.h"
#include "transaction.h"
#include "metrics.h"
#include "utils.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

Bank *g_bank;       // single bank instance pointer
BufferPool *g_pool; // single buffer pool instance pointer
int verbose = 0;

#define MAX_TRANSACTIONS 64


int main(int argc, char *argv[])
{
    char *accounts_file, *trace_file;
    int tick_ms;

    if (parse_args(argc, argv, &accounts_file, &trace_file, &tick_ms) != 0)
    {
        fprintf(stderr,
                "Usage: %s --accounts=FILE --trace=FILE "
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

     if (verbose) {
    printf("=== Banking System Execution Log ===\n");
    printf("Timer thread started (tick interval: %dms)\n\n", tick_ms);
    printf("[main] %d account(s) loaded  initial total: PHP %d.%02d\n",
           bank.num_accounts, initial_total / 100, initial_total % 100);
        printf("Tick 0:\n");
    }

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

    if (verbose) {
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