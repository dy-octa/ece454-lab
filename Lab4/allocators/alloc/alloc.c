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
#define PUT(p, val)      (*(uintptr_t *)(p) = (val))//, fprintf(stderr, "[%x] PUT(%p, 0x%x)\n", pthread_self(), p, val))

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
#define LIST_CNT 5

/* Head nodes of segregated lists */
block* list_heads[LIST_CNT];
/* Block size constraint of each list */
const int list_size[LIST_CNT] = {16, 32, 48, 64, 80};
pthread_rwlock_t list_rwlock[LIST_CNT];
pthread_rwlock_t heap_rw_lock;

/* Minimum size each time heap break increases */
int chunksize;

/* Used for logging */
int cmd_cnt;
long long heap_starts;
int coalesce_counter;


/* Debug output helpers */
//#define DEBUG_MODE

#ifdef DEBUG_MODE

#define DEBUG(f, ...) (fprintf(stderr, (f), __VA_ARGS__))

#define RDLOCK(ptr) (fprintf(stderr, "[%x] attempt rdlock %s(%p) in %s\n", pthread_self(), ptr == &heap_rw_lock? "heap": "list", ptr, __FUNCTION__), \
pthread_rwlock_rdlock(ptr), fprintf(stderr, "[%x] rdlock %s(%p) in %s\n", pthread_self(), ptr == &heap_rw_lock? "heap": "list", ptr, __FUNCTION__))
#define WRLOCK(ptr) (fprintf(stderr, "[%x] attempt wrlock %s(%p) in %s\n", pthread_self(), ptr == &heap_rw_lock? "heap": "list", ptr, __FUNCTION__), \
pthread_rwlock_wrlock(ptr), fprintf(stderr, "[%x] wrlock %s(%p) in %s\n", pthread_self(), ptr == &heap_rw_lock? "heap": "list", ptr, __FUNCTION__))
#define UNLOCK(ptr) (pthread_rwlock_unlock(ptr), \
fprintf(stderr, "[%x] unlock %s(%p) in %s\n", pthread_self(), ptr == &heap_rw_lock? "heap": "list", ptr, __FUNCTION__))

//#define RUN_MM_CHECK

#else

#define DEBUG(f, ...) (0)

#define RDLOCK(ptr) (pthread_rwlock_rdlock(ptr))
#define WRLOCK(ptr) (pthread_rwlock_wrlock(ptr))
#define UNLOCK(ptr) (pthread_rwlock_unlock(ptr))

#endif


/**********************************************************
 * pthread_rwlock_promote
 * Promote a rwlock that is already rdlock-ed by the current thread to wrlock
 **********************************************************/
void pthread_rwlock_promote(pthread_rwlock_t* lock) {
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock(&mutex);
	UNLOCK(lock);
	WRLOCK(lock);
	pthread_mutex_unlock(&mutex);
}

/**********************************************************
 * list_insert
 * Insert the free block bp to the free address-ordered list list_no
 **********************************************************/
void list_insert(block* bp, int list_no) {
    block* pos = list_heads[list_no];
//    /* Find the appropriate position to insert the node */
//    while (pos->next && pos->next < bp)
//        pos = pos->next;
    if (pos -> next) {
        pos -> next -> prev = bp;
        bp -> next = pos -> next;
    }
    else bp -> next = NULL;
    pos -> next = bp;
    bp -> prev = pos;
#ifdef DEBUG_MODE
    DEBUG("[%x] Inserted %d(%p) to list[%d] ", pthread_self(), (int)((void*)bp - heap_starts), bp, list_no);
    if (bp->prev <= list_heads[LIST_CNT - 1])
        DEBUG("prev -> list[%d](%p) ", ((void*)bp->prev - (void*)list_heads[0]) / EMPTY_BLOCKSIZE, bp->prev);
    else DEBUG("prev -> %d(%p) ", (int)((void*)(bp->prev) - (void*)heap_starts), bp->prev);
    if (bp->next == NULL)
	DEBUG("next -> NULL\n", 0);
    else DEBUG("next -> %d(%p)\n", (int)((void*)(bp->next) - heap_starts), bp->next);
#endif
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
	DEBUG("[%x] PUT(%p, 0x%x)\n", pthread_self(), &bp->size, size);
    bp->size = size;
    SET_FOOTER(bp);
    for (i=search_from; i > 0; --i) {
        if (size >= list_size[i])
            break;
    }
	WRLOCK(&list_rwlock[i]);
    list_insert(bp, i);
	UNLOCK(&list_rwlock[i]);
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void) {
//	freopen ("mm.log", "w", stdout);
//	freopen ("/dev/tty", "a", stderr);
//	freopen("size.log", "a", stderr);
//	freopen("sizes.log", "w", stderr);
	DEBUG("Starting mm_init..\n", 0);
    mem_init();

    void* heap_listp;
    cmd_cnt = 0;
    // Initial chunk size
    chunksize = 8224;
    // Make up the space for list heads, prologue & epilogue
    if ((heap_listp = mem_sbrk(6 * WSIZE + LIST_CNT * EMPTY_BLOCKSIZE)) == (void *) -1)
        return -1;
    block* pt = heap_listp;
    // Set up an empty block as the head (sentinel) node of each segregated list
    for (int i=0; i < LIST_CNT; ++i) {
        pt -> prev = NULL;
        pt -> next = NULL;
        pt -> size = PACK(EMPTY_BLOCKSIZE, 0); // sentinel header
        SET_FOOTER(pt); // sentinel footer
        list_heads[i] = pt;
	    DEBUG("list #%d(size: %d): %p\n", i, list_size[i], list_heads[i]);
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
	DEBUG("Init: allocate %d bytes, %p -> %p\n", mem_usage(), heap_listp, MOVE(pt, WSIZE));
	pthread_rwlock_init(&heap_rw_lock, NULL);
    return 0;
}

/**********************************************************
 * coalesce
 * Centralized coalesce, scan throught the entire heap to coalesce all free blocks
 * Will acquire a global write lock, so it will block and be blocked by any other threads
 **********************************************************/
void coalesce() {
	block* start = dseg_lo + 5*WSIZE + LIST_CNT * EMPTY_BLOCKSIZE;
	WRLOCK(&heap_rw_lock);
	DEBUG("[%x] Enter coalesce\n", pthread_self());
	for (block* bp = start;  (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp)) {
		if (!GET_ALLOC(bp) && !GET_ALLOC(NEXT_BLKP(bp))) {
			DEBUG("[%x] Coalesce %d", pthread_self(), (int)((void*)bp - heap_starts));
			list_remove(bp);
			do {
				block* nbp = NEXT_BLKP(bp);
				DEBUG(", %d", (int)((void*)nbp - heap_starts));
				list_remove(nbp);
				DEBUG("[%x] PUT(%p, 0x%x)\n", pthread_self(), &bp->size, bp->size + NEXT_BLKP(bp) -> size);
				bp -> size += NEXT_BLKP(bp) -> size;
				SET_FOOTER(bp);
			} while (!GET_ALLOC(NEXT_BLKP(bp)));
			DEBUG("\n", 0);
			list_insert(bp, find_list(bp->size));
		}
	}
	UNLOCK(&heap_rw_lock);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
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
	DEBUG("[%x] PUT(%p, 0x%x)\n", pthread_self(), &bp->size, PACK(size, 0));
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
 * Postcondition: if list_no != LIST_CNT, the write lock is released
 **********************************************************/
ablock* place(block* bp, int asize, int free_size, int list_no, int des_direction) {
	static char rec_direction = 0;
	char direction = des_direction == -1? rec_direction : des_direction;
	DEBUG("[%x] allocate %d in block %d(%p) sized %d at list[%d], direction: %s\n",
	      pthread_self(), asize, (int)((void*)bp - heap_starts), bp, free_size, list_no, direction == 0? "LOW":"HIGH");
    if (free_size - asize <= EMPTY_BLOCKSIZE) // If the remaining space after the split could not hold a block
        asize = free_size;

	if (list_no == LIST_CNT)
		--list_no;
    else {
		list_remove(bp);
		UNLOCK(&list_rwlock[list_no]);
	}

    if (direction == 0) { // Split the block from the lower address
	    DEBUG("[%x] PUT(%p, 0x%x)\n", pthread_self(), &bp->size, PACK(asize, 1));
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
	    DEBUG("[%x] PUT(%p, 0x%x)\n", pthread_self(), &bp->size, PACK(asize, 1));
        bp -> size = PACK(asize, 1);
        if (free_size > asize)
            relocate_free_segment(rem, free_size - asize, list_no);
    }
    rec_direction ^= 1; // Direction changes next time
    return (ablock*)bp;
}

void* mm_free_thread(void* ptr) {
//	DEBUG("%x attempt to acquire lock in free\n", pthread_self());
//	pthread_mutex_lock(&malloc_lock);
//	DEBUG("%x acquired lock in free\n", pthread_self());
#ifdef RUN_MM_CHECK
	mm_check();
#endif
	RDLOCK(&heap_rw_lock);
	DEBUG("[%x] mm_free %d(%p) @ %d\n", pthread_self(),  (int)(ptr - heap_starts), ptr, ++cmd_cnt);
	block* bp = DATA2BLOCK((block *)ptr);
	DEBUG("[%x] PUT(%p, 0x%x)\n", pthread_self(), &bp->size, GET_SIZE(bp));
	bp -> size = GET_SIZE(bp);
	SET_FOOTER(bp);
	// prev and next of an allocated block should be overlapped by data
	bp->next = NULL;
	bp->prev = NULL;

	int list_no = find_list(bp->size);
	WRLOCK(&list_rwlock[list_no]);
	list_insert(bp, list_no);
	UNLOCK(&list_rwlock[list_no]);
	DEBUG("[%x] %d success\n", pthread_self(), (int)(ptr - heap_starts));
	UNLOCK(&heap_rw_lock);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
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
	mm_free_thread(ptr);
//	pthread_t th;
//	void* exit_status;
//	DEBUG("New free request: %d(%p)\n", (int)(ptr - heap_starts), ptr);
//	pthread_create(&th, NULL, mm_free_thread, ptr);
//	DEBUG("[%x] Free request %d(%p) handled by [%x]\n", pthread_self(), (int)(ptr - heap_starts), ptr, th);
//	pthread_join(th, &exit_status);
//	DEBUG("[%x] Joined in free request %d(%p) handled by [%x]\n", pthread_self(), (int)(ptr - heap_starts), ptr, th);
}

void* mm_malloc_thread(int asize) {
	size_t extendsize; /* amount to extend heap if no fit */
	block *bp;
	int list_no, n_list_no;
//	fprintf(stderr, "%d\n", asize);
	if (++coalesce_counter >= 20) {
		coalesce_counter = 0;
		coalesce();
	}
	DEBUG("[%x] mm_malloc %d @ %d\n", pthread_self(), asize, ++cmd_cnt);

	/* Find the appropriate list to start to search for a fit free block */
	list_no = find_list(asize);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
	RDLOCK(&heap_rw_lock);

	for (n_list_no = list_no; n_list_no < LIST_CNT; ++n_list_no) {
		/* Search the free list for a fit */
		WRLOCK(&list_rwlock[n_list_no]);
		if ((bp = find_fit(asize, list_heads[n_list_no])) != NULL) {
//			pthread_rwlock_promote(&list_rwlock[n_list_no]);
			void *ret = place(bp, asize, GET_SIZE(bp), n_list_no, -1)->data;
			DEBUG("[%x] mm_malloc %d -> %d(%p) success\n", pthread_self(), asize, (int) (ret - heap_starts), ret);
			UNLOCK(&heap_rw_lock);
#ifdef RUN_MM_CHECK
			mm_check();
#endif
			return ret;
		}
		UNLOCK(&list_rwlock[n_list_no]);
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
	UNLOCK(&heap_rw_lock);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
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
	return mm_malloc_thread(asize);
//	pthread_t th;
//	void* exit_status;
//	DEBUG("New malloc request: %d\n", asize);
//	pthread_create(&th, NULL,mm_malloc_thread, &asize);
//	DEBUG("[%x] Malloc request %d handled by [%x]\n", pthread_self(), asize, th);
//	pthread_join(th, &exit_status);
//	DEBUG("[%x] Joined in malloc request %d, return %p handled by [%x]\n", pthread_self(), th, size, exit_status, th);
//	return exit_status;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 * Precondition: Current thread is holding none of the locks
 *********************************************************/
int mm_check(void) {
    block* start = dseg_lo + 5*WSIZE + LIST_CNT * EMPTY_BLOCKSIZE;
    block *bp, *nbp;
	pthread_rwlock_wrlock(&heap_rw_lock);
    // Traverse all the free lists
    for (int i = 0; i < LIST_CNT; ++i)
        // Traverse the blocks in the free list
        for (nbp = list_heads[i] -> next; nbp != NULL; nbp = nbp -> next) {
            // Check if the block is free
            if (GET_ALLOC(nbp)) {
                fprintf(stderr, "[%x] Error: Block %d(%p) sized %d in free list %d is allocated\n", pthread_self(),
                        (int)((void*)nbp - (void*)start), nbp, (int)GET_SIZE(nbp), i);
				while (1);
                return 0;
            }
            // Check if the size of the block fits the list
            if ((i != 0 && GET_SIZE(nbp) < list_size[i]) || (i != LIST_CNT - 1 && GET_SIZE(nbp) >= list_size[i+1] )) {
	            fprintf(stderr, "[%x] Error: Block %d(%p) sized %d stored free list for size %d\n", pthread_self(),
	                    (int)((void*)nbp - (void*)start), nbp, (int)GET_SIZE(nbp), list_size[i]);
			while (1);
                return 0;
            }
            // Check if the block presents in the implicit list (linked in the heap by sizes)
            for (bp = start; (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp))
                if (bp == nbp)
                    break;
            if ((void*)bp > dseg_hi) {
                fprintf(stderr, "[%x] Error: Block %d(%p) sized %d in free list %d could not be found in contiguity list\n", pthread_self(),
                        (int)((void*)nbp - (void*)start), nbp, (int)GET_SIZE(nbp), i);
			while (1);
                return 0;
            }
        }
    // Check if the prologue is correct
    if (GET(MOVE(start, -WSIZE)) != PACK(EMPTY_BLOCKSIZE, 1)) {
        fprintf(stderr, "[%x] Error: Illegal prologue %p: %d\n", pthread_self(), MOVE(start, -WSIZE), (int)GET(MOVE(start, -WSIZE)));
		while (1);
    }
//	DEBUG("Start scanning from heap range %p, to %p\n", start, mem_heap_hi() - WSIZE);
    // Traverse through the heap
    for (bp = start; (void*)bp < dseg_hi - WSIZE; bp = NEXT_BLKP(bp)) {
        int size = GET_SIZE(bp);
//		DEBUG("(%p, %d)\n", bp, size);
        // Check if the block data is 16B-aligned
        if (((size_t) (bp -> data)) & 0xF) {
            fprintf(stderr, "[%x] Error: Block %d(%p), sized %d, alloc:%d not aligned to 16B\n",
                    pthread_self(), (int)((void*)bp - (void*)start), bp, (int)GET_SIZE(bp), GET_ALLOC(bp));
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
                fprintf(stderr, "[%x] Error: Block %d(%p), sized %d could not be found in list %d\n", pthread_self(),
                        (int)((void*)bp - (void*)start), bp, size, listno);
				while (1);
                return 0;
            }
        }
    }
    //Check if the epilogue is correct
    if (bp -> size != PACK(1, 1)) {
        fprintf(stderr, "[%x] Error: Illegal epilogue %p: %d\n", pthread_self(), bp, (int)bp->size);
	    while (1);
    }

    // Output the memory locations in the current heap
    DEBUG("[%x] Current heap:\n", pthread_self());
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
	DEBUG("[%x] Current list:\n", pthread_self());
	for (int i=0; i<LIST_CNT; ++i)
		if (list_heads[i] -> next != NULL) {
			DEBUG("list[%d](%d): ", i, list_size[i]);
			for (nbp = list_heads[i] -> next; nbp != NULL; nbp = nbp -> next)
				DEBUG("%d(%p)[%d], ", (int)((void*)nbp - heap_starts), nbp, nbp->size);
			DEBUG("\n", 0);
		}
	DEBUG("\n\n", 0);
	pthread_rwlock_unlock(&heap_rw_lock);
    return 1;
}
