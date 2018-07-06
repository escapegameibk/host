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
                int* cp = malloc(sizeof(int));
                *cp = cl;
                pthread_t th;
                if(pthread_create(&th,NULL,handle_comm,cp)){
                        println("error at interface comm handle \
                                        thread creation",ERROR);
                }

        } while (!shutting_down);
        
        println("interface leaving operational state",DEBUG);

        close(unix_fd);
        unlink(SOCKET_LOCATION);

        return NULL;
}

void * handle_comm(void* fd){

        int fdi = *(int*)fd;

#define INT_LEN 64
        char* intermediate = malloc(INT_LEN);
        memset(intermediate,0,INT_LEN);
        sprintf(intermediate, "handling connection for fd %i",fdi);
        println(intermediate,DEBUG);
        free(intermediate);

#define BUFFERLENGTH 2560

        char* buffer = malloc(BUFFERLENGTH);

        while(!shutting_down){
                
                memset(buffer,0,BUFFERLENGTH);
                if(read(fdi,buffer,BUFFERLENGTH) <= 0){
                        
                        intermediate = malloc(INT_LEN);
                        memset(intermediate,0,INT_LEN);
                        sprintf(intermediate,"closing unix socket for fd %i",
                                        fdi);
                        println(intermediate,DEBUG);
                        free(intermediate);
                        break;
#undef INT_LEN
                }else{
                        execute_command(fdi,buffer);
                }
        }

        free(buffer);
        close (fdi);
        free(fd);
#undef BUFFERLENGTH

        return NULL;
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

        debug = malloc(INT_LEN);
        memset(debug,0,INT_LEN);
        sprintf(debug,"fd %i requested action %i",sock_fd,action);
        println(debug,DEBUG);
        free(debug);

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
                /* Return a complete list of states to the client.
                 * This ONLY returns an array of values, rather than the
                 * actual values.
                 */
                json_object_to_fd(sock_fd, 
                                json_object_object_get(config_glob, 
                                        "states"), 
                                JSON_C_TO_STRING_PRETTY);
                break;

        case 4:
                        /* Force-triggers the incoming state and ignores all
                         * dependencies. It will not be triggerable until the
                         * the game is reset to the start state.
                         */
                        
                
                break;
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
        n |= json_object_object_add(obj,"game",json_object_object_get(
                                config_glob,"name"));
        n|= json_object_object_add(obj, "duration",
                json_object_new_int64(game_duration));

        json_object_to_fd(sock_fd, obj, JSON_C_TO_STRING_PRETTY);

        json_object_put(obj);
        return n;

}

int print_changeables_interface(int sockfd){


        json_object* obj = json_object_new_object();
        json_object* stats = json_object_new_array();
        int n = 0;
        for(size_t i = 0; i < state_cnt; i ++){
                n |= json_object_array_add(stats, json_object_new_int64(
                        state_trigger_status[i]));
        }
        json_object_object_add(obj, "states", stats);
        json_object_object_add(obj, "start_time", 
                json_object_new_int64(game_timer_start));

        json_object_to_fd(sockfd, obj, JSON_C_TO_STRING_PRETTY);
        
        json_object_put(obj);
        return n;
}
