/* a tool colletion
 * Copyright © 2018 tyrolyean
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tools.h"
#include "log.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <termios.h>
#include <stdio.h>

int* remove_array_element(size_t array_length, int* array,int element){

        for (size_t i = element; i < array_length-1; i++) {
                array[i] = array[i+1];
        }
        
        array = realloc(array,array_length-1);

        return array;
}

int* add_array_element(size_t array_length, int* array, int element){

        array = realloc(array,array_length+1);
        array[array_length+1] = element;

        return array;

}

int first_free_in_array(size_t arrlen, int* array){

        int min = 0;


        for(size_t i = 0; i < arrlen;i++){
                if(min == array[i]){
                        i = 0;
                        min++;
                }
        }

        return min;
}

/* Sleep milliseconds returns 0 on success and < 0 on error */
int sleep_ms(uint32_t ms){
	struct timespec tim;
	tim.tv_sec = (ms - (ms % 1000)) / 1000;
	tim.tv_nsec = (ms % 1000) * 1000000;

	println("sleeping %is and %ins", DEBUG, tim.tv_sec, tim.tv_nsec);
	
	struct timespec rel;

	nanosleep(&tim, &rel);

	return 1;
}

/* As it is not possible to get the baud rate from the kernel via a int
 * representation of it, this wrapper matches int representation of the baud
 * rate with a preprocessor-definition and returns the latter.
 * Returns 0 on error */

int get_baud_from_int(int baud_in){
	
	int baud = 0;

	switch(baud_in){
		
		case 460800:
			baud = B460800;
		break;

		case 57600:
			baud = B57600;
		break;
		default:
			println("Invalid baud rate specified: %i", ERROR,
				baud_in);
		break;
	}

	return baud;
	
}

/* Searches the given array for the given element. Returns < 0 if not found,
 * and the array index if found. */

int is_in_array(size_t element, size_t* arr, size_t arrlen){

	int position = -1;
	for(size_t i = 0; i < arrlen; i++){
		if(arr[i] == element){
			position = i;
			break;
		}
	}
	
	return position;
}

char* printable_bytes(uint8_t* data, size_t len){

	if(data == NULL){
		return NULL;
	}
	char* str = malloc(len * 4 + 1);
	memset(str,0, len * 4 + 1);
	
	for(size_t i = 0; i < len; i++){

		sprintf(&str[strlen(str)], "<%x>", data[i]);
	}

	return str;
	
}
