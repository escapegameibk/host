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
#include "data.h"

#include <sys/time.h>
#include <json-c/json.h>
#include <stddef.h>
#include <unistd.h>

int init_hints(){

	hints_enabled = false;
	auto_hinting = false;

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
	
	hints_enabled = true;

	/* Attempt to load dependencies for auto-hinting */
	
	json_object* hint_dependencies = json_object_object_get(config_glob, 
		"hint_dependencies");
	if(hint_dependencies == NULL){
		println("Auto-hinting not configured. Disabling", DEBUG);
		return 0;
	}
	size_t hinting_depth = json_object_array_length(
		hint_dependencies);
	println("Hinting to a depth of %i. Correct hint amount assumed.", DEBUG,
		hinting_depth);
	for(size_t i = 0; i < hinting_depth; i++){
		json_object * dependencies = json_object_array_get_idx(
			hint_dependencies, i);
		for(size_t dep = 0; dep < json_object_array_length(
			dependencies); dep++){
			
			json_object* dependency = json_object_array_get_idx(dependencies,dep);

			if(init_general_dependency(dependency) < 0){
				println("Failed to init dependency for hint!",
					ERROR);
				return -1;
			}
		}
	}

	auto_hinting = true;

	println("Hint system initialized.", DEBUG);

	return 0;
}

int start_hints(){

	if(!auto_hinting){
		println("Not starting hint thread", DEBUG);
		return 0;
	}

	pthread_t hintt;

	if(pthread_create(&hintt, NULL, loop_hints,NULL)){
                println("failed to create hints thread",ERROR);
                return -1;
        }

	return 0;
}

void* loop_hints(){

	while(!shutting_down){
		struct timespec tim;
	        tim.tv_sec = 0;
                tim.tv_nsec = PATROL_INTERVAL_NS;
                
                if(nanosleep(&tim , NULL) < 0 ){
                        println("interupt in game loop! sleep failed",ERROR);
                }

		if(check_autohints() < 0){
			println("Failed to check autohints!!", ERROR);
			continue;
		}
	
	}

	return NULL;
}

int check_autohints(){

	json_object* root_dependencies = json_object_object_get(config_glob, 
		"hint_depenencies");

	/* This is used to only trigger an hint on a high flank, rather than a
	 * low flank*/
	static bool last = false;

		for(size_t hint_lvl = 0; hint_lvl < 
		json_object_array_length(root_dependencies); hint_lvl ++){

		json_object* hints_lvl = json_object_array_get_idx(
			root_dependencies, hint_lvl);

		for(size_t i = 0; i < json_object_array_length(hints_lvl); i++){
			
			json_object* dependency = json_object_array_get_idx(
				hints_lvl, i);

			int eval = check_dependency(dependency);
			if(eval >= 0){
				if(eval > 0){
					/* Continue to check until everything 
					 * is done */
					continue;
				}else{
					/* Quit if a dependency is not forfilled
					 */
					 goto hint_not_ready;
				}
			}else{
				println("FAILED TO CHECK DEPENDENCY! DUMPING!",
					ERROR);
				json_object_to_fd(STDOUT_FILENO, dependency, 
					JSON_C_TO_STRING_PRETTY);
				return -1;
			}

		}

		if(last){
			break;
		}

		size_t event_id = get_highest_active_event() + 1;

		/* End up here, if ready to execute hint */
		println("Ready to execute hint for level %i and event %i", 
			DEBUG, hint_lvl, event_id);
		
		/* I don't even pretend to bother, wether the hint has actually
		 * been played, or nit */
		 execute_hint(event_id, hint_lvl);
		 
		 last = true;
		 break;
		
hint_not_ready:
		/* Continue to check the other hints */
		last = false;
		continue;
	}

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
