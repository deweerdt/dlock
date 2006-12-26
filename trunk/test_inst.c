/*
    test2.c - dlock API sample file
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
#include "dlock.h"

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

	MUTEX_INIT(&mutexA, 0);
	MUTEX_INIT(&mutexB, 0);
	MUTEX_INIT(&mutexC, 0);
	MUTEX_INIT(&mutexD, 0);
	MUTEX_INIT(&mutexE, 0);

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

	dlock_dump();
	return 1;
}
