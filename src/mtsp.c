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
#include <modbus/modbus.h>
#include <stdint.h>
#include <string.h>

#include "mtsp.h"
#include "log.h"
#include "data.h"

int init_modbus(char* device, int baud){

        
        mdb_ctx = modbus_new_rtu(device, 460800, 'N', 8, 1);
        modbus_rtu_set_serial_mode(mdb_ctx,MODBUS_RTU_RS485);
        modbus_set_debug(mdb_ctx,1);

        if(modbus_connect(mdb_ctx) < 0){
                println("Failed to connect to modbus!", ERROR);
                return -1;
        }

        return 0;
}

