/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */

/* Segregated free list: size of lists?
 * First fit for throughput vs best fit for utilization?
 * Coalesce on free or deferred coalescing?
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Memory? I can't remember her",
    /* First member's full name */
    "Chinmayee Gidwani",
    /* First member's email address */
    "chinmayee.gidwani@mail.utoronto.ca",
    /* Second member's full name (do not modify this as this is an individual lab) */
    "",
    /* Second member's email address (do not modify this as this is an individual lab)*/
    ""
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1)) // size of payload from header
#define GET_ALLOC(p)    (GET(p) & 0x1) // whether it's allocated or not

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

void* heap_listp = NULL;

/* Specify size_t possible sizes for segregated free list */
size_t free_list_sizes[14] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144};


/* Use doubly linked list for blocks */
typedef struct linked_list{
    struct linked_list* prev;
    struct linked_list* next;
} node;

/* Contains array of pointers to the head of each seg. free list by size*/
node* free_lists[14];


/* Function declarations */
int get_index(int size);
void free_list_remove(node* remove_block, int index);
void free_list_add(void *bp);

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
   if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);                         // alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
     heap_listp += DSIZE;

     for(int i=0; i<14; i++){
         free_lists[i] = NULL;
     }

     return 0;
 }

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the free list searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    int index = get_index(asize);
    while(index < 15){
        node *bp = free_lists[index];
        while(bp != NULL){
            int bsize = GET_SIZE(HDRP(bp));
            if(bsize >= asize){
                free_list_remove(bp, index);
                return (void *) bp;
                // split remaining block here
            }
            bp = bp->next;
        }


        index++;
    }

    return NULL;




    /*
    int i = get_index(asize);
    //debug print here
    int block_size;
    void* bp;
    printf("in find_fit");
    while(i < 14){
        printf("while loop, index %d\n", i);
        node *current = free_lists[i];
        //if(current != NULL){
            //bp = (void*) current; //return pointer
            while(current != NULL){
                bp = (void*) current;
                // traverse each free list for block
                if(asize <= GET_SIZE(HDRP(bp))){
                    // block fit found
                    printf("asize %d || ", asize);
                    printf("bsize %d\n", GET_SIZE(HDRP(bp)));
                    //bp = (void*) current; //current might have changed, update bp
                    free_list_remove(current, i);
                    return bp;
                }
                current = current -> next;
            }
        //}
        i++;
    }

    return NULL; */
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
  //printf("in place \n");
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }
    //printf("in mm_free");
    size_t size = GET_SIZE(HDRP(bp));
    printf("Freeing size: %d\n", size);
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    free_list_add(coalesce(bp)); //coalesce and add to free list
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    //printf("starting program...\n");
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    printf("The asize is: %d\n",asize);
    //printf("in mm_malloc\n");
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        //printf("fit found, now placing\n");
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}



int get_index(int size){
    int index = 0;
    size_t seg_size = 32; //smallest block available, anything bigger will also use block size 32
    // in get index
    while(size > seg_size && index < 15){
        // in loop
        seg_size *= 2; // keep multiplying by two until big enough block size found
        index++;
    }
    /* TODO: what happens if bigger block size than index 14 is requested?*/
    return index;
}

/* Add block bp to the free list whose range matches
 * Add to the head of the free list
 * Return index in free_lists that the block was inserted into
 */
void free_list_add(void *bp){
    /* find appropriate free list given size
     * If free_lists[index] is null (ie no block of that size class has been
     * added to list yet), add block and update lists and heads.
     * If it is not null, add bp to head of the appropriatefree list.
     */
    size_t payload_size = GET_SIZE(HDRP(bp)); //includes header + footer (?)
    int index = 0;

    /* create new node for free list */
    node *new_free_block = (node*)bp;
    new_free_block -> prev = NULL;
    new_free_block -> next = NULL;

    index = get_index(payload_size);
    printf("payload size %d || ", payload_size);
    printf("index %d\n", index);

    if(free_lists[index] == NULL){
        /* nothing in free_list[index], insert one new block */
        free_lists[index] = new_free_block;
    } else {
        /* list not empty, insert new block at the head */
        free_lists[index] -> prev = new_free_block;
        new_free_block -> next = free_lists[index];
        free_lists[index] = new_free_block;
    }

    //return index;

}

/* Remove block bp from free list 
 * Update head of free list
 */
void free_list_remove(node* remove_block, int index){
    /* create remove node for free list */
    //node *remove_block = (node*)bp;
    //int index = get_index(GET_SIZE(HDRP(remove_block)));

    if(remove_block->prev == NULL && remove_block->next == NULL){
        // There is only one block in the list
        free_lists[index] = NULL;
    } else if(free_lists[index] == remove_block){
        // r_m is at the head of the list
        free_lists[index] = remove_block->next;
        free_lists[index]->prev = NULL;
    } else if(remove_block->prev != NULL && remove_block -> next == NULL){
        // block is the tail of the list
        remove_block->prev->next = NULL;
    } else{
        // Not only block, so link up adjacent blocks
        remove_block->next->prev = remove_block->prev;
        remove_block->prev->next = remove_block->next;
    }
    remove_block -> next = NULL;
    remove_block -> prev = NULL;

}


/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
//print out heap
	void* heap_start = heap_listp;
	
	printf("START OF HEAP \n");
	while(GET_SIZE(HDRP(heap_start)) != 0){
		int curr_alloc = GET_ALLOC(HDRP(heap_start));
		
		printf("Address: 0x:%x tSize: %d Allocated: %d\n",heap_start, GET_SIZE(HDRP(heap_start)), curr_alloc);
		
		heap_start = NEXT_BLKP(heap_start);
		
		//check for any blocks that escape coalescing
		if(curr_alloc == 0 && GET_ALLOC(HDRP(heap_start)) == 0){
			printf("block escaped coalescing, but this could be fine if this was called before coalesce\n");
		}
		
		//check for overlap between any blocks
		if(FTRP(heap_start) > HDRP(NEXT_BLKP(heap_start)) ){
			printf("THERE IS BLOCK OVERLAP at: %x\n",heap_start);
			return 0;
		}
	}
	printf("END OF HEAP \n");
	
	printf("START OF SEG LIST\n");
	//print out free list
	//and check to see if each block is free. 
	for (int i = 0; i < 14; i++){
		node* traverse = free_lists[i];
		printf("hash value: %d\n",i);
		while(traverse != NULL){
			
			int free_bit = GET_ALLOC(HDRP(traverse));
			
			printf("Address: 0x:%x tSize: %d Allocated: %d\n",traverse, GET_SIZE(HDRP(traverse)), free_bit);
			
			if(free_bit!=0){
				printf("free bit isn't 0. This could be fine depending on where mm_check() is called.\n");
			}
			
			traverse = traverse->next;
		}
		
	}
	
	
	printf("END OF SEG LIST\n");
  return 1;
}
