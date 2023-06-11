#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/time.h>
#include "thread.h"

/* test de validation de la pré-emption (et par effet de bord de certaines implémentation de priorités)
 *
 * valgrind doit etre content.
 * La durée du programme doit etre proportionnelle au nombre de threads donnés en argument.
 * L'affichage doit être une série d'id alternée dans un ordre plus ou
 * moin aléatoire, et non une suite de 0, puis de 1, puis de 2, ...
 *
 * support nécessaire:
 * - thread_create()
 * - retour sans thread_exit()
 * - thread_join() sans récupération de la valeur de retour
 */


static void * thfunc( void *arg )
{
    unsigned long i, j = 0;

    for(i=0; i<100;i++) {
	for(j=0; j<10000000;j++) {
	}
	fprintf(stderr, "%ld ", (intptr_t)arg );
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    thread_t *th;
    int i, err, nb;
    struct timeval tv1, tv2;
    unsigned long us;

    if (argc < 2) {
        printf("argument manquant: nombre de threads\n");
        return -1;
    }

    nb = atoi(argv[1]);
    th = malloc(nb * sizeof(*th));
    if (!th) {
        perror("malloc");
        return -1;
    }

    gettimeofday(&tv1, NULL);
    /* on cree tous les threads */
    for(i=0; i<nb; i++) {
        err = thread_create(&th[i], thfunc, (void*)((intptr_t)i));
        assert(!err);
    }

    /* On participe au réchauffement climatique */
    thfunc( (void*)((intptr_t)-1) );

    /* on les join tous, maintenant qu'ils sont tous morts */
    for(i=0; i<nb; i++) {
        err = thread_join(th[i], NULL);
        assert(!err);
    }
    gettimeofday(&tv2, NULL);

    us = (tv2.tv_sec-tv1.tv_sec)*1000000+(tv2.tv_usec-tv1.tv_usec);
    printf("Programme exécuté en %ld us\n", us);

    free(th);
    return 0;
}
