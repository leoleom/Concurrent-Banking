#ifndef METRICS_H
#define METRICS_H

#include <pthread.h>
#include <stdbool.h>

#include "buffer_pool.h"

#define MAX_TX_METRICS 64

typedef struct
{
    int tx_id;
    int start_tick;
    int actual_start;
    int actual_end;
    int wait_ticks;
    int num_ops;
    bool committed;
} TxMetrics;

typedef struct
{
    TxMetrics entries[MAX_TX_METRICS];
    int count;
    int total_loads;
    int total_unloads;
    int peak_pool_usage;
    int blocked_operations;
    pthread_mutex_t lock;
} MetricsRegistry;

extern MetricsRegistry metrics;

void metrics_init(void);
void metrics_record_tx(TxMetrics *m);
void metrics_report(int total_ticks);
void metrics_conservation_check(int initial_total, int final_total);
void metrics_destroy(void);

#endif 
