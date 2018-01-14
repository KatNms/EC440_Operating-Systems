/*
Katrina Nemes
Project 2 OS
*/


//libraries included
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>


/***********************************
For reference, these registers exist

#define JB_BX 0
#define JB_SI 1
#define JB_DI 2
#define JB_BP 3

************************************/

#define JB_SP 4
#define JB_PC 5

#define MAX_THREAD 128
#define PTHREAD_TIMER 50000		//50 ms
#define STACK_SIZE 32767

#define READY 1
#define RUNNING 2
#define EXITED 3


int first_time = 0;			//first time pthread_create is called
int threads_index = 0;		//thread total number of threads
int current = 0;			//current thread
int next_thread = 0;		//next threads
int running_counter = 0; 	//counts the running threads;
int ii = 1;					//counter
int running_threads = 0;	//number of running threads

//creates thread control block
//one struct is initialized per thread
struct Thread_Control_Block
{
	pthread_t thread_id;
	int status;
	jmp_buf thread_state;	//from setjump.h (jump data structure)
	void *stack;

};	struct Thread_Control_Block thread_array[(MAX_THREAD)];



//finds next runnable thread
//called in schedule()
void next_runnable()
{	

	//printf("next_runnable function called \n");

	int counter_runnable = 0;
	while(counter_runnable < 128)
	{	
		//increment counters!
		counter_runnable++;
		current++;
		if (current == 128)
		{
			ii++;
			current = 0;
		}

		//if current thread is ready to goooo
		if(thread_array[current].status == READY)
		{
			return;
		}
	}
	return;
}

//jumps to next thread if current one is done
void schedule()
{

	//printf("you made it to schedule!\n");

	if (setjmp(thread_array[current].thread_state) == 0)
	{
		next_runnable();			//find next runnable thread
		longjmp(thread_array[current].thread_state, 1);		
		
	}
	else
	{
		return;
	}
}


//handles the timer interrupt
void alarm_handler(int signo)
{
	schedule();
}


//sends signal for interrupts and timer
void alarm_set()
{
	//printf("woo, alarm_set\n");
	struct sigaction sigact;
	sigact.sa_handler = alarm_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_NODEFER;
	sigaction(SIGALRM, &sigact, NULL);

	if (ualarm(PTHREAD_TIMER, PTHREAD_TIMER) < 0)
	{
		perror("schedule"); 
	}
}

//timer when new thread is created
void pthread_firsttime()
{
	thread_array[next_thread].thread_id = threads_index; 	//set thread id to thread_index value
	thread_array[next_thread].status = READY;				//status ready
	

	//thread_array[next_thread].stack = malloc(STACK_SIZE * sizeof(void*));	//allocate stack 	***commented out because for some reason it made 2 stacks test fail
	setjmp(thread_array[next_thread].thread_state);

	threads_index ++;
	alarm_set();
}


//creates a new thread and registers them in Thread_Control_Block to be scheduled
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,		
void *(*start_routine)(void	*),		
void *arg)
{  
	//printf("thread is being created\n");
	
	//if first call to the function starts the timer
	if(first_time == 0)
	{
		pthread_firsttime();
		first_time = 1;
	}
	
	if(next_thread < 128)		
	{
		next_thread++;
		
		//creates new thread
		thread_array[next_thread].thread_id = threads_index;					//gets new thread id
		*thread = thread_array[next_thread].thread_id;			
		thread_array[next_thread].status = READY;								//declares status ready
		thread_array[next_thread].stack = malloc(STACK_SIZE * sizeof(void*));	//alocating stack
		
		
		void *env = thread_array[next_thread].stack + STACK_SIZE;				//goes to top of stack
		
		env -= 4;
		*((unsigned long int*)env) = (unsigned long int)arg;
		env -= 4; 
		*((unsigned long int*)env) = (unsigned long int)pthread_exit;
		
		setjmp(thread_array[next_thread].thread_state);

		thread_array[next_thread].thread_state[0].__jmpbuf[JB_SP] = ptr_mangle((unsigned long int) env);
		thread_array[next_thread].thread_state[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int) start_routine);
		//add to index

		threads_index ++;
		schedule();
		return 0;
	}
	else
	{
		return -1;
	}
}

//returns the current thread_id
pthread_t pthread_self()
{
	return thread_array[current].thread_id;
}


void pthread_exit(void *v_ptr)
{

	thread_array[current].status = EXITED;	//declare status exit
	//free(thread_array[current].stack);	//easier to decalre threads EXITED than free them, so commented out

	schedule();
	__builtin_unreachable();
}


//provided on bandit
int ptr_mangle(int p)
{
	unsigned int ret;
	asm(" movl %1, %%eax;\n"
	" xorl %%gs:0x18, %%eax;"
	" roll $0x9, %%eax;"
	" movl %%eax, %0;"
	: "=r"(ret)
	: "r"(p)
	: "%eax");
	 return ret;
}
	
