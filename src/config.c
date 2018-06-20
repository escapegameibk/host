/* A configuration parser for the escape game innsbruck
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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "log.h"

int load_config(const char* location){

        int fd;

        char * config_path;
        
        if(location == NULL){
                
                /* the default location is a file in the same directory called
                 * config.json
                 */
                config_path = "config.json";
        }else{
                config_path = (char*)location;
        }

        size_t intlen = strlen(config_path) + 50; 
        char * debug = malloc(intlen);
        memset(debug,0,intlen);
        snprintf(debug,intlen,"parsing config from file: %s", config_path);
        println(debug,DEBUG);

        config_glob = json_object_from_file(config_path);

        if(config_glob == NULL){
                println("FAILED TO PARSE CONFIG!",ERROR);
                println(json_util_get_last_err(),ERROR);
                return -2;
        }

        free(debug);

        return 0;
}
