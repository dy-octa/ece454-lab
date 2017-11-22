#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
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
#define PAGESIZE (4096)

#define MAX_THREAD 200 // Max number of thread supported

#define MAX(x, y) ((x) > (y)?(x) :(y))
#define MIN(x, y) ((x) < (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size_t)((size) | (alloc)))

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

typedef struct superblock_ {
	struct superblock_ *next;
	pthread_mutex_t lock; // 40B
	block* head; // Head of explicit list of free blocks
	ablock prologue;
	size_t prologue_footer;
	char data[4016]; // 4K minus size of other metadata
	size_t epilogue[1]; // 8B remained, used as epilogue
	//Remember to change the size of data and padding when the structure of metadata changed
} superblock;

typedef struct global_header_ {
	pthread_t pthread_id[MAX_THREAD];
	superblock* ptr[MAX_THREAD];
	int pthread_cnt;
} global_header;

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

/* #Superblock of a ptr */
#define SUPERBLOCK_NO(ptr) (((int)((void*)ptr - (void*)superblocks)) / PAGESIZE)

/* Pointer to the superblock of a ptr */
#define SUPERBLOCK_OF(ptr) (&superblocks[SUPERBLOCK_NO(ptr)])

/* Pointer to the data of the superblock of ptr */
#define SUPERBLOCK_DATA(ptr) ((void*)(SUPERBLOCK_OF(ptr) -> data))

/* Offset of a pointer in the payload of a superblock */
#define BLOCK_OFFSET(ptr) ((int)((void*)ptr - (void*)SUPERBLOCK_DATA(ptr)))

/* Used for logging */
int cmd_cnt;

/* Global heap rwlock */
pthread_rwlock_t heap_rw_lock;

/* Global meta data and its lock */
global_header *global_metadata;
pthread_rwlock_t global_metadata_rwlock;

/* Pointer to the start of superblocks, should be equal to lowest address of heap */
superblock* superblocks;

/* Debug output helpers */
//#define DEBUG_MODE

#ifdef DEBUG_MODE

#define DEBUG(f, ...) (fprintf(stderr, (f), __VA_ARGS__))

//#define RUN_MM_CHECK

#else

#define DEBUG(f, ...) (0)

#endif

#ifdef LOCK_LOG

#define LOCK(ptr) (fprintf(stderr, "[%x] attempt lock %d(%p) in %s\n", (unsigned)pthread_self(), SUPERBLOCK_NO(ptr), ptr, __FUNCTION__), \
pthread_mutex_lock(ptr), fprintf(stderr, "[%x] attempt lock %d(%p) in %s\n", (unsigned)pthread_self(), SUPERBLOCK_NO(ptr), ptr, __FUNCTION__))
#define UNLOCK(ptr) (fprintf(stderr, "[%x] attempt unlock %d(%p) in %s\n", (unsigned)pthread_self(), SUPERBLOCK_NO(ptr), ptr, __FUNCTION__), \
pthread_mutex_unlock(ptr), fprintf(stderr, "[%x] attempt unlock %d(%p) in %s\n", (unsigned)pthread_self(), SUPERBLOCK_NO(ptr), ptr, __FUNCTION__))

#define RDLOCK(ptr) (fprintf(stderr, "[%x] attempt rdlock %p in %s\n", (unsigned)pthread_self(), ptr, __FUNCTION__), \
pthread_rwlock_rdlock(ptr), fprintf(stderr, "[%x] rdlock %p in %s\n", (unsigned)pthread_self(), ptr, __FUNCTION__))
#define WRLOCK(ptr) (fprintf(stderr, "[%x] attempt wrlock %p in %s\n", (unsigned)pthread_self(), ptr, __FUNCTION__), \
pthread_rwlock_wrlock(ptr), fprintf(stderr, "[%x] wrlock %p in %s\n", (unsigned)pthread_self(), ptr, __FUNCTION__))
#define RW_UNLOCK(ptr) (pthread_rwlock_unlock(ptr), \
fprintf(stderr, "[%x] un_rwlock %p in %s\n", (unsigned)pthread_self(), ptr, __FUNCTION__))

#else

#define LOCK(ptr) (pthread_mutex_lock(ptr))
#define UNLOCK(ptr) (pthread_mutex_unlock(ptr))

#define RDLOCK(ptr) (pthread_rwlock_rdlock(ptr))
#define WRLOCK(ptr) (pthread_rwlock_wrlock(ptr))
#define RW_UNLOCK(ptr) (pthread_rwlock_unlock(ptr))

#endif
/**********************************************************
 * superblock_lookup
 * For a newly encountered thread, insert its arena
 * (pointer to its first superblock) to the global metadata
 * Return the list_no of the arena
 **********************************************************/

int insert_arena(pthread_t pthread_id, superblock* sbp) {
	WRLOCK(&global_metadata_rwlock);
	DEBUG("[%x] Inserted sb[%d] to th[%x]\n", (unsigned)pthread_self(), SUPERBLOCK_NO(sbp), (unsigned)pthread_id);
	int cnt = (*global_metadata).pthread_cnt++;
	if (cnt == MAX_THREAD) {
		fprintf(stderr, "TOO MANY THREADS!\n");
		exit(0);
	}
	(*global_metadata).pthread_id[cnt] = pthread_id;
	(*global_metadata).ptr[cnt] = sbp;
	RW_UNLOCK(&global_metadata_rwlock);
	return cnt;
}

/**********************************************************
 * superblock_lookup
 * Lookup superblock belonging to the current thread
 * Return the listno of the pointer to that superblock
 * If the thread id is not found, return -1
 **********************************************************/

int superblock_lookup(pthread_t pthread_id) {
	RDLOCK(&global_metadata_rwlock);
	int cnt = (*global_metadata).pthread_cnt;
	for (int i=0; i<cnt; ++i) {
		if ((*global_metadata).pthread_id[i] == pthread_id) {
			RW_UNLOCK(&global_metadata_rwlock);
			return i;
		}
	}
	RW_UNLOCK(&global_metadata_rwlock);
	return -1;
}


/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course.
 **********************************************************/
void* extend_heap(size_t words) {
    void *ptr;
    size_t size;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    pthread_mutex_lock(&mutex);
    if ((ptr = mem_sbrk(size)) == (void *) -1) {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    pthread_mutex_unlock(&mutex);

    return ptr;
}


/**********************************************************
 * allocate_superblock
 * Extend the heap and allocate a new superblock on the top of the current heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course.
 * Return the pointer to the superblock
 **********************************************************/
superblock* allocate_superblock() {
	superblock* sbp;
	if ((sbp = extend_heap(PAGESIZE/WSIZE)) == NULL)
		return NULL;
	if (superblocks == NULL) // So it is allocating the first superblock
		superblocks = sbp;
	sbp -> next = NULL;
	pthread_mutex_init(&sbp -> lock, NULL);

	sbp -> prologue.size = PACK(DSIZE, 1);
	SET_FOOTER(&sbp -> prologue); // filled to sbp -> prologue_footer
	PUT(&sbp -> epilogue, 1);

	sbp -> head = (block*)&sbp -> data;
	sbp -> head -> size = sizeof(sbp->data);
	sbp -> head -> prev = sbp -> head -> next = NULL;
	SET_FOOTER(sbp -> head);
	//Set up the entire data part as a free block
	DEBUG("[%x] allocated superblock %d(%p)\n", (unsigned)pthread_self(), SUPERBLOCK_NO(sbp), sbp);

	return sbp;
}

/**********************************************************
 * list_insert
 * Insert the free block bp to the free address-ordered list list_no
 **********************************************************/
void list_insert(block* bp) {
    superblock* sbp = SUPERBLOCK_OF(bp);
	if (sbp -> head != NULL) {
		bp -> next = sbp -> head;
		bp -> next -> prev = bp;
	}
	else bp -> next = NULL;
	bp -> prev = NULL;
	sbp -> head = bp;
#ifdef DEBUG_MODE
	DEBUG("[%x] Inserted %d(%p) to superblock %d(%p)\n", (unsigned)pthread_self(), BLOCK_OFFSET(bp), bp, SUPERBLOCK_NO(bp), sbp);
#endif
}

/**********************************************************
 * list_remove
 * Remove the block bp from free list
 * Update head in superblock when necessary
 **********************************************************/
void list_remove(block* bp) {
    if (bp -> prev)
        bp -> prev -> next = bp -> next;
    if (bp -> next)
        bp -> next -> prev = bp -> prev;
	superblock* sbp = SUPERBLOCK_OF(bp);
	if (sbp -> head == bp)
		sbp -> head = bp -> next;
    bp -> prev = bp -> next = NULL;
}

/**********************************************************
 * relocate_free_segment
 * Build up a free block from a free segment at bp sized size
 * Assume that the size <= list_size[search_from]
 * search_from is used to accelerate the search if some extra
 * information is known.
 * Precondition: the thread is holding the lock of the superblock of bp
 **********************************************************/
void relocate_free_segment(block* bp, size_t size) {
	DEBUG("[%x] Relocate free segment %d(%p) sized %d in superblock %d(%p)\n",
	      (unsigned)pthread_self(), BLOCK_OFFSET(bp), bp, (int)size, SUPERBLOCK_NO(bp), SUPERBLOCK_OF(bp));
    bp->size = size;
    SET_FOOTER(bp);
    list_insert(bp);
}

/**********************************************************
 * mm_init
 * Initialize the heap.
 **********************************************************/
int mm_init(void) {
//	freopen ("mm.log", "w", stdout);
//	freopen ("/dev/tty", "a", stderr);
//	freopen("size.log", "a", stderr);
//	freopen("sizes.log", "w", stderr);
	DEBUG("Starting mm_init..\n", 0);
	if (sizeof(superblock) != PAGESIZE) {
		fprintf(stderr, "Invalid superblock size!");
		exit(0);
	}
    mem_init();
	superblocks = mem_sbrk(PAGESIZE);
    cmd_cnt = 0;
	global_metadata = superblocks;
	memset(global_metadata, 0, sizeof(global_header));
	pthread_mutex_init(&heap_rw_lock, NULL);
	pthread_rwlock_init(&global_metadata_rwlock, NULL);
    return 0;
}

/**********************************************************
 * coalesce
 * Centralized coalesce, scan throught the entire heap to coalesce all free blocks
 * Precondition: the thread is holding the lock of the superblock
 **********************************************************/
void *coalesce(block *bp) {
    int prev_alloc = GET_ALLOC(PREV_BLKP(bp));
    int next_alloc = GET_ALLOC(NEXT_BLKP(bp));
    size_t size = GET_SIZE(bp);
    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    } else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(NEXT_BLKP(bp));
        list_remove(bp);
        list_remove((NEXT_BLKP(bp)));
    } else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(PREV_BLKP(bp));
        list_remove(bp);
        list_remove(PREV_BLKP(bp));
        bp = PREV_BLKP(bp);
    } else {            /* Case 4 */
        size += GET_SIZE(PREV_BLKP(bp)) +
                GET_SIZE(NEXT_BLKP(bp));
        list_remove(bp);
        list_remove(PREV_BLKP(bp));
        list_remove(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
    }
    relocate_free_segment(bp, size); // Set up block for the new coalesced free segment
    return bp;
}

/**********************************************************
 * find_fit
 * Traverse the free list listp searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 * Precondition: the thread is holding the lock of the superblock
 **********************************************************/
block *find_fit(size_t asize, block* listp) {
    block *bp;
    /* Starting from the first block in the list, note that listp can be NULL */
    for (bp = listp; bp != NULL; bp = bp -> next) {
        if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp)))
            return bp;
    }
    return NULL;
}

/**********************************************************
 * place
 * Split a block sized asize from block bp with size free_size, located on list# <= list_no
 * des_direction: specify whether split the upper part(1) or lower part(0) of the block
 * -1: split in alternative ways
 * Precondition: the thread is holding the lock of the superblock
 **********************************************************/
ablock* place(block* bp, int asize, int free_size, int des_direction) {
	static char rec_direction = 0;
	char direction = des_direction == -1? rec_direction : des_direction;
	DEBUG("[%x] allocate %d in block %d(%p) sized %d at superblock %d(%p), direction: %s\n",
	      (unsigned)pthread_self(), asize, BLOCK_OFFSET(bp), bp, free_size, SUPERBLOCK_NO(bp), SUPERBLOCK_OF(bp), direction == 0? "LOW":"HIGH");
    if (free_size - asize <= EMPTY_BLOCKSIZE) // If the remaining space after the split could not hold a block
        asize = free_size;
	list_remove(bp);
    if (direction == 0) { // Split the block from the lower address
	    DEBUG("[%x] PUT(%p, 0x%x)\n", (unsigned)pthread_self(), &bp->size, (unsigned)PACK(asize, 1));
        bp -> size = PACK(asize, 1);
        SET_FOOTER(bp);
        if (free_size > asize) {
            block* rem = NEXT_BLKP(bp);
            relocate_free_segment(rem, free_size - asize);
        }
    }
    else { // Split the block from the upper address
        block* rem = bp;
        PUT(MOVE(NEXT_BLKP(bp), -WSIZE), PACK(asize, 1));
        bp = PREV_BLKP(NEXT_BLKP(bp));
	    DEBUG("[%x] PUT(%p, 0x%x)\n", (unsigned)pthread_self(), &bp->size, (unsigned)PACK(asize, 1));
        bp -> size = PACK(asize, 1);
        if (free_size > asize)
            relocate_free_segment(rem, free_size - asize);
    }
    rec_direction ^= 1; // Direction changes next time
    return (ablock*)bp;
}

void* mm_free_thread(void* ptr) {
#ifdef RUN_MM_CHECK
	mm_check();
#endif
	RDLOCK(&heap_rw_lock);
	DEBUG("[%x] mm_free %d(%p) @ %d\n", (unsigned)pthread_self(),  BLOCK_OFFSET(ptr), ptr, ++cmd_cnt);
	block* bp = (block*)DATA2BLOCK(ptr);

	pthread_mutex_lock(&SUPERBLOCK_OF(bp)->lock);
	DEBUG("[%x] PUT(%p, 0x%x)\n", (unsigned)pthread_self(), &bp->size, (unsigned)GET_SIZE(bp));
	bp -> size = GET_SIZE(bp);
	SET_FOOTER(bp);
	// prev and next of an allocated block should be overlapped by data
	bp->next = NULL;
	bp->prev = NULL;
	list_insert(bp);
	coalesce(bp);
    pthread_mutex_unlock(&SUPERBLOCK_OF(bp)->lock);

	DEBUG("[%x] %d mm_free success\n", (unsigned)pthread_self(), BLOCK_OFFSET(ptr));
	RW_UNLOCK(&heap_rw_lock);
#ifdef RUN_MM_CHECK
	mm_check();
#endif
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
}

void* mm_malloc_thread(int asize) {
	block *bp;
	DEBUG("[%x] mm_malloc %d @ %d\n", (unsigned)pthread_self(), asize, ++cmd_cnt);

#ifdef RUN_MM_CHECK
	mm_check();
#endif
	RDLOCK(&heap_rw_lock);

	pthread_t id = (unsigned)pthread_self();
	int list_no = superblock_lookup(id);
	superblock* sbp;
	if (list_no == -1) {
		sbp = allocate_superblock();
		list_no = insert_arena(id, sbp);
	}
	else sbp = (*global_metadata).ptr[list_no];

	for (; sbp != NULL; sbp = sbp -> next) {
		LOCK(&sbp -> lock);
		if ((bp = find_fit(asize, sbp -> head)) != NULL) {
			void *ret = place(bp, asize, GET_SIZE(bp), -1) -> data;
			DEBUG("[%x] mm_malloc %d -> %d(%p) success\n", (unsigned)pthread_self(), asize, BLOCK_OFFSET(ret), ret);
			UNLOCK(&sbp -> lock);
			RW_UNLOCK(&heap_rw_lock);
#ifdef RUN_MM_CHECK
			mm_check();
#endif
			return ret;
		}
		UNLOCK(&sbp -> lock);
	}

	/* No fit found. Allocate a new superblock */
	// Adjust the chunk size adaptively
	if ((sbp = allocate_superblock()) == NULL) {
		DEBUG("allocate superblock failed!!!\n", 0);
		return NULL;
	}
	// No need to lock, because no other thread will access this new superblock
	if ((bp = find_fit(asize, sbp -> head)) == NULL) {
		DEBUG("size too large!!!\n", 0);
		return NULL;
	}
	sbp -> next = (*global_metadata).ptr[list_no];
	(*global_metadata).ptr[list_no] = sbp;
	void* ret = place(bp, asize, GET_SIZE(bp), -1) -> data;
	DEBUG("[%x] mm_malloc %d -> %d(%p) success\n", (unsigned)pthread_self(), asize, BLOCK_OFFSET(ret), ret);
	RW_UNLOCK(&heap_rw_lock);
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
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 * Precondition: Current thread is holding none of the locks
 *********************************************************/
int mm_check(void) {
    block *bp, *nbp;
	superblock* sbp;
	if (cmd_cnt < 25536)
		return 0;
	pthread_rwlock_wrlock(&heap_rw_lock);
	int cnt = (*global_metadata).pthread_cnt;
	for (int i=0; i<cnt; ++i) {
		pthread_t th_id = (*global_metadata).pthread_id[i];
		DEBUG("Thread [%x] in arena[%d]\n", (unsigned)th_id, i);
		for (sbp = (*global_metadata).ptr[i]; sbp != NULL; sbp = sbp->next) {
			int sb_no = SUPERBLOCK_NO(sbp);
			// Check if the superblock is aligned to 4K
			if ((int) ((void *) sbp - (void *) dseg_lo) % PAGESIZE != 0) {
				fprintf(stderr, "[%x] Error: Unaligned superblock %d(%p) of [%x]",
				        (unsigned)pthread_self, sb_no, sbp, (unsigned)th_id);
				while (1);
			}
			// Check if the prologue is correct
			if (sbp->prologue.size != PACK(DSIZE, 1)) {
				fprintf(stderr, "[%x] Error: Illegal prologue at superblock %d(%p): %x\n",
				        (unsigned)pthread_self(), sb_no, sbp, (int)sbp->prologue.data);
				while (1);
			}
			// Check if the prologue footer is correct
			if (sbp->prologue_footer != PACK(DSIZE, 1)) {
				fprintf(stderr, "[%x] Error: Illegal prologue_footer at superblock %d(%p): %x\n",
				        (unsigned)pthread_self(), sb_no, sbp, (int)sbp->prologue_footer);
				while (1);
			}
			// Check if the epilogue is correct
			if (sbp->epilogue[0] != PACK(1, 1)) {
				fprintf(stderr, "[%x] Error: Illegal epilogue %p: %x\n", (unsigned)pthread_self(), &sbp->epilogue[0], (int)sbp->epilogue[0]);
				while (1);
			}

			// Traverse through the explicit list
			for (bp = sbp->head; bp != NULL; bp = bp->next) {
				// Check if the block is free
				if (GET_ALLOC(bp)) {
					fprintf(stderr, "[%x] Error: Block %d(%p) sized %d in superblock %d(%p) is allocated\n",
					        (unsigned)pthread_self(), BLOCK_OFFSET(bp), bp, (int) GET_SIZE(bp), sb_no, sbp);
					while (1);
				}

				if (bp -> next && bp -> next -> prev != bp) {
					fprintf(stderr, "[%x] Error: Block %d(%p) sized %d in superblock %d(%p) is not correctly double-linked\n",
					        (unsigned)pthread_self(),BLOCK_OFFSET(bp), bp, (int) GET_SIZE(bp), sb_no, sbp);
					while (1);
				}
				// Check if the block presents in the implicit list (linked in the heap by sizes)
				for (nbp = (block*)&sbp->data; (void *) nbp < (void*)&sbp->epilogue; nbp = NEXT_BLKP(nbp))
					if (bp == nbp)
						break;
				if ((void *) nbp > (void*)&sbp->epilogue) {
					fprintf(stderr,
					        "[%x] Error: Block %d(%p) sized %d in superblock %d(%p)could not be found in contiguity list\n",
					        (unsigned) pthread_self(), BLOCK_OFFSET(bp), bp, (int) GET_SIZE(bp), sb_no, sbp);
					while (1);
				}
			}

			// Traverse through the heap
			for (bp = (block*)&sbp->data; (void *) bp < (void*)&sbp->epilogue; bp = NEXT_BLKP(bp)) {
				int size = GET_SIZE(bp);
				// Check if the block data is 16B-aligned
				if (((size_t)(bp->data)) & 0xF) {
					fprintf(stderr,
					        "[%x] Error: Block %d(%p) sized %d alloc %d in superblock %d(%p) is not 16B-aligned\n",
					        (unsigned)pthread_self(), BLOCK_OFFSET(bp), bp, (int) GET_SIZE(bp), (int) GET_ALLOC(bp), sb_no, sbp);
					while (1);
				}
				// For a free block, check if it can be found in free list
				if (size > 0 && !GET_ALLOC(bp)) {
					for (nbp = sbp->head; nbp != NULL; nbp = nbp->next)
						if (nbp == bp)
							break;
					if (nbp == NULL) {
						fprintf(stderr,
						        "[%x] Error: Block %d(%p) sized %d in superblock %d(%p) could not be found in free list\n",
						        (unsigned)pthread_self(), BLOCK_OFFSET(bp), bp, (int)GET_SIZE(bp), sb_no, sbp);
						while (1);
					}
				}
			}

			// Output the memory locations in the current heap
			DEBUG("[%x] Current superblock %d(%p):\n", (unsigned)pthread_self(), sb_no, sbp);
			int acc_addr = 0;
			for (bp = (block*)&sbp->data; (void *) bp < (void*)&sbp->epilogue; bp = NEXT_BLKP(bp))
				DEBUG("\t%d%c\t|", (int) GET_DATASIZE(bp), GET_ALLOC(bp) ? 'a' : 'f');
			DEBUG("\n", 0);
			// Output the memory offsets of the block boundaries
			for (bp = (block*)&sbp->data; (void *) bp < (void*)&sbp->epilogue; bp = NEXT_BLKP(bp)) {
				acc_addr += GET_SIZE(bp);
				DEBUG("\t\t%d", acc_addr);
			}
			DEBUG("\n\n", 0);
			DEBUG("[%x] Current list %d(%p):\n", (unsigned)pthread_self(), sb_no, sbp);
			for (nbp = sbp->head; nbp != NULL; nbp = nbp->next)
				DEBUG("%d(%p)[%d]%s", BLOCK_OFFSET(nbp), nbp, (int)nbp->size, nbp->next == NULL? "\n":", ");
			DEBUG("\n\n", 0);
		}
	}

	pthread_rwlock_unlock(&heap_rw_lock);
    return 1;
}
