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
    int srows;
    int scols;
    int nrows;
    int nrowsmax;
    int ncols;
    int ncolsmax;
    int gens_max;
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
                  const int ncolsmax,
                  const int gens_max){

    //printf("thread address of boards: %p, %p\n", inboard, outboard);
    //printf("Called thread_board\n");
    int i, j;
    int LDA = nrowsmax;

    for (i = srows; i < nrows; i++) {
        for (j = scols; j < ncols; j++) {
            const int inorth = mod(i - 1, nrowsmax);
            const int isouth = mod(i + 1, nrowsmax);
            const int jwest = mod(j - 1, ncolsmax);
            const int jeast = mod(j + 1, ncolsmax);

            const char neighbor_count =
                    BOARD (inboard, inorth, jwest) +
                    BOARD (inboard, inorth, j) +
                    BOARD (inboard, inorth, jeast) +
                    BOARD (inboard, i, jwest) +
                    BOARD (inboard, i, jeast) +
                    BOARD (inboard, isouth, jwest) +
                    BOARD (inboard, isouth, j) +
                    BOARD (inboard, isouth, jeast);
            //pthread_mutex_lock(&mutex);
            BOARD(outboard, i, j) = alivep(neighbor_count, BOARD (inboard, i, j));
            //pthread_mutex_unlock(&mutex);
        }
    }

    //SWAP_BOARDS(outboard, inboard);

    /*for (i = nrows/2; i < nrows; i++) {
        for (j = scols; j < ncols; j++) {
            const int inorth = mod(i - 1, nrows);
            const int isouth = mod(i + 1, nrows);
            const int jwest = mod(j - 1, ncols);
            const int jeast = mod(j + 1, ncols);

            const char neighbor_count =
                    BOARD (inboard, inorth, jwest) +
                    BOARD (inboard, inorth, j) +
                    BOARD (inboard, inorth, jeast) +
                    BOARD (inboard, i, jwest) +
                    BOARD (inboard, i, jeast) +
                    BOARD (inboard, isouth, jwest) +
                    BOARD (inboard, isouth, j) +
                    BOARD (inboard, isouth, jeast);

            BOARD(outboard, i, j) = alivep(neighbor_count, BOARD (inboard, i, j));
        }
    }*/


    //printf("returning from thread_board\n");
    return;
}

/*****************************************************************************
 * Thread entry function
 ****************************************************************************/
void* thread_handler(void* thread_args){
    arguments* threadArgs = (arguments *) thread_args;
    char* outboard = threadArgs->outboard;
    char* inboard = threadArgs->inboard;
    const int srows = threadArgs->srows;
    const int scols = threadArgs->scols;
    const int nrows = threadArgs->nrows;
    const int nrowsmax = threadArgs->nrowsmax;
    const int ncols = threadArgs->ncols;
    const int ncolsmax = threadArgs->ncolsmax;
    const int gens_max = threadArgs->gens_max;
    thread_board(outboard,inboard, srows, scols, nrows, nrowsmax, ncols, ncolsmax, gens_max);
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
    arguments thread_args[4];
    for(int i = 0; i < 4; i++){
        thread_args[i].inboard = inboard;
        thread_args[i].outboard = outboard;
        thread_args[i].srows = 0;
        thread_args[i].scols = 0;
        thread_args[i].ncols = ncols;
        thread_args[i].nrows = nrows;
        thread_args[i].nrowsmax = nrows;
        thread_args[i].ncolsmax = ncols;
        thread_args[i].gens_max = gens_max;
    }

    for (curgen = 0; curgen < gens_max; curgen++) {
        //printf("Curgen count to watch for deadlock: %d\n", curgen);
        //printf("inboard and outboard addresses: %p, %p\n", inboard, outboard);

        /*
         * MultiThreaded
         */
        pthread_t test_thread1;
        pthread_t test_thread2;
        pthread_t test_thread3;
        pthread_t test_thread4;

        for(int i = 0; i < 4; i++){
            thread_args[i].inboard = inboard;
            thread_args[i].outboard = outboard;
        }

        //Top Row

        //Middle Row 1

        //Middle Row 2

        //Bottom Row
        thread_args[0].srows = 0;
        thread_args[0].scols = 0;
        thread_args[0].nrows = nrows/2;
        thread_args[0].ncols = ncols/2;
        pthread_create(&test_thread1, NULL, thread_handler, &thread_args[0]);

        thread_args[1].srows = nrows/2;
        thread_args[1].scols = 0;
        thread_args[1].nrows = nrows;
        thread_args[1].ncols = ncols/2;
        pthread_create(&test_thread2, NULL, thread_handler, &thread_args[1]);

        thread_args[2].srows = 0;
        thread_args[2].scols = ncols/2;
        thread_args[2].nrows = nrows/2;
        thread_args[2].ncols = ncols;
        pthread_create(&test_thread3, NULL, thread_handler, &thread_args[2]);

        thread_args[3].srows = nrows/2;
        thread_args[3].scols = ncols/2;
        thread_args[3].nrows = nrows;
        thread_args[3].ncols = ncols;
        pthread_create(&test_thread4, NULL, thread_handler, &thread_args[3]);

        pthread_join(test_thread1, NULL);
        pthread_join(test_thread2, NULL);
        pthread_join(test_thread3, NULL);
        pthread_join(test_thread4, NULL);
        SWAP_BOARDS(outboard, inboard);


        /*
         * Sequential
         */
        /*thread_args.inboard = inboard;
        thread_args.outboard = outboard;
        thread_args.srows = 0;
        thread_args.nrows = nrows/2;
        thread_handler(&thread_args);
        thread_args.srows = nrows/2;
        thread_args.nrows = nrows;
        thread_handler(&thread_args);
        SWAP_BOARDS(outboard, inboard);*/
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

