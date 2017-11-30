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

typedef struct thread_args {
	char *outboard;
	char *inboard;
	int thread;
	int nrows;
	int ncols;
} arguments;

#define SWAP_BOARDS(b1, b2)  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD(__board, __i, __j)  (__board[(__i) + LDA*(__j)])

#define BLOCKWIDTH 512
#define BLOCKHEIGHT 128
#define BOARDWIDTH 1024
#define BOARDHEIGHT 1024
#define STEPHEIGHT 4
#define BYTEOF(b, x, y) ((b) + ((x)<<7) + ((y)>>8)) // Byte of (x, y) in a 1K*1K board
#define SETBIT(b, v, y) ((b) & (255^(1<<(y))) | ((v)<<(y))) // Return b'[y] with b[y] set to v
#define SETCELL(b, x, y, v) (*BYTEOF(b, x, y) = SETBIT(*BYTEOF(b, x, y), v, (y)&7)) // Set cell (x, y) in 1K*1K board to v
#define TESTCELL(b, x, y) ((*BYTEOF(b, x, y)) >> ((y)&7)) // Test cell (x, y) in 1K*1K board

#define BORDERBYTEOF(b, y) ((b) + ((y)>>8)) // Byte of y in a 1K border
#define SETBORDER(b, y, v) ((*BORDERBYTEOF(b, y)) = SETBIT(*BORDERBYTEOF(b, y), v, (y)&7)) // Set cell y in 1K border to v
#define TESTBORDER(b, y) ((*BORDERBYTEOF(b, y)) >> (y&7)) // Test y in 1K border

void initialize_board(const char inboard[BOARDHEIGHT][BOARDWIDTH], char packed[2][BOARDHEIGHT / 8][BOARDWIDTH / 8],
                      char *borders[4][BOARDHEIGHT]) {
	int i, j;
	char *ptr = &packed;
	for (i = 0; i < BOARDHEIGHT; ++i)
		for (j = 0; j < BOARDWIDTH; j += 8) {
			*(ptr++) = inboard[i][j] | (inboard[i][j + 1] << 1) | (inboard[i][j + 2] << 2) | (inboard[i][j + 3] << 3) |
			           (inboard[i][j + 4] << 4) | (inboard[i][j + 5] << 5) | (inboard[i][j + 6] << 6) |
			           (inboard[i][j + 7] << 7);
			if (j == 0)
				SETBORDER(borders[0], i, inboard[i][0]);
			else if (j == BLOCKWIDTH - 8)
				SETBORDER(borders[1], i, inboard[i][BLOCKWIDTH - 1]);
			else if (j == BLOCKWIDTH)
				SETBORDER(borders[2], i, inboard[i][BLOCKWIDTH]);
			else if (j == BOARDWIDTH - 8)
				SETBORDER(borders[3], i, inboard[i][BOARDWIDTH - 1]);
		}

}

void
initialize_active(const char *inboard, short list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], const int srows, const int scols) {
	for (int i = srows; i < srows + BLOCKHEIGHT; ++i) {
		for (int j = scols; j < scols + BLOCKWIDTH; ++j)
			*(list++) = j;
		*(list++) = -1;
	}
}

void
transfer_boards(const char fromboard[BOARDHEIGHT / 8][BOARDWIDTH / 8], char toboard[BOARDHEIGHT / 8][BOARDWIDTH / 8],
                short *changelist, int len, const int srows, const int scols) {
	int row = srows;
	for (int i = 0; i < len; ++i) {
		for (; in_list[i] == -1; ++i, ++row);
		int col = in_list[i];
		*BYTEOF(toboard, row, col) = *BYTEOF(fromboard, row, col);
	}
}

void
board_step(int n_itr, const char inboard[BOARDHEIGHT / 8][BOARDWIDTH / 8], const char *in_left, const char *in_right,
           char outboard[BOARDHEIGHT / 8][BOARDWIDTH / 8], char *out_left, char *out_right, const int srows,
           const int scols,
           const short *in_list, int in_list_len,
           short *out_list, int *out_list_len,
           short *left_change, short *right_change, short *up_change, short *down_change,
           const short *temp_changelist, short mark[BOARDHEIGHT][BOARDWIDTH], short *low_defer) {
	int row = srows, col;
	int cnt = 0;
	int lcc, rcc, ucc, dcc, occ = 0, ldc = 0;
	lcc = rcc = ucc = dcc = 1;
	for (int i = 0; i < in_list_len; ++i) {
		for (; in_list[i] == -1; ++i, ++row) {
			if (tcc > 0) {
				if (row == srows) {
					memcpy(up_change + 1, temp_changelist, sizeof(short) * tcc);
					up_change[0] = tcc;
				} else if (row == srows + BLOCKHEIGHT - 1) {
					memcpy(down_change + 1, temp_changelist, sizeof(short) * tcc);
					down_change[0] = tcc;
				}
				if (temp_changelist[0] == scols) {
					SETBORDER(out_left, row, 0);
					left_change[lcc++] = row;
				}
				if (temp_changelist[tcc - 1] == scols + BLOCKWIDTH - 1) {
					SETBORDER(out_right, row, 0);
					right_change[rcc++] = row;
				}
				if (row != srows) {
					for (int k = 0; k < tcc; ++k) {
						col = temp_changelist[k];
						if (col != scols)
							if (mark[row - 1][col - 1] != n_itr) {
								mark[row - 1][col - 1] = n_itr;
								out_list[occ++] = col - 1;
							}
						if (mark[row - 1][col] != n_itr) {
							mark[row - 1][col] = n_itr;
							out_list[occ++] = col;
						}
						if (col != scols + BOARDWIDTH - 1)
							if (mark[row - 1][col + 1] != n_itr) {
								mark[row - 1][col + 1] = n_itr;
								out_list[occ++] = col + 1;
							}
					}
					out_list[occ++] = -1;
				}
				int rightmost = 0;
				for (int k = 0; k < tcc; ++k) {
					col = temp_changelist[k];
					if (col != scols)
						if (col - 1 > rightmost) {
							rightmost = col - 1;
							out_list[occ++] = col - 1;
							mark[row][col - 1] = n_itr;
						}
					if (col > rightmost) {
						rightmost = col;
						out_list[occ++] = col;
						mark[row][col] = n_itr;
					}
					if (col != scols + BOARDWIDTH - 1)
						if (col + 1 > rightmost) {
							rightmost = col + 1;
							out_list[occ++] = col;
							mark[row][col + 1] = n_itr;
						}
				}
				while (ldc > 0) {
					col = low_defer[--ldc];
					if (mark[row][ldc] != n_itr) {
						mark[row][ldc] = n_itr;
						out_left[occ++] = col;
					}
				}
				ldc = 0;
				for (int k = 0; k < tcc; ++k) {
					col = temp_changelist[k];
					if (col != scols)
						low_defer[ldc++] = col - 1;
					low_defer[ldc++] = col;
					if (col != scols + BOARDWIDTH - 1)
						low_defer[ldc++] = col + 1;
				}
			}
		}
		col = in_list[i];
		int cnt = 0;
		if (col == scols) {
			cnt += TESTBORDER(in_left, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1));
			cnt += TESTBORDER(in_left, row);
			cnt += TESTBORDER(in_left, (row + 1) & (BOARDHEIGHT - 1));
		} else {
			cnt += TESTCELL(inboard, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1), col - 1);
			cnt += TESTCELL(inboard, row, col - 1);
			cnt += TESTCELL(inboard, (row + 1) & (BOARDHEIGHT - 1), col - 1);
		}

		cnt += TESTCELL(inboard, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1), col);
		cnt += TESTCELL(inboard, (row + 1) & (BOARDHEIGHT - 1), col);


		if (col == scols + BLOCKWIDTH - 1) {
			cnt += TESTBORDER(in_right, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1));
			cnt += TESTBORDER(in_right, row);
			cnt += TESTBORDER(in_right, (row + 1) & (BOARDHEIGHT - 1));
		} else {
			cnt += TESTCELL(inboard, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1), col + 1);
			cnt += TESTCELL(inboard, row, col + 1);
			cnt += TESTCELL(inboard, (row + 1) & (BOARDHEIGHT - 1), col + 1);
		}

		if (TESTCELL(inboard, row, col)) {
			if (cnt < 2 || cnt > 3) {
				SETCELL(outboard, row, col, 0);
				temp_changelist[tcc++] = col;
			}
		} else {
			if (cnt == 3) {
				SETCELL(outboard, row, cnt == 3);
				temp_changelist[tcc++] = col;
			}
		}
	}
}

/*****************************************************************************
 * Game of Life processing thread. Only processes a particular block of the
 * entire game board.
 ****************************************************************************/
void thread_board(char *outboard,
                  char *inboard,
                  const int srows,
                  const int scols,
                  const int nrows,
                  const int nrowsmax,
                  const int ncols,
                  const int ncolsmax) {
	int i, j, i1, j1;
	int LDA = nrowsmax;
	int inorth, isouth, jwest, jeast, inorth2, isouth2, jwest2, jeast2;
	char neighbor_count, neighbor_count2;

	for (i = srows; i < nrows; i += 32) {
		for (j = scols; j < ncols; j += 64) {
			//128bit rows * 512bit columns tiles
			for (i1 = i; i1 < i + 32; i1++) {
				inorth = mod(i1 - 1, nrowsmax);
				isouth = mod(i1 + 1, nrowsmax);
				inorth2 = mod((i1) - 1, nrowsmax);
				isouth2 = mod((i1) + 1, nrowsmax);
				for (j1 = j; j1 < ((j + 64) - 1); j1 += 2) {
					neighbor_count = BOARD (inboard, inorth, j1) +
					                 BOARD (inboard, isouth, j1);
					neighbor_count2 = BOARD (inboard, inorth2, (j1 + 1)) +
					                  BOARD (inboard, isouth2, (j1 + 1));

					jwest = mod(j1 - 1, ncolsmax);
					jeast = mod(j1 + 1, ncolsmax);
					jwest2 = mod((j1 + 1) - 1, ncolsmax);
					jeast2 = mod((j1 + 1) + 1, ncolsmax);

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
					BOARD(outboard, i1, (j1 + 1)) = alivep(neighbor_count2, BOARD (inboard, i1, (j1 + 1)));
				}
			}
		}
	}
	return;
}

/*****************************************************************************
 * Thread entry function
 ****************************************************************************/
void *thread_handler(void *thread_args) {
	int srows;
	int scols;

	arguments *threadArgs = (arguments *) thread_args;
	char *outboard = threadArgs->outboard;
	char *inboard = threadArgs->inboard;
	int nrows = threadArgs->nrows;
	const int nrowsmax = threadArgs->nrows;
	int ncols = threadArgs->ncols;
	const int ncolsmax = threadArgs->ncols;

	switch (threadArgs->thread) {
		case 0:
			srows = 0;
			scols = 0;
			nrows = nrows / 4;
			ncols = ncols / 4;
			break;

		case 1:
			srows = 0;
			scols = ncols / 4;
			nrows = nrows / 4;
			ncols = ncols / 2;
			break;

		case 2:
			srows = 0;
			scols = ncols / 2;
			nrows = nrows / 4;
			ncols = (3 * ncols) / 4;
			break;

		case 3:
			srows = 0;
			scols = (3 * ncols) / 4;
			nrows = nrows / 4;
			ncols = ncols;
			break;

		case 4:
			srows = nrows / 4;
			scols = 0;
			nrows = nrows / 2;
			ncols = ncols / 4;
			break;

		case 5:
			srows = nrows / 4;
			scols = ncols / 4;
			nrows = nrows / 2;
			ncols = ncols / 2;
			break;

		case 6:
			srows = nrows / 4;
			scols = ncols / 2;
			nrows = nrows / 2;
			ncols = (3 * ncols) / 4;
			break;

		case 7:
			srows = nrows / 4;
			scols = (3 * ncols) / 4;
			nrows = nrows / 2;
			ncols = ncols;
			break;

		case 8:
			srows = nrows / 2;
			scols = 0;
			nrows = (3 * nrows) / 4;
			ncols = ncols / 4;
			break;

		case 9:
			srows = nrows / 2;
			scols = ncols / 4;
			nrows = (3 * nrows) / 4;
			ncols = ncols / 2;
			break;

		case 10:
			srows = nrows / 2;
			scols = ncols / 2;
			nrows = (3 * nrows) / 4;
			ncols = (3 * ncols) / 4;
			break;

		case 11:
			srows = nrows / 2;
			scols = (3 * ncols) / 4;
			nrows = (3 * nrows) / 4;
			ncols = ncols;
			break;

		case 12:
			srows = (3 * nrows) / 4;
			scols = 0;
			nrows = nrows;
			ncols = ncols / 4;
			break;

		case 13:
			srows = (3 * nrows) / 4;
			scols = ncols / 4;
			nrows = nrows;
			ncols = ncols / 2;
			break;

		case 14:
			srows = (3 * nrows) / 4;
			scols = ncols / 2;
			nrows = nrows;
			ncols = (3 * ncols) / 4;
			break;

		case 15:
			srows = (3 * nrows) / 4;
			scols = (3 * ncols) / 4;
			nrows = nrows;
			ncols = ncols;
			break;

		default:
			return;
	}

	thread_board(outboard, inboard, srows, scols, nrows, nrowsmax, ncols, ncolsmax);
	return;
}

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/

char *multi_game_of_life(char *outboard,
                         char *inboard,
                         const int nrows,
                         const int ncols,
                         const int gens_max) {

	/* HINT: in the parallel decomposition, LDA may not be equal to
       nrows! */
	int curgen;
	arguments thread_args[16];
	for (int i = 0; i < 16; i++) {
		thread_args[i].nrows = nrows;
		thread_args[i].ncols = ncols;
		thread_args[i].thread = i;
	}

	pthread_t test_thread[16];
	for (curgen = 0; curgen < gens_max; curgen++) {
		for (int i = 0; i < 16; i++) {
			thread_args[i].outboard = outboard;
			thread_args[i].inboard = inboard;
			pthread_create(&test_thread[i], NULL, thread_handler, &thread_args[i]);
		}
		for (int j = 0; j < 16; j++) {
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


char *
game_of_life(char *outboard,
             char *inboard,
             const int nrows,
             const int ncols,
             const int gens_max) {
	return multi_game_of_life(outboard, inboard, nrows, ncols, gens_max);
}

