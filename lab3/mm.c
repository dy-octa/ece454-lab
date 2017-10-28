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
		"Cheesecake",
		/* First member's full name */
		"Andrei Patranoiu",
		/* First member's email address */
		"andrei.patranoiu@mail.utoronto.ca",
		/* Second member's full name (leave blank if none) */
		"Mutian He",
		/* Second member's email address (leave blank if none) */
		"mutian.he@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define QSIZE       (4 * WSIZE)            /* quadword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */


#define MAX(x, y) ((x) > (y)?(x) :(y))
#define MIN(x, y) ((x) < (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p, val)      (*(uintptr_t *)(p) = (val))

/* Move a pointer pt by offset bytes */
#define MOVE(pt, offset) (((void*)pt) + offset)

/* Align the size to 16B */
#define ALIGN_16B(size) (((size)&15LL)?((size|15LL)+1):size)

typedef struct block_st{
	size_t size;
	struct block_st* prev;
	struct block_st* next;
	char data[0];
} block;

/* Size of an empty block */
#define EMPTY_BLOCKSIZE  (QSIZE)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     ((size_t)((((block*)p) -> size) & ~(DSIZE - 1)))
#define GET_DATASIZE(p)     (GET_SIZE(p) - EMPTY_BLOCKSIZE)
#define GET_ALLOC(p)    ((((block*)p) -> size) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define FTRP(bp)        ((size_t)(MOVE(bp, GET_SIZE(bp) - WSIZE)))
#define DATA2BLOCK(ptr) ((block*)(MOVE(ptr, -3*WSIZE)))

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((block*)MOVE(bp, GET_SIZE(bp)))
#define PREV_BLKP(bp) ((block*)MOVE(bp, -GET_SIZE(MOVE(bp, -WSIZE))))

/* Set the footer of a block pointer pt */
#define SET_FOOTER(pt) (PUT(FTRP(pt), ((block*) pt) -> size))

/* Number of segregated lists */
#define LIST_CNT 20

/* Debug output helpers */
//#define DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
#define DEBUG(f, ...) (fprintf(stderr, (f), __VA_ARGS__))
#else
#define DEBUG(f, ...) (0)
#endif

void *heap_listp = NULL;
block* list_heads[LIST_CNT];
const int list_size[LIST_CNT] = {10, 19, 67, 75, 115, 131, 451, 515, 4075, 4098, 6655, 8193, 11045, 15363, 19610, 23949, 28703, 147587, 303363, 459139};
int cmd_cnt;

/**********************************************************
 * list_insert
 * Insert the free block bp to the free list list_no
 **********************************************************/
void list_insert(block* bp, int list_no) {
	if (list_heads[list_no] -> next) {
		list_heads[list_no] -> next -> prev = bp;
		bp -> next = list_heads[list_no] -> next;
	}
	else bp -> next = NULL;
	list_heads[list_no] -> next = bp;
	bp -> prev = list_heads[list_no];
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
	if (asize <= list_size[0])
		return 0;
	for (i=LIST_CNT - 1; i>=0; --i)
		if (asize >= list_size[i])
			return i;
	return LIST_CNT;
}

/**********************************************************
 * relocate_free_segment
 * Locate a free segment from bp sized size
 * Assume that the size <= list_size[search_from]
 **********************************************************/
void relocate_free_segment(block* bp, size_t size, int search_from) {
	int i;
	bp->size = size;
	SET_FOOTER(bp);
	for (i=search_from; i > 0; --i) {
		if (size >= list_size[i])
			break;
	}
	list_insert(bp, i);
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void) {
	freopen ("/dev/tty", "a", stderr);
	cmd_cnt = 0;
	if ((heap_listp = mem_sbrk(6 * WSIZE + LIST_CNT * EMPTY_BLOCKSIZE)) == (void *) -1)
		return -1;
	DEBUG("Init: allocate %d bytes, %p -> ", mem_heapsize(), heap_listp);
//	freopen("mm.log", "a", stderr);
	block* pt = heap_listp;
	for (int i=0; i < LIST_CNT; ++i) {
		pt -> prev = NULL;
		pt -> next = NULL;
		pt -> size = PACK(EMPTY_BLOCKSIZE, 0); // sentinel header
		SET_FOOTER(pt); // sentinel footer
		list_heads[i] = pt;
		pt = MOVE(pt, EMPTY_BLOCKSIZE);
	}
	pt = MOVE(pt, WSIZE);
	pt -> prev = NULL;
	pt -> next = NULL;
	pt -> size = PACK(EMPTY_BLOCKSIZE, 1); // prologue header
	SET_FOOTER(pt); // prologue footer
	pt = MOVE(pt, EMPTY_BLOCKSIZE);
	pt -> size = PACK(1, 1); // epilogue header
	DEBUG("%p\n", MOVE(pt, WSIZE));
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
	relocate_free_segment(bp, size, LIST_CNT - 1);
	return bp;
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
block* extend_heap(size_t words) {
	block *bp;
	size_t size;

	/* Allocate an even number of words to maintain alignments */
	size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
	if ((bp = mem_sbrk(size)) == (void *) -1)
		return NULL;
	bp = MOVE(bp, -WSIZE); // Remove old epilogue header

	/* Initialize free block header/footer and the epilogue header */
	bp -> size = PACK(size, 0);                  // free block header
	SET_FOOTER(bp);               // free block footer

	list_insert(bp, find_list(bp->size));

	block* epil = NEXT_BLKP(bp);
	NEXT_BLKP(bp) -> size = PACK(1, 1); // Set new epilogue footer

	/* Coalesce if the previous block was free */
	return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
block *find_fit(size_t asize, block* listp) {
	block *bp;
	for (bp = listp; bp != NULL; bp = bp -> next) {
		if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp))) {
			return bp;
		}
	}
	return NULL;
}

/**********************************************************
 * place
 * Allocate a new block from block bp
 * Relocate the remaining free segment
 **********************************************************/
block* place(block* bp, int asize, int free_size, int listno) {
	if (free_size - asize <= EMPTY_BLOCKSIZE)
		asize = free_size;
	list_remove(bp);
	bp -> size = PACK(asize, 1);
	SET_FOOTER(bp);
	if (free_size > asize) {
		block* rem = NEXT_BLKP(bp);
		relocate_free_segment(rem, free_size - asize, listno);
	}
	return bp;
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void* ptr) {
//	mm_check();
	DEBUG("mm_free %p @ %d\n", ptr, ++cmd_cnt);
	if (ptr == NULL) {
		return;
	}
	block* bp = DATA2BLOCK(ptr);
	bp -> size = GET_SIZE(bp);
	SET_FOOTER(bp);
	// prev and next of an allocated block should be NULL
	list_insert(bp, find_list(bp->size));
	coalesce(bp);
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
//	mm_check();
	DEBUG("mm_malloc %d @ %d\n", size, ++cmd_cnt);
	size_t asize; /* adjusted block size */
	size_t extendsize; /* amount to extend heap if no fit */
	block *bp;
	int list_no, n_list_no;

	/* Ignore spurious requests */
	if (size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	asize = ALIGN_16B(size + EMPTY_BLOCKSIZE);

	list_no = find_list(asize);
	for (n_list_no = list_no; n_list_no < LIST_CNT; ++n_list_no)
		/* Search the free list for a fit */
		if ((bp = find_fit(asize, list_heads[n_list_no])) != NULL) {
			void* ret = place(bp, asize, GET_SIZE(bp), n_list_no)->data;
			DEBUG("malloc return: %p\n", ret);
			return ret;
		}

	/* No fit found. Get more memory and place the block */
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	void* ret = place(bp, asize, GET_SIZE(bp), LIST_CNT - 1) -> data;
	DEBUG("malloc return: %p\n", ret);
	return ret;
}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size) {
	/* If size == 0 then this is just free, and we return NULL. */
//	mm_check();
	DEBUG("mm_realloc %p to size %d @ %d\n", ptr, size, ++cmd_cnt);
	--cmd_cnt;
	if (size == 0) {
		mm_free(ptr);
		return NULL;
	}
	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL)
		return (mm_malloc(size));

	block *oldptr = DATA2BLOCK(ptr);
	block *newptr;

	newptr = DATA2BLOCK(mm_malloc(size));
	if (newptr == NULL)
		return NULL;

	/* Copy the old data. */
	memcpy(newptr -> data, oldptr -> data, MIN(GET_DATASIZE(oldptr), size));
	--cmd_cnt;
	mm_free(oldptr->data);
	DEBUG("realloc %p -> return %p (%d)\n", ptr, newptr->data, size);
	return newptr -> data;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void) {
	block* start = mem_heap_lo() + 5*WSIZE + LIST_CNT * EMPTY_BLOCKSIZE;
	block *bp, *nbp;
	for (int i = 0; i < LIST_CNT; ++i)
		for (nbp = list_heads[i] -> next; nbp != NULL; nbp = nbp -> next) {
			if (GET_ALLOC(nbp)) {
				fprintf(stderr, "Error: Block %p sized %d in free list %d is allocated\n", nbp, GET_SIZE(nbp), i);
				return 0;
			}
			if (GET_SIZE(nbp) < list_size[i] || (i != LIST_CNT - 1 && GET_SIZE(nbp) >= list_size[i+1] )) {
				fprintf(stderr, "Error: Block %p sized %d stored free list for size %d\n", nbp, GET_SIZE(nbp), list_size[i]);
				return 0;
			}
			for (bp = start; bp < mem_heap_hi() - WSIZE; bp = NEXT_BLKP(bp))
				if (bp == nbp)
					break;
			if (bp > mem_heap_hi()) {
				fprintf(stderr, "Error: Block %p sized %d in free list %d could not be found in contiguity list\n", nbp, GET_SIZE(nbp), i);
				return 0;
			}
		}
	if (GET(MOVE(start, -WSIZE)) != PACK(EMPTY_BLOCKSIZE, 1))
		fprintf(stderr, "Error: Illegal prologue %p: %d\n", MOVE(start, -WSIZE), GET(MOVE(start, -WSIZE)));
	DEBUG("Start scanning from heap range %p, to %p\n", start, mem_heap_hi() - WSIZE);
	for (bp = start; bp < mem_heap_hi() - WSIZE; bp = NEXT_BLKP(bp)) {
		int size = GET_SIZE(bp);
		DEBUG("(%p, %d)\n", bp, size);
		if (((size_t) (bp -> data)) & 0xF) {
			fprintf(stderr, "Error: Block %p, sized %d, alloc:%d not aligned to 16B\n", bp, GET_SIZE(bp), GET_ALLOC(bp));
			return 0;
		}

		if (size > 0 && !GET_ALLOC(bp)) {
			int listno = find_list(size);
			for (nbp = list_heads[listno]; nbp != NULL; nbp = nbp -> next)
				if (nbp == bp)
					break ;
			if (nbp == NULL) {
				fprintf(stderr, "Error: Block %p, sized %d could not be found in list %d\n", bp, size, listno);
				return 0;
			}
		}
	}
	if (bp -> size != PACK(1, 1))
		fprintf(stderr, "Error: Illegal epilogue %p: %d\n", bp, bp->size);
	return 1;
}
