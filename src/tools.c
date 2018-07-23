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

	return (memcmp(&tim, &rel, sizeof(tim)) == 0) - 1;
}
