#ifndef __THREAD_H__
#define __THREAD_H__

#ifndef USE_PTHREAD


/* identifiant de thread
 * NB: pourra être un entier au lieu d'un pointeur si ca vous arrange,
 *     mais attention aux inconvénient des tableaux de threads
 *     (consommation mémoire, cout d'allocation, ...).
 */
struct thread;
typedef struct thread* thread_t;

/* recuperer l'identifiant du thread courant.
 */
extern thread_t thread_self(void);

/* creer un nouveau thread qui va exécuter la fonction func avec l'argument funcarg.
 * renvoie 0 en cas de succès, -1 en cas d'erreur.
 */
extern int thread_create(thread_t *newthread, void *(*func)(void *), void *funcarg);

/* passer la main à un autre thread.
 */
extern int thread_yield(void);

/* attendre la fin d'exécution d'un thread.
 * la valeur renvoyée par le thread est placée dans *retval.
 * si retval est NULL, la valeur de retour est ignorée.
 */
extern int thread_join(thread_t thread, void **retval);

/* terminer le thread courant en renvoyant la valeur de retour retval.
 * cette fonction ne retourne jamais.
 *
 * L'attribut noreturn aide le compilateur à optimiser le code de
 * l'application (élimination de code mort). Attention à ne pas mettre
 * cet attribut dans votre interface tant que votre thread_exit()
 * n'est pas correctement implémenté (il ne doit jamais retourner).
 */
extern void thread_exit(void *retval);

/* Interface possible pour les mutex */
typedef struct thread_mutex { thread_t locked; } thread_mutex_t;
int thread_mutex_init(thread_mutex_t *mutex);
int thread_mutex_destroy(thread_mutex_t *mutex);
int thread_mutex_lock(thread_mutex_t *mutex);
int thread_mutex_unlock(thread_mutex_t *mutex);

/* Interface Sémaphore */
struct thread_sem;
typedef struct thread_sem *thread_sem_t;
int thread_sem_init(thread_sem_t *sem, int pshared, unsigned int value);
int thread_sem_destroy(thread_sem_t *sem);
int thread_sem_wait(thread_sem_t sem);
int thread_sem_post(thread_sem_t sem);

/* Interface Barrières */
struct thread_barrier;
typedef struct thread_barrier* thread_barrier_t;

int thread_barrier_init(thread_barrier_t *barrier, unsigned int count);
int thread_barrier_wait(thread_barrier_t barrier);
int thread_barrier_destroy(thread_barrier_t *barrier);

/* Interface thread_cond */
struct thread_cond;
typedef struct thread_cond* thread_cond_t;

int thread_cond_init(thread_cond_t *cond);
int thread_cond_wait(thread_cond_t cond, thread_mutex_t *mutex);
int thread_cond_signal(thread_cond_t cond);
int thread_cond_broadcast(thread_cond_t cond);
int thread_cond_destroy(thread_cond_t *cond);


/* Signaux */
typedef enum signals {  
        SIG_USER1=0,
        SIG_USER2,
        SIG_KILL, 
        Error = -1 
} signal_type;

typedef void(*sig_handler)(signal_type);

typedef struct signal_t {
    signal_type type;
    sig_handler handler;
    sig_handler old_handler;
} signal_t;

typedef struct thread_signal_t {
    signal_t *signal_array[10];
    int current_signal;
} thread_signal_t;

int thread_kill(thread_t *thread, int signal);
int thread_signal_wait(int signal);
int thread_signal_timed_wait(int signal, int timeout);
void thread_sigaction_t(int signal, sig_handler new_handler);
signal_t *get_signal (signal_type type);
int  get_thread_current_signal(thread_t *thread);
void set_thread_signal_array(thread_t *thread, int index, signal_t *signal);
void thread_signal_init(thread_signal_t *th);
int thread_signal_free(thread_signal_t *th);
void default_signal_handler(signal_type sig);
void signal_ignore(signal_type sig);
char * signal_enum_to_string(int signal);

/***************************  
    Side function definition 
****************************/
/**
 * Fonction d'initialisation de notre librairie
 */
void initializer(void)__attribute__((constructor));

/**
 * Fonction de libération de notre librairie
 */
void cleaner(void)__attribute__((destructor));

/**
 * Créer et initialiser un thread 
 */
struct thread * thread_init(void);

/**
 *  Fonction pour récupérer le premier thread dans la file d'attente avec la priorité la plus haute
 */ 
struct thread* get_thread(void);

/**
 * Fonction pour mettre à jour la priorité d'un thread
 */
void update_thread_priority(struct thread *thread);


/**
 * Fonction pour définir la priorité d'un thread
 */
void set_thread_priority(struct thread *thread, int priority);

/**
 * fonction intermédiaire pour nos thread
 */
void call_function(void *(*func)(void*), void *funcarg);

/**
 * Initialisation de la file d'attente 
 */
void queue_init(void);

void queue_init_kthread(void);
/**
 * Fonction pour ajouter un thread à la queue de la file d'attente
 */
void add_thread_to_queue_tail(struct thread *thread);

void add_thread_to_kqueue_tail(struct thread *thread, int i );
/**
 * Fonction pour ajouter un thread à la tête de la file d'attente correspondant à sa priorité
 */
void add_thread_to_queue_head(struct thread *thread);

void add_thread_to_kqueue_head(struct thread *thread, int i);
/**
 * Fonction pour supprimer un thread de la queue de la file d'attente correspondant à sa priorité
 */
void remove_thread_from_queue(struct thread *thread);

void remove_thread_from_kqueue(struct thread *thread, int id);
/**
 *  Fonction pour vérifier que toute les files sont vides 
 */
int is_queue_empty(void);

/**
 * Changer de contexte
 */
void handle_swap(struct thread *thread,struct thread *next);

/**
 * Initialiser le timer pour le mécanisme préemption
 */
void init_timer(int time_slice);

/**
 * Installer le timer pour la préemption 
 */
void timer_handler();

/**
 * Installer le signal handler pour le signal de préemption
 */
void install_handler(void);

/**
 * Permettre au thread de capturer le signal de préemption 
 */
void enable_interrupt(void);

/**
 * Bloquer la capture du signal de préemption par le thread
 */
void disable_interrupt(void);

void print_queue(void);

struct thread *get_thread_multicore(void);
#else /* USE_PTHREAD */

/* Si on compile avec -DUSE_PTHREAD, ce sont les pthreads qui sont utilisés */
#include <sched.h>
#include <pthread.h>
#define thread_t pthread_t
#define thread_self pthread_self
#define thread_create(th, func, arg) pthread_create(th, NULL, func, arg)
#define thread_yield sched_yield
#define thread_join pthread_join
#define thread_exit pthread_exit

/* Interface possible pour les mutex */
#define thread_mutex_t            pthread_mutex_t
#define thread_mutex_init(_mutex) pthread_mutex_init(_mutex, NULL)
#define thread_mutex_destroy      pthread_mutex_destroy
#define thread_mutex_lock         pthread_mutex_lock
#define thread_mutex_unlock       pthread_mutex_unlock

#endif /* USE_PTHREAD */

#endif /* __THREAD_H__ */
