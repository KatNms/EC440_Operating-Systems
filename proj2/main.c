#include<stdio.h>
#include<pthread.h>
#include<stdlib.h>

#define THREAD_CNT 128

// waste some time
void *thread_count(void *arg) //start routine = thread_count
{
        unsigned long int c = (unsigned long int)arg;
        int i;
        for (i = 0; i < c; i++) {
                if ((i % 1000) == 0) {
                        printf("tid: 0x%x Just counted to %d of %ld\n", \
                        (unsigned int)pthread_self(), i, c);
                }
        }
    return arg;
}


int main(int argc, char **argv) //have an infinite loop?
{
	pthread_t threads[THREAD_CNT];
	int i;
    unsigned long int cnt = 100000;

    //create THREAD_CNT threads aka 128
    for(i = 0; i<THREAD_CNT; i++) 
    {
    	pthread_create(&threads[i], NULL, thread_count, (void *)((i+1)*cnt));
    }

	
    while (1) //don't want first thread to end
    {
        //run five-ever
    }
	
}
