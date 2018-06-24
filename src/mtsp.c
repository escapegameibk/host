/* A controller for a protocol called "MTSP" from some russian guys.
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

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "mtsp.h"
#include "log.h"
#include "data.h"
#include "serial.h"

int init_mtps(char* device, int baud){

        
        mtsp_fd = open(device,O_RDWR);
        if(mtsp_fd < 0){
                println("Failed to open device at %s!",ERROR,device);
                return -1;
        }

        if(set_interface_attribs(mtsp_fd,baud) < 0){

                println("Failed to set interface attribs for device at %s!",
                                ERROR,device);
                close(mtsp_fd);
                return -1;
        }

        return 0;
}

