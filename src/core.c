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
#include "hints.h"
#include "module.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

size_t core_sequence_count = 0;
struct sequence_dependency_t **core_sequential_dependencies = NULL;

size_t flank_dependency_count = 0;
struct flank_dependency_t** flank_dependencies = NULL;

struct length_dependency_t** length_dependencies = NULL;
size_t length_dependency_count = 0;

int init_core(){
	
	println("Initializing core components done!", DEBUG);

	/* initialize timer values */
        game_timer_start = 0;
	game_timer_end = 0;

#ifndef NOALARM
	/* Initialize alarm */
	alarm_on = false;
#endif

        game_duration = json_object_get_int64(
                json_object_object_get(config_glob,"duration"));
        if(game_duration == 0){
                println("game duration not specified! defaulting to %i",WARNING,
                        DEFAULT_GAME_TIME);
                game_duration = DEFAULT_GAME_TIME;
        }
        println("game duration configured to be %i",DEBUG,game_duration);

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
			println("Failed to check sequential dependencies!", 
				ERROR);
		}

		if(core_update_lengths() < 0){
			
			println("Failed to check length dependencies!", 
				ERROR);

		}
		struct timespec tim;
		tim.tv_sec = 0;
		tim.tv_nsec = PATROL_INTERVAL_NS;
		nanosleep(&tim, NULL);

	}

	println("Core component watcher leaving operational state", DEBUG);
	
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
				sub_dependencies,i), json_object_get_int( 
				json_object_object_get(dependency, "event_id")));
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
		/* In order to avoid unintended updates prevent them from 
		 * happening! Saves the real value of the match to do so.
		 */
		seq->pointer = 0;
		
		/* Setup flanks */
		seq->last_dependencies = malloc(sequence_length * sizeof(int));
		memset(seq->last_dependencies, 0, 
			sequence_length * sizeof(int));

		/* Append the struct to the array */
		core_sequential_dependencies = realloc(
			core_sequential_dependencies, ++core_sequence_count 
			* sizeof(struct sequence_dependency_t *));

		core_sequential_dependencies[core_sequence_count - 1] = seq;

		println("Successfully initialized syequence with id %i", DEBUG, 
			seq->dependency_id);
	}else if(strcasecmp(type,"flank") == 0){
		/* Flank dependencies have an internal struct wich has to be
		 * initialized. */

		struct flank_dependency_t* dep = malloc(sizeof(struct
			flank_dependency_t));
		memset(dep, 0, sizeof(struct flank_dependency_t));

		json_object* high = json_object_object_get(dependency,"high");
		if(high == NULL || json_object_get_boolean(high)){
			dep->high = true;
		}
		
		json_object* low = json_object_object_get(dependency,"low");
		if(low == NULL || json_object_get_boolean(low)){
			dep->low = true;
		}
		dep->id = get_dependency_id(dependency);

		/* As initial state the dependency has -1 as it's last value */
		dep->last = -1;
		flank_dependencies = realloc(flank_dependencies, 
			sizeof(struct flank_dependency_t*) * ++flank_dependency_count);
		flank_dependencies[flank_dependency_count - 1] = dep;

		/* Initialize the sub-dependency */
		return init_dependency(json_object_object_get(dependency,
			"dependency"),json_object_get_int(
			json_object_object_get(dependency,"event_id")));
	}else if(strcasecmp(type,"or") == 0){
		json_object* deps = json_object_object_get(dependency, 
			"dependencies");
		for(size_t i = 0; i < json_object_array_length(deps); i++){
			init_dependency(json_object_array_get_idx(deps, i),
				json_object_get_int(json_object_object_get(
				dependency,"event_id")));
		}

	}else if(strcasecmp(type,"length") == 0){
		/* Length depenencies are used to see, wheter a dependenncy
		 * is triggered for longer than a certain amount of time.
		 */
		json_object* sub_dependency = json_object_object_get(dependency,
			"dependency");

		if(sub_dependency == NULL){

			println("Specfied length dep. without inner dependency!"
				, ERROR);
				
			json_object_to_fd(STDOUT_FILENO, dependency,
				JSON_C_TO_STRING_PRETTY);
			return -1;
		}

		json_object* threshold = json_object_object_get(dependency, 
			"length");
		if(threshold == NULL){
			
			println("Specified length dep. without length:",
				ERROR);
				
			json_object_to_fd(STDOUT_FILENO, dependency,
				JSON_C_TO_STRING_PRETTY);
			return -2;
			
		}

		size_t length = json_object_get_int(threshold);
		
		json_object* below = json_object_object_get(dependency, 
			"below");
		
		
		bool below_val = false;

		if(below != NULL){
			below_val = json_object_get_boolean(below);
		}
		
		json_object* above = json_object_object_get(dependency, 
			"above");
		
		
		bool above_val = true;

		if(above != NULL){
			above_val = json_object_get_boolean(above);
		}

		if(above_val && below_val){
			println("You specified a sependency which needs to be \
either below or above a certain threshold. Do you know what a threshold is?",
				WARNING);
		}
		
		struct length_dependency_t* dep = malloc(sizeof(struct 
			length_dependency_t));

		memset(dep, 0, sizeof(struct length_dependency_t));

		dep->below = below_val;
		dep->above = above_val;
		dep->id = get_dependency_id(dependency);
		dep->root_dependency = dependency;
		dep->sub_dependency = sub_dependency;
		dep->threshold = length;
		dep->activation = 0;	/* 0 represents never */

		/* Store the struct to the global array */
		length_dependencies = realloc(length_dependencies, 
			sizeof(struct length_dependency_t**) * 
			++length_dependency_count);
		length_dependencies[length_dependency_count-1] = dep;

		/* Initialize the sub-dependency */
		return init_dependency(sub_dependency, json_object_get_int(
			json_object_object_get(dependency,"event_id")));

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
	
	}else if(strcasecmp(type,"flank") == 0){
		/* Look for the right flank dependency and check wether a
		 * flank has occured or not and return accordingly */
		 for(size_t i = 0; i < flank_dependency_count; i++){
			struct flank_dependency_t* dep = flank_dependencies[i];
			if(dep->id != get_dependency_id(dependency)){
				continue;
			}

			/* We have a winner here */
			int last = dep->last;
			int now = check_dependency(json_object_object_get(
				dependency,"dependency"));
			if(now == last){
				/* Boring */
				return false;
			}
			
			if(last == -1){
				/* Error correction and init state */
				dep->last = now;
				return 0;
			}

			if(last < now){
				dep->last = now;
				return dep->high;
			}else{
				dep->last = now;
				return dep->low;

			}

		 }
		
		/* WTF?! */
		println("FOUND UNINITIALIZED FLANK DEPENDENCY!!!", ERROR);
		println("THIS SHOULD BE IMPOSSIBLE!!!!", ERROR);
		return -3;
		
	}else if(strcasecmp(type,"or") == 0){
		/* This is an OR of all dependencies given in the dependencies
		 * field */
		json_object* deps = json_object_object_get(dependency,
			"dependencies");
		for(size_t i = 0; i < json_object_array_length(deps); i++){

			int val = check_dependency(json_object_array_get_idx(
				deps,i));

			if(val != 0){
				return val;
			}

		}
		return false;

		
	}else if(strcasecmp(type,"never") == 0){
		/* This is unable to return true at any time */
		return false;
	}else if(strcasecmp(type,"length") == 0){
		/* This is unable to return true at any time */
		for(size_t i = 0; i < length_dependency_count; i++){
			struct length_dependency_t* dep = 
				length_dependencies[i];

			if(dep->id != get_dependency_id(dependency)){
				continue;
			}
			/* Found my struct */
			
			/* Currently the dependency is not fullfilled */
			if(dep->activation == 0){
				return 0;
			}

			unsigned long long int current_time = 
				(unsigned long long int) time(NULL);

			if(dep->below && current_time < (dep->activation + 
				dep->threshold)){
				return 1;
			}
			
			if(dep->above && current_time > (dep->activation + 
				dep->threshold)){
				return 2;
			}
			
			if(current_time == (dep->activation + dep->threshold)){
				return 3;
			}

			return 0;
		}
		
		/* WTF?! */
		println("FOUND UNINITIALIZED LENGTH DEPENDENCY!!!", ERROR);
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
                game_timer_start = (unsigned long long)time(NULL);

        }else if(strcasecmp(action_name,"timer_stop") == 0){
                println("Stopping game timer", INFO);
                game_timer_end = (unsigned long long)time(NULL);
        
	}else if(strcasecmp(action_name,"timer_reset") == 0){
                println("Clearing game timer", INFO);
                game_timer_end = 0;
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
			seq->pointer = 0;

		}
		if(reset_modules() < 0){
			println("Failed to reset modules!", ERROR);
			return -1;
		}
		json_object* default_lang = json_object_object_get(config_glob, 
			"default_lang");
		if(default_lang != NULL){
			int lang = json_object_get_int(default_lang);
			println("Reseting language to default %i", DEBUG, lang);
			language = lang;
		}

        }else if(strcasecmp(action_name,"delay") == 0){
		uint32_t delay = json_object_get_int64(
			json_object_object_get(trigger, "delay"));
		println("sleeping %ims!", DEBUG, delay);
		return sleep_ms(delay);
#ifndef NOALARM
	}else if(strcasecmp(action_name,"alarm") == 0){
		core_trigger_alarm();
		return 0;
	}else if(strcasecmp(action_name,"alarm_release") == 0){
		core_release_alarm();
		return 0;
#endif
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
				/* I don't know why this was commented out, but
				 * it should be pretty useful for debugging..
				 */
				json_object_to_fd(STDOUT_FILENO,
					sequence->dependency,
					JSON_C_TO_STRING_PRETTY);
				return -1;

		}

		for(size_t dep = 0; dep < 
			json_object_array_length(dependencies); 
			dep++){
			
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
				
				if(evaluated <=
					sequence->last_dependencies[dep]){
					sequence->last_dependencies[dep] = 
						state;
					continue;

				}else{
					sequence->last_dependencies[dep] = 
						state;

				}

				/* The dependency is fullfilled and the last
				 * one written to the so far array is NOT the
				 * same one. Write this one to the beginning
				 * and shift the rest.
				 */
				 memmove(&sequence->sequence_so_far[1], 
					&sequence->sequence_so_far[0], 
					(sequence->sequence_length - 1) * 
					sizeof(size_t));
				sequence->sequence_so_far[0] = dep;

				println("Adding %i to sequence with id %i:", 
					DEBUG, dep, sequence->dependency_id);

				size_t seq_pointer_old = sequence->pointer;
				bool pointer_reset = false;
				if(sequence->pointer >= 
					sequence->sequence_length){
					/* Overflow */
					sequence->pointer = 0;
					pointer_reset = true;
				}

				for(size_t i = 0; i <= sequence->pointer; i++){
					
					/* I'm sorry for this code mess, but I
					 * wanted to implement that this way. So
					 * what this is doing, is it is looking,
					 * if everything up to the currently
					 * added sequence match the targeted
					 * sequence. If not, then reset,
					 * if it does, continue to check. 
					 */

					if(sequence->sequence_so_far[
						sequence->pointer - i] !=
						sequence->target_sequence[
						sequence->sequence_length - i
						- 1]){

							sequence->pointer = 0;
							pointer_reset = true;
							break;
					}
				}

				if(!pointer_reset){
					sequence->pointer++;

				}

				if(sequence->pointer <= seq_pointer_old){
					/* The current pointer of the first
					 * element to match the sequence has
					 * been reset 
					 */
					println("Added wrong input to sequence."
						, DEBUG);
					
					json_object* restri = 
						json_object_object_get(
						sequence->dependency,
						"trigger_wrong");

					if(restri != NULL){
						if(trigger_array(restri) < 0){
							println(
"Failed to trigger wrong array in sequence dependency!",
								ERROR);
						}
					}
				}else{
					/* Successfully added an element to the
					 * sequence cue.
					 */
					println("Added right input to sequence."
						, DEBUG);
					json_object* cortri = 
						json_object_object_get(
						sequence->dependency,
						"trigger_right");

					if(cortri != NULL){
						if(trigger_array(cortri) < 0){
							println(
"Failed to trigger correct array in sequence dependency!",
								ERROR);
						}
					}

				}

				/* Covers an edge-case where the dependency
				 * was wring for the last attempt, but is 
				 * correct for the next one
				 */
				if((sequence->sequence_so_far[0] == 
					sequence->target_sequence[
					sequence->sequence_length - 1]) && 
					pointer_reset){
					
					sequence->pointer++;
					
				}
				
				json_object* trigger = json_object_object_get(
					sequence->dependency, "update_trigger");	
					
				if(trigger != NULL){
					println(
"Triggering aditional dependencies for sequence update."
						, DEBUG);

						if(trigger_array(trigger) < 0){
							println(
"Failed to trigger additional array in sequence dependency!",
								ERROR);
						}
					

				}

				for(size_t i = 0; i < sequence->sequence_length;
					i++){
					println("%i:\t%i/%i", DEBUG, i, 
						sequence->sequence_so_far[i],
						sequence->target_sequence[i]);
				}
				return 0;
				
			}

		}

	}

	return 0;
}

int core_update_lengths(){

	for(size_t i = 0; i < length_dependency_count; i++){
		struct length_dependency_t* dep = length_dependencies[i];

		int state = check_dependency(dep->sub_dependency);

		if(state < 0){
			println("Failed to check a sub-dep. of a length dep.!",
				ERROR);
			
			json_object_to_fd(STDOUT_FILENO, dep->root_dependency,
				JSON_C_TO_STRING_PRETTY);
			
			return -1;
		}

		if(check_dependency(json_object_object_get(dep->root_dependency,
			"lock")) > 0){
			/* This is now locked. DON'T DO ANYTHING ANYMORE */
			continue;
		}

		if((state == 1) && (dep->activation == 0)){
			/* This is my time to shine! */
			dep->activation = (unsigned long long int) time(NULL);

			json_object* start_trigger = json_object_object_get(
				dep->root_dependency, "start_triggers");

			if(start_trigger != NULL){
				return trigger_array(start_trigger);
			}

		}else if((state == 0) && (dep->activation != 0)){
			
			json_object* fail_trigger = json_object_object_get(
				dep->root_dependency, "fail_triggers");
			
			json_object* success_trigger = json_object_object_get(
				dep->root_dependency, "success_end_triggers");
			
			unsigned long long int current_time = 
				(unsigned long long int) time(NULL);

			if((dep->below && current_time < (dep->activation + 
				dep->threshold)) || 
				(dep->above && current_time > (dep->activation + 
				dep->threshold)) ||
				(current_time == (dep->activation + 
				dep->threshold))){
				
				dep->activation = 0;
				
				if(success_trigger == NULL){
					continue;
				}
				if(trigger_array(success_trigger) < 0){
					println("Failed to trigger len arr!",
						ERROR);
					return -1;
				}

			}else{
				dep->activation = 0;
				
				if(fail_trigger == NULL){
					continue;
				}
				if(trigger_array(fail_trigger) < 0){
					println("Failed to trigger len arr!",
						ERROR);
					return -1;
				}

			}
		}
	}

	return 0;
}

#ifndef NOALARM

/* Updated the alarm as needed. */
void core_trigger_alarm(){

	println("Triggered alarm!", DEBUG);
	/* If the alarm was already on, there is no reason to turn it off. */
	alarm_on = true;
	return;
}

void core_release_alarm(){

	println("Released alarm!", DEBUG);
	alarm_on = false;
	return;
}

#endif
