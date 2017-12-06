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
void check_list(short list[], int cnt, int srows, int scols, char *msg) {
	int i, last_row = -10, last_i = -1;
	for (i = 0; i < cnt; ++i) {
		if (list[i] < 0) {
			if (-list[i] - 1 <= last_row) {
				printf("%s: Non-ascending list in block (%d,%d), row %d @ %d, last %d @ %d\n",
				       msg, srows / 128, scols / 512, -list[i] - 1, i, last_row, last_i);
				while (1);
			}
			last_row = -list[i] - 1;
			last_i = i;
			if (i < cnt - 1 && list[i + 1] < 0) {
				printf("%s: Consecutive negative in block (%d,%d), row %d, next %d @ %d\n",
				       msg, srows / 128, scols / 512, -list[i] - 1, -list[i + 1] - 1, i);
				while (1);
			}
		} else if (i + 1 < cnt && list[i + 1] >= 0 && list[i + 1] <= list[i]) {
			printf("%s: Non-ascendinglist col in block (%d,%d), row %d, col: %d, %d, @ %d\n",
			       msg, srows / 128, scols / 512, last_row, list[i], list[i + 1], i);
			while (1);
		}
	}
}

void check_border(short list[], int len, int srows, int scols, char *msg) {
	int i;
	for (i = 0; i < len && list[i] != -1 && list[i + 1] != -1; ++i) {
		if (list[i] >= list[i + 1]) {
			printf("%s: Non-ascending border list in block (%d,%d), %d <= %d @ %d\n",
			       msg, srows / 128, scols / 512, list[i + 1], list[i], i);
			while (1);
		}
	}
}

void output_list(short list[], int len) {
	int i, row, alter = 0;
	for (i = 0; i < len; ++i) {
		if (list[i] < 0) {
			row = -list[i] - 1;
			++i;
		}
//		if (row >=231 && row <= 233)
			printf("(%d, %d)%s", row, list[i], (++alter) % 8 == 0 ? "\n" : " ");
	}
	if (alter % 8 != 0) printf("\n");
}

void output_border(short list[], int len) {
	int i, alter = 0;
	for (i = 0; i < len; ++i) {
		if (list[i] < 0)
			break;
//		if ((list[i] >= 304 && list[i] <= 309))
			printf("%d%s", list[i], (++alter) % 8 == 0 ? "\n" : " ");
	}
	if (alter % 8 != 0) printf("\n");
}

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
 * Active list: each element stores the col, record of a row starts with negative elements -row-1
 * Change list: each element stores the col, record of a row starts with negative elements -row-1
 * (There should be no empty row record)
 * Border Change list: terminated by -1 or the maximum number of elements (BLOCKWIDTH or BLOCKHEIGHT) when no -1 presents
 * Row and col are all in ascending order
*/

char corners[2][2];

#define BLOCKWIDTH 512
#define BLOCKHEIGHT 128
#define BOARDWIDTH 1024
#define BOARDHEIGHT 1024
#define N_THREADS 16

#define BYTEOF(b, x, y) ((char*)((b) + ((((x)<<10) + (y)) >> 3) )) // Byte of (x, y) in a 1K*1K board
#define SETBIT(b, v, y) (((255 ^ (1<<(y))) & (b))| ((v)<<(y))) // Return b'[y] with b[y] set to v, b is a byte value
#define SETCELL(b, x, y, v) (*BYTEOF(b, x, y) = SETBIT(*BYTEOF(b, x, y), v, (y)&7)) // Set cell (x, y) in 1K*1K board to v
#define TESTCELL(b, x, y) (((*BYTEOF(b, x, y)) >> ((y)&7))&1) // Test cell (x, y) in 1K*1K board

#define BORDERBYTEOF(b, y) ((char*)((b) + ((y)>>3))) // Byte of y in a 1K border
#define SETBORDER(b, y, v) ((*BORDERBYTEOF(b, y)) = SETBIT(*BORDERBYTEOF(b, y), v, (y)&7)) // Set cell y in 1K border to v
#define TESTBORDER(b, y) (((*BORDERBYTEOF(b, y)) >> ((y)&7))&1) // Test y in 1K border
#define max(a, b) (a>b?a:b)
#define min(a, b) (a<b?a:b)

/*****************************************************************************
 * Pack the board and border buffers, called before threads start
 ****************************************************************************/
void initialize_board(const char inboard[BOARDHEIGHT][BOARDWIDTH], char packed[2][BOARDHEIGHT * BOARDWIDTH / 8],
                      char borders[4][2][BOARDHEIGHT / 8]) {
	int i, j;
	char *ptr = packed;
	for (i = 0; i < BOARDHEIGHT; ++i) {
		for (j = 0; j < BOARDWIDTH; j += 8) {
			*(ptr++) = inboard[i][j] | (inboard[i][j + 1] << 1) | (inboard[i][j + 2] << 2) | (inboard[i][j + 3] << 3) |
			           (inboard[i][j + 4] << 4) | (inboard[i][j + 5] << 5) | (inboard[i][j + 6] << 6) |
			           (inboard[i][j + 7] << 7);
		}
		SETBORDER(borders[0][0], i, inboard[i][0]);
		SETBORDER(borders[1][0], i, inboard[i][BLOCKWIDTH - 1]);
		SETBORDER(borders[2][0], i, inboard[i][BLOCKWIDTH]);
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
int initialize_active(short list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], const int srows, const int scols) {
	int cnt = 0;
	for (int i = srows; i < srows + BLOCKHEIGHT; ++i) {
		list[cnt++] = -i - 1;
		for (int j = scols; j < scols + BLOCKWIDTH; ++j)
			list[cnt++] = j;
	}
	return cnt;
}

/*****************************************************************************
 * Copy the cells changed in the current iteration (written to the board)
 * to the board to be written in the next iteration
 * Called after each iteration in each thread
 ****************************************************************************/
void
transfer_boards(const char fromboard[BOARDHEIGHT * BOARDWIDTH / 8],
                char toboard[BOARDHEIGHT * BOARDWIDTH / 8],
                short *change_list, int change_cnt, const int srows, const int scols) {
	int row = srows;
	for (int i = 0; i < change_cnt; ++i) {
		if (change_list[i] < 0)
			row = -change_list[i++] - 1;
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
int board_step(const char inboard[BOARDHEIGHT * BOARDWIDTH / 8],
               char outboard[BOARDHEIGHT * BOARDWIDTH / 8],
               const int srows, const int scols,
               char self_left[BLOCKWIDTH], char self_right[BLOCKWIDTH],
               const char ext_left[BLOCKHEIGHT], const char ext_right[BLOCKHEIGHT],
               short left_change[BLOCKHEIGHT], short right_change[BLOCKHEIGHT],
               short up_change[BLOCKWIDTH], short down_change[BLOCKWIDTH],
               const short active_list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], int active_cnt,
               short change_list[BLOCKHEIGHT * (BLOCKWIDTH + 1)], char *self_corner, int n_itr) {
	int row = srows, col;
	int left_i, right_i, up_i, down_i, change_cnt;
	int newrow = 0, rightmost=scols - 1, offset=1;
//	if (n_itr == 1 && srows == BLOCKHEIGHT * 0 && scols == BLOCKWIDTH * 0)
//		printf(" ");
	left_i = right_i = up_i = down_i = change_cnt = 0;
	for (int i = 0; i < active_cnt; i += (offset == 1)) {
		if (active_list[i] < 0 && !(active_list[i] == -1 && i>0 && active_list[i-1] < 0)) {
			newrow = 1;
			row = -active_list[i++] - 1;
			rightmost = scols - 1;
		}
		if (offset == 1)
			offset = max(-1, rightmost - active_list[i] + 1);
		else ++offset;
		if (active_list[i] + offset >= scols + BLOCKWIDTH) {
			offset = 1;
			continue;
		}
		rightmost = col = active_list[i] + offset;

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

		int val = TESTCELL(inboard, row, col);
		if (val) {
			if (cnt < 2 || cnt > 3)
				SETCELL(outboard, row, col, 0);
			else continue;
		} else {
			if (cnt == 3)
				SETCELL(outboard, row, col, 1);
			else continue;
		}
		if (newrow) {
			change_list[change_cnt++] = -row - 1;
			newrow = 0;
		}
		change_list[change_cnt++] = col;
		if (col == scols) {
			SETBORDER(self_left, row, !val);
			left_change[left_i++] = row;
			if (row == srows)
				*self_corner |= SETBIT(*self_corner, !val, 0);
			else if (row == srows + BLOCKHEIGHT - 1)
				*self_corner |= SETBIT(*self_corner, !val, 2);
		} else if (col == scols + BLOCKWIDTH - 1) {
			SETBORDER(self_right, row, !val);
			right_change[right_i++] = row;
			if (row == srows)
				*self_corner |= SETBIT(*self_corner, !val, 1);
			else if (row == srows + BLOCKHEIGHT - 1)
				*self_corner |= SETBIT(*self_corner, !val, 3);
		}
		if (row == srows)
			up_change[up_i++] = col;
		else if (row == srows + BLOCKHEIGHT - 1)
			down_change[down_i++] = col;
	}
	if (left_i < BLOCKHEIGHT)
		left_change[left_i] = -1;
	if (right_i < BLOCKHEIGHT)
		right_change[right_i] = -1;
	if (up_i < BLOCKWIDTH)
		up_change[up_i] = -1;
	if (down_i < BLOCKWIDTH)
		down_change[down_i] = -1;
	// Border change lists are terminated by -1
//	check_list(change_list, change_cnt, srows, scols, "Change");
//	check_border(left_change, BLOCKHEIGHT, srows, scols, "Left change");
//	check_border(right_change, BLOCKHEIGHT, srows, scols, "Right change");
//	check_border(up_change, BLOCKWIDTH, srows, scols, "Up change");
//	check_border(down_change, BLOCKWIDTH, srows, scols, "Down change");
	return change_cnt;
}

/*
 * In the part of code below we generate the list of active cells based on the list of changed cells
 * We first generate the "midactives": for each changed cell (x, y), midactives are (x-1, y), (x, y) and (x+1, y)
 * The midactives are stored in temp_list in row-column order
 * Then we generate the results are store them to the change list
 *
 * On each row R, the midactives are the results of changed cells in R-1, R and R+1, plus the changed cells on external
 * borders and corners.
 * Since changed cells are stored in ascending order in change list, the midactives can be calculated out by a 3-way
 * merge, which is similar to the 2-way merge used in merge sort.
 */

/*****************************************************************************
 * Insert the cells to midactive list
 * These cells are on the upper or lower border
 ****************************************************************************/
int
ud_borders_midactive(short midactives[], int cnt, short change_list[], int change_cnt, int i1, int b1, int i2, int b2,
                     short ext_change[], char corner_l, char corner_r, int row, int scols) {
	int ie = 0, col;
	if (i1 >= b1 && i2 >= b2 && corner_l == 0 && corner_r == 0 && ext_change[0] == -1)
		return cnt;
	midactives[cnt++] = -row - 1;
	if (corner_l)
		midactives[cnt++] = scols-1;
	for (;;) {
		if ((i1 >= b1) && (i2 >= b2) && (ie == BLOCKWIDTH || ext_change[ie] < 0))
			break;
		col = BOARDWIDTH;
		if (i1 < b1 && change_list[i1] < col)
			col = change_list[i1];
		if (i2 < b2 && change_list[i2] < col)
			col = change_list[i2];
		if (ie < BLOCKWIDTH && ext_change[ie] >= 0 && ext_change[ie] < col)
			col = ext_change[ie];
		midactives[cnt++] = col;
		if (i1 < b1 && change_list[i1] == col)
			++i1;
		if (i2 < b2 && change_list[i2] == col)
			++i2;
		if (ie < BLOCKWIDTH && ext_change[ie] >= 0 && ext_change[ie] == col)
			++ie;
	}
	if (corner_r)
		midactives[cnt++] = scols + BLOCKWIDTH - 1;
	return cnt;
}

/*****************************************************************************
 * Insert the cells to midactive list
 * These cells are on some row in the middle
 ****************************************************************************/
int row_midactive(short midactives[], int cnt, short change_list[], int change_cnt, int i1, int b1, int i2, int b2,
                  int i3, int b3,
                  char left, char right, int row, int scols) {
	int col, c1, c2, c3;
	if (i1 >= b1 && i2 >= b2 && i3 >= b3 && left == 0 && right == 0)
		return cnt;
	midactives[cnt++] = -row - 1;
	if (left)
		midactives[cnt++] = scols-1;
	c1 = change_list[i1], c2 = change_list[i2], c3 = change_list[i3];
	for (; i1 < b1 || i2 < b2 || i3 < b3;) {
		col = BOARDWIDTH;
		if (i1 < b1)
			col = c1<col?c1:col;
		if (i2 < b2)
			col = c2<col?c2:col;
		if (i3 < b3)
			col = c3<col?c3:col;
		midactives[cnt++] = col;
		if (i1 < b1 && c1 == col)
			c1 = change_list[++i1];
		if (i2 < b2 && c2 == col)
			c2 = change_list[++i2];
		if (i3 < b3 && c3 == col)
			c3 = change_list[++i3];
	}
	if (right && midactives[cnt - 1] < scols + BLOCKWIDTH - 1)
		midactives[cnt++] = scols + BLOCKWIDTH;
	return cnt;
}

/*****************************************************************************
 * Build the active list based on midactives
 * Results are stored to active list
 * Return the size of the active list
 ****************************************************************************/
int generate_active_list(short midactives[], int ma_cnt, short active_list[], int scols) {
	int i, cnt = 0, col, rightmost = scols;
	for (i = 0; i < ma_cnt; ++i) {
		if (midactives[i] < 0 && !(i>0 && midactives[i] == -1 && midactives[i-1]<0)) {
			active_list[cnt++] = midactives[i++];
			rightmost = scols;
		}
		col = midactives[i];
		if (col - 1 >= rightmost) {
			active_list[cnt++] = col - 1;
			rightmost = col;
		}
		if (col < scols + BLOCKWIDTH && col >= rightmost) {
			active_list[cnt++] = col;
			rightmost = col + 1;
		}
		if (col + 1 < scols + BLOCKWIDTH && col + 1 >= rightmost) {
			active_list[cnt++] = col + 1;
			rightmost = col + 2;
		}
	}
	return cnt;
}

/*****************************************************************************
 * Build the active list for next iteration according to all the change lists
 * Results are stored back to change list
 * Return the size of the active list
 ****************************************************************************/
int build_activelist(short midactives[], short change_list[], int change_cnt,
                     short ext_left_change[], short ext_right_change[],
                     short ext_up_change[], short ext_down_change[], char *ext_corner[2],
                     int srows, int scols, int n_itr) {
	int i, up, down, down_b, row, cnt, left_i, right_i, left, right;
	cnt = left_i = right_i = 0;
	row = srows;

//	if (srows == 256) {
//		printf("in b_a scols=%d\n", scols);
//		output_border(ext_left_change, BOARDHEIGHT);
//		printf("\n");
//	}
//	if (srows == 256 && scols == 512)
//		printf(" ");
	// Upper border
	int i1, i2, i2_b;
	for (down = 1; change_list[down] >= 0 && down < change_cnt; ++down);
	if (down < change_cnt)
		for (down_b = down + 1; change_list[down_b] >= 0 && down_b < change_cnt; ++down_b);
	else down_b = down;
	if (-change_list[0] -1 == srows)
		i1 = 1;
	else i1 = down;
	if (-change_list[down] -1 == srows + 1)
		i2 = down+1;
	else if (-change_list[0]-1 == srows + 1)
		i2 = 1, down_b = down;
	else i2 = down_b;
	if (change_cnt == 0)
		i1 = down = i2 = down_b = 0;
	left = ((*ext_corner[0] >> 3) & 1) | (ext_left_change[0] != -1 && ext_left_change[0] <= srows + 1);
	right = ((*ext_corner[0] >> 2) & 1) | (ext_right_change[0] != -1 && ext_right_change[0] <= srows + 1);
	cnt = ud_borders_midactive(midactives, cnt, change_list, change_cnt, i1, down, i2, down_b, ext_up_change,
	                           left, right, srows, scols);

	// Initialize
	up = 0;
	for (i = up + 1; i < change_cnt && change_list[i] >= 0; ++i);
	if (i < change_cnt) {
		for (down = i + 1; down < change_cnt && change_list[down] >= 0; ++down);
		if (down < change_cnt)
			for (down_b = down + 1; down_b < change_cnt && change_list[down_b] >= 0; ++down_b);
		else down_b = change_cnt;
	} else down = down_b = change_cnt;
	row = srows + BLOCKHEIGHT - 1;
	if (ext_left_change[0] != -1)
		row = min(row, max(ext_left_change[0] - 1, srows + 1));
	if (ext_right_change[0] != -1)
		row = min(row, max(ext_right_change[0] - 1, srows + 1));
	row = min(row, max((-change_list[up] - 1) - 1, srows + 1));
	while (i < change_cnt && -change_list[i] - 1 < row) {
		up = i, i = down, down = down_b;
		if (down_b != change_cnt)
			for (++down_b; down_b < change_cnt && change_list[down_b] >= 0; ++down_b);
	}
	if (-change_list[up] - 1 >= row)
		down_b = down, down = i, i = up;
	if (change_cnt == 0)
		i = up = down = down_b = 0;

	// row: current row where we try to find the active cells
	// change_list[i..i_b): a row of changed cells, #row is the minimum one that >= row
	// [up..up_b), [down..down_b): intervals that is exactly the previous and next row of i in change list,
	// though they might not be contiguous in the board
	for (;;) {
//		if (srows == 128 && scols == 0 && row >= 229)
//			printf(" ");
		if (i == change_cnt &&
		    (left_i == BLOCKHEIGHT || ext_left_change[left_i] == -1) &&
		    (right_i == BLOCKHEIGHT || ext_right_change[right_i] == -1))
			break;
		if (row == srows + BLOCKHEIGHT - 1)
			break;

		left = left_i < BLOCKHEIGHT && ext_left_change[left_i] != -1 &&
		       ext_left_change[left_i] - 1 <= row && row <= ext_left_change[left_i] + 1;
		right = right_i < BLOCKHEIGHT && ext_right_change[right_i] != -1 &&
		        ext_right_change[right_i] - 1 <= row && row <= ext_right_change[right_i] + 1;
		int i1, i2, i3, i3_b;
		if (up != i && -change_list[up] - 1 == row - 1)
			i1 = up + 1;
		else i1 = i;
		if (i < change_cnt && -change_list[i] - 1 == row)
			i2 = i + 1;
		else i2 = down;
		if (down != down_b && -change_list[down] - 1 == row + 1)
			i3 = down + 1, i3_b = down_b;
		else i3 = i3_b = down_b;
		if (i < change_cnt && -change_list[i] - 1 == row + 1)
			i3 = i + 1, i3_b = down;
		cnt = row_midactive(midactives, cnt, change_list, change_cnt, i1, i, i2, down, i3, i3_b,
		                    left, right, row, scols);

		++row;
		while ((left_i < BLOCKHEIGHT && ext_left_change[left_i] != -1) && ext_left_change[left_i] + 1 < row)
			++left_i;
		while ((right_i < BLOCKHEIGHT && ext_right_change[right_i] != -1) && ext_right_change[right_i] + 1 < row)
			++right_i;
		while (i < change_cnt && -change_list[i] - 1 < row) {
			up = i, i = down, down = down_b;
			if (down_b != change_cnt)
				for (++down_b; down_b < change_cnt && change_list[down_b] >= 0; ++down_b);
		}

		int cand_row = srows + BLOCKHEIGHT - 1;
		if (left_i < BLOCKHEIGHT && ext_left_change[left_i] != -1) {
			cand_row = max(row, min(cand_row, ext_left_change[left_i] - 1));
			cand_row = max(row, min(cand_row, ext_left_change[left_i]));
			cand_row = max(row, min(cand_row, ext_left_change[left_i] + 1));
		}
		if (right_i < BLOCKHEIGHT && ext_right_change[right_i] != -1) {
			cand_row = max(row, min(cand_row, ext_right_change[right_i] - 1));
			cand_row = max(row, min(cand_row, ext_right_change[right_i]));
			cand_row = max(row, min(cand_row, ext_right_change[right_i] + 1));
		}
		if (i != change_cnt) {
			cand_row = max(row, min(cand_row, -change_list[i] - 1 - 1));
			cand_row = max(row, min(cand_row, -change_list[i] - 1));
		}
		if (up != i)
			cand_row = max(row, min(cand_row, -change_list[up] - 1 + 1));
	}
	// Lower row remained
	if (up != i && -change_list[up] - 1 >= row - 1
	    && -change_list[up] - 1 < srows + BLOCKHEIGHT - 2) { // Rows influenced by left and right border should be already used up
		row = -change_list[up] - 1 + 1;
		cnt = row_midactive(midactives, cnt, change_list, change_cnt, up + 1, i, i, i, i, i, 0, 0, row, scols);
	}

	//Lower border
	row = srows + BLOCKHEIGHT - 1;
	while (left_i < BLOCKHEIGHT && ext_left_change[left_i] != -1) ++left_i;
	while (right_i < BLOCKHEIGHT && ext_right_change[right_i] != -1) ++right_i;
	if (change_cnt > 0) {
		for (i = change_cnt - 1; change_list[i] >= 0 && i >= 0; --i);
		if (i > 0)
			for (up = i - 1; change_list[up] >= 0 && up >= 0; --up);
		else up = i;

		if (i < change_cnt && -change_list[i] - 1 == row)
			i2 = i + 1;
		else i2 = change_cnt;
		if (change_list[up] == -(row - 1) - 1)
			i1 = up + 1;
		else i1 = i;
		if (change_list[i] == -(row - 1) - 1)
			i2 = i + 1;
	}
	else i = up = i1 = i2 = 0;

	left = ((*ext_corner[1] >> 1) & 1) | (left_i > 0 && ext_left_change[left_i - 1] >= row - 1);
	right = ((*ext_corner[1] >> 0) & 1) | (left_i > 0 && ext_right_change[right_i - 1] >= row - 1);
	cnt = ud_borders_midactive(midactives, cnt, change_list, change_cnt, i1, i, i2, change_cnt, ext_down_change,
	                           left, right, row, scols);

//	check_list(midactives, cnt, srows, scols, "Mid");

//	cnt = generate_active_list(midactives, cnt, change_list, scols);

//	check_list(change_list, cnt, srows, scols, "Active");
	return cnt;
}

/*****************************************************************************
 * Print current alive cells for debugging
 ****************************************************************************/
void alive_cells(char board[BOARDHEIGHT * BOARDWIDTH / 8], int max_cnt) {
	int i, j;
	for (i = 0; i < BOARDHEIGHT; ++i)
		for (j = 0; j < BOARDWIDTH; ++j)
			if (TESTCELL(board, i, j)) {
				if (--max_cnt >= 0) {
					printf("(%d, %d)\n", i, j);
				} else return;
			}
}

/*****************************************************************************
 * Game of Life processing thread. Only processes a particular block of the
 * entire game board.
 ****************************************************************************/
pthread_mutex_t dbg_mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_board(char board[2][BOARDHEIGHT * BOARDWIDTH / 8],
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
	short *change_list = (short *) aligned_alloc(64, 2 * sizeof(short) * BLOCKHEIGHT * (BLOCKWIDTH + 1));
	short *active_list = (short *) ((char *) change_list + sizeof(short) * BLOCKHEIGHT * (BLOCKWIDTH + 1));
	int active_cnt, change_cnt;
	active_cnt = initialize_active(active_list, srows, scols);
	for (curgen = 0; curgen < gens_max; curgen++) {

//		pthread_mutex_lock(&dbg_mutex);
		change_cnt = board_step(board[cur], board[cur ^ 1], srows, scols,
		                        self_left[cur ^ 1], self_right[cur ^ 1], ext_left[cur], ext_right[cur],
		                        self_left_change, self_right_change, self_up_change, self_down_change,
		                        active_list, active_cnt, change_list, self_corner, curgen + 1);
//		printf("%d: Block (%d, %d) finish change\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//		if (srows == 0 && scols == 0) {
//			printf("%d: Block (%d, %d) finish change\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//			output_list(change_list, change_cnt);
//			output_border(self_up_change, BLOCKWIDTH);
//			printf("\n");
//		}
//		if (srows == 128 && scols == 0) {
//			printf("%d: Block (%d, %d) finish change\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//			output_list(change_list, change_cnt);
//			printf("\nDown:\n");
//			output_border(self_down_change, BLOCKWIDTH);
//			printf("\n");
//		}
//		if (srows == 0 && scols == 0) {
//			printf("%d: Block (%d, %d) finish change\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//			output_list(change_list, change_cnt);
//			printf("\n");
//		}
//		pthread_mutex_unlock(&dbg_mutex);

		pthread_mutex_lock(mutex);
		*done = *done + 1;
		if (*done < N_THREADS) {
			pthread_cond_wait(cond, mutex);
		}
		if (*done >= N_THREADS) {
			*done = 0;
			pthread_cond_broadcast(cond);
		}
		pthread_mutex_unlock(mutex);

//		pthread_mutex_lock(&dbg_mutex);
		transfer_boards(board[cur ^ 1], board[cur], change_list, change_cnt, srows, scols);
		transfer_borders(self_left[cur ^ 1], self_left[cur], self_left_change);
		transfer_borders(self_right[cur ^ 1], self_right[cur], self_right_change);
		active_cnt = build_activelist(active_list, change_list, change_cnt,
		                              ext_left_change, ext_right_change, ext_up_change, ext_down_change,
		                              ext_corner, srows, scols, curgen + 1);

//		short *tmp = active_list;
//		active_list = change_list;
//		change_list = tmp;
//		printf("%d: Block (%d, %d) finish update\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//		if (srows <= 128)
//			printf("Corner: %d %d %d %d\n", (*self_corner)&1, ((*self_corner)>>1)&1, ((*self_corner)>>2)&1, ((*self_corner)>>3)&1);
//		if (srows == 128 && scols == 0) {
//			printf("%d: Block (%d, %d) finish update\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//			printf("Ext Up:\n");
//			output_border(ext_up_change, BLOCKHEIGHT);
//			output_list(active_list, active_cnt);
//			printf("\n");
//		}
//		if (srows == 0 && scols == 0) {
//			printf("%d: Block (%d, %d) finish update\n", curgen, srows / BLOCKHEIGHT, scols / BLOCKWIDTH);
//			output_list(active_list, active_cnt);
//			printf("\n");
//		}
//		pthread_mutex_unlock(&dbg_mutex);

		pthread_mutex_lock(mutex);
		*done = *done + 1;
		if (*done < N_THREADS) {
			pthread_cond_wait(cond, mutex);
		}
		if (*done >= N_THREADS) {
			*done = 0;
			pthread_cond_broadcast(cond);
		}
		pthread_mutex_unlock(mutex);
		cur ^= 1;
	}
}

/*****************************************************************************
 * Thread entry function
 ****************************************************************************/
void *thread_handler(void *v_args) {
	arguments *threadArgs = (arguments *) v_args;
	thread_board(threadArgs->board, threadArgs->mutex, threadArgs->cond, threadArgs->done, threadArgs->srows,
	             threadArgs->scols, threadArgs->gens_max,
	             threadArgs->self_left, threadArgs->self_right, threadArgs->ext_left, threadArgs->ext_right,
	             threadArgs->self_left_change, threadArgs->self_right_change, threadArgs->self_up_change,
	             threadArgs->self_down_change,
	             threadArgs->ext_left_change, threadArgs->ext_right_change, threadArgs->ext_up_change,
	             threadArgs->ext_down_change, threadArgs->ext_corner, &threadArgs->self_corner);
	return NULL;
}

/*****************************************************************************
 * Extract a packed board to a normal char
 * -cell board
 ****************************************************************************/
void extract_board(char board[BOARDHEIGHT][BOARDWIDTH], char packed[BOARDHEIGHT * BOARDWIDTH / 8]) {
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
	// Board: [2][nrows][ncols] bits
	// Borders: 4 * [2][nrows] bits
	// Changelist: N_THREADS * 2 * udsize bits
	char *mem = aligned_alloc(64, 2 * nrows * ncols / 8 + 4 * 2 * nrows / 8 +
	                              (udsize + lrsize) * 2 * N_THREADS);
	char *packed_board = mem;
	char *borders[4] = {mem + 2 * nrows * ncols / 8, mem + 2 * nrows * ncols / 8 + 2 * nrows / 8,
	                    mem + 2 * nrows * ncols / 8 + 4 * nrows / 8, mem + 2 * nrows * ncols / 8 + 6 * nrows / 8};

	char *ud_changelist = mem + 2 * nrows * ncols / 8 + 4 * 2 * nrows / 8;
	char *lr_changelist = ud_changelist + udsize * 2 * N_THREADS;

	// Two borders array in each element
	initialize_board(inboard, packed_board, borders[0]);
	memset(corners, 0, sizeof(corners));
	arguments thread_args[N_THREADS];
	memset(thread_args, 0, sizeof(thread_args));
	for (int i = 0; i < N_THREADS; i++) {
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

		int down_index = ((i & 1 ? N_THREADS : 0) + (i / 2) * 2);
		int up_index = i < 2 ? down_index + 15 : down_index - 1;
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

	for (int i = 0; i < N_THREADS; ++i) {
		if (i < 2)
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
			thread_args[i].ext_corner[0] = &thread_args[(i - 3 + N_THREADS) & 15].self_corner;
			thread_args[i].ext_corner[1] = &thread_args[(i + 1) & 15].self_corner;
		} else {
			thread_args[i].ext_corner[0] = &thread_args[(i - 1 + N_THREADS) & 15].self_corner;
			thread_args[i].ext_corner[1] = &thread_args[(i + 3) & 15].self_corner;
		}
	}
	pthread_t test_thread[N_THREADS];
	for (int i = 0; i < N_THREADS; i++) {
		pthread_create(&test_thread[i], NULL, thread_handler, &thread_args[i]);
	}
	for (int j = 0; j < N_THREADS; j++) {
		pthread_join(test_thread[j], NULL);
	}
	extract_board(outboard, packed_board + (gens_max & 1 ? nrows * ncols / 8 : 0));
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

