#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <pthread.h>
#include <stdint.h>

#define MAX_OPERATIONS 256

typedef enum
{
    OP_DEPOSIT,  // Add money to account
    OP_WITHDRAW, // Remove money from account
    OP_TRANSFER, // Move money between two accounts
    OP_BALANCE,  // Read account balance
} OpType;

typedef struct
{
    OpType type;
    int account_id;      // Primary account
    int amount_centavos; // Amount in centavos
    int target_account;  // For TRANSFER only
} Operation;

typedef enum
{
    TX_PENDING,
    TX_RUNNING,
    TX_COMMITTED,
    TX_ABORTED
} TxStatus;

typedef struct
{
    int tx_id;
    Operation ops[MAX_OPERATIONS]; // Max 256 operations per transaction
    int num_ops;
    int start_tick; // When transaction should start
    pthread_t thread;

    // Timing (measured in ticks)
    int actual_start;
    int actual_end;
    int wait_ticks;

    // Status
    TxStatus status;
} Transaction;


void *execute_transaction(void *arg);
void abort_transaction(Transaction *tx, const char *reason);

extern Transaction *current_tx;

#endif