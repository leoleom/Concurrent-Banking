#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <pthread.h>
#include <semaphore.h>

#include "bank.h"
#define BUFFER_POOL_SIZE 5

typedef struct
{
    int account_id;
    Account *data;
    bool in_use;
} BufferSlot;

typedef struct
{
    BufferSlot slots[BUFFER_POOL_SIZE];
    sem_t empty_slots;
    sem_t full_slots;
    pthread_mutex_t pool_lock;
} BufferPool;

void buffer_pool_init(BufferPool *pool);
void buffer_pool_load(BufferPool *pool, int account_id);
void buffer_pool_unload(BufferPool *pool, int account_id);
void buffer_pool_destroy(BufferPool *pool);
#endif