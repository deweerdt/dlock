/*
    dlock.c - dlock core code
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


#include <sys/queue.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <asm/atomic.h>
#include <signal.h>
#include <dlfcn.h>

typedef void (*sighandler_t)(int);

#include "dlock.h"
#include "tree.h"

/* Internal dump funcions for dlock hacking only */
#undef LOCK_DEBUG
#ifdef LOCK_DEBUG
#define DLOCKprintf(fmt, args...) printf(fmt, args)
#else
#define DLOCKprintf(fmt, args...)
#endif

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

void dlock_mutex_init(pthread_mutex_t *mutex, void *v, const char *mutex_name, char *fn, int ln);
void dlock_mutex_lock(pthread_mutex_t *mutex, unsigned int tid);
void dlock_mutex_unlock(pthread_mutex_t *mutex, unsigned int tid);
void dlock_dump();
void dlock_gen_dot();


static void *libpthread_handle = NULL;
static FILE *dlock_log_file;

static int (*o_pthread_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
static int (*o_pthread_mutex_lock)(pthread_mutex_t *);
static int (*o_pthread_mutex_trylock)(pthread_mutex_t *);
static int (*o_pthread_mutex_unlock)(pthread_mutex_t *);

static struct mutex_desc *get_mutex_desc(pthread_mutex_t * m);

#if defined(__i386__) || defined(__x86_64__)
static __inline__ unsigned long long int arch_get_ticks(void)
{
	unsigned long long int x;
	asm volatile ("rdtsc" : "=A" (x));
	return x;
}
#else
#error "Unsupported arch, please report to frederik.deweerdt@gmail.com"
#endif /* arch_get_ticks */

/* Maximum number of mutex/action pairs recorded in a lock chain */
#define MAX_LOCK_DEPTH 20
/* Number of different lock sequences recorded */
#define MAX_LOCK_SEQ 1024
/* Number of different mutexes known to the system */
#define MAX_KNOWN_MUTEXES 1024
/* Length of the recorded name of a mutex */
#define MAX_MUTEX_NAME 255
#define MAX_FN_NAME 255

static unsigned long machine_mhz = ~0UL;
static unsigned long calibrating_loop()
{
	struct timeval a;
	unsigned long long start, end;
	unsigned long mhz;

	gettimeofday(&a, NULL);
	start = arch_get_ticks();
	for (;;) {
		unsigned long usec;
		struct timeval b;
		gettimeofday(&b, NULL);
		usec = (b.tv_sec - a.tv_sec) * 1000000;
		usec += b.tv_usec - a.tv_usec;
		if (usec >= 1000000)
			break;
	}
	end = arch_get_ticks();
	mhz = end - start;
	return mhz;
}

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
	dlock_mutex_init(mutex, v, mutex_name, fn, ln);
	return ret;
}

int ___pthread_mutex_lock(pthread_mutex_t *mutex, char *lname, char *fn, int ln)
{
	int ret;
	DLOCKprintf("locking %d %p %s_%s_%d\n", (int)pthread_self(), mutex, lname, fn, ln);
	ret = o_pthread_mutex_lock(mutex);
	dlock_mutex_lock(mutex, (int)pthread_self());
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
	dlock_mutex_unlock(mutex, (int)pthread_self());
	return o_pthread_mutex_unlock(mutex);
}

#define HIJACK(a, x, y) if (!(o_##x = dlsym(a , y))) {\
			   fprintf(dlock_log_file, "symbol %s() not found, exiting\n", #y);\
                	   exit(-1);\
                        }

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	char buf[16];
	sprintf(buf, "%p", mutex);
	dlock_mutex_init(mutex, (void *)attr, buf, "?", 0);
	return o_pthread_mutex_init(mutex, attr);
}
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	dlock_mutex_lock(mutex, pthread_self());
	return o_pthread_mutex_lock(mutex);
}
int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	return o_pthread_mutex_trylock(mutex);
}
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	dlock_mutex_unlock(mutex, pthread_self());
	return o_pthread_mutex_unlock(mutex);
}

void dlock_hijack_libpthread()
{
	if ( (libpthread_handle = dlopen("libpthread.so", RTLD_NOW)) == NULL)
		if ( (libpthread_handle = dlopen("libpthread.so.0", RTLD_NOW)) == NULL) {
			fprintf(dlock_log_file, "error loading libpthread!\n");
			exit(-1);
		}
	HIJACK(libpthread_handle, pthread_mutex_init, "pthread_mutex_init");
	HIJACK(libpthread_handle, pthread_mutex_lock, "pthread_mutex_lock");
	HIJACK(libpthread_handle, pthread_mutex_trylock, "pthread_mutex_trylock");
	HIJACK(libpthread_handle, pthread_mutex_unlock, "pthread_mutex_unlock");
}

enum pthread_state {
	LOCKED,
	UNLOCKED,
};

struct lock_action {
	pthread_mutex_t *mutex;
	enum pthread_state action;
	unsigned long long mtime;
};

struct thread_list {
	unsigned int tid;
	struct dlock_node *root;
	struct dlock_node *current_node;
	LIST_ENTRY(thread_list) entries;
};

struct sequence {
	unsigned long long max_held;
	unsigned long length;
	unsigned long cksum;
	unsigned long times;
	struct dlock_node *root;
};

struct mutex_desc {
	pthread_mutex_t *mutex;
	char name[MAX_MUTEX_NAME];
	char fn_name[MAX_FN_NAME];
	int ln;
	pthread_mutex_t *depends_on[MAX_KNOWN_MUTEXES];
};

/* Holds the known mutex_desc entries */
static struct mutex_desc mutex_descs[MAX_KNOWN_MUTEXES];
/* current index in the mutex_descs array */
static unsigned int md_index = 0;
/* Big DLOCK lock. todo: find a more fine grained locking scheme*/
static pthread_mutex_t descs_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;
/* linked list containing the different threads running
   in the guest program */
static LIST_HEAD(llisthead, thread_list) lhead;

/* Holds the sequences known to the system */
static struct sequence tsequences[MAX_LOCK_SEQ];
/* Current index in the global sequences array */
static int index_tsequences = 0;
/* protectes tsequences and index_tsequences */
static pthread_mutex_t sequences_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Called by ld */
void _init()
{
	int flags;
	const char *str_flags = getenv("DLOCK_FLAGS");
	const char *preload = getenv("LD_PRELOAD");

	if (!str_flags)
		flags = 0;
	else
		flags = strtol(str_flags, NULL, 10);

	if (preload && !str_flags) {
		flags = DLOCK_INIT_HANDLE_USR1;
	}

	if (flags & DLOCK_INIT_CALIBRATE)
		machine_mhz = calibrating_loop();

	if (flags & DLOCK_INIT_HANDLE_USR1)
		signal(SIGUSR1, (sighandler_t) dlock_dump);

	if (flags & DLOCK_REGISTER_ATEXIT)
		atexit(dlock_dump);

	dlock_log_file = stderr;
	if (flags & DLOCK_LOG_FILE) {
		char *log_file = getenv("DLOCK_LOG_FILE");
		FILE *f;

		f = fopen(log_file, "w+");
		if (!f)
			fprintf(dlock_log_file,
				"dlock: Cannot open %s file, falling back to stderr (remember to set DLOCK_LOG_FILE)\n",
				log_file ? log_file : "<null>");
		else
			dlock_log_file = f;
	}

	dlock_hijack_libpthread();
}

static struct thread_list *find_list(unsigned int tid) 
{
	struct thread_list *np;

	o_pthread_mutex_lock(&thread_list_mutex);
	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next)
		if (np->tid == tid)
			break;
	o_pthread_mutex_unlock(&thread_list_mutex);
	return np;
}

static struct thread_list *create_queue(unsigned int tid)
{
	struct thread_list *t;

	t = find_list(tid);
	if (!t) {
		t = malloc(sizeof(struct thread_list));
		o_pthread_mutex_lock(&thread_list_mutex);
		LIST_INSERT_HEAD(&lhead, t, entries);
		o_pthread_mutex_unlock(&thread_list_mutex);
	}
	t->tid = tid;
	t->root = calloc(1, sizeof(struct dlock_node));
	t->current_node = t->root;
	return t;
}

/**
 * @brief Returns the mutex, the current mutex depends on
 *
 * @param t  
 *
 * @return the parent mutex or NULL if no parent mutex was found
 **/
static pthread_mutex_t *get_previous_lock(struct thread_list *t)
{
	return t->current_node->parent->mutex;
}

/**
 * @brief Add a dependecy relation between @master_mutex and the 
 * mutex described by @desc
 *
 * @param desc The desc_mutex we want to mark as depending on @master_mutex 
 * @param master_mutex The mutex on which the mutex described by @desc depends 
 *
 * @return 
 **/
static void new_dependency(struct mutex_desc *desc, pthread_mutex_t *master_mutex)
{
	int i;
	
	/* skip to an empty stop in depends_on[] */
	for (i=0; desc->depends_on[i] != master_mutex 
		  && desc->depends_on[i] != NULL 
		  && i < MAX_KNOWN_MUTEXES; i++);

	/* sanity check */
	assert(i != MAX_KNOWN_MUTEXES && "Too much mutexes");

	/* known dependency, don't add */
	if (desc->depends_on[i] == master_mutex)
		return;

	/* assign new dependency */
	desc->depends_on[i] = master_mutex;	
}

/**
 * @brief Tests whenever @mutex depends on @probed
 *
 * @param mutex  
 * @param probed  
 *
 * @return 1 if @mutex depends on probed, 0 otherwise
 **/
static int is_depending_on(pthread_mutex_t *mutex, pthread_mutex_t *probed)
{
	int i;
	struct mutex_desc *desc;

	if (mutex == probed) {
		return 1;
	}

	desc = get_mutex_desc(mutex);

	for (i=0; desc->depends_on[i] != NULL && i < MAX_KNOWN_MUTEXES; i++) {
		if (desc->depends_on[i] == probed) {
			return 1;
		}
		if (is_depending_on(desc->depends_on[i], probed)) {
			return 1;
		}
	}
	return 0;
}

#define ALLOC_STEP 4
static void append_lock(struct thread_list *t, pthread_mutex_t *mutex) 
{
	pthread_mutex_t *previous;

	t->current_node->nb_children++;
	#if 1
	t->current_node->children = realloc(t->current_node->children, 
		(t->current_node->nb_children) * sizeof(struct dlock_node));
	#else /* enable this if you have many childs for one lock */
	if ((t->current_node->nb_children % ALLOC_STEP) == 1) {
		t->current_node->children = realloc(t->current_node->children, 
			(t->current_node->nb_children + ALLOC_STEP) 
			* sizeof(struct dlock_node));
	}
	#endif
	CUR_CHILD(t) = calloc(1, sizeof(struct dlock_node));
	CUR_CHILD(t)->parent =  t->current_node;
	CUR_CHILD(t)->mutex =  mutex;
	CUR_CHILD(t)->lock_time = arch_get_ticks();

	t->current_node = CUR_CHILD(t);

	previous = get_previous_lock(t);

	if (previous) {
		/* let us know if the previous lock was already
		 * depending on mutex (deadlock?)*/
		if (is_depending_on(previous, mutex)) {
			dlock_dump();
			fprintf(dlock_log_file, "New dependency %p -> %p leads to deadlock\n", previous, mutex);
			exit(0);
		}
		new_dependency(get_mutex_desc(mutex), previous);
	}
}

static int append_unlock(struct thread_list *t, pthread_mutex_t *mutex) 
{
	t->current_node->unlock_time = arch_get_ticks();
	t->current_node = t->current_node->parent;	

	/* is loop closed? */
	return (t->root->nb_children 
		&& FIRST_NODE(t)->mutex == mutex);
}

void dlock_mutex_init(pthread_mutex_t * mutex, void *v,
		   const char *mutex_name, char *fn, int ln)
{
	int skip = 0;

	/* Skip the leading ampersand for aesthetic purposes */
	if (mutex_name[0] == '&') {
		skip = 1;
	}
	o_pthread_mutex_lock(&descs_mutex);
	mutex_descs[md_index].mutex = mutex;
	strncpy(mutex_descs[md_index].name, mutex_name + skip,
		MAX_MUTEX_NAME);
	strncpy(mutex_descs[md_index].fn_name, fn, MAX_FN_NAME);
	mutex_descs[md_index].ln = ln;

	md_index++;
	o_pthread_mutex_unlock(&descs_mutex);
}

void dlock_mutex_lock(pthread_mutex_t *mutex, unsigned int tid)
{
	struct thread_list *t;

	t = find_list(tid);
	if (!t) {
		t = create_queue(tid);
	}
	append_lock(t, mutex);
}

static void register_sequence(struct thread_list *t)
{
	unsigned long long mtime;

	o_pthread_mutex_lock(&sequences_mutex);

	tsequences[index_tsequences].cksum = cksum(t->root, 0);
	mtime = FIRST_NODE(t)->unlock_time - FIRST_NODE(t)->lock_time;
	tsequences[index_tsequences].max_held = mtime;
	tsequences[index_tsequences].times = 1;
	tsequences[index_tsequences].root = t->root;
	tsequences[index_tsequences].length = get_tree_depth(t->root, 0);
	index_tsequences++;

	o_pthread_mutex_unlock(&sequences_mutex);
	return;
}

/* Returns true if the sequence held by thread tid is
   already known to dlock */
static int is_registered_sequence(struct thread_list *t) {
	int i, cksm, ret = 0;
	unsigned long long mtime, now;

	cksm = cksum(t->root, 0);
	o_pthread_mutex_lock(&sequences_mutex);
	for (i=0; i < index_tsequences; i++) {
		/* it _is_ registered */
		if (tsequences[i].cksum == cksm) {
			/* 
			 * the max held time registered 
			 * may have changed, update it
			 */
			now = arch_get_ticks();
			mtime = now - FIRST_NODE(t)->lock_time;
			if (tsequences[i].max_held < mtime)
				tsequences[i].max_held = mtime;
			tsequences[i].times++;
			ret = 1;
			goto out;
		}
	}
out:
	o_pthread_mutex_unlock(&sequences_mutex);
	return ret;
}

void dlock_mutex_unlock(pthread_mutex_t *mutex, unsigned int tid)
{
	struct thread_list *t;

	t = find_list(tid);
	/* if not found: we started unlocking without a previous lock */
	if (!t) {
		assert(0);
	}
	/* if loop closed */
	if (append_unlock(t, mutex)) {
		/* do we already have such a sequence ? */
		if(!is_registered_sequence(t)) {
			register_sequence(t);
			/* reset the thread's queue */
			create_queue(tid);
		} else {
			/* simply drop the sequence,
			   we already know it */
			free_nodes(t->root);
			create_queue(tid);
		}
	}
}

static struct mutex_desc *get_mutex_desc(pthread_mutex_t * m)
{
	int i;

	o_pthread_mutex_lock(&descs_mutex);
	for (i = 0; mutex_descs[i].mutex != 0 && i < MAX_KNOWN_MUTEXES; i++) {
		if (m == mutex_descs[i].mutex) {
			o_pthread_mutex_unlock(&descs_mutex);
			return &mutex_descs[i];
		}
	}
	o_pthread_mutex_unlock(&descs_mutex);
	assert(0);
	return NULL;
}

void print_lock_tree(struct dlock_node *n, int depth)
{
	int i;
	for(i=0; i < depth - 1; i++) {
		if (i == depth - 2) {
			fprintf(dlock_log_file, "\\----->\t");
		} else if (i == 0) {
			fprintf(dlock_log_file, "|\t");
		} else {
			fprintf(dlock_log_file, "\t");
		}
	}
	/* special case, skip root */
	if (depth) {
		char *name = get_mutex_desc(n->mutex)->name;
		char *locked = "";

		/* display that the lock is held */
		if (n->unlock_time == 0)
			locked = " (locked)";

		fprintf(dlock_log_file, "%s%s\n", name, locked);
	}
	for (i = 0; i < n->nb_children; i++) {
		print_lock_tree(n->children[i], depth+1);
	}
}

void dlock_dump()
{
	int i, j;

	fprintf(dlock_log_file,
	     "********************************************************************************\n");
	fprintf(dlock_log_file, "Registered lock sequences:\n");
	for (i = 0; i < index_tsequences; i++) {
		fprintf(dlock_log_file,
		"held %llu ms, length: %lu, times %lu\n",
		machine_mhz == ~0UL ? 0 : tsequences[i].max_held / (machine_mhz / 1000),
		tsequences[i].length, tsequences[i].times);

		print_lock_tree(tsequences[i].root, 0);
	}

	fprintf (dlock_log_file,
	     "********************************************************************************\n");
	fprintf(dlock_log_file, "Thread list:\n");
	struct thread_list *np;
	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next) {
		fprintf(dlock_log_file, "[%u]", np->tid);
		if (np->root->nb_children != 0) {
			fprintf(dlock_log_file, "\n");
			print_lock_tree(np->root, 0);
		} else {
			fprintf(dlock_log_file, " no mutexes held\n");
		}
	}

	/* The mutexes need to be inited with pthread_mutex_init to be printed here */
	if (md_index > 0) {

		fprintf (dlock_log_file,
		     "********************************************************************************\n");

		fprintf(dlock_log_file, "Mutexes known to dlock:\n");
		for (i = 0; i < md_index; i++) {
			fprintf(dlock_log_file, "\t%p %s=%s:%d\n",
				mutex_descs[i].mutex,
				get_mutex_desc(mutex_descs[i].mutex)->name,
				mutex_descs[i].fn_name, mutex_descs[i].ln);
			fprintf(dlock_log_file, "\t\tdepends on:\n");
			for (j=0; mutex_descs[i].depends_on[j] != NULL; j++) {
				fprintf(dlock_log_file, "\t\t%p\n", mutex_descs[i].depends_on[j]);
			}
		}
	}

}

//TODO: does not work properly
void dlock_gen_dot()
{
#if 0
	int i, j;
	fprintf(dlock_log_file, "digraph g {\n");
	for (i = 0; i < index_tsequences; i++) {
		int first = 1;
		for (j = 0; j < tsequences[i].length; j++) {
			if (tsequences[i].seq[j].action == LOCKED) {
				char *name =
					get_mutex_desc(sequences[i].seq[j].mutex)->name;
				if (first)
					first = 0;
				else
					fprintf(dlock_log_file, "-> ");
				fprintf(dlock_log_file, "\"%s\" ", name);	
			}
		}
		fprintf(dlock_log_file, ";\n");
	}
	fprintf(dlock_log_file, "}\n");
#endif
}
