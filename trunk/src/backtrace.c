#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#define C_BACKTRACE_DEPTH 8
#define C_BACKTRACESTRING_SIZE 255

struct layout{
  struct layout * next;
  void * return_address;
};

void * __libc_stack_end;
/*int __backtrace (void **array, int size) __attribute__ ((no));*/
static int __backtrace (void **array, int size)
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


static void format_backtrace(char * backtrace_string,void ** backtrace,size_t size){
  int i;
  sprintf(backtrace_string,"%p",backtrace[0]);
  for (i = 1 ; i < size ; i++){
    char pointer[12];
    sprintf(pointer," %p",backtrace[i]);
    strcat(backtrace_string,pointer);
  }
}


static __attribute__((unused)) void cp_bt(char *str)
{

  void * backtrace[C_BACKTRACE_DEPTH];
  __backtrace(backtrace,C_BACKTRACE_DEPTH);
  format_backtrace(str,backtrace,C_BACKTRACE_DEPTH);
}

static __attribute__((unused)) void bt()
{
  void * backtrace[C_BACKTRACE_DEPTH];
  __backtrace(backtrace,C_BACKTRACE_DEPTH);
  char backtrace_string[C_BACKTRACESTRING_SIZE];
  format_backtrace(backtrace_string,backtrace,C_BACKTRACE_DEPTH);
  printf("Call trace: [[%s]]\n", backtrace_string);
}


