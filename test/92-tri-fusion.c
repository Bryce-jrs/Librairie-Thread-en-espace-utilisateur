#include <stdio.h>
#include <stdlib.h>
#include <thread.h>
#include <time.h>
#define MAX_THREADS 16

struct thread_args {
    int* tab;
    int start;
    int end;
    int* tmp;
};

void merge(int* tab, int start, int mid, int end, int* tmp) {
    int i = start, j = mid + 1, k = 0;
    while (i <= mid && j <= end) {
        if (tab[i] < tab[j]) {
            tmp[k++] = tab[i++];
        } else {
            tmp[k++] = tab[j++];
        }
    }
    while (i <= mid) {
        tmp[k++] = tab[i++];
    }
    while (j <= end) {
        tmp[k++] = tab[j++];
    }
    for (i = 0; i < k; ++i) {
        tab[start + i] = tmp[i];
    }
}

void* merge_sort(void* arg) {
    struct thread_args* args = (struct thread_args*)arg;
    int* tab = args->tab;
    int start = args->start;
    int end = args->end;
    int* tmp = args->tmp;

    if (start < end) {
        int mid = (start + end) / 2;

        // Trie les deux moitiés du tableau en parallèle
        thread_t threads[2];
        struct thread_args thread_args[2];
        thread_args[0].tab = tab;
        thread_args[0].start = start;
        thread_args[0].end = mid;
        thread_args[0].tmp = tmp;
        thread_create(&threads[0],merge_sort, &thread_args[0]);

        thread_args[1].tab = tab;
        thread_args[1].start = mid + 1;
        thread_args[1].end = end;
        thread_args[1].tmp = tmp;
        thread_create(&threads[1], merge_sort, &thread_args[1]);

        // Attend que les deux moitiés soient triées
        thread_join(threads[0], NULL);
        thread_join(threads[1], NULL);

        // Fusionne les deux moitiés triées
        merge(tab, start, mid, end, tmp);
    }

    thread_exit(NULL);
    return (void*)666;
}

int main(int argc, char* argv[]) {
    int size = 10;
    if (argc == 2) {
        size = atoi(argv[1]);
    }

    // Initialise un tableau de nombres aléatoires
    int* tab = malloc(size * sizeof(int));
    int i;
    srand(time(NULL));
    for (i = 0; i < size; ++i) {
        tab[i] = rand() % 1000;
    }

    // Initialise un tableau temporaire utilisé pour la fusion
    int* tmp = malloc(size * sizeof(int));

    // Trie le tableau en utilisant des threads
    thread_t thread;
    struct thread_args args;
    args.tab = tab;
    args.start = 0;
    args.end = size - 1;
    args.tmp = tmp;
    thread_create(&thread, merge_sort, &args);
    thread_join(thread, NULL);

    // Affiche le tableau trié
    printf("Tableau trié : ");
    for (i = 0; i < size; i++) {
        printf("%d ", tab[i]);
    }
    printf("\n");

    return 0;
}
