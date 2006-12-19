#include <pthread.h>
#include <assert.h>
#include <stdio.h>

enum {
	DLOCK_INIT_CALIBRATE = (1 << 0),
	DLOCK_INIT_HANDLE_USR1 = (1 << 1),
	DLOCK_INIT_HANDLE_USR2 = (1 << 2),
};

#define DLOCK_ORDERING
#define DLOCK_LATENCY

#ifndef DLOCK_ORDERING
static void ordering_init(pthread_mutex_t *mutex, void *v, const char *mutex_name, char *fn, int ln) {}
static void ordering_lock(pthread_mutex_t *mutex, unsigned int tid) {}
static void ordering_unlock(pthread_mutex_t *mutex, unsigned int tid) {}
static void ordering_dump() {}
void ordering_gen_dot() {}
static void dlock_init(int flags) {}
#else /* DLOCK_ORDERING */
void ordering_init(pthread_mutex_t *mutex, void *v, const char *mutex_name, char *fn, int ln);
void ordering_lock(pthread_mutex_t *mutex, unsigned int tid);
void ordering_unlock(pthread_mutex_t *mutex, unsigned int tid);
void ordering_dump();
void ordering_gen_dot();
void dlock_init(int flags);
#endif /* DLOCK_ORDERING */

/** some encapsulation macros */
int ___pthread_mutex_destroy(pthread_mutex_t *mutex);
int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name, char *fn, int ln);
int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln);
int ___pthread_mutex_try_lock(pthread_mutex_t *mutex);
int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln);

/** unlocks a mutex */
#define MUTEX_UNLOCK(a) ___pthread_mutex_unlock(a,#a, __FILE__, __LINE__)

/** locks a mutex */
#define MUTEX_LOCK(a) ___pthread_mutex_lock(a,#a, __FILE__, __LINE__)

/** locks a mutex */
#define MUTEX_TRY_LOCK(a) ___pthread_mutex_try_lock(a)

/** inits a mutex */
#define MUTEX_INIT(a) ___pthread_mutex_init(a,0,#a, __FILE__, __LINE__)

/** destroys a mutex */
#define MUTEX_DESTROY(a) ___pthread_mutex_destroy(a)

