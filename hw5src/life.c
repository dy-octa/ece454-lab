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
    arguments thread_args[16];
    for(int i = 0; i < 16; i++){
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
        pthread_t test_thread5;
        pthread_t test_thread6;
        pthread_t test_thread7;
        pthread_t test_thread8;
        pthread_t test_thread9;
        pthread_t test_thread10;
        pthread_t test_thread11;
        pthread_t test_thread12;
        pthread_t test_thread13;
        pthread_t test_thread14;
        pthread_t test_thread15;
        pthread_t test_thread16;

        for(int i = 0; i < 16; i++){
            thread_args[i].inboard = inboard;
            thread_args[i].outboard = outboard;
        }

        //Top Row
        thread_args[0].srows = 0;
        thread_args[0].scols = 0;
        thread_args[0].nrows = nrows/4;
        thread_args[0].ncols = ncols/4;
        pthread_create(&test_thread1, NULL, thread_handler, &thread_args[0]);
        thread_args[1].srows = 0;
        thread_args[1].scols = ncols/4;
        thread_args[1].nrows = nrows/4;
        thread_args[1].ncols = ncols/2;
        pthread_create(&test_thread2, NULL, thread_handler, &thread_args[1]);
        thread_args[2].srows = 0;
        thread_args[2].scols = ncols/2;
        thread_args[2].nrows = nrows/4;
        thread_args[2].ncols = (3*ncols)/4;
        pthread_create(&test_thread3, NULL, thread_handler, &thread_args[2]);
        thread_args[3].srows = 0;
        thread_args[3].scols = (3*ncols)/4;
        thread_args[3].nrows = nrows/4;
        thread_args[3].ncols = ncols;
        pthread_create(&test_thread4, NULL, thread_handler, &thread_args[3]);

        //Middle Row 1
        thread_args[4].srows = nrows/4;
        thread_args[4].scols = 0;
        thread_args[4].nrows = nrows/2;
        thread_args[4].ncols = ncols/4;
        pthread_create(&test_thread5, NULL, thread_handler, &thread_args[4]);
        thread_args[5].srows = nrows/4;
        thread_args[5].scols = ncols/4;
        thread_args[5].nrows = nrows/2;
        thread_args[5].ncols = ncols/2;
        pthread_create(&test_thread6, NULL, thread_handler, &thread_args[5]);
        thread_args[6].srows = nrows/4;
        thread_args[6].scols = ncols/2;
        thread_args[6].nrows = nrows/2;
        thread_args[6].ncols = (3*ncols)/4;
        pthread_create(&test_thread7, NULL, thread_handler, &thread_args[6]);
        thread_args[7].srows = nrows/4;
        thread_args[7].scols = (3*ncols)/4;
        thread_args[7].nrows = nrows/2;
        thread_args[7].ncols = ncols;
        pthread_create(&test_thread8, NULL, thread_handler, &thread_args[7]);

        //Middle Row 2
        thread_args[8].srows = nrows/2;
        thread_args[8].scols = 0;
        thread_args[8].nrows = (3*nrows)/4;
        thread_args[8].ncols = ncols/4;
        pthread_create(&test_thread9, NULL, thread_handler, &thread_args[8]);
        thread_args[9].srows = nrows/2;
        thread_args[9].scols = ncols/4;
        thread_args[9].nrows = (3*nrows)/4;
        thread_args[9].ncols = ncols/2;
        pthread_create(&test_thread10, NULL, thread_handler, &thread_args[9]);
        thread_args[10].srows = nrows/2;
        thread_args[10].scols = ncols/2;
        thread_args[10].nrows = (3*nrows)/4;
        thread_args[10].ncols = (3*ncols)/4;
        pthread_create(&test_thread11, NULL, thread_handler, &thread_args[10]);
        thread_args[11].srows = nrows/2;
        thread_args[11].scols = (3*ncols)/4;
        thread_args[11].nrows = (3*nrows)/4;
        thread_args[11].ncols = ncols;
        pthread_create(&test_thread12, NULL, thread_handler, &thread_args[11]);

        //Bottom Row
        thread_args[12].srows = (3*nrows)/4;
        thread_args[12].scols = 0;
        thread_args[12].nrows = nrows;
        thread_args[12].ncols = ncols/4;
        pthread_create(&test_thread13, NULL, thread_handler, &thread_args[12]);
        thread_args[13].srows = (3*nrows)/4;
        thread_args[13].scols = ncols/4;
        thread_args[13].nrows = nrows;
        thread_args[13].ncols = ncols/2;
        pthread_create(&test_thread14, NULL, thread_handler, &thread_args[13]);
        thread_args[14].srows = (3*nrows)/4;
        thread_args[14].scols = ncols/2;
        thread_args[14].nrows = nrows;
        thread_args[14].ncols = (3*ncols)/4;
        pthread_create(&test_thread15, NULL, thread_handler, &thread_args[14]);
        thread_args[15].srows = (3*nrows)/4;
        thread_args[15].scols = (3*ncols)/4;
        thread_args[15].nrows = nrows;
        thread_args[15].ncols = ncols;
        pthread_create(&test_thread16, NULL, thread_handler, &thread_args[15]);

        pthread_join(test_thread1, NULL);
        pthread_join(test_thread2, NULL);
        pthread_join(test_thread3, NULL);
        pthread_join(test_thread4, NULL);
        pthread_join(test_thread5, NULL);
        pthread_join(test_thread6, NULL);
        pthread_join(test_thread7, NULL);
        pthread_join(test_thread8, NULL);
        pthread_join(test_thread9, NULL);
        pthread_join(test_thread10, NULL);
        pthread_join(test_thread11, NULL);
        pthread_join(test_thread12, NULL);
        pthread_join(test_thread13, NULL);
        pthread_join(test_thread14, NULL);
        pthread_join(test_thread15, NULL);
        pthread_join(test_thread16, NULL);
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

