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
#define MAX_KNOWN_MUTEXES 256
/* Length of the recorded name of a mutex */
#define MAX_MUTEX_NAME 255
#define MAX_FN_NAME 255

static unsigned long machine_mhz = -1;
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


typedef enum {
	LOCKED,
	UNLOCKED,
} pthread_state_t;

struct lock_action {
	pthread_mutex_t *mutex;
	pthread_state_t action;
	unsigned long long mtime;
};

struct thread_list {
	unsigned int tid;
	int index;
	struct lock_action seq[MAX_LOCK_DEPTH];
	LIST_ENTRY(thread_list) entries;
};

struct sequence {
	struct lock_action seq[MAX_LOCK_DEPTH];
	unsigned long long max_held;
	unsigned long length;
	unsigned long cksum;
	unsigned long times;
};

struct mutex_desc {
	pthread_mutex_t *mutex;
	char mutex_name[MAX_MUTEX_NAME];
	char fn_name[MAX_FN_NAME];
	int ln;
};

/* Holds the known mutex_desc entries */
struct mutex_desc mutex_descs[MAX_KNOWN_MUTEXES];
/* current index in the mutex_descs array */
static unsigned int md_index = 0;
/* Big DLOCK lock. todo: find a more fine grained locking scheme*/
static pthread_mutex_t dlock_mutex = PTHREAD_MUTEX_INITIALIZER;
/* linked list containing the different threads running
   in the guest program */
static LIST_HEAD(llisthead, thread_list) lhead;
/* Current index in the global sequences array */
static atomic_t index_sequences = ATOMIC_INIT(0);
/* Holds the sequences known to the system */
static struct sequence sequences[MAX_LOCK_SEQ];

static struct thread_list *find_list(unsigned int tid) 
{
	struct thread_list *np;

	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next)
		if (np->tid == tid)
			break;
	return np;
}

static void create_queue(unsigned int tid)
{
	struct thread_list *t;

	t = find_list(tid);
	if (!t) {
		t = malloc(sizeof(struct thread_list));
		LIST_INSERT_HEAD(&lhead, t, entries);
	}
	o_pthread_mutex_lock(&dlock_mutex);
	t->index = 0;
	memset(t->seq, 0, sizeof(t->seq));
	t->tid = tid;
	o_pthread_mutex_unlock(&dlock_mutex);
}

static void append_lock(unsigned int tid, pthread_mutex_t *mutex) 
{
	struct thread_list *t;

	t = find_list(tid);
	o_pthread_mutex_lock(&dlock_mutex);
	t->seq[t->index].mutex = mutex;
	t->seq[t->index].mtime = arch_get_ticks();
	t->seq[t->index].action = LOCKED;
	t->index++;
	o_pthread_mutex_unlock(&dlock_mutex);
}

static int append_unlock(unsigned int tid, pthread_mutex_t *mutex) 
{
	struct thread_list *t;
	int ret;

	t = find_list(tid);
	o_pthread_mutex_lock(&dlock_mutex);
	t->seq[t->index].mutex = mutex;
	t->seq[t->index].mtime = arch_get_ticks();
	t->seq[t->index].action = UNLOCKED;
	t->index++;
	/* is loop closed? */
	ret = t->seq[0].mutex == mutex;
	o_pthread_mutex_unlock(&dlock_mutex);
	return ret;
}

void dlock_mutex_init(pthread_mutex_t * mutex, void *v,
		   const char *mutex_name, char *fn, int ln)
{
	int skip = 0;

	/* Skip the leading ampersand for aesthetic purposes */
	if (mutex_name[0] == '&') {
		skip = 1;
	}
	o_pthread_mutex_lock(&dlock_mutex);
	mutex_descs[md_index].mutex = mutex;
	strncpy(mutex_descs[md_index].mutex_name, mutex_name + skip,
		MAX_MUTEX_NAME);
	strncpy(mutex_descs[md_index].fn_name, fn, MAX_FN_NAME);
	mutex_descs[md_index].ln = ln;
	md_index++;
	o_pthread_mutex_unlock(&dlock_mutex);
}
void dlock_mutex_lock(pthread_mutex_t *mutex, unsigned int tid)
{
	if (!find_list(tid)) {
		create_queue(tid);
	}
	append_lock(tid, mutex);
}

#define BIG_PRIME (4294967291u)
/* Returns a checksum used to uniquely identify a mutex/action sequence
   this helps distinguishing between a A->B->B->A and a B->A->B->A
   sequence */
static unsigned long cksum(unsigned int tid)
{
	struct thread_list *t;
	int i;
	unsigned long cksm = 0;

	t = find_list(tid);
	for (i=0; i < t->index; i++) {
		cksm = (cksm << 4)^(cksm >> 28)^(((unsigned long)t->seq[i].mutex) 
			 + t->seq[i].action);
	}
	return cksm % BIG_PRIME;
}

static void register_sequence(unsigned int tid)
{
	struct thread_list *t;
	unsigned long long mtime;

	t = find_list(tid);
	o_pthread_mutex_lock(&dlock_mutex);
	memcpy(sequences[atomic_read(&index_sequences)].seq, t->seq, 
		sizeof(t->seq));
	sequences[atomic_read(&index_sequences)].cksum = cksum(tid);
	mtime = t->seq[t->index - 1].mtime - t->seq[0].mtime; 
	sequences[atomic_read(&index_sequences)].max_held = mtime;
	sequences[atomic_read(&index_sequences)].times = 1;
	sequences[atomic_read(&index_sequences)].length = t->index;
	atomic_inc(&index_sequences);
	o_pthread_mutex_unlock(&dlock_mutex);
	return;
}

/* Returns true if the sequence held by thread tid is
   already known to dlock */
static int is_registered_sequence(unsigned int tid) {
	int i;
	int cksm;
	struct thread_list *t;
	unsigned long long mtime, now;

	cksm = cksum(tid);
	for (i=0; i < atomic_read(&index_sequences); i++) {
		/* it _is_ registerd */
		if (sequences[i].cksum == cksm) {
			/* 
			 * Quick and dirty hack to get
			 * the max held time registered 
			 */
			now = arch_get_ticks();
			t = find_list(tid);
			mtime = now - t->seq[0].mtime;
			if (sequences[i].max_held < mtime)
				sequences[i].max_held = mtime;
			sequences[i].times++;
			return 1;
		}
	}
	return 0;
}

void dlock_mutex_unlock(pthread_mutex_t *mutex, unsigned int tid)
{
	if (!find_list(tid)) {
		assert(0);
	}
	/* if loop closed */
	if (append_unlock(tid, mutex)) {
		/* do we already have such a sequence ? */
		if(!is_registered_sequence(tid)) {
			register_sequence(tid);
			/* reset the thread's queue */
			create_queue(tid);
		} else {
			/* simply drop the sequence,
			   we already know it */
			create_queue(tid);
		}
	}
}

static char *get_mutex_name(pthread_mutex_t * m)
{
	int i;

	for (i = 0; mutex_descs[i].mutex != 0 && i < MAX_KNOWN_MUTEXES; i++) {
		if (m == mutex_descs[i].mutex) {
			return mutex_descs[i].mutex_name;
		}
	}
	return NULL;
}

void dlock_dump()
{
	int i, j;

	o_pthread_mutex_lock(&dlock_mutex);

	fprintf(dlock_log_file,
	     "********************************************************************************\n");
	fprintf(dlock_log_file, "Registered lock sequences:\n");
	for (i = 0; i < atomic_read(&index_sequences); i++) {
		fprintf(dlock_log_file, "\t");
		for (j = 0; j < sequences[i].length; j++) {
			char *name = get_mutex_name(sequences[i].seq[j].mutex);
			if (name) {
				fprintf(dlock_log_file, "[%s %s] ", name,
					(sequences[i].seq[j].action == LOCKED)
					? "locked" : "unlocked");
			} else {
				fprintf(dlock_log_file, "[%p %s] ",
					sequences[i].seq[j].mutex,
					(sequences[i].seq[j].action == LOCKED)
					? "locked" : "unlocked");
			}
		}
		fprintf(dlock_log_file,
			"\n\t\theld %llu ms, length: %lu, times %lu\n",
			sequences[i].max_held / (machine_mhz / 1000),
			sequences[i].length, sequences[i].times);
	}

	fprintf (dlock_log_file,
	     "********************************************************************************\n");
	fprintf(dlock_log_file, "Thread list:\n");
	struct thread_list *np;
	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next) {
		fprintf(dlock_log_file, "\t[%u]", np->tid);
		if (np->seq[0].mutex != 0) {
			fprintf(dlock_log_file, "\n");
			for (i = 0; np->seq[i].mutex != 0; i++) {
				fprintf(dlock_log_file, "\t\t%s %s ",
					get_mutex_name(np->seq[i].mutex),
					(np->seq[i].action == LOCKED)
					? "locked\n" : "unlocked\n");
			}
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
				get_mutex_name(mutex_descs[i].mutex),
				mutex_descs[i].fn_name, mutex_descs[i].ln);
		}
	}

	o_pthread_mutex_unlock(&dlock_mutex);
}

//TODO: does not work properly
void dlock_gen_dot()
{
	int i, j;
	fprintf(dlock_log_file, "digraph g {\n");
	o_pthread_mutex_lock(&dlock_mutex);
	for (i = 0; i < atomic_read(&index_sequences); i++) {
		int first = 1;
		for (j = 0; j < sequences[i].length; j++) {
			if (sequences[i].seq[j].action == LOCKED) {
				char *name =
					get_mutex_name(sequences[i].seq[j].mutex);
				if (first)
					first = 0;
				else
					fprintf(dlock_log_file, "-> ");
				fprintf(dlock_log_file, "\"%s\" ", name);	
			}
		}
		fprintf(dlock_log_file, ";\n");
	}
	o_pthread_mutex_unlock(&dlock_mutex);
	fprintf(dlock_log_file, "}\n");
}
