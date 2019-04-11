/* Module specific functions for the escape game innsbruck's host
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

#ifndef MODULE_H
#define MODULE_H

#include <json-c/json.h>
#include <stdbool.h>

struct module_t{
	int (*init_dependency)(json_object*);
	int (*init_module)();
	int (*start_module)();
	int (*check_dependency)(json_object*, float*);
	int (*execute_trigger)(json_object*, bool);
	int (*reset_module)();

	bool enabled, running;
	char *name;
};

extern struct module_t modules[];
extern size_t module_count;

int init_modules();
int start_modules();
int reset_modules();
int test_modules();

int check_dependency(json_object* dependency, float* percentage);
int execute_trigger(json_object* trigger, bool dry);
int init_general_dependency(json_object* dependency);

struct module_t* get_module_by_name(const char* name);
#endif
