#include <pthread.h>
#include <assert.h>
#include <stdio.h>

/* Internal dump funcions for dlock hacking only */
#undef LOCK_DEBUG
#ifdef LOCK_DEBUG
#define DLOCKprintf(fmt, args...) printf(fmt, args)
#else
#define DLOCKprintf(fmt, args...)
#endif

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
static inline int ___pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DLOCKprintf("destroying %d %p\n", (int)pthread_self(), mutex);
	return pthread_mutex_destroy(mutex);
}

static inline int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name, char *fn, int ln)
{
	int ret;
	DLOCKprintf("initing %d %p %s\n", (int)pthread_self(), mutex, mutex_name);
	ret = pthread_mutex_init(mutex, v);	
	ordering_init(mutex, v, mutex_name, fn, ln);
	return ret;
}

static inline int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	int ret;
	DLOCKprintf("locking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
	ret = pthread_mutex_lock(mutex);
	ordering_lock(mutex, (int)pthread_self());
	return ret;
}

static inline int ___pthread_mutex_try_lock(pthread_mutex_t *mutex)
{
	DLOCKprintf("try locking %d %p\n", (int)pthread_self(), mutex);
	return pthread_mutex_trylock(mutex);
}

static inline int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	DLOCKprintf("unlocking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
	ordering_unlock(mutex, (int)pthread_self());
	return pthread_mutex_unlock(mutex);
}

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

