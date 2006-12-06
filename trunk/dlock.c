#include <sys/queue.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#undef ASSERT
#define ASSERT(s, e, d) {					\
	if (!(e)) {						\
								\
		printf("DLOCK ASSERT: %s %s %s:%d\n%s\n", 	\
			#e, s, __FILE__, __LINE__,		\
			(d)->last_used_bt);			\
		bt();						\
		int *i;						\
		i = (int *)0xdeadbeef;				\
		fflush(stdout);					\
		*i = 123;					\
	}							\
}


#define GET_CPU_TICS(x) do { \
				asm volatile("rdtsc;":"=A"(x):); \
			} while(0)

LIST_HEAD(listhead, entry) head;
pthread_mutex_t dlock_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef enum {
	LOCKED,
	UNLOCKED,
} pthread_state_t;

#define C_BACKTRACE_DEPTH 8
#define C_BACKTRACESTRING_SIZE 255

struct entry {
	pthread_mutex_t *mutex;
	pthread_state_t state;
	unsigned long long last_action;
	int current_owner;
  	char last_used_bt[C_BACKTRACESTRING_SIZE];
	LIST_ENTRY(entry) entries;
};

struct layout{
  struct layout * next;
  void * return_address;
};


void * __libc_stack_end;
/*int __backtrace (void **array, int size) __attribute__ ((no));*/
int __backtrace (void **array, int size)
{
  /* We assume that all the code is generated with frame pointers set. */
  register void *ebp __asm__ ("ebp") = ebp ;
  register void *esp __asm__ ("esp");

  struct layout *current;
  int cnt = 0;

  /* We skip the call to this function, it makes no sense to record it. */
  current = (struct layout *) ebp;

  while (cnt < size)
    {
      if ((void *) current < esp || (void *) current > __libc_stack_end)
	/* This means the address is out of range. Note that for the
	 * toplevel we see a frame pointer with value NULL which clearly is
	 * out of range. */
	break;

      array[cnt++] = current->return_address;

      current = current->next;
    }

  return cnt;
}


void format_backtrace(char * backtrace_string,void ** backtrace,size_t size){
  int i;
  sprintf(backtrace_string,"%p",backtrace[0]);
  for (i = 1 ; i < size ; i++){
    char pointer[12];
    sprintf(pointer," %p",backtrace[i]);
    strcat(backtrace_string,pointer);
  }
}


void cp_bt(char *str) 
{

  void * backtrace[C_BACKTRACE_DEPTH];
  __backtrace(backtrace,C_BACKTRACE_DEPTH);
  format_backtrace(str,backtrace,C_BACKTRACE_DEPTH);
}

void bt() 
{

  void * backtrace[C_BACKTRACE_DEPTH];
  __backtrace(backtrace,C_BACKTRACE_DEPTH);
  char backtrace_string[C_BACKTRACESTRING_SIZE];
  format_backtrace(backtrace_string,backtrace,C_BACKTRACE_DEPTH);
  printf("Call trace: [[%s]]\n", backtrace_string);
}



void latency_time_lock(pthread_mutex_t *mutex, int tid)
{
	static int first = 1;
	struct entry *np;
	pthread_mutex_lock(&dlock_mutex);
	if (first) {
		LIST_INIT(&head);
		first = 0;
	}
	for (np = head.lh_first; np != NULL; np = np->entries.le_next)
		if(np->mutex == mutex)
			break;

	/* insert new mutex */
	if (!np) {
		np = malloc(sizeof(struct entry));
		strcpy(np->last_used_bt, "nobody");
		LIST_INSERT_HEAD(&head, np, entries);
	} else {
	/* do checks */
		ASSERT("dlock", np->mutex == mutex, np);
		ASSERT("dlock", np->state == UNLOCKED, np);
	}
	np->mutex = mutex;
	np->current_owner = tid;
	np->state = LOCKED;
	cp_bt(np->last_used_bt);
	GET_CPU_TICS(np->last_action);
	pthread_mutex_unlock(&dlock_mutex);
}
#define MAX_TICKS 20000000
void latency_time_unlock(pthread_mutex_t *mutex, int tid)
{
	unsigned long long ticks;
	struct entry *np;
	pthread_mutex_lock(&dlock_mutex);
	for (np = head.lh_first; np != NULL; np = np->entries.le_next)
		if(np->mutex == mutex)
			break;
	if (!np) {
		printf("%p\n", mutex);
		ASSERT("dlock unlocking unknown mutex", 0, np);
	}
	ASSERT("dlock", np->mutex == mutex, np);
	ASSERT("dlock", np->current_owner == tid, np);
	ASSERT("dlock", np->state == LOCKED, np);
	GET_CPU_TICS(ticks);
	ASSERT("dlock", (ticks - np->last_action) < MAX_TICKS, np);
	np->state = UNLOCKED;
	cp_bt(np->last_used_bt);
	GET_CPU_TICS(np->last_action);
	pthread_mutex_unlock(&dlock_mutex);
} 

#define MAX_LOCK_DEPTH 20
#define MAX_LOCK_SEQ 1024


pthread_mutex_t ordering_mutex = PTHREAD_MUTEX_INITIALIZER;

int index_sequences = 0;
unsigned int hashes[MAX_LOCK_SEQ];

LIST_HEAD(llisthead, thread_list) lhead;
pthread_mutex_t dlock_seq_mutex = PTHREAD_MUTEX_INITIALIZER;

struct lock_action {
	pthread_mutex_t *mutex;
	pthread_state_t action;
	unsigned long long mtime;
};
struct thread_list {
	int tid;
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
struct sequence sequences[MAX_LOCK_SEQ];

struct thread_list *find_list(int tid) 
{
	struct thread_list *np;
	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next)
		if (np->tid == tid)
			break;
	return np;
}
void create_queue(int tid)
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

void append_lock(int tid, pthread_mutex_t *mutex) 
{
	struct thread_list *t;
	t = find_list(tid);
	pthread_mutex_lock(&ordering_mutex);
	t->seq[t->index].mutex = mutex;
	asm volatile ("rdtsc":"=A"(t->seq[t->index].mtime));
	t->seq[t->index].action = LOCKED;
	t->index++;
	pthread_mutex_unlock(&ordering_mutex);
}

int append_unlock(int tid, pthread_mutex_t *mutex) 
{
	struct thread_list *t;
	int ret;
	t = find_list(tid);
	pthread_mutex_lock(&ordering_mutex);
	t->seq[t->index].mutex = mutex;
	asm volatile ("rdtsc":"=A"(t->seq[t->index].mtime));
	t->seq[t->index].action = UNLOCKED;
	t->index++;
	/* loop is closed? */
	ret = t->seq[0].mutex == mutex;
	pthread_mutex_unlock(&ordering_mutex);
	return ret;
}

void ordering_lock(pthread_mutex_t *mutex, int tid)
{
	if (!find_list(tid)) {
		create_queue(tid);
	}
	append_lock(tid, mutex);
}

#define BIG_PRIME (4294967291u)
static unsigned long cksum(int tid)
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

void register_sequence(int tid)
{
	struct thread_list *t;
	unsigned long long mtime;
	t = find_list(tid);
	pthread_mutex_lock(&ordering_mutex);
	memcpy(sequences[index_sequences].seq, t->seq, 
		sizeof(t->seq));
	sequences[index_sequences].cksum = cksum(tid);
	mtime = t->seq[t->index - 1].mtime - t->seq[0].mtime; 
	sequences[index_sequences].max_held = mtime;
	sequences[index_sequences].times = 1;
	sequences[index_sequences].length = t->index;
	index_sequences++;
	pthread_mutex_unlock(&ordering_mutex);
	return;
}

int is_registered_sequence(int tid) {
	int i;
	int cksm;
	struct thread_list *t;
	unsigned long long mtime, now;
	

	cksm = cksum(tid);
	for (i=0; i < index_sequences; i++) {
		if (sequences[i].cksum == cksm) {
			/* 
			 * Quick and dirty hack to get
			 * the max held time registered 
			 */
			asm volatile ("rdtsc": "=A"(now));
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

void ordering_unlock(pthread_mutex_t *mutex, int tid)
{
	if (!find_list(tid)) {
		assert(0);
	}
	/* if loop closed */
	if (append_unlock(tid, mutex)) {
		/* do we already have such a sequence ? */
		if(!is_registered_sequence(tid)) {
			register_sequence(tid);
			create_queue(tid);
		} else {
			/* simply drop the sequence,
			   we already know it */
			create_queue(tid);
		}
	}
}

void ordering_dump()
{
	int i, j, k;
        pthread_mutex_t * known_mutexes[26];
	int index_km=0;
	pthread_mutex_lock(&ordering_mutex);
	for (i=0; i < index_sequences; i++) {
		for (j=0; j < sequences[i].length; j++) {
			char name = '0';
			for (k=0; k < index_km; k++) {
				if (known_mutexes[k] == sequences[i].seq[j].mutex) {
					name = (char)('A'+k);
					break;
				}
			}
			if (name == '0') {
				known_mutexes[index_km] = sequences[i].seq[j].mutex;
				name = (char)('A'+index_km);
				index_km++;
			}
			printf("[%c %s] ", name,
					(sequences[i].seq[j].action == LOCKED) 
					? "locked" : "unlocked");  
		}
		printf("\ncksum 0x%08lx\t\theld %llu ms, length: %lu, times %lu\n", sequences[i].cksum, 
		       sequences[i].max_held / 500000, 
		       sequences[i].length,
		       sequences[i].times);
	}
	printf("\n***********************************************\n\n");
	struct thread_list *np;
	for (np = lhead.lh_first; np != NULL; np = np->entries.le_next) {
		printf("[%d]\n", np->tid);
		for (i=0; np->seq[i].mutex != 0; i++) {
			printf("%p %s ", np->seq[i].mutex, (np->seq[i].action == LOCKED) 
			? "locked" : "unlocked");  
		}
	}
	printf("\n");
	pthread_mutex_unlock(&ordering_mutex);

	for (k=0; k < index_km; k++) {
		printf("%p %c\n", known_mutexes[k], 'A'+k);
	}
}
