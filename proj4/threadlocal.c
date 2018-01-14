#include <pthread.h> 
#include <sys/mman.h> // mmap 
#include <signal.h> // sigaction
#include <stdlib.h> 
#include <stdio.h> 
#include <unistd.h> //getpagesize

#define TABLE_SIZE 288 //HOW BIG SHOULD THIS BE?

struct TLS
{
	pthread_t tid; //id of thread that this TLS is for
	unsigned int size; // size of TLS in bytes
	unsigned int page_num; // number of pages in bytes 
	struct page **pages; // array of pointers to pages
};

struct page 
{
	unsigned int address; // start address of page
	int ref_count; //counter for shared pages

};


int initialized; 
int page_size; // = 0 maybe?
struct TLS TLS_table[TABLE_SIZE];
int TLS_count; 


void tls_handle_page_fault(int sig, siginfo_t *si, void * context)
{
	// if thread tried to access another threads tls
	unsigned int pfault = ((unsigned int) si->si_addr) & ~(page_size - 1);
	int foundpfault = 0; 

	int currentTid = pthread_self(); 
	int j; 
	int foundLSA = -1; 
	//finding LSA 
	for (j = 0; j < TLS_count+1; j++) 
	{
		if (TLS_table[j].tid == currentTid) foundLSA = j; // current thread has an LSA 
	}
	if (foundLSA ==  -1) //no current LSA found, must be regular segfault
	{
		printf(" no LSA found for current thread, returning to regular seg fault \n");
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL); 
		raise(sig);

	}
	else
	{
		// for each page
		int i;
		for (i = 0; i < TLS_table[foundLSA].page_num + 1; i++)
		{
			if (TLS_table[foundLSA].pages[i] -> address == pfault)
			{
				printf(" invalid data access: exiting thread! \n");
				pthread_exit(NULL);
				foundpfault = 1; 
			}
		}
		if (foundpfault == 0) //if page fault wasn't found, then regular segfault 
		{
			printf(" returning to regular seg fault \n");
			signal(SIGSEGV, SIG_DFL);
			signal(SIGBUS, SIG_DFL); 
			raise(sig);
		}
	
}

}

void tls_init()
{
	struct sigaction sigact; 

	// get size of a page
	page_size = getpagesize();

	// install signal handler for page faults (SIGSEGV, SIGBUS)
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_SIGINFO; // use extending signal handling
	sigact.sa_sigaction = tls_handle_page_fault; 

	sigaction(SIGBUS, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL); 

	//initialzing the table that holds all the TLS 
	//TLS_table = calloc(TABLE_SIZE, sizeof(struct TLS)); 
	TLS_count = 0; 

	initialized = 1; 
}


int tls_create(unsigned int size)
// creates a local storage area (LSA) for can hold a size amount of bytes for currently running thread
// returns 0 on success or -1 on error (if the thread already has storage over 0 bytes)
// once the tls is created, it can be read or written to 
{
	printf("tls create called \n"); 
	if (initialized == 0) // first time calling tls_create
	{
		printf(" initializing \n"); 
		tls_init(); 
	}

	// error handling: check if that size > 0 
	if (size < 0) return -1; 

	//checking if current thread has an LSA
	int currentTid = pthread_self(); 
	int j; 
	int foundLSA = -1; 
	for (j = 0; j < TLS_count+1; j++)
	{
		if (TLS_table[j].tid == currentTid) foundLSA = j; // current thread has an LSA 
	}
	if (foundLSA > -1) return -1; 


	// allocate TLS using calloc
	struct TLS* newTLS = (struct TLS*) calloc(1, sizeof(struct TLS));
	printf("allocated new TLS \n"); 
	// initalize TLS
	newTLS -> tid = pthread_self(); 
	newTLS -> size = size; 
	newTLS -> page_num = (size-1)/(page_size ) + 1 ; 

	// allocate pages of the TLS: array of pointers using calloc
	newTLS -> pages =  (struct page**) calloc(newTLS -> page_num, sizeof(struct page*)); 
	
	// allocate all pages for this TLS 
	int i;
	for (i = 0; i < newTLS -> page_num; i++)
	{
		struct page* p = (struct page *) calloc(1, sizeof(struct page*));
		p -> address = (unsigned int) mmap(0, page_size, 0, MAP_ANON | MAP_PRIVATE, 0, 0); 
		p -> ref_count = 1;
		newTLS -> pages[i] = p; 
	}

	// add this id and TLS mapping to global hash table
	TLS_count++; 
	foundLSA = -1; 
	for (j = 0; j < TLS_count+1; j++) //checking if any deleted indices are free
	{
		if (TLS_table[j].size == -1) foundLSA = j; // size is -1  if index was deleted 
	}
	if (foundLSA == -1) TLS_table[TLS_count] = *newTLS; // if no deleted found, then save at next index
	else  TLS_table[j] = *newTLS; 

	return 0; 

}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
// reads in length bytes, started from memory location buffer and writes them into the storage area of current thread
// returns 0 on success or -1 on error (if asked to write more mem than the LSA has or if the thread has no LSA)
// function assumes that the buffer holds at least length bytes
{
	
	printf("tls write called \n"); 
	//checking if current thread has an LSA
	int currentTid = pthread_self(); 
	int j; 
	int foundLSA = -1; 
	for (j = 0; j < TLS_count+1; j++)
	{
		if (TLS_table[j].tid == currentTid) foundLSA = j; // current thread has an LSA 
	}
	if (foundLSA == -1) return -1; 

	// LSA can only hold offset + length < size of LSA 
	if (TLS_table[foundLSA].size < offset + length) return -1; 


	printf(" no error in tls write \n"); 
	// unprotect all pages belonging to thread's TLS 
	int k;
	for (k = 0; k < TLS_table[foundLSA].page_num; k++) 
	{
		printf("  unprotecting page \n"); 
		tls_unprotect(TLS_table[foundLSA].pages[k]);
	}
	
	// perform write operation 
	int idx, cnt; 
	char* dst; 
	for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) 
	 {
	 	printf("   performing write operation \n"); 
	   struct page *p, *copy;
	   unsigned int pn, poff;
	   pn = idx / page_size;
	   poff = idx % page_size;
	   //tls_unprotect(TLS_table[foundLSA].pages[pn]);
	   
	   p = TLS_table[foundLSA].pages[pn];


	   if (p->ref_count > 1) {
	       /* this page is shared, create a private copy (COW) */
			copy = (struct page *) calloc(1, sizeof(struct page));
			copy->address = (unsigned int) mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
			copy->ref_count = 1;
			TLS_table[foundLSA].pages[pn] = copy;
			/* update original page */
			p->ref_count--;
			tls_protect(p);
			p = copy;
		}
	   	dst = ((char *) p->address) + poff;
	   	*dst = buffer[cnt];
	   	
	   //printf("    unprotecting page... %d\n", *dst);
	   //tls_protect(TLS_table[foundLSA].pages[pn]);
	   printf("    writing: %d\n", *dst);
	 } 


	// protect all pages belonging to thread's TLS
	
	for (k = 0; k < TLS_table[foundLSA].page_num; k++) 
	{
		tls_protect(TLS_table[foundLSA].pages[k]);
	}


	return 0; 
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
// reads length bytes from the LSA, starting at point offset, and writes them to what buffer is pointing to
// returns 0 on success or -1 on error (if asked to read past the LSA or if thread has no LSA)
// the function trusts that the buffer is large enough to hold at least length bytes
{

	//checking if current thread has an LSA
	int currentTid = pthread_self(); 
	int j; 
	int foundLSA = -1; 
	for (j = 0; j < TLS_count+1; j++)
	{
		if (TLS_table[j].tid == currentTid) foundLSA = j; // current thread has an LSA 
	}
	if (foundLSA == -1) return -1; 

	// LSA can only hold offset + length < size of LSA 
	if (TLS_table[foundLSA].size < offset + length) return -1; 

	// unprotect all pages belonging to thread's TLS 
	int k;
	for (k = 0; k < TLS_table[foundLSA].page_num; k++) 
	{
		printf("  unprotecting page \n");
		tls_unprotect(TLS_table[foundLSA].pages[k]);
	}
	printf(" PAGE NUM %d \n", TLS_table[foundLSA].page_num); 
	
	// perform read operation 
	int idx, cnt; 
	char* src; 
	for (cnt = 0, idx = offset; idx < (offset +length); ++cnt, ++idx) 
	{
		printf(" on iteration %d\n", cnt); 
	    struct page *p;
	    unsigned int pn, poff;
	    pn = idx / page_size;
	    //tls_unprotect(TLS_table[foundLSA].pages[pn]);
	    poff = idx % page_size;
	    p = TLS_table[foundLSA].pages[pn];
	    src = ((char *) p->address) + poff;
	    buffer[cnt] = *src;
	   // tls_protect(TLS_table[foundLSA].pages[pn]);
	}

	// protect all pages belonging to thread's TLS
	for (k = 0; k < TLS_table[foundLSA].page_num; k++) 
	{
		tls_protect(TLS_table[foundLSA].pages[k]);
	}
 
	return 0; 

}

int tls_destroy()
// destroys the previously allocated LSA of the current thread
// returns 0 on success, -1 if the thread does not have an LSA
{

	printf(" tls destroyed called \n");
	//checking if current thread has an LSA
	int currentTid = pthread_self(); 
	int j; 
	int foundLSA = -1;
	for (j = 0; j < TLS_count+1; j++)
	{
		if (TLS_table[j].tid == currentTid) foundLSA = j; // current thread has an LSA 
	}
	if (foundLSA == -1) 
	{
			printf(" lsa not found \n ");
			return -1; 
	}

	// clean up all pages
	int i;
	for (i = 0; i < TLS_table[foundLSA].page_num; i++){
		if (TLS_table[foundLSA].pages[i] -> ref_count == 1) //if no other reference to page
		{
			munmap((void*)TLS_table[foundLSA].pages[i] -> address, page_size); //delete page
		}
		else TLS_table[foundLSA].pages[i] -> ref_count--; //descrease reference count
	}

	// clean up TLS 
	free(TLS_table[foundLSA].pages);

	//free(TLS_table[foundLSA]); 
	TLS_table[foundLSA].size = -1; //size = -1 makes it so another LSA can take this index
	// remove TLS from table
	printf(" tls destroyed finished \n");
	return 0; 
}

int tls_clone(pthread_t tid)
// clones the LSA of the target thread tid 
// the LSA is not copied, the current thread will instead point to the same location as tid 
// returns 0 on success, -1 if current thread already has an LSA or if tid does not have an LSA
{

	//checking if current thread has an LSA
	int currentTid = pthread_self(); 
	int j; 
	int foundLSA = -1;
	for (j = 0; j < TLS_count+1; j++)
	{
		if (TLS_table[j].tid == currentTid) foundLSA = j; // current thread has an LSA 
	}
	if (foundLSA > -1) {
		printf(" current thread already has LSA \n "); 
		return -1; 
	}

	// checking if target thread has an LSA
	int foundtargetLSA = -1 ;
	for (j = 0; j < TLS_count+1; j++)
	{
		if (TLS_table[j].tid == tid) foundtargetLSA = j; // target thread has no LSA 
	}
	if (foundtargetLSA == -1) {
		printf(" target thread has no LSA \n "); 
		return -1; 
	}

	// do cloining, allocate TLS
	// allocate TLS using calloc
	struct TLS* clonedTLS = (struct TLS*) calloc(1, sizeof(struct TLS));
	printf("allocated new TLS to clone \n"); 
	// initalize TLS
	clonedTLS -> tid = currentTid; 
	clonedTLS -> size = TLS_table[foundtargetLSA].size; 
	clonedTLS -> page_num = TLS_table[foundtargetLSA].page_num; 

	// allocate pages of the TLS: array of pointers using calloc
	clonedTLS -> pages = (struct page**) calloc(clonedTLS -> page_num, sizeof(struct page*)); 
	
	// allocate all pages for this TLS 
	// copy pages and adjust reference counts
	int i;
	for (i = 0; i < clonedTLS -> page_num; i++)
	{
		//struct page* p = calloc(1, sizeof(page_size)); ;
		TLS_table[foundtargetLSA].pages[i] -> ref_count++; 
		clonedTLS -> pages[i] = TLS_table[foundtargetLSA].pages[i]; 
	}

	// add this thread to table
	TLS_count++; 
	foundLSA = -1; 
	for (j = 0; j < TLS_count+1; j++) //checking if any deleted indices are free
	{
		if (TLS_table[j].size == -1) foundLSA = j; // size is -1  if index was deleted 
	}
	if (foundLSA == -1) TLS_table[TLS_count] = *clonedTLS; // if no deleted found, then save at next index
	else TLS_table[j] = *clonedTLS; 

	return 0;
}


void tls_protect(struct page *p){

	if (mprotect((void *) p->address, page_size, 0)) {
    	fprintf(stderr, "tls_protect: could not protect page\n");
    	exit(1);
	}

}

void tls_unprotect(struct page *p){


	if (mprotect((void *) p->address, page_size, PROT_READ | PROT_WRITE)) {
		fprintf(stderr, "tls_unprotect: could not unprotect page\n");
		exit(1); }

}

















