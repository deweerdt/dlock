/*
    test2.c - dlock perf test
    Copyright (C) 2006,2007  Frederik Deweerdt <frederik.deweerdt@gmail.com>

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


#include <pthread.h>
#include <unistd.h>
#include "dlock.h"

#define THREADS 20

pthread_mutex_t m1;
pthread_mutex_t m2;
pthread_mutex_t m3;
pthread_mutex_t m4;
void *f1(void *unused)
{
	MUTEX_LOCK(&m1);
	MUTEX_UNLOCK(&m1);
	MUTEX_LOCK(&m2);
	MUTEX_UNLOCK(&m2);
	MUTEX_LOCK(&m3);
	MUTEX_UNLOCK(&m3);
	MUTEX_LOCK(&m4);
	MUTEX_UNLOCK(&m4);

	return NULL;
}

void *f2(void *unused)
{
	MUTEX_LOCK(&m1);
	MUTEX_UNLOCK(&m1);
	MUTEX_LOCK(&m3);
	MUTEX_UNLOCK(&m3);
	MUTEX_LOCK(&m4);
	MUTEX_UNLOCK(&m4);
	MUTEX_LOCK(&m2);
	MUTEX_UNLOCK(&m2);

	return NULL;
}

void *f3(void *unused)
{
	MUTEX_LOCK(&m1);
	MUTEX_LOCK(&m2);
	MUTEX_UNLOCK(&m2);
	MUTEX_LOCK(&m4);
	MUTEX_UNLOCK(&m4);
	MUTEX_LOCK(&m3);
	MUTEX_UNLOCK(&m3);
	MUTEX_UNLOCK(&m1);

	return NULL;
}

void *f4(void *unused)
{
	MUTEX_LOCK(&m1);
	MUTEX_LOCK(&m4);
	MUTEX_UNLOCK(&m4);
	MUTEX_UNLOCK(&m1);
	MUTEX_LOCK(&m2);
	MUTEX_UNLOCK(&m2);
	MUTEX_LOCK(&m3);
	MUTEX_UNLOCK(&m3);

	return NULL;
}

int main(int argc, char **argv)
{
	int i,j;
	pthread_t t[THREADS];

	MUTEX_INIT(&m1, NULL);
	MUTEX_INIT(&m2, NULL);
	MUTEX_INIT(&m3, NULL);
	MUTEX_INIT(&m4, NULL);

	for (j = 0; j < 500; j++) {
		for (i = 0; i < THREADS; i++) {
			switch(i%4) {
				case 0:
					pthread_create(&t[i], NULL, f1, NULL);
					break;
				case 1:
					pthread_create(&t[i], NULL, f2, NULL);
					break;
				case 2:
					pthread_create(&t[i], NULL, f3, NULL);
					break;
				case 3:
					pthread_create(&t[i], NULL, f4, NULL);
					break;
			}
		}

		for (i = 0; i < THREADS; i++) {
			pthread_join(t[i], NULL);
		}
	}

	return 0;
}
