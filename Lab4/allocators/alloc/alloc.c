#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <stdint.h>

#include "memlib.h"
#include "malloc.h"

name_t myname = {
     /* team name to be displayed on webpage */
     "Cheesecake",
     /* Full name of first team member */
     "Andrei Patranoiu",
     /* Email address of first team member */
     "andrei.patranoiu@mail.utoronto.ca",
     /* Student Number of first team member */
     "998130696"
     /* Full name of second team member */
     "Mutian He",
     /* Email address of second team member */
     "mutian.he@mail.utoronto.ca",
     /* Student Number of second team member */
     "1004654475"
};


/*************************************************************************
 * Basic Constants and Macro
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define QSIZE       (4 * WSIZE)            /* quadword size (bytes) */
#define MAXCHUNKSIZE   (32768)      /* maximal heap chunk size (bytes) */


#define MAX(x, y) ((x) > (y)?(x) :(y))
#define MIN(x, y) ((x) < (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p, val)      (*(uintptr_t *)(p) = (val))

/* Move a pointer pt by offset bytes */
#define MOVE(pt, offset) (((void*)(pt)) + (offset))

/* Align the size */
#define ALIGN_16B(size) (((size)&15LL)?(((size)|15LL)+1):(size))
#define ALIGN_4K(size) (((size)&4095LL)?(((size)|4095LL)+1):(size))

/* Free block structure */
typedef struct block_st{
    size_t size;
    struct block_st* prev;
    struct block_st* next;
    char data[0];
} block;

/* Allocated block structure */
typedef struct ablock_st{
    size_t size;
    char data[0];
} ablock;
// Footer can not be accessed directly without using the macro below

/* Size of an empty free block */
#define EMPTY_BLOCKSIZE  (QSIZE)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     ((size_t)((((block*)(p)) -> size) & ~(DSIZE - 1)))
#define GET_ALLOC(p)    ((int)((((block*)(p)) -> size) & 0x1))

/* Size of payload of an allocated block p */
#define GET_DATASIZE(abp)     (GET_SIZE(abp) - DSIZE)

/* Given block ptr bp, compute address of its header and footer */
#define FTRP(bp)        ((size_t*)(MOVE(bp, GET_SIZE(bp) - WSIZE)))

/* Given pointer to data of an allocated block, return a pointer to the block */
#define DATA2BLOCK(ptr) ((ablock*)(MOVE(ptr, -sizeof(ablock))))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((block*)MOVE(bp, GET_SIZE(bp)))
#define PREV_BLKP(bp) ((block*)MOVE(bp, -GET_SIZE(MOVE(bp, -WSIZE))))

/* Set the footer of a block pointer pt */
#define SET_FOOTER(pt) ({PUT(FTRP(pt), ((block*) (pt)) -> size);})

/* Last block in the heap */
#define LAST_BLOCK (PREV_BLKP(dseg_hi + 1 - WSIZE))

/* Number of segregated lists */
#define LIST_CNT 19

/* Debug output helpers */
#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG(f, ...) (fprintf(stderr, (f), __VA_ARGS__))
#define RUN_MM_CHECK
#else
#define DEBUG(f, ...) (0)
#endif

/* Head nodes of segregated lists */
block* list_heads[LIST_CNT];
/* Block size constraint of each list */
const int list_size[LIST_CNT] = {23, 32, 80, 88, 128, 144, 176, 464, 528, 1734, 4088, 4111, 5573, 8206, 11152, 15472, 19528, 23961, 28437};
pthread_rwlock_t list_rwlock[LIST_CNT];
pthread_rwlock_t heap_rw_lock;

/* Minimum size each time heap break increases */
int chunksize;

/* Used for logging */
int cmd_cnt;
int heap_starts;
int coalesce_counter;

/**********************************************************
 * pthread_rwlock_promote
 * Promote a rwlock that is already rdlock-ed by the current thread to wrlock
 **********************************************************/
void pthread_rwlock_promote(pthread_rwlock_t* lock) {
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);
	pthread_rwlock_unlock(lock);
	pthread_rwlock_wrlock(lock);
	pthread_mutex_unlock(&mutex);
}

/**********************************************************
 * list_insert
 * Insert the free block bp to the free address-ordered list list_no
 **********************************************************/
void list_insert(block* bp, int list_no) {
    block* pos = list_heads[list_no];
    /* Find the appropriate position to insert the node */
    while (pos->next && pos->next < bp)
        pos = pos->next;
    if (pos -> next) {
        pos -> next -> prev = bp;
        bp -> next = pos -> next;
    }
    else bp -> next = NULL;
    pos -> next = bp;
    bp -> prev = pos;
    DEBUG("[%x] Inserted %d(%p) to list[%d] ", pthread_self(), (int)((void*)bp - heap_starts), bp, list_no);
    if ((void*)bp->prev < heap_starts)
        DEBUG("prev -> head[%d](%p) ", bp->prev - list_heads[0], bp->prev);
    else DEBUG("prev -> %d(%p) ", (int)((void*)(bp->prev) - heap_starts), bp->prev);
    if (bp->next == NULL)
	DEBUG("next -> NULL\n", 0);
    else DEBUG("next -> %d(%p)\n", (int)((void*)(bp->next) - heap_starts), bp->next);
}

/**********************************************************
 * list_remove
 * Remove the block bp from free list
 **********************************************************/
void list_remove(block* bp) {
    if (bp -> prev)
        bp -> prev -> next = bp -> next;
    if (bp -> next)
        bp -> next -> prev = bp -> prev;
    bp -> prev = bp -> next = NULL;
}

/**********************************************************
 * find_list
 * Find the appropriate list for a block sized asize
 **********************************************************/
int find_list(size_t asize) {
    int i;
    /* List #0 also stores the blocks < list_size[0] */
    if (asize <= list_size[0])
        return 0;
    for (i=LIST_CNT - 1; i>=0; i--)
        if (asize >= list_size[i])
            return i;
    return LIST_CNT - 1;
}

/**********************************************************
 * relocate_free_segment
 * Build up a free block from a free segment at bp sized size
 * Assume that the size <= list_size[search_from]
 * search_from is used to accelerate the search if some extra
 * information is known.
 **********************************************************/
void relocate_free_segment(block* bp, size_t size, int search_from) {
    int i;
    bp->size = size;
    SET_FOOTER(bp);
    for (i=search_from; i > 0; --i) {
        if (size >= list_size[i])
            break;
    }
	pthread_rwlock_wrlock(&list_rwlock[i]);
    list_insert(bp, i);
	pthread_rwlock_unlock(&list_rwlock[i]);
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void) {
//	freopen ("mm.log", "w", stdout);
	freopen ("/dev/tty", "a", stderr);
//	freopen("size.log", "a", stderr);
	DEBUG("Starting mm_init..\n", 0);
    mem_init();

    void* heap_listp;
    cmd_cnt = 0;
    // Initial chunk size
    chunksize = 8224;
    // Make up the space for list heads, prologue & epilogue
    if ((heap_listp = mem_sbrk(6 * WSIZE + LIST_CNT * EMPTY_BLOCKSIZE)) == (void *) -1)
        return -1;
    DEBUG("Init: allocate %d bytes, %p -> ", mem_usage(), heap_listp);
    block* pt = heap_listp;
    // Set up an empty block as the head (sentinel) node of each segregated list
    for (int i=0; i < LIST_CNT; ++i) {
        pt -> prev = NULL;
        pt -> next = NULL;
        pt -> size = PACK(EMPTY_BLOCKSIZE, 0); // sentinel header
        SET_FOOTER(pt); // sentinel footer
        list_heads[i] = pt;
	    pthread_rwlock_init(&list_rwlock[i], NULL);
        pt = MOVE(pt, EMPTY_BLOCKSIZE);
    }
    pt = MOVE(pt, WSIZE);
    pt -> prev = NULL;
    pt -> next = NULL;
    pt -> size = PACK(EMPTY_BLOCKSIZE, 1); // prologue header
    SET_FOOTER(pt); // prologue footer
    heap_starts = pt = MOVE(pt, EMPTY_BLOCKSIZE);
    pt -> size = PACK(1, 1); // epilogue header
//    DEBUG("%p\n", MOVE(pt, WSIZE));
	pthread_rwlock_init(&heap_rw_lock, NULL);
    DEBUG("Exiting mm_init..\n", 0);
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
void coalesce() {
	pthread_rwlock_wrlock(&heap_rw_lock);
	for (block* bp = heap_starts;  (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(bp) && !GET_ALLOC(NEXT_BLKP(bp))) {
			list_remove(bp);
			do {
				block* nbp = NEXT_BLKP(bp);
				list_remove(nbp);
				bp -> size += NEXT_BLKP(nbp) -> size;
				SET_FOOTER(bp);
			} while (!GET_ALLOC(NEXT_BLKP(bp)));
			list_insert(bp, find_list(bp->size));
		}
	}
#ifdef RUN_MM_CHECK
    mm_check();
#endif
	pthread_rwlock_unlock(&heap_rw_lock);
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header.
 * Return a stray free block.
 **********************************************************/
block* extend_heap(size_t words) {
    block *bp;
    size_t size;
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	pthread_mutex_lock(&mutex);
    if ((bp = mem_sbrk(size)) == (void *) -1) {
	    pthread_mutex_unlock(&mutex);
	    return NULL;
    }

	bp = MOVE(bp, -WSIZE); // Remove old epilogue header

	/* Initialize free block header/footer and the epilogue header */
	bp -> size = PACK(size, 0);                  // free block header
	SET_FOOTER(bp);               // free block footer

	NEXT_BLKP(bp) -> size = PACK(1, 1); // Set new epilogue footer

	pthread_mutex_unlock(&mutex);

    return bp;
}


/**********************************************************
 * find_fit
 * Traverse the free list listp searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
block *find_fit(size_t asize, block* listp) {
#ifdef BEST_FIT
    block *bp, *ret = NULL;
	int sized;
	/* Starting from the first "real" block in the list */
	for (bp = listp -> next; bp != NULL; bp = bp -> next) {
		if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp))) {
			if (ret == NULL || GET_SIZE(bp) - asize < sized) {
				sized = GET_SIZE(bp) - asize;
				ret = bp;
			}
		}
	}
	return ret;
#else
    // use first fit method as default
    block *bp;
    /* Starting from the first "real" block in the list */
    for (bp = listp -> next; bp != NULL; bp = bp -> next) {
        if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp)))
            return bp;
    }
    return NULL;
#endif
}

/**********************************************************
 * place
 * Split a block sized asize from block bp with size free_size, located on list# <= list_no
 * des_direction: specify whether split the upper part(1) or lower part(0) of the block
 * -1: split in alternative ways
 * Precondition: if list_no == LIST_CNT, bp was not in any list
 * otherwise bp was in list[list_no] and the thread is holding a write lock of list[list_no]
 **********************************************************/
ablock* place(block* bp, int asize, int free_size, int list_no, int des_direction) {
    if (free_size - asize <= EMPTY_BLOCKSIZE) // If the remaining space after the split could not hold a block
        asize = free_size;

	if (list_no == LIST_CNT)
		--list_no;
    else {
		list_remove(bp);
		pthread_rwlock_unlock(&list_rwlock[list_no]);
	}

    static char rec_direction = 0;
    char direction = des_direction == -1? rec_direction : des_direction;
    if (direction == 0) { // Split the block from the lower address
        bp -> size = PACK(asize, 1);
        SET_FOOTER(bp);
        if (free_size > asize) {
            block* rem = NEXT_BLKP(bp);
            relocate_free_segment(rem, free_size - asize, list_no);
        }
    }
    else { // Split the block from the upper address
        block* rem = bp;
        PUT(MOVE(NEXT_BLKP(bp), -WSIZE), PACK(asize, 1));
        bp = PREV_BLKP(NEXT_BLKP(bp));
        bp -> size = PACK(asize, 1);
        if (free_size > asize)
            relocate_free_segment(rem, free_size - asize, list_no);
    }
    rec_direction ^= 1; // Direction changes next time
    return (ablock*)bp;
}

void* mm_free_thread(void* args) {
	void* ptr = args;
//	DEBUG("%x attempt to acquire lock in free\n", pthread_self());
//	pthread_mutex_lock(&malloc_lock);
//	DEBUG("%x acquired lock in free\n", pthread_self());
	pthread_rwlock_rdlock(&heap_rw_lock);
    	DEBUG("[%x] mm_free %d(%p) @ %d\n", pthread_self(),  (int)(ptr - heap_starts), ptr, ++cmd_cnt);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
	block* bp = DATA2BLOCK((block *)ptr);
	bp -> size = GET_SIZE(bp);
	SET_FOOTER(bp);
	// prev and next of an allocated block should be overlapped by data
	bp->next = NULL;
	bp->prev = NULL;

	int list_no = find_list(bp->size);
	pthread_rwlock_wrlock(&list_rwlock[list_no]);
	list_insert(bp, list_no);
	pthread_rwlock_unlock(&list_rwlock[list_no]);
	DEBUG("[%x] %d success\n", pthread_self(), (int)(ptr - heap_starts));
#ifdef RUN_MM_CHECK
    mm_check();
#endif
	pthread_rwlock_unlock(&heap_rw_lock);
//	pthread_mutex_unlock(&malloc_lock);
//	DEBUG("%x released lock in free\n", pthread_self());
	return NULL;
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
	pthread_t th;
	void* exit_status;
	DEBUG("New free request: %d(%p)\n", (int)(ptr - heap_starts), ptr);
	pthread_create(&th, NULL, mm_free_thread, ptr);
	pthread_join(th, &exit_status);
}

void* mm_malloc_thread(void* args) {
	size_t extendsize; /* amount to extend heap if no fit */
	block *bp;
	int list_no, n_list_no;
	size_t asize = *((size_t*)args); /* adjusted block size */

	if (++coalesce_counter >= 20) {
		coalesce_counter = 0;
		coalesce();
	}
//	DEBUG("%x attempt to acquire lock in malloc\n", pthread_self());
//	pthread_mutex_lock(&malloc_lock);
//	DEBUG("%x acquired lock in malloc\n", pthread_self());
#ifdef RUN_MM_CHECK
    mm_check();
#endif
    DEBUG("[%x] mm_malloc %d @ %d -> ", pthread_self(), asize, ++cmd_cnt);

	/* Find the appropriate list to start to search for a fit free block */
	list_no = find_list(asize);
	pthread_rwlock_rdlock(&heap_rw_lock);
	for (n_list_no = list_no; n_list_no < LIST_CNT; ++n_list_no)
		/* Search the free list for a fit */
		pthread_rwlock_rdlock(&list_rwlock[n_list_no]);
		if ((bp = find_fit(asize, list_heads[n_list_no])) != NULL) {
			pthread_rwlock_promote(&list_rwlock[n_list_no]);
			void* ret = place(bp, asize, GET_SIZE(bp), n_list_no, -1)->data;
			DEBUG("%d(%p)\n", (int)(ret - heap_starts), ret);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
//			pthread_mutex_unlock(&malloc_lock);
			pthread_rwlock_unlock(&heap_rw_lock);
			return ret;
		}

	/* No fit found. Get more memory and place the block */
	// Adjust the chunk size adaptively
	if (asize > chunksize)
		chunksize = MIN(MAXCHUNKSIZE, chunksize * 2);

	extendsize = MAX(asize, chunksize);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {
		DEBUG("failed!!!\n", 0);
		return NULL;
	}
	void* ret = place(bp, asize, GET_SIZE(bp), LIST_CNT, -1) -> data;
	DEBUG("%d(%p)\n", (int)(ret - heap_starts), ret);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
//	pthread_mutex_unlock(&malloc_lock);
//	DEBUG("%x released lock in malloc\n", pthread_self());
	pthread_rwlock_unlock(&heap_rw_lock);
	return ret;
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(...)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size) {
	/* Ignore spurious requests */
	if (size == 0)
		return NULL;
    /* Adjust block size to include overhead and alignment reqs. */
    size_t asize = ALIGN_16B(size + DSIZE);
	pthread_t th;
	void* exit_status;
	DEBUG("New malloc request: %d\n", size);
	pthread_create(&th, NULL,mm_malloc_thread, &asize);
	pthread_join(th, &exit_status);
	return exit_status;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void) {
    block* start = dseg_lo + 5*WSIZE + LIST_CNT * EMPTY_BLOCKSIZE;
    block *bp, *nbp;
    // Traverse all the free lists
    for (int i = 0; i < LIST_CNT; ++i)
        // Traverse the blocks in the free list
        for (nbp = list_heads[i] -> next; nbp != NULL; nbp = nbp -> next) {
            // Check if the block is free
            if (GET_ALLOC(nbp)) {
                fprintf(stderr, "Error: Block %p sized %d in free list %d is allocated\n", nbp, (int)GET_SIZE(nbp), i);
		while (1);
                return 0;
            }
            // Check if the size of the block fits the list
            if ((i != 0 && GET_SIZE(nbp) < list_size[i]) || (i != LIST_CNT - 1 && GET_SIZE(nbp) >= list_size[i+1] )) {
                fprintf(stderr, "Error: Block %p sized %d stored free list for size %d\n", nbp, (int)GET_SIZE(nbp), list_size[i]);
		while (1);
                return 0;
            }
            // Check if the block presents in the implicit list (linked in the heap by sizes)
            for (bp = start; (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp))
                if (bp == nbp)
                    break;
            if ((void*)bp > dseg_hi) {
                fprintf(stderr, "Error: Block %p sized %d in free list %d could not be found in contiguity list\n", nbp, (int)GET_SIZE(nbp), i);
		while (1);
                return 0;
            }
        }
    // Check if the prologue is correct
    if (GET(MOVE(start, -WSIZE)) != PACK(EMPTY_BLOCKSIZE, 1)) {
        fprintf(stderr, "Error: Illegal prologue %p: %d\n", MOVE(start, -WSIZE), (int)GET(MOVE(start, -WSIZE)));
	while (1);
    }
//	DEBUG("Start scanning from heap range %p, to %p\n", start, mem_heap_hi() - WSIZE);
    // Traverse through the heap
    for (bp = start; (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp)) {
        int size = GET_SIZE(bp);
//		DEBUG("(%p, %d)\n", bp, size);
        // Check if the block data is 16B-aligned
        if (((size_t) (bp -> data)) & 0xF) {
            fprintf(stderr, "Error: Block %p, sized %d, alloc:%d not aligned to 16B\n", bp, (int)GET_SIZE(bp), GET_ALLOC(bp));
	    while (1);
            return 0;
        }
        // For a free block, check if it can be found in free list
        if (size > 0 && !GET_ALLOC(bp)) {
            int listno = find_list(size);
            for (nbp = list_heads[listno]; nbp != NULL; nbp = nbp -> next)
                if (nbp == bp)
                    break ;
            if (nbp == NULL) {
                fprintf(stderr, "Error: Block %p, sized %d could not be found in list %d\n", bp, size, listno);
		while (1);
                return 0;
            }
        }
    }
    //Check if the epilogue is correct
    if (bp -> size != PACK(1, 1)) {
        fprintf(stderr, "Error: Illegal epilogue %p: %d\n", bp, (int)bp->size);
	while (1);
    }

    // Output the memory locations in the current heap
    DEBUG("Current heap in %x:\n", pthread_self());
    int acc_addr = 0;
    for (bp = start;  (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp))
        DEBUG("\t%d%c\t|", (int)GET_DATASIZE(bp), GET_ALLOC(bp)?'a':'f');
    DEBUG("\n", 0);
    // Output the memory offsets of the block boundaries
    for (bp = start;  (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp)) {
        acc_addr += GET_SIZE(bp);
        DEBUG("\t\t%d", acc_addr);
    }
    DEBUG("\n\n", 0);
    return 1;
}
