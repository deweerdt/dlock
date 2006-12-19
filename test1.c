#include <unistd.h>
#include <pthread.h>

pthread_mutex_t mutexA;
pthread_mutex_t mutexB;
pthread_mutex_t mutexC;
pthread_mutex_t mutexD;
pthread_mutex_t mutexE;

void *f(void *unused)
{
	pthread_mutex_lock(&mutexA);
	pthread_mutex_lock(&mutexB);
	pthread_mutex_unlock(&mutexB);
	pthread_mutex_unlock(&mutexA);
	
	return NULL;
}

int main()
{
	pthread_t t;

	pthread_mutex_init(&mutexA, NULL);
	pthread_mutex_init(&mutexB, NULL);
	pthread_mutex_init(&mutexC, NULL);
	pthread_mutex_init(&mutexD, NULL);
	pthread_mutex_init(&mutexE, NULL);

	pthread_create(&t, NULL, f, NULL);
#if 1
	pthread_mutex_lock(&mutexA);
	pthread_mutex_lock(&mutexB);
	pthread_mutex_unlock(&mutexB);
	pthread_mutex_lock(&mutexC);
	pthread_mutex_unlock(&mutexC);
	pthread_mutex_unlock(&mutexA);

#endif

	pthread_mutex_lock(&mutexC);
	pthread_mutex_unlock(&mutexC);

#if 1
	pthread_mutex_lock(&mutexD);
	pthread_mutex_unlock(&mutexD);

	pthread_mutex_lock(&mutexE);
	pthread_mutex_lock(&mutexD);
	pthread_mutex_unlock(&mutexD);
	//pthread_mutex_unlock(&mutexE);
#endif
	pthread_join(t, NULL);

	sleep(1000);
	return 1;
}
