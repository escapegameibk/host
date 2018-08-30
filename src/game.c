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

#include "game.h"
#include "config.h"
#include "log.h"
#include "data.h"
#include "mtsp.h"
#include "sound.h"
#include "core.h"
#include "tools.h"

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

int init_game(){

        const char* name = get_name_from_object(config_glob);
	dependency_list = malloc(0);
	dependency_count = 0;

        if(name == NULL){
                
                println("Initializing game: %s",DEBUG, "UNSPECIFIED!");
        }else{

                println("Initializing game: %s",DEBUG , name);
        }

        memset(&game_thread, 0, sizeof(game_thread));
        
        /* Allocate space for states */
        state_cnt = json_object_array_length(json_object_object_get(
                config_glob,"events"));
        state_trigger_status = malloc(state_cnt * sizeof(bool));
        memset(state_trigger_status, 0, state_cnt * sizeof(bool));
        
        println("a total of %i states has been loaded", DEBUG, state_cnt);

	/* initialize all dependencys of all events */
	size_t depcnt = 0;
	size_t* event_ids = NULL;
	json_object** dependencies = get_all_dependencies(&depcnt, &event_ids);
	
	for(size_t i = 0; i < depcnt; i++){
		if(init_dependency(dependencies[i], event_ids[i]) < 0){
			println("Failed to init depedency no %i! Dumping:",
				ERROR, i);
			json_object_to_fd(STDIN_FILENO, dependencies[i],
				JSON_C_TO_STRING_PRETTY);
			return -1;

		}
	}
	
	/* Allocate the nescessary memory for the reset jobs and null it. */
	reset_jobs = malloc(state_cnt * sizeof(bool*));
	memset(reset_jobs, 0, state_cnt * sizeof(bool*));

	free(dependencies);
	free(event_ids);
        return 0;
}

/* This function adds metadata to the dependency, and add it to a global list.
 *  String comparisons of dependencies are no longer valid after this function
 */
int init_dependency(json_object* dependency, int event_id){
	
	/* Add the dependency to the global array */	
	
	dependency_list = realloc(dependency_list, ++dependency_count * 
		sizeof(json_object *));
	dependency_list[dependency_count - 1] = dependency;

	/* Add metadata to the dependency */
	json_object_object_add(dependency, "event_id", json_object_new_int(
		event_id));

	return init_general_dependency(dependency);
}

/* This function passes the dependency to it's module. It may add metadata,
 * but still prepares the dependency so it may be retrieved via the check 
 * dependency function. */
int init_general_dependency(json_object* dependency){
	
	/* Add metadata to dependency */
	json_object_object_add(dependency, "id", json_object_new_int(
		dependency_count - 1));

	const char* module_name = json_object_get_string(
				json_object_object_get(dependency,"module"));

	if(module_name == NULL){
		println("Specified no module in dependency!", ERROR);
		return -1;
	}
#ifndef NOMTSP
	else if(strcasecmp(module_name,"mtsp") == 0){
		/* Question the MTSP module */
		return mtsp_init_dependency(dependency);
	}
#endif
	else if(strcasecmp(module_name,"core") == 0){
		/* Question the core module */
		return core_init_dependency(dependency);
	}
	
	else if(strcasecmp(module_name,"snd") == 0){
		/* Question the core module */
		return snd_init_dependency(dependency);
	}
	else{
		println("Unknown module specified [%s]!", ERROR, module_name);
		return -2;
	}

	return 0;
	
}

/* I may have to tell you, that this is NOT, and i repeat NOT a function used
 * to start a game, but rather to start a thread, which controlls the game.
 */

int start_game(){

        if(pthread_create(&game_thread, NULL, loop_game,NULL)){
                println("failed to create game thread",ERROR);
                return -1;
        }

	/* If defined execute an init trigger */
	json_object* init_trigger = json_object_object_get(config_glob, 
		"init_event");

	if(init_trigger != NULL){
		size_t trig = json_object_get_int(init_trigger);

		println("initially triggering event no %i", DEBUG, trig);

		trigger_event(trig);
	}

        return 0;
}

int patrol(){

       
        for(size_t i = 0; i < state_cnt; i++){
		/* Check all dependencys of all states and trigger
		 * if nescessary.
		 */

		if(state_trigger_status[i]){
			/* Ommit already triggered events */
			continue;
		}
		json_object* event = json_object_array_get_idx(
			json_object_object_get(config_glob,"events"),i);
		if(event == NULL){
			println("Failed to iterate events", ERROR);
			return -1;
		}

		json_object* event_depends = json_object_object_get(event
			,"dependencies");

		if(event_depends == NULL){
			println("event without dependencies!!! triggering", 
				WARNING);
			trigger_event(i);
		}
		bool met = true;
		for(size_t dep = 0; dep < json_object_array_length(
			event_depends); dep++){
			int check = check_dependency(json_object_array_get_idx(
				event_depends, dep));

			if(check < 1){
				met = false;
				break;
			}
		}
		
		if(met){
			println("All dependencies clear to execute event %i!",
				 INFO, i);
			trigger_event(i);
		}else{
			/* If there is a reset job pending for this event,
			 * reset it, and clear the reset job. 
			 */
			if(reset_jobs[i]){
				reset_jobs[i] = false;
				state_trigger_status[i] = false;
				println("Reset job for event %i done!", DEBUG);
			}
		}
                
        }
        
        return 0;
}

void* loop_game(){

        struct timespec tim, tim2;

        while(!shutting_down){
                tim.tv_sec = 0;
                tim.tv_nsec = PATROL_INTERVAL_NS;
                
                if(nanosleep(&tim , &tim2) < 0 ){
                        println("interupt in game loop! sleep failed",ERROR);
                }

        if(patrol() < 0){
                        println("failed to patrol!!",ERROR);
                        continue;
                }
        }

        println("TERMINATING WATCHER", DEBUG);
        println("bailing out. you're on your own. good luck.",DEBUG);

        return NULL;
}

int trigger_event(size_t event_id){
        
        if((state_cnt <= event_id) | (state_trigger_status[event_id])){
                println("Tried to trigger invalid event %i", WARNING, event_id);
                return -1;
        }

	static bool trigger_lock = false;
	while(trigger_lock){/* NOP */}
	trigger_lock = true;
	
	/* Sleep 1 ms to prevent unintended collisions */
	sleep_ms(1);
        
        state_trigger_status[event_id] = 1;
       
	json_object* event = json_object_array_get_idx(json_object_object_get(
		config_glob,"events"), event_id);
       
        /* Iterate through triggers */
       

         json_object* triggers = json_object_object_get(event,"triggers");

	if(triggers == NULL){
		println("Attempted to trigger an event without triggers", INFO);
		/* It was still a success i guess */
		trigger_lock = false;
		return 0;
	}

        println("Triggering %i triggers for event %s/%i",DEBUG, 
		json_object_array_length(triggers),get_name_from_object(event), 
		event_id);
		
	for(size_t triggercnt = 0; triggercnt < 
		json_object_array_length(triggers); triggercnt++){
		
		json_object* trigger = json_object_array_get_idx(triggers,
                        triggercnt);
		

		if(execute_trigger(trigger) < 0){
			println("FAILED TO EXECUTE TRIGGER! RESETTING!", ERROR);
                        state_trigger_status[event_id] = 0;
			/* untrigger event */
			trigger_lock = false;
			return -1;
		}

	}
	
	/* If the event was marked for autoreset, create a reset job for it */
	json_object* autoreset = json_object_object_get(event,"autoreset");

	if(autoreset != NULL){
		if(json_object_get_boolean(autoreset)){
			println("Creating reset job for event %i", DEBUG, 
				event_id);
			reset_jobs[event_id] = true;
		}
	}

	println("Done triggering event!", DEBUG);
	trigger_lock = false;

        return 0;
}


/*
 * This is a helper function used to execute the triggers and return afterwards.
 */
void* helper_async_trigger(void* event_id){

	size_t* ev = event_id;

	if(trigger_event(*ev) < 0){

		println("Failed to asynchroneously execute event. Not handling"
			,ERROR);
		free(event_id);
		return NULL;
	
	}

	free(event_id);
	return NULL;
}

/*
 * Spawns a new posix thread which executes an event's triggers.
 */

void async_trigger_event(size_t event_id){

	
	if(state_trigger_status[event_id]){
		/* Shit happens */
		return;
	}

	size_t *event_idp = malloc(sizeof(size_t));
	*event_idp = event_id;

	pthread_t executing_thread;

        if(pthread_create(&executing_thread, NULL, helper_async_trigger, 
		event_idp)){
                println("failed to create asynchroneous trigger thread",ERROR);
                return;
        }

	return;

}


int execute_trigger(json_object* trigger){


	const char* module = json_object_get_string(json_object_object_get(
                trigger,"module"));
	println("Executing trigger %s", DEBUG, get_name_from_object(trigger));
        
        /* Find out which module is concerned and execute the trigger
         * in the specified function of the module.
         */

        if(strcasecmp(module,"core") == 0){
                /* The core module is concerned. */
                if(core_trigger(trigger) < 0){
                        println("Failed to execute trigger for core!",
                                ERROR);
                        return -1;
                }
		return 0;
        }
 
#ifndef NOMTSP
	if(strcasecmp(module,"mtsp") == 0){
                /* The mtsp module is concerned. */
                if(mtsp_trigger(trigger) < 0){
                        println("Failed to execute trigger for mtsp!",
                                ERROR);
                        
                        return -1;

                }
		return 0;
	}
#endif

#ifndef NOSOUND

	if(strcasecmp(module,"snd") == 0){
		/* The sound module is concerned. */
		if(sound_trigger(trigger) < 0){
			println("Failed to execute trigger for snd!",
				ERROR);
                       
			return -1;
		}
		return 0;
	}
#endif
        println("UNKNOWN MODULE %s!!",ERROR, module);
	return -2;

}


/* Checks wether a dependency is met
 * Returns < 0 on error, 0 if not, and > 0 if forfilled.
 */

int check_dependency(json_object* dependency){
	
	const char* module_name = json_object_get_string(
				json_object_object_get(dependency,"module"));
	if(module_name == NULL){
		println("Specified no module in dependency!", ERROR);
		return -1;
	}
#ifndef NOMTSP
	else if(strcasecmp(module_name,"mtsp") == 0){
		/* Question the MTSP module. yes QUESTION IT! */
		return mtsp_check_dependency(dependency);
	}
#endif
	else if(strcasecmp(module_name,"core") == 0){
		/* Question the core module */
		return core_check_dependency(dependency);
	}
	else{
		println("Unknown module specified [%s]!", ERROR, module_name);
		return -2;
	}


	return 0;
}

/* This returns an array with all dependencies as elements. It can be 
 * free'd afterwards */
json_object** get_all_dependencies(size_t* depcnt, size_t** event_idsp){

	json_object** deps = NULL;
	size_t* event_ids = NULL;
	*depcnt = 0;
	
	/* 
	 * Iterate through events
	 */

	for(size_t event_i = 0; event_i < state_cnt; event_i++ ){
		
			json_object* event = json_object_array_get_idx(
		json_object_object_get(config_glob, "events"),event_i);
		
		for(size_t dep_i = 0; dep_i < json_object_array_length(
			json_object_object_get(event,"dependencies")); 
			dep_i++){
			
			deps = realloc(deps, ++*depcnt * sizeof(json_object*));
			event_ids = realloc(event_ids, *depcnt * 
				sizeof(size_t));
			
			deps[*depcnt - 1] = json_object_array_get_idx(
				json_object_object_get(event,"dependencies"), 
				dep_i);
			event_ids[*depcnt - 1] = event_i;

		}
	}
	
	*event_idsp = event_ids;


	return deps;
}

/* 
 * This returns a dependency's id, or it's place in the global dependency
 * array. This is needed for various things, e.g nested dependencys, which are
 * supported by some modules like the core. This functions returns >=0 on
 * sucess and <0 on error.
 */
int get_dependency_id(json_object* dependency){
	
	json_object* id = json_object_object_get(dependency, "id");

	if(id != NULL){
		return json_object_get_int(id);
	}
		
	println("Probably uninitialized dependency! Dumping", ERROR);
	json_object_to_fd(STDOUT_FILENO,dependency, JSON_C_TO_STRING_PRETTY);
	
	return -1;
}

/* The main reason this function exists is, that the auto-hinting system uses
 * this, to find out, which hints to execute. Returns 0 in case nothing has been
 * triggered yet as well as in case only the 0 event has been executed. */
size_t get_highest_active_event(){
	
	size_t highest = 0; 

	for(size_t i = 0; i < state_cnt; i++){
		if(state_trigger_status[i] == 0){
			highest = i;
		}
	}

	return highest;
}
