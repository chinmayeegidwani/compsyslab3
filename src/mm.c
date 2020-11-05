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

#define NUM_LISTS 15

void* heap_listp = NULL;

/* Specify size_t possible sizes for segregated free list */
//size_t free_list_sizes[14] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144};


/* Use doubly linked list for blocks */
typedef struct node{
    struct node* prev;
    struct node* next;
} node;

/* Contains array of pointers to the head of each seg. free list by size*/
node* free_lists[NUM_LISTS];


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

     for(int i=0; i<NUM_LISTS; i++){
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
    int index;

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
    // merge with block ahead
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //size of next block
        index = get_index(GET_SIZE(HDRP(NEXT_BLKP(bp)))); // index of next block
        node* next_block = (node*) NEXT_BLKP(bp);
        free_list_remove(next_block, index); // remove next block from free list

        PUT(HDRP(bp), PACK(size, 0)); //set allocated bits in hdr and ftr
        PUT(FTRP(bp), PACK(size, 0)); //update size in bp hdr and ftr
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
    // merge with block behind
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        index = get_index(GET_SIZE(HDRP(PREV_BLKP(bp))));
        node* next_block = (node*) PREV_BLKP(bp); //40
        free_list_remove(next_block, index);

        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        int prev_index = get_index(GET_SIZE(HDRP(PREV_BLKP(bp))));
        int next_index = get_index(GET_SIZE(HDRP(NEXT_BLKP(bp))));
        node* prev_block = (node*) PREV_BLKP(bp);
        node* next_block = (node*) NEXT_BLKP(bp);
        free_list_remove(prev_block,prev_index);
        free_list_remove(next_block, next_index);

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
    int bsize, remainder;
    while(index < NUM_LISTS){
    		node* bp = free_lists[index];
            while (bp != NULL) {
                bsize = GET_SIZE(HDRP(bp));
                remainder = bsize - asize;
                if (bsize >= asize) { //block fits
                    if (remainder < 2*DSIZE) {
                        // block cannot be split further
                        free_list_remove(bp, index);
                        return (void*) bp;
                    }else{
                        // block can be split further
                        free_list_remove(bp, index);
                        void* remainder_bp = (void*)bp + asize;

                        // updating bp to be asize big
                        PUT(HDRP(bp), PACK(asize,0));
                        PUT(FTRP(bp), PACK(asize,0));
                        // updating remainder block, add to free list
                        PUT(HDRP(remainder_bp), PACK(remainder,0));
                        PUT(FTRP(remainder_bp), PACK(remainder,0));

                        free_list_add(remainder_bp);

                        return (void*) bp;
                }
               
            }
             bp = bp->next;
            }
    	index++;
    }
    
    return NULL;
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
    //printf("Freeing size: %d\n", size);
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    bp = coalesce(bp);

    free_list_add(bp); //coalesce and add to free list

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
    //printf("The asize is: %d\n",asize);
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
    void *anotherptr;
    size_t copySize;
    size_t asize, bsize, remainder;
    size_t old_size;

    old_size = GET_SIZE(HDRP(oldptr));
    copySize = old_size;
    if (size < old_size)
        copySize = size;

    /* Adjust block size to include overhead and alignment reqs. */
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if (asize > old_size) { //expand

        PUT(HDRP(ptr),PACK(old_size,0));
        PUT(FTRP(ptr),PACK(old_size,0));
        void* new_bp = coalesce(ptr);
    	bsize = GET_SIZE(HDRP(new_bp));
    	remainder = bsize - asize;
    	if (bsize >= asize) { // if coalescing fits
    		memmove(new_bp, ptr, old_size - DSIZE);

        	PUT(HDRP(new_bp),PACK(bsize,1));
        	PUT(FTRP(new_bp),PACK(bsize,1));

            return new_bp;
    		
    	} else { //coalescing does not fit
            // malloc over as default
            anotherptr = mm_malloc(asize);
            if (anotherptr == NULL)
            return NULL;
            memmove(anotherptr, oldptr, copySize);
            mm_free(oldptr);
            return anotherptr;
        }
    } else { //shrink
    	remainder = old_size - asize;
    	
            if (remainder < 2*DSIZE) {
                // block cannot be split further
                //free_list_remove(bp, index);
                return ptr;
            }else{
                // block can be split further
                //free_list_remove(bp, index);
                void* remainder_bp = ptr + asize;

                // updating remainder block, add to free list
                PUT(HDRP(remainder_bp), PACK(remainder,0));
                PUT(FTRP(remainder_bp), PACK(remainder,0));

                free_list_add(remainder_bp);

                // updating bp to be asize big
                PUT(HDRP(ptr), PACK(asize,1));
                PUT(FTRP(ptr), PACK(asize,1));


                return ptr;
            }
    }
    
    return NULL;
}

int get_index(int size){
	int index = 0;
	size_t seg_size = 32;
	
	//keep shifting val to the left until it's bigger than size
	while(seg_size < size && index < (NUM_LISTS -1)) {
		seg_size *= 2;
		index++;
	}

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
    //printf("payload: %d \n\n", payload_size);
    int index = 0;

    /* create new node for free list */
    node *new_free_block = (node*)bp;
    new_free_block -> prev = NULL;
    new_free_block -> next = NULL;

    index = get_index(payload_size);
    //printf("payload size %d || ", payload_size);
    //printf("index %d\n", index);

    if(free_lists[index] == NULL){
        /* nothing in free_list[index], insert one new block */
        free_lists[index] = new_free_block;
    } else {
        /* list not empty, insert new block at the head */
        free_lists[index] -> prev = new_free_block;
        free_lists[index] -> prev -> next = free_lists[index];
        free_lists[index] = new_free_block;
    }

    //return index;
    //mm_check();
}

/* Remove block bp from free list 
 * Update head of free list
 */
void free_list_remove(node* remove_block, int index){
    /* create remove node for free list */
    //node *remove_block = (node*)bp;
    //int index = get_index(GET_SIZE(HDRP(remove_block)));

    //printf("index: %d\n\n", index);
    //printf("remove block: %d\n\n", remove_block);
    //mm_check();


    if(remove_block->prev == NULL && remove_block->next == NULL){
        // There is only one block in the list
        free_lists[index] = NULL;
    } else if(remove_block->prev == NULL && remove_block -> next != NULL){
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
  return 1;
}
