#include <sys/queue.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <asm/atomic.h>
#include <signal.h>
typedef void (*sighandler_t)(int);

#include "dlock.h"

#ifdef DLOCK_ORDERING

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

void dlock_init(int flags)
{
	if (DLOCK_INIT_CALIBRATE)
		machine_mhz = calibrating_loop();
	if (DLOCK_INIT_HANDLE_USR1)
		signal(SIGUSR1, (sighandler_t)ordering_dump);
	if (DLOCK_INIT_HANDLE_USR2)
		signal(SIGUSR2, (sighandler_t)ordering_dump);
				
}

#undef ASSERT
#define ASSERT(s, e, d) {					\
	if (!(e)) {						\
		printf("DLOCK ASSERT: %s %s %s:%d\n%s\n", 	\
			#e, s, __FILE__, __LINE__,		\
			(d)->last_used_bt);			\
		bt();						\
		fflush(stdout);					\
		abort();					\
	}							\
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
static pthread_mutex_t ordering_mutex = PTHREAD_MUTEX_INITIALIZER;
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
	pthread_mutex_lock(&ordering_mutex);
	t->index = 0;
	memset(t->seq, 0, sizeof(t->seq));
	t->tid = tid;
	pthread_mutex_unlock(&ordering_mutex);
}

static void append_lock(unsigned int tid, pthread_mutex_t *mutex) 
{
	struct thread_list *t;

	t = find_list(tid);
	pthread_mutex_lock(&ordering_mutex);
	t->seq[t->index].mutex = mutex;
	t->seq[t->index].mtime = arch_get_ticks();
	t->seq[t->index].action = LOCKED;
	t->index++;
	pthread_mutex_unlock(&ordering_mutex);
}

static int append_unlock(unsigned int tid, pthread_mutex_t *mutex) 
{
	struct thread_list *t;
	int ret;

	t = find_list(tid);
	pthread_mutex_lock(&ordering_mutex);
	t->seq[t->index].mutex = mutex;
	t->seq[t->index].mtime = arch_get_ticks();
	t->seq[t->index].action = UNLOCKED;
	t->index++;
	/* loop is closed? */
	ret = t->seq[0].mutex == mutex;
	pthread_mutex_unlock(&ordering_mutex);
	return ret;
}

void ordering_init(pthread_mutex_t * mutex, void *v,
		   const char *mutex_name, char *fn, int ln)
{
	int skip = 0;

	/* Skip the leading ampersand for aesthetic purposes */
	if (mutex_name[0] == '&') {
		skip = 1;
	}
	pthread_mutex_lock(&ordering_mutex);
	mutex_descs[md_index].mutex = mutex;
	strncpy(mutex_descs[md_index].mutex_name, mutex_name + skip,
		MAX_MUTEX_NAME);
	strncpy(mutex_descs[md_index].fn_name, fn, MAX_FN_NAME);
	mutex_descs[md_index].ln = ln;
	md_index++;
	pthread_mutex_unlock(&ordering_mutex);
}
void ordering_lock(pthread_mutex_t *mutex, unsigned int tid)
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
	pthread_mutex_lock(&ordering_mutex);
	memcpy(sequences[atomic_read(&index_sequences)].seq, t->seq, 
		sizeof(t->seq));
	sequences[atomic_read(&index_sequences)].cksum = cksum(tid);
	mtime = t->seq[t->index - 1].mtime - t->seq[0].mtime; 
	sequences[atomic_read(&index_sequences)].max_held = mtime;
	sequences[atomic_read(&index_sequences)].times = 1;
	sequences[atomic_read(&index_sequences)].length = t->index;
	atomic_inc(&index_sequences);
	pthread_mutex_unlock(&ordering_mutex);
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

void ordering_unlock(pthread_mutex_t *mutex, unsigned int tid)
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

void ordering_dump()
{
	int i, j;

	pthread_mutex_lock(&ordering_mutex);
	printf
	    ("********************************************************************************\n");
	printf("Registered lock sequences:\n");
	for (i = 0; i < atomic_read(&index_sequences); i++) {
		printf("\t");
		for (j = 0; j < sequences[i].length; j++) {
			char *name =
			    get_mutex_name(sequences[i].seq[j].mutex);
			printf("[%s %s] ", name,
			       (sequences[i].seq[j].action == LOCKED)
			       ? "locked" : "unlocked");
		}
		printf("\n\t\theld %llu ms, length: %lu, times %lu\n",
		       sequences[i].max_held / (machine_mhz / 1000),
		       sequences[i].length, sequences[i].times);
	}
	printf
	    ("********************************************************************************\n");
	printf("Thread list:\n");
	struct thread_list *np;
	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next) {
		printf("\t[%u]", np->tid);
		if (np->seq[0].mutex != 0) {
			printf("\n");
			for (i = 0; np->seq[i].mutex != 0; i++) {
				printf("\t\t%s %s ",
				       get_mutex_name(np->seq[i].mutex),
				       (np->seq[i].action == LOCKED)
				       ? "locked\n" : "unlocked\n");
			}
		} else {
			printf(" no mutexes held\n");
		}
	}
	printf
	    ("********************************************************************************\n");

	printf("Mutexes known to dlock:\n");
	for (i = 0; i < md_index; i++) {
		printf("\t%p %s=%s:%d\n", mutex_descs[i].mutex,
		       get_mutex_name(mutex_descs[i].mutex),
		       mutex_descs[i].fn_name, mutex_descs[i].ln);
	}

	pthread_mutex_unlock(&ordering_mutex);
}

//TODO: does not work properly
void ordering_gen_dot()
{
	int i, j;
	printf("digraph g {\n");
	pthread_mutex_lock(&ordering_mutex);
	for (i = 0; i < atomic_read(&index_sequences); i++) {
		int first = 1;
		for (j = 0; j < sequences[i].length; j++) {
			if (sequences[i].seq[j].action == LOCKED) {
				char *name =
					get_mutex_name(sequences[i].seq[j].mutex);
				if (first)
					first = 0;
				else
					printf("-> ");
				printf("\"%s\" ", name);	
			}
		}
		printf(";\n");
	}
	pthread_mutex_unlock(&ordering_mutex);
	printf("}\n");
}
#endif /* DLOCK_ORDERING */
