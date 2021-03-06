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
		
		case 115200:
			baud = B115200;
		break;
		
		case 38400:
			baud = B38400;
		break;
		
		case 9600:
			baud = B9600;
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

/*
 * Matches two arrays, but the second array is used from the back to the front
 */
size_t get_arr_match_inverted(size_t* arr1, size_t* arr2, size_t len){

	size_t mtch = 0;
	if(arr1 == NULL || arr2 == NULL){
		println("Attempted array compare with NULL!", ERROR);
		return 0;
	}

	for(; mtch < len; mtch++){
		if(arr1[mtch] != arr2[mtch]){
			return mtch;
		}
	}

	return mtch;
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

char* printable_bytes_buf(uint8_t* data, size_t len, char* buffer, 
	size_t buflen){

	if(data == NULL || buffer == NULL){
		return NULL;
	}

	if(buflen < len * 4 + 1){
		return NULL;
	}
	
	for(size_t i = 0; i < len; i++){

		sprintf(&buffer[strlen(buffer)], "<%x>", data[i]);
	}

	return buffer;

}

ssize_t get_lines_in_string(const char* str){

	if(str == NULL){
		return -1;
	}

	ssize_t lncnt = 0;

	for(size_t i = 0; str[i] != '\0'; i++){
		lncnt += (str[i] == '\n');
	}

	return lncnt;

}

char* append_to_first_string(const char* str1, const char* str2){

	size_t len = strlen(str1) + strlen(str2) + 1;

	char* newstr = malloc(len);

	if(newstr == NULL){
		return NULL;
	}
	memset(newstr, 0, len);
	
	strcpy(newstr, str1);
	
	strncat(newstr, str2, len);
	return newstr;
}

char* append_long_to_first_string(const char* str1, long long int num){

	char str2[200];
	memset(str2, 0, sizeof(str2));
	snprintf(str2, sizeof(str2), "%lli", num);

	size_t len = strlen(str1) + strlen(str2) + 1;

	char* newstr = malloc(len);

	if(newstr == NULL){
		return NULL;
	}
	memset(newstr, 0, len);
	
	strcpy(newstr, str1);
	
	strncat(newstr, str2, len);

	return newstr;
}

/* 
 * May return any time. Continuity should be given! May NOT return real
 * system time!
 */

time_t get_current_ec_time(){

	struct timespec tp;

	if(clock_gettime(CLOCK_MONOTONIC_RAW, &tp) < 0){
		println("Failed to get system time!! assuming 0", ERROR);
		return 0;
	}

	/* The cast shouldn't be nescessary, but I'm doing it anyways */
	return (time_t)tp.tv_sec;

}

time_t get_ec_time_unix_offset(){

	struct timespec tp,tp_rtc;
	
	if(clock_gettime(CLOCK_MONOTONIC_RAW, &tp) < 0){
		println("Failed to get monotonic time!! assuming 0", ERROR);
		memset(&tp,0,sizeof(tp));
	}
	
	if(clock_gettime(CLOCK_REALTIME, &tp_rtc) < 0){
		println("Failed to get rtc time!! assuming 0", ERROR);
		memset(&tp_rtc,0,sizeof(tp_rtc));
	}

	time_t tdiff = tp_rtc.tv_sec - tp.tv_sec;

	if((tp_rtc.tv_nsec + 500000000) < tp.tv_nsec){
		tdiff--;
	
	}else if(tp_rtc.tv_nsec > (tp.tv_nsec + 500000000)){
		tdiff++;
	}

	return tdiff;

}

time_t ec_time_to_unix(time_t ec_tim){
	if(ec_tim != 0){
		return ec_tim + get_ec_time_unix_offset();
	}else{
		return 0;
	}
}
