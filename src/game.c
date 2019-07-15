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
#include "ecproto.h"
#include "module.h"

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

/*
 * Initializes core gmae components and all dependencies. Trigger initialisation
 * is as of now still a TODO
 */
int init_game(){

	pthread_mutex_init(&game_trigger_lock, NULL);

        const char* name = get_name_from_object(config_glob);
	dependency_list = NULL;
	dependency_count = 0;

        if(name == NULL){
                
                println("Initializing game: WTF?",DEBUG);

        }else{

                println("Initializing game: %s",DEBUG , name);
        }

        memset(&game_thread, 0, sizeof(game_thread));
        
        /* Allocate space for states */
	json_object* events = json_object_object_get(config_glob, "events");
	if(!json_object_is_type(events, json_type_array)){
		println("Global events object is NOT an array! Dumping root:",
			ERROR);
		json_object_to_fd(STDIN_FILENO, events,
			JSON_C_TO_STRING_PRETTY);
		return -1;
	}
        
	event_cnt = json_object_array_length(events);
        event_trigger_status = malloc(event_cnt * sizeof(uint8_t));
	
	if(event_trigger_status == NULL){
		println("Failed to allocate space for event trigger states!",
			ERROR);
		return -1;
	}
        memset(event_trigger_status, EVENT_RESET, event_cnt * sizeof(uint8_t));
        
        println("a total of %i events has been loaded", DEBUG, event_cnt);

	/* initialize all dependencys of all events. Dependencies may have
	 * sub-dependencies, which also need initilisation
	 */
	size_t depcnt = 0;
	size_t* event_ids = NULL;
	json_object** dependencies = get_root_dependencies(&depcnt, &event_ids);
	
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
	reset_jobs = malloc(event_cnt * sizeof(bool*));
	if(reset_jobs == NULL){
		println("Failed to allocate memory for reset_jobs!!", ERROR);
		return -1;
	}
	memset(reset_jobs, 0, event_cnt * sizeof(bool*));

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
		
		/* This doesn't use the trigger maybe function due to the fact
		 * that the startup should only be considered finished if that
		 * is finished. This method always triggers synchroneous.
		 */
		trigger_event(trig, false);
	}

        return 0;
}

int patrol(){

        for(size_t i = 0; i < event_cnt; i++){
		/* Check all dependencys of all states and trigger
		 * if nescessary.
		 */

		if(event_trigger_status[i] != EVENT_RESET && !reset_jobs[i]){
			/* Ommit already triggered events which dont have a
			 * reset job 
			 */
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
			/* I would consider this a bit stupid configured... */
			println("event without dependencies!!! triggering", 
				WARNING);
			trigger_event_maybe_blocking(i, false);
		}
		bool met = true;
		for(size_t dep = 0; dep < json_object_array_length(
			event_depends); dep++){
			int check = check_dependency(json_object_array_get_idx(
				event_depends, dep), NULL);

			if(check < 1){
				met = false;
				break;
			}
		}
		
		if(met && !reset_jobs[i]){
			println("All dependencies clear to execute event %i!",
				 INFO, i);
			trigger_event_maybe_blocking(i, false);

		}else if(!met && reset_jobs[i]){
			/* If there is a reset job pending for this event,
			 * reset it, and clear the reset job. 
			 */
			if(reset_jobs[i]){
				reset_jobs[i] = false;
				untrigger_event(i);
				println("Reset job for event %i done!", DEBUG,
					i);
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

int trigger_event(size_t event_id, bool enforced){
        
        if(event_cnt <= event_id){

                println("Tried to trigger invalid event %i", WARNING, 
			event_id);
                return -1;

	}else if(event_trigger_status[event_id] > 
			EVENT_IN_PREPARATION){
                
		println("Tried to trigger already triggered event %i", WARNING, 
			event_id);
                return -1;

        }
	
	json_object* event = json_object_array_get_idx(json_object_object_get(
		config_glob,"events"), event_id);
	
	json_object* async_o = json_object_object_get(event, "async");
	bool async = false;
	if(async_o != NULL){
		/* true and 1 are bot okay in my mind... */
		if(!json_object_is_type(async_o, json_type_boolean) &&
			!json_object_is_type(async_o, json_type_int)){
			
			println("Async field is of non-boolean type!", ERROR);
			return -1;
		}
	}
	/* Asyncroneous events should be completely independent from regular
	 * execution
	 */
	if(!async){
		pthread_mutex_lock(&game_trigger_lock);

	}
	event_trigger_status[event_id] = EVENT_IN_EXECUTION;
	/* Sleep 1 ms to prevent unintended collisions */
	sleep_ms(1);
        
       
       
        /* Iterate through triggers */

        json_object* triggers = json_object_object_get(event,"triggers");

	if(triggers == NULL){
		println("Attempted to trigger an event without triggers", INFO);
		/* It was still a success i guess */
	}else{

		println("Triggering %i triggers for event %s/%i",DEBUG, 
			json_object_array_length(triggers),
			get_name_from_object(event), event_id);
		
		for(size_t triggercnt = 0; triggercnt < 
			json_object_array_length(triggers); triggercnt++){
		
			json_object* trigger = json_object_array_get_idx(
				triggers,triggercnt);
		

			if(execute_trigger(trigger, false) < 0){
				println("FAILED TO EXECUTE TRIGGER! RESETTING!", 
					ERROR);
				event_trigger_status[event_id] = EVENT_RESET;
				/* untrigger event */
				
				if(!async){

					pthread_mutex_unlock(
						&game_trigger_lock);
				}
				return -1;
			}

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

	if(event_trigger_status[event_id] != EVENT_IN_EXECUTION){
		println("Event execution status has changed during execution."
			" Leaving the execution status unchanged",
			DEBUG);
	}else{
		if(enforced){
		
			println("Event %li has been executed FORCEFULLY", 
				DEBUG_MORE,  event_id);
			event_trigger_status[event_id] = 
				EVENT_TRIGGERED_ENFORCED;

		}else{
			println("Event %li has been executed peacefully", 
				DEBUG_MORE, event_id);
			event_trigger_status[event_id] = EVENT_TRIGGERED;
		}
	}

	if(!async){
		pthread_mutex_unlock(&game_trigger_lock);
	}

        return 0;
}

struct event_trigger_parameter_t {
	size_t event_id;
	bool enforced;
};

/*
 * This is a helper function used to execute the triggers and return afterwards.
 */
void* helper_async_trigger(void* params){

	struct event_trigger_parameter_t *pa = params;


	if(trigger_event(pa->event_id, pa->enforced) < 0){

		println("Failed to asynchroneously execute event. Not handling"
			,ERROR);
		free(pa);
		return NULL;
	
	}

	free(pa);
	return NULL;
}

/*
 * Spawns a new posix thread which executes an event's triggers.
 */

void async_trigger_event(size_t event_id, bool enforced){

	
	if(event_trigger_status[event_id] != EVENT_RESET){
		/* Shit happens */
		return;
	}
	
	if(event_id >= event_cnt){
		println("Tried to asynchroneously trigger a too high event %i!",
			ERROR, event_id);
		return;
	}

	/* Mark the event as currently beeing triggered to avoid a race 
	 * condition with the loop thread where the new thread would spawn too
	 * late and the main thread would re-trigger the same event.
	 */
	event_trigger_status[event_id] = EVENT_IN_PREPARATION;

	struct event_trigger_parameter_t *event_params = 
		malloc(sizeof(struct event_trigger_parameter_t));
	
	if(event_params == NULL){
		println("Failed to allocate memory for async ev param buffer!",
			ERROR);
		println("WOW, I can't even allocate 8 byte of ram... da fuck?",
			ERROR);
		return;
	}

	event_params->event_id = event_id;
	event_params->enforced = enforced;

	pthread_t executing_thread;

        if(pthread_create(&executing_thread, NULL, helper_async_trigger, 
		event_params)){
                println("failed to create asynchroneous trigger thread",ERROR);
                return;
        }

	return;
}

/*
 * This function triggers an event async if the async field in the event is set,
 * synchroneusly if not. This function is needed for inline execution
 *
 */
int trigger_event_maybe_blocking(size_t event_id, bool enforced){

	json_object* event = 
		json_object_array_get_idx(json_object_object_get(
		config_glob, "events"), event_id);

	if(event == NULL){
		println("Failed to get event by id [%i]. got NULL", 
			ERROR, event_id);
		return -1;
	}

	bool async = false;
	json_object* async_field = json_object_object_get(event, "async");
	if(async_field != NULL){
		async = json_object_get_boolean(async_field);

	}

	if(async){
		println("Triggering event %lu non-blocking!", DEBUG, event_id);
		async_trigger_event(event_id, enforced);
		return 0;
	}else{
		println("Triggering event %lu blocking!", DEBUG, event_id);
		return trigger_event(event_id, enforced);
	}

	return 0;

}

int untrigger_event(size_t event_id){
	json_object* event =  get_event_by_id(event_id);

	if(event == NULL){
		println("Failed to get event by id [%i]. got NULL", 
		ERROR, event_id);
		return -1;
	}

	/* Execute the untrigger triggers in the event. */
	json_object* untriggers = json_object_object_get(event, "untriggers");
	if(untriggers != NULL){
		if(trigger_array(untriggers) < 0){
			println("Failed to execute untriggers field in event"
				" [%i]! Dumping root:", ERROR, event_id);
			
			json_object_to_fd(STDOUT_FILENO,event, 
				JSON_C_TO_STRING_PRETTY);
			return -1;
		}

	}

	event_trigger_status[event_id] = EVENT_RESET;

	return 0;

	
}


/* This returns an array with all dependencies as elements. It can be 
 * free'd afterwards */
json_object** get_root_dependencies(size_t* depcnt, size_t** event_idsp){

	json_object** deps = NULL;
	size_t* event_ids = NULL;
	*depcnt = 0;
	
	/* 
	 * Iterate through events
	 */

	for(size_t event_i = 0; event_i < event_cnt; event_i++ ){
		json_object* event = get_event_by_id(event_i);
		if(event == NULL){
			println("Failed to get event %i!", ERROR, event_i);
			return NULL;
		}
		
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
	
	if(event_idsp != NULL){
		*event_idsp = event_ids;
	}


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
size_t get_hint_event(){
	
	size_t highest = 0; 

	for(size_t i = 0; i < event_cnt; i++){
		if(event_trigger_status[i] == EVENT_TRIGGERED){

			json_object* evnt =  json_object_array_get_idx(
				json_object_object_get(config_glob, "events"), 
				i);

			json_object* hint_allow = json_object_object_get(evnt, 
				"hintable");
			
			bool hintable = true;
			if(hint_allow != NULL){
				hintable = json_object_get_boolean(hint_allow);
			}
			
			if(hintable){
				highest = i;

			}
		}
	}

	return highest + 1;
}
int trigger_array(json_object* triggers){
	
	if(json_object_get_type(triggers) == json_type_array){
		
		for(size_t i = 0; i < json_object_array_length(triggers); i++){
			json_object* trigger = 
				json_object_array_get_idx(triggers, i);

			if(execute_trigger(trigger, false) < 0){
				println("Failed to execute %i trigger in array",
					ERROR, i);
				return -2;
			}
		}
		return 0;
	}else{
		println("Required array, but got single object! Dumping:", 
			ERROR);
	
		json_object_to_fd(STDOUT_FILENO,triggers, 
			JSON_C_TO_STRING_PRETTY);

		return -1;

	}
}

/* This returns an array with all triggers as elements. It can be 
 * free'd afterwards */
json_object** get_root_triggers(size_t* trigcnt, size_t** event_idsp){

	json_object** trigs = NULL;
	size_t* event_ids = NULL;
	*trigcnt = 0;
	
	/* 
	 * Iterate through events
	 */

	for(size_t event_i = 0; event_i < event_cnt; event_i++ ){
		
		json_object* event = json_object_array_get_idx(
			json_object_object_get(config_glob, "events"),event_i);
	
		json_object* triggers = 
			json_object_object_get(event,"triggers");
		
		if(json_object_get_type(triggers) != json_type_array){
			println("Triggers returned non-array! Probably "
				"undefined triggers in event. Dumping:", ERROR);
		
			json_object_to_fd(STDOUT_FILENO,event, 
				JSON_C_TO_STRING_PRETTY);
			return NULL;
			
		}

		for(size_t trig_i = 0; trig_i < json_object_array_length(
			triggers); trig_i++){
			
			trigs = realloc(trigs, ++*trigcnt * 
				sizeof(json_object*));
			event_ids = realloc(event_ids, *trigcnt * 
				sizeof(size_t));
			
			trigs[*trigcnt - 1] = json_object_array_get_idx(
				triggers, 
				trig_i);
			event_ids[*trigcnt - 1] = event_i;

		}
	}
	
	if(event_idsp != NULL){
		*event_idsp = event_ids;
	}else{
		free(event_ids);
	}

	if(trigs == NULL){
		println("Not a single regular tirgger was found! WTF are you "
			"doing with me?", ERROR);
		return NULL;
	}

	return trigs;
}

json_object* get_event_by_id(size_t event_id){
	
	json_object* events = json_object_object_get(config_glob, "events");
	if(events == NULL){
		println("Failed to get events from global config!!", ERROR);
		return NULL;
	}

	if(!json_object_is_type(events, json_type_array)){
		
		println("Events has wrong type!! Dumping root:", ERROR);
		json_object_to_fd(STDIN_FILENO, events,
			JSON_C_TO_STRING_PRETTY);
		return NULL;
	}

	if(event_id >= json_object_array_length(events)){
		
		println("Event id out of range!", ERROR);
		return NULL;
	}

	return json_object_array_get_idx(events, event_id);
}
