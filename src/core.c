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

#include "core.h"
#include "config.h"
#include "game.h"
#include "log.h"
#include "tools.h"
#include "data.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

size_t core_sequence_count = 0;
struct sequence_dependency_t *core_sequential_trigger = NULL;

int init_core(){
	
	println("Initializing core components done!", DEBUG);

	return 0;
}

int start_core(){

	pthread_t core_thread;
        if(pthread_create(&core_thread,NULL,core_loop,NULL)){               
                 println("failed to create core thread", ERROR);                      
                 return -2;                                                      
        }


	return 0;
}

void* core_loop(){

	while(!shutting_down){

		if(core_update_sequential() < 0){
			println("Failed to check sequential dependency!", 
				DEBUG);
		}

	}
	
	return NULL;
}


int core_init_dependency(json_object* dependency){
	
	json_object* type_obj = json_object_object_get(dependency, "type");
	
	if(type_obj == NULL){
		println("Core option without type. This will probably fail!",
			WARNING);
		return 1;
	}
	const char* type = json_object_get_string(type_obj);
	if(strcasecmp(type,"sequence") == 0){

		json_object* sequence = json_object_object_get(dependency,
			"sequence");
		if(sequence == NULL){
			println("specified sequence without any actual sequence"
			, ERROR);
			return -1;
		}
		size_t sequence_length = json_object_array_length(sequence);
		if(sequence_length == 0){
			println("Empty sequence detected!", ERROR);
			return -2;
		}
		println("Registered sequence with 3 elements", DEBUG);

		struct sequence_dependency_t seq = { dependency,
			sequence_length, 
			malloc(sizeof(size_t) * sequence_length),
			malloc(sequence_length * sizeof(size_t))};
		/* Write the target sequence to the struct */
		for(size_t i = 0; i < sequence_length; i++){
			size_t dep = json_object_get_int(
				json_object_array_get_idx(sequence,i));

			seq.target_sequence[i] = dep;

		}
		memset(seq.sequence_so_far, 0, 
			sequence_length * sizeof(size_t));
		/* Append the struct to the array */
	}
	/* Ignore everything else */



	return 0;
}

/* Checks wether a core dependency is met
 * Returns < 0 on error, 0 if not, and > 0 if forfilled.
 */

int core_check_dependency(json_object* dependency){

	const char* type = json_object_get_string(json_object_object_get(
		dependency,"type"));

	if(type == NULL){
		println("Unspecified type in core dependency!", ERROR);
		return -1;
	}else if(strcasecmp(type,"event") == 0){
		/* Check wether a trigger has already been executed */
		int32_t event = json_object_get_int(
			json_object_object_get(dependency,"event"));
		json_object* target_obj = json_object_object_get(dependency, 
			"target");

		bool target = true;

		if(target_obj != NULL){
			/* A target may be ommited if the event just needs to
			 * be triggered'
			 */
			 target = json_object_get_boolean(target_obj);

		}

		if(event >= (int32_t) state_cnt || event < 0){
			println("Invalid event specified in dependency.",
				ERROR);
			return -2;
		}else{
			return (state_trigger_status[event] == target);
		}
	}else{
		println("invalid type specified in core dependency : %s",
			ERROR, type);
		return -3;
	}

}

int core_trigger(json_object* trigger){
        const char* action_name = json_object_get_string(json_object_object_get(
                trigger, "action"));

        if(action_name == NULL){
                println("unable to trigger empty action! assuming nop",
                        WARNING);
                return 0;
        }else if(strcasecmp(action_name,"timer_start") == 0){
                println("Starting game timer", INFO);
                game_timer_start = (unsigned long)time(NULL);
        }else if(strcasecmp(action_name,"timer_stop") == 0){
                println("Stopping game timer", INFO);
                game_timer_start = 0;
        }else if(strcasecmp(action_name,"reset") == 0){
                println("Resetting states", INFO);
                for(size_t i = 0; i < state_cnt; i++){
                        state_trigger_status[i] = 0;
                }

        }else if(strcasecmp(action_name,"delay") == 0){
		uint32_t delay = json_object_get_int64(
			json_object_object_get(trigger, "delay"));
		println("sleeping %ims!", DEBUG, delay);
		return sleep_ms(delay);
	
	}else{
                println("Unknown core action specified: %s",ERROR, action_name);
                return -1;
        }
        
        return 0;
}

int core_update_sequential(){

	for(size_t i = 0; i < core_sequence_count; i++){
		
		struct sequence_dependency_t* sequence = 
			&core_sequential_trigger[i];
		
		json_object* dependencies = json_object_object_get(
			sequence->dependency, "dependencies");

		for(size_t dep = 0; dep < json_object_array_length(
			dependencies); dep++){
			if(sequence->sequence_so_far[0] == dep){
				continue;
			}
			
			json_object* dependency = json_object_array_get_idx(
				dependencies,dep);

			int state = check_dependency(dependency);
			if(state < 0){
				println("Failed to check dependency! dumping:",
					ERROR);
				json_object_to_fd(STDOUT_FILENO, dependency, 
					JSON_C_TO_STRING_PLAIN);
				return -1;
			}else{
				bool evaluated = !!(state);
				
				if(!evaluated){
					continue;
				}
				/* The dependency is fullfilles and the last
				 * one written to the so far array is NOT the
				 * same one. Write this one to the beginning
				 * and shift the rest.*/
				 memmove(&sequence->sequence_so_far[1], 
					&sequence->sequence_so_far[0], 
					(sequence->sequence_length - 1) * 
					sizeof(size_t));
				sequence->sequence_so_far[0] = dep;
			}

		}

	}

	return 0;
}
