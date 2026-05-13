#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>

#define TICK_INTERVAL_MS 100

extern volatile int global_tick;
extern volatile int simulation_running;
extern pthread_mutex_t tick_lock;
extern pthread_cond_t tick_changed;

void *timer_thread(void *arg);
void wait_until_tick(int target_tick);

#endif 