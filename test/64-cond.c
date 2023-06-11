#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread.h"

#ifndef USE_PTHREAD


thread_mutex_t mutex;
thread_cond_t cond;

void* thread_func(void* arg){
    printf("Thread %ld started\n", (long) arg);
    thread_mutex_lock(&mutex);
    printf("Thread %ld acquired lock\n", (long) arg);
    printf("Thread %ld sleeping for 3 seconds\n", (long) arg);
    thread_cond_wait(cond, &mutex);
    printf("Thread %ld woke up\n", (long) arg);
    thread_mutex_unlock(&mutex);
    printf("Thread %ld released lock\n", (long) arg);
    return NULL;
}

int main(){
    thread_t threads[5];

    thread_mutex_init(&mutex);
    thread_cond_init(&cond);

    for(int i = 0; i < 5; i++){
        thread_create(&threads[i], thread_func, (void*)((intptr_t)i));
    }

    thread_yield();
    printf("Main thread signaling waiting threads\n");
    thread_cond_broadcast(cond);

    for(int i = 4; i >= 0; i--){
        thread_join(threads[i], NULL);
    }

    thread_cond_destroy(&cond);
    thread_mutex_destroy(&mutex);
    return 0;
}

#else

int main() {
    return 0;
}

#endif