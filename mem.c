#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>
#include "mem.h"

typedef struct block_hd{
	struct block_hd* next;
	int size_status;
}block_header;

block_header* list_head = NULL;
static int uno_alloc = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

int Mem_Init(int size)
{
	int psize;
 	int puffer;
	int fd;
	short int alloc_size;
 	void* space_ptr;
  
 	if(0 != uno_alloc)
  	{
    		fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
    		return -1;
  	}
  	if(size <= 0)
  	{
    		fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
    		return -1;
  	}
  	if (pthread_mutex_init(&lock, NULL) != 0)
  	{
    		fprintf(stderr,"Error:mutex init failed\n");
    		return -1;
  	}	

  	psize = getpagesize();
  	puffer = size % psize;
  	puffer = (psize - puffer) % psize;
  	alloc_size = size + puffer;

  	fd = open("/dev/zero", O_RDWR);
  	if(-1 == fd)
  	{
    		fprintf(stderr,"Error:mem.c: Cannot open /dev/zero\n");
    		return -1;
  	}
  	space_ptr = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  	if (MAP_FAILED == space_ptr)
  	{
    		fprintf(stderr,"Error:mem.c: mmap cannot allocate space\n");
    		uno_alloc = 0;
    		return -1;
  	}
  
 	 uno_alloc = 1;
  
  	list_head = (block_header*)space_ptr;
  	list_head->next = NULL;
  	list_head->size_status = alloc_size - (int)sizeof(block_header);
  
  	return 0;
}


void* Mem_Alloc(int size)
{
	block_header* temp = list_head;
	block_header* addr;
	short int mem_left; 
	int h_size = (int)sizeof(block_header);
	void* ret;
 	
	if ((size % 8) != 0)
		size = size - (size % 8) + 8;

	pthread_mutex_lock(&lock);	
	while (temp != NULL) {
		mem_left = temp->size_status - size - h_size; 
		// size of new free block
		if ( (mem_left > 0) && !(temp->size_status & 1)) {
			temp->size_status = size + 1;
			ret = (void *) (temp + h_size);
			addr = temp->next;
			// makes next free block
			temp->next = (block_header *)(ret + size);
			temp->next->size_status = mem_left;
			temp->next->next = addr;
			pthread_mutex_unlock(&lock);
			return ret;
		}
		else if((mem_left >= -h_size)&& !(temp->size_status & 1)){
		// can't split into 2 blocks so just change status to allocated
		// because there's only room for header
			
			// to show that block is allocated
			temp->size_status += 1;
			pthread_mutex_unlock(&lock);
			return (void *)(temp + h_size);
		}
		temp = temp->next;
	}
	pthread_mutex_unlock(&lock);	
	return NULL;
}

int Mem_Free(void* ptr)
{
	if (ptr == NULL)
		return -1;

	// declaring variables
	block_header* temp = list_head;
	int h_size = (int)sizeof(block_header);

	pthread_mutex_lock(&lock);
	// looks if first block is the one we're trying to free
	if (temp + h_size == ptr)
 	{
		if (temp->next != NULL) {
			int n_size = temp->next->size_status;
			if (!(n_size & 1))
			// next block is free
			// then we coalesce
			{
				temp->size_status += n_size + h_size;
				temp->next = temp->next->next;
			}
		}
		temp->size_status--;
		pthread_mutex_unlock(&lock);
		return 0;
	}

	while (temp != NULL) {
		if ((temp->next != NULL) && ((temp->next) + h_size == ptr))
		// next block is it
		{
			if(!(temp->size_status & 1))
			// we can coalesce it
			{
				temp->size_status += temp->next->size_status + h_size;
				temp->next = temp->next->next;
			} else 
			// you just skip over it if you can't
				temp = temp->next;
			
			// sees if we can coalesce with the block following the one we want to free

			if (temp->next != NULL){
				int n_size = temp->next->size_status;
				if (!(n_size & 1))
				// next block is free
				{
					temp->size_status += n_size + h_size;
					temp->next = temp->next->next;
				}
			}
			// free it
			temp->size_status--;
			pthread_mutex_unlock(&lock);
			return 0;
		}
		temp = temp->next;
	}
	pthread_mutex_unlock(&lock);
	return -1;
}


int
Mem_Available(){
	pthread_mutex_lock(&lock);
	int total = 0;
	block_header* temp = list_head;

	while (temp != NULL) {
		if(!(temp->size_status & 1))
		total += temp->size_status;
	}

	pthread_mutex_unlock(&lock);
	return total;
} 

void 
Mem_Dump() {
	block_header* temp = list_head;

	while (temp != NULL){
		if (temp->size_status & 1) {//not free
			fprintf(stdout,"Allocated\t%p\tsize:\t%d",temp,temp->size_status);
		}
		else
			fprintf(stdout,"Allocated\t%p\tsize:\t%d",temp,temp->size_status);

	}	
	return;
}
