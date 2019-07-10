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
#include <sys/time.h>
#include <sys/types.h>

int *remove_array_element(size_t array_length, int* array,int element);
int *add_array_element(size_t array_length, int* array, int element);

int first_free_in_array(size_t arrlen, int* array);

int sleep_ms(uint32_t ms);

int get_baud_from_int(int baud_in);

int is_in_array(size_t element, size_t* arr, size_t arrlen);

size_t get_arr_match_inverted(size_t* arr1, size_t* arr2, size_t len);

char* printable_bytes(uint8_t* data, size_t len);
char* printable_bytes_buf(uint8_t* data, size_t len, char* buffer, 
	size_t buflen);

ssize_t get_lines_in_string(const char* str);

char* append_to_first_string(const char* str1, const char* str2);
char* append_long_to_first_string(const char* str1, long long int num);
/* Time related functions */
time_t get_current_ec_time();
time_t get_ec_time_unix_offset();
time_t ec_time_to_unix(time_t ec_tim);
#endif
