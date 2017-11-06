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
#define MAXCHUNKSIZE   (32768)      /* initial heap size (bytes) */


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

typedef struct block_st{
	size_t size;
	struct block_st* prev;
	struct block_st* next;
	char data[0];
} block;

typedef struct ablock_st{
	size_t size;
	char data[0];
} ablock;

/* Size of an empty block */
#define EMPTY_BLOCKSIZE  (QSIZE)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     ((size_t)((((block*)(p)) -> size) & ~(DSIZE - 1)))
#define GET_DATASIZE(p)     (GET_SIZE(p) - DSIZE)
#define GET_ALLOC(p)    ((int)((((block*)(p)) -> size) & 0x1))

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
#define LAST_BLOCK (PREV_BLKP(mem_heap_hi() + 1 - WSIZE))

/* Number of segregated lists */
#define LIST_CNT 19

/* Debug output helpers */
//#define DEBUG_MODE

#ifdef DEBUG_MODE
#define DEBUG(f, ...) (fprintf(stderr, (f), __VA_ARGS__))
#define RUN_MM_CHECK
#else
#define DEBUG(f, ...) (0)
#endif

void *heap_listp = NULL;
block* list_heads[LIST_CNT];
const int list_size[LIST_CNT] = {9, 18, 66, 74, 114, 130, 162, 450, 514, 1720, 4074, 4097, 5559, 8192, 11138, 15458, 19514, 23947, 28423};
int cmd_cnt;
int heap_starts;
int chunksize;

void why(){
	printf("WHY\n");
	printf("WHY\n");
	printf("WHY\n");
	printf("WHY\n");

	return;
}

/**********************************************************
 * list_insert
 * Insert the free block bp to the free address-ordered list list_no
 **********************************************************/
void list_insert(block* bp, int list_no) {
	//printf("I'M INSERTING SOMETHING\n");
	block* pos = list_heads[list_no];
	while (pos->next && pos->next < bp)
		pos = pos->next;
	if (pos -> next) {
		pos -> next -> prev = bp;
		bp -> next = pos -> next;
	}
	else bp -> next = NULL;
	pos -> next = bp;
	bp -> prev = pos;

	/*printf("bp: %d", bp);
	printf("bp: %d", bp);
	printf("bp: %d", bp);
	printf("bp: %d", bp);
	printf("bp: %d", bp);*/
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
	for (i=LIST_CNT - 1; i>=0; i--)
		if (asize >= list_size[i])
			return i;
	return LIST_CNT - 1;
}

/**********************************************************
 * relocate_free_segment
 * Set up a free block from a free segment at bp sized size
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
//	freopen ("mm.log", "a", stderr);
//	freopen ("/dev/tty", "a", stderr);
//	freopen("size.log", "a", stderr);
	cmd_cnt = 0;
	chunksize = 8224;
	if ((heap_listp = mem_sbrk(6 * WSIZE + LIST_CNT * EMPTY_BLOCKSIZE)) == (void *) -1)
		return -1;
	DEBUG("Init: allocate %d bytes, %p -> ", mem_heapsize(), heap_listp);
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
	heap_starts = MOVE(pt, WSIZE);
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
	//printf("COALESCE\n");
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
	relocate_free_segment(bp, size, LIST_CNT - 1); // Set up block for the new free segment
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

	NEXT_BLKP(bp) -> size = PACK(1, 1); // Set new epilogue footer

	/* Coalesce if the previous block was free */
	return coalesce(bp);
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
	for (bp = listp -> next; bp != NULL; bp = bp -> next) {
		if (!GET_ALLOC(bp) && (asize <= GET_SIZE(bp)))
			return bp;
	}
	return NULL;
#endif
}

/**********************************************************
 * place
 * Split a block sized asize from block bp with size free_size, located on list# <= listno
 * des_direction: specify whether split the upper part(1) or lower part(0) of the block
 * -1: split in alternative ways
 **********************************************************/
ablock* place(block* bp, int asize, int free_size, int listno, int des_direction) {
	if (free_size - asize <= EMPTY_BLOCKSIZE)
		asize = free_size;
	list_remove(bp);
	static char rec_direction = 0;
	char direction = des_direction == -1? rec_direction : des_direction;
	if (direction == 0) {
		bp -> size = PACK(asize, 1);
		SET_FOOTER(bp);
		if (free_size > asize) {
			block* rem = NEXT_BLKP(bp);
			relocate_free_segment(rem, free_size - asize, listno);
		}
	}
	else {
		block* rem = bp;
		PUT(MOVE(NEXT_BLKP(bp), -WSIZE), PACK(asize, 1));
		bp = PREV_BLKP(NEXT_BLKP(bp));
		bp -> size = PACK(asize, 1);
		if (free_size > asize)
			relocate_free_segment(rem, free_size - asize, listno);
	}

	rec_direction ^= 1;
	return (ablock*)bp;
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void* ptr) {
#ifdef RUN_MM_CHECK
	mm_check();
#endif
	DEBUG("mm_free %d(%p) @ %d\n", (int)(ptr - heap_starts), ptr, ++cmd_cnt);
	if (ptr == NULL) {
		return;
	}
	block* bp = DATA2BLOCK((block *)ptr);
	bp -> size = GET_SIZE(bp);
	SET_FOOTER(bp);
	// prev and next of an allocated block should be NULL
	bp->next = NULL;
	bp->prev = NULL;
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
#ifdef RUN_MM_CHECK
	mm_check();
#endif
//	fprintf(stderr, "%d\n", size);
	DEBUG("mm_malloc %d @ %d -> ", size, ++cmd_cnt);
	size_t asize; /* adjusted block size */
	size_t extendsize; /* amount to extend heap if no fit */
	block *bp;
	int list_no, n_list_no;

	/* Ignore spurious requests */
	if (size == 0)
		return NULL;

	/* Adjust block size to include overhead and alignment reqs. */
	asize = ALIGN_16B(size + DSIZE);

	list_no = find_list(asize);
	for (n_list_no = list_no; n_list_no < LIST_CNT; ++n_list_no)
		/* Search the free list for a fit */
		if ((bp = find_fit(asize, list_heads[n_list_no])) != NULL) {
			void* ret = place(bp, asize, GET_SIZE(bp), n_list_no, -1)->data;
			DEBUG("%d(%p)\n", (int)(ret - heap_starts), ret);
			return ret;
		}

	/* No fit found. Get more memory and place the block */
//	if (!GET_ALLOC(LAST_BLOCK))
//		extendsize -= GET_SIZE(LAST_BLOCK);

//	static int asize_rec[25] = {0};
//	static int asize_pt = 0;
//	static int ext_cnt = 0;
//	static int asize_sum = 0;

//	asize_sum = asize_sum - asize_rec[asize_pt] + asize;
//	asize_rec[asize_pt] = asize;
//	asize_pt = (asize_pt + 1) % 25;
//	if (++ext_cnt>=25)
//		CHUNKSIZE = asize_sum;

	if (asize > chunksize)
		chunksize = MIN(MAXCHUNKSIZE, chunksize * 2);
	block* last = LAST_BLOCK();
	if (!(GET_ALLOC(last) || last < heap_starts))
		asize -= GET_SIZE(last);
	extendsize = MAX(asize, chunksize);
	if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
		return NULL;
	void* ret = place(bp, asize, GET_SIZE(bp), LIST_CNT - 1, -1) -> data;
	DEBUG("%d(%p)\n", (int)(ret - heap_starts), ret);
	return ret;
}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size) {
	/* If size == 0 then this is just free, and we return NULL. */
#ifdef RUN_MM_CHECK
	mm_check();
#endif
	DEBUG("mm_realloc %d(%p) to size %d @ %d\n", (int)(ptr - heap_starts), ptr, size, ++cmd_cnt);
	--cmd_cnt;
	if (size == 0) {
		mm_free(ptr);
		return NULL;
	}
	/* If oldptr is NULL, then this is just malloc. */
	if (ptr == NULL)
		return (mm_malloc(size));

	ablock *bp = DATA2BLOCK(ptr);
	int asize = ALIGN_16B(size + DSIZE);
	// Coalesce with next empty block
	if (!GET_ALLOC(NEXT_BLKP(bp))) {
		list_remove(NEXT_BLKP(bp));
		bp -> size = PACK(GET_SIZE(bp) + GET_SIZE(NEXT_BLKP(bp)), 1);
		SET_FOOTER(bp);
	}
	// Try to split the current block
	if (GET_SIZE(bp) >= asize) {
		++cmd_cnt;
		return place(bp, asize, GET_SIZE(bp), LIST_CNT - 1, 0)->data;
	}

	// When we have to allocate a new block
	ablock* newptr = DATA2BLOCK(mm_malloc(size));
	if (newptr == NULL)
		return NULL;

	/* Copy the old data. */
	memcpy(newptr -> data, bp -> data, MIN(GET_DATASIZE(bp), size));
	--cmd_cnt;
	mm_free(bp->data);
	DEBUG("realloc %d(%p) -> return %d(%p) size %d\n", (int)(ptr - heap_starts), ptr, (int)((void*)newptr - heap_starts), newptr, size);
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
	// Traverse all the free lists
	for (int i = 0; i < LIST_CNT; ++i)
		for (nbp = list_heads[i] -> next; nbp != NULL; nbp = nbp -> next) {
			if (GET_ALLOC(nbp)) {
				fprintf(stderr, "Error: Block %p sized %d in free list %d is allocated\n", nbp, (int)GET_SIZE(nbp), i);
				return 0;
			}
			if (GET_SIZE(nbp) < list_size[i] || (i != LIST_CNT - 1 && GET_SIZE(nbp) >= list_size[i+1] )) {
				fprintf(stderr, "Error: Block %p sized %d stored free list for size %d\n", nbp, (int)GET_SIZE(nbp), list_size[i]);
				return 0;
			}
			for (bp = start; (void*)bp < mem_heap_hi() - WSIZE; bp = NEXT_BLKP(bp))
				if (bp == nbp)
					break;
			if ((void*)bp > mem_heap_hi()) {
				fprintf(stderr, "Error: Block %p sized %d in free list %d could not be found in contiguity list\n", nbp, (int)GET_SIZE(nbp), i);
				return 0;
			}
		}
	if (GET(MOVE(start, -WSIZE)) != PACK(EMPTY_BLOCKSIZE, 1))
		fprintf(stderr, "Error: Illegal prologue %p: %d\n", MOVE(start, -WSIZE), (int)GET(MOVE(start, -WSIZE)));
//	DEBUG("Start scanning from heap range %p, to %p\n", start, mem_heap_hi() - WSIZE);
	// Traverse through the heap
	for (bp = start; (void*)bp < mem_heap_hi() - WSIZE; bp = NEXT_BLKP(bp)) {
		int size = GET_SIZE(bp);
//		DEBUG("(%p, %d)\n", bp, size);
		if (((size_t) (bp -> data)) & 0xF) {
			fprintf(stderr, "Error: Block %p, sized %d, alloc:%d not aligned to 16B\n", bp, (int)GET_SIZE(bp), GET_ALLOC(bp));
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
		fprintf(stderr, "Error: Illegal epilogue %p: %d\n", bp, (int)bp->size);

	// Out put a diagram for the current heap
	DEBUG("Current heap:\n", 0);
	int acc_addr = 0;
	for (bp = start;  (void*)bp < mem_heap_hi() - WSIZE; bp = NEXT_BLKP(bp))
		DEBUG("\t%d%c\t|", (int)GET_DATASIZE(bp), GET_ALLOC(bp)?'a':'f');
	DEBUG("\n", 0);
	for (bp = start;  (void*)bp < mem_heap_hi() - WSIZE; bp = NEXT_BLKP(bp)) {
		acc_addr += GET_SIZE(bp);
		DEBUG("\t\t%d", acc_addr);
	}
	DEBUG("\n\n", 0);
	return 1;
}
