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
                config_glob,"states"));
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
