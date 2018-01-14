/*
Katrina Nemes
Project 3 OS
*/


//libraries included
#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <semaphore.h>
//#include "queue.h"


/***********************************
For reference, these registers exist

#define JB_BX 0
#define JB_SI 1
#define JB_DI 2
#define JB_BP 3

************************************/

#define JB_SP 4			//stack pointer
#define JB_PC 5			//program counter

#define MAX_THREAD 128
#define PTHREAD_TIMER 50000		//50 ms
#define STACK_SIZE 32767
#define MAX_SEM 256

#define EXITED 0
#define EMPTY 1
#define READY 2
#define RUNNING 3
#define UNUSED 4
#define BLOCKED 5


int first_time = 0;			//first time pthread_create is called
int threads_index = 0;		//thread total number of threads
int current = 0;			//current thread
int next_thread = 0;		//next threads
int running_counter = 0; 	//counts the running threads;
//int ii = 1;					//counter
int running_threads = 0;	//number of running threads
int sem_index = 1;

//creates thread control block
//one struct is initialized per thread, stores info about the thread (registers, state, etc)
struct Thread_Control_Block
{
	pthread_t thread_id;
	int status;
	jmp_buf thread_state;	//from setjump.h (jump data structure)
	void *stack;

	void *exit_status;
	pthread_t join_array[128];

};	struct Thread_Control_Block thread_array[128];

//semaphores
struct SEM
{
	int head;
	int bottom;
	int flag;		//if the semaphore hasn't been used
	int queue[128];
	int sem_value;
}; struct SEM sem_array[128];

//struct SEM semaphore_array[(MAX_SEM)];


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

//locking mechanism
//pause scheduling alarm
void lock()
{
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGALRM);
    sigprocmask(SIG_BLOCK, &signal_set, NULL);
}


//resume scheduling alarm
void unlock()
{
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
}

//return value of thread without calling the pthread_exit
void pthread_exit_wrapper()
{
	unsigned int res;
	asm("movl %%eax, %0\n":"=r"(res));
	pthread_exit((void *) res);

}

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
		if(current == 128)
		{
			//ii++;
			current = 0;
		}

		//this is the next thread
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
	lock();

	//printf("you made it to schedule!\n");

	if (setjmp(thread_array[current].thread_state) == 0)
	{
		next_runnable();			//find next runnable thread
		longjmp(thread_array[current].thread_state, 1);		
		unlock();
		
	}
	else
	{
		unlock();
		return;
	}
}


//handles the timer interrupt
void alarm_handler(int signo)
{
	schedule();
}


//sends signal for interrupts and timer every 50ms
void alarm_set()
{
	//printf("woo, alarm_set\n");
	struct sigaction sigact;				//sigaction structure create
	sigact.sa_handler = alarm_handler;		//add the alarm handler
	sigemptyset(&sigact.sa_mask);			//reinitializes signal set
	sigact.sa_flags = SA_NODEFER;			//flag, if there's an old alarm, it wont interfere with new alarm because signal doesnt get received within its handler
	sigaction(SIGALRM, &sigact, NULL);		//creating a new alarm when you receive the SIGALARM flag

	if (ualarm(PTHREAD_TIMER, PTHREAD_TIMER) < 0)	//if ualarm returns a value less than zero there's something wrong with the scheduler
	{												//supposed to return the remaining ms in your timer
		perror("schedule"); 
	}
}

//timer when new thread is created
void pthread_firsttime()
{
	thread_array[next_thread].thread_id = threads_index; 	//set thread id to thread_index value
	thread_array[next_thread].status = READY;				//status ready
	
	//thread_array[next_thread].stack = malloc(STACK_SIZE);	//allocate stack 	***commented out because for some reason it made 2 stacks test fail
	setjmp(thread_array[next_thread].thread_state);

	threads_index ++;
	alarm_set();
}


//creates a new thread and registers them in Thread_Control_Block to be scheduled
int pthread_create(pthread_t *thread, 
	const pthread_attr_t *attr,		
	void *(*start_routine)(void	*),		
	void *arg)
{  

	lock();

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
		
		env -= 4;	//stack pointer decrement
		*((unsigned long int*)env) = (unsigned long int)arg;
		env -= 4; 
		*((unsigned long int*)env) = (unsigned long int)pthread_exit_wrapper;
		
		setjmp(thread_array[next_thread].thread_state);

		thread_array[next_thread].thread_state[0].__jmpbuf[JB_SP] = ptr_mangle((unsigned long int) env);
		thread_array[next_thread].thread_state[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int) start_routine);
		//add to index

		threads_index++;
		

		unlock();
		schedule();
		return 0;
	}
	else
	{
		unlock();
		return -1;
	}
}

//returns the current thread_id
pthread_t pthread_self()
{
	return thread_array[current].thread_id;
}

//modified
void pthread_exit(void *value_ptr)
{
	int index = 0;
	//lock(); //block sigalarm so scheduler doesnt get called while destroyign the thread


	int j;
	for(j=0; j < 128; j++)
	{
		//if the thread_id is equal to the thread id in join array
		index = thread_array[current].join_array[j];
		//break;
		//int index
		//you get thread index from index = thread_arra[curent].join_array[j];
		//thread_array[index].status = Ready

		thread_array[index].status = READY;
	}
	
		threads_index--;		
	

	//clean up resources
	thread_array[current].status = EXITED;
	thread_array[current].exit_status = value_ptr;	



	schedule();
	__builtin_unreachable();
}


int pthread_join(pthread_t thread, void **value_ptr)
{
	lock();
	int i = 0;
	//set the value_ptr to the thread's exit value 
	
	//check if the thread's in use, block it
	//add to blocked array

	
	while(thread_array[i].thread_id != thread && i < 128 )
	{
		i++;
	}
	
	if(thread_array[i].status != EXITED) //if the thread is not EXITED
	{
		thread_array[current].status = BLOCKED;

		int j;
		for (j = 0; j < 128; j++)
		{
			//if the thread has the same thread id as anything in the thread array
			if(thread_array[j].thread_id == thread) 
			{
				int k;
				for (k = 0; k < 128; k++)
				{
					if(thread_array[j].join_array[k] == 0)
					{
						thread_array[j].join_array[k] = thread_array[current].thread_id;
						break;
					}
				}
			}
		}
	}

	schedule();
	if(value_ptr !=NULL)
    {
        *value_ptr = thread_array[thread].exit_status;
    }

	unlock();
	return 0;


}


int sem_init (sem_t *sem, int pshared, unsigned value)
{
	/*
	int n;
	for(n=0; n<128;n++)
	{
		if(sem_array[n].flag==0)
		{
			break;
			//you found a semaphore you can use/is unitialized
		}
	}


	sem_array[n].sem_value = value;
	sem_array[n].flag = 1;		//semaphore is now initialized
	sem_array[n].bottom=0;
	sem_array[n].head=0;



	sem->__align = n;	//saves which semaphore you're referring to, so when you use it for wait, you'll save your index
	//makes indexing easier into whatever semaphore you want
	 //stores reference to sem struct in sem_t struct
	//look for one that is not initialized and use it 	
*/
	return 0;
}

int sem_wait(sem_t *sem)
{
	/*
	lock();
	int m;

	for(m=0; m<128; m++)
	{
		if(sem_array[m].sem_value > 0)
		{
			sem_array[m].sem_value--;
			//schedule();
			unlock();

			return 0;
		}

		if(sem_array[m].sem_value == 0)
		{
			//block
			thread_array[current].status = BLOCKED;
			schedule();

		}
	}


	unlock();
	*/
	return 0;

}

int sem_post(sem_t *sem)
{
	/*
	lock();

	int y;

		if(sem_array[y].sem_value > 0)
		{
			sem_array[y].sem_value++;
			int z;
			for(z=0;z < 128; z++)
			{
				if(thread_array[z].status == BLOCKED)
				{
					break;
				}
			}
			thread_array[z].status = READY;	
		}
	
	unlock();
	*/
	return 0;

}

int sem_destory(sem_t *sem)
{
	/*
	lock();

	int q;
	q = sem->__align;
	sem_array[q].flag = 0;

	unlock();
*/
	return 0;

}

