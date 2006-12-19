#include "dlock.h"
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t mutexA;
pthread_mutex_t mutexB;
pthread_mutex_t mutexC;
pthread_mutex_t mutexD;
pthread_mutex_t mutexE;

void *f(void *unused)
{
	MUTEX_LOCK(&mutexA);
	MUTEX_LOCK(&mutexB);
	MUTEX_UNLOCK(&mutexB);
	MUTEX_UNLOCK(&mutexA);
	
	return NULL;
}

int main()
{
	pthread_t t;

	dlock_init(~0UL);

	MUTEX_INIT(&mutexA);
	MUTEX_INIT(&mutexB);
	MUTEX_INIT(&mutexC);
	MUTEX_INIT(&mutexD);
	MUTEX_INIT(&mutexE);

	pthread_create(&t, NULL, f, NULL);
#if 1
	MUTEX_LOCK(&mutexA);
	MUTEX_LOCK(&mutexB);
	MUTEX_UNLOCK(&mutexB);
	MUTEX_LOCK(&mutexC);
	MUTEX_UNLOCK(&mutexC);
	MUTEX_UNLOCK(&mutexA);

#endif

	MUTEX_LOCK(&mutexC);
	MUTEX_UNLOCK(&mutexC);

#if 1
	MUTEX_LOCK(&mutexD);
	MUTEX_UNLOCK(&mutexD);

	MUTEX_LOCK(&mutexE);
	MUTEX_LOCK(&mutexD);
	MUTEX_UNLOCK(&mutexD);
	//MUTEX_UNLOCK(&mutexE);
#endif
	pthread_join(t, NULL);

	ordering_dump();
	ordering_gen_dot();
	return 1;
}
