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
#include <stdbool.h>

int init_core();
int start_core();

void* core_loop();

int core_init_dependency(json_object* dependency);
int core_check_dependency(json_object* dependency);
int core_trigger(json_object *trigger);

int core_update_sequential();

#ifndef NOALARM
void core_trigger_alarm();
void core_release_alarm();
#endif /* NOALARM */

long long int game_timer_start, game_timer_end, game_duration;

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
	size_t pointer; /* Where we are right now */
	int* last_dependencies;
};

extern struct sequence_dependency_t** core_sequential_dependencies;
extern size_t core_sequence_count;

struct flank_dependency_t{
	bool high;
	bool low;
	/* The dependencies last value */
	int last;
	int id;
};

struct length_dependency_t{

	/* Contains wether the duration the dependency is fullfilled is under
	 * or over the given threshold.
	 */	
	bool below;
	bool above;
	
	int id;
	size_t threshold; /* The threshold in seconds */
};

extern struct length_dependency_t** length_dependencies;
extern size_t length_dependency_count;

extern struct flank_dependency_t** flank_dependencies;
extern size_t flank_dependency_count;

/* Alarm related things. An alarm tries to get the operator's attention.*/
#ifndef NOALARM
bool alarm_on;
#endif /* NOALARM */

#endif /* CORE_H */
