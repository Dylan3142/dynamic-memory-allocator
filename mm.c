/* 
 * mm-implicit.c -  Simple allocator based on implicit free lists, 
 *                  first fit placement, and boundary tag coalescing. 
 *
 * Each block has header and footer of the form:
 * 
 *      31                     3  2  1  0 
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      ----------------------------------- 
 * 
 * where s are the meaningful size bits and a/f is set 
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap  
 *  -----------------------------------------------------------------   
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name */
  "team skib",
  /* First member's full name */
  "dylan carter",
  /* First member's email address */
  "dyca3990@colorado.edu",
  /* Second member's full name (leave blank if none) */
  "",
  /* Second member's email address (leave blank if none) */
  ""
};

/////////////////////////////////////////////////////////////////////////////
// Constants and macros
//
// These correspond to the material in Figure 9.43 of the text
// The macros have been turned into C++ inline functions to
// make debugging code easier.
//
/////////////////////////////////////////////////////////////////////////////
#define WSIZE       4       /* word size (bytes) */  
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)
#define NEXT_FREE(bp) (*(char **)(bp))

static inline int MAX(int x, int y) {
  return x > y ? x : y;
}

//
// Pack a size and allocated bit into a word
// We mask of the "alloc" field to insure only
// the lower bit is used
//
static inline uint32_t PACK(uint32_t size, int alloc) {
  return ((size) | (alloc & 0x1));
}

//
// Read and write a word at address p
//
static inline uint32_t GET(void *p) { return  *(uint32_t *)p; }
static inline void PUT( void *p, uint32_t val)
{
  *((uint32_t *)p) = val;
}

//
// Read the size and allocated fields from address p
//
static inline uint32_t GET_SIZE( void *p )  { 
  return GET(p) & ~0x7;
}

static inline int GET_ALLOC( void *p  ) {
  return GET(p) & 0x1;
}

//
// Given block ptr bp, compute address of its header and footer
//
static inline void *HDRP(void *bp) {

  return ( (char *)bp) - WSIZE;
}
static inline void *FTRP(void *bp) {
  return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

//
// Given block ptr bp, compute address of next and previous blocks
//
static inline void *NEXT_BLKP(void *bp) {
  return  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}

static inline void* PREV_BLKP(void *bp){
  return  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}

/////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//
static char *free_listp;
static char *heap_listp;  /* pointer to first block */  
//
// function prototypes for internal helper routines
//
static void *extend_heap(uint32_t words);
static void place(void *bp, uint32_t asize);
static void *find_fit(uint32_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkblock(void *bp);
static void insert_free(void *bp);
static void remove_free(void *bp); 

static void insert_free(void *bp)
{
  NEXT_FREE(bp) = free_listp;
  free_listp = bp;
}

static void remove_free(void *bp)
{
  if(free_listp == NULL)
  {
    return; 
  }
  if(free_listp == bp)
  {
    free_listp = NEXT_FREE(bp);
    return; 
  }

  void *prev = free_listp;
  while (prev != NULL && NEXT_FREE(prev) != bp)
  {
    prev = NEXT_FREE(prev); 
  }

  if(prev != NULL)
  {
    NEXT_FREE(prev) = NEXT_FREE(bp); 
  }
}

//
// mm_init - Initialize the memory manager 
//
int mm_init(void) 
{
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
  {
    return -1; 
  }

  free_listp = NULL;

  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3 * WSIZE), PACK(0,1)); 

  heap_listp += (2 * WSIZE); 

  if(extend_heap(CHUNKSIZE / WSIZE) == NULL)
  {
    return -1; 
  }
 
  return 0;
}


//
// extend_heap - Extend heap with free block and return its block pointer
//
static void *extend_heap(uint32_t words) 
{
  char *bp;
  uint32_t size; 

  if(words % 2)
  {
    size = (words + 1) * WSIZE;
  }
  else
  {
    size = words * WSIZE; 
  }

  if((bp = mem_sbrk(size)) == (void *)-1)
  {
    return NULL;
  }

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0)); 

  PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); 

  return coalesce(bp);
}


//
// Practice problem 9.8
//
// find_fit - Find a fit for a block with asize bytes 
//
static void *find_fit(uint32_t asize)
{
    void *bp; 

    for(bp = free_listp; bp != NULL; bp = NEXT_FREE(bp))
    {
      if(asize <= GET_SIZE(HDRP(bp)))
      {
        return bp; 
      }
    }
    return NULL;
}

// 
// mm_free - Free a block 
//
void mm_free(void *bp)
{
  if (bp == NULL)
  {
    return; 
  }

  uint32_t size = GET_SIZE(HDRP(bp)); 

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0)); 
  coalesce(bp); 
}

//
// coalesce - boundary tag coalescing. Return ptr to coalesced block
//
static void *coalesce(void *bp) 
{
  int prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  uint32_t size = GET_SIZE(HDRP(bp)); 

  if(!prev_alloc)
  {
    remove_free(PREV_BLKP(bp));
  }
  if(!next_alloc)
  {
    remove_free(NEXT_BLKP(bp));
  }
  if(prev_alloc && next_alloc)
  {
    return bp; 
  }
  else if (prev_alloc && !next_alloc)
  {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }
  else if (!prev_alloc && next_alloc)
  {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    bp = PREV_BLKP(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0)); 
  }
  else
  {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    bp = PREV_BLKP(bp);
    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0)); 
  }
  insert_free(bp); 

  return bp; 
}

//
// mm_malloc - Allocate a block with at least size bytes of payload 
//
void *mm_malloc(uint32_t size) 
{
  uint32_t asize; 
  uint32_t extendsize; 
  char *bp; 

  if(size == 0)
  {
    return NULL; 
  }

  asize = ALIGN(size + OVERHEAD);
  if(asize < 2 * DSIZE)
  {
    asize = 2 * DSIZE; 
  }

  if((bp = find_fit(asize)) != NULL)
  {
    place(bp, asize);
    return bp; 
  }

  extendsize = MAX(asize, CHUNKSIZE);
  if((bp = extend_heap(extendsize / WSIZE)) == NULL)
  {
    return NULL;
  }

  place(bp, asize);
  return bp; 
} 

//
//
// Practice problem 9.9
//
// place - Place block of asize bytes at start of free block bp 
//         and split if remainder would be at least minimum block size
//
static void place(void *bp, uint32_t asize)
{
  uint32_t csize = GET_SIZE(HDRP(bp));

  remove_free(bp);

  if((csize - asize) >= (2 * DSIZE))
  {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    void *next_bp = NEXT_BLKP(bp);
    PUT(HDRP(next_bp), PACK(csize - asize, 0));
    PUT(FTRP(next_bp), PACK(csize - asize, 0));

    insert_free(next_bp);
  }
  else
  {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}


//
// mm_realloc -- implemented for you
//
void *mm_realloc(void *ptr, uint32_t size)
{
    // Standard realloc semantics
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    uint32_t oldsize = GET_SIZE(HDRP(ptr));

    // Compute adjusted size, same way as mm_malloc
    uint32_t asize = ALIGN(size + OVERHEAD);
    if (asize < 2 * DSIZE) {
        asize = 2 * DSIZE;
    }

    // Case 1: new size fits in the old block → shrink in place
    if (asize <= oldsize) {
        uint32_t remainder = oldsize - asize;

        if (remainder >= 2 * DSIZE) {
            // Shrink current block
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));

            // Create a new free block with the leftover space
            void *next_bp = NEXT_BLKP(ptr);
            PUT(HDRP(next_bp), PACK(remainder, 0));
            PUT(FTRP(next_bp), PACK(remainder, 0));

            // Coalesce new free block with neighbors and
            // insert it into the free list
            coalesce(next_bp);
        }
        // If remainder is too small, just keep the slightly bigger block
        return ptr;
    }

    // Case 2: try to expand into next block if it's free and large enough
    void *next_bp = NEXT_BLKP(ptr);
    if (!GET_ALLOC(HDRP(next_bp))) {
        uint32_t next_size = GET_SIZE(HDRP(next_bp));
        uint32_t combined = oldsize + next_size;

        if (combined >= asize) {
            // We're going to consume the next free block
            remove_free(next_bp);

            // Use combined space for this block
            PUT(HDRP(ptr), PACK(combined, 1));
            PUT(FTRP(ptr), PACK(combined, 1));

            return ptr;
        }
    }

    // Case 3: cannot grow in place → fall back to malloc+copy
    void *newp = mm_malloc(size);
    if (newp == NULL) {
        printf("ERROR: mm_malloc failed in mm_realloc\n");
        exit(1);
    }

    // Copy the payload (not the header/footer)
    uint32_t copySize = oldsize - OVERHEAD;
    if (size < copySize) {
        copySize = size;
    }
    memcpy(newp, ptr, copySize);
    mm_free(ptr);
    return newp;
}


//
// mm_checkheap - Check the heap for consistency 
//
void mm_checkheap(int verbose) 
{
  //
  // This provided implementation assumes you're using the structure
  // of the sample solution in the text. If not, omit this code
  // and provide your own mm_checkheap
  //
  void *bp = heap_listp;
  
  if (verbose) {
    printf("Heap (%p):\n", heap_listp);
  }

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
	printf("Bad prologue header\n");
  }
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose)  {
      printblock(bp);
    }
    checkblock(bp);
  }
     
  if (verbose) {
    printblock(bp);
  }

  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
    printf("Bad epilogue header\n");
  }
}

static void printblock(void *bp) 
{
  uint32_t hsize, halloc, fsize, falloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));  
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));  
    
  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%d:%c] footer: [%d:%c]\n",
	 bp, 
	 (int) hsize, (halloc ? 'a' : 'f'), 
	 (int) fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
  if ((uintptr_t)bp % 8) {
    printf("Error: %p is not doubleword aligned\n", bp);
  }
  if (GET(HDRP(bp)) != GET(FTRP(bp))) {
    printf("Error: header does not match footer\n");
  }
}

