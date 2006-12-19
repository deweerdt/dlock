/*
    dlock.h - dlock header file
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


#include <pthread.h>
#include <assert.h>
#include <stdio.h>

enum {
	DLOCK_INIT_CALIBRATE = (1 << 0),
	DLOCK_INIT_HANDLE_USR1 = (1 << 1),
	DLOCK_INIT_HANDLE_USR2 = (1 << 2),
	DLOCK_REGISTER_ATEXIT = (1 << 3),
	DLOCK_LOG_FILE = (1 << 4),
};


#ifndef DLOCK
static void dlock_dump() {}
void dlock_gen_dot() {}

static int ___pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return pthread_mutex_destroy(mutex);
}

static int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name, char *fn, int ln)
{
	return pthread_mutex_init(mutex, v);
}

static int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	return pthread_mutex_lock(mutex);
}

static int ___pthread_mutex_try_lock(pthread_mutex_t *mutex)
{
	return pthread_mutex_trylock(mutex);
}

static int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	return pthread_mutex_unlock(mutex);
}

#else /* DLOCK*/

int ___pthread_mutex_destroy(pthread_mutex_t *mutex);
int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name, char *fn, int ln);
int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln);
int ___pthread_mutex_try_lock(pthread_mutex_t *mutex);
int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln);

void dlock_dump();
void dlock_gen_dot();
#endif /* DLOCK */

/** some encapsulation macros */
/** unlocks a mutex */
#define MUTEX_UNLOCK(a) ___pthread_mutex_unlock(a,#a, __FILE__, __LINE__)

/** locks a mutex */
#define MUTEX_LOCK(a) ___pthread_mutex_lock(a,#a, __FILE__, __LINE__)

/** locks a mutex */
#define MUTEX_TRY_LOCK(a) ___pthread_mutex_try_lock(a)

/** inits a mutex */
#define MUTEX_INIT(a, v) ___pthread_mutex_init(a,v,#a, __FILE__, __LINE__)

/** destroys a mutex */
#define MUTEX_DESTROY(a) ___pthread_mutex_destroy(a)

