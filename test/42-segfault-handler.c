#include <stdio.h>
#include <assert.h>
#include "thread.h"

/* test de la detection et gestion de segfault.
 *
 * le programme doit retourner correctement.
 * valgrind doit être content.
 *
 * support nécessaire:
 * - thread_create()
 * - thread_join() avec récupération valeur de retour, avec et sans thread_exit()
 */

int rec(int a){
    return rec(a+1);
}

static void * thfunc(void *dummy __attribute__((unused)))
{
  rec(666);
  return NULL; 
}

int main()
{
  thread_t th;
  int err;
  void *res = NULL;

  err = thread_create(&th, thfunc, NULL);

  assert(!err);
  err = thread_join(th, &res);

  printf("program end OK\n");
  return 0;
}