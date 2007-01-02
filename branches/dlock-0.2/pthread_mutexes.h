/*
    pthread_mutexes.h - original pthread_mutex_* calls storage
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

#ifndef _PTHREAD_MUTEXES_H_
#define _PTHREAD_MUTEXES_H_

/* placeholders for the _true_ libpthread functions */
extern int (*o_pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
extern int (*o_pthread_mutex_lock)(pthread_mutex_t *);
extern int (*o_pthread_mutex_trylock)(pthread_mutex_t *);
extern int (*o_pthread_mutex_unlock)(pthread_mutex_t *);

#endif /* _PTHREAD_MUTEXES_H_ */
