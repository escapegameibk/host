/* a command interface via unix socket
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

#include "interface.h"
#include "log.h"
#include "data.h"
#include "tools.h"
#include "config.h"
#include "game.h"
#include "core.h"
#include "hints.h"

#include <json-c/json.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

int init_interface(){

#define INT_LEN 64
        char * debug_out = malloc(INT_LEN);
        memset(debug_out,0,INT_LEN);
        sprintf(debug_out,"initiating unix socket at %s",SOCKET_LOCATION);
        println(debug_out,DEBUG);
        free(debug_out);
        /* initiate the socket fd */
        struct sockaddr_un addr;

        if ( (unix_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                println("failed to create unix socket",ERROR);
                return -1;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_LOCATION, sizeof(addr.sun_path)-1);
        
        /* remove still persistant files.*/
        unlink(SOCKET_LOCATION);
       
        /* bind the socket to it's location */
        if(bind(unix_fd,(const struct sockaddr*)&addr,sizeof(addr)) != 0){
                println("failed to bind to unix socket",ERROR);
                close(unix_fd);
                return -2;
        }
        if(chmod(SOCKET_LOCATION,0x1FF)){ // 1FF is 777 in octal
                println("Failed to set socket permissions!",ERROR);
                close(unix_fd);
                return -3;
        }
        
        if(listen(unix_fd,10) != 0){
                println("failed to listen on unix socket",ERROR);
                close(unix_fd);
                return -4;
        }

        /* Initialize the pthread object */
        memset(&interface_thread, 0, sizeof(interface_thread));

        /* Debug and init for the json library */
        debug_out = malloc(INT_LEN);
        memset(debug_out,0,INT_LEN);
        sprintf(debug_out,"json library init. version: %s",JSON_C_VERSION);
        println(debug_out,DEBUG);
        free(debug_out);
        
#undef INT_LEN
        /* init successfull */
        return 0;
}

int start_interface(){

        println("starting unix socket handler",DEBUG);
             
        if(pthread_create(&interface_thread,NULL,loop_interface,NULL)){
                println("failed to create thread", ERROR);
                return -1;
        }

        return 0;
}

void * loop_interface(){

        println("interface entering operational state",DEBUG);

        do {
                int cl = 0;
                if ( (cl = accept(unix_fd, NULL, NULL)) == -1) {
                        /* unacceptable! */
                        println("failed to accept connection at unix socket",
                                        ERROR);
                        continue;
                        
                }
                
                handle_comm(cl);

        } while (!shutting_down);
        
        println("interface leaving operational state",DEBUG);

        close(unix_fd);
        unlink(SOCKET_LOCATION);

        return NULL;
}

int handle_comm(int fd){

#define BUFFERLENGTH 2560

        char buffer[BUFFERLENGTH];;

        while(!shutting_down){
                
                memset(buffer,0,BUFFERLENGTH);
                if(read(fd,buffer,BUFFERLENGTH) <= 0){
                        
                        break;
                }else{
                        execute_command(fd,buffer);
			break;
                }
        }
	
        close (fd);
#undef BUFFERLENGTH

        return 0;
}

/*
 * this function executes commands handed to it by clients.
 * commands are json objects which contain an integer for the action type
 * and the rest is undefined.
 */
int execute_command(int sock_fd, char* command){

        struct json_tokener *tok = json_tokener_new();
        struct json_object* parsed = json_tokener_parse_ex(tok, command,
                        strlen(command));
        char * debug;
#define INT_LEN 64

        if(parsed == NULL){
                /* failed to parse */
                debug = malloc(INT_LEN);
                memset(debug,0,INT_LEN);
                sprintf(debug,"invalid json received for fd %i",sock_fd);
                println(debug,WARNING);
                free(debug);
                return -1;
        }
        json_tokener_free(tok);

        /* every valid comand is required to have a action */
        struct json_object* act = json_object_object_get(parsed,"action");
        if(act == NULL){
                /* no action given */ 
                debug = malloc(INT_LEN);
                memset(debug,0,INT_LEN);
                sprintf(debug,"no action provided by fd %i",sock_fd);
                println(debug,WARNING);
                free(debug);
                free(parsed);
                return -2;
        }
        int32_t action = json_object_get_int(act);


        switch(action){
        case 0:
                /* NOOP */
                {};
                break;
        case 1:
                /* return info to the client*/

                print_info_interface(sock_fd);

                break;
        case 2:
                /* request update */
                /* this requestan update statement. all variables will be
                 * dumped, if they may change throughout the game.
                 */
                print_changeables_interface(sock_fd);
                break;

        case 3: 
                /* Return a complete list of events to the client.
                 * This ONLY returns an array of values, rather than the
                 * actual values.
                 */
                print_events_interface(sock_fd);
                break;

        case 4:
                        /* Force-triggers the incoming event and ignores all
                         * dependencies. It will not be triggerable until the
                         * the game is reset to the start state.
                         */
			println("Enforced trigger for event %i!", INFO, 
				json_object_get_int(json_object_object_get(
				parsed,"event")));
                        async_trigger_event(json_object_get_int(
                                json_object_object_get(parsed,"event")));
		break;
	case 5:
		/* Forced untrigger the incoming event. Does NOT reset
		the triggers. */

		println("Enforced un-trigger for event %i!", INFO, 
			json_object_get_int(json_object_object_get(
			parsed,"event")));

		/* This is REALLY dirty, but does the trick.*/
		state_trigger_status[json_object_get_int(
			json_object_object_get(parsed,"event"))] = false;

                break;
	case 6:
		
		/* Return all dependencies. Only their definitions, NOT their
		 * values!
		 */
		
		print_dependencies_interface(sock_fd);

		break;

	case 7:
		/* Returns all dependency states, without ANY other inforamtion
		 * about them in the same order as action 6 returns them .
		 */

		print_dependency_states_interface(sock_fd);
		break;
#ifndef NOHINTS
	case 8:
		/* Returns, if available, a list of all hints, structured into
		 * two arrays. The top one in order of items corresponding to
		 * an event, and the sub-arrays containing any hint. Please
		 * Consult the hints module for more information.
		 */
		print_hints_interface(sock_fd);
		break;
	case 9:
		/* Enfoces the execution of a hint.
		 */
		
		execute_hint(json_object_get_int(json_object_object_get(parsed,
			"event_id")),json_object_get_int(
			json_object_object_get(parsed, "hint_id")));
		break;
#endif /* NOHINTS */

#ifndef NOALARM
	case 10:
		
		/* Releases the alarm 
		 */
		 core_release_alarm();

		break;
#endif /* NOALARM */
        default:
                /* OOPS */
                debug = malloc(INT_LEN);
                memset(debug,0,INT_LEN);
                sprintf(debug,"invalid action %i by fd %i",action,sock_fd);
                println(debug,WARNING);
                free(debug);
                break;

        }


#undef INT_LEN
        /* cleanup */
        json_object_put(parsed);
        return 0;

}

int print_info_interface(int sock_fd){


        json_object* obj = json_object_new_object();
        json_object* version = json_object_new_array();
        
        int n = 0;

        n |= json_object_array_add(version,json_object_new_int(VERSION_MAJOR));
        n |= json_object_array_add(version,json_object_new_int(VERSION_MINOR));
        n |= json_object_array_add(version,json_object_new_int(
                                VERSION_RELEASE));
        if(n!=0){
             free(version);
             println("Error whilst printing version info to client",ERROR);
             return -1;
        }

        n |= json_object_object_add(obj,"version",version);
        n |= json_object_object_add(obj,"game",json_object_new_string(
                json_object_get_string(json_object_object_get(
                config_glob,"name"))));
                
        n|= json_object_object_add(obj, "duration",
                json_object_new_int64(game_duration));

#ifndef NOHINTS
	n |= json_object_object_add(obj, "hints", json_object_new_boolean(
		hints_enabled));
#endif

	json_object* color = json_object_object_get(config_glob, "color");
	
	if(color != NULL){
		json_object* colors = NULL;
		json_object_deep_copy(color, &colors, 
			json_c_shallow_copy_default);
		json_object_object_add(obj, "colors", colors);
	}

#ifndef NOALARM
        n|= json_object_object_add(obj, "alarm",
                json_object_new_boolean(true));
#endif

        json_object_to_fd(sock_fd, obj, JSON_C_TO_STRING_PRETTY);

        json_object_put(obj);
        return n;

}

int print_changeables_interface(int sockfd){


        json_object* obj = json_object_new_object();
        json_object* stats = json_object_new_array();
        int n = 0;
        for(size_t i = 0; i < state_cnt; i ++){
                n |= json_object_array_add(stats, json_object_new_int(
                        state_trigger_status[i]));

                if(n != 0){
                        println("Failed to add event to array!",ERROR);
                        json_object_put(obj);
                        json_object_put(stats);
                }
        }
        json_object_object_add(obj, "events", stats);
        json_object_object_add(obj, "start_time", 
                json_object_new_int64(game_timer_start));
        json_object_object_add(obj, "end_time", 
                json_object_new_int64(game_timer_end));

#ifndef NOALARM
        n|= json_object_object_add(obj, "alarm",
                json_object_new_boolean(alarm_on));
#endif

        json_object_to_fd(sockfd, obj, JSON_C_TO_STRING_PLAIN);
        
        json_object_put(obj);
        return n;
}

int print_events_interface(int sockfd){

        json_object* eventarr = json_object_object_get(config_glob, "events");
        json_object* arr_names = json_object_new_array();
        for(size_t i = 0; i < json_object_array_length(eventarr); i ++){
                json_object_array_add(arr_names, json_object_new_string(
                json_object_get_string(json_object_object_get(
                json_object_array_get_idx(eventarr,i),"name"))));
        }
        json_object_to_fd(sockfd, arr_names, JSON_C_TO_STRING_PLAIN);
        json_object_put(arr_names);

        return 0;
}

int print_dependencies_interface(int sockfd){


	json_object** dependencies = dependency_list;
	json_object* dep_array = json_object_new_array();
	for(size_t i = 0; i < dependency_count; i++){
		
		const char* module = json_object_get_string(
			json_object_object_get(dependencies[i], "module"));
#ifndef ALLDEPENDS
		/* Ommit core dependencies */
		if(strcasecmp(module, "core") == 0){
			continue;
		}
#endif	
		json_object* dep = NULL;
		json_object_deep_copy(dependencies[i], &dep, 
			json_c_shallow_copy_default);
		
		json_object_array_add(dep_array, dep);
	}

	json_object_to_fd(sockfd, dep_array, JSON_C_TO_STRING_PLAIN);
	json_object_put(dep_array);

	return 0;
	
}

int print_dependency_states_interface(int sockfd){

	json_object* stats = json_object_new_array();
	
	json_object** dependencies = dependency_list;
	for(size_t i = 0; i < dependency_count; i++){
		
		const char* module = json_object_get_string(
			json_object_object_get(dependencies[i], "module"));

#ifndef ALLDEPENDS
		/* Ommit core dependencies */
		if(strcasecmp(module, "core") == 0){
			continue;
		}
#endif
		json_object* stat = json_object_new_int(check_dependency(
			dependencies[i]));
		json_object_array_add(stats, stat);
	}

	json_object_to_fd(sockfd, stats, JSON_C_TO_STRING_PLAIN);
	json_object_put(stats);

	return 0;
	
}

int print_hints_interface(int sockfd){
	
	return json_object_to_fd(sockfd, get_hints_root(), 
		JSON_C_TO_STRING_PLAIN);
}
