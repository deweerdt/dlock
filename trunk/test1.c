/*
    test1.c - dlock uninstrumented sample program
    Copyright (C) 2006  Frederik Deweerdt <frederik.deweerdt@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


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
