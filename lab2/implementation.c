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
void multiMatrix(int A[3][3], int B[3][3], int C[3][3]){
	int sum = 0;

	/*for(int i = 0; i \x3C 3; i++){
		for(int j = 0; j \x3C 3; j++){
			sum = 0;
			for(int k = 0; k \x3C 3; k++){
				sum = sum +  (A[i][k] * B[k][j]);
			}
			C[i][j] = sum;
		}
	}*/

	C[0][0] = (A[0][0]*B[0][0]) + (A[0][1]*B[1][0] + (A[0][2]*B[2][0]));
	C[0][1] = (A[0][0]*B[0][1]) + (A[0][1]*B[1][1] + (A[0][2]*B[2][1]));
	C[0][2] = (A[0][0]*B[0][2]) + (A[0][1]*B[1][2] + (A[0][2]*B[2][2]));
	C[1][0] = (A[1][0]*B[0][0]) + (A[1][1]*B[1][0] + (A[1][2]*B[2][0]));
	C[1][1] = (A[1][0]*B[0][1]) + (A[1][1]*B[1][1] + (A[1][2]*B[2][1]));
	C[1][2] = (A[1][0]*B[0][2]) + (A[1][1]*B[1][2] + (A[1][2]*B[2][2]));
	C[2][0] = (A[2][0]*B[0][0]) + (A[2][1]*B[1][0] + (A[2][2]*B[2][0]));
	C[2][1] = (A[2][0]*B[0][1]) + (A[2][1]*B[1][1] + (A[2][2]*B[2][1]));
	C[2][2] = (A[2][0]*B[0][2]) + (A[2][1]*B[1][2] + (A[2][2]*B[2][2]));


	return;
}
void implementation_driver(struct kv *sensor_values, int sensor_values_count, unsigned char *frame_buffer,
                           unsigned int width, unsigned int height, bool grading_mode) {
    int processed_frames = 0;
	int final_matrix[2][3][3] = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {}};
	int matUp[3][3] = {{1, 0, 0}, {0, 1, 0}, {-1, 0, 1}};
	int matDown[3][3] = {{1, 0, 0}, {0, 1, 0}, {1, 0, 1}};
	int matLeft[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, -1, 1}};
	int matRight[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 1, 1}};
	int matRot[3][3][3] = {{{0, -1, 0}, {1, 0, 0}, {0, width - 1, 1}},
		                {{-1, 0, 0}, {0, -1, 0}, {width - 1, width - 1, 1}},
		                {{0, 1, 0}, {-1, 0, 0}, {width - 1, 0, 1}}};
	int matFlipX[3][3] = {{-1, 0, 0}, {0, 1, 0}, {height - 1, 0, 1}};
	int matFlipY[3][3] = {{1, 0, 0}, {0, -1, 0}, {0, width - 1, 1}};
	int current_matrix = 0;


	unsigned char* render_buffer = allocateFrame(width, height);

    for (int sensorValueIdx = 0; sensorValueIdx < sensor_values_count; sensorValueIdx++) {
        if (!strcmp(sensor_values[sensorValueIdx].key, "W")) {
	        matUp[2][0] = - sensor_values[sensorValueIdx].value;
	        multiMatrix(final_matrix[current_matrix], matUp, final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "A")) {
	        matLeft[2][1] = - sensor_values[sensorValueIdx].value;
	        multiMatrix(final_matrix[current_matrix], matLeft, final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "S")) {
	        matDown[2][0] = sensor_values[sensorValueIdx].value;
	        multiMatrix(final_matrix[current_matrix], matDown, final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "D")) {
	        matRight[2][1] = sensor_values[sensorValueIdx].value;
	        multiMatrix(final_matrix[current_matrix], matRight, final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "CW") || !strcmp(sensor_values[sensorValueIdx].key, "CCW")) {
	        int itr = sensor_values[sensorValueIdx].value;
	        if (!strcmp(sensor_values[sensorValueIdx].key, "CCW"))
		        itr = - itr;
	        itr %= 4;
	        if (itr < 0)
		        itr = itr + 4;
//	        printf("Now %s, %d : %d\n", sensor_values[sensorValueIdx].key, sensor_values[sensorValueIdx].value, itr);
	        if (itr != 0)
	            multiMatrix(final_matrix[current_matrix], matRot[itr - 1], final_matrix[current_matrix^1]);
	        else current_matrix ^= 1;
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MX")) {
	        multiMatrix(final_matrix[current_matrix], matFlipX, final_matrix[current_matrix^1]);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MY")) {
	        multiMatrix(final_matrix[current_matrix], matFlipY, final_matrix[current_matrix^1]);
        }
	    current_matrix ^= 1;
        processed_frames += 1;
//	    printf("%d\n", processed_frames);
        if (processed_frames % 25 == 0) {
//			printf("Current matrix:\n\t%d %d %d\n\t%d %d %d\n\t%d %d %d\n",
//			       final_matrix[current_matrix][0][0], final_matrix[current_matrix][0][1], final_matrix[current_matrix][0][2],
//			       final_matrix[current_matrix][1][0], final_matrix[current_matrix][1][1], final_matrix[current_matrix][1][2],
//			       final_matrix[current_matrix][2][0], final_matrix[current_matrix][2][1], final_matrix[current_matrix][2][2]);
	        memset(render_buffer, 255, width * height * 3);
	        for (int row = 0; row < height; ++row)
		        for (int col = 0; col < width; ++col) {
			        int pos = row * width * 3 + col * 3;
			        if (!(frame_buffer[pos] == 255 && frame_buffer[pos + 1] == 255 && frame_buffer[pos + 2] == 255)) {
				        int render_row = row * final_matrix[current_matrix][0][0] + col * final_matrix[current_matrix][1][0] + final_matrix[current_matrix][2][0];
				        int render_col = row * final_matrix[current_matrix][0][1] + col * final_matrix[current_matrix][1][1] + final_matrix[current_matrix][2][1];
//				        if (!(render_col >= 0 && render_col < width && render_row >= 0 && render_row < height) ) {
//					        printf("(%d, %d) -> (%d, %d)", row, col, render_row, render_col);
//				        }
				        int render_pos = render_row * width * 3 + render_col * 3;
				        render_buffer[render_pos] = frame_buffer[pos];
				        render_buffer[render_pos + 1] = frame_buffer[pos + 1];
				        render_buffer[render_pos + 2] = frame_buffer[pos + 2];
			        }
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
