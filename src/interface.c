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
#include "module.h"
#include "video.h"

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

	/* Load and map events to the interface events, as not all are
	 * printable / should be printed */

	json_object* events = json_object_object_get(config_glob,"events");
	printable_event_cnt = 0;
	printable_event_states = NULL;
	printable_events = NULL;
	

	for(size_t i = 0; i < json_object_array_length(events); i++){
		json_object* event = json_object_array_get_idx(events,i);
		json_object* hid = json_object_object_get(event, "hidden");
		if(hid != NULL){
		
			bool hidden = json_object_get_boolean(hid);

			if(hidden){
				println("Ommitting hidden event in interface \
for %i/%s",
					DEBUG, i,get_name_from_object(event));

				continue;
			}

		}

		/* I can display the event. Map it and add it to the array */
		printable_events = realloc(printable_events, 
			++printable_event_cnt * sizeof(size_t));
		printable_events[printable_event_cnt - 1 ] = i;
		
		printable_event_states = realloc(printable_event_states, 
			printable_event_cnt * sizeof(bool*));
		printable_event_states[printable_event_cnt - 1] = 
			&state_trigger_status[i];

	}

        println("initiating unix socket at %s",DEBUG, SOCKET_LOCATION);
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
	
	/* ####################################################################
	 * Initialize controls 
	 * ####################################################################
	 */
	json_object* ctrls = json_object_object_get(config_glob, "controls");
	if(ctrls != NULL){
		for(size_t i = 0; i < json_object_array_length(ctrls); i++){
			
			json_object* ctrl = json_object_array_get_idx(ctrls, i);

			const char* type = json_object_get_string(
				json_object_object_get(ctrl,"type"));
			if(type == NULL){
				println("Invalid control defined! No type!!",
					ERROR);
				json_object_to_fd(STDOUT_FILENO, ctrl,
					JSON_C_TO_STRING_PRETTY);
				return -1;
			}else if(strcasecmp(type, "linear") == 0){
				
				if(json_object_object_get(ctrl, "min") == 
						NULL){
					println("missing min in linear "
						"control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				
				if(json_object_object_get(ctrl, "name") == 
						NULL){
					println("missing name in linear "
						"control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				
				if(json_object_object_get(ctrl, "max") == 
						NULL){
					println("missing max in linear "
						"control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				
				if(json_object_object_get(ctrl, "step") == 
						NULL){
					println("missing step in linear "
						"control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				
				if(json_object_object_get(ctrl, "value") == 
						NULL){
					println("missing value in linear "
						"control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				const char* val_name = json_object_get_string(
					json_object_object_get(ctrl, "value"));
				
				if(json_object_object_get(ctrl, "trigger") == 
						NULL){
					println("missing trigger in linear "
						"control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				
				if(json_object_object_get(ctrl, "initial") == 
						NULL){
					println("missing initial value in"
						"linear control!",ERROR);

					json_object_to_fd(STDOUT_FILENO, ctrl,
						JSON_C_TO_STRING_PRETTY);
					return -1;
				}
				
				int32_t init_val = json_object_get_int(
					json_object_object_get(ctrl, 
					"initial"));

				for(size_t i = 0; i < json_object_array_length(
					json_object_object_get(ctrl,"trigger"));
					i++){
					
					json_object* trigger = 
						json_object_array_get_idx(
						json_object_object_get(
						ctrl,"trigger"), i);

					json_object_object_del(trigger,
						val_name);
					json_object_object_add(trigger,val_name,
						json_object_new_int(init_val));

				}


			}else{
				println("Unknown type in control: %s!", ERROR,
					type);
				json_object_to_fd(STDOUT_FILENO, ctrl,
					JSON_C_TO_STRING_PRETTY);
				return -1;

			}
			

		}

	}
	println("Initialized interface controls!", DEBUG);
	

        /* Initialize the pthread object */
        memset(&interface_thread, 0, sizeof(interface_thread));

        /* Debug and init for the json library */
        println("json library init. version: %s",DEBUG ,JSON_C_VERSION);
        
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
				printable_events[json_object_get_int(
				json_object_object_get(parsed,"event"))]);

                        async_trigger_event(printable_events[
				json_object_get_int(json_object_object_get(
				parsed,"event"))]);
		break;
	case 5:
		/* Forced untrigger the incoming event. Does NOT reset
		the triggers. */

		println("Enforced un-trigger for event %i!", INFO, 
			printable_events[json_object_get_int(
			json_object_object_get(parsed,"event"))]);

		/* This is REALLY dirty, but does the trick.*/
		state_trigger_status[printable_events[json_object_get_int(
			json_object_object_get(parsed,"event"))]] = false;

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
		
		execute_hint(printable_events[json_object_get_int(
		json_object_object_get(parsed,"event_id"))],json_object_get_int(
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
	case 11:
		
		/* Change the language to the passed lang_id if possible */
		println("Global language change! From %i to %i!", DEBUG,
			language,json_object_get_int(json_object_object_get(
			parsed,"lang")));
		language = json_object_get_int(json_object_object_get(parsed,
			"lang"));
		break;
#ifndef NOHINTS
	case 12:
		/* A hint extension. Get execution state. Returns the total
		 * number of hints executed, as well as the hint execution per
		 * hint. A of Version 5.0 this is nescessary if hints are 
		 * enabled.
		 */
		print_hint_states_interface(sock_fd);
		break;
#endif /* NOHINTS */

#ifndef NOVIDEO

	case 13:
		/* A get for the desired video. The monitor variable should
		 * specify what monitor is requesting this thing. */
		print_video_url(sock_fd, json_object_object_get(parsed, 
			"monitor"));

		break;
	case 14:
		/* An acknowlegement of the requesting monitor, that it has
		 * finished playing the current video, and that it requests
		 * the next video.
		 */
		
		video_finished(json_object_get_int(json_object_object_get(
			parsed,"monitor")));

		break;

#endif /* NOVIDEO */

#ifndef NOMEMLOG

	case 15:
		/* Print the log file size to the client */
		
		print_log_size_interface(sock_fd);

		break;
	
	case 16:
		/* Print the log file entirely to te client */
		
		print_log_interface(sock_fd);

		break;
#endif /* NOMEMLOG */
 
	case 17:
		/* Prints a list of available modules to the client */
		print_modules_interface(sock_fd);

		break;
	case 18:
		/* Executes a trigger sent by the client */
		execute_client_trigger_interface( parsed);

		break;

        default:
                /* OOPS */
                println("Invalid action %i by fd %i",WARNING, action, sock_fd);
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
		get_name_from_object(config_glob)));
                
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
	
	json_object* langs = NULL;
	json_object_deep_copy(json_object_object_get(config_glob, "langs"), 
		&langs,json_c_shallow_copy_default);
	if(langs != NULL){
		json_object_object_add(obj, "langs",  langs);
	}
	
	json_object* controls = NULL;
	json_object_deep_copy(json_object_object_get(config_glob, "controls"), 
		&controls,json_c_shallow_copy_default);
	if(controls != NULL){
		json_object_object_add(obj, "controls",  controls);
	}


        json_object_to_fd(sock_fd, obj, JSON_C_TO_STRING_PRETTY);
	
        json_object_put(obj);

        return n;

}

int print_changeables_interface(int sockfd){


        json_object* obj = json_object_new_object();
        json_object* stats = json_object_new_array();
        int n = 0;
        for(size_t i = 0; i < printable_event_cnt; i ++){
                n |= json_object_array_add(stats, json_object_new_int(
                        *printable_event_states[i]));

                if(n != 0){
                        println("Failed to add event to array!",ERROR);
                        json_object_put(obj);
                        json_object_put(stats);
                }
        }
        json_object_object_add(obj, "events", stats);
        json_object_object_add(obj, "start_time", 
                json_object_new_int64(ec_time_to_unix(game_timer_start)));
        json_object_object_add(obj, "end_time", 
                json_object_new_int64(ec_time_to_unix(game_timer_end)));
	
#ifndef NOALARM
        n|= json_object_object_add(obj, "alarm",
                json_object_new_boolean(alarm_on));
#endif
        
	n|= json_object_object_add(obj, "lang",
                json_object_new_int(language));
	

        json_object_to_fd(sockfd, obj, JSON_C_TO_STRING_PLAIN);
        
        json_object_put(obj);
        return n;
}

int print_events_interface(int sockfd){

        json_object* eventarr = json_object_object_get(config_glob, "events");
        json_object* arr_names = json_object_new_array();
        for(size_t i = 0; i < printable_event_cnt; i ++){
                json_object_array_add(arr_names, json_object_new_string(
                get_name_from_object(json_object_array_get_idx(eventarr,
		printable_events[i]))));
        }
        json_object_to_fd(sockfd, arr_names, JSON_C_TO_STRING_PLAIN);
        json_object_put(arr_names);

        return 0;
}

int print_dependencies_interface(int sockfd){

	size_t depcnt = 0;
	json_object** dependencies = get_printables_dependencies(&depcnt);
	json_object* dep_array = json_object_new_array();
	for(size_t i = 0; i < depcnt; i++){
		
		
		json_object* dep = json_object_new_object();
		json_object* orig_dep = dependencies[i];
		json_object_object_add(dep,"name", json_object_new_string(
			get_name_from_object(orig_dep)));
		
		json_object_object_add(dep,"event_id", json_object_new_int(
			is_in_array(json_object_get_int(json_object_object_get(
			orig_dep,"event_id")), printable_events, 
			printable_event_cnt)));
			
		json_object_array_add(dep_array, dep);
	}

	json_object_to_fd(sockfd, dep_array, JSON_C_TO_STRING_PLAIN);
	json_object_put(dep_array);

	free(dependencies);

	return 0;
	
}

int print_dependency_states_interface(int sockfd){

	json_object* stats = json_object_new_array();
	
	size_t depcnt = 0;
	json_object** dependencies = get_printables_dependencies(&depcnt);
	for(size_t i = 0; i < depcnt ; i++){
			

		float percentage;
		int ret = check_dependency(dependencies[i], &percentage);
		
		if(ret < 0){
			println("Interface failed to check dependency!", ERROR);
			println("Dumping root:", ERROR);
			json_object_to_fd(STDOUT_FILENO, dependencies[i],
				JSON_C_TO_STRING_PRETTY);

		}

		json_object* stat = json_object_new_double(percentage);

		json_object_array_add(stats, stat);

	}
	free(dependencies);

	json_object_to_fd(sockfd, stats, JSON_C_TO_STRING_PLAIN);
	json_object_put(stats);

	return 0;
	
}

#ifndef NOHINTS
int print_hints_interface(int sockfd){
	
	json_object* hnt_print = json_object_new_array();

	if(!hints_enabled){
		
		json_object* hnts = json_object_new_array();
		
		json_object_to_fd(sockfd, hnts, 
			JSON_C_TO_STRING_PLAIN);
		
	
		json_object_put(hnts);

		return 0;
	}

	for(size_t i = 0; (i < printable_event_cnt) && (printable_events[i] < 
		json_object_array_length(get_hints_root())); i++){
		
		json_object* hnts = json_object_new_array();
		json_object* real_hnts = json_object_array_get_idx(
			get_hints_root(),printable_events[i]);

		
		for(size_t hnt_id = 0; hnt_id < 
			json_object_array_length(real_hnts); hnt_id ++){
			
			json_object* hnt = json_object_new_object();
			json_object* hint = json_object_array_get_idx(real_hnts,
				hnt_id);
				
			json_object_object_add(hnt,"name", 
				json_object_new_string(get_name_from_object(
				hint)));
			
			/* Add content if existant */
			json_object* content = json_object_object_get(hint,
				"content");
	
			if(content != NULL){
				json_object_object_add(hnt,"content", 
					json_object_new_string(
					get_name_from_object(content)));
			}

			
			json_object_array_add(hnts,hnt);

		}

		json_object_array_add(hnt_print, hnts);
	}
	
	json_object_to_fd(sockfd, hnt_print, 
		JSON_C_TO_STRING_PLAIN);

	json_object_put(hnt_print);
	return 0;
}

int print_hint_states_interface(int sockfd){
	
	json_object* hnt_print = json_object_new_array();
	
	if(!hints_enabled){
		
		
		json_object_to_fd(sockfd, hnt_print, 
			JSON_C_TO_STRING_PLAIN);
		
	
		json_object_put(hnt_print);

		return 0;
	}
	
	for(size_t i = 0; (i < printable_event_cnt) && (printable_events[i] < 
		json_object_array_length(get_hints_root())); i++){
		
		json_object* hnts = json_object_new_array();
		json_object* real_hnts = json_object_array_get_idx(
			get_hints_root(),printable_events[i]);
		
		for(size_t hnt_id = 0; hnt_id < 
			json_object_array_length(real_hnts); hnt_id ++){
			
			json_object* hnt = json_object_new_boolean(hint_exec_states[i][hnt_id]);
			json_object_array_add(hnts,hnt);
		}

		json_object_array_add(hnt_print, hnts);
	}

	json_object* hint_states = json_object_new_object();
	json_object_object_add(hint_states, "hint_states", hnt_print);
	json_object_object_add(hint_states, "hint_count", json_object_new_int(
		get_hint_exec_cnt()));
	
	json_object_to_fd(sockfd, hint_states, 
		JSON_C_TO_STRING_PLAIN);

	json_object_put(hint_states);
	return 0;
}
#endif /* NOHINTS */

#ifndef NOVIDEO
int print_video_url(int sockfd, json_object* device_no){

	size_t dev_id = 0;

	/* Assume monitor 0 when not specified */
	if(device_no != NULL){
		dev_id = json_object_get_int(device_no);
	}

	if(dev_id >= video_device_cnt){
		println("Requested URL for non existing video monitor %i!", 
			WARNING, dev_id);
		return -1;
	}

	json_object* url = json_object_new_object();
	if(video_current_urls[dev_id] != NULL){

		json_object_object_add(url, "URL", 
			json_object_new_string(video_current_urls[dev_id]));
	}else{
		json_object_object_add(url, "URL", NULL);

	}

	json_object_to_fd(sockfd, url, 
		JSON_C_TO_STRING_PLAIN);

	json_object_put(url);
	
	return 0;
}
#endif /* NOVIDEO */


#ifndef NOMEMLOG


int print_log_size_interface(int sock_fd){


	json_object* len = json_object_new_int64(get_log_len());
	
	json_object_to_fd(sock_fd, len, 
		JSON_C_TO_STRING_PLAIN);
	
	json_object_put(len);
	
	return 0;

}

int print_log_interface(int sock_fd){


	char* log = get_log();

	json_object* len = json_object_new_string(log);
	free(log);
	
	json_object_to_fd(sock_fd, len, 
		JSON_C_TO_STRING_PLAIN);

	json_object_put(len);

	return 0;

}

#endif /* NOMEMLOG */

int print_modules_interface(int sock_fd){
	
	json_object* mod_array = json_object_new_array();

	for(size_t i = 0; i < module_count; i++){
		struct module_t* mod = &modules[i];
		if(mod->enabled){
			json_object_array_add(mod_array, 
				json_object_new_string(mod->name));
		}
	}
	
	json_object_to_fd(sock_fd, mod_array, 
		JSON_C_TO_STRING_PLAIN);

	json_object_put(mod_array);
	

	return 0;

}

int execute_client_trigger_interface( json_object* req){

	json_object* trigger = json_object_object_get(req, "trigger");

	if(trigger == NULL){
		println("Received a trigger request from interface with no "
			"trigger! Dumping root:", WARNING);
	
		json_object_to_fd(STDOUT_FILENO, req,
			JSON_C_TO_STRING_PLAIN);
		return -1;

	}

	if(execute_trigger(trigger ,false) < 0){
		println("Failed to execute trigger received via interface!"
			" Dumping root!",
			ERROR);
		
		json_object_to_fd(STDOUT_FILENO, req,
			JSON_C_TO_STRING_PLAIN);
		
		return -1;
	}else{
		println("Succesfully triggered interface received trigger!", 
			DEBUG);
		
	}

	return 0;
}

/* ############################################################################
 * # Helper functions.							      #
 * # Used to get all kinds of stuff, which has to be gotten mre than once.    #
 * ############################################################################
 */

json_object** get_printables_dependencies(size_t *depcntp){

	json_object** deps = NULL;
	size_t depcnt = 0;

	for(size_t i = 0; i < dependency_count; i++){

		json_object* dep = dependency_list[i];
#ifndef ALLDEPENDS
		if(strcasecmp(json_object_get_string(json_object_object_get(dep,
			"module")), "core") == 0){
		
			if(strcasecmp(json_object_get_string(
				json_object_object_get(dep,"type")), 
				"event") == 0){
				continue;
			}
			if(strcasecmp(json_object_get_string(
				json_object_object_get(dep,"type")), 
				"never") == 0){
				continue;
			}
			if(strcasecmp(json_object_get_string(
				json_object_object_get(dep,"type")), 
				"flank") == 0){
				continue;
			}
			if(strcasecmp(json_object_get_string(
				json_object_object_get(dep,"type")), 
				"or") == 0){
				continue;
			}
			if(strcasecmp(json_object_get_string(
				json_object_object_get(dep,"type")), 
				"and") == 0){
				continue;
			}
		}

		json_object* hidden = json_object_object_get(dep, "hidden");
		bool hid = false;

		if(hidden != NULL){
			hid = json_object_get_boolean(hidden);
		}

		if(hid){
			continue;
		}
		
#endif
		
		/* Check if the parent event of the dependency is visible */
		size_t event_id = json_object_get_int(json_object_object_get(
			dep,"event_id"));
		if(is_in_array(event_id, printable_events, 
			printable_event_cnt) < 0){
			continue;
		}

		
		deps = realloc(deps,++depcnt * sizeof(json_object*));
		deps[depcnt - 1] = dep;

	}

	*depcntp = depcnt;
	return deps;

}
