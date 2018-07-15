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

#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

int init_game(){

        const char* name = json_object_get_string(json_object_object_get(
                                config_glob, "name"));
        if(name == NULL){
                
                println("Initializing game: %s",DEBUG, "UNSPECIFIED!");
        }else{

                println("Initializing game: %s",DEBUG , name);
        }

        memset(&game_thread, 0, sizeof(game_thread));
        /* initialize timer values */
        game_timer_start = 0;
        game_duration = json_object_get_int64(
                json_object_object_get(config_glob,"duration"));
        if(game_duration == 0){
                println("game duration not specified! defaulting to %i",WARNING,
                        DEFAULT_GAME_TIME);
                game_duration = DEFAULT_GAME_TIME;
        }
        println("game duration configured to be %i",DEBUG,game_duration);
        
        /* Allocate space for states */
        state_cnt = json_object_array_length(json_object_object_get(
                config_glob,"events"));
        state_trigger_status = malloc(state_cnt * sizeof(bool));
        memset(state_trigger_status, 0, state_cnt * sizeof(bool));
        
        println("a total of %i states has been loaded", DEBUG, state_cnt);
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
			println("All dependencies clear to exdcute event %i!",
				 INFO, i);
			trigger_event(i);
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

        println("TERMINATING WATCHER", INFO);
        println("bailing out. you're on your own. good luck.",DEBUG);

        return NULL;
}

int trigger_event(size_t event_id){
        
        if((state_cnt <= event_id) | (state_trigger_status[event_id])){
                println("Tried to trigger invalid event %i", WARNING, event_id);
                return -1;
        }
        
        state_trigger_status[event_id] = 1;
        /* Iterate through triggers */
       
         json_object* triggers = json_object_object_get(
                json_object_array_get_idx(json_object_object_get(config_glob,
                 "events"),event_id),"triggers");

	if(triggers == NULL){
		println("Attempted to trigger an event without triggers", INFO);
		/* It was still a success i guess */
		return 0;
	}

        println("Triggering %i triggers ",DEBUG, json_object_array_length(triggers));
         for(size_t triggercnt = 0; triggercnt < 
                json_object_array_length(triggers); triggercnt++){
                json_object* trigger = json_object_array_get_idx(triggers,
                        triggercnt);
                println("Executing trigger for event %i: %s",DEBUG, event_id,
                        json_object_get_string(json_object_object_get(trigger,
                        "name")));
                const char* module = json_object_get_string(json_object_object_get(
                        trigger,"module"));
                
                /* Find out which module is concerned and execute the trigger
                 * in the specified function of the module.
                 */

                if(strcasecmp(module,"core") == 0){
                        /* The core module is concerned. */
                        if(core_trigger(trigger) < 0){
                                println("Failed to execute trigger for core!",
                                        ERROR);
                                /* Untrigger event */
                                state_trigger_status[event_id] = 0;
                                return -1;
                        }
                        continue;
                }
 
#ifndef NOMTSP
                if(strcasecmp(module,"mtsp") == 0){
                        /* The mtsp module is concerned. */
                        if(mtsp_trigger(trigger) < 0){
                                println("Failed to execute trigger for mtsp!",
                                        ERROR);
                                
                                /* Untrigger event */
                                state_trigger_status[event_id] = 0;
                                return -1;
                        }
                        continue;
                }
#endif

#ifndef NOSOUND

                if(strcasecmp(module,"mtsp") == 0){
                        /* The sound module is concerned. */
                        if(sound_trigger(trigger) < 0){
                                println("Failed to execute trigger for snd!",
                                        ERROR);
                                
                                /* Untrigger event */
                                state_trigger_status[event_id] = 0;
                                return -1;
                        }
                        continue;
                }
#endif
                println("UNKNOWN MODULE %s!!",ERROR, module);
                state_trigger_status[event_id] = 0;
                return -2;

        }
        

        return 0;
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
		/* Question the MTSP module */
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

		if(event >= state_cnt || event < 0){
			println("Invalid event specified in dependency.",
				ERROR);
			return -2;
		}else{
			return state_trigger_status[event];
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
        }else{
                println("Unknown core action specified: %s",ERROR, action_name);
                return -1;
        }
        
        return 0;
}
