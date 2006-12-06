#include <pthread.h>
#include <assert.h>
void latency_time_lock(pthread_mutex_t *mutex, int tid);
void latency_time_unlock(pthread_mutex_t *mutex, int tid);
void ordering_lock(pthread_mutex_t *mutex, int tid);
void ordering_unlock(pthread_mutex_t *mutex, int tid);

#undef LOCK_DEBUG
#ifdef LOCK_DEBUG
#define LOCKprintf(...) printf(...)
#else
#define LOCKprintf(...)
#endif

/** some encapsulation macros */
static inline int ___pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	LOCKprintf("destroying %d %p\n", (int)pthread_self(), mutex);
	return pthread_mutex_destroy(mutex);
}

static inline int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name)
{
	LOCKprintf("initing %d %p %s\n", (int)pthread_self(), mutex, mutex_name);
	return pthread_mutex_init(mutex, v);
}
#define LOCK_DEBUG_ORD 1

static inline int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	int ret;
	LOCKprintf("locking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
	ret = pthread_mutex_lock(mutex);
#ifdef LOCK_DEBUG_LAT
	latency_time_lock(mutex, (int)pthread_self());
#endif
#ifdef LOCK_DEBUG_ORD
	ordering_lock(mutex, (int)pthread_self());
#endif
	return ret;
}

static inline int ___pthread_mutex_try_lock(pthread_mutex_t *mutex)
{
	LOCKprintf("try locking %d %p\n", (int)pthread_self(), mutex);
	return pthread_mutex_trylock(mutex);
}

static inline int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	LOCKprintf("unlocking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
#ifdef LOCK_DEBUG_LAT
	latency_time_unlock(mutex, (int)pthread_self());
#endif
#ifdef LOCK_DEBUG_ORD
	ordering_unlock(mutex, (int)pthread_self());
#endif
	return pthread_mutex_unlock(mutex);
}


/** unlocks a mutex */
#define MUTEX_UNLOCK(a) assert(___pthread_mutex_unlock(a,#a, __FILE__, __LINE__)==0)

/** locks a mutex */
#define MUTEX_LOCK(a)   assert(___pthread_mutex_lock(a,#a, __FILE__, __LINE__)==0)

/** locks a mutex */
#define MUTEX_TRY_LOCK(a)   assert(___pthread_mutex_try_lock(a)==0)

/** inits a mutex */
#define MUTEX_INIT(a)   assert(___pthread_mutex_init(a,0,#a)==0)

/** destroys a mutex */
#define MUTEX_DESTROY(a) assert(___pthread_mutex_destroy(a)==0)

void ordering_dump() ;

