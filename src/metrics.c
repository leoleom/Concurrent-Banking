#include "metrics.h"
#include "utils.h"
#include "buffer_pool.h"
#include <stdio.h>
#include <string.h>

MetricsRegistry metrics;

// initialzie mutex
void metrics_init(void)
{
    memset(&metrics, 0, sizeof(MetricsRegistry));
    pthread_mutex_init(&metrics.lock, NULL);
}

void metrics_record_tx(TxMetrics *m)
{
    // null check
    if (!m)
        return;

    pthread_mutex_lock(&metrics.lock);
    if (metrics.count < MAX_TX_METRICS)
        metrics.entries[metrics.count++] = *m;
    else
        fprintf(stderr, "WARNING: registry full, TX %d dropped\n",
                m->tx_id);
    pthread_mutex_unlock(&metrics.lock);
}

void metrics_report(int total_ticks)
{
    pthread_mutex_lock(&metrics.lock);

    printf("\n=== Transaction Performance Metrics ===\n");
    printf("%-6s %-10s %-12s %-5s %-10s %-10s\n",
           "TxID", "StartTick", "ActualStart", "End", "WaitTicks", "Status");
    printf("%-6s %-10s %-12s %-5s %-10s %-10s\n",
           "-----", "---------", "-----------", "---", "---------", "----------");

    int committed = 0, aborted = 0, total_wait = 0;

    for (int i = 0; i < metrics.count; i++)
    {
        TxMetrics *m = &metrics.entries[i];
        printf("T%-5d %-10d %-12d %-5d %-10d %-10s\n",
               m->tx_id,
               m->start_tick,
               m->actual_start,
               m->actual_end,
               m->wait_ticks,
               m->committed ? "COMMITTED" : "ABORTED");
        if (m->committed)
            committed++;
        else
            aborted++;
        total_wait += m->wait_ticks;
    }

    printf("\n");
    printf("Total transactions : %d\n", metrics.count);
    printf("Committed          : %d\n", committed);
    printf("Aborted            : %d\n", aborted);

    if (metrics.count > 0)
        printf("Average wait time  : %.2f ticks\n",
               (double)total_wait / metrics.count);

    if (total_ticks > 0)
        printf("Throughput         : %d transactions / %d ticks = %.2f tx/tick\n",
               metrics.count, total_ticks,
               (double)metrics.count / total_ticks);

    printf("\n=== Buffer Pool Report ===\n");
    printf("Pool size          : %d slots\n", BUFFER_POOL_SIZE);
    printf("Total loads        : %d\n", metrics.total_loads);
    printf("Total unloads      : %d\n", metrics.total_unloads);
    printf("Peak usage         : %d slots\n", metrics.peak_pool_usage);
    printf("Blocked operations : %d\n", metrics.blocked_operations);

    pthread_mutex_unlock(&metrics.lock);
}

// verification of total money after completion of transactions
void metrics_conservation_check(int initial_total, int final_total)
{
    char ibuf[24], fbuf[24], ebuf[24];
    fmt_centavos(initial_total, ibuf, sizeof(ibuf));
    fmt_centavos(final_total, fbuf, sizeof(fbuf));

    /* Expected total = initial + deposits - withdrawals */
    int expected_total = initial_total + metrics.total_deposits - metrics.total_withdrawals;
    fmt_centavos(expected_total, ebuf, sizeof(ebuf));

    printf("\nInitial total: %s\n", ibuf);
    printf("Final total  : %s\n", fbuf);
    printf("Expected total: %s (after PHP %d.%02d deposits, PHP %d.%02d withdrawals)\n", 
           ebuf, metrics.total_deposits / 100, metrics.total_deposits % 100,
           metrics.total_withdrawals / 100, metrics.total_withdrawals % 100);

    if (expected_total == final_total)
        printf("Conservation check: PASSED\n\n");
    else
    {
        char dbuf[24];
        fmt_centavos(final_total - expected_total, dbuf, sizeof(dbuf));
        printf("Conservation check: FAILED (discrepancy: %s)\n\n", dbuf);
    }
}

void metrics_destroy(void)
{
    pthread_mutex_destroy(&metrics.lock);
}
