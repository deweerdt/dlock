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

#ifndef _DLOCK_H_
#define _DLOCK_H_

#include <pthread.h>
#include <assert.h>
#include <stdio.h>


#ifndef DLOCK
static void __attribute__((unused)) dlock_dump() {}
static void __attribute__((unused)) dlock_gen_dot() {}

static int __attribute__((unused)) ___pthread_spin_destroy(pthread_spinlock_t *spin)
{
	return pthread_spin_destroy(spin);
}

static int __attribute__((unused)) ___pthread_spin_init(pthread_spinlock_t *spin, int i, char *spin_name, char *fn, int ln)
{
	return pthread_spin_init(spin, i);
}

static int __attribute__((unused)) ___pthread_spin_lock(pthread_spinlock_t *spin, char *lname, char *fn, int ln)
{
	return pthread_spin_lock(spin);
}

static int __attribute__((unused)) ___pthread_spin_trylock(pthread_spinlock_t *spin)
{
	return pthread_spin_trylock(spin);
}

static int __attribute__((unused)) ___pthread_spin_unlock(pthread_spinlock_t *spin, char *lname, char *fn, int ln)
{
	return pthread_spin_unlock(spin);
}

static int __attribute__((unused)) ___pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	return pthread_mutex_destroy(mutex);
}

static int __attribute__((unused)) ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *mutex_name, char *fn, int ln)
{
	return pthread_mutex_init(mutex, v);
}

static int __attribute__((unused)) ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	return pthread_mutex_lock(mutex);
}

static int __attribute__((unused)) ___pthread_mutex_try_lock(pthread_mutex_t *mutex)
{
	return pthread_mutex_trylock(mutex);
}

static int __attribute__((unused)) ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	return pthread_mutex_unlock(mutex);
}

#else /* DLOCK*/

/* pthread mutexes */
int ___pthread_mutex_destroy(pthread_mutex_t *mutex);
int ___pthread_mutex_init(pthread_mutex_t *mutex, void *v, char *lname, char *fn, int ln);
int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln);
int ___pthread_mutex_try_lock(pthread_mutex_t *mutex);
int ___pthread_mutex_unlock(pthread_mutex_t *mutex, char *lname, char *fn, int ln);

/* pthread spinlocks */
int ___pthread_spin_destroy(pthread_spinlock_t *spin);
int ___pthread_spin_init(pthread_spinlock_t *spin, int i, char *lname, char *fn, int ln);
int ___pthread_spin_lock(pthread_spinlock_t *spin, char *lname, char *fn, int ln);
int ___pthread_spin_try_lock(pthread_spinlock_t *spin);
int ___pthread_spin_unlock(pthread_spinlock_t *spin, char *lname, char *fn, int ln);

void dlock_dump();
void dlock_gen_dot();
#endif /* DLOCK */

/* dlock instrumentation API */
#define MUTEX_UNLOCK(a)		___pthread_mutex_unlock(a,#a, __FILE__, __LINE__)
#define MUTEX_LOCK(a)		___pthread_mutex_lock(a,#a, __FILE__, __LINE__)
#define MUTEX_TRY_LOCK(a)	___pthread_mutex_try_lock(a)
#define MUTEX_INIT(a, v)	___pthread_mutex_init(a,v,#a, __FILE__, __LINE__)
#define MUTEX_DESTROY(a)	___pthread_mutex_destroy(a)

#define SPIN_UNLOCK(a)		___pthread_spin_unlock(a,#a, __FILE__, __LINE__)
#define SPIN_LOCK(a)		___pthread_spin_lock(a,#a, __FILE__, __LINE__)
#define SPIN_TRY_LOCK(a)	___pthread_spin_trylock(a)
#define SPIN_INIT(a, v)		___pthread_spin_init(a,v,#a, __FILE__, __LINE__)
#define SPIN_DESTROY(a)		___pthread_spin_destroy(a)

#endif /* _DLOCK_H_ */
