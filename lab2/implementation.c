#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line
#define STRIDE 10
#define UNROLL16(S) {{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};{S};}
#define UNROLL8(S) {{S};{S};{S};{S};{S};{S};{S};{S};}
#define UNROLL4(S) {{S};{S};{S};{S};}

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          Do not forget to modify the team_name and team member information !!!
 **********************************************************************************************************************/
void print_team_info() {
	// Please modify this field with something interesting
	char team_name[] = "Cheesecake";

	// Please fill in your information
	char student1_first_name[] = "Andrei";
	char student1_last_name[] = "Patranoiu";
	char student1_student_number[] = "998130696";

	// Please fill in your partner's information
	// If yon't have partner, do not modify this
	char student2_first_name[] = "Mutian";
	char student2_last_name[] = "He";
	char student2_student_number[] = "1004654475";

	// Printing out team information
	printf("*******************************************************************************************************\n");
	printf("Team Information:\n");
	printf("\tteam_name: %s\n", team_name);
	printf("\tstudent1_first_name: %s\n", student1_first_name);
	printf("\tstudent1_last_name: %s\n", student1_last_name);
	printf("\tstudent1_student_number: %s\n", student1_student_number);
	printf("\tstudent2_first_name: %s\n", student2_first_name);
	printf("\tstudent2_last_name: %s\n", student2_last_name);
	printf("\tstudent2_student_number: %s\n", student2_student_number);
}

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          You can modify anything else in this file
 ***********************************************************************************************************************
 * @param sensor_values - structure stores parsed key value pairs of program instructions
 * @param sensor_values_count - number of valid sensor values parsed from sensor log file or commandline console
 * @param frame_buffer - pointer pointing to a buffer storing the imported  24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param grading_mode - turns off verification and turn on instrumentation
 ***********************************************************************************************************************
 *
 **********************************************************************************************************************/
typedef struct {
	int typeA;
	int B[2];
} Matrix;
char typeAMuls[] = {0, 1, 2, 3, 4, 5, 6, 7, 1, 0, 3, 2, 6, 7, 4, 5, 2, 3, 0, 1, 5, 4, 7, 6, 3, 2, 1, 0, 7, 6, 5, 4, 4,
                    5, 6, 7, 0, 1, 2, 3, 5, 4, 7, 6, 2, 3, 0, 1, 6, 7, 4, 5, 1, 0, 3, 2, 7, 6, 5, 4, 3, 2, 1, 0};

void multiMatrix(Matrix *A, Matrix *B, Matrix *C) {
	C->typeA = typeAMuls[(A->typeA)<<3 | (B->typeA)];
	if ((B->typeA >> 2) & 1) {
		C->B[0] = A->B[1];
		C->B[1] = A->B[0];
	}
	else {
		C->B[0] = A->B[0];
		C->B[1] = A->B[1];
	}
	if (B->typeA & 1)
		C->B[1] = -C->B[1];
	if ((B->typeA >> 1) & 1)
		C->B[0] = -C->B[0];
	C->B[0] += B->B[0];
	C->B[1] += B->B[1];
}
/*
 * All the transformation matrices can be represented as a 3*3 matrix M = [A 0; B 1], where A is a 2*2 matrix and B is a
 * 1*2 matrix.
 * A is a diagonal matrix or a anti-diagonal matrix, and the non-zero values are 1 or -1, like [-1 0; 0 1] or
 * [0 1; -1 0]. There're only 8 kinds of A-like matrices.
 * Inspect the multiplication results of a series of M: M_1 * M_2 = [A_1*A_2 0; B_1*A_2+B_2 1]. A_1 * A_2 is still an
 * A-like submatrix. The new B can be directly resolved from the type of A.
 * Therefore we can only use 3 numbers to represent a matrix: type of A and two values in B.
 */
// Standard A-types:
//{1 0; 0 1}
//{1 0; 0 -1}
//{-1 0; 0 1}
//{-1 0; 0 -1}
//{0 1; 1 0}
//{0 -1; 1 0}
//{0 1; -1 0}
//{0 -1; -1 0}
int testRow[300000];
int testCol[300000];
int testPixel[300000];
int renderedPos[300000];
void implementation_driver(struct kv *sensor_values, int sensor_values_count, unsigned char *frame_buffer,
                           unsigned int width, unsigned int height, bool grading_mode) {
	int processed_frames = 0;
	Matrix final_matrix[2] = {{0, 0, 0},
	                          {0, 0, 0}}; // {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {}};
	Matrix matUp = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {-1, 0, 1}};
	Matrix matDown = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {1, 0, 1}};
	Matrix matLeft = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {0, -1, 1}};
	Matrix matRight = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {0, 1, 1}};
	Matrix matRot[3] = {{5, 0,         width - 1},
	                    {3, width - 1, width - 1},
	                    {6, width - 1, 0}};
//						{{{0, -1, 0}, {1, 0, 0}, {0, width - 1, 1}},
//						 {{-1, 0, 0}, {0, -1, 0}, {width - 1, width - 1, 1}},
//						 {{0, 1, 0}, {-1, 0, 0}, {width - 1, 0, 1}}};
	Matrix matFlipX = {2, height - 1, 0}; // {{-1, 0, 0}, {0, 1, 0}, {height - 1, 0, 1}};
	Matrix matFlipY = {1, 0, width - 1}; //{{1, 0, 0}, {0, -1, 0}, {0, width - 1, 1}};
	int current_matrix = 0;

	//int testArray[10669];
	//int testRow[10669];
	//int testCol[10669];
	int i = 0;
	int resizeFlag = 299000;
	int width_3 = width * 3;

	int pos, row, col, size = height * width_3, cnt = 0;
	unsigned char* lastfp;
//	for (int row0 = 0; row0 < height; row0 += STRIDE)
//		for (int col0 = 0; col0 < width; col0 += STRIDE)
	row = col = 0;
	for (unsigned char* fp = frame_buffer; fp < frame_buffer + size; fp+=3) {
		if ((*((unsigned int *) fp) & 0xFFFFFF) != 16777215) {
			testPixel[i] = (*((unsigned int *) fp) & 0xFFFFFF) | (0xFF000000);
			if (fp != lastfp + 3 || col == width - 1) {
				if ((fp - frame_buffer) >= (row + 1) * width_3)
					row = (int) (fp - frame_buffer) / width_3;
				col = ((int)(fp - frame_buffer) - (row * width_3)) / 3;
			}
			else ++col;
			testRow[i] = row;
			testCol[i] = col;
			i++;
			lastfp = fp;
		}
	}
	unsigned char *render_buffer = (unsigned char*)malloc((width * height * 3 + 4) * sizeof(char));
	memset(render_buffer, 255, width_3 * height);

	for (int sensorValueIdx = 0; sensorValueIdx < sensor_values_count; sensorValueIdx++) {
		int multi_instr = 0;
		if (!strcmp(sensor_values[sensorValueIdx].key, "W")) {
			multi_instr = sensor_values[sensorValueIdx].value;
			while (sensorValueIdx + 1 < sensor_values_count && !strcmp(sensor_values[sensorValueIdx + 1].key, "W") &&
			       (processed_frames + 1) % 25 != 0) {

				multi_instr += sensor_values[sensorValueIdx + 1].value;
				sensorValueIdx++;
				processed_frames++;
			}
			//matUp.B[0] = - sensor_values[sensorValueIdx].value;
			matUp.B[0] = -multi_instr;
			multiMatrix(&final_matrix[current_matrix], &matUp, &final_matrix[current_matrix ^ 1]);
		} else if (!strcmp(sensor_values[sensorValueIdx].key, "A")) {
			multi_instr = sensor_values[sensorValueIdx].value;
			while (sensorValueIdx + 1 < sensor_values_count && !strcmp(sensor_values[sensorValueIdx + 1].key, "A") &&
			       (processed_frames + 1) % 25 != 0) {

				multi_instr += sensor_values[sensorValueIdx + 1].value;
				sensorValueIdx++;
				processed_frames++;
			}
			//matLeft.B[1] = - sensor_values[sensorValueIdx].value;
			matLeft.B[1] = -multi_instr;
			multiMatrix(&final_matrix[current_matrix], &matLeft, &final_matrix[current_matrix ^ 1]);
		} else if (!strcmp(sensor_values[sensorValueIdx].key, "S")) {
			multi_instr = sensor_values[sensorValueIdx].value;
			while (sensorValueIdx + 1 < sensor_values_count && !strcmp(sensor_values[sensorValueIdx + 1].key, "S") &&
			       (processed_frames + 1) % 25 != 0) {

				multi_instr += sensor_values[sensorValueIdx + 1].value;
				sensorValueIdx++;
				processed_frames++;
			}
			//matDown.B[0] = sensor_values[sensorValueIdx].value;
			matDown.B[0] = multi_instr;
			multiMatrix(&final_matrix[current_matrix], &matDown, &final_matrix[current_matrix ^ 1]);
		} else if (!strcmp(sensor_values[sensorValueIdx].key, "D")) {
			multi_instr = sensor_values[sensorValueIdx].value;
			while (sensorValueIdx + 1 < sensor_values_count && !strcmp(sensor_values[sensorValueIdx + 1].key, "D") &&
			       (processed_frames + 1) % 25 != 0) {

				multi_instr += sensor_values[sensorValueIdx + 1].value;
				sensorValueIdx++;
				processed_frames++;
			}
			//matRight.B[1] = sensor_values[sensorValueIdx].value;
			matRight.B[1] = multi_instr;
			multiMatrix(&final_matrix[current_matrix], &matRight, &final_matrix[current_matrix ^ 1]);
		} else if (!strcmp(sensor_values[sensorValueIdx].key, "CW") ||
		           !strcmp(sensor_values[sensorValueIdx].key, "CCW")) {
			int itr = sensor_values[sensorValueIdx].value;
			if (!strcmp(sensor_values[sensorValueIdx].key, "CCW"))
				itr = -itr;
			itr %= 4;
			if (itr < 0)
				itr = itr + 4;
			if (itr != 0)
				multiMatrix(&final_matrix[current_matrix], &matRot[itr - 1], &final_matrix[current_matrix ^ 1]);
			else current_matrix ^= 1;
		} else if (!strcmp(sensor_values[sensorValueIdx].key, "MX")) {
			multiMatrix(&final_matrix[current_matrix], &matFlipX, &final_matrix[current_matrix ^ 1]);
		} else if (!strcmp(sensor_values[sensorValueIdx].key, "MY")) {
			multiMatrix(&final_matrix[current_matrix], &matFlipY, &final_matrix[current_matrix ^ 1]);
		}
		current_matrix ^= 1;
		processed_frames += 1;
		if (processed_frames % 25 == 0) {
			int *baseRow, *baseCol;
			int typeA = final_matrix[current_matrix].typeA;
			if ((typeA >> 2) & 1) {
				baseRow = testCol;
				baseCol = testRow;
			}
			else {
				baseRow = testRow;
				baseCol = testCol;
			}

			for (int j = 0; j < i; j++) {
				int render_row = baseRow[j];
				int render_col = baseCol[j];
				if (typeA & 1)
					render_col = -render_col;
				if ((typeA >> 1) & 1)
					render_row = -render_row;
				render_row += final_matrix[current_matrix].B[0];
				render_col += final_matrix[current_matrix].B[1];
				int render_pos = render_row * width_3 + render_col * 3;
				renderedPos[j] = render_pos;
				*((unsigned int *) (render_buffer + render_pos)) &= testPixel[j];
			}
			verifyFrame(render_buffer, width, height, grading_mode);
//			char filename[100];
//			sprintf(filename, "%dimp.bmp", processed_frames);
//			writeBMP(width, height, render_buffer, filename);
			int *j;
			for (j = renderedPos; j < renderedPos + i - 4;) {
				UNROLL4(*((unsigned int *) (render_buffer + (*j))) |= 0xffffffff;
						        ++j;);
			}
			for (; j < renderedPos + i; ++j) {
				*((unsigned int *) (render_buffer + (*j))) |= 0xffffffff;
			}
		}
	}
	free(render_buffer);
}
