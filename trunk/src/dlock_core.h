/*
    dlock_core.h - dlock core structures
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

#ifndef _DLOCK_CORE_H_
#define _DLOCK_CORE_H_

/* Internal dump funcions for dlock hacking only */
#undef LOCK_DEBUG
#ifdef LOCK_DEBUG
#define DLOCKprintf(fmt, args...) printf(fmt, args)
#else
#define DLOCKprintf(fmt, args...)
#endif

/* this is absolutely needed because pthread_spindlock_lock_t are really
 * volatile ints */
typedef volatile void dlock_lock_t;

enum lock_type {
	MUTEX,
	SPINLOCK,
	UNKNOWN,
};

enum pthread_state {
	LOCKED,
	UNLOCKED,
};

enum {
	DLOCK_INIT_CALIBRATE = (1 << 0),
	DLOCK_INIT_HANDLE_USR1 = (1 << 1),
	DLOCK_INIT_HANDLE_USR2 = (1 << 2),
	DLOCK_REGISTER_ATEXIT = (1 << 3),
	DLOCK_LOG_FILE = (1 << 4),
};

#undef ASSERT
#define ASSERT(s, e, d) {						\
	if (!(e)) {							\
		fprintf(stderr, "DLOCK ASSERT: %s %s %s:%d\n%s\n",	\
			#e, s, __FILE__, __LINE__,			\
			(d)->last_used_bt);				\
		bt();							\
		fflush(stderr);						\
		abort();						\
	}								\
}

#define HIJACK(a, x, y) if (!(o_##x = dlsym(a , y))) {\
			   fprintf(dlock_log_file, "symbol %s() not found, exiting\n", #y);\
                	   exit(-1);\
                        }

void dlock_lock_init(dlock_lock_t *lock, void *v, const char *lock_name, char *fn,
		     int ln, enum lock_type type);
void dlock_lock(dlock_lock_t *lock, unsigned int tid);
void dlock_unlock(dlock_lock_t *lock, unsigned int tid);
void dlock_dump();
void dlock_gen_dot();

#endif /* _DLOCK_CORE_H_ */
