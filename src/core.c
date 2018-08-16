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
#include "mtsp.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

size_t core_sequence_count = 0;
struct sequence_dependency_t **core_sequential_dependencies = NULL;

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
		struct timespec tim;
		tim.tv_sec = 0;
		tim.tv_nsec = PATROL_INTERVAL_NS;
		nanosleep(&tim, NULL);

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
		println("Registered sequence with %i elements", DEBUG,
			json_object_array_length(sequence));
		json_object* sub_dependencies = json_object_object_get(
			dependency, "dependencies");
		for(size_t i = 0; i < json_object_array_length(sub_dependencies
			);i ++){

			init_dependency(json_object_array_get_idx(
				sub_dependencies,i));
		}
		

		struct sequence_dependency_t* seq = malloc(sizeof(struct 
			sequence_dependency_t));
		memset(seq, 0, sizeof(struct sequence_dependency_t));
		json_object* dep_cpy = NULL;
		json_object_deep_copy(dependency, &dep_cpy, 
			json_c_shallow_copy_default);
		seq->dependency = dep_cpy;
		seq->dependency_id = get_dependency_id(dependency);
		seq->sequence_length = sequence_length; 
		seq->target_sequence = calloc(1, sizeof(size_t) * 
			sequence_length);
		seq->sequence_so_far = calloc(1, sequence_length * 
			sizeof(size_t));
		/* Write the target sequence to the struct */
		for(size_t i = 0; i < sequence_length; i++){
			size_t dep = json_object_get_int(
				json_object_array_get_idx(sequence,i));

			seq->target_sequence[i] = dep;

		}
		/* Append the struct to the array */
		core_sequential_dependencies = realloc(
			core_sequential_dependencies, ++core_sequence_count 
			* sizeof(struct sequence_dependency_t *));
		core_sequential_dependencies[core_sequence_count - 1] = seq;
		println("Successfully initialized syequence with id %i", DEBUG, 
			seq->dependency_id);
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

	}else if(strcasecmp(type,"sequence") == 0){
	
		/* Search for the sequence in the array and compare it's
		 * current value with it's target
		 */

		 for(size_t i = 0; i < core_sequence_count; i++){

			struct sequence_dependency_t* seq = 
				core_sequential_dependencies[i];
			if(seq->dependency_id != get_dependency_id(dependency)){
				continue;
			}

			/* Return ">=0 == true" compliant */
			for(size_t i = 0; i < seq->sequence_length; i++){
				if(seq->target_sequence[i] !=
					seq->sequence_so_far[i]){
					return 0;

				}
			}
			return 1;

		 }
		
		/* WTF?! */
		println("FOUND UNINITIALIZED SEQUENTIAL DEPENDENCY!!!", ERROR);
		println("THIS SHOULD BE IMPOSSIBLE!!!!", ERROR);
		return -3;

	}else{
		println("invalid type specified in core dependency : %s",
			ERROR, type);
		return -4;
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
		/* Reset sequence states */
		for(size_t i = 0; i < core_sequence_count; i++){
			struct sequence_dependency_t* seq = 
				core_sequential_dependencies[i];
				memset(seq->sequence_so_far, 0, 
					seq->sequence_length);
		}

		if(reset_mtsp() < 0){
			println("Failed to reset MTSP!", ERROR);
			return -1;
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
			core_sequential_dependencies[i];

		
		json_object* dependencies = json_object_object_get(
			sequence->dependency, "dependencies");

		if(dependencies == NULL){
			println("FAILED TO GET DEPENENDENCIES FOR SEQUENCE! \
DUMPING:",
				ERROR);
				/*json_object_to_fd(STDOUT_FILENO,
					sequence->dependency,
					JSON_C_TO_STRING_PRETTY);*/
				return -1;

		}

		for(size_t dep = 0; dep < 
			json_object_array_length(dependencies); 
			dep++){
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
				return -2;
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

				println("Adding %i to sequence with id %i:", 
					DEBUG, dep, sequence->dependency_id);
				json_object* trigger = json_object_object_get(
					sequence->dependency, "update_trigger");

				if(trigger != NULL){
					println(
"Triggering aditional dependencies for sequence update."
						, DEBUG);

					for(size_t i = 0; i < 
						json_object_array_length(
						trigger); i++){
						execute_trigger(
						json_object_array_get_idx(
						trigger,i));
					}
					

				}

				for(size_t i = 0; i < sequence->sequence_length;
					i++){
					println("%i:\t%i/%i", DEBUG, i, 
						sequence->sequence_so_far[i],
						sequence->target_sequence[i]);
				}
				
			}

		}

	}

	return 0;
}
