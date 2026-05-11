
#include "timer.h"
#include <unistd.h>
#include <stdio.h>

volatile int    global_tick       = 0;
volatile int    simulation_running = 1;
pthread_mutex_t tick_lock         = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  tick_changed      = PTHREAD_COND_INITIALIZER;

 // increments global_tick
void *timer_thread(void *arg)
{
    int tick_ms = *(int *)arg;

    while (simulation_running) {
        usleep(tick_ms * 1000);

        pthread_mutex_lock(&tick_lock);

        global_tick++;
        printf("\nTick %d:\n", global_tick);
        fflush(stdout);

        pthread_cond_broadcast(&tick_changed); //wakes transactions
        pthread_mutex_unlock(&tick_lock);
    }

    return NULL;
}

// blocks calling thread until global_tick >= target_tick
void wait_until_tick(int target_tick)
{
    pthread_mutex_lock(&tick_lock);
    while (global_tick < target_tick)
        pthread_cond_wait(&tick_changed, &tick_lock);
    pthread_mutex_unlock(&tick_lock);
}