#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <thread.h>
#include <time.h>

/*
 * Exemple d'application de cette bibliothèque de threads.
 * Calcule la somme des éléments d'un tableau par diviser pour régner.
 * Calcule aussi cette somme "à la main" pour vérifier le résultat.
 */

struct triplet {
  int start;
  int end;
  int *arr;
};

void *sum(void *arg) {
  struct triplet *t = (struct triplet *)arg;
  long *ret = malloc(sizeof(long));
  if (t->end == t->start) {
    *ret = (long)t->arr[t->end];
  } else {
    int mid = (t->start + t->end) / 2;
    thread_t thread1, thread2;
    struct triplet arg1, arg2;
    arg1.start = t->start;
    arg1.end = mid;
    arg1.arr = t->arr;
    arg2.start = mid + 1;
    arg2.end = t->end;
    arg2.arr = t->arr;
    int err = thread_create(&thread1,sum,&arg1);
    assert(!err);
    err = thread_create(&thread2,sum, &arg2);
    assert(!err);
    long *ret1, *ret2;
    err = thread_join(thread1, (void *)&ret1);
    assert(!err);
    err = thread_join(thread2, (void *)&ret2);
    assert(!err);
    *ret = (long)*ret1 + *ret2;
    free(ret1);
    free(ret2);
  }
  thread_exit((void *)ret);
  return (void*)666;
}

int main(int argc, char **argv) {
  int size = 1000;
  int max = 10000;
  if (argc == 3) {
    size = atoi(argv[1]);
    max = atoi(argv[2]);
  }
  int *arr = malloc(size * sizeof(int));
  srand(time(NULL));
  for (int i = 0; i < size; i++) {
    arr[i] = rand() % max;
  }
  struct triplet t = {0, size - 1, arr};
  long *retval;
  thread_t thread;
  int err = thread_create(&thread, sum,&t);
  assert(!err);
  err = thread_join(thread, (void **)&retval);
  assert(!err);
  printf("Sum calculated using threads: %ld\n", *retval);
  free(retval);
  long res = 0;
  for (int i = 0; i < size; i++) {
    res += arr[i];
  }
  printf("Sum calculated manually: %ld\n", res);
  free(arr);
  return EXIT_SUCCESS;
}
