#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

int tls_create(unsigned int size);
int tls_destroy();
int tls_clone(pthread_t tid);
int tls_write(unsigned int offset, unsigned int length, char * buffer);
int tls_read(unsigned int offset, unsigned int length, char * buffer);

pthread_t tid;
pthread_t tid2;

void * do_something(void *arg)
{
	arg = NULL;
	tls_clone(tid);
	char buffa2[10];

	int i;
	for(i = 0; i < 10; i++)
	{
		buffa2[i] = i*2;
	}
	tls_write(0, 10, buffa2);

	char buffer[10];
	tls_read(0, 10, buffer);

	for(i = 0; i < 10; i++)
	{
		printf("buffer2 - should be %d: %d\n",i*2, buffer[i]);
	}
	printf("destroy success: %d\n",tls_destroy());

	
	return arg;
}

int main(int argc, const char* argv[] )
{
	tid = pthread_self();
	tls_create(4100);
	char buffer[10];
	int i;
	for(i = 0; i < 10; i++)
	{
		buffer[i] = i;
	}
	tls_write(0, 10, buffer);

	pthread_create(&tid2, NULL, do_something, NULL);

    pthread_join(tid2, NULL);
	
	char buffa3[10];

	tls_read(0, 10, buffa3);
	printf("Managed to read... \n");

	for(i = 0; i < 10; i++)
	{
		printf("buffer3 - should be %d: %d\n", i, buffa3[i]);
	}

	printf("destroy success: %d\n",tls_destroy());
	return 0;
}
