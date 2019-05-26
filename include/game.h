/* the game state machine for the escape game ibk
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

#ifndef GAME_H
#define GAME_H

#include <pthread.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <stdint.h>
#include <pthread.h>

#define PATROL_INTERVAL_NS 5000000
#define DEFAULT_GAME_TIME 3600

/* Execution states of events */
#define EVENT_RESET        0
#define EVENT_IN_EXECUTION 1
#define EVENT_TRIGGERED    2

int init_game();
int init_dependency(json_object* dependency, int event_id);
int start_game();

/* checks for changes and act accordingly */
int patrol();
void* loop_game();

int trigger_event(size_t event_id);
void async_trigger_event(size_t event_id);
int trigger_event_maybe_blocking(size_t event_id);
int untrigger_event(size_t event_id);

/* utility functions */

json_object** get_root_dependencies(size_t* depcn, size_t** event_idsp);
int get_dependency_id(json_object* dependency);
size_t get_hint_event();
int trigger_array(json_object* triggers);
json_object** get_root_triggers(size_t* trigcnt, size_t** event_idsp);

json_object** dependency_list;
size_t dependency_count;
pthread_t game_thread;

/* Contains the amount of loaded events and wether they are executed, in 
 * execution or something else. May contain any of the preprocessor definitions
 * from the beginning of this file.
 */
uint8_t * event_trigger_status;
size_t event_cnt;

/* Pending autoresets. For events, which don't require a global reset, but are
 * autoresetting. I can't reset them immediately, but have to wait until it
 * should be possible to trigger it again... This is done in the routine. */
bool* reset_jobs;

pthread_mutex_t game_trigger_lock;

#endif
