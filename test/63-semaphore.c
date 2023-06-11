#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/time.h>
#include "thread.h"

#ifndef USE_PTHREAD

/* test de faire une somme avec plein de thread sur un compteur partagé
 *
 * valgrind doit etre content.
 * Les résultats doivent etre égals au nombre de threads * 1000.
 * La durée du programme doit etre proportionnelle au nombre de threads donnés en argument.
 *
 * support nécessaire:
 * - thread_create()
 * - retour sans thread_exit()
 * - thread_join() sans récupération de la valeur de retour
 * - thread_sem_init()
 * - thread_sem_destroy()
 * - thread_sem_wait()
 * - thread_sem_post()
 */


int counter = 0;
thread_sem_t sem;

static void *thfunc(void *_nb)
{
    unsigned long nb = (unsigned long) _nb;
    (void) nb;
    unsigned long i = 0;
    int tmp;

    for(i = 0; i < 1000; i++) {
        /* Verrouille la section critique accédant a counter */
        thread_sem_wait(sem);
        tmp = counter;
        thread_yield();
        tmp++;
        thread_yield();
        counter = tmp;
        thread_sem_post(sem);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    thread_t *th;
    int i, nb_threads;
    int err, nb;
    struct timeval tv1, tv2;
    unsigned long us;

    if (argc < 2) {
        printf("argument manquant: nombre de threads\n");
        return -1;
    }

    nb = atoi(argv[1]);
    nb_threads = nb ;

    if (thread_sem_init(&sem,0, 1) != 0) {
        fprintf(stderr, "thread_sem_init failed\n");
        return -1;
    }

    th = malloc(nb_threads * sizeof(*th));
    if (!th) {
        perror("malloc");
        return -1;
    }

    gettimeofday(&tv1, NULL);
    /* on cree tous les threads */
    for (i = 0; i < nb_threads; i++) {
        err = thread_create(&th[i], thfunc, (void*)((intptr_t)i));
        assert(!err);
    }

    /* on leur passe la main, ils vont tous terminer */
    for (i = 0; i < nb; i++) {
        thread_yield();
    }

    /* on les joine tous, maintenant qu'ils sont tous morts */
    for (i = 0; i < nb_threads; i++) {
        err = thread_join(th[i], NULL);
        assert(!err);
    }
    gettimeofday(&tv2, NULL);

    free(th);
    thread_sem_destroy(&sem);

    if (counter == (nb * 1000)) {
        printf("La somme a été correctement calculée: %d * 1000 = %d\n", nb, counter);
    } else {
        printf("Le résultat est INCORRECT: %d * 1000 != %d\n", nb, counter);
        err = EXIT_FAILURE;
    }
    if(err == EXIT_SUCCESS) {
        us = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
        printf("Programme exécuté en %ld us\n", us);
    }
    return err;
}

#else

int main() {
    return 0;
}

#endif