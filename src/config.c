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

#include "config.h"
#include "log.h"

int load_config(const char* location){

        int fd;

        if(location == NULL){
                
                /* the default location is a file in the same directory called
                 * config.json
                 */
                fd = open("config.txt",O_RDONLY);
        }else{
                fd = open(location,O_RDONLY);
        }
        
        config_glob = json_object_from_fd(fd);
        if(config_glob == NULL){
                println("FAILED TO PARSE CONFIG!",ERROR);
                return -1;
        }

        return 0;
}
