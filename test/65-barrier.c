#include <stdio.h>
#include <stdlib.h>
#include "thread.h"

#ifndef USE_PTHREAD

#define NUM_THREADS 3
#define BARRIER_COUNT 3

thread_barrier_t barrier;

void* thread_func(void* arg) {
    int id = (int)arg;
    for(int i = 0; i < BARRIER_COUNT; i++) {
        printf("Thread %d before barrier %d\n", id, i);
        thread_barrier_wait(barrier);
        printf("Thread %d after barrier %d\n", id, i);
    }
    return NULL;
}

int main() {
    thread_t threads[NUM_THREADS];

    thread_barrier_init(&barrier, NUM_THREADS);

    for(int i = 0; i < NUM_THREADS; i++){
        thread_create(&threads[i], thread_func, (void*)((intptr_t)i));
    }

    for(int i = 0; i < NUM_THREADS; i++){
        thread_join(threads[i], NULL);
    }

    thread_barrier_destroy(&barrier);
    return 0;
}

#else

int main() {
    return 0;
}

#endif