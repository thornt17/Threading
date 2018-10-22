#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define MAX_BYTES 32767
#define MAX_THREADS 128
#define READY 0
#define RUNNING 1
#define EXITED 2
#define JB_BX 0
#define JB_SI 1
#define JB_DI 2
#define JB_BP 3
#define JB_SP 4
#define JB_PC 5

// disable control operations while in critical section
int interrupts_disabled=0;

int threadcount = 0; //for pthread_create, number of threads
int isfirst = 1; //"boolean" for checking if first time running pthread_create
int count = 0; //for indexing threads

struct Thread
{
  pthread_t id;
  int stackpointer;
  int PCpointer;
  unsigned long int * stack;
  int basic_state;
  jmp_buf jump_state;
};

struct Thread mythreads[MAX_THREADS];

pthread_t pthread_self()
{
  return mythreads[count].id;
}

static int ptr_mangle(int p)
{
  unsigned int ret;
  asm(" movl %1, %%eax;\n"
      " xorl %%gs:0x18, %%eax;" " roll $0x9, %%eax;"
      " movl %%eax, %0;"
      : "=r"(ret)
      : "r"(p)
      : "%eax"
      );
  return ret;
}

void schedule()
{
  int prev = interrupts_disabled;
  interrupts_disabled = 1;
  if (prev) return; 

  if(setjmp(mythreads[count].jump_state) == 0)
    {
      int i;
      for(i = 0; i < MAX_THREADS; i++)
	{
	  count++;
	  if(count == MAX_THREADS)
	    {
	      count = 0;
	    }
	  if(mythreads[count].basic_state == READY)
	    {
	      mythreads[count].jump_state[0].__jmpbuf[JB_SP] = mythreads[count].stackpointer;
	      mythreads[count].jump_state[0].__jmpbuf[JB_PC] = mythreads[count].PCpointer;
	      mythreads[count].basic_state = RUNNING;
	      //printf("%d",count);
	      break;
	    }
	}
      /*sigset_t sigublock;
	sigemptyset(&sigublock);
	sigaddset(&sigublock, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigublock, NULL);*/

      interrupts_disabled = 0;
      longjmp(mythreads[count].jump_state, 1);
    }
  else
    {
      /*sigset_t sigublock;
	sigemptyset(&sigublock);
	sigaddset(&sigublock, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigublock, NULL);*/
    }
  interrupts_disabled = 0;

}

void start_timer()
{
  /*struct sigaction mysignal;
    memset (&mysignal, 0, sizeof(mysignal));
    mysignal.sa_sigaction = schedule;
    mysignal.sa_flags = SA_NODEFER;
    sigaction(SIGALRM, &mysignal, NULL);

    struct itimerval mytimer;
    mytimer.it_value.tv_usec = 50;
    mytimer.it_value.tv_sec = 0;
    mytimer.it_interval.tv_usec = 50000;
    mytimer.it_interval.tv_sec = 0;

    setitimer(ITIMER_REAL, &mytimer, NULL);*/

  struct sigaction timer;
  timer.sa_handler = schedule;

  sigemptyset(&timer.sa_mask);
  timer.sa_flags = SA_NODEFER;

  sigaction(SIGALRM, &timer, NULL);
  ualarm(50000,50000);
  //return;
}


void pthread_exit(void *value_ptr)
{
  threadcount--;
  mythreads[count].basic_state = EXITED;
  schedule();
  __builtin_unreachable();
}

int pthread_create(pthread_t *thread, const pthread_attr_t  *attr, void *(*start_routine) (void *), void *arg) //add
{
  
  if(threadcount >= MAX_THREADS)
    {
      perror("ERROR");
      return -1;
    }
  else if(threadcount < MAX_THREADS)
    {
      *thread = (pthread_t)threadcount;
      mythreads[threadcount].id = *thread;
      
      mythreads[threadcount].basic_state = READY;
      setjmp(mythreads[threadcount].jump_state);

      mythreads[threadcount].stack = malloc(MAX_BYTES);

      mythreads[threadcount].stack[MAX_BYTES/4 - 1] = (unsigned long int)arg;
      mythreads[threadcount].stack[MAX_BYTES/4 - 2] = (unsigned long int)pthread_exit;

      mythreads[threadcount].stackpointer = ptr_mangle((int)(mythreads[threadcount].stack + MAX_BYTES/4 - 2));
      mythreads[threadcount].PCpointer = ptr_mangle((int) start_routine);

      if(threadcount == 0)
	{
	  //isfirst = 0;
	  //*thread = (pthread_t)threadcount;
	  //mythreads[0].id = 0;
	  //mythreads[0].stack = NULL;
	  mythreads[0].basic_state = RUNNING;
	  //setjmp(mythreads[0].jump_state);
	  threadcount++;
	  // OK	  start_timer(); 
	  longjmp(mythreads[0].jump_state, 1);
	}
      else
	{
	  threadcount++;
	}
      //schedule();
    }

  return 2;
}


