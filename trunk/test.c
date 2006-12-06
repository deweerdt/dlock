#include "dlock.h"

int main()
{
	pthread_mutex_t mutexA = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutexB = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutexC = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutexD = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mutexE = PTHREAD_MUTEX_INITIALIZER;
	MUTEX_LOCK(&mutexA);
	MUTEX_LOCK(&mutexB);
	MUTEX_UNLOCK(&mutexB);
	MUTEX_UNLOCK(&mutexA);

	MUTEX_LOCK(&mutexA);
	MUTEX_LOCK(&mutexB);
	MUTEX_UNLOCK(&mutexB);
	MUTEX_UNLOCK(&mutexA);

	MUTEX_LOCK(&mutexC);
	MUTEX_UNLOCK(&mutexC);

	MUTEX_LOCK(&mutexD);
	MUTEX_UNLOCK(&mutexD);

	MUTEX_LOCK(&mutexE);

	ordering_dump();
	return 1;
}
