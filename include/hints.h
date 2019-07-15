/* Hint system
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

#ifndef NOHINTS

#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>

#ifndef HINTS_H
#define HINTS_H

int init_hints();
int start_hints();
void* loop_hints();

int check_autohints();

json_object* get_hints_root();
int execute_hint(size_t event_id, size_t hint_id);

bool hints_enabled;
bool auto_hinting;

bool* hint_reset_jobs;

bool** hint_exec_states;
/* Contains the amount of levels of hints, that the globabl field contains */
size_t hint_lvl_amount;
/* Contains the amount of hints per level */
extern size_t *hints_lvled;

/* Returns the amount of already executed hints with the exception of those who
 * don't count due to the counts field */
size_t get_hint_exec_cnt();
int reset_hints();

json_object* get_hint_by_id(size_t event_id, size_t hint_id);

#endif /* HINTS_H */
#endif /* NOHINTS */
