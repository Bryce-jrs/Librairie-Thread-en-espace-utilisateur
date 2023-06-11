#include <stdio.h>
#include <assert.h>
#include "thread.h"

#ifndef USE_PTHREAD

void *thread_sender_func(void*arg) {
    thread_t * thread = (thread_t *)arg;
    int res = thread_kill(thread,SIG_KILL);
    (void) res;
    thread_exit(NULL);
    return (void*) 0xdeadbeef;
}

void *thread_receiver_func() {
    int res = thread_signal_wait(SIG_KILL);
    (void) res;
    thread_exit(NULL);
    return (void*) 0xdeadbeef;
}

int main() {
    thread_t th1, th2;
    int err;

    err = thread_create(&th1,thread_sender_func, (void *)&th2);
    assert(!err);
    
    err = thread_create(&th2,thread_receiver_func, NULL);
    assert(!err);

    
    err = thread_join(th1, NULL);
    assert(!err);
    
    err = thread_join(th2, NULL);
    assert(!err);
    
    return 0;
}

#else

int main() {
    return 0;
}

#endif