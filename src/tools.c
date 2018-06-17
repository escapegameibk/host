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
#include <stdlib.h>
#include <limits.h>

int* remove_array_element(size_t array_length, int* array,int element){

        for (int i = element; i < array_length-1; i++) {
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


        for(int i = 0; i < arrlen;i++){
                if(min == array[i]){
                        i = 0;
                        min++;
                }
        }

        return min;
}
