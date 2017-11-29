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

typedef struct thread_args{
    char* outboard;
    char* inboard;
    pthread_mutex_t* mutex;
    pthread_cond_t* cond;
    int* done;
    int thread;
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
                  const int gens_max,
                  pthread_mutex_t* mutex,
                  pthread_cond_t* cond,
                  int* done){
    int i, j, i1, j1, curgen;
    int LDA = nrowsmax;
    int inorth, isouth, jwest, jeast, jwest2, jeast2;
    int jwest3, jeast3, jwest4, jeast4;
    char neighbor_count, neighbor_count2;
    char neighbor_count3, neighbor_count4;

    //printf("genmax is what: %d\n", gens_max);

    for (curgen = 0; curgen < gens_max; curgen++) {
        //printf("curgen?: %d\n", curgen);
        for (i = srows; i < nrows; i += 32) {
            for (j = scols; j < ncols; j += 64) {
                //128bit rows * 512bit columns tiles
                for (i1 = i; i1 < i + 32; i1++) {
                    inorth = mod(i1 - 1, nrowsmax);
                    isouth = mod(i1 + 1, nrowsmax);
                    for (j1 = j; j1 < ((j + 64) - 3); j1 += 4) {
                        jwest = mod(j1 - 1, ncolsmax);
                        jeast = mod(j1 + 1, ncolsmax);
                        jwest2 = mod((j1 + 1) - 1, ncolsmax);
                        jeast2 = mod((j1 + 1) + 1, ncolsmax);
                        jwest3 = mod((j1 + 2) - 1, ncolsmax);
                        jeast3 = mod((j1 + 2) + 1, ncolsmax);
                        jwest4 = mod((j1 + 3) - 1, ncolsmax);
                        jeast4 = mod((j1 + 3) + 1, ncolsmax);

                        neighbor_count =
                                BOARD (inboard, inorth, j1) +
                                BOARD (inboard, isouth, j1) +
                                BOARD (inboard, inorth, jwest) +
                                BOARD (inboard, inorth, jeast) +
                                BOARD (inboard, i1, jwest) +
                                BOARD (inboard, i1, jeast) +
                                BOARD (inboard, isouth, jwest) +
                                BOARD (inboard, isouth, jeast);
                        neighbor_count2 =
                                BOARD (inboard, inorth, (j1 + 1)) +
                                BOARD (inboard, isouth, (j1 + 1)) +
                                BOARD (inboard, inorth, jwest2) +
                                BOARD (inboard, inorth, jeast2) +
                                BOARD (inboard, i1, jwest2) +
                                BOARD (inboard, i1, jeast2) +
                                BOARD (inboard, isouth, jwest2) +
                                BOARD (inboard, isouth, jeast2);
                        neighbor_count3 =
                                BOARD (inboard, inorth, (j1 + 2)) +
                                BOARD (inboard, isouth, (j1 + 2)) +
                                BOARD (inboard, inorth, jwest3) +
                                BOARD (inboard, inorth, jeast3) +
                                BOARD (inboard, i1, jwest3) +
                                BOARD (inboard, i1, jeast3) +
                                BOARD (inboard, isouth, jwest3) +
                                BOARD (inboard, isouth, jeast3);
                        neighbor_count4 =
                                BOARD (inboard, inorth, (j1 + 3)) +
                                BOARD (inboard, isouth, (j1 + 3)) +
                                BOARD (inboard, inorth, jwest4) +
                                BOARD (inboard, inorth, jeast4) +
                                BOARD (inboard, i1, jwest4) +
                                BOARD (inboard, i1, jeast4) +
                                BOARD (inboard, isouth, jwest4) +
                                BOARD (inboard, isouth, jeast4);

                        BOARD(outboard, i1, j1) = alivep(neighbor_count, BOARD (inboard, i1, j1));
                        BOARD(outboard, i1, (j1 + 1)) = alivep(neighbor_count2, BOARD (inboard, i1, (j1 + 1)));
                        BOARD(outboard, i1, (j1 + 2)) = alivep(neighbor_count3, BOARD (inboard, i1, (j1 + 2)));
                        BOARD(outboard, i1, (j1 + 3)) = alivep(neighbor_count4, BOARD (inboard, i1, (j1 + 3)));
                    }
                }
            }
        }
        pthread_mutex_lock(mutex);
        *done = *done + 1;
        if( *done < 16 ) {
            /* block this thread until another thread signals cond. While
           blocked, the mutex is released, then re-aquired before this
           thread is woken up and the call returns. */
            pthread_cond_wait(cond, mutex);
        }
        if( *done >= 16){

            *done = 0;
            pthread_cond_broadcast(cond);
        }
        SWAP_BOARDS(outboard, inboard);
        pthread_mutex_unlock( mutex );
    }
    return;
}

/*****************************************************************************
 * Thread entry function
 ****************************************************************************/
void* thread_handler(void* thread_args){
    arguments* threadArgs = (arguments *) thread_args;
    char* outboard = threadArgs->outboard;
    char* inboard = threadArgs->inboard;
    int nrows = threadArgs->nrows;
    const int nrowsmax = threadArgs->nrowsmax;
    int ncols = threadArgs->ncols;
    const int ncolsmax = threadArgs->ncolsmax;
    const int srows = threadArgs->srows;
    const int scols = threadArgs->scols;
    const int gens_max = threadArgs->gens_max;
    pthread_mutex_t *mutex = threadArgs->mutex;
    pthread_cond_t *cond = threadArgs->cond;
    int* done = threadArgs->done;

    thread_board(outboard,inboard, srows, scols, nrows, nrowsmax, ncols, ncolsmax, gens_max, mutex, cond, done);
	return NULL;
}

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/

char* multi_game_of_life (char* outboard,
					char* inboard,
					const int nrows,
					const int ncols,
					const int gens_max){


    int thread_done = 0;
    pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t thread_cond = PTHREAD_COND_INITIALIZER;

    arguments thread_args[16];
    for(int i = 0; i < 16; i++){
        thread_args[i].outboard = outboard;
        thread_args[i].inboard = inboard;
        thread_args[i].mutex = &thread_mutex;
        thread_args[i].cond = &thread_cond;
        thread_args[i].done = &thread_done;
        thread_args[i].nrows = nrows;
        thread_args[i].ncols = ncols;
        thread_args[i].nrowsmax = nrows;
        thread_args[i].ncolsmax = ncols;
        thread_args[i].scols = 0;
        thread_args[i].srows = 0;
        thread_args[i].thread = i;
        thread_args[i].gens_max = gens_max;

        switch (i){
            case 0:
                thread_args[i].srows = 0;
                thread_args[i].scols = 0;
                thread_args[i].nrows = nrows/4;
                thread_args[i].ncols = ncols/4;
                break;
            case 1:
                thread_args[i].srows = 0;
                thread_args[i].scols = ncols/4;
                thread_args[i].nrows = nrows/4;
                thread_args[i].ncols = ncols/2;
                break;
            case 2:
                thread_args[i].srows = 0;
                thread_args[i].scols = ncols/2;
                thread_args[i].nrows = nrows/4;
                thread_args[i].ncols = (3*ncols)/4;
                break;
            case 3:
                thread_args[i].srows = 0;
                thread_args[i].scols = (3*ncols)/4;
                thread_args[i].nrows = nrows/4;
                thread_args[i].ncols = ncols;
                break;
            case 4:
                thread_args[i].srows = nrows/4;
                thread_args[i].scols = 0;
                thread_args[i].nrows = nrows/2;
                thread_args[i].ncols = ncols/4;
                break;
            case 5:
                thread_args[i].srows = nrows/4;
                thread_args[i].scols = ncols/4;
                thread_args[i].nrows = nrows/2;
                thread_args[i].ncols = ncols/2;
                break;
            case 6:
                thread_args[i].srows = nrows/4;
                thread_args[i].scols = ncols/2;
                thread_args[i].nrows = nrows/2;
                thread_args[i].ncols = (3*ncols)/4;
                break;
            case 7:
                thread_args[i].srows = nrows/4;
                thread_args[i].scols = (3*ncols)/4;
                thread_args[i].nrows = nrows/2;
                thread_args[i].ncols = ncols;
                break;
            case 8:
                thread_args[i].srows = nrows/2;
                thread_args[i].scols = 0;
                thread_args[i].nrows = (3*nrows)/4;
                thread_args[i].ncols = ncols/4;
                break;
            case 9:
                thread_args[i].srows = nrows/2;
                thread_args[i].scols = ncols/4;
                thread_args[i].nrows = (3*nrows)/4;
                thread_args[i].ncols = ncols/2;
                break;
            case 10:
                thread_args[i].srows = nrows/2;
                thread_args[i].scols = ncols/2;
                thread_args[i].nrows = (3*nrows)/4;
                thread_args[i].ncols = (3*ncols)/4;
                break;
            case 11:
                thread_args[i].srows = nrows/2;
                thread_args[i].scols = (3*ncols)/4;
                thread_args[i].nrows = (3*nrows)/4;
                thread_args[i].ncols = ncols;
                break;
            case 12:
                thread_args[i].srows = (3*nrows)/4;
                thread_args[i].scols = 0;
                thread_args[i].nrows = nrows;
                thread_args[i].ncols = ncols/4;
                break;
            case 13:
                thread_args[i].srows = (3*nrows)/4;
                thread_args[i].scols = ncols/4;
                thread_args[i].nrows = nrows;
                thread_args[i].ncols = ncols/2;
                break;
            case 14:
                thread_args[i].srows = (3*nrows)/4;
                thread_args[i].scols = ncols/2;
                thread_args[i].nrows = nrows;
                thread_args[i].ncols = (3*ncols)/4;
                break;
            case 15:
                thread_args[i].srows = (3*nrows)/4;
                thread_args[i].scols = (3*ncols)/4;
                thread_args[i].nrows = nrows;
                thread_args[i].ncols = ncols;
                break;
            default:
                break;
        }
    }

    pthread_t test_thread[16];

        for(int i = 0; i < 16; i++){
            pthread_create(&test_thread[i], NULL, thread_handler, &thread_args[i]);
        }
        for(int j = 0; j < 16; j++){
            pthread_join(test_thread[j], NULL);
        }

    //}
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

