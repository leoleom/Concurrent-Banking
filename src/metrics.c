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
    char ibuf[24], fbuf[24];
    fmt_centavos(initial_total, ibuf, sizeof(ibuf));
    fmt_centavos(final_total, fbuf, sizeof(fbuf));

    printf("\nInitial total: %s\n", ibuf);
    printf("Final total  : %s\n", fbuf);

    if (initial_total == final_total)
        printf("Conservation check: PASSED\n\n");
    else
    {
        char dbuf[24];
        fmt_centavos(final_total - initial_total, dbuf, sizeof(dbuf));
        printf("Conservation check: FAILED (discrepancy: %s)\n\n", dbuf);
    }
}

void metrics_destroy(void)
{
    pthread_mutex_destroy(&metrics.lock);
}
