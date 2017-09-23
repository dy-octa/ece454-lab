#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "utilities.h"  // DO NOT REMOVE this line
#include "implementation_reference.h"   // DO NOT REMOVE this line

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image up
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveUp(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
    for (int row = 0; row < (height - offset); row++) {
	    int rowpos = row * width * 3;
	    int drowpos = (row + offset) * width * 3;
        for (int column = 0; column < width; column++) {
            int position_rendered_frame = rowpos + column * 3;
            int position_buffer_frame = drowpos + column * 3;
            buffer_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
            buffer_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
            buffer_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
        }
    }

    // fill left over pixels with white pixels
	int position_buffer_frame = (height - offset) * width * 3;
	int position_end_buffer_frame = height * width * 3;
	memset(buffer_frame + position_buffer_frame, 255, position_end_buffer_frame - position_buffer_frame);
	return buffer_frame;
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image left
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveRight(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
	// store shifted pixels to temporary buffer
	for (int row = 0; row < height; row++) {
		int rowpos = row * width * 3;
		for (int column = width - 1; column >= offset; column--) {
			int position_rendered_frame =  rowpos + column * 3;
			int position_buffer_frame = rowpos + (column - offset) * 3;
			buffer_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
			buffer_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
			buffer_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
		}
	}

	// fill left over pixels with white pixels
	for (int row = 0; row < height; row++)
		memset(buffer_frame + row * width * 3, 255, offset * 3);
	return buffer_frame;
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image up
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveDown(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
	// store shifted pixels to temporary buffer
	for (int row = height - 1; row >= offset; row--) {
		int rowpos = row * width * 3;
		int drowpos = (row - offset) * width * 3;
		for (int column = 0; column < width; column++) {
			int position_rendered_frame = rowpos + column * 3;
			int position_buffer_frame = drowpos + column * 3;
			buffer_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
			buffer_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
			buffer_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
		}
	}

	// fill left over pixels with white pixels
	memset(buffer_frame, 255, offset * width * 3);
	return buffer_frame;
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param offset - number of pixels to shift the object in bitmap image right
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note1: White pixels RGB(255,255,255) are treated as background. Object in the image refers to non-white pixels.
 * Note2: You can assume the object will never be moved off the screen
 **********************************************************************************************************************/
unsigned char *processMoveLeft(unsigned char *buffer_frame, unsigned width, unsigned height, int offset) {
	// store shifted pixels to temporary buffer
	for (int row = 0; row < height; row++) {
		int rowpos = row * width * 3;
		for (int column = 0; column < (width - offset); column++) {
			int position_rendered_frame = rowpos + column * 3;
			int position_buffer_frame = rowpos + (column + offset) * 3;
			buffer_frame[position_rendered_frame] = buffer_frame[position_buffer_frame];
			buffer_frame[position_rendered_frame + 1] = buffer_frame[position_buffer_frame + 1];
			buffer_frame[position_rendered_frame + 2] = buffer_frame[position_buffer_frame + 2];
		}
	}

	// fill left over pixels with white pixels
	for (int row = 0; row < height; row++)
		memset(buffer_frame + row * width * 3 + (width - offset) * 3, 255, offset * 3);
	return buffer_frame;
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param rotate_iteration - rotate object inside frame buffer clockwise by 90 degrees, <iteration> times
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note: You can assume the frame will always be square and you will be rotating the entire image
 **********************************************************************************************************************/
unsigned char *processRotateCW(unsigned char *buffer_frame, unsigned width, unsigned height,
                               int rotate_iteration) {
	// allocate memory for temporary image buffer
	unsigned char static *rendered_frame = NULL;
	if (rendered_frame == NULL)
		rendered_frame = allocateFrame(width, height);

	rotate_iteration %= 4;
	// store shifted pixels to temporary buffer
	for (int iteration = 0; iteration < rotate_iteration; iteration++) {
		int render_column = width - 1;
		int render_row = 0;
		for (int row = 0; row < width; row++) {
			for (int column = 0; column < height; column++) {
				int position_frame_buffer = row * width * 3 + column * 3;
				rendered_frame[render_row * width * 3 + render_column * 3] = buffer_frame[position_frame_buffer];
				rendered_frame[render_row * width * 3 + render_column * 3 + 1] = buffer_frame[position_frame_buffer + 1];
				rendered_frame[render_row * width * 3 + render_column * 3 + 2] = buffer_frame[position_frame_buffer + 2];
				render_row += 1;
			}
			render_row = 0;
			render_column -= 1;
		}

	}

	// copy the temporary buffer back to original frame buffer
	buffer_frame = copyFrame(rendered_frame, buffer_frame, width, height);

	// return a pointer to the updated image buffer
	return buffer_frame;
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param rotate_iteration - rotate object inside frame buffer counter clockwise by 90 degrees, <iteration> times
 * @return - pointer pointing a buffer storing a modified 24-bit bitmap image
 * Note: You can assume the frame will always be square and you will be rotating the entire image
 **********************************************************************************************************************/
unsigned char *processRotateCCW(unsigned char *buffer_frame, unsigned width, unsigned height,
                                int rotate_iteration) {
	return buffer_frame = processRotateCWReference(buffer_frame, width, height, 3 * rotate_iteration);
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
#define swap(a,b) (tmp=a,a=b,b=tmp)
unsigned char *processMirrorX(unsigned char *buffer_frame, unsigned int width, unsigned int height, int _unused) {
	char tmp;
	// store shifted pixels to temporary buffer
	for (int row = 0; row * 2 < height; row++) {
		int rowpos = row * height * 3;
		int drowpos = (height - row - 1) * height * 3;
		for (int column = 0; column < width; column++) {
			int position_rendered_frame = rowpos + column * 3;
			int position_buffer_frame = drowpos + column * 3;
			swap(buffer_frame[position_buffer_frame], buffer_frame[position_rendered_frame]);
			swap(buffer_frame[position_buffer_frame+1], buffer_frame[position_rendered_frame+1]);
			swap(buffer_frame[position_buffer_frame+2], buffer_frame[position_rendered_frame+2]);
		}
	}
	return buffer_frame;
}

/***********************************************************************************************************************
 * @param buffer_frame - pointer pointing to a buffer storing the imported 24-bit bitmap image
 * @param width - width of the imported 24-bit bitmap image
 * @param height - height of the imported 24-bit bitmap image
 * @param _unused - this field is unused
 * @return
 **********************************************************************************************************************/
unsigned char *processMirrorY(unsigned char *buffer_frame, unsigned width, unsigned height, int _unused) {
	char tmp;
	// store shifted pixels to temporary buffer
	for (int row = 0; row < height; row++) {
		int rowpos = row * height * 3;
		for (int column = 0; column * 2 < width; column++) {
			int position_rendered_frame = rowpos + column * 3;
			int position_buffer_frame = rowpos + (width - column - 1) * 3;
			swap(buffer_frame[position_buffer_frame], buffer_frame[position_rendered_frame]);
			swap(buffer_frame[position_buffer_frame+1], buffer_frame[position_rendered_frame+1]);
			swap(buffer_frame[position_buffer_frame+2], buffer_frame[position_rendered_frame+2]);
		}
	}
	return buffer_frame;
}

/***********************************************************************************************************************
 * WARNING: Do not modify the implementation_driver and team info prototype (name, parameter, return value) !!!
 *          Do not forget to modify the team_name and team member information !!!
 **********************************************************************************************************************/
void print_team_info(){
    // Please modify this field with something interesting
    char team_name[] = "default-name";

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
void implementation_driver(struct kv *sensor_values, int sensor_values_count, unsigned char *frame_buffer,
                           unsigned int width, unsigned int height, bool grading_mode) {
    int processed_frames = 0;
    for (int sensorValueIdx = 0; sensorValueIdx < sensor_values_count; sensorValueIdx++) {
//        printf("Processing sensor value #%d: %s, %d\n", sensorValueIdx, sensor_values[sensorValueIdx].key,
//               sensor_values[sensorValueIdx].value);
        if (!strcmp(sensor_values[sensorValueIdx].key, "W")) {
            frame_buffer = processMoveUp(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "A")) {
            frame_buffer = processMoveLeft(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "S")) {
            frame_buffer = processMoveDown(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "D")) {
            frame_buffer = processMoveRight(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "CW")) {
            frame_buffer = processRotateCW(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "CCW")) {
            frame_buffer = processRotateCCW(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MX")) {
            frame_buffer = processMirrorX(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        } else if (!strcmp(sensor_values[sensorValueIdx].key, "MY")) {
            frame_buffer = processMirrorY(frame_buffer, width, height, sensor_values[sensorValueIdx].value);
//            printBMP(width, height, frame_buffer);
        }
        processed_frames += 1;
        if (processed_frames % 25 == 0) {
            verifyFrame(frame_buffer, width, height, grading_mode);
        }
    }
    return;
}
