#include "buffer_pool.h"
#include <stdio.h>
#include <string.h>

void buffer_pool_init(BufferPool *pool)
{
    for (int i = 0; i < BUFFER_POOL_SIZE; i++)
    {
        pool->slots[i].account_id = -1;
        pool->slots[i].data = NULL;
        pool->slots[i].in_use = false;
    }

    sem_init(&pool->empty_slots, 0, BUFFER_POOL_SIZE);
    sem_init(&pool->full_slots, 0, 0);
    pthread_mutex_init(&pool->pool_lock, NULL);
}

// Load account into buffer pool (producer)
void buffer_pool_load(BufferPool *pool, int account_id, Account *acc)
{
    sem_wait(&pool->empty_slots); // Wait for empty slot

    pthread_mutex_lock(&pool->pool_lock);

    // Find empty slot and load account
    for (int i = 0; i < BUFFER_POOL_SIZE; i++)
    {
        if (!pool->slots[i].in_use)
        {
            pool->slots[i].account_id = account_id;
            pool->slots[i].data = acc;
            pool->slots[i].in_use = true;
            break;
        }
    }

    pthread_mutex_unlock(&pool->pool_lock);

    sem_post(&pool->full_slots); // Signal slot is full
}

// Unload account from buffer pool (consumer)
void buffer_pool_unload(BufferPool *pool, int account_id)
{
    sem_wait(&pool->full_slots); // Wait for full slot

    pthread_mutex_lock(&pool->pool_lock);

    // Find and unload account
    for (int i = 0; i < BUFFER_POOL_SIZE; i++)
    {
        if (pool->slots[i].in_use &&
            pool->slots[i].account_id == account_id)
        {
            pool->slots[i].in_use = false;
            pool->slots[i].account_id = -1;
            pool->slots[i].data       = NULL;
            printf("[pool]  unloaded acc=%-4d  slot=%d\n", account_id, i);
            break;
        }
    }

    pthread_mutex_unlock(&pool->pool_lock);

    sem_post(&pool->empty_slots); // Signal slot is empty
}

void buffer_pool_destroy(BufferPool *pool)
{
    sem_destroy(&pool->empty_slots);
    sem_destroy(&pool->full_slots);
    pthread_mutex_destroy(&pool->pool_lock);
}