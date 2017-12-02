/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include "life.h"
#include "util.h"
#include "stdlib.h"
#include "stdio.h"
#include <string.h>
#include "pthread.h"

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/

typedef struct thread_args {
	char *board;
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	int *done;
	int thread;
	int srows;
	int scols;
	int gens_max;
	char *self_left, *self_right;
	char *ext_left, *ext_right;
	short *self_left_change, *self_right_change, *self_up_change, *self_down_change;
	short *ext_left_change, *ext_right_change, *ext_up_change, *ext_down_change;
	char *ext_corner[2];
	char self_corner;
} arguments;
/*
 * Data structures:
 * Board: each bit stores the cell
 * Border: each bit stores the cell
 * Active list: each element stores the col, record of a row starts with negative elements -row
 * Change list: each element stores the col, record of a row starts with negative elements -row
 * (There should be no empty row record)
 * Border Change list: terminated by -1 or the maximum number of elements (BLOCKWIDTH or BLOCKHEIGHT) when no -1 presents
 * Unordered in each row, but group of rows are in ascending order
 * Hence left & right border change lists are in ascending order
*/

char corners[2][2];

#define BLOCKWIDTH 512
#define BLOCKHEIGHT 128
#define BOARDWIDTH 1024
#define BOARDHEIGHT 1024
#define N_THREADS 16

#define BYTEOF(b, x, y) ((char*)((b) + ((x)<<7) + ((y)>>8))) // Byte of (x, y) in a 1K*1K board
#define SETBIT(b, v, y) (255 & ((v)<<(y)) & (b)) // Return b'[y] with b[y] set to v, b is a byte value
#define SETCELL(b, x, y, v) (*BYTEOF(b, x, y) = SETBIT(*BYTEOF(b, x, y), v, (y)&7)) // Set cell (x, y) in 1K*1K board to v
#define TESTCELL(b, x, y) ((*BYTEOF(b, x, y)) >> ((y)&7)) // Test cell (x, y) in 1K*1K board

#define BORDERBYTEOF(b, y) ((char*)((b) + ((y)>>8))) // Byte of y in a 1K border
#define SETBORDER(b, y, v) ((*BORDERBYTEOF(b, y)) = SETBIT(*BORDERBYTEOF(b, y), v, (y)&7)) // Set cell y in 1K border to v
#define TESTBORDER(b, y) ((*BORDERBYTEOF(b, y)) >> (y&7)) // Test y in 1K border

/*****************************************************************************
 * Pack the board and border buffers, called before threads start
 ****************************************************************************/
void initialize_board(const char inboard[BOARDHEIGHT][BOARDWIDTH], char packed[2][(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)],
                      char borders[4][2][BOARDHEIGHT / 8]) {
	int i, j;
	char *ptr = &packed;
	for (i = 0; i < BOARDHEIGHT; ++i)
		for (j = 0; j < BOARDWIDTH; j += 8) {
			*(ptr++) = inboard[i][j] | (inboard[i][j + 1] << 1) | (inboard[i][j + 2] << 2) | (inboard[i][j + 3] << 3) |
			           (inboard[i][j + 4] << 4) | (inboard[i][j + 5] << 5) | (inboard[i][j + 6] << 6) |
			           (inboard[i][j + 7] << 7);
			if (j == 0)
				SETBORDER(borders[0][0], i, inboard[i][0]);
			else if (j == BLOCKWIDTH - 8)
				SETBORDER(borders[1][0], i, inboard[i][BLOCKWIDTH - 1]);
			else if (j == BLOCKWIDTH)
				SETBORDER(borders[2][0], i, inboard[i][BLOCKWIDTH]);
			else if (j == BOARDWIDTH - 8)
				SETBORDER(borders[3][0], i, inboard[i][BOARDWIDTH - 1]);
		}
	memcpy(packed[1], packed[0], sizeof(packed[0]));
	for (i = 0; i < 4; ++i)
		memcpy(borders[i][1], borders[i][0], sizeof(borders[i][0]));
}

/*****************************************************************************
 * Set active cells to all cells in the block
 * Called before first iteration in each thread
 ****************************************************************************/
void initialize_active(short list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], const int srows, const int scols) {
	for (int i = srows; i < srows + BLOCKHEIGHT; ++i) {
		*(list++) = -i;
		for (int j = scols; j < scols + BLOCKWIDTH; ++j)
			*(list++) = j;
	}
}

/*****************************************************************************
 * Copy the cells changed in the current iteration (written to the board)
 * to the board to be written in the next iteration
 * Called after each iteration in each thread
 ****************************************************************************/
void
transfer_boards(const char fromboard[(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)],
                char toboard[(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)],
                short *change_list, int change_cnt, const int srows, const int scols) {
	int row = srows;
	for (int i = 0; i < change_cnt; ++i) {
		if (change_list[i] < 0)
			row = -change_list[i++];
		int col = change_list[i];
		*BYTEOF(toboard, row, col) = *BYTEOF(fromboard, row, col);
	}
}

/*****************************************************************************
 * Copy the cells changed in the current iteration (written to the border buffers)
 * to the border buffers to be written in the next iteration
 * Called after each iteration in each thread
 ****************************************************************************/
void transfer_borders(const char from_border[BOARDHEIGHT], const char to_border[BOARDHEIGHT],
                      short border_change_list[BOARDHEIGHT]) {
	for (int i = 0; border_change_list[i] >= 0 && i < BOARDHEIGHT; ++i)
		*BORDERBYTEOF(to_border, border_change_list[i]) = *BORDERBYTEOF(from_border, border_change_list[i]);
}

/*****************************************************************************
 * Simulate the game for one step:
 * Check the active_list (list of cells that might change in this iteration)
 * Precondition:
 * active_list is the active list
 * Postcondition:
 * Write the updated version of cells to the outboard
 * Write the updated version of cells on the borders to the border buffer
 * Store cells that changed in this iteration
 * Store cells on the borders that are changed to border change_list
 ****************************************************************************/
int board_step(const char inboard[(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)],
               char outboard[(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)],
               const int srows, const int scols,
               char self_left[BLOCKWIDTH], char self_right[BLOCKWIDTH],
               const char ext_left[BLOCKHEIGHT], const char ext_right[BLOCKHEIGHT],
               short left_change[BLOCKHEIGHT], short right_change[BLOCKHEIGHT],
               short up_change[BLOCKWIDTH], short down_change[BLOCKWIDTH],
               const short active_list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], int active_cnt,
               short change_list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], char *self_corner) {
	int row = srows, col;
	int lcc, rcc, ucc, dcc, tcc;
	int newrow = 0;
	lcc = rcc = ucc = dcc = tcc = 0;
	for (int i = 0; i < active_cnt; ++i) {
		if (active_list[i] < 0) {
			newrow = 1;
			row = -active_list[i++];
		}
		col = active_list[i];
		int cnt = 0;
		if (col == scols) {
			cnt += TESTBORDER(ext_left, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1));
			cnt += TESTBORDER(ext_left, row);
			cnt += TESTBORDER(ext_left, (row + 1) & (BOARDHEIGHT - 1));
		} else {
			cnt += TESTCELL(inboard, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1), col - 1);
			cnt += TESTCELL(inboard, row, col - 1);
			cnt += TESTCELL(inboard, (row + 1) & (BOARDHEIGHT - 1), col - 1);
		}

		cnt += TESTCELL(inboard, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1), col);
		cnt += TESTCELL(inboard, (row + 1) & (BOARDHEIGHT - 1), col);


		if (col == scols + BLOCKWIDTH - 1) {
			cnt += TESTBORDER(ext_right, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1));
			cnt += TESTBORDER(ext_right, row);
			cnt += TESTBORDER(ext_right, (row + 1) & (BOARDHEIGHT - 1));
		} else {
			cnt += TESTCELL(inboard, (row + BOARDHEIGHT - 1) & (BOARDHEIGHT - 1), col + 1);
			cnt += TESTCELL(inboard, row, col + 1);
			cnt += TESTCELL(inboard, (row + 1) & (BOARDHEIGHT - 1), col + 1);
		}

		if (TESTCELL(inboard, row, col)) {
			if (cnt < 2 || cnt > 3) {
				SETCELL(outboard, row, col, 0);
				col = -col;
			}
		} else {
			if (cnt == 3) {
				SETCELL(outboard, row, col, 1);
				col = -col;
			}
		}
		if (col < 0) {
			col = -col;
			if (newrow) {
				change_list[tcc++] = -row;
				newrow = 0;
			}
			change_list[tcc++] = col;
			if (col == scols) {
				SETBORDER(self_left, row, 1);
				left_change[lcc++] = row;
				if (row == srows)
					*self_corner |= 1;
				else if (row == srows + BLOCKHEIGHT - 1)
					*self_corner |= 4;
			} else if (col == scols + BLOCKWIDTH - 1) {
				SETBORDER(self_right, row, 1);
				right_change[rcc++] = row;
				if (row == srows)
					*self_corner |= 2;
				else if (row == srows + BLOCKHEIGHT - 1)
					*self_corner |= 8;
			}
			if (row == srows)
				up_change[ucc++] = col;
			else if (row == srows + BLOCKHEIGHT - 1)
				down_change[ucc++] = col;
		}
	}
	if (lcc < BLOCKHEIGHT)
		left_change[lcc] = -1;
	if (rcc < BLOCKHEIGHT)
		right_change[rcc] = -1;
	if (ucc < BLOCKWIDTH)
		up_change[ucc] = -1;
	if (dcc < BLOCKWIDTH)
		down_change[dcc] = -1;
	// Border change lists are terminated by -1
	return tcc;
}

/*****************************************************************************
 * Insert the cells to active list
 * These cells are on the 'row', but affected by the changed cells on 'self_row'
 ****************************************************************************/
short mark[BOARDHEIGHT][BOARDWIDTH]; // To be eliminated

int change_list_mark(short active_list[], short change_list[], int change_cnt,
                     short ext_left_change[], short ext_right_change[],
                     int srows, int scols, int self_row, int row, int n_itr, int cnt, int i, int left_i, int right_i) {
	int col;
	if (i < change_cnt && self_row == -change_list[i]) {
		for (++i; i < change_cnt && change_list[i] >= 0; ++i) {
			col = change_list[i];

			if (col > scols && mark[row][col - 1] != n_itr) {
				mark[row][col - 1] = n_itr;
				active_list[cnt++] = col - 1;
			}

			if (mark[row][col] != n_itr) {
				mark[row][col] = n_itr;
				active_list[cnt++] = col;
			}


			if (col < scols + BLOCKWIDTH - 1 && mark[row][col + 1] != n_itr) {
				mark[row][col + 1] = n_itr;
				active_list[cnt++] = col + 1;
			}
		}
	}
	if (left_i < BLOCKHEIGHT && self_row == ext_left_change[left_i]) {
		if (mark[row][scols] != n_itr) {
			mark[row][scols] = n_itr;
			active_list[cnt++] = scols;
		}
	}
	if (right_i < BLOCKHEIGHT && self_row == ext_right_change[right_i]) {
		if (mark[row][scols + BLOCKWIDTH - 1] != n_itr) {
			mark[row][scols + BLOCKWIDTH - 1] = n_itr;
			active_list[cnt++] = scols + BLOCKWIDTH - 1;
		}
	}
	return cnt;
}

/*****************************************************************************
 * Build the active list for next iteration according to all the change lists
 * Return the size of the active list
 ****************************************************************************/
int build_activelist(short active_list[], short change_list[], int change_cnt,
                     short ext_left_change[], short ext_right_change[],
                     short ext_up_change[], short ext_down_change[], char *ext_corner[2],
                     int srows, int scols, int n_itr) {
	int i, row, col, cnt, left_i, right_i, last_row;
	int last_i, last_left_i, last_right_i;
	cnt = 0;
	row = srows;
	// Influences from upper border
	if (mark[srows][scols] != n_itr)
		if ((*ext_corner[0] >> 3) & 1) {
			mark[srows][scols] = n_itr;
			active_list[cnt++] = -row;
			active_list[cnt++] = scols;
		}
	for (i = 0; ext_up_change[i] >= 0 && i < BLOCKWIDTH; ++i) {
		col = ext_up_change[i];
		if (col > scols && mark[srows][col - 1] != n_itr) {
			mark[srows][col - 1] = n_itr;
			if (cnt == 0)
				active_list[cnt++] = -row;
			active_list[cnt++] = col - 1;
		}

		if (mark[srows][col] != n_itr) {
			mark[srows][col] = n_itr;
			if (cnt == 0)
				active_list[cnt++] = -row;
			active_list[cnt++] = col;
		}


		if (col < scols + BLOCKWIDTH && mark[srows][col + 1] != n_itr) {
			mark[srows][col + 1] = n_itr;
			active_list[cnt++] = col + 1;
		}
	}
	if (mark[srows][scols + BLOCKWIDTH - 1] != n_itr)
		if ((*ext_corner[0] >> 2) & 1) {
			mark[srows][scols + BLOCKWIDTH - 1] = n_itr;
			if (cnt == 0)
				active_list[cnt++] = -row;
			active_list[cnt++] = scols + BLOCKWIDTH - 1;
		}
	last_row = -1;

	for (i = left_i = right_i = 0;;) {
		row = BOARDHEIGHT;
		if (i < change_cnt && row < -change_list[i])
			row = -change_list[i];
		if (left_i < BLOCKHEIGHT && row < ext_left_change[left_i])
			row = ext_left_change[left_i];
		if (right_i < BLOCKHEIGHT && row < ext_right_change[right_i])
			row = ext_right_change[right_i];

		// When last row is far enough
		if (last_row <= row - 2 && last_row != -1) {
			active_list[cnt++] = -(last_row + 1);
			cnt = change_list_mark(active_list, change_list, change_cnt, ext_left_change, ext_right_change,
			                       srows, scols, last_row, last_row + 1, n_itr, cnt, last_i, last_left_i, last_right_i);
		}

		// Upper row
		if (row != srows) {
			if (!(last_row >= row - 2 && last_row != -1)) // When middle or lower row of last row overlap with upper row
				active_list[cnt++] = -(row - 1);
			cnt = change_list_mark(active_list, change_list, change_cnt, ext_left_change, ext_right_change,
			                       srows, scols, row, row - 1, n_itr, cnt, i, left_i, right_i);
		}

		// When last row is just before the current row
		if (last_row == row - 1 && last_row != -1) {
			active_list[cnt++] = -(last_row + 1);
			cnt = change_list_mark(active_list, change_list, change_cnt, ext_left_change, ext_right_change,
			                       srows, scols, last_row, last_row + 1, n_itr, cnt, last_i, last_left_i, last_right_i);

		}

		// Middle row
		if (!(last_row == row - 1 && last_row != -1))
			active_list[cnt++] = -row;
		cnt = change_list_mark(active_list, change_list, change_cnt, ext_left_change, ext_right_change,
		                       srows, scols, row, row, n_itr, cnt, i, left_i, right_i);

		last_row = row; // Lower row left for next iteration
		last_i = i, last_left_i = left_i, last_right_i = right_i;
		if (i < change_cnt && row == -change_list[i])
			for (; i < change_cnt && change_list[i] >= 0; ++i);
		if (left_i < BLOCKHEIGHT && row == ext_left_change[left_i])
			++left_i;
		if (right_i < BLOCKHEIGHT && row == ext_right_change[right_i])
			++right_i;
		if (i == change_cnt && (left_i == BLOCKHEIGHT || ext_left_change[left_i] == -1) &&
		    (right_i == BLOCKHEIGHT || ext_right_change[right_i] == -1))
			break;
	}
	// Lower row remained
	if (last_row != -1 && last_row != row + BLOCKHEIGHT - 1) {
		active_list[cnt++] = -(last_row + 1);
		cnt = change_list_mark(active_list, change_list, change_cnt, ext_left_change, ext_right_change,
		                       srows, scols, last_row, last_row + 1, n_itr, cnt, last_i, last_left_i, last_right_i);
	}

	int rec_cnt = cnt;
	row = srows + BLOCKHEIGHT - 1;
	if (last_row + 1 == row + BLOCKHEIGHT - 1)
		rec_cnt = -1;
	// Influences from lower border
	if (mark[srows][scols] != n_itr)
		if ((*ext_corner[1] >> 1) & 1) {
			mark[srows][scols] = n_itr;
			if (cnt == rec_cnt)
				active_list[cnt++] = -row;
			active_list[cnt++] = scols;
		}
	for (i = 0; ext_up_change[i] >= 0 && i < BLOCKWIDTH; ++i) {
		col = ext_up_change[i];
		if (col > scols && mark[srows][col - 1] != n_itr) {
			mark[srows][col - 1] = n_itr;
			if (cnt == rec_cnt)
				active_list[cnt++] = -row;
			active_list[cnt++] = col - 1;
		}

		if (mark[srows][col] != n_itr) {
			mark[srows][col] = n_itr;
			if (cnt == rec_cnt)
				active_list[cnt++] = -row;
			active_list[cnt++] = col;
		}


		if (col < scols + BLOCKWIDTH && mark[srows][col + 1] != n_itr) {
			mark[srows][col + 1] = n_itr;
			active_list[cnt++] = col + 1;
		}
	}
	if (mark[srows][scols + BLOCKWIDTH - 1] != n_itr)
		if (*ext_corner[1] & 1) {
			mark[srows][scols + BLOCKWIDTH - 1] = n_itr;
			if (cnt == rec_cnt)
				active_list[cnt++] = -row;
			active_list[cnt++] = scols + BLOCKWIDTH - 1;
		}
	return cnt;
}

/*****************************************************************************
 * Game of Life processing thread. Only processes a particular block of the
 * entire game board.
 ****************************************************************************/
void thread_board(char board[2][(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)],
                  pthread_mutex_t *mutex, pthread_cond_t *cond, int *done,
                  const int srows, const int scols, const int gens_max,
                  char self_left[2][BOARDHEIGHT / 8], char self_right[2][BOARDHEIGHT / 8],
                  char ext_left[2][BOARDHEIGHT / 8], char ext_right[2][BOARDHEIGHT / 8],
                  short self_left_change[BOARDHEIGHT], short self_right_change[BOARDHEIGHT],
                  short self_up_change[BOARDWIDTH], short self_down_change[BOARDWIDTH],
                  short ext_left_change[BOARDHEIGHT], short ext_right_change[BOARDHEIGHT],
                  short ext_up_change[BOARDWIDTH], short ext_down_change[BOARDWIDTH], char *ext_corner[2],
                  char *self_corner) {
	int curgen, cur = 0;
	short *change_list = (short*)aligned_alloc(64, sizeof(short) * BLOCKHEIGHT * (BLOCKWIDTH + 1));
	short *active_list = (short*)((char*)change_list + sizeof(short) * BLOCKHEIGHT * (BLOCKWIDTH + 1));
	int active_cnt = BLOCKHEIGHT * BLOCKWIDTH, change_cnt;
	initialize_active(change_list, srows, scols);

	for (curgen = 0; curgen < gens_max; curgen++) {
		*self_corner = 0;
		change_cnt = board_step(board[cur], board[cur ^ 1], srows, scols,
		                        self_left[cur ^ 1], self_right[cur ^ 1], ext_left[cur], ext_right[cur],
		                        self_left_change, self_right_change, self_up_change, self_down_change,
		                        active_list, active_cnt, change_list, self_corner);
		pthread_mutex_lock(mutex);
		*done = *done + 1;
		if (*done < 16) {
			pthread_cond_wait(cond, mutex);
		}
		if (*done >= 16) {
			*done = 0;
			pthread_cond_broadcast(cond);
		}
		pthread_mutex_unlock(mutex);

		transfer_boards(board[cur ^ 1], board[cur], change_list, change_cnt, srows, scols);
		transfer_borders(self_left[cur ^ 1], self_left[cur], self_left_change);
		transfer_borders(self_right[cur ^ 1], self_right[cur], self_right_change);
		active_cnt = build_activelist(active_list, change_list, change_cnt,
		                              ext_left_change, ext_right_change, ext_up_change, ext_down_change,
		                              ext_corner, srows, scols, curgen);

		pthread_mutex_lock(mutex);
		*done = *done + 1;
		if (*done < 16) {
			pthread_cond_wait(cond, mutex);
		}
		if (*done >= 16) {
			*done = 0;
			pthread_cond_broadcast(cond);
		}
		cur ^= 1;
		pthread_mutex_unlock(mutex);
	}
}

/*****************************************************************************
 * Thread entry function
 ****************************************************************************/
pthread_mutex_t dbg_mutex = PTHREAD_MUTEX_INITIALIZER;
void *thread_handler(void *v_args) {
	arguments *threadArgs = (arguments *) v_args;
	pthread_mutex_lock(&dbg_mutex);
	thread_board(threadArgs->board, threadArgs->mutex, threadArgs->cond, threadArgs->done, threadArgs->srows,
	             threadArgs->scols, threadArgs->gens_max,
	             threadArgs->self_left, threadArgs->self_right, threadArgs->ext_left, threadArgs->ext_right,
	             threadArgs->self_left_change, threadArgs->self_right_change, threadArgs->self_up_change,
	             threadArgs->self_down_change,
	             threadArgs->ext_left_change, threadArgs->ext_right_change, threadArgs->ext_up_change,
	             threadArgs->ext_down_change, threadArgs->ext_corner, &threadArgs->self_corner);
	pthread_mutex_unlock(&dbg_mutex);
	return NULL;
}

/*****************************************************************************
 * Extract a packed board to a normal char-cell board
 ****************************************************************************/
void extract_board(char board[BOARDHEIGHT][BOARDWIDTH], char packed[(BOARDHEIGHT / 8) * (BOARDWIDTH / 8)]) {
	int i, j;
	for (i = 0; i < BOARDHEIGHT; ++i)
		for (j = 0; j < BOARDWIDTH; ++j)
			board[i][j] = TESTCELL(packed, i, j);
}

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/

char *multi_game_of_life(char *outboard,
                         char *inboard,
                         const int nrows,
                         const int ncols,
                         const int gens_max) {


	int thread_done = 0;
	pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t thread_cond = PTHREAD_COND_INITIALIZER;
	size_t udsize = sizeof(short) * (BLOCKWIDTH);
	size_t lrsize = sizeof(short) * (BLOCKHEIGHT);
	char *mem = aligned_alloc(64, 2 * nrows * ncols / 64 + nrows * 4 / 8 +
	                              (udsize + lrsize) * 2 * 16);
	char *packed_board = mem;
	char *borders[4] = {mem + 2 * nrows * ncols / 64, mem + 2 * nrows * ncols / 64 + 2 * nrows / 8,
	                    mem + 2 * nrows * ncols / 64 + 4 * nrows / 8, mem + 2 * nrows * ncols / 64 + 6 * nrows / 8};

	char *ud_changelist = mem + 2 * nrows * ncols / 64 + nrows * 4 / 8;
	char *lr_changelist = ud_changelist + udsize * 2 * 16;

	// Two borders array in each element
	initialize_board(inboard, packed_board, borders[0]);
	memset(corners, 0, sizeof(corners));
	arguments thread_args[16];
	for (int i = 0; i < 16; i++) {
		thread_args[i].board = packed_board;
		thread_args[i].mutex = &thread_mutex;
		thread_args[i].cond = &thread_cond;
		thread_args[i].done = &thread_done;
		thread_args[i].thread = i;
		thread_args[i].gens_max = gens_max;

		if (i & 1) {
			thread_args[i].self_left = borders[2];
			thread_args[i].self_right = borders[3];
			thread_args[i].ext_left = borders[1];
			thread_args[i].ext_right = borders[0];
		} else {
			thread_args[i].self_left = borders[0];
			thread_args[i].self_right = borders[1];
			thread_args[i].ext_left = borders[3];
			thread_args[i].ext_right = borders[2];
		}
		thread_args[i].srows = (nrows / 8) * (i / 2);
		thread_args[i].scols = (i & 1) * (ncols / 2);

		int down_index = ((i & 1 ? 16 : 0) + (i / 2) * 2);
		int up_index = i < 2 ? down_index - 1 + 15 : down_index - 1;
		thread_args[i].self_up_change = ud_changelist + up_index * udsize;
		thread_args[i].self_down_change = ud_changelist + down_index * udsize;

		int left_index, right_index;
		if (i & 1) {
			left_index = 1 + (i / 2) * 4;
			right_index = left_index + 1;
		} else {
			left_index = 3 + (i / 2) * 4;
			right_index = left_index - 3;
		}
		thread_args[i].self_left_change = lr_changelist + left_index * lrsize;
		thread_args[i].self_right_change = lr_changelist + right_index * lrsize;
	}

	for (int i = 0; i < 16; ++i) {
		if (i <= 2)
			thread_args[i].ext_up_change = thread_args[i + 14].self_down_change;
		else thread_args[i].ext_up_change = thread_args[i - 2].self_down_change;
		if (i >= 14)
			thread_args[i].ext_down_change = thread_args[i - 14].self_up_change;
		else thread_args[i].ext_down_change = thread_args[i + 2].self_up_change;
		if (i & 1) {
			thread_args[i].ext_left_change = thread_args[i - 1].self_right_change;
			thread_args[i].ext_right_change = thread_args[i - 1].self_left_change;
		} else {
			thread_args[i].ext_left_change = thread_args[i + 1].self_right_change;
			thread_args[i].ext_right_change = thread_args[i + 1].self_left_change;
		}
		if (i & 1) {
			thread_args[i].ext_corner[0] = &thread_args[(i - 3 + 16) & 15].self_corner;
			thread_args[i].ext_corner[1] = &thread_args[(i + 1) & 15].self_corner;
		} else {
			thread_args[i].ext_corner[0] = &thread_args[(i - 1 + 16) & 15].self_corner;
			thread_args[i].ext_corner[1] = &thread_args[(i + 3) & 15].self_corner;
		}
	}

	pthread_t test_thread[16];
	for (int i = 0; i < 16; i++) {
		pthread_create(&test_thread[i], NULL, thread_handler, &thread_args[i]);
	}
	for (int j = 0; j < 16; j++) {
		pthread_join(test_thread[j], NULL);
	}
	extract_board(outboard, packed_board + (gens_max & 1 ? nrows * ncols / 64 : 0));
	return outboard;
}


char *
game_of_life(char *outboard,
             char *inboard,
             const int nrows,
             const int ncols,
             const int gens_max) {
	return multi_game_of_life(outboard, inboard, nrows, ncols, gens_max);
}

