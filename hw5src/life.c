/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include "stdlib.h"
#include "stdio.h"
#include "pthread.h"

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_args{
    char* outboard;
    char* inboard;
    int thread;
    int nrows;
    int ncols;
} arguments;

#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD( __board, __i, __j )  (__board[(__i) + LDA*(__j)])

/*****************************************************************************
 * Game of Life processing thread. Only processes a particular block of the
 * entire game board.
 ****************************************************************************/
void thread_board(char* outboard,
                  char* inboard,
                  const int srows,
                  const int scols,
                  const int nrows,
                  const int nrowsmax,
                  const int ncols,
                  const int ncolsmax){
    int i, j, i1, j1;
    int LDA = nrowsmax;
    int inorth, isouth, jwest, jeast, inorth2, isouth2, jwest2, jeast2;
    char neighbor_count, neighbor_count2;

    for (i = srows; i < nrows; i+=32) {
        for (j = scols; j < ncols; j+=64) {
            //128bit rows * 512bit columns tiles
            for(i1 = i; i1 < i + 32; i1++){
                inorth = mod(i1 - 1, nrowsmax);
                isouth = mod(i1 + 1, nrowsmax);
                inorth2 = mod((i1) - 1, nrowsmax);
                isouth2 = mod((i1) + 1, nrowsmax);
                for(j1 = j; j1 < ((j + 64) - 1); j1+=2){
                    neighbor_count = BOARD (inboard, inorth, j1) +
                                     BOARD (inboard, isouth, j1);
                    neighbor_count2 = BOARD (inboard, inorth2, (j1+1)) +
                                     BOARD (inboard, isouth2, (j1+1));

                    jwest = mod(j1 - 1, ncolsmax);
                    jeast = mod(j1 + 1, ncolsmax);
                    jwest2 = mod((j1+1) - 1, ncolsmax);
                    jeast2 = mod((j1+1) + 1, ncolsmax);

                    neighbor_count +=
                            BOARD (inboard, inorth, jwest) +
                            BOARD (inboard, inorth, jeast) +
                            BOARD (inboard, i1, jwest) +
                            BOARD (inboard, i1, jeast) +
                            BOARD (inboard, isouth, jwest) +
                            BOARD (inboard, isouth, jeast);
                    neighbor_count2 +=
                            BOARD (inboard, inorth, jwest2) +
                            BOARD (inboard, inorth, jeast2) +
                            BOARD (inboard, i1, jwest2) +
                            BOARD (inboard, i1, jeast2) +
                            BOARD (inboard, isouth, jwest2) +
                            BOARD (inboard, isouth, jeast2);

                    BOARD(outboard, i1, j1) = alivep(neighbor_count, BOARD (inboard, i1, j1));
                    BOARD(outboard, i1, (j1+1)) = alivep(neighbor_count2, BOARD (inboard, i1, (j1+1)));
                }
            }
        }
    }
    return;
}

/*****************************************************************************
 * Thread entry function
 ****************************************************************************/
void* thread_handler(void* thread_args){
    int srows;
    int scols;

    arguments* threadArgs = (arguments *) thread_args;
    char* outboard = threadArgs->outboard;
    char* inboard = threadArgs->inboard;
    int nrows = threadArgs->nrows;
    const int nrowsmax = threadArgs->nrows;
    int ncols = threadArgs->ncols;
    const int ncolsmax = threadArgs->ncols;

    switch (threadArgs->thread){
        case 0:
            srows = 0;
            scols = 0;
            nrows = nrows/4;
            ncols = ncols/4;
            break;

        case 1:
            srows = 0;
            scols = ncols/4;
            nrows = nrows/4;
            ncols = ncols/2;
            break;

        case 2:
            srows = 0;
            scols = ncols/2;
            nrows = nrows/4;
            ncols = (3*ncols)/4;
            break;

        case 3:
            srows = 0;
            scols = (3*ncols)/4;
            nrows = nrows/4;
            ncols = ncols;
            break;

        case 4:
            srows = nrows/4;
            scols = 0;
            nrows = nrows/2;
            ncols = ncols/4;
            break;

        case 5:
            srows = nrows/4;
            scols = ncols/4;
            nrows = nrows/2;
            ncols = ncols/2;
            break;

        case 6:
            srows = nrows/4;
            scols = ncols/2;
            nrows = nrows/2;
            ncols = (3*ncols)/4;
            break;

        case 7:
            srows = nrows/4;
            scols = (3*ncols)/4;
            nrows = nrows/2;
            ncols = ncols;
            break;

        case 8:
            srows = nrows/2;
            scols = 0;
            nrows = (3*nrows)/4;
            ncols = ncols/4;
            break;

        case 9:
            srows = nrows/2;
            scols = ncols/4;
            nrows = (3*nrows)/4;
            ncols = ncols/2;
            break;

        case 10:
            srows = nrows/2;
            scols = ncols/2;
            nrows = (3*nrows)/4;
            ncols = (3*ncols)/4;
            break;

        case 11:
            srows = nrows/2;
            scols = (3*ncols)/4;
            nrows = (3*nrows)/4;
            ncols = ncols;
            break;

        case 12:
            srows = (3*nrows)/4;
            scols = 0;
            nrows = nrows;
            ncols = ncols/4;
            break;

        case 13:
            srows = (3*nrows)/4;
            scols = ncols/4;
            nrows = nrows;
            ncols = ncols/2;
            break;

        case 14:
            srows = (3*nrows)/4;
            scols = ncols/2;
            nrows = nrows;
            ncols = (3*ncols)/4;
            break;

        case 15:
            srows = (3*nrows)/4;
            scols = (3*ncols)/4;
            nrows = nrows;
            ncols = ncols;
            break;

        default:
            return;
    }

    thread_board(outboard,inboard, srows, scols, nrows, nrowsmax, ncols, ncolsmax);
	return;
}

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/

char* multi_game_of_life (char* outboard,
					char* inboard,
					const int nrows,
					const int ncols,
					const int gens_max){

	/* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
	int curgen;
    arguments thread_args[16];
    for(int i = 0; i < 16; i++){
        thread_args[i].nrows = nrows;
        thread_args[i].ncols = ncols;
        thread_args[i].thread = i;
    }

    pthread_t test_thread[16];
    for (curgen = 0; curgen < gens_max; curgen++) {
        for(int i = 0; i < 16; i++){
            thread_args[i].outboard = outboard;
            thread_args[i].inboard = inboard;
            pthread_create(&test_thread[i], NULL, thread_handler, &thread_args[i]);
        }
        for(int j = 0; j < 16; j++){
            pthread_join(test_thread[j], NULL);
        }
        SWAP_BOARDS(outboard, inboard);
    }
	/*
     * We return the output board, so that we know which one contains
     * the final result (because we've been swapping boards around).
     * Just be careful when you free() the two boards, so that you don't
     * free the same one twice!!!
     */
	return inboard;
}




char*
game_of_life (char* outboard, 
	      char* inboard,
	      const int nrows,
	      const int ncols,
	      const int gens_max)
{
  return multi_game_of_life (outboard, inboard, nrows, ncols, gens_max);
}

