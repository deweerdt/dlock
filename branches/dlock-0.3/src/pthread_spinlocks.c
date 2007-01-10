/*
    pthread_spinlocks.c - hooks for the pthread_spin_* API
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
int (*o_pthread_spin_init)(pthread_spinlock_t *spin, int shared);
int (*o_pthread_spin_lock)(pthread_spinlock_t *);
int (*o_pthread_spin_trylock)(pthread_spinlock_t *);
int (*o_pthread_spin_unlock)(pthread_spinlock_t *);

int ___pthread_spin_destroy(pthread_spinlock_t *spin)
{
	DLOCKprintf("destroying %d %p\n", (int)pthread_self(), spin);
	return pthread_spin_destroy(spin);
}

int ___pthread_spin_init(pthread_spinlock_t *spin, int i, char *spin_name, char *fn, int ln)
{
	int ret;
	DLOCKprintf("initing %d %p %s\n", (int)pthread_self(), spin, spin_name);
	ret = o_pthread_spin_init(spin, i);	
	dlock_lock_init(spin, (void *)i, spin_name, fn, ln, SPINLOCK);
	return ret;
}

int ___pthread_spin_lock(pthread_spinlock_t *spin, char *lname, char *fn, int ln)
{
	int ret;
	DLOCKprintf("locking %d %p %s_%s_%d\n", (int)pthread_self(), spin, lname, fn, ln);
	ret = o_pthread_spin_lock(spin);
	dlock_lock(spin, (int)pthread_self());
	return ret;
}

int ___pthread_spin_trylock(pthread_spinlock_t *spin)
{
	DLOCKprintf("try locking %d %p\n", (int)pthread_self(), spin);
	return o_pthread_spin_trylock(spin);
}

int ___pthread_spin_unlock(pthread_spinlock_t *spin, char *lname, char *fn, int ln)
{
	DLOCKprintf("unlocking %d %p %s_%s_%d\n", (int)pthread_self(), spin, lname, fn, ln);
	dlock_unlock(spin, (int)pthread_self());
	return o_pthread_spin_unlock(spin);
}

int pthread_spin_init(pthread_spinlock_t *spin, int i)
{
	char buf[16];
	sprintf(buf, "%p", spin);
	dlock_lock_init(spin, (void *)i, buf, "?", 0, SPINLOCK);
	return o_pthread_spin_init(spin, i);
}
int pthread_spin_lock(pthread_spinlock_t *spin)
{
	dlock_lock(spin, pthread_self());
	return o_pthread_spin_lock(spin);
}
int pthread_spin_trylock(pthread_spinlock_t *spin)
{
	return o_pthread_spin_trylock(spin);
}

int pthread_spin_unlock(pthread_spinlock_t *spin)
{
	dlock_unlock(spin, pthread_self());
	return o_pthread_spin_unlock(spin);
}
