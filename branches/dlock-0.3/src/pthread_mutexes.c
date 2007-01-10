/*
    pthread_mutexes.c - hooks for the pthread_mutex_* API
    Copyright (C) 2006,2007 Frederik Deweerdt <frederik.deweerdt@gmail.com>

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

#include "dlock.h"
#include "dlock_core.h"

/* placeholders for the _true_ libpthread functions */
int (*o_pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int (*o_pthread_mutex_lock)(pthread_mutex_t *);
int (*o_pthread_mutex_trylock)(pthread_mutex_t *);
int (*o_pthread_mutex_unlock)(pthread_mutex_t *);

int ___pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DLOCKprintf("destroying %d %p\n", (int)pthread_self(), mutex);
	return pthread_mutex_destroy(mutex);
}

int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name, char *fn, int ln)
{
	int ret;
	DLOCKprintf("initing %d %p %s\n", (int)pthread_self(), mutex, mutex_name);
	ret = o_pthread_mutex_init(mutex, v);	
	dlock_lock_init(mutex, v, mutex_name, fn, ln, MUTEX);
	return ret;
}

int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	int ret;
	DLOCKprintf("locking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
	ret = o_pthread_mutex_lock(mutex);
	dlock_lock(mutex, (int)pthread_self());
	return ret;
}

int ___pthread_mutex_try_lock(pthread_mutex_t *mutex)
{
	DLOCKprintf("try locking %d %p\n", (int)pthread_self(), mutex);
	return o_pthread_mutex_trylock(mutex);
}

int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	DLOCKprintf("unlocking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
	dlock_unlock(mutex, (int)pthread_self());
	return o_pthread_mutex_unlock(mutex);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	char buf[16];
	sprintf(buf, "%p", mutex);
	dlock_lock_init(mutex, (void *)attr, buf, "?", 0, MUTEX);
	return o_pthread_mutex_init(mutex, attr);
}
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	dlock_lock(mutex, pthread_self());
	return o_pthread_mutex_lock(mutex);
}
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	return o_pthread_mutex_trylock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	dlock_unlock(mutex, pthread_self());
	return o_pthread_mutex_unlock(mutex);
}
