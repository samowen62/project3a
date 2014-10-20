#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "mem.h"

/* this structure serves as the header for each block */
typedef struct block_hd{
  /* The blocks are maintained as a linked list */
  /* The blocks are ordered in the increasing order of addresses */
  struct block_hd* next;

  /* size of the block is always a multiple of 4 */
  /* ie, last two bits are always zero - can be used to store other information*/
  /* LSB = 0 => free block */
  /* LSB = 1 => allocated/busy block */

  /* For free block, block size = size_status */
  /* For an allocated block, block size = size_status - 1 */

  /* The size of the block stored here is not the real size of the block */
  /* the size stored here = (size of block) - (size of header) */
  int size_status;

}block_header;

/* Global variable - This will always point to the first block */
/* ie, the block with the lowest address */
block_header* list_head = NULL;


/* Function used to Initialize the memory allocator */
/* Not intended to be called more than once by a program */
/* Argument - sizeOfRegion: Specifies the size of the chunk which needs to be allocated */
/* Returns 0 on success and -1 on failure */
int Mem_Init(int sizeOfRegion)
{
  int pagesize;
  int padsize;
  int fd;
  int alloc_size;
  void* space_ptr;
  //static int allocated_once = 0;
  
 /* if(0 != allocated_once)
  {
    fprintf(stderr,"Error:mem.c: Mem_Init has allocated space during a previous call\n");
    return -1;
  }*/
  if(sizeOfRegion <= 0)
  {
    fprintf(stderr,"Error:mem.c: Requested block size is not positive\n");
    return -1;
  }

  pagesize = getpagesize();

  padsize = sizeOfRegion % pagesize;
  padsize = (pagesize - padsize) % pagesize;

  alloc_size = sizeOfRegion + padsize;

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
    allocated_once = 0;
    return -1;
  }
  
  //allocated_once = 1;
  
  /* To begin with, there is only one big, free block */
  list_head = (block_header*)space_ptr;
  list_head->next = NULL;
  /* Remember that the 'size' stored in block size excludes the space for the header */
  list_head->size_status = alloc_size - (int)sizeof(block_header);
  
  return 0;
}


/* Function for allocating 'size' bytes. */
/* Returns address of allocated block on success */
/* Returns NULL on failure */
/* Here is what this function should accomplish */
/* - Check for sanity of size - Return NULL when appropriate */
/* - Round up size to a multiple of 4 */
/* - Traverse the list of blocks and allocate the first free block which can accommodate the requested size */
/* -- Also, when allocating a block - split it into two blocks when possible */
/* Tips: Be careful with pointer arithmetic */
void* Mem_Alloc(int size)
{

	block_header* temp = list_head;
	block_header* addr;
	int length; 
	int h_size = (int)sizeof(block_header);
	void* ret;
 	
	if ((size % 8) != 0)
		size = size - (size % 8) + 8;
	
	while (temp != NULL) {
		mem_left = temp->size_status - size - h_size; 
		// size of new free block
		if ( (mem_left > 0) && !(temp->size_status & 1)) {
			temp->size_status = size + 1;
			ret = (void *) ((int)temp + h_size);
			addr = temp->next;
			// makes next free block
			temp->next = (block_header *)((int)ret + size);
			temp->next->size_status = mem_left;
			temp->next->next = addr;
			return ret;
		}
		else if((mem_left >= -h_size)&& !(temp->size_status & 1)){
		// can't split into 2 blocks so just change status to allocated
		// because there's only room for header
			
			// to show that block is allocated
			temp->size_status += 1;
			return (void *)((int)temp + h_size);
		}
		temp = temp->next;
	}	
	return NULL;
}

/* Function for freeing up a previously allocated block */
/* Argument - ptr: Address of the block to be freed up */
/* Returns 0 on success */
/* Returns -1 on failure */
/* Here is what this function should accomplish */
/* - Return -1 if ptr is NULL */
/* - Return -1 if ptr is not pointing to the first byte of a busy block */
/* - Mark the block as free */
/* - Coalesce if one or both of the immediate neighbours are free */
int Mem_Free(void *ptr)
{
	
	if (ptr == NULL)
		return -1;

	// declaring variables
	block_header* temp = list_head;
	int h_size = (int)sizeof(block_header);

	// looks if first block is the one we're trying to free
	if ((int)temp + h_size == (int)ptr)
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
		return 0;
	}

	while (temp != NULL) {
		if ((temp->next != NULL) && ((int)(temp->next) + h_size == (int)ptr))
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
			return 0;
		}
		temp = temp->next;
	}
	return -1;
}

/* Function to be used for debug */
/* Prints out a list of all the blocks along with the followinginforormation for each block */
/* No.      : Serial number of the block */
/* Status   : free/busy */
/* Begin    : Address of the first useful byte in the block */
/* End      : Address of the last byte in the block */
/* Size     : Size of the block (excluding the header) */
/* t_Size   : Size of the block (including the header) */
/* t_Begin  : Address of the first byte in the block (this is where the header starts) */
void Mem_Dump()
{
  int counter;
  block_header* current = NULL;
  char* t_Begin = NULL;
  char* Begin = NULL;
  int Size;
  int t_Size;
  char* End = NULL;
  int free_size;
  int busy_size;
  int total_size;
  char status[5];

  free_size = 0;
  busy_size = 0;
  total_size = 0;
  current = list_head;
  counter = 1;
  fprintf(stdout,"************************************Block list***********************************\n");
  fprintf(stdout,"No.\tStatus\tBegin\t\tEnd\t\tSize\tt_Size\tt_Begin\n");
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  while(NULL != current)
  {
    t_Begin = (char*)current;
    Begin = t_Begin + (int)sizeof(block_header);
    Size = current->size_status;
    strcpy(status,"Free");
    if(Size & 1) /*LSB = 1 => busy block*/
    {
      strcpy(status,"Busy");
      Size = Size - 1; /*Minus one for ignoring status in busy block*/
      t_Size = Size + (int)sizeof(block_header);
      busy_size = busy_size + t_Size;
    }
    else
    {
      t_Size = Size + (int)sizeof(block_header);
      free_size = free_size + t_Size;
    }
    End = Begin + Size;
    fprintf(stdout,"%d\t%s\t0x%08lx\t0x%08lx\t%d\t%d\t0x%08lx\n",counter,status,(unsigned long int)Begin,(unsigned long int)End,Size,t_Size,(unsigned long int)t_Begin);
    total_size = total_size + t_Size;
    current = current->next;
    counter = counter + 1;
  }
  fprintf(stdout,"---------------------------------------------------------------------------------\n");
  fprintf(stdout,"*********************************************************************************\n");

  fprintf(stdout,"Total busy size = %d\n",busy_size);
  fprintf(stdout,"Total free size = %d\n",free_size);
  fprintf(stdout,"Total size = %d\n",busy_size+free_size);
  fprintf(stdout,"*********************************************************************************\n");
  fflush(stdout);
  return;
}
