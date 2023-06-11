#include "thread.h"
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include <valgrind/valgrind.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>

#define CONTEXT_STACK_SIZE 32*1024
#define MAX_PRIORITY 10
#define MIN_PRIORITY 0
#define TIMESLICE 10
#define UNITARY_QUANTUM 100
#define MAX_SIGNALS 10
#define NBR_SIGNALS 3
#define PAGE_SIZE 4096

/**********************   
    Global Variables 
***********************/
thread_t current_thread = NULL;
int number_thread = 0;
struct thread *main_thread = NULL;
struct itimerval timer,remainingtime;
struct sigaction act_timer;
signal_t signals[NBR_SIGNALS] = {
    {.type = SIG_USER1, .handler = default_signal_handler, .old_handler = default_signal_handler},
    {.type = SIG_USER2, .handler = default_signal_handler, .old_handler = default_signal_handler},
    {.type = SIG_KILL, .handler = default_signal_handler, .old_handler = default_signal_handler}
};
signal_t no_signal = {.type = Error, .handler = NULL};

#ifdef FIFO
TAILQ_HEAD(threadqueue, thread) ready;
#endif

#ifdef PRIORITY
struct threadqueue {
    int priority;
    TAILQ_HEAD(tqhead, thread) threads;
};
struct threadqueue ready[MAX_PRIORITY];
#endif


/**
    @struct struct thread
    @brief Structure représentant nos threads.
    Cette structure contient les informations nécessaires pour représenter un thread, telles que son identifiant,
    son contexte, son résultat de retour, son état d'exécution, etc.
*/
struct thread{
    int id; 
    ucontext_t uc; 
    void *retval;
    int is_done;
    struct thread *joiner;
    TAILQ_ENTRY(thread) threads;
    int valgrind_stackid;
    int is_locked;
    int id_first;
    int priority;
    thread_signal_t *th;
};


/*************************************** 
    Interface Functions Implementation 
****************************************/

/**
    @brief Récupérer l'identifiant du thread courant.
    Cette fonction renvoie l'identifiant du thread courant.
    @return Identifiant du thread courant.
*/
extern thread_t thread_self(void){
    return current_thread;
}


/**
    @fn extern int thread_create(thread_t *newthread, void *(*func)(void *), void *funcarg)
    @brief Créer un nouveau thread.
    Cette fonction crée un nouveau thread qui exécute la fonction func avec l'argument funcarg.
    @param newthread Pointeur vers la variable qui recevra l'identifiant du nouveau thread.
    @param func Pointeur vers la fonction que le thread exécutera.
    @param funcarg Pointeur vers l'argument que le thread passera à la fonction.
    @return 0 si la création du thread a réussi, -1 en cas d'erreur.
*/

extern int thread_create(thread_t *newthread, void *(*func)(void *), void *funcarg){
    *newthread = thread_init();
    // printf("newthread %p, stack %p\n", *newthread, (*newthread)->uc.uc_stack.ss_sp);
    makecontext(&(*newthread)->uc,(void (*)(void))call_function, 2, func, funcarg);
    add_thread_to_queue_tail(*newthread);
    number_thread++;
    return 0;
}

/**
    @fn extern int thread_yield(void)
    @brief Passer la main à un autre thread.
    Cette fonction permet de céder la main à un autre thread de même priorité ou de priorité supérieure.
    @return 0 si la fonction s'est exécutée avec succès.
*/
extern int thread_yield(void){
    #ifdef PREEMPTION
    disable_interrupt();
    #endif
    struct thread *self = thread_self();
    if (self != NULL && self->is_locked != 1) {
        remove_thread_from_queue(self);
        update_thread_priority(self);
        add_thread_to_queue_tail(self);
        current_thread = get_thread();
        handle_swap(self,current_thread);
    }
    return 0;
}

/**
    @fn extern int thread_join(thread_t thread, void **retval)
    @brief Attendre la fin d'exécution d'un thread.
    la valeur renvoyée par le thread est placée dans *retval.
    si retval est NULL, la valeur de retour est ignorée.
    @return 0 si la fonction s'est exécutée avec succès.
 */
extern int thread_join(thread_t thread, void **retval){
    //printf("joining %p\n", thread);
    #ifdef PREEMPTION
    disable_interrupt();
    #endif
    if(thread == NULL) {
        return -1;
    }
    if(!thread->is_done){
        if(thread->id == current_thread->id_first) {
            return EDEADLK;
        }
        if(thread->id_first == -1){
            if(current_thread->id_first == -1) {
                thread->id_first = current_thread->id;
            }
            else {
                thread->id_first = current_thread->id_first;
            }
        }
        //printf("here1 %p\n", thread);
        thread->joiner = current_thread;
        remove_thread_from_queue(thread->joiner);
        update_thread_priority(thread->joiner);
        current_thread = get_thread();
        handle_swap(thread->joiner,current_thread);
        //printf("here2 %p\n", thread);
    }
    if(retval!=NULL){
        *retval = thread->retval;
    }
    if(thread!=main_thread){
        //printf("not main ending %p\n", thread);
        thread_signal_free(thread->th); 
        VALGRIND_STACK_DEREGISTER(thread->valgrind_stackid);
        #ifdef STACKOVERFLOW
        mprotect(thread->uc.uc_stack.ss_sp-PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE);
        free(thread->uc.uc_stack.ss_sp-PAGE_SIZE);
        #else 
        free(thread->uc.uc_stack.ss_sp);
        #endif
        free(thread);
        //printf("end not main %p\n", thread);
    }
    return 0; 
}

/** 
    @brief le thread courant en renvoyant la valeur de retour retval.
    cette fonction ne retourne jamais.
    L'attribut noreturn aide le compilateur à optimiser le code de
    l'application (élimination de code mort). Attention à ne pas mettre
    cet attribut dans votre interface tant que votre thread_exit()
    n'est pas correctement implémenté (il ne doit jamais retourner).
 */
extern void thread_exit(void *retval){
    #ifdef PREEMPTION
    disable_interrupt();
    #endif
    thread_t self = thread_self();
    self->retval = retval;
    self->is_done = 1;
    remove_thread_from_queue(self);
    update_thread_priority(self);
    number_thread--;
    if(self->joiner){
        add_thread_to_queue_head(self->joiner);
    }
    current_thread = get_thread();
    handle_swap(self,current_thread);
    exit(0);
}

/************************************
    Implementation des Mutex 
*************************************/

int thread_mutex_init(thread_mutex_t *mutex) {
    mutex->locked = NULL;
    return 0;
}

int thread_mutex_destroy(thread_mutex_t *mutex) {
    (void) mutex;
    return 0; 
}

int thread_mutex_lock(thread_mutex_t *mutex) {
    mutex->locked = thread_self();
    mutex->locked->is_locked = 1;
    return 0;
}

int thread_mutex_unlock(thread_mutex_t *mutex) {
    mutex->locked->is_locked = 0;
    mutex->locked = NULL;
    return 0;
}



/************************************
    Implementation des Sémaphores 
*************************************/

/**
 *  @struct thread_sem
 *  @brief Structure de notre sémaphore
 */
struct thread_sem{
    atomic_int max_resources;       /*!<Nombre maximal de ressources*/
    atomic_int current_resources;   /*!<Nombre actuel de ressources*/
    atomic_int destroyed;           /*!<Indicateur de destruction*/
    thread_mutex_t *lock;           /*!<Mutex pour protéger la sémaphore */
};

/**
    @fn int thread_sem_init(thread_sem_t *sem, int pshared, unsigned int value)
    @brief Fonction d'initialisation de notre sémaphore
    @param sem Pointeur vers la sémaphore à initialiser
    @param pshared Ignoré (toujours égal à zéro)
    @param value Valeur initiale de la sémaphore
    @return int 0 si l'initialisation a réussi, -1 sinon 
 */
int thread_sem_init(thread_sem_t *sem, int pshared, unsigned int value){

    /**
    * allocation de la mémoire allouée à la sémaphore 
    */ 
    *sem = malloc(sizeof(struct thread_sem));

    assert(pshared==0);
    if(*sem == NULL){
        return -1;
    }
    /**
     * allocation et initialisation des champs de la sémaphore
     */
    atomic_init(&((*sem)->destroyed), 0);
    atomic_init(&((*sem)->max_resources), value);
    atomic_init(&((*sem)->current_resources), value);
    (*sem)->lock = malloc(sizeof(thread_mutex_t));
    if (thread_mutex_init((*sem)->lock) != 0) {
            return -1;
    }
    return 0;
}

/**
    @fn int thread_sem_destroy(thread_sem_t *sem)
    @brief fonction de destruction de notre sémaphore
    @param sem Pointeur vers la sémaphore à détruire
    @return int 0 si la destruction a réussi, -1 sinon 
 */
int thread_sem_destroy(thread_sem_t *sem){
    thread_mutex_lock((*sem)->lock);
    atomic_store(&((*sem)->destroyed), 1);
    /**
     * Si des threads sont en attente, on les réveille
     */
    while (atomic_load(&((*sem)->current_resources)) < atomic_load(&((*sem)->max_resources))) {
        thread_yield();
    }
    thread_mutex_unlock((*sem)->lock);
    thread_mutex_destroy((*sem)->lock);
    free(*sem);
    return 0;
}

/**
    @fn int thread_sem_wait(thread_sem_t sem)
    @brief Fonction d'attente d'un thread sur une sémaphore
    @param sem Sémaphore sur laquelle le thread attend
    @return int 0 si le thread a réussi à acquérir la ressource, -1 si la sémaphore a été détruite
 */
int thread_sem_wait(thread_sem_t sem) {
    thread_mutex_lock(sem->lock);
    
    /**
     * Tant que le nombre de ressources est null, on passe la main,
     * sinon on décrémente le nombre de ressources
     */
    while (atomic_load(&(sem->current_resources)) == 0) {
        /**
         * Si la sémaphore a été détruite, on renvoie -1
         */
        if (atomic_load(&(sem->destroyed))) {
            thread_mutex_unlock(sem->lock);
            return -1;
        }
        
        thread_mutex_unlock(sem->lock);
        thread_yield();
        thread_mutex_lock(sem->lock);
    }
    
    atomic_fetch_sub(&(sem->current_resources), 1);
    thread_mutex_unlock(sem->lock);
    return 0;
}

/**
    @fn int thread_sem_post(thread_sem_t sem)
    @brief Fonction d'incrémentation du sémaphore
    @param sem Sémaphore à incrémenter
    @return int 0 si l'incrémentation a réussi, -1 sinon
 */
int thread_sem_post(thread_sem_t sem){
    thread_mutex_lock(sem->lock);
    
    /**
     * Si le nombre de ressources courantes est bien inférieur au nombre de ressources maximales
     */
    if (atomic_load(&(sem->current_resources)) < atomic_load(&(sem->max_resources))) {
        atomic_fetch_add(&(sem->current_resources), 1);
    }
    
    thread_mutex_unlock(sem->lock);
    return 0;
}

/************************************
    Implementation des Barrières
*************************************/

/**
    @struct thread_barrier
    @brief Structure de nos barrières
 */
struct thread_barrier{
    atomic_int count;       /*!<Nombre total de threads devant atteindre la barrière */
    atomic_int waiting;     /*!<Nombre de threads en attente sur la barrière*/
    thread_mutex_t *lock;   /*!<Mutex pour la protection de la barrière*/
};

/**
    @fn int thread_barrier_init(thread_barrier_t *barrier, unsigned int count)
    @brief fonction d'initialisation de notre structure de barrière
    @brief Initialise une barrière avec un nombre donné de threads attendus
    @param barrier Pointeur vers la structure de barrière à initialiser
    @param count Nombre total de threads devant atteindre la barrière
    @return 0 si l'initialisation a réussi, -1 sinon 
 */
int thread_barrier_init(thread_barrier_t *barrier, unsigned int count){
    /**
     *  On s'assure que count est strictement positif
     *  Car on ne peut attendre un nombre négatif ou zéro thread 
     */
    if (count <= 0) {
        return -1;
    }
    /**
     * on initialise les champs de notre barrier 
     */
    *barrier = malloc(sizeof(struct thread_barrier));
    if (*barrier == NULL) {
        return -1;
    }
    atomic_init(&((*barrier)->count), count);
    atomic_init(&((*barrier)->waiting), 0);
    (*barrier)->lock = malloc(sizeof(thread_mutex_t));
    if (thread_mutex_init((*barrier)->lock) != 0) {
        free(*barrier);
        return -1;
    }
    return 0;
}

/**
    @fn int thread_barrier_wait(thread_barrier_t barrier)
    @brief fonction d'attente sur une barrier
    @brief Attend sur la barrière jusqu'à ce que tous les threads attendus aient atteint la barrière
    @param barrier Structure de barrière à attendre
    @return 0 si tous les threads attendus ont atteint la barrière
 */
int thread_barrier_wait(thread_barrier_t barrier){
    thread_mutex_lock(barrier->lock);
   
    /**
     * Si le nombre de threads en attente est égal au nombre de threads attendu
     * Bingo !!! 
     */
    if (atomic_load(&(barrier->waiting)) == atomic_load(&(barrier->count))) {
        atomic_store(&(barrier->waiting), 0);
        atomic_fetch_sub(&(barrier->count), atomic_load(&(barrier->count)));
        thread_mutex_unlock(barrier->lock);
        return 0;
    } else {
        /**
         * incrémente le nombre de thread en attente sur la barrier
         */
        atomic_fetch_add(&(barrier->waiting), 1);

        /**
         * Sinon on suspend le thread en attente jusqu'à ce que le soit le cas
         */
        while (atomic_load(&(barrier->waiting)) != atomic_load(&(barrier->count))) {
            thread_mutex_unlock(barrier->lock);
            thread_yield();
            thread_mutex_lock(barrier->lock);
        }
        /**
         * On remet les compteurs à zéro 
         */
        atomic_store(&(barrier->waiting), 0);
        atomic_fetch_sub(&(barrier->count), atomic_load(&(barrier->count)));
        thread_mutex_unlock(barrier->lock);
        return 0;
    }
}

/**
    @fn int thread_barrier_destroy(thread_barrier_t *barrier)
    @brief Détruit une barrière et libère les ressources associées
    @param barrier Pointeur vers la structure de barrière à détruire
    @return 0 si la destruction a réussi, -1 sinon
 */
int thread_barrier_destroy(thread_barrier_t *barrier){
    thread_mutex_destroy((*barrier)->lock);
    free((*barrier)->lock);
    free(*barrier);
    return 0;
}

/************************************
    Implementation des thread_cond
*************************************/

/**
    @struct thread_cond
    @brief Structure pour une condition de thread
*/
struct thread_cond {
    TAILQ_HEAD(cond_queue, thread) queue_cond; /*!<File d'attente de threads en attente de la condition*/
};

/**
    @fn int thread_cond_init(thread_cond_t *cond)
    @brief Initialise la condition spécifiée
    @param cond Pointeur vers la condition à initialiser
    @return 0 si réussi, -1 si une erreur est survenue
    Cette fonction alloue de la mémoire pour la condition spécifiée et initialise ses champs, y compris sa file d'attente.
*/
int thread_cond_init(thread_cond_t *cond) {
    /**
     * Allocation de la mémoire pour la condition 
     */
    *cond = malloc(sizeof(struct thread_cond));
    if (*cond == NULL) {
        return -1;
    }
    /**
     * initialisation des champs de la condition
     */
    TAILQ_INIT(&((*cond)->queue_cond));
    return 0;
}

/**
    @brief Attend la condition spécifiée
    @param cond Condition à attendre
    @param mutex Verrou à utiliser pour la synchronisation
    @return 0 si réussi, -1 si une erreur est survenue
    Cette fonction ajoute le thread courant à la file d'attente de la condition, relâche le verrou, suspend le thread courant, puis acquiert à nouveau le verrou.
*/
int thread_cond_wait(thread_cond_t cond, thread_mutex_t *mutex) {
    /**
    * Vérifie que les arguments sont bien définis 
    */
    if (cond == NULL || mutex == NULL) {
        return -1;
    }
    
    /**
     * Ajouter le thread courant à la file d'attente de la condition
     */
    TAILQ_INSERT_TAIL(&(cond->queue_cond), current_thread, threads);

    /**
     * Relâcher le verrou et suspendre le thread courant
     */
    thread_mutex_unlock(mutex);
    TAILQ_REMOVE(&ready, current_thread, threads);
    thread_yield();

    /**
     * Récupérer le premier thread de la file d'attente de la condition
     */
    thread_t thread = TAILQ_FIRST(&(cond->queue_cond));
    while (thread != current_thread) {
        /**
         * Si ce n'est pas le thread courant, le retirer de la file d'attente et le remettre dans la queue de threads prêts
         */
        TAILQ_REMOVE(&(cond->queue_cond), thread, threads);
        add_thread_to_queue_tail(thread);
        thread = TAILQ_FIRST(&(cond->queue_cond));
    }

    /**
     * Retirer le thread courant de la file d'attente de la condition
     */
    TAILQ_REMOVE(&(cond->queue_cond), current_thread, threads);

    /**
     * Acquérir à nouveau le verrou
     */
    thread_mutex_lock(mutex);
    return 0;
}

/**
    @fn int thread_cond_signal(thread_cond_t cond)
    @brief Signale un thread en attente de la condition spécifiée
    @param cond Condition à signaler
    @return 0 si réussi, -1 si une erreur est survenue
    Cette fonction récupère le premier thread en attente de la condition spécifiée et le remet dans la queue de threads prêts.
*/
int thread_cond_signal(thread_cond_t cond) {
    if (cond == NULL) {
        return -1;
    }
    /**
     * Récupérer le premier thread de la file d'attente de la condition
     */
    thread_t thread = TAILQ_FIRST(&(cond->queue_cond));

    if (thread != NULL) {
        /**
         * Si la file d'attente n'est pas vide, retirer le premier thread et le remettre dans la queue de threads prêts
         */
        TAILQ_REMOVE(&(cond->queue_cond), thread, threads);
        add_thread_to_queue_tail(thread);
    }

    return 0;
}

/**
    @fn int thread_cond_broadcast(thread_cond_t cond)
    @brief Signale tous les threads en attente de la condition spécifiée
    @param cond Condition à signaler
    @return 0 si réussi, -1 si une erreur est survenue
    Cette fonction récupère tous les threads en attente de la condition spécifiée et les remet dans la queue de threads prêts.
*/
int thread_cond_broadcast(thread_cond_t cond) {
    if (cond == NULL) {
        return -1;
    }
    /**
     * Pour chaque thread de la file d'attente de la condition, le retirer et le remettre dans la queue de threads prêts
     */
    while (!TAILQ_EMPTY(&(cond->queue_cond))) {
        thread_t thread = TAILQ_FIRST(&(cond->queue_cond));
        TAILQ_REMOVE(&(cond->queue_cond), thread, threads);
        add_thread_to_queue_tail(thread);
    }

    return 0;
}

/**
    @fn int thread_cond_destroy(thread_cond_t *cond)
    @brief Détruit la condition spécifiée
    @param cond Pointeur vers la condition à détruire
    @return 0 si réussi, -1 si une erreur est survenue
    Cette fonction libère la mémoire allouée pour la condition spécifiée.
*/
int thread_cond_destroy(thread_cond_t *cond) {
    if (cond == NULL || *cond == NULL) {
        return -1;
    }

    free(*cond);
    *cond = NULL;

    return 0;
}

/*******************************  
    Implementation Signaux 
********************************/

int thread_kill(thread_t *thread, int signal){
    if(signal<NBR_SIGNALS && signal >= 0){
        signal_t * signal_to_send = get_signal((signal_type)signal);
        if(get_thread_current_signal(thread)<MAX_SIGNALS-1){
            int index = get_thread_current_signal(thread);
            set_thread_signal_array(thread,index,signal_to_send);
            printf("thread %d sends %s signal to thread %d\n",current_thread->id,signal_enum_to_string(signal),(*thread)->id);
            return 1;
        }
    }
    return 0;
}

int thread_signal_wait(int signal) {
    if(signal<NBR_SIGNALS && signal >= 0){
        signal_t * signal_to_wait_for = get_signal((signal_type)signal);
        if (current_thread->th == NULL) {
            return 0;
        }
        int index = current_thread->th->current_signal-1;
        while(current_thread->th->signal_array[index] != signal_to_wait_for){
            thread_yield();
        }
        if (signal_to_wait_for->handler == NULL) {
            default_signal_handler(signal_to_wait_for->type);
        }
        else {
            signal_to_wait_for->handler(signal_to_wait_for->type);
        }
        return 1;
    }
    return 0;
}

int thread_signal_timed_wait(int signal, int timeout) {
    if(signal<NBR_SIGNALS && signal >= 0){
        signal_t * signal_to_wait_for = get_signal((signal_type)signal);
        if (current_thread->th == NULL) {
            return -1;
        }
        int i = 0;
        int index = current_thread->th->current_signal -1;
        while(current_thread->th->signal_array[index] != signal_to_wait_for){
            thread_yield();
            i++;
            if(i>timeout){
                break;
            }
        }
        if (signal_to_wait_for->handler == NULL) {
            default_signal_handler(signal_to_wait_for->type);
        }
        else {
            signal_to_wait_for->handler(signal_to_wait_for->type);
        }
        return 0;
    }
    return -1;
}

void thread_sigaction_t(int signal, sig_handler new_handler) {
    if(signal<NBR_SIGNALS && signal >= 0){
        signal_t * signal_got = get_signal((signal_type)signal);
        if (signal_got->type == SIG_KILL && new_handler == signal_ignore) {
            fprintf(stderr, "Signal SIG_KILL can not be ignored\n");
        }
        else {
            signal_got->old_handler = signal_got->handler;
            signal_got->handler = new_handler;
        }
    }
}

void signal_ignore(signal_type sig) {
    printf("Ignoring the signal %d received\n", (int)sig);
}

signal_t *get_signal (signal_type type) {
    switch (type){
    case SIG_USER1 :
        return &signals[0];
        break;
    case SIG_USER2 :
        return &signals[1];
        break;
    case SIG_KILL :
        return &signals[3];
        break;
    default:
        return &no_signal;
        break;
    }
}

int  get_thread_current_signal(thread_t *thread) {
    return (*thread)->th->current_signal;
}

void set_thread_signal_array(thread_t *thread, int index, signal_t *signal){
    (*thread)->th->signal_array[index] = signal;
    (*thread)->th->current_signal = index+1;
}

void thread_signal_init(thread_signal_t *th){
    if(th != NULL){
        int i = 0;
        for (i=0; i<MAX_SIGNALS; i++){
            th->signal_array[i] = &no_signal;
        }
        th->current_signal = 0;
    }
}

int thread_signal_free(thread_signal_t *th){
  if(th != NULL){
    free(th);
    return 0;
  }
  return -1;
}

void default_signal_handler(signal_type sig) {
    switch (sig){
    case SIG_KILL:
        printf("thread %d receives signal SIG_KILL \n", current_thread->id);
        thread_exit(NULL);
        break;
    case SIG_USER1 :
        printf("thread %d receives SIG_USER1 \n",current_thread->id);
        break;
    case SIG_USER2 :
        printf("thread %d receives SIG_USER2 \n",current_thread->id);
        break;
    default:
        break;
    }
  
}

char * signal_enum_to_string(int signal){
    switch(signal){
        case SIG_USER1: return "SIG_USER1"; break;
        case SIG_USER2: return "SIG_USER2"; break;
        case SIG_KILL : return "SIG_KILL"; break;
        default: return "unknown"; break;
    }
}
/*******************************  
    Side Function Implementation 
********************************/

/**
 * Fonction d'initialisation de notre librairie
 */
void initializer(void){
    queue_init();
    main_thread = thread_init();
    current_thread = main_thread;
    add_thread_to_queue_tail(main_thread);
    number_thread++;
    #ifdef PREEMPTION
    install_handler();
    #ifdef PREEMPTION
    init_timer(TIMESLICE);
    #endif
    #ifdef RR
    init_timer(main_thread->time_slice);
    #endif
    setitimer(ITIMER_VIRTUAL, &timer, NULL);
    #endif
}

/**
 * Fonction de libération de notre librairie
 */
void cleaner(void){
    if(main_thread!=NULL){
        VALGRIND_STACK_DEREGISTER(main_thread->valgrind_stackid);
        #ifdef STACKOVERFLOW
        free(main_thread->uc.uc_stack.ss_sp-PAGE_SIZE);
        #else
        free(main_thread->uc.uc_stack.ss_sp);
        #endif
        thread_signal_free(main_thread->th); 
        free(main_thread);
    }
}


#ifdef STACKOVERFLOW
/**
 * Fonction de gestion de segfault
 */
void segfault_handler(int sig, siginfo_t *si, void *unused) {
    (void) unused;
    (void) si;
    (void) sig;
    printf("segfault_handler\n");
    
    if(current_thread == main_thread){
        printf("Main segfaulted\n");
        _exit(1);
    }
    else {
        printf("Signal SIGSEGV (segfault) reçu in %p.\n", current_thread);
        printf("Adresse mémoire problématique : %p\n", si->si_addr);
        thread_exit(NULL);
    }

}

void* sp[SIGSTKSZ];
#endif 
/**
 * Créer et initialiser un thread 
 */
struct thread * thread_init(void){
    //printf("init\n");
    struct thread * thread = malloc(sizeof(struct thread));
    thread->th = malloc(sizeof(thread_signal_t));
    thread_signal_init(thread->th);
    thread->retval = NULL;
    getcontext(&thread->uc);
    #ifdef STACKOVERFLOW
    void * stack;
    //printf("titi1\n");
    if(posix_memalign(&stack, PAGE_SIZE, CONTEXT_STACK_SIZE+PAGE_SIZE) != 0){
        printf("Erreur lors de l'initialisation de la stack du thread\n");
        fflush(stdout);
        return NULL;
    }
    if (mprotect(stack, PAGE_SIZE, 0) != 0) {
        printf("Erreur lors de la protection en écriture et en lecture de la fin de la pile\n");
        return NULL;
    }
    thread->uc.uc_stack.ss_sp =stack+PAGE_SIZE;

    if(current_thread == NULL){
        // Débordement de pile
        stack_t ss;
        ss.ss_sp = sp;
        ss.ss_size = SIGSTKSZ;
        ss.ss_flags = 0;

        // Définir la pile alternative avec sigaltstack
        if (sigaltstack(&ss, NULL) == -1) {
            perror("Erreur de définition de la pile alternative");
            return NULL;
        }
        // Installer un gestionnaire de signal pour SIGSEGV
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sa.sa_sigaction = segfault_handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGSEGV, &sa, NULL) == -1) {
            perror("Erreur de l'installation du gestionnaire de signal pour SIGSEGV");
            return NULL;
        }
    }
    #else
    thread->uc.uc_stack.ss_sp = malloc(CONTEXT_STACK_SIZE);
    #endif
    thread->uc.uc_stack.ss_size = CONTEXT_STACK_SIZE;
    thread->valgrind_stackid=VALGRIND_STACK_REGISTER(
                            thread->uc.uc_stack.ss_sp,
                            thread->uc.uc_stack.ss_sp + 
                            thread->uc.uc_stack.ss_size
                            );
    thread->uc.uc_link = NULL;
    thread->id =number_thread;
    thread->is_done=0;
    thread->joiner = NULL;
    thread->is_locked=0;
    thread->id_first = -1; //Id positif seulement. On peut pas mettre 0 sinon on détecte une boucle avec lui même -> Deadlock
    thread-> priority = MAX_PRIORITY-1%(number_thread+1);

    return thread;
}

/**
 * Changer de contexte
 */
void handle_swap(struct thread *thread,struct thread *next){
    #ifdef PREEMPTION
    setitimer(ITIMER_VIRTUAL, &timer, &remainingtime);
    enable_interrupt();
    #endif
    swapcontext(&thread->uc,&next->uc);
}
/**
 * fonction intermédiaire pour nos thread
 */
void call_function(void *(*func)(void*), void *funcarg){
    void * retval = func(funcarg);
    thread_exit(retval);
}


/**
 * Installer le timer pour la préemption 
 */
void timer_handler(){
    #ifdef PREEMPTION
    disable_interrupt();
    if(is_queue_empty()){
        setitimer(ITIMER_VIRTUAL, &timer, NULL);
    }
    thread_yield();
    #endif
}
/**
 * Initialiser le timer pour le mécanisme préemption
 */
void init_timer(int time_slice){
    getitimer(ITIMER_VIRTUAL,&timer);
    timer.it_value.tv_sec = time_slice/1000;
    timer.it_value.tv_usec = (time_slice * 1000) % 1000000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
}
/**
 * Installer le signal handler pour le signal de préemption
 */
void install_handler(void){
    act_timer.sa_handler = &timer_handler;
    act_timer.sa_flags=0;
    sigemptyset(&act_timer.sa_mask);
    sigaddset(&act_timer.sa_mask, SIGVTALRM);
    sigaction(SIGVTALRM, &act_timer, NULL);
}
/**
 * Permettre au thread de capturer le signal de préemption 
 */
void enable_interrupt(void){
    sigemptyset(&act_timer.sa_mask);
}
/**
 * Bloquer la capture du signal de préemption par le thread
 */
void disable_interrupt(void){
     sigaddset(&act_timer.sa_mask, SIGVTALRM);
}


/**
 * Initialisation de la file d'attente 
 */
void queue_init(void) {
    /**
     *  code pour l'ordonnancement avec FIFO 
     */
    #ifdef FIFO
    TAILQ_INIT(&ready);
    #endif

    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    for (int i = 0; i < MAX_PRIORITY; i++) {
        ready[i].priority = i;
        TAILQ_INIT(&ready[i].threads);
    }
    #endif
}
/**
 * Fonction pour ajouter un thread à la queue de la file d'attente
 */
void add_thread_to_queue_tail(struct thread *thread){
    /**
     *  code pour l'ordonnancement avec FIFO 
     */
    #ifdef FIFO
    TAILQ_INSERT_TAIL(&ready,thread, threads);
    #endif

    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    TAILQ_INSERT_TAIL(&ready[thread->priority].threads, thread, threads);
    #endif
}
/**
 * Fonction pour ajouter un thread à la tête de la file d'attente correspondant à sa priorité
 */
void add_thread_to_queue_head(struct thread *thread){
    /**
     *  code pour l'ordonnancement avec FIFO 
     */
    #ifdef FIFO
    TAILQ_INSERT_HEAD(&ready,thread,threads);
    #endif

    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    TAILQ_INSERT_HEAD(&ready[thread->priority].threads, thread, threads);
    #endif
}
/**
 * Fonction pour supprimer un thread de la queue de la file d'attente correspondant à sa priorité
 */
void remove_thread_from_queue(struct thread *thread) {
    /**
     *  code pour l'ordonnancement avec FIFO 
     */
    #ifdef FIFO
    TAILQ_REMOVE(&ready,thread,threads);
    #endif

    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    TAILQ_REMOVE(&ready[thread->priority].threads, thread, threads);
    #endif
}
/**
 *  Fonction pour récupérer le premier thread dans la file d'attente avec la priorité la plus haute
 */ 
struct thread* get_thread(void) {
    /**
     *  code pour l'ordonnancement avec FIFO 
     */
    #ifdef FIFO
    return TAILQ_FIRST(&ready);
    #endif

    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        if (!TAILQ_EMPTY(&ready[i].threads)) {
            return TAILQ_FIRST(&ready[i].threads);
        }
    }
    #endif
}
/**
 * Fonction pour mettre à jour la priorité d'un thread
 */
void update_thread_priority(struct thread *thread) {
    /**
     *  code pour l'ordonnancement avec FIFO 
     */
    #ifdef FIFO
    thread->priority = thread->priority;
    #endif
    
    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    thread->priority = thread->priority-1;
    if (thread->priority < 0) {
        thread->priority = MAX_PRIORITY-1;
    }
    #endif
}
/**
 * Fonction pour définir la priorité d'un thread
 */
void set_thread_priority(struct thread *thread, int priority) {
    thread->priority = priority;
}
/**
 *  Fonction pour vérifier que toute les files sont vides 
 */
int is_queue_empty(void){
    #ifdef FIFO
    if(!TAILQ_EMPTY(&ready)){
        return -1;
    }
    #endif
    /**
     *  code pour l'ordonnancement avec priorité 
     */
    #ifdef PRIORITY
    for (int i = MAX_PRIORITY - 1; i >= 0; i--) {
        if (!TAILQ_EMPTY(&ready[i].threads)) {
            return -1;
        }
    }
    #endif
    return 0;
}