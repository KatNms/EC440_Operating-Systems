#include <pthread.h> 
#include <sys/mman.h> 
#include <signal.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <unistd.h> 

//#define PAGE_SIZE getpagesize()

//page struct (given)
struct page 
{
	unsigned int address; // start address of page
	int ref_count; //counter for shared pages

};

//thread local storage struct (given)
struct tls
{
	pthread_t tid; 				// thread id
	unsigned int size; 			// bytes
	unsigned int page_num; 		// pages
	struct page **pages; 		// array of pointers to pages

}; struct tls TLS[128];		//chose 128 since 128 threads

//global variables
int initialized; 
int page_size; 
int tls_count; 
unsigned int p_fault;
int fault;
int has_lsa;
int LSA_target;
int idx;
int cnt; 
char* src; 


/*
I refuse

struct hash_element
{
	pthread_t tid;		
	TLS *tls;
	strct hash_element *next;

}; struct hash_element* hash_table[128];
*/


//partially given, handles page faults
void tls_handle_page_fault(int sig, siginfo_t *si, void * context)
{
	/*note: a pfault is achieved when thread 2 for example tries to access thread 1 */
	p_fault = ((unsigned int) si->si_addr) & ~(page_size - 1);
	fault = 0; 

	pthread_t current = pthread_self(); 
	int i; 
	has_lsa = -1; 
	

	for (i = 0; i < tls_count+1; i++) 
	{
		if (TLS[i].tid == current) 
		{
			//if TLS has same thread id as current thread then set the lsa index to updates value of tls_count
			has_lsa = i;
		}
	}
	if (has_lsa ==  -1) 
	{
		//no current LSA found so segfault
		printf("no LSA found for current thread, it is a seg fault \n");
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL); 
		raise(sig);

	}
	else
	{
		for (i = 0; i < TLS[has_lsa].page_num + 1; i++)
		{//from i = 0 to the page number of the that thread's local storage +1
			if (TLS[has_lsa].pages[i] -> address == p_fault)
			{//if there is a a pfault then print hte following message
				//printf("attempted/invalid data access: calling pthread_exit(NULL)! \n");
				pthread_exit(NULL);
				fault = 1; 
			}
		}
		if (fault == 0) 
		//if page fault wasn't found, then segfault 
		{
			//printf(" returning to regular seg fault \n");
			signal(SIGSEGV, SIG_DFL);
			signal(SIGBUS, SIG_DFL); 
			raise(sig);
		}
	
	}
}

//given
void tls_protect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, 0)) {
    	fprintf(stderr, "tls_protect: could not protect page\n");
    	exit(1);
	}

}

//given
void tls_unprotect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, PROT_READ | PROT_WRITE)) {
		fprintf(stderr, "tls_unprotect: could not unprotect page\n");
		exit(1); }

}

//mostly given
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

	//added these 2 lines
	tls_count = 0; 
	initialized = 1; 
}


int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
	
	printf("WRITE called \n"); 
	pthread_t current = pthread_self(); 
	int i; 
	int idx; 
	int cnt; 
	char* dst; 
	has_lsa = -1; 

	struct tls *read_tls;

	//error handling
	//get current thread's TLS
	for (i = 0; i < tls_count+1; i++)
	{
		if (TLS[i].tid == current)
		{ 
			has_lsa = i;
		}
	}


	
	if (TLS[has_lsa].size < offset + length)
	{
		//offset+length is greater than LSA size ERROR
		return -1; 
	}


	if (has_lsa == -1) 
	{
		
		return -1; 
	}

	printf("WRITE is great\n"); 
	

	// unprotect all pages belonging to thread's tls 
	
	//use i in case i was making me fail. idk how it just does wierd shit

	for (i = 0; i < TLS[has_lsa].page_num; i++) 
	{
		printf("UNPROTECTED \n"); 
		tls_unprotect(TLS[has_lsa].pages[i]);
	}
	
	// perform write operation (provided and slighted modified)
	printf("write op \n"); 
    struct page *p, *copy;	//create a page of pointers
    unsigned int pn, poff;	
    pn = idx / page_size;	//get pagenumber
    poff = idx % page_size;	//byte page offset
	int length_pages = (poff + length + page_size-1)/page_size; 
	int length_copy = 0;
	 
	for (cnt = 0, idx = offset; idx < (offset + length); ++cnt, ++idx) 
	 {
	 	//unprotect current page
	 	if(mprotect((void*) TLS[has_lsa].pages[i]->address, page_size, PROT_WRITE)== -1)
	 	{
	 		return -1;
	 	}
		  
	   p = TLS[has_lsa].pages[pn];	//mod, get page from array



	   if (p->ref_count > 1) {
	       /* this page is shared, create a private copy (COW) */
	   	

	   	//not working, FIX later



			copy = (struct page *) calloc(1, sizeof(struct page));	//creates a memory space for copy
			copy->address = (unsigned int) mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);	//create copy of page
			copy->ref_count = 1; //set ref count
			TLS[has_lsa].pages[pn] = copy; //change page to copy page
			/* update original page */
			

			p->ref_count--;
			tls_protect(p);
			p = copy;
		}
	   	dst = ((char *) p->address) + poff;
	   	*dst = buffer[cnt];
	   	
	   printf("tls_write wrote: %d\n", *dst);
	 } 


	// protect all pages belonging to thread's tls
	
	for (i = 0; i < TLS[has_lsa].page_num; i++) 
	{
		tls_protect(TLS[has_lsa].pages[i]);
	}


	return 0; 
}
int tls_create(unsigned int size)
{
	//printf("CREATE called \n"); 
	pthread_t current = pthread_self(); 
	int i; 

	// error handling
	if (!initialized) 
	{
		printf("must initalize\n"); 
		tls_init(); //call the init because its the first time initalizing
	}

	
	if (size < 0) 
	{
		printf("in create, size is not greater than 0\n");
		return -1; 
	}

	//local storage area check, first reset lsa index to -1
	has_lsa = -1; 
	while(i < 128)
	{
		if (TLS[i].tid == current) 
		{
			has_lsa = i; 
			
		}	
		else if (has_lsa > -1)
		{			
			 return -1; 
		}
		i++;
	}

	// allocate tls 
	struct tls* tls_new = (struct tls*) calloc(1, sizeof(struct tls));
	//printf("allocated tls_new \n"); 
	
	// initalize tls
	tls_new -> tid = pthread_self(); 
	tls_new -> size = size; 
	tls_new -> page_num = (size-1)/(page_size ) + 1 ; 

	// allocate pages of the tls
	tls_new -> pages =  (struct page**) calloc(tls_new -> page_num, sizeof(struct page*)); 
	
	// allocate all pages for this tls 
	for (i = 0; i < tls_new -> page_num; i++)
	{
		struct page* p = (struct page *) calloc(1, sizeof(struct page*));
		p -> address = (unsigned int) mmap(0, page_size, 0, MAP_ANON | MAP_PRIVATE, 0, 0); 
		p -> ref_count = 1;
		tls_new -> pages[i] = p; 
	}

	// add this id and tls mapping to global hash table
	tls_count++; 
	has_lsa = -1; 

	while (i < 128) 
	//checking if any deleted indices are free
	{
		if (TLS[i].size == -1) 
		{//if the size is -1 then the index was deleted
		//set lsa index to i
			has_lsa = i; 
			TLS[i] = *tls_new;  
		}

		
		else if (has_lsa == -1) 
		{
			TLS[tls_count] = *tls_new; // if no deleted found, then save at next index
		}

		i++;

	}
	return 0; 

}


int tls_read(unsigned int offset, unsigned int length, char *buffer)
{

	//checking if current thread has an LSA
	pthread_t current = pthread_self(); 
	int i; 
	has_lsa = -1; 

	//same error handling as before
	for (i = 0; i < tls_count+1; i++)
	{
		if (TLS[i].tid == current)
		{ 
			has_lsa = i; 
		}
	}
	if (has_lsa == -1) 
	{
		return -1; 
	}

	if (TLS[has_lsa].size < offset + length)
	{ 
		return -1; 
	}
	
	// unprotect all pages belonging to thread's tls , same as before
	for (i = 0; i < TLS[has_lsa].page_num; i++) 
	{
		printf("UNPROTECT \n");
		tls_unprotect(TLS[has_lsa].pages[i]);
	}
	printf("page_num: %d \n", TLS[has_lsa].page_num); 
	
	
	// perform read operation(mostly provided)
	for (cnt = 0, idx = offset; idx < (offset +length); ++cnt, ++idx) 
	{
		printf("looped through: %d\n", cnt); //check to see if for loop worked successfully
	    struct page *p;			//page struct
	    unsigned int pn, poff;
	    pn = idx / page_size;
	    poff = idx % page_size;
	    p = TLS[has_lsa].pages[pn];	//mod line to reference my table
	    src = ((char *) p->address) + poff;
	    buffer[cnt] = *src;
	 }

	// protect all pages belonging to thread's tls
	for (i = 0; i < TLS[has_lsa].page_num; i++) 
	{
		tls_protect(TLS[has_lsa].pages[i]); 	
	}
 
	return 0; 

}

int tls_destroy()
{

	printf("DESTROY called\n");
	pthread_t current = pthread_self(); 
	int i; 
	has_lsa = -1;

	//error handling same as before but you dont need to chength size, doesnt matter here
	
	for (i = 0; i < tls_count+1; i++)
	{
		if (TLS[i].tid == current) 
		{
			has_lsa = i; 
		}
	}
	if (has_lsa == -1) 
	{
			return -1; 
	}

	// clean up all pages
	for (i = 0; i < TLS[has_lsa].page_num; i++){
		if (TLS[has_lsa].pages[i] -> ref_count == 1) 
		//if no other reference to page
		{
			munmap((void*)TLS[has_lsa].pages[i] -> address, page_size); 
			//deletes page
		}
		else TLS[has_lsa].pages[i] -> ref_count--; 
	}

	// clean up tls 
	free(TLS[has_lsa].pages);
	TLS[has_lsa].size = -1; 
	return 0;
}

int tls_clone(pthread_t tid)
{
	pthread_t current = pthread_self(); 
	int i; 
	has_lsa = -1;

	//error handling same as before
	for (i = 0; i < tls_count+1; i++)
	{
		if (TLS[i].tid == current) 
		{
			has_lsa = i; 
		}
	}
	if (has_lsa > -1) 
	{
		//printf(" current thread already has LSA \n "); 
		return -1; 
	}

	// checking if target thread has an LSA. same as usual but lsa index is now lsa target
	LSA_target = -1 ;
	for (i = 0; i < tls_count+1; i++)
	{
		if (TLS[i].tid == tid) 
		{
			LSA_target = i; 
		}
	}
	if (LSA_target == -1) 
	{
		printf("ERROR: target thread has no LSA \n "); 
		return -1; 
	}

	// do cloining, allocate tls
	struct tls* tls_clone = (struct tls*) calloc(1, sizeof(struct tls));
	

	// initalize tls
	tls_clone -> tid = current; 
	tls_clone -> size = TLS[LSA_target].size; 
	tls_clone -> page_num = TLS[LSA_target].page_num; 

	// allocate pages of the tls
	tls_clone -> pages = (struct page**) calloc(tls_clone -> page_num, sizeof(struct page*)); 
	
	// allocate all pages for this tls and copy pages/update references
	for (i = 0; i < tls_clone -> page_num; i++)
	{
		TLS[LSA_target].pages[i] -> ref_count++; 
		tls_clone -> pages[i] = TLS[LSA_target].pages[i]; 
	}

	// add this thread to table
	tls_count++; 
	has_lsa = -1; 
	
	for (i = 0; i < tls_count+1; i++) 
	//loops through all of tls_count to see if any thread's have local storage of size -1 meaning they are deleted
	{
		if (TLS[i].size == -1) 
		{
			has_lsa = i; 
		}
	}
	if (has_lsa == -1) 
	{
		TLS[tls_count] = *tls_clone; 
		// no TLS was deleted so save at next index
	}
	else
	{ 
		TLS[i] = *tls_clone; 
		//now clone it
	}

	return 0;
}



