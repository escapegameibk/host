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

#ifndef TOOLS_H
#define TOOLS_H

#include <stddef.h>
#include <stdint.h>

int *remove_array_element(size_t array_length, int* array,int element);
int *add_array_element(size_t array_length, int* array, int element);

int first_free_in_array(size_t arrlen, int* array);

int sleep_ms(uint32_t ms);

int get_baud_from_int(int baud_in);

int is_in_array(size_t element, size_t* arr, size_t arrlen);

size_t get_arr_match_from_start(size_t* arr1, size_t* arr2, size_t len);
#endif
