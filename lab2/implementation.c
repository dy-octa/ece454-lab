#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          Do not forget to modify the team_name and team member information !!!
 **********************************************************************************************************************/
void print_team_info(){
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
	 char typeA;
	 int B[2];
 } Matrix;
char typeAMuls[] = {0, 1, 2, 3, 4, 5, 6, 7, 1, 0, 3, 2, 6, 7, 4, 5, 2, 3, 0, 1, 5, 4, 7, 6, 3, 2, 1, 0, 7, 6, 5, 4, 4,
                    5, 6, 7, 0, 1, 2, 3, 5, 4, 7, 6, 2, 3, 0, 1, 6, 7, 4, 5, 1, 0, 3, 2, 7, 6, 5, 4, 3, 2, 1, 0};
void multiMatrix(Matrix* A, Matrix* B, Matrix* C){
	C->typeA = typeAMuls[((short)(A->typeA))<<3 | ((short)(B->typeA))];
	switch (B->typeA) {
		case 0: C->B[0] = A->B[0]; C->B[1] = A->B[1];break;
		case 1: C->B[0] = A->B[0]; C->B[1] = -A->B[1];break;
		case 2: C->B[0] = -A->B[0]; C->B[1] = A->B[1];break;
		case 3: C->B[0] = -A->B[0]; C->B[1] = -A->B[1];break;
		case 4: C->B[0] = A->B[1]; C->B[1] = A->B[0];break;
		case 5: C->B[0] = A->B[1]; C->B[1] = -A->B[0];break;
		case 6: C->B[0] = -A->B[1]; C->B[1] = A->B[0];break;
		case 7: C->B[0] = -A->B[1]; C->B[1] = -A->B[0];break;
	}
	C->B[0] += B->B[0];
	C->B[1] += B->B[1];
	return;
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
void implementation_driver(struct kv *sensor_values, int sensor_values_count, unsigned char *frame_buffer,
                           unsigned int width, unsigned int height, bool grading_mode) {
    int processed_frames = 0;
	Matrix final_matrix[2] = {{0, 0, 0}, {0, 0, 0}}; // {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {}};
	Matrix matUp = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {-1, 0, 1}};
	Matrix matDown = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {1, 0, 1}};
	Matrix matLeft = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {0, -1, 1}};
	Matrix matRight = {0, 0, 0}; // {{1, 0, 0}, {0, 1, 0}, {0, 1, 1}};
	Matrix matRot[3] = {{5, 0, width - 1}, {3, width - 1, width - 1}, {6, width - 1, 0}};
//						{{{0, -1, 0}, {1, 0, 0}, {0, width - 1, 1}},
//						 {{-1, 0, 0}, {0, -1, 0}, {width - 1, width - 1, 1}},
//						 {{0, 1, 0}, {-1, 0, 0}, {width - 1, 0, 1}}};
	Matrix matFlipX = {2, height - 1, 0}; // {{-1, 0, 0}, {0, 1, 0}, {height - 1, 0, 1}};
	Matrix matFlipY = {1, 0, width - 1}; //{{1, 0, 0}, {0, -1, 0}, {0, width - 1, 1}};
	int current_matrix = 0;

	int testArray[10669];
	int testRow[10669];
	int testCol[10669];
	int i = 0;
	for (int row = 0; row < height; ++row){
		for (int col = 0; col < width; ++col) {
			int pos = row * width * 3 + col * 3;
			if (!(frame_buffer[pos] == 255 && frame_buffer[pos + 1] == 255 && frame_buffer[pos + 2] == 255)) {
			 //printf("what is i: %d\n", i);
			 testArray[i] = row * width * 3 + col * 3;
			 testRow[i] = row;
			 testCol[i] = col;
			 i++;
			}

		}
	}


	unsigned char* render_buffer = allocateFrame(width, height);

    for (int sensorValueIdx = 0; sensorValueIdx < sensor_values_count; sensorValueIdx++) {
        if (!strcmp(sensor_values[sensorValueIdx].key, "W")) {
	        matUp.B[0] = - sensor_values[sensorValueIdx].value;
	        multiMatrix(&final_matrix[current_matrix], &matUp, &final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "A")) {
	        matLeft.B[1] = - sensor_values[sensorValueIdx].value;
	        multiMatrix(&final_matrix[current_matrix], &matLeft, &final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "S")) {
	        matDown.B[0] = sensor_values[sensorValueIdx].value;
	        multiMatrix(&final_matrix[current_matrix], &matDown, &final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "D")) {
	        matRight.B[1] = sensor_values[sensorValueIdx].value;
	        multiMatrix(&final_matrix[current_matrix], &matRight, &final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "CW") || !strcmp(sensor_values[sensorValueIdx].key, "CCW")) {
	        int itr = sensor_values[sensorValueIdx].value;
	        if (!strcmp(sensor_values[sensorValueIdx].key, "CCW"))
		        itr = - itr;
	        itr %= 4;
	        if (itr < 0)
		        itr = itr + 4;
//	        printf("Now %s, %d : %d\n", sensor_values[sensorValueIdx].key, sensor_values[sensorValueIdx].value, itr);
	        if (itr != 0)
	            multiMatrix(&final_matrix[current_matrix], &matRot[itr - 1], &final_matrix[current_matrix^1]);
	        else current_matrix ^= 1;
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MX")) {
	        multiMatrix(&final_matrix[current_matrix], &matFlipX, &final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MY")) {
	        multiMatrix(&final_matrix[current_matrix], &matFlipY, &final_matrix[current_matrix^1]);
        }
	    current_matrix ^= 1;
        processed_frames += 1;
//	    printf("%d\n", processed_frames);
        if (processed_frames % 25 == 0) {
//	        printf("Process %d...\n", processed_frames);
	        memset(render_buffer, 255, width * height * 3);
	        /*for (int row = 0; row < height; ++row)
		        for (int col = 0; col < width; ++col) {
			        int pos = row * width * 3 + col * 3;
			        if (!(frame_buffer[pos] == 255 && frame_buffer[pos + 1] == 255 && frame_buffer[pos + 2] == 255)) {
				        int render_row = final_matrix[current_matrix].B[0];
				        int render_col = final_matrix[current_matrix].B[1];
				        switch (final_matrix[current_matrix].typeA) {
					        case 0: render_row += row; render_col += col; break;
					        case 1: render_row += row; render_col -= col; break;
					        case 2: render_row -= row; render_col += col; break;
					        case 3: render_row -= row; render_col -= col; break;
					        case 4: render_row += col; render_col += row; break;
					        case 5: render_row += col; render_col -= row; break;
					        case 6: render_row -= col; render_col += row; break;
					        case 7: render_row -= col; render_col -= row; break;
				        }
//				        if (render_row < 0 || render_row >= height || render_col < 0 || render_col >=width)
//					        printf("!!!(%d, %d)->(%d, %d)!!!\n", row, col, render_row, render_col);
				        int render_pos = render_row * width * 3 + render_col * 3;
				        render_buffer[render_pos] = frame_buffer[pos];
				        render_buffer[render_pos + 1] = frame_buffer[pos + 1];
				        render_buffer[render_pos + 2] = frame_buffer[pos + 2];
			        }
		        }*/

	        for (int j = 0; j < i; j++){
		        int render_row = final_matrix[current_matrix].B[0];
		        int render_col = final_matrix[current_matrix].B[1];
		        switch (final_matrix[current_matrix].typeA) {
			        case 0: render_row += testRow[j]; render_col += testCol[j]; break;
			        case 1: render_row += testRow[j]; render_col -= testCol[j]; break;
			        case 2: render_row -= testRow[j]; render_col += testCol[j]; break;
			        case 3: render_row -= testRow[j]; render_col -= testCol[j]; break;
			        case 4: render_row += testCol[j]; render_col += testRow[j]; break;
			        case 5: render_row += testCol[j]; render_col -= testRow[j]; break;
			        case 6: render_row -= testCol[j]; render_col += testRow[j]; break;
			        case 7: render_row -= testCol[j]; render_col -= testRow[j]; break;
		        }
//				if (render_row < 0 || render_row >= height || render_col < 0 || render_col >=width)
//		            printf("!!!(%d, %d)->(%d, %d)!!!\n", testRow[j], testCol[j], render_row, render_col);
	        	int render_pos = render_row * width * 3 + render_col * 3;
	        	render_buffer[render_pos] = frame_buffer[testArray[j]];
	        	render_buffer[render_pos + 1] = frame_buffer[testArray[j] + 1];
	        	render_buffer[render_pos + 2] = frame_buffer[testArray[j] + 2];

	        }
            verifyFrame(render_buffer, width, height, grading_mode);
//	        printf("%d finished\n", processed_frames);

//	        char filename[100];
//	        sprintf(filename, "%dimp.bmp", processed_frames);
//	        writeBMP(width, height, render_buffer, filename);

//	        final_matrix[current_matrix][0][0] = 1, final_matrix[current_matrix][0][1] = 0, final_matrix[current_matrix][0][2] =0;
//	        final_matrix[current_matrix][1][0] = 0, final_matrix[current_matrix][1][1] = 1, final_matrix[current_matrix][1][2] =0;
//	        final_matrix[current_matrix][2][0] = 0, final_matrix[current_matrix][2][1] = 0, final_matrix[current_matrix][2][2] =1;
        }
    }
	deallocateFrame(render_buffer);
    return;
}
