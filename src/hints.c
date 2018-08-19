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

#include "hints.h"
#include "config.h"
#include "game.h"
#include "log.h"

#include <json-c/json.h>
#include <stddef.h>

int init_hints(){

	hints_enabled = false;

	json_object* hints = json_object_object_get(config_glob,"hints");

	if(hints == NULL){
		println("No hints configured. disabling.", WARNING);
		return 0;
	}

	if(json_object_array_length(hints) == 0){
		println("Hints array empty. disabling", WARNING);
		return 0;
	}

	unsigned int hintcnt = 0;

	for(size_t i = 0; i < json_object_array_length(hints); i++){
		hintcnt += json_object_array_length(json_object_array_get_idx(
			hints,i));	
	}

	println("Configured a total of %i hints.", DEBUG, hintcnt);
	println("Hint system initialized.", DEBUG);

	hints_enabled = true;

	return 0;
}

json_object* get_hints_root(){

	if(hints_enabled){
		return json_object_object_get(config_glob, "hints");
	}else{
		return NULL;
	}
}

int execute_hint(size_t event_id, size_t hint_id){
		
	if(!hints_enabled){
		return -1;
	}

	json_object* hints = get_hints_root();

	if(json_object_array_length(hints) <= event_id){
		println("Got too high event id for hints! %i", ERROR, event_id);
		return -2;
	}
	
	json_object* event_hints = json_object_array_get_idx(hints, event_id);
	
	if(json_object_array_length(event_hints) <= hint_id){
		println("Got too high hint id for hints! Event %i Hint %i", 
			ERROR, event_id, hint_id);
		return -3;
	}

	json_object* hint = json_object_array_get_idx(event_hints, hint_id);

	if(hint == NULL){
		println("Failed to address hint! event %i, hint %i", ERROR, 
			event_id, hint_id);
		return -4;
	}

	const char* name = json_object_get_string(json_object_object_get(hint,
		"name"));

	println("Executing Hint %s", DEBUG, name);
	
	json_object* trigger = json_object_object_get(hint, "triggers");

	for(size_t i = 0; i < json_object_array_length(trigger); i++){
		execute_trigger(json_object_array_get_idx(trigger, i));
	}

	println("Done executing hint!", DEBUG);

	return 0;
}

#endif /* NOHINTS */
