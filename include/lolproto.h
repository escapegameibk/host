/* A EC-Proto implementation for the escape game innsbruck's host
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

#ifndef NOLOL

#ifndef LOLPROTO_H
#define LOLPROTO_H

#define LOLPROTO_OVERHEAD  7

#define LOLPROTO_LONG_PAY  32
#define LOLPROTO_SHORT_PAY 4

#include <stdint.h>
#include <stdbool.h>
#include <json-c/json.h>

#include <pthread.h>

#define LOL_DEFAULT_PORT "/dev/ttyUSB0"
#define LOL_DEFAULT_BAUD B115200

struct lol_frame_t{
	size_t   len;
	uint8_t	 address;
	uint8_t* payload;
};

int lol_init_dependency(json_object* dependency);

int init_lol();
int start_lol();

void* lol_listen_fordata();

int recieve_lol_frame(struct lol_frame_t* frame);

extern uint8_t lol_start_sequence[3];

int lolfd;

pthread_mutex_t lol_lock;


#endif /* LOLPROTO_H */
#endif /* NOLOL */
