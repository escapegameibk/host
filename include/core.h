/* Core utilities of the escape game innsbruck's game host
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef CORE_H
#define CORE_H

#include <json-c/json.h>

int init_core();
int start_core();

void* core_loop();

int core_init_dependency(json_object* dependency);
int core_check_dependency(json_object* dependency);
int core_trigger(json_object *trigger);

int core_update_sequential();

long long int game_timer_start, game_duration;

struct{

	json_object* trigger;
	unsigned long long int interval_ms;
	unsigned short int id;

} core_permanent_trigger_t;

struct sequence_dependency_t{
	/* The root dependency containing the dependencys used for the sequence
	 */
	json_object* dependency;
	int dependency_id;
	/* The array legnth, not the byte length of the sequences */
	size_t sequence_length;
	/* These 2 objects contain the sequences of triggered dependencies */
	size_t* target_sequence;
	size_t* sequence_so_far;

};

struct sequence_dependency_t** core_sequential_dependencies;
size_t core_sequence_count;
struct core_permanent_trigger_t* core_permanent_trigger;

#endif /* CORE_H */
